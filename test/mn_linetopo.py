#!/usr/bin/python

import argparse
from mininet.topo import Topo
from mininet.node import OVSKernelSwitch, RemoteController
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.cli import CLI
from mininet.log import lg
from dump_topo import dump_out_band_topo, dump_in_band_topo

global args
args = None

def parse_args():
    parser = argparse.ArgumentParser(description="Simple line topology for testing with mininet")
    parser.add_argument('--bw', '-B',
                        dest="bw",
                        action="store",
                        help="Bandwidth of links",
                        type=float,
                        default=10.0)

    parser.add_argument('-n',
                        dest="n",
                        action="store",
                        help="Number of switches in line.  Must be >= 1",
                        type=int,
                        default=5)

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

    parser.add_argument('--dump-topo',
                        dest="topo_outfile",
                        type=str,
                        action="store",
                        help="Output topology in json format",
                        default="/tmp/topo.json")

    args = parser.parse_args()
    return args

lg.setLogLevel('info')

# It's no more a LineTopo, but let me not rename it
class LineTopo(Topo):
    def __init__(self, n=1, bw=10, in_band=False, no_debug=False):
        # Add default members to class.
        super(LineTopo, self ).__init__()

        # Create template host, switch, and link
        hconfig = {'inNamespace':True}
        collector_config = {'inNamespace':False}
        lconfig = {'bw': bw, 'delay': '0.0ms'}
        flink_config = {}

        # Create switch and host nodes
        self.addHost('h1', **hconfig)
        self.addHost('h2', **hconfig)

        for i in xrange(n):
            swconfig = {'dpid': "%016x" % (i+1)}
            self.addSwitch('s%d' % (i+1), **swconfig)

        for i in xrange(n-1):
            self.addLink('s%d' % (i+1), 's%d' % (i+2), **lconfig)

        self.addLink('h1', 's1', **lconfig)
        self.addLink('h2', 's%d' % n, **lconfig)

        if not no_debug and not in_band:
            # out-of-band postcard collection
            self.addHost('h0', **collector_config)
            swconfig = {'dpid': "%016x" % (n+1)}
            self.addSwitch('s0', **swconfig)
            # Fast links between switches/dump host to common switch
            # Adding the host first tells us the out_port for the host
            self.addLink('s0', 'h0', **flink_config)
            for i in xrange(n):
                self.addLink('s%d' % (i+1), 's0', **flink_config)
        elif not no_debug and in_band:
            # in-band postcard collection
            # collector attached to switch s1
            self.addHost('h0', **collector_config)
            self.addLink('s1', 'h0', **flink_config)

def main():
    topo = LineTopo(args.n, args.bw, args.in_band, args.no_debug)

    # dump topo in json format
    if args.in_band:
        dump_in_band_topo(topo, args.topo_outfile)
    else:
        dump_out_band_topo(topo, args.topo_outfile)

    net = Mininet(topo=topo, switch=OVSKernelSwitch, controller=RemoteController,
            link=TCLink, ipBase='11.0.0.0/8', autoSetMacs=True, autoStaticArp=True)
    net.start()
    CLI(net)
    net.stop()

if __name__ == "__main__":
    args = parse_args()
    main()
