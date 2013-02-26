#ifndef TEST_UTIL_HH
#define TEST_UTIL_HH

#include <sstream>
#include "netsight.hh"

using namespace std;

PostcardList *gen_chain_pcards(int chain_len);
void make_chain_topo(int chain_len, stringstream &ss);
void shuffle(PostcardList *pl);

#endif //TEST_UTIL_HH
