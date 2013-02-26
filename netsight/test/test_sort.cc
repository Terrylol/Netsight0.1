#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <crafter.h>

#include "tut/tut.hpp"
#include "packet.hh"
#include "helper.hh"
#include "picojson.h"
#include "netsight.hh"
#include "test_util.hh"
#include "topo_sort/topo_sort.hh"

namespace tut 
{
    using namespace std;

    struct sort_data {
        Topology t;
        PostcardList *pl;
    };

    typedef test_group<sort_data> sort_tg;
    typedef sort_tg::object sorttestobject;

    sort_tg sort_test_group("sort test");

    template<> template<>
    void
    sorttestobject::test<1>()
    {
        set_test_name("sort: line-topo");
        int chain_len = 5;
        stringstream ss(stringstream::in | stringstream::out); 
        make_chain_topo(chain_len, ss);
        t.read_topo(ss);
        pl = gen_chain_pcards(chain_len);
        printf("Chain PostcardList:\n");
        pl->print();
        shuffle(pl);
        printf("Shuffled Chain PostcardList:\n");
        pl->print();
        ensure("postcard list length sanity", pl->length == chain_len);
        topo_sort(pl, t);
        ensure("postcard list length sanity", pl->length == chain_len);
        printf("Sorted Chain PostcardList:\n");
        pl->print();
    }

    template<> template<>
    void
    sorttestobject::test<2>()
    {
        set_test_name("sort: incomplete packet history");
    }

    template<> template<>
    void
    sorttestobject::test<3>()
    {
        set_test_name("sort: loop");
    }

    template<> template<>
    void
    sorttestobject::test<4>()
    {
        set_test_name("sort: multicast");
    }

}
