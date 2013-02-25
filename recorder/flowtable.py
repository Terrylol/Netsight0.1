from openfaucet import ofproto, ofstats, ofmatch
from utils import *
import time
import struct
import dpkt

class Rule:
    def __init__(self, flow_stats):
        # Flow stats contains match, actions and stats fields
        self.flow_stats = flow_stats
        now = time.time()
        self.create_time = now
        self.last_hit_time = now
        self.send_flow_rem = True
        self.check_overlap = False

    def hard_timeout(self):
        return self.flow_stats.hard_timeout

    def idle_timeout(self):
        return self.flow_stats.idle_timeout

    def overlaps(self, other):
        """Check for rule overlap; @self matches @other if
        a packet might match both and priorities are same."""
        if self.flow_stats.priority != other.flow_stats.priority:
            return False
        a = self.flow_stats.match
        b = other.flow_stats.match
        wc = a.wildcards
        for field in wc._fields:
            f1 = getattr(a.wildcards, field)
            f2 = getattr(b.wildcards, field)
            wc = wc._replace(**{field: f1 or f2})
        if 0:
            # For debugging
            print ''
            print a
            print b
            conds = [
                not (a.nw_src is not None and b.nw_src is not None and \
                        ((struct.unpack('!I', a.nw_src[0])[0] ^ struct.unpack('!I', b.nw_src[0])[0]) & wc.nw_src)) ,
                not (a.nw_dst is not None and b.nw_dst is not None and \
                        (struct.unpack('!I', a.nw_dst[0])[0] ^ struct.unpack('!I', b.nw_dst[0])[0]) & wc.nw_dst) ,
                 (wc.in_port or a.in_port == b.in_port),
                 (wc.dl_src or a.dl_src == b.dl_src),
                 (wc.dl_dst or a.dl_dst == b.dl_dst),
                 (wc.dl_vlan or a.dl_vlan == b.dl_vlan),
                 (wc.dl_vlan_pcp or a.dl_vlan_pcp == b.dl_vlan_pcp),
                 (wc.dl_type or a.dl_type == b.dl_type),
                 (wc.nw_tos or a.nw_tos == b.nw_tos),
                 (wc.nw_proto or a.nw_proto == b.nw_proto),
                 (wc.tp_src or a.tp_src == b.tp_src),
                 (wc.tp_dst or a.tp_dst == b.tp_dst)
                ]
            print conds
        if not (a.nw_src is not None and b.nw_src is not None and \
                ((struct.unpack('!I', a.nw_src[0])[0] ^ struct.unpack('!I', b.nw_src[0])[0]) & wc.nw_src)) \
            and not (a.nw_dst is not None and b.nw_dst is not None and \
            (struct.unpack('!I', a.nw_dst[0])[0] ^ struct.unpack('!I', b.nw_dst[0])[0]) & wc.nw_dst) \
            and (wc.in_port or a.in_port == b.in_port) \
            and (wc.dl_src or a.dl_src == b.dl_src) \
            and (wc.dl_dst or a.dl_dst == b.dl_dst) \
            and (wc.dl_vlan or a.dl_vlan == b.dl_vlan) \
            and (wc.dl_vlan_pcp or a.dl_vlan_pcp == b.dl_vlan_pcp) \
            and (wc.dl_type or a.dl_type == b.dl_type) \
            and (wc.nw_tos or a.nw_tos == b.nw_tos) \
            and (wc.nw_proto or a.nw_proto == b.nw_proto) \
            and (wc.tp_src or a.tp_src == b.tp_src) \
            and (wc.tp_dst or a.tp_dst == b.tp_dst):
                return True
        return False

    def __str__(self):
        m = self.flow_stats.match
        wc = m.wildcards
        ret = []
        # TODO add colours
        for field in m._fields:
            wcard = getattr(wc, field)
            if type(wcard) == bool:
                if wcard == True:
                    ret.append("%s=*" % field)
                else:
                    val = getattr(m, field)
                    if field in ['dl_src', 'dl_dst']:
                        val = mac(val)
                    ret.append("%s=%s" % (field, val))
            else:
                val = getattr(m, field)
                if field in ['nw_src', 'nw_dst']:
                    value = getattr(m, field)
                    try:
                        val = ipaddr(value[0])
                        val = "%s/%d" % (val, value[1])
                    except:
                        val = "*"
                ret.append("%s=%s" % (field, val))
        rule =  ' '.join(ret)

        # Properties
        prop = []
        for field in self.flow_stats._fields:
            if field in ['match', 'actions']:
                continue
            prop.append("%s: %d" % (field, getattr(self.flow_stats, field)))
        prop = "\t" + ",  ".join(prop)

        acts = []
        for action in self.flow_stats.actions:
            acts.append("%s<%s>" % (type(action).__name__, collect(action, ',')))
        acts = "\n".join(indent(acts))

        return "\n".join([rule, prop, acts])

