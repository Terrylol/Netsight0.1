#include <vector>
#include <pthread.h>
#include "netsight.hh"
#include "path_table.hh"
#include "filter/regexp.hh"

using namespace std;

extern vector<string> regexes;
extern vector<PacketHistoryFilter> filters;
extern PathTable path_table;
extern PostcardList stage;
extern pthread_mutex_t stage_lock;
extern pthread_cond_t round_cond;

void*
run_history_worker(void *args)
{
    while(true) {
        pthread_mutex_lock(&stage_lock);
        pthread_cond_wait(&round_cond, &stage_lock);

        // Empty stage and populate path_table
        PostcardNode *pn = stage.head;
        for(int i = 0; i < stage.length; i++) {
            PostcardNode *p = stage.remove(pn);
            path_table.insert_postcard(p);
            pn = pn->next;
        }

        pthread_mutex_unlock(&stage_lock);

        //TODO:
        // For each old enough PostcardList:
        // - topo-sort the PostcardList
        // - create a PackedPostcardList, and
        // - match against all the filters

    }
}
