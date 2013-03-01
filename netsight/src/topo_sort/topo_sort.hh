#ifndef TOPO_SORT_HH
#define TOPO_SORT_HH

#include <cstdlib>
#include <ctime>
#include <unordered_map>

#include "packet.hh"
#include "picojson.h"
#include "helper.hh"
#include "postcard.hh"

using namespace std;

class Topology {
    private:
        unordered_map<int, unordered_map<int, int> > g;
        void add_node(int dpid);
        void add_edge(int dpid1, int port1, int dpid2, int port2);
    public:
        int get_neighbor(int dpid, int outport)
        {
            if(likely(g.find(dpid) != g.end() && g[dpid].find(outport) != g[dpid].end()))
                return g[dpid][outport];
            return 0;
        }

        void set_neighbor(int dpid, int outport, int nbr)
        {
            g[dpid][outport] = nbr;
        }
        void read_topo(istream &in);
};

void topo_sort(PostcardList *pl, Topology &topo);

#endif