class FlowTable(object):
    def __init__(self, size=1000):
        self.size = size
        self.entries = []

    def insert(self, flow_stats):
        r = Rule(flow_stats)
        self.insert_rule(r)

    def insert_rule(self, r):
        if r.check_overlap:
            if self.exists(r):
                return False
        self.entries.append(r)

    def insert_match_action(self, m, a, priority=10000):
        flow_stats = ofstats.FlowStats(table_id=0, # Will be ignored
                match=m, actions=[a],
                idle_timeout=0, hard_timeout=0,
                cookie=0, duration_sec=0, duration_nsec=0,
                priority=priority, packet_count=0, byte_count=0)
        self.insert(flow_stats)

    def lookup(self, in_port, packet):
        # Extract an exact match from packet
        packet = dpkt.ethernet.Ethernet(str(packet))
        try:
            tag = packet.tag
            vlan = tag & ((1 << 13) - 1)
            vlan_pcp = (tag & (0x7 << 13)) >> 13
        except:
            vlan = 0xffff
            vlan_pcp = None
        match = ofmatch.Match.create_wildcarded(
                in_port=in_port, dl_src=packet.src, dl_dst=packet.dst,
                dl_vlan=vlan, dl_vlan_pcp=vlan_pcp, dl_type=packet.type)
        if packet.type == dpkt.ethernet.ETH_TYPE_IP:
            ip = packet.data
            match = match._replace(nw_tos=ip.tos, nw_proto=ip.p,
                    nw_src=(ip.src, 32), nw_dst=(ip.dst, 32))
            if ip.p == dpkt.ip.IP_PROTO_TCP:
                tcp = ip.data
                match = match._replace(tp_src=tcp.sport, tp_dst=tcp.dport)
        return self.lookup_match(match)

    def lookup_match(self, match):
        class DummyFlowStats():
            def __init__(self):
                self.match = match
                self.priority = 0

        r1 = Rule(DummyFlowStats())
        for r2 in sorted(self.entries, key=lambda e: -e.flow_stats.priority):
            r1.flow_stats.priority = r2.flow_stats.priority
            if r2.overlaps(r1):
                return r2.flow_stats.match

    def each_expired(self):
        now = time.time()
        for r in self.entries:
            if r.hard_timeout() and r.create_time + r.hard_timeout() > now:
                yield r, ofproto.OFPRR_HARD_TIMEOUT
            elif r.idle_timeout() and r.last_hit_time + r.idle_timeout() > now:
                yield r, ofproto.OFPRR_IDLE_TIMEOUT
        raise StopIteration

    def remove(self, rule):
        self.entries.remove(rule)

    def collect_expired(self):
        return list(self.each_expired())

    def exists_overlap(self, rule):
        for r in entries:
            if r.overlaps(rule):
                return True
        return False

    def __str__(self):
        return "\n".join(map(lambda e: e.__str__(),
            sorted(self.entries, key=lambda e: -e.flow_stats.priority)))

    def __repr__(self):
        return "<FlowTable size=%d entries=%d>" % (self.size, len(self.entries))

    def s(self):
        print self.__str__()

