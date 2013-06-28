#!/usr/bin/python

"""
FatTree.py: run routing expts on a fat tree topology

Nikhil Handigol
"""

import argparse
import sys

from mininet.net import Mininet
from mininet.node import OVSKernelSwitch, RemoteController
from mininet.link import Link, TCLink
from mininet.cli import CLI
from mininet.log import setLogLevel, info, warn, output

from ripl.dctopo import FatTreeTopo
from dump_topo import dump_out_band_topo, dump_in_band_topo

parser = argparse.ArgumentParser(description="FatTree topology for testing \
        NetSight")
parser.add_argument('--bw', '-B',
                    dest="bw",
                    action="store",
                    help="Bandwidth of links",
                    type=float,
                    default=10.0)

parser.add_argument('-k',
                    dest="k",
                    action="store",
                    help="Fat-tree size.",
                    type=int,
                    default=4)

parser.add_argument('--no-debug',
                    dest="no_debug",
                    action="store_true",
                    help="Build a loop-free non-debug topology",
                    default=False)

parser.add_argument('--in-band',
                    dest="in_band",
                    action="store_true",
                    help="In-band postcard collection",
                    default=False)

parser.add_argument('--no-arp',
                    dest="no_arp",
                    action="store_true",
                    help="Don't set Static ARP entries for all hosts",
                    default=False)

parser.add_argument('--dump-topo',
                    dest="topo_outfile",
                    type=str,
                    action="store",
                    help="Output topology in json format",
                    default="/tmp/topo.json")

args = parser.parse_args()

class NSFatTreeTopo(FatTreeTopo):

    def __init__(self, k=args.k, speed=args.bw/1000.):
        '''init'''

        super(NSFatTreeTopo, self).__init__(k, speed)

        # Create template host, switch, and link
        hconfig = {'inNamespace': False, 'ip': '10.%d.0.1' % k}
        lconfig = {}
        swconfig = {'dpid': "%016x" % (k << 16)} # unique dpid for the collector switch
        if not args.no_debug and not args.in_band:
            # out-of-band postcard collection
            self.addHost('h0', **hconfig)
            self.addSwitch('s0', **swconfig)
            # Fast links between switches/dump host to common switch
            # Adding the host first tells us the out_port for the host
            self.addLink('s0', 'h0', **lconfig)
            for s in self.switches():
                self.addLink(s, 's0', **lconfig)
        elif not args.no_debug and args.in_band:
            # in-band postcard collection
            # collector attached to a core switch
            self.addHost('h0', **hconfig)

            c_index = 1 # should be between 1 and args.k/2
            core_sw_no = 1 # should be between 1 and args.k/2
            core_sw = FatTreeTopo.FatTreeNodeID(args.k, c_index,
                    core_sw_no).name_str()
            self.addLink(core_sw, 'h0', **lconfig)

            # Tip: You can generate an edge switch using the tuple (p, e, 1), where
            # p is between 0 and args.k - 1
            # e is between 0 and args.k/2 - 1

    def port(self, src, dst):
        # If there are non-FatTree nodes, query the superclass for ports
        if not ('layer' in self.node_info[src] and
                'layer' in self.node_info[dst]):
            return super(FatTreeTopo, self).port(src, dst)

        return super(NSFatTreeTopo, self).port(src, dst)



def main():
    setLogLevel( 'info' )

    topo = NSFatTreeTopo()

    # dump topo in json format
    if args.in_band:
        dump_in_band_topo(topo, args.topo_outfile)
    else:
        dump_out_band_topo(topo, args.topo_outfile)

    net = Mininet(topo, switch=OVSKernelSwitch, controller=RemoteController, 
            link=Link, autoSetMacs=True, autoStaticArp=not args.no_arp)

    net.start()
    CLI(net)
    net.stop()

if __name__ == '__main__':
    main()
