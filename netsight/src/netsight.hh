#ifndef NETSIGHT_HH
#define NETSIGHT_HH

#include <vector>
#include <string>
#include <unordered_map>
#include <csignal>

#include <pcap.h>
#include <pthread.h>

#include "packet.hh"
#include "postcard.hh"
#include "path_table.hh"
#include "flow.hh"
#include "types.hh"

#include "filter/regexp.hh"

#define ROUND_LENGTH 1000 //round length in ms

using namespace std;

class NetSight {
    private:

        vector<string> regexes;
        vector<PacketHistoryFilter> filters;
        PostcardList stage;
        PathTable path_table;
        pthread_mutex_t stage_lock;
        pthread_cond_t round_cond;
        pthread_t postcard_t;
        pthread_t history_t;

        /* NetSight is a singleton class*/
        NetSight();
        NetSight(NetSight const&); // Don't Implement
        void operator=(NetSight const&); // Don't implement

        /* static thread and signal handler functions */
        static void sig_handler(int signum);
        static void *postcard_worker(void *args)
        {
            NetSight &n = NetSight::get_instance();
            return n.run_postcard_worker(args);
        }

        static void *history_worker(void *args)
        {
            NetSight &n = NetSight::get_instance();
            return n.run_history_worker(args);
        }

        void sniff_pkts(const char *dev);
        void postcard_handler(const struct pcap_pkthdr *header, const u_char *packet);
        void *run_postcard_worker(void *args);
        void *run_history_worker(void *args);

    public:
        static NetSight &get_instance()
        {
            static NetSight n;
            return n;
        }

        void interact();
};

#endif //NETSIGHT_HH
