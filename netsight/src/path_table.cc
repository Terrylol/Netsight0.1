#include <cstdint>

#include "path_table.hh"
#include "netsight.hh"

void 
PathTable::insert_postcard(PostcardNode *p)
{
    Packet *pkt = p->pkt;
    FlowKey key(*pkt);
    PostcardList &pl = table[key.hsh];
    pl.push_back(p);
}
