#ifndef __PATH_TABLE_HH__
#define __PATH_TABLE_HH__

#include <unordered_map>
#include "types.hh"
#include "flow.hh"
#include "netsight.hh"

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
