#include <fstream>
#include <string>
#include <cerrno>

#include "topo_sort.hh"
#include "helper.hh"
#include "types.hh"

using namespace std;

void
Topology::read_topo(const char *filename)
{

    /*
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
    */

    ifstream in_f(filename, ios::in | ios::binary);
    picojson::value v, topo_j, nodes_j, links_j;

    string err = picojson::parse(v, in_f);
    topo_j = v.get("topo");
    nodes_j = topo_j.get("nodes");
    links_j = topo_j.get("links");
    
    picojson::array nodes_list = nodes_j.get<picojson::array>();
    for (picojson::array::iterator iter = nodes_list.begin(); iter != nodes_list.end(); ++iter) {
        int dpid = (*iter).get("dpid").get<ull>();
        g[dpid] = unordered_map<int, int>();
    }

    picojson::array links_list = links_j.get<picojson::array>();
    for (picojson::array::iterator iter = links_list.begin(); iter != links_list.end(); ++iter) {
        ull src_dpid = (*iter).get("src_dpid").get<ull>();
        ull src_port = (*iter).get("src_port").get<ull>();
        ull dst_dpid = (*iter).get("dst_dpid").get<ull>();
        ull dst_port = (*iter).get("dst_port").get<ull>();
        set_neighbor(src_dpid, src_port, dst_dpid);
        set_neighbor(dst_dpid, dst_port, src_dpid);
    }

}

void 
topo_sort(PostcardList *pl, Topology &topo)
{
    unordered_map<int, PostcardNode*> locs;

    // populate locs
    PostcardNode *curr = pl->head;
    while(curr != NULL) {
        if (locs.find(curr->dpid) != locs.end()) {
            fprintf(stderr, "Loop detected.  Can't do toposort with cycles.\n");
            assert(false);
        }
        locs[curr->dpid] = curr;
        curr = curr->next;
    }

    int i = 0;
    EACH(it, locs) {
        PostcardNode *curr = it->second;
        int nbr = topo.get_neighbor(curr->dpid, curr->outport);
        if(nbr) {
            PostcardNode *nxt = locs[nbr];
            curr->next = nxt;
            if (nxt)
                nxt->prev = curr;
        }
    }

    EACH(it, locs) {
        if (it->second->prev == NULL)
            pl->head = it->second;
        if (it->second->next == NULL)
            pl->tail = it->second;
    }
}

