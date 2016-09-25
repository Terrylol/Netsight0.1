import logging as L
import argparse
import random
import shlex
import struct
import sys
import time
import signal
import subprocess
from pydoc import pager
import StringIO
import bdb

# third party libraries
import pexpect
from twisted.internet import reactor, defer, protocol, interfaces
from openfaucet import ofcontroller, ofproto, ofaction, buffer, ofmatch, ofstats
from zope import interface
import bson

# local packages
from colouredlog import colourise
from utils import *
from db_handler import MongoHandler
from topology import NDBTopo
from flowtable import FlowTable
from nox_ext import NOXVendorHandler

L.basicConfig()
of_logger = L.getLogger('ofproto')
of_logger.setLevel(L.INFO)

ndb_logger = L.getLogger('ndb')
ndb_logger.setLevel(L.DEBUG)

controller_logger = L.getLogger('controller')
controller_logger.setLevel(L.DEBUG)
colourise(controller_logger)

switch_logger = L.getLogger('switches')
switch_logger.setLevel(L.DEBUG)

parser = argparse.ArgumentParser(description="Simple Proxy Openflow protocol implementation")
parser.add_argument('--host',
                    dest="controller_host",
                    action="store",
                    help="Hostname for controller",
                    default='localhost')

parser.add_argument('--port',
                    dest="controller_port",
                    action="store",
                    help="Port of controller",
                    default=10000)

parser.add_argument('--controller',
                    dest="controller_cmd",
                    action="store",
                    help="Actual controller to run",
                    default="controller ptcp:10000 --log-file")

parser.add_argument('--switch',
                    dest="switch_process",
                    help="Switch process name",
                    default="ovs-openflowd",
                    choices=['ovs-openflowd', 'ovs-vswitchd'])

parser.add_argument('--of-dump',
                    dest="of_dump",
                    action="store_true",
                    help="Dump the OF protocol messages proxied by the debugger.",
                    default=False)

parser.add_argument('--topo',
                    dest="topo",
                    help="Topology input in json format",
                    default=None)

parser.add_argument('--in-band',
                    dest="in_band",
                    action="store_true",
                    help="In-band postcard collection",
                    default=False)

parser.add_argument('--db-host',
                    dest="db_host",
                    help="Host name/IP of the FlowTable database",
                    default='localhost')

parser.add_argument('--db-port',
                    dest="db_port",
                    action="store",
                    help="Port of FlowTable database",
                    default=27017)

args = parser.parse_args()

# Assumption: Each switch has a specific port through which packets
# that need to be logged are sent.  
# Also, event dumped packets are tagged with VLAN 2

# Default collector dump port
dump_port_net = 3

# This is the port connected to the logging host on the collector switch
dump_port_host = 1

# debug space
debug_vlan = 0x2

def debug_pkt(buf):
    (vlan_type, vlan_tag) = struct.unpack('>HH', buf[6+6:6+6+4])
    vlan_id = vlan_tag & 0x0fff
    return vlan_type == dpkt.ethernet.ETH_TYPE_8021Q and vlan_id == debug_vlan

def pack(num, bytes):
    digs = []
    while bytes:
        bytes -= 1
        digs.append(chr(num % 256))
        num /= 256
    return ''.join(reversed(digs))

def process_num(process):
    pid = subprocess.Popen('pidof %s' % process, stdout=subprocess.PIPE, shell=True).communicate()[0]
    try:
        return int(pid)
    except:
        print 'PID of %s not found' % process
        return None

def tag_actions_net(actions, psid, version, in_port, collector_port):
    sw = get_switch_by_psid(psid)
    dpid = sw['dpid'] if sw else None
    print 'tag_actions_net: dpid: %d, psid: %d, outport: %d' % (dpid, psid, collector_port)

    extra_actions = []
    # OpenFlow requires you to use OFPP_IN_PORT to explicitly send back out of input port
    if in_port == collector_port:
        collector_port = ofproto.OFPP_IN_PORT
    # in-band collection: if the in_port is wildcarded (None) send a duplicate postcard out of OFPP_IN_PORT
    # The postcard routing mechanism in the debug flowspace will eliminate one of the duplicates
    elif args.in_band and in_port is None:
        extra_actions = [ofaction.ActionOutput(port=ofproto.OFPP_IN_PORT, max_len=2000)]

    port = 0
    for a in list(actions):
        if a.__class__ == ofaction.ActionOutput:
            port = a.port
            break
    version = pack(version, 3)
    port = pack(port, 2)
    psid = pack(psid , 1)

    tag_actions = [ofaction.ActionSetVlanVid(vlan_vid=debug_vlan),
            ofaction.ActionSetDlDst(dl_addr=version + port + psid),
            ofaction.ActionOutput(port=collector_port, max_len=2000)] + extra_actions
    return tag_actions

def tag_actions_host(port):
    actions = []
    actions.append(ofaction.ActionOutput(port=port, max_len=2000))
    return actions

