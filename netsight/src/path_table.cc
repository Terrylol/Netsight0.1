/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#include <cstdint>

#include "postcard.hh"
#include "path_table.hh"
#include "packet.hh"
#include "flow.hh"

void 
PathTable::insert_postcard(PostcardNode *p)
{
    Packet *pkt = p->pkt;
    FlowKey key(*pkt);
    PostcardList &pl = table[key.hsh];
    pl.push_back(p);
}
