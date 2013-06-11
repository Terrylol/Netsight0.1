#include <cstdio>
#include "postcard.hh"
#include "helper.hh"

PostcardNode::PostcardNode(Packet *p)
{
    prev = next = NULL;
    pkt = p;
    dpid = get_dpid();
    inport = -1;
    outport = get_outport();
    version = get_version();
}

int 
PostcardNode::get_dpid()
{
    u8 dpid = 0; 
    memcpy(&dpid, &(pkt->eth.dst[5]), DPID_TAG_LEN);
    return (int)dpid;
}

int 
PostcardNode::get_outport()
{
    u16 outport = 0;
    memcpy(&outport, &(pkt->eth.dst[3]), OUTPORT_TAG_LEN);
    outport = ntohs(outport);
    return (int)outport;
}

int 
PostcardNode::get_version()
{
    u32 version = 0;
    memcpy(&version, &(pkt->eth.dst[0]), VERSION_TAG_LEN);
    version = ntohl(version);
    version = version >> ((sizeof(version) - VERSION_TAG_LEN)*8);
    return (int)version;
}

string 
PostcardNode::str()
{
    stringstream ss;
    ss << "Switch: dpid: " << dpid  << hex << "(0x" << dpid << dec << ")" << endl;
    ss << "\t" << "packet: " << pkt->str_hex() << endl;
    ss << "\t" << "match: " << endl;
    ss << "\t" << "action: " << endl;
    ss << "\t" << "version: " << version << endl;
    ss << "\t" << "inport: " << inport << endl;
    ss << "\t" << "outport: " << outport << endl;
    return ss.str();
}

JSON
PostcardNode::json()
{
    JSON j;
    j["dpid"] = V((u64)dpid);
    j["inport"] = V((u64)inport);
    j["outport"] = V((u64)outport);
    j["version"] = V((u64)version);
    j["packet"] = V(pkt->json());
    return j;
}

PostcardNode *
PostcardNode::decode_json(picojson::value &pn_j)
{
    picojson::value pkt_j = pn_j.get("packet");
    string pkt_hex = pkt_j.get("buff").get<string>();
    size_t pkt_bufsize;
    u8 *pkt_buf = (u8*) malloc(pkt_hex.size()/2);
    bzero(pkt_buf, sizeof(pkt_buf));
    byteify_packet(pkt_hex.data(), pkt_buf, &pkt_bufsize);

    Packet *pkt = new Packet(pkt_buf, pkt_bufsize, 0, 0, pkt_bufsize, true);
    PostcardNode *pn = new PostcardNode(pkt);
    pn->dpid = (int) (pn_j.get("dpid").get<double>());
    pn->inport = (int) (pn_j.get("inport").get<double>());
    pn->dpid = (int) (pn_j.get("outport").get<double>());
    pn->version = (int) (pn_j.get("version").get<double>());

    free(pkt_buf);
    return pn;
}

/*
 * insert p after loc
 * if loc is NULL, insert at the head of the list
 * */
void 
PostcardList::insert(PostcardNode *p, PostcardNode *loc)
{
    if(loc == NULL) {
        p->next = head;
        if(p->next != NULL) {
            p->next->prev = p;
        }
        head = p;
        if(tail == NULL)
            tail = p;
    }
    else if(loc == tail) {
        p->prev = loc;
        loc->next = p;
        tail = p;
    }
    else {
        p->next = loc->next;
        p->prev = loc;
        p->next->prev = p;
        loc->next = p;
    }
    DBG("Inserting postcard: length = %d\n", length);
    length++;
}

PostcardNode*
PostcardList::remove(PostcardNode *p)
{
    if(p->prev != NULL) {
        p->prev->next = p->next;
    }
    else {
        head = p->next;
    }

    if(p->next != NULL) {
        p->next->prev = p->prev;
    }
    else {
        tail = p->prev;
    }
    p->next = p->prev = NULL;
    length--;
    DBG("Removing postcard: length = %d\n", length);
    return p;
}

void 
PostcardList::clear()
{
    PostcardNode *p = head;
    while(p != NULL) {
        PostcardNode *next = p->next;
        free(p->pkt);
        free(p);
        p = next;
    }
    head = tail = NULL;
    length = 0;
}

string
PostcardList::str()
{
    string s;
    PostcardNode *pn = head;
    while(pn) {
        s += pn->str();
        pn = pn->next;
    }
    return s;
}

JSON 
PostcardList::json()
{
    JSON j;
    vector<picojson::value> v;
    PostcardNode *pn = head;
    while(pn) {
        v.push_back(V(pn->json()));
        pn = pn->next;
    }

    j["postcard_list"] = V(v);
    return j;
}

PostcardList *
PostcardList::decode_json(picojson::value &message_j)
{
    PostcardList *pl = new PostcardList();
    picojson::value pl_j = message_j.get("postcard_list");
    picojson::array &pl_j_vec = pl_j.get<picojson::array>();
    for (picojson::array::iterator iter = pl_j_vec.begin(); iter != pl_j_vec.end(); ++iter) {
        pl->push_back(PostcardNode::decode_json(*iter));
    }
    return pl;
}

