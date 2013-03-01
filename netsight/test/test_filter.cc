#include <iostream>
#include <cstdio>
#include <cstring>

#include "tut/tut.hpp"
#include "packet.hh"
#include "helper.hh"
#include "filter/regexp.hh"
#include "test_util.hh"

namespace tut 
{
    using namespace std;

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