class SoftwareSwitch(object):
    # Maybe we can create a Switch() class that can be
    # subclassed to create a pause method for hardware
    # switches
    switches = []
    @classmethod
    def get(cls):
        out = subprocess.Popen("pgrep %s" % args.switch_process, shell=True, stdout=subprocess.PIPE).communicate()[0]
        for lines in out.strip().split("\n"):
            yield lines

    def __init__(self, pid, logger):
        self.pid = pid
        self.logger = logger
        SoftwareSwitch.switches.append(self)

    def pause(self):
        self.p = pexpect.spawn("gdb")
        self.p.expect("(gdb)")
        self.p.sendline("attach %s" % self.pid)
        self.p.expect("(gdb)")
        self.logger.info("Paused software switch pid=%s" % self.pid)

    def resume(self):
        self.p.sendline("quit")
        self.p.sendline("y")

class Controller(object):
    # Should probably use the gdb library for this?
    def __init__(self, logger, cmd=args.controller_cmd):
        self.cmd = cmd
        self.p = None
        self.logger = logger
        self.logger.info("Init controller with cmd=%s" % cmd)
        self.pid = None

    def start(self):
        # TODO Check for errors
        self.logger.info("Starting controller under gdb")
        if self.cmd == 'nox':
            nox_pid = process_num('lt-nox_core')
            self.pid = nox_pid
            if nox_pid:
                self.logger.info('Attaching nox pid %d to gdb' % nox_pid)
                self.p = pexpect.spawn("gdb")
                self.p.expect("(gdb)")
                self.p.sendline("attach %d" % nox_pid)
                self.resume()
        else:
            cmds = shlex.split(self.cmd)
            prog = cmds[0]
            args = ' '.join(cmds[1:])
            self.p = pexpect.spawn("gdb %s" % prog)
            self.p.expect("(gdb)")
            self.p.sendline("r %s" % args)

    def pause(self):
        self.p.sendintr()
        self.p.expect("(gdb)")
        self.logger.info("Paused controller")

    def resume(self):
        self.p.sendline("continue")
        self.logger.info("Resuming controller")

    def stop(self):
        self.p.sendintr()
        self.p.expect("(gdb)")
        self.p.send("q")
        self.p.send("y")
        self.logger.info("Quit controller")

def handle_sigint(signal=None, frame=None, **kwargs):
    global ndb
    ndb.db.flush()
    ndb.psid_db.flush()
    handle_pdb_error()

if __name__ == "__main__":
    c = None
    if args.controller_cmd.upper() != 'NONE':
        c = Controller(logger=controller_logger)
    signal.signal(signal.SIGINT, handle_sigint)

def handle_pdb_error(failure=None):
    if c:
        c.stop()
    ndb.stop()
    if failure and not failure.check(bdb.BdbQuit):
        # It is some other exception, so let's print it
        tb = failure.getTraceback()
        print red(tb)
    reactor.callFromThread(reactor.stop)

