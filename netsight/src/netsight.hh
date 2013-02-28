#ifndef NETSIGHT_HH
#define NETSIGHT_HH

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <csignal>

#include <pcap.h>
#include <pthread.h>

#include "packet.hh"
#include "postcard.hh"
#include "path_table.hh"
#include "flow.hh"
#include "types.hh"

#include "filter/regexp.hh"
#include "topo_sort/topo_sort.hh"

#define ROUND_LENGTH 1000 //round length in ms

using namespace std;

class NetSight {
    private:
        string sniff_dev;
        string topo_filename;
        Topology topo;

        struct bpf_program postcard_fp;  /* compiled filter program (expression) */
        pcap_t *postcard_handle;         /* packet capture handle */

        /* Signals that are handled by various threads */
        sigset_t sigset;
        pthread_t sig_handler_t;

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

        /* signal handler thread */
        static void *sig_handler(void *args);

        /* postcard worker thread */
        static void *postcard_worker(void *args);
        void *run_postcard_worker(void *args);

        /* history worker thread */
        static void *history_worker(void *args);
        void *run_history_worker(void *args);

        void sniff_pkts(const char *dev);
        void postcard_handler(const struct pcap_pkthdr *header, const u_char *packet);
        void cleanup();
        void interact();

    public:
        void start();
        static NetSight &get_instance()
        {
            static NetSight n;
            return n;
        }

        void set_sniff_dev(string s)
        { 
            sniff_dev = s;
        }

        void set_topo_filename(string s)
        {
            topo_filename = s;
            fstream fs(topo_filename.c_str(), ios::in);
            topo.read_topo(fs);
        }
};

#endif //NETSIGHT_HH
