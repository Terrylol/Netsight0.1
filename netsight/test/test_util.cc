#include <crafter.h>

#include "test_util.hh"
#include "helper.hh"
#include "types.hh"

PostcardList *gen_chain_pcards(int chain_len)
{
    //printf("Generating chain postcards of length %d\n", chain_len);
    PostcardList *pl = new PostcardList();
    int i = 0;
    for(i = 0; i < chain_len; i++) {
        int dpid = i+1;
        int outport = 1;
        int version = 1;
        char tag[18];
        sprintf(tag, "00:00:%02x:00:%02x:%02x", version, outport, dpid);
        //printf("version %d, outport %d, dpid %d, tag \"%s\"\n", version, outport, dpid, tag);

        /* Create an ethernet header */
        Crafter::Ethernet eth_header;
        eth_header.SetDestinationMAC(tag);
        eth_header.SetSourceMAC("00:00:00:00:00:01");
        eth_header.SetType(0x800);

        /* Create an IP header */
        Crafter::IP ip_header;
        /* Set the Source and Destination IP address */
        ip_header.SetSourceIP("10.0.0.1");
        ip_header.SetDestinationIP("10.0.0.2");

        /* Create an TCP - SYN header */
        Crafter::TCP tcp_header;
        tcp_header.SetSrcPort(11);
        tcp_header.SetDstPort(80);
        tcp_header.SetSeqNumber(Crafter::RNG32());
        tcp_header.SetFlags(Crafter::TCP::SYN);

        /* A raw layer, this could be any array of bytes or chars */
        Crafter::RawLayer payload("ArbitraryPayload");

        /* Create a packets */
        //printf("Crafting the postcard\n");
        Crafter::Packet tcp_packet = eth_header / ip_header / tcp_header / payload;
        tcp_packet.PreCraft();

        Packet *pkt = new Packet(tcp_packet.GetBuffer(), tcp_packet.GetSize(), 0, i, tcp_packet.GetSize());
        //printf("Adding the postcard to the PostcardList as a PostcardNode\n");
        pl->push_back(new PostcardNode(pkt));
        //printf("version %d, outport %d, dpid %d\n", pl->tail->version, pl->tail->outport, pl->tail->dpid);
    }

    return pl;
}

void 
make_chain_topo(int chain_len, stringstream &ss)
{
    picojson::object v, topo; 
    picojson::array nodes, links;

    // add nodes
    for(int i = 0; i < chain_len; i++) {
        picojson::object n;
        n["dpid"] = V((u64)(i + 1));
        stringstream name;
        name << i+1;
        n["name"] = V(name.str());
        nodes.push_back(V(n));
    }

    // add links
    for(int i = 1; i < chain_len; i++) {
        int outport = 1;
        int inport = 2;
        picojson::object e;
        e["src_dpid"] = V((u64)i);
        e["src_port"] = V((u64)outport);
        e["dst_dpid"] = V((u64)(i + 1));
        e["dst_port"] = V((u64)inport);
        links.push_back(V(e));
    }

    topo["nodes"] = V(nodes);
    topo["links"] = V(links);
    v["topo"] = V(topo);

    ss << V(v).serialize();
    cout << ss.str() << endl;
}

void shuffle(PostcardList *pl)
{
    // populate pn_vec
    vector<PostcardNode *> pn_vec;
    PostcardNode *pn = pl->head;
    while(pn) {
        pn_vec.push_back(pn);
        PostcardNode *pn_next = pn->next;
        pn->prev = pn->next = NULL;
        pn = pn_next;
    }

    assert(pn_vec.size() == pl->length);

    //shuffle contents of pn_vec
    srand(time(NULL));
    for(unsigned int i = 0; i < pn_vec.size(); i++) {
        int k = pn_vec.size() - i;
        int j = i + (rand() % k);

        //swap i, j
        PostcardNode *tmp = pn_vec[j];
        pn_vec[j] = pn_vec[i];
        pn_vec[i] = tmp;
    }

    //setup pointers
    for(unsigned int i = 0; i < pn_vec.size() - 1; i++) {
        pn_vec[i]->next = pn_vec[i+1];
        pn_vec[i+1]->prev = pn_vec[i];
    }
    if(pn_vec.size() > 0) {
        pl->head = pn_vec[0];
        pl->tail = pn_vec[pn_vec.size() - 1];
    }
    if(pl->length > 0) {
        assert(pl->head->prev == NULL);
        assert(pl->tail->next == NULL);
    }
}