# XXX Maybe change this class name to "SwitchProxy"?
class NdbProxy(ofproto.OpenflowProtocol):
    """Implements a debuggig proxy, like FlowVisor.

    @peer:
        The peer of this protocol.  If this instance talks to
        the switch, its peer talks to the controller and vice-versa.

    @paired:
        If True, this instance talks to the controller.

    @name:
        Dummy settable parameter

    @flowtable:
        A local copy of the flowtable.  Maintained and upto date
        only for the instance that talks to switch.

    @my_xids:
        Transaction ids for transactions that this instance has
        initiated.

    @xid_to_cb:
        Maps xids to their respective completion callbacks.

    @debug_mode:
        If True, we are in "Debug" mode where we effectively
        use the local flow table.  So, if a controller installs
        an entry, we install it locally and emulate timeouts,
        counters, etc.
    """

    def __init__(self, peer=None, paired=False):
        super(NdbProxy, self).__init__()
        self.peer = peer
        self.paired = paired
        self.topo = None
        self.my_xids = []
        self.xid_to_cb = {}
        self.debug_mode = False
        self.switch_features = None
        self.dpid = None
        self.dpname = None
        self.connected = False
        self._should_tag = False
        self._features = None

    def master(self):
        return not self.paired

    def slave(self):
        return self.paired

    def flowtable(self):
        return self.ndb.get_flowtable_by_dpid(self.dpid)

    def backup_flowtable(self):
        self.ndb.backup_flowtable_by_dpid(self.dpid)

    def new_flowtable(self):
        self.ndb.new_flowtable_for_dpid(self.dpid)

    def connectionMade(self):
        # override
        self.logger.info("connection made")
        self.connected = True
        self._buffer = buffer.ReceiveBuffer()
        
        # Send hello as soon as the connection is established 
        # In the new model, the hello is not proxied
        self.send_hello()


    def cleaner(self):
        if self.debug_mode:
            self.logger.info("cleaner")
            for rule, reason in self.flowtable().each_expired():
                if rule.send_flow_rem:
                    dt = rule.last_hit_time - rule.create_time
                    self.peer.send_flow_removed(rule.match, rule.cookie, rule.priority,
                            reason, int(dt), int((dt - int(dt)) * 1e9), rule.idle_timeout,
                            # TODO: be careful when measuring this
                            rule.packet_count, rule.byte_count)
                self.flowtable().remove(rule)
        reactor.callLater(1, self.cleaner)

    def start_transaction(self, xid, cb):
        self.logger.info("Start transaction %x" % xid)
        self.my_xids.append(xid)
        self.xid_to_cb[xid] = cb

    def finish_transaction(self, xid):
        self.logger.info("Finish transaction %x" % xid)
        self.my_xids.remove(xid)
        cb = self.xid_to_cb[xid]
        if cb:
            reactor.callLater(0, cb, xid)
        del self.xid_to_cb[xid]

    def read_from_switch(self, cb=None):
        if self.master():
            self.logger.info("Starting read_from_switch")
            # backup our own flowtable
            self.backup_flowtable()
            self.new_flowtable()
            xid = random.getrandbits(32)
            self.start_transaction(xid, cb)
            self.logger.info("Starting to read from switch, xid=%x" % xid)
            self.send_stats_request_flow(xid, ofmatch.Match.create_wildcarded(), 0xff, ofproto.OFPP_NONE)

    def flush_flowtable(self, cb=None):
        if self.master():
            xid = random.getrandbits(32)
            self.send_flow_mod_delete(False, ofmatch.Match.create_wildcarded(), 0, ofproto.OFPP_NONE)
            self.start_transaction(xid, cb)
            self.send_barrier_request(xid)

    def restore_to_switch(self, cb=None):
        if self.master():
            xid = random.getrandbits(32)
            self.logger.info("Restore to switch xid=%x" % xid)
            for r in self.flowtable().entries:
                self.send_flow_mod_add(r.flow_stats.match, r.flow_stats.cookie,
                        r.flow_stats.idle_timeout, r.flow_stats.hard_timeout,
                        # send_flow_rem is bogus; switch doesn't tell us what controller did earlier
                        # XXX better to be safe and set it to True?
                        r.flow_stats.priority, 0xffffffff, r.send_flow_rem, False,
                        # XXX same with emerg, which i'll set to False for now
                        False, r.flow_stats.actions)
            self.start_transaction(xid, cb)
            self.send_barrier_request(xid)

    def update_flowtable_from_stats(self, flow_stats_all):
        for flow_stats in flow_stats_all:
            self.flowtable().insert(flow_stats)

    def install_routes_to_logger(self, cb=None):
        # do not install routes to logger if the switch hasn't identified itself yet
        if self.dpname is None:
            return
        self.logger.info("Installing routes to logger on %s" % self.dpname)
        if args.in_band:
            # in-band postcard collection
            # set highest priority entry matching all packets belonging to the debug flowspace
            # route matched postcards over the collector spanning tree
            xid = random.getrandbits(32)
            init_xid = xid
            outport = self.topo.collector_dump_ports[self.dpid]
            actions = tag_actions_host(outport)
            self.logger.info('in-band install_routes_to_logger:' 
                    'dpid:%d, outport:%d, xid:%d' % (self.dpid, outport, xid))

            valid_st_inports = [ports[self.dpid] for (nbr, ports) in self.topo.collector_st[self.dpid].iteritems()]
            if outport in valid_st_inports:
                valid_st_inports.remove(outport)

            self.start_transaction(xid, cb)

            # Highest priority: Forward postcards if packets come in from legit ST in_ports
            for in_port in valid_st_inports:
                match = ofmatch.Match.create_wildcarded(in_port=in_port, dl_vlan=debug_vlan)
                self.send_flow_mod_add(match, xid, # cookie
                                       0, # idle_timeout
                                       0, # hard_timeout
                                       0xffff, # highest priority
                                       0xffffffff, # buffer_id
                                       False,
                                       False,
                                       False,
                                       actions)
                self.advance_flowtable_version((match, actions))
                xid += 1

            # Drop all other postcards
            match = ofmatch.Match.create_wildcarded(dl_vlan=debug_vlan)
            actions = []
            self.send_flow_mod_add(match, xid, # cookie
                                   0, # idle_timeout
                                   0, # hard_timeout
                                   0xfffe, # One below highest priority
                                   0xffffffff, # buffer_id
                                   False,
                                   False,
                                   False,
                                   actions) # Empty action is equivalent to a drop
            self.send_barrier_request(init_xid)
            self.advance_flowtable_version((match, actions))
            self.start_tagging()
        else:
            if self.dpid not in self.topo.g.nodes():
                self.logger.info("install_routes_to_logger: debug switch")
                # This is the debugging switch
                xid = random.getrandbits(32)
                match = ofmatch.Match.create_wildcarded(dl_vlan=debug_vlan)

                self.send_flow_mod_add(match, xid, # cookie
                                       0, # idle_timeout
                                       0, # hard_timeout
                                       0xffff, # priority
                                       0xffffffff, # buffer_id
                                       False,
                                       False,
                                       False,
                                       tag_actions_host(dump_port_host))
                # Any other packet (flood attempt by learning switch) on
                # this switch will be dropped...
                self.send_flow_mod_add(ofmatch.Match.create_wildcarded(), xid + 1,
                                       0, # idle_timeout
                                       0, # hard_timeout
                                       0xfffe, # priority
                                       0xffffffff, # buffer_id
                                       False,
                                       False,
                                       False,
                                       #[ofaction.ActionOutput(port=ofproto.OFPP_LOCAL, max_len=2000)])
                                       [])
                self.start_transaction(xid, cb)
                self.send_barrier_request(xid)
            else:
                # These are other switches.  We needn't do anything, as
                # controller messages will get modded to include the new
                # actions (set-vlan 2, out-port 3).
                self.logger.info("install_routes_to_logger: Normal switch; skipping")
                self.start_tagging()
        return

    def start_tagging(self):
        self._should_tag = True
        if self.peer:
            self.peer._should_tag = True

    def should_tag(self):
        # Easy to over-ride later
        return self._should_tag == True

    def __repr__(self):
        if self.master():
            return "<NdbProxy for switch dpid %s (%s)>" % (self.dpid, self.dpname)
        else:
            return "<NdbProxy slave for switch dpid %s (%s)>" % (self.peer.dpid, self.peer.dpname)

    def connectionLost(self, reason):
        self.logger.debug("connection lost %s" % reason)
        self.connected = False

    def slave_connected(self):
        self.logger.info("slave connected to controller")

    def advance_flowtable_version(self, entry):
        assert(self.dpid != None)
        self.ndb.advance_flowtable_version(self.dpid, entry)

    def add_tag(self, actions, in_port=None):
        return actions + tag_actions_net(actions,
                                         self.ndb.get_psid(self.dpid),
                                         self.ndb.get_flowtable_version(self.dpid),
                                         in_port,
                                         self.topo.collector_dump_ports.get(self.dpid, dump_port_net))


    # Proxy functions for all protocol messages.  This could have been
    # less verbose...
    def handle_hello(self):
        # Send features request immediately upon receiving the first hello
        if self.master() and self._features is None:
            xid = random.getrandbits(32)
            self.send_features_request(xid)

    def handle_echo_request(self, xid, data):
        # In the new model, the echoes are proxied only if connected
        if not self.peer:
            self.send_echo_reply(xid, data)
        else:
            self.peer.send_echo_request(xid, data)

    def handle_echo_reply(self, xid, data):
        # Echo reply can only come from the controller, so let through
        if not self.peer:
            reactor.callLater(1, self.handle_echo_reply, xid, data)
            return
        self.peer.send_echo_reply(xid, data)

    def handle_features_request(self, xid):
        # Can come only from the controller
        if not self.master() and self._features:
            self.send_features_reply(xid, self._features)

            # if all switches have been connected, print a message
            switches_connected = True
            for s in self.topo.g.nodes():
                if not s in self.ndb.switches:
                    switches_connected = False
                    break
            if switches_connected:
                print T.colored('ALL SWITCHES CONNECTED!', 'cyan')

    def handle_features_reply(self, xid, s_f):
        # Snoop on this
        if self.master():
            # save a copy of the features
            self._features = s_f

            dpid = s_f.datapath_id
            dpname = '?'
            for p in s_f.ports:
                if p.port_no == ofproto.OFPP_LOCAL:
                    dpname = p.name
                    break
            self.dpid = dpid
            self.dpname = dpname

            # Create a slave peer now!
            if dpid in self.ndb.topo.g.nodes():
                if self.peer is None:
                    self.logger.info("connecting slave to controller")
                    reactor.connectTCP(args.controller_host, args.controller_port,
                                       NdbPeerClientFactory(name='slave', ndb=self.ndb, peer=self, topo=self.topo))
                else:
                    self.logger.info("reusing already connected slave..")

            self.ndb.add_switch(dpid, dpname, s_f)
            self.cleaner()

            # update logger name
            self.logger = L.getLogger('ofproto_%s_%s' % (self.dpname, self.factory.name))
            self.logger.setLevel(L.INFO)
            if args.of_dump:
                self.logger.setLevel(L.DEBUG)
            if self.peer:
                self.peer.logger = L.getLogger('ofproto_%s_%s' % (self.peer.dpname, self.peer.factory.name))
                self.peer.logger.setLevel(L.INFO)
                if args.of_dump:
                    self.peer.logger.setLevel(L.DEBUG)

            self.install_routes_to_logger()

    def handle_get_config_request(self, xid):
        if not self.peer:
            reactor.callLater(1, self.handle_get_config_request, xid)
            return
        self.peer.send_get_config_request(xid)

    def handle_get_config_reply(self, xid, switch_config):
        if not self.peer:
            reactor.callLater(1, self.handle_get_config_reply, xid, switch_config)
            return
        self.peer.send_get_config_reply(xid, switch_config)

    def handle_set_config(self, switch_config):
        if not self.peer:
            reactor.callLater(1, self.handle_set_config, switch_config)
            return
        self.peer.send_set_config(switch_config)

    def handle_packet_in(self, buffer_id, total_len, in_port, reason, data):

        if not self.peer:
            reactor.callLater(1, self.handle_packet_in, buffer_id, total_len, in_port, reason, data)
            return

        # Some controllers (NOX) flush the entire flow table on bootup
        # Therefore, if we get a packet_in ...
        # (1) from the debug flowspace in an in-band setup, or
        # (2) from the multiplexer switch in an out-of-band setup
        # we should (re)install the debug routing rules
        if (args.in_band and debug_pkt(str(data))) or (not args.in_band and self.dpid not in self.topo.g.nodes()):
            self.logger.info('handle_packet_in: missed debug packet-in')
            if not self.master():
                self.logger.error('handle_packet_in: missed debug packet-in at slave')
            else:
                self.logger.info('handle_packet_in: reinstalling route to debugger')
                data_lst = [str(data)] if buffer_id == 0xffffffff else []
                if args.in_band:
                    self.install_routes_to_logger()
                    actions = [ofaction.ActionOutput(port=ofproto.OFPP_TABLE, max_len=2000)]
                    self.send_packet_out(buffer_id, in_port, actions, data_lst)
                else:
                    if self.dpid in self.topo.g.nodes():
                        self.logger.error('handle_packet_in: dpid: %d, in_port: %d... Debug packet at non-multiplexer switch' % (self.dpid, in_port))
                    else:
                        self.install_routes_to_logger()
                        actions = [ofaction.ActionOutput(port=ofproto.OFPP_TABLE, max_len=2000)]
                        self.send_packet_out(buffer_id, in_port, actions, data_lst)
            return

        if self.dpid not in self.topo.g.nodes():
            self.logger.info('handle_packet_in: dpid: %s, in_port: %d... '
                    'Dropping' % (repr(self.dpid), in_port))
            return

        if self.master():
            pkt = dpkt.ethernet.Ethernet(str(data))
            self.logger.info('handle_packet_in: dpid: %d, in_port: %d, type: 0x%x' % (self.dpid, in_port, pkt.type))

        self.peer.send_packet_in(buffer_id, total_len, in_port, reason, [str(data)])

    def handle_flow_removed(self, match, cookie, priority, reason, duration_sec, duration_nsec,
                    idle_timeout, packet_count, byte_count):
        if not self.peer:
            reactor.callLater(1, self.handle_flow_removed, match, cookie, priority, reason,
                    duration_sec, duration_nsec, idle_timeout, packet_count, byte_count)
            return

        if self.master():
            self.logger.info('handle_flow_removed: dpid: %d\nmatch: %s\nreason: %d' % (self.dpid, repr(match), reason))

        self.peer.send_flow_removed(match, cookie, priority, reason, duration_sec, duration_nsec,
                    idle_timeout, packet_count, byte_count)

    def handle_port_status(self, reason, desc):
        if not self.peer:
            reactor.callLater(1, self.handle_port_status, reason, desc)
            return
        self.peer.send_port_status(reason, desc)

    def handle_packet_out(self, buffer_id, in_port, actions, data):
        if not self.peer:
            reactor.callLater(1, self.handle_packet_out, buffer_id, in_port, actions, data)
            return

        self.logger.info('handle_packet_out: dpid: %d\nactions: %s' % (self.dpid, repr(actions)))

        if not self.master():
            # We need to capture packets sent via packet-out as well.
            # We could log it directly, but easier to tell the switch
            # to send it to the logger...
            orig_actions = list(actions)
            if self.should_tag():
                actions = self.add_tag(list(actions), in_port)
            self.advance_flowtable_version(("buffer_id=%s" % buffer_id,
                orig_actions))
        self.peer.send_packet_out(buffer_id, in_port, actions, [str(data)])

    def handle_flow_mod_add(
            self, match, cookie, idle_timeout, hard_timeout, priority, buffer_id,
            send_flow_rem, check_overlap, emerg, actions):
        if not self.peer:
            reactor.callLater(1, self.handle_flow_mod_add,
                match, cookie, idle_timeout, hard_timeout, priority, buffer_id,
                send_flow_rem, check_overlap, emerg, actions)
            return

        if not self.master():
            self.logger.info('handle_flow_mod_add: dpid: %d, prio: %d\nmatch: %s\nactions: %s' % (self.dpid, priority, match, actions))
        
        if not self.master():
            actions = list(actions)
            orig_actions = list(actions)
            if self.should_tag():
                actions = self.add_tag(actions, match.in_port)
            self.advance_flowtable_version((match, orig_actions))
        self.peer.send_flow_mod_add(
                match, cookie, idle_timeout, hard_timeout, priority, buffer_id,
                send_flow_rem, check_overlap, emerg, actions)

    def handle_flow_mod_modify(
            self, strict, match, cookie, idle_timeout, hard_timeout, priority,
            buffer_id, send_flow_rem, check_overlap, emerg, actions):
        if not self.peer:
            reactor.callLater(1, self.handle_flow_mod_modify,
                self, strict, match, cookie, idle_timeout, hard_timeout, priority,
                buffer_id, send_flow_rem, check_overlap, emerg, actions)
            return

        if not self.master():
            self.logger.info('handle_flow_mod_modify: dpid: %d, prio: %d\nmatch: %s\nactions: %s' % (self.dpid, priority, match, actions))
        
        if not self.master():
            actions = list(actions)
            orig_actions = list(actions)
            if self.should_tag():
                actions = self.add_tag(actions, match.in_port)
            self.advance_flowtable_version((match, orig_actions))
        self.peer.send_flow_mod_modify(
                self, strict, match, cookie, idle_timeout, hard_timeout, priority,
                buffer_id, send_flow_rem, check_overlap, emerg, actions)

    def handle_flow_mod_delete(self, strict, match, priority, out_port):
        if not self.peer:
            reactor.callLater(1, self.handle_flow_mod_delete, strict, match, priority, out_port)
            return

        if not self.master():
            self.logger.info('handle_flow_mod_delete: dpid: %d, prio: %d\nmatch: %s\nout_port: %d' % (self.dpid, priority, match, out_port))
        
        if not self.master():
            self.advance_flowtable_version((match, "flow-mod-delete"))
        self.peer.send_flow_mod_delete(strict, match, priority, out_port)

        # If the controller is flushing all the entries, proactively reinstall debug rules
        if match == ofmatch.Match.create_wildcarded() and not strict and out_port == ofproto.OFPP_NONE:
            self.logger.info('All flow entries flushed... reinstalling debug flow-mods')
            self.peer.install_routes_to_logger()

    def handle_port_mod(self, port_no, hw_addr, config, mask, advertise):
        if not self.peer:
            reactor.callLater(1, self.handle_port_mod,
                        port_no, hw_addr, config, mask, advertise)
            return
        self.peer.send_port_mod(port_no, hw_addr, config, mask, advertise)
        return

    def handle_stats_request_desc(self, xid):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_desc, xid)
            return
        self.peer.send_stats_request_desc(xid)

    def handle_stats_request_flow(self, xid, match, table_id, out_port):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_flow, xid, match, table_id, out_port)
            return
        self.peer.send_stats_request_flow(xid, match, table_id, out_port)

    def handle_stats_request_aggregate(self, xid, match, table_id, out_port):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_aggregate, xid, match, table_id, out_port)
            return
        self.peer.send_stats_request_aggregate(xid, match, table_id, out_port)

    def handle_stats_request_table(self, xid):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_table, xid)
            return
        self.peer.send_stats_request_table(xid)

    def handle_stats_request_port(self, xid, port_no):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_port, xid, port_no)
            return
        self.peer.send_stats_request_port(xid, port_no)

    def handle_stats_request_queue(self, xid, port_no, queue_id):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_request_queue, xid, port_no, queue_id)
            return
        self.peer.send_stats_request_queue(xid, port_no, queue_id)

    def handle_stats_reply_desc(self, xid, desc_stats):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_desc, xid, desc_stats)
            return
        self.peer.send_stats_reply_desc(xid, desc_stats)

    def handle_stats_reply_flow(self, xid, flow_stats, reply_more):
        if xid in self.my_xids:
            self.logger.info("getting stats_reply_flow(xid=%x)" % xid)
            self.update_flowtable_from_stats(flow_stats)
            if not reply_more:
                self.logger.info("done with stats_reply_flow(xid=%x)" % xid)
                self.finish_transaction(xid)
            return
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_flow, xid, flow_stats, reply_more)
            return
        self.peer.send_stats_reply_flow(xid, flow_stats, reply_more)

    def handle_stats_reply_aggregate(self, xid, packet_count, byte_count,
            flow_count):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_aggregate, xid, packet_count, byte_count,
                    flow_count)
            return
        self.peer.send_stats_reply_aggregate(xid, packet_count, byte_count, flow_count)

    def handle_stats_reply_table(self, xid, table_stats, reply_more):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_table, xid, table_stats, reply_more)
            return

        self.peer.send_stats_reply_table(xid, table_stats, reply_more)

    def handle_stats_reply_port(self, xid, port_stats, reply_more):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_port, xid, port_stats, reply_more)
            return
        self.peer.send_stats_reply_port(xid, port_stats, reply_more)

    def handle_stats_reply_queue(self, xid, queue_stats, reply_more):
        if not self.peer:
            reactor.callLater(1, self.handle_stats_reply_queue, xid, queue_stats, reply_more)
            return
        self.peer.send_stats_reply_queue(xid, queue_stats, reply_more)

    def handle_barrier_request(self, xid):
        if not self.peer:
            reactor.callLater(1, self.handle_barrier_request, xid)
            return
        self.peer.send_barrier_request(xid)

    def handle_barrier_reply(self, xid):
        if not self.peer:
            reactor.callLater(1, self.handle_barrier_reply, xid)
            return
        if xid in self.my_xids:
            self.finish_transaction(xid)
            return
        self.peer.send_barrier_reply(xid)

    def handle_queue_get_config_request(self, xid, port_no):
        if not self.peer:
            reactor.callLater(1, self.handle_queue_get_config_request, xid, port_no)
            return
        self.peer.send_queue_get_config_request(xid, port_no)

    def handle_queue_get_config_reply(self, xid, port_no, queues):
        if not self.peer:
            reactor.callLater(1, self.handle_queue_get_config_reply, xid, port_no, queues)
            return
        self.peer.send_queue_get_config_reply(xid, port_no, queues)

    def handle_error(self, xid, error):
        self.logger.error('Error: OFPT_ERROR message received at %s (dpid %d)' % (self.dpname, self.dpid))
        self.logger.error('xid: %d, error: %s' % (xid, str(error)))
        if not self.peer:
            reactor.callLater(1, self.handle_error, xid, error)
            return
        self.peer.send_error(error, xid)

