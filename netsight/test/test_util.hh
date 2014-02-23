/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#ifndef TEST_UTIL_HH
#define TEST_UTIL_HH

#include <sstream>
#include "postcard.hh"

using namespace std;

PostcardList *gen_chain_pcards(int chain_len);
void make_chain_topo(int chain_len, stringstream &ss);
void shuffle(PostcardList *pl);

#endif //TEST_UTIL_HH
