#include <fstream>
#include <string>
#include <cerrno>

#include "topo_sort.hh"
#include "helper.hh"
#include "types.hh"

#define OFPP_FLOOD 65531
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

    neighbors.clear();

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
        add_node(dpid);
    }

    picojson::array links_list = links_j.get<picojson::array>();
    for (picojson::array::iterator iter = links_list.begin(); iter != links_list.end(); ++iter) {
        u64 src_dpid = (u64) (*iter).get("src_dpid").get<double>();
        u64 src_port = (u64) (*iter).get("src_port").get<double>();
        u64 dst_dpid = (u64) (*iter).get("dst_dpid").get<double>();
        u64 dst_port = (u64) (*iter).get("dst_port").get<double>();
        add_edge(src_dpid, src_port, dst_dpid, dst_port);
    }
}

void Topology::add_node(int dpid)
{
    if(neighbors.find(dpid) == neighbors.end()) {
        neighbors[dpid] = unordered_map<int, int>();
    }
}

void Topology::add_edge(int dpid1, int port1, int dpid2, int port2)
{
    add_node(dpid1);
    add_node(dpid2);
    set_neighbor(dpid1, port1, dpid2);
    set_neighbor(dpid2, port2, dpid1);
    ports[dpid1][dpid2] = make_pair(port1, port2);
}

static inline PostcardNode *get_next(unordered_map<int, vector<PostcardNode*> > &locs, int dpid=0)
{
    if(dpid) {
        unordered_map<int, vector<PostcardNode*> >::iterator it = locs.find(dpid);
        if(it != locs.end()) {
            vector<PostcardNode*> &pn_vec = it->second;
            if(pn_vec.size() > 0) {
                PostcardNode *pn = pn_vec[0];
                pn_vec.erase(pn_vec.begin());
                if(pn_vec.empty())
                    locs.erase(it);
                return pn;
            }
        }
        return NULL;
    }
    else {
        EACH(it, locs) {
            vector<PostcardNode*> &pn_vec = it->second;
            if(pn_vec.size() > 0) {
                PostcardNode *pn = pn_vec[0];
                pn_vec.erase(pn_vec.begin());
                if(pn_vec.empty())
                    locs.erase(it);
                return pn;
            }
        }
    }

    return NULL;
}

void 
topo_sort(PostcardList *pl, Topology &topo)
{
    unordered_map<int, vector<PostcardNode*> > locs;
    DBG("Topo-sorting:\n%s\n", pl->str().c_str());

    // populate locs
    DBG("PostcardList pl size: %d\n", pl->length);
    DBG("Populating locs datastructure\n");
    PostcardNode *curr = pl->head;
    while(curr != NULL) {
        (locs[curr->dpid]).push_back(curr);
        PostcardNode *next = curr->next;
        curr->next = curr->prev = NULL; //reset the pointers
        curr = next;
    }

    //reset the fields of pl
    DBG("locs size: %d\n", locs.size());
    DBG("Resetting the fields of pl\n");
    pl->head = pl->tail = NULL;
    pl->length = 0;
    curr = NULL;
    while((curr = get_next(locs)) != NULL) {
        DBG("Current size of locs: %d\n", locs.size());
        DBG("curr->dpid = %d\n", curr->dpid);
        PostcardList tmp_pl;
        while(curr != NULL) {
            tmp_pl.push_back(curr);
            PostcardNode *nxt = NULL;
            if(curr->outport == OFPP_FLOOD) {
                EACH(nit, topo.get_neighbor_map(curr->dpid)) {
                    int nbr = nit->second;
                    // remove the postcard with dpid=nbr from locs
                    PostcardNode *tmp_nxt = get_next(locs, nbr);
                    if(tmp_nxt) {
                        curr->next = tmp_nxt;
                        tmp_nxt->prev = curr;
                        tmp_nxt->inport = topo.get_ports(curr->dpid, tmp_nxt->dpid).second;
                        // ensure that we're not going in the reverse direction
                        if(tmp_nxt->outport != topo.get_ports(tmp_nxt->dpid, curr->dpid).first) {
                            nxt = tmp_nxt;
                            break;
                        }
                        else {
                            // if we are put tmp_nxt back into locs
                            vector<PostcardNode*>::iterator it = (locs[tmp_nxt->dpid]).begin();
                            (locs[tmp_nxt->dpid]).insert(it, tmp_nxt);
                        }
                    }
                }
            }
            else {
                int nbr = topo.get_neighbor(curr->dpid, curr->outport);
                if(nbr) {
                    // remove the postcard with dpid=nbr from locs
                    nxt = get_next(locs, nbr);
                    if(nxt) {
                        curr->next = nxt;
                        nxt->prev = curr;
                        nxt->inport = topo.get_ports(curr->dpid, nxt->dpid).second;
                    }
                }
            }
            curr = nxt;
        }
        
        //prepend tmp_pl to pl
        DBG("Prepending tmp_pl to pl:\n%s\n", tmp_pl.str().c_str());
        if(pl->head) {
            if(tmp_pl.tail) {
                tmp_pl.tail->next = pl->head;
                pl->head->prev = tmp_pl.tail;
                if(topo.get_neighbor((tmp_pl.tail)->dpid, (tmp_pl.tail)->outport) == (pl->head)->dpid) {
                    //update the inport of pl->head
                    (pl->head)->inport = topo.get_ports((tmp_pl.tail)->dpid, (pl->head)->dpid).second;
                }
            }
            if(tmp_pl.head) {
                pl->head = tmp_pl.head;
            }
        }
        else {
            pl->head = tmp_pl.head;
        }
        if(!pl->tail) {
            pl->tail = tmp_pl.tail;
        }
        pl->length += tmp_pl.length;
        DBG("End of iteration: PL length: %d\n", pl->length);
    }

    DBG("Done Topo-sorting:\n%s\n", pl->str().c_str());
}