class Ndb:
    " A debugger instance that maintains state about the network."
    def __init__(self, listen_port=6633):
        self.switches = MyDict()
        self.listen_port = listen_port
        self.masters = []
        self.topo = NDBTopo(args.topo) if args.topo else None

        #assert args.in_band == self.topo.in_band, "The specified postcard collection type (in-band vs out-of-band) \
         #       and the input topology don't match"

        waitListening(args.db_host, args.db_port)

    def start(self):
        ndb_logger.info("Starting ndb; please wait for switches to connect")

        # Start a flow-table DB, and flush any existing entries
        self.db = MongoHandler(dpid=None, host=args.db_host, port=args.db_port,
                flush_on_del=True)
        self.db.flush()

        # Start a psid to dpid db
        self.psid_db = MongoHandler(host=args.db_host, port=args.db_port,
                coll_name='psid-to-dpid', flush_on_del=True)
        self.psid_db.flush()

        # Save topology in the db
        self.topo_db = MongoHandler(host=args.db_host, port=args.db_port,
                coll_name='topo', flush_on_del=True)
        self.topo_db.flush()
        self.topo_db.add_record(self.topo.topo_json)

        for sw_pid in SoftwareSwitch.get():
            SoftwareSwitch(sw_pid, logger=switch_logger)

        if c:
            c.start()
        factory = NdbPeerClientFactory(logger=of_logger, name='master', ndb=self, peer=None, topo=self.topo)
        reactor.listenTCP(self.listen_port, factory)
        reactor.run()
        return self

    def stop(self):
        pass

    def add_switch(self, dpid, dpname, features=None):
        ndb_logger.info("Adding switch %s %s" % (dpid, dpname))
        info = MyDict({ 'dpid': dpid,
                        'dpname': dpname,
                        'features': features,
                        'version': 1,
                        'flowmods': {}, # maps version-number to the flow-mod (match, action) at that version
                        'db': MongoHandler(dpid, args.db_host, args.db_port,
                            flush_on_del=True),
                }, should_trim=True, indent_level=1)
        ndb_logger.info(green("Number of tables: %d" % features.n_tables))
        if self.switches.has_key(dpid):
            self.switches[dpid].update(info)
        else:
            self.switches[dpid] = info
            self.switches[dpid]['flowtable'] = FlowTable()
            # TODO: a more robust way of allocating psids
            psid = len(self.switches)
            self.switches[dpid]['psid'] =  psid # 1B private switch ID
            self.psid_db.add_record({'psid': psid, 'dpid': dpid})

    def get_flowtable_by_dpid(self, dpid):
        if self.switches.has_key(dpid):
            return self.switches[dpid]['flowtable']
        else:
            ndb_logger.info("get_flowtable: Could not find flowtable for dpid %s" % dpid)
            return FlowTable()

    def new_flowtable_for_dpid(self, dpid):
        if self.switches.has_key(dpid):
            self.switches[dpid]['flowtable'] = FlowTable()
        else:
            ndb_logger.info("new_flowtable: Could not find flowtable for dpid %s" % dpid)

    def backup_flowtable_by_dpid(self, dpid):
        sw = self.switches.get(dpid, None)
        if sw:
            sw['backup_flowtable'] = sw['flowtable']
        else:
            ndb_logger.info("backup_flowtable: Could not find flowtable for dpid %s" % dpid)

    def advance_flowtable_version(self, dpid, entry):
        try:
            v = self.switches[dpid]['version']
            self.switches[dpid]['flowmods'][v] = entry
            # Update flow-table DB
            # match
            match_str = entry[0]
            if isinstance(entry[0], ofmatch.Match):
                match_str = entry[0].serialize()

            # actions
            ofp = ofproto.OpenflowProtocol()
            nox_handler = NOXVendorHandler(ofp)
            ofp.set_vendor_handlers([nox_handler])
            actions_str = []
            if isinstance(entry[1], list) or isinstance(entry[1], tuple):
                for a in entry[1]:
                    try:
                        actions_str.append(''.join(ofp.serialize_action(a)))
                    except:
                        actions_str.append(a)
            else:
                actions_str.append(entry[1])
            db_rec = {'dpid': dpid, 'psid': self.get_psid(dpid),
                    'version': v,
                    'match': bson.binary.Binary(match_str),
                    'actions': [bson.binary.Binary(a) for a in actions_str]
                    }
            self.switches[dpid]['db'].add_record(db_rec)
            self.switches[dpid]['version'] += 1
            ndb_logger.debug("Advancing flow table dpid %s to version %d" % (dpid, self.switches[dpid]['version']))
        except KeyError:
            ndb_logger.critical("Could not found dpid %s" % dpid)
        return

    def get_switch_by_name(self, dpname):
        return

    def get_masters(self):
        return self.masters

    def add_master(self, m):
        self.masters.append(m)

    def remove_master(self, m):
        try:
            self.masters.remove(m)
        except ValueError:
            ndb_logger.critical("Tried to remove an unknown master")

    def get_switches(self):
        return list(self.switches.itervalues())

    def get_flowtable_version(self, dpid):
        try:
            return self.switches[dpid]['version']
        except:
            return 0xffffffffffff

    def get_psid(self, dpid):
        try:
            return self.switches[dpid]['psid']
        except:
            return 0xff

