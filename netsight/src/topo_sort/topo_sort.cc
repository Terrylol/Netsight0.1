#include <fstream>
#include <string>
#include <cerrno>

#include "topo_sort.hh"
#include "helper.hh"
#include "types.hh"

using namespace std;

void
Topology::read_topo(istream &in)
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

    picojson::value v, topo_j, nodes_j, links_j;

    string err = picojson::parse(v, in);

    topo_j = v.get("topo");
    nodes_j = topo_j.get("nodes");
    links_j = topo_j.get("links");
    
    picojson::array &nodes_list = nodes_j.get<picojson::array>();
    for (picojson::array::iterator iter = nodes_list.begin(); iter != nodes_list.end(); ++iter) {
        //NOTE: For some weird reason, picojson reads int values in the json
        //string as double
        int dpid = (int) (*iter).get("dpid").get<double>();
        g[dpid] = unordered_map<int, int>();
    }

    picojson::array links_list = links_j.get<picojson::array>();
    for (picojson::array::iterator iter = links_list.begin(); iter != links_list.end(); ++iter) {
        u64 src_dpid = (u64) (*iter).get("src_dpid").get<double>();
        u64 src_port = (u64) (*iter).get("src_port").get<double>();
        u64 dst_dpid = (u64) (*iter).get("dst_dpid").get<double>();
        u64 dst_port = (u64) (*iter).get("dst_port").get<double>();
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
        PostcardNode *next = curr->next;
        curr->next = curr->prev = NULL; //reset the pointers
        curr = next;
    }

    EACH(it, locs) {
        PostcardNode *curr = it->second;
        assert(curr->dpid == it->first);
        int nbr = topo.get_neighbor(curr->dpid, curr->outport);
        printf("Checking neighbor of %d:\n", curr->dpid);
        if((nbr > 0) && (locs.find(nbr) != locs.end())) {
            PostcardNode *nxt = locs[nbr];
            printf("%d--%d\n", curr->dpid, nxt->dpid);
            curr->next = nxt;
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

