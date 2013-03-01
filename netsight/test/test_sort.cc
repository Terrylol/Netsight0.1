#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <crafter.h>

#include "tut/tut.hpp"
#include "packet.hh"
#include "helper.hh"
#include "picojson.h"
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
        shuffle(pl);
        topo_sort(pl, t);
        ensure("postcard list length sanity", pl->length == chain_len);
        PostcardNode *curr = pl->head;
        while(curr && curr->next) {
            ensure("dpid ordering", curr->dpid = curr->next->dpid - 1);
            curr = curr->next;
        }
    }

    template<> template<>
    void
    sorttestobject::test<2>()
    {
        set_test_name("sort: incomplete packet history");
        int chain_len = 10;
        stringstream ss(stringstream::in | stringstream::out); 
        make_chain_topo(chain_len, ss);
        t.read_topo(ss);
        pl = gen_chain_pcards(chain_len);
        shuffle(pl);
        pl->remove(pl->head);
        topo_sort(pl, t);

        int new_len = 0;
        PostcardNode *pn = pl->head;
        while(pn) {
            new_len++;
            pn = pn->next;
        }
        ensure("postcard list length sanity", new_len == chain_len-1);
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