class NdbPeerClientFactory(protocol.ReconnectingClientFactory):
    def __init__(self, protocol_class=NdbProxy, logger=of_logger,
            name='master', ndb=None, peer=None, error_data_bytes=64,
            topo=None):
        self.peer = peer
        self.protocol_class = protocol_class
        self.logger = logger
        self.name = name
        self.ndb = ndb
        self.error_data_bytes = error_data_bytes
        self.topo = topo

    def buildProtocol(self, addr):
        paired = (self.name=='slave')
        p = self.protocol_class(self.peer, paired=paired)
        p.topo = self.topo
        p.factory = self
        p.name = self.name
        p.ndb = self.ndb
        p.error_data_bytes = self.error_data_bytes
        p.logger = L.getLogger('ofproto_%s' % self.name)
        p.logger.setLevel(L.INFO)
        if args.of_dump:
            p.logger.setLevel(L.DEBUG)
        p.log_extra = {'remote_host': addr.host,
                       'remote_port': addr.port}
        nox_handler = NOXVendorHandler(p)
        p.set_vendor_handlers([nox_handler])
        if self.peer:
            # We are our peer's peer
            self.peer.peer = p
            self.peer.slave_connected()

            # peer's attributes like dpid and dpname should be set by now
            p.dpid = self.peer.dpid
            p.dpname = self.peer.dpname
            p._should_tag = self.peer._should_tag
            p._features = self.peer._features
        self.protocol = p
        if p.master():
            self.ndb.add_master(p)
        return self.protocol

    def startedConnecting(self, connector):
        pass

    def clientConnectionLost(self, connector, reason):
        self.logger.debug("Client connection lost due to %s; reconnecting..." % reason)
        connector.connect()
        return
        if self.peer and self.peer.connected:
            self.peer.connectionLost(reason)
        if self.protocol.master():
            self.ndb.remove_master(self.protocol)

    def clientConnectionFailed(self, c, r):
        self.clientConnectionLost(c, r)

