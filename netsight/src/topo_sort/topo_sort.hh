#ifndef TOPO_SORT_HH
#define TOPO_SORT_HH

#include <cstdlib>
#include <ctime>
#include <unordered_map>
#include <utility>

#include "packet.hh"
#include "picojson.h"
#include "helper.hh"
#include "postcard.hh"

using namespace std;

class Topology {
    private:
        unordered_map<int, unordered_map<int, int> > neighbors;
        unordered_map<int, unordered_map<int, pair<int, int> > > ports;
        void add_node(int dpid);
        void add_edge(int dpid1, int port1, int dpid2, int port2);
    public:
        int get_neighbor(int dpid, int outport)
        {
            if(likely(neighbors.find(dpid) != neighbors.end() && neighbors[dpid].find(outport) != neighbors[dpid].end()))
                return neighbors[dpid][outport];
            return 0;
        }
        unordered_map<int, int> &get_neighbor_map(int dpid) 
        {
            return neighbors[dpid];
        }

        void set_neighbor(int dpid, int outport, int nbr)
        {
            neighbors[dpid][outport] = nbr;
        }

        pair<int, int> &get_ports(int src_dpid, int dst_dpid)
        {
            return ports[src_dpid][dst_dpid];
        }

        void read_topo(istream &in);
};

void topo_sort(PostcardList *pl, Topology &topo);

#endif
