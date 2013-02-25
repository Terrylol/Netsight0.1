#!/usr/bin/python
from collections import defaultdict
import networkx as nx
from networkx.algorithms.mst import minimum_spanning_tree
import json

'''Topology Module:
    Read topology in json format
    Store topology as a Graph object
    To be used for:
        - Postcard collection: in-band and out-of-band
        - Constructing Backtrace

Topo json format:
{
    "topo": {
                "nodes": [ {"dpid":dpid1, "name":name1},
                           {"dpid":dpid2, "name":name2},
                           ...
                ]

                "links": [{"src_dpid": dpid1, "src_port": port1, 
                            "dst_dpid": dpid2, "dst_port": port2}, 
                            ...
                ]
    }
    "collector": {
                "dpid": dpid
                "port": port
    }
    OR
    "collector": [ {"dpid": dpid1, "port": port1}, 
                    ... 
                    {"dpid": dpidN, "port": portN}
    ]

}
'''

# TODO:
# Check datapath id format
# Unit tests

class NDBTopo (object):
    '''NDB topology'''
    def __init__(self, filename=None, topo_json=None, topo=None, sw_to_dpid=None):
        # topology 
        # Assume bi-directional links
        self.g = nx.Graph()   

        # in-band postcard collection?
        self.in_band = False

        # collector location
        # used only for in-band 
        self.collector_loc = {}             

        # Spanning tree over which to collect postcards
        # used only for in-band 
        self.collector_st = None

        # Port on each switch out of which to dump packets to send to collector
        self.collector_dump_ports = {}

        self.topo_fname = filename
        if filename:
            topo_f = open(filename, 'r')
            self.topo_json = json.load(topo_f)
            topo_f.close()
            self.load_json()

        if topo_json:
            self.topo_json = topo_json
            self.load_json()

        if topo and sw_to_dpid:
            self.load_topo(topo, sw_to_dpid)

    def load_json(self):
        # Load topology -- nodes and links
        topo_nodes = self.topo_json['topo']['nodes']
        topo_links = self.topo_json['topo']['links']
        for n in topo_nodes:
            name = n['name'] 
            dpid = n['dpid'] 
            self.g.add_node(dpid)
        for e in topo_links:
            src_dpid = e['src_dpid'] 
            src_port = e['src_port']
            dst_dpid = e['dst_dpid']
            dst_port = e['dst_port']
            self.g.add_edge(src_dpid, dst_dpid, {src_dpid:src_port, dst_dpid:dst_port})

        # Load collector location
        collector = self.topo_json['collector']
        if type(collector) == list:
            # out-of-band collector
            self.in_band = False
            for cloc in collector:
                dpid = cloc['dpid']
                port = cloc['port']
                self.collector_dump_ports[dpid] = port
        elif type(collector) == dict:
            # in-band collector
            self.in_band = True
            self.collector_loc = dict(collector)
            self.get_collector_st()
            self.print_collector_st()

    def load_topo(self, topo, sw_to_dpid):
        # Load topology -- nodes and links
        switches = topo.switches()
        for sw in switches:
            dpid = sw_to_dpid[sw]
            self.g.add_node(dpid)
        for (src, dst) in topo.links():
            if src in switches and dst in switches:
                src_dpid = sw_to_dpid[src]
                src_port = topo.port(src, dst)[0]
                dst_dpid = sw_to_dpid[dst]
                dst_port = topo.port(src, dst)[1]
                self.g.add_edge(src_dpid, dst_dpid, {src_dpid:src_port, dst_dpid:dst_port})
        # TODO: add collector option

    # Create a spanning tree to the collector
    # Populate collector_dump_ports using breadth first search 
    def get_collector_st(self):
        '''Compute spanning tree to collector
        Populate collector_dump_ports'''

        # create spanning tree
        # copy edge properties; minimum_spanning_tree doesn't do it automatically
        mst = minimum_spanning_tree(self.g)
        self.collector_st = nx.Graph()
        for n in self.g.nodes():
            self.collector_st.add_node(n)
        for (u, v) in mst.edges():
            self.collector_st.add_edge(u, v, self.g[u][v])

        # BFS to compute collector_dump_ports
        print 'BFS to create collector spanning tree:'
        bfs_q = []
        seen_nodes = []
        bfs_q.append(self.collector_loc)
        while len(bfs_q) > 0:
            print bfs_q
            curr_sw = bfs_q.pop(0)
            curr_dpid = curr_sw['dpid']
            curr_port = curr_sw['port']
            for neighbor in self.collector_st[curr_dpid]:
                if neighbor in seen_nodes:
                    continue
                neighbor_port = self.collector_st[curr_dpid][neighbor][neighbor]
                bfs_q.append({'dpid':neighbor, 'port':neighbor_port})
            self.collector_dump_ports[curr_dpid] = curr_port
            seen_nodes.append(curr_dpid)

    def print_collector_st(self):
        print 'Collector location: %s' % str(self.collector_loc)
        print 'Spanning tree:'
        for (u,v) in self.collector_st.edges():
            print '\t%d:%d --- %d:%d' % (u, self.collector_st[u][v][u], v, self.collector_st[u][v][v])
        print 'Collector dump ports:'
        for (dpid, port) in self.collector_dump_ports.iteritems():
            print 'dpid: %d, port: %d' % (dpid, port)


class NDBTopoFastSort(object):
    '''NDB topology optimized for fast sorting.'''
    def __init__(self, input_topo):
        """
        inputs:
        input_topo: NDBTopo.
        """
        self.opposite = {}  # [dpid][outport] = opposing dpid
        for src in input_topo.g:
            for dst in input_topo.g[src]:
                outport = input_topo.g[src][dst][src]
                inport = input_topo.g[src][dst][dst]
                if src not in self.opposite:
                    self.opposite[src] = {}
                self.opposite[src][outport] = dst
                if dst not in self.opposite:
                    self.opposite[dst] = {}
                self.opposite[dst][inport] = src

        self.port = {}  # [dpid][dpid]
        for src in input_topo.g:
            for dst in input_topo.g[src]:
                outport = input_topo.g[src][dst][src]
                inport = input_topo.g[src][dst][dst]
                if src not in self.port:
                    self.port[src] = {}
                self.port[src][dst] = outport
                if dst not in self.port:
                    self.port[dst] = {}
                self.port[dst][src] = inport

if __name__ == '__main__':
    '''Unit tests'''
    # TODO
    pass
