#include <iostream>
#include <cstdio>
#include <cstring>
#include <crafter.h>

#include "tut/tut.hpp"
#include "packet.hh"
#include "helper.hh"
#include "netsight.hh"
#include "filter/regexp.hh"

namespace tut 
{
    using namespace std;

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
            sprintf(tag, "00:00:%02d:00:%02d:%02d", version, outport, dpid);
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

    struct filter_data {
        int chain_len;
        char regex_str[255];
        PostcardList *pl;
        filter_data()
        {
            chain_len = 5;
            pl = gen_chain_pcards(chain_len);
        }
        ~filter_data()
        {
            pl->clear();
            free(pl);
        }
    };
    
    typedef test_group<filter_data> filter_tg;
    typedef filter_tg::object filtertestobject;

    filter_tg filter_test_group("filter test");

    template<> template<>
    void
    filtertestobject::test<1>()
    {
        set_test_name("match: .*");
        sprintf(regex_str, ".*");
        PacketHistoryFilter phf(regex_str);
        int m = phf.match(*pl);
        ensure("match", m > 0);
    }

    template<> template<>
    void
    filtertestobject::test<2>()
    {
        set_test_name("match: .*X");
        sprintf(regex_str, ".*{{ --bpf ip --dpid %d --outport 1 }}", chain_len);
        PacketHistoryFilter phf(regex_str);
        int m = phf.match(*pl);
        ensure("match", m > 0);
    }

    template<> template<>
    void
    filtertestobject::test<3>()
    {
        set_test_name("non-match: .*X");
        PostcardList *pl = gen_chain_pcards(chain_len);
        sprintf(regex_str, ".*{{ --bpf ip --dpid %d }}", chain_len+1);
        PacketHistoryFilter phf(regex_str);
        int m = phf.match(*pl);
        ensure("non-match", m == 0);
    }

    template<> template<>
    void
    filtertestobject::test<4>()
    {
        set_test_name("match: X.*X");
        sprintf(regex_str, "{{ --bpf ip --dpid 1 }}.*{{ --bpf ip --dpid %d }}", chain_len);
        PacketHistoryFilter phf(regex_str);
        int m = phf.match(*pl);
        ensure("match", m > 0);
    }

    template<> template<>
    void
    filtertestobject::test<5>()
    {
        set_test_name("match: X.*X.*X");
        sprintf(regex_str, "{{ --bpf ip --dpid 1 }}.*{{ --bpf ip --dpid %d }}.*{{ --bpf ip --dpid %d }}", chain_len/2, chain_len);
        PacketHistoryFilter phf(regex_str);
        int m = phf.match(*pl);
        ensure("match", m > 0);
    }

}


