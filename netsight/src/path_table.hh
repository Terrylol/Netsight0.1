/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#ifndef __PATH_TABLE_HH__
#define __PATH_TABLE_HH__

#include <unordered_map>
#include "types.hh"
#include "postcard.hh"
#include "flow.hh"

using namespace std;

/*
 * A path table is a hash map of postcards keyed by the packet ID
 * key: packet ID
 * value: PostcardList
 * */
class PathTable {
    public:
        unordered_map <u64, PostcardList> table;
        void insert_postcard(PostcardNode *pn);
};

#endif //__PATH_TABLE_HH__