def switches():
    global ndb
    ret = StringIO.StringIO()
    for key,sw in ndb.switches.iteritems():
        info = sw['features']
        if not info:
            print >>ret, "Switch <unknown>"
            continue
        print >>ret, "Datapath ID: %s" % info.datapath_id
        print >>ret, "Ports:"
        p = []
        for port in info.ports:
            a = ("name: %s" % port.name)
            b = ("port_no: %d" % port.port_no)
            c = ("hw_addr: %s" % mac(port.hw_addr))
            config = "%s %s %s" % (a, b, c)
            p.append(config)
        print >>ret, "\n".join(indent(p))

    if len(ret.getvalue().split("\n")) > 40:
        pager(ret.getvalue())
    else:
        print ret.getvalue()

    ret.close()

# Switch by Name
def get_switch_by_name(name):
    global ndb
    for key,sw in ndb.switches.iteritems():
        info = sw['features']
        if not info:
            continue
        for p in info.ports:
            if p.port_no == ofproto.OFPP_LOCAL and name == p.name:
                return sw
    return None

# Switch by PSID
def get_switch_by_psid(psid):
    global ndb
    for key,sw in ndb.switches.iteritems():
        if(sw['psid'] == psid):
            return sw
    return None

if __name__ == "__main__":
    global ndb
    ndb = Ndb(listen_port=6633)
    ndb.start()

