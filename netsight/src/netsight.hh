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
#include "helper.hh"
#include "zhelpers.hh"

#include "db_handler.hh"
#include "filter/regexp.hh"
#include "topo_sort/topo_sort.hh"
#include "api/api.hh"

#define ROUND_LENGTH 1000 //round length in ms
#define PACKET_HISTORY_PERIOD 100 //packet history duration in ms

#define NETSIGHT_CONTROL_PORT 5566 
#define NETSIGHT_HISTORY_PORT 5567
#define POLL_TIMEOUT_MS 100 //timeout zmq poll every 100ms

using namespace std;

class NetSight {
    private:
        string sniff_dev;
        string topo_filename;

        /* private data structures */
        Topology topo;
        unordered_map<int, int> psid_to_dpid;

        struct bpf_program postcard_fp;  /* compiled filter program (expression) */
        pcap_t *postcard_handle;         /* packet capture handle */

        /* MongoDB handlers */
        MongoHandler ft_db;
        MongoHandler psid_db;
        MongoHandler topo_db;
        void read_topo_db();
        void read_psid_db();
        void get_flow_entry(int dpid, int version, string &match, vector<string> &actions);

        /* API Sockets */
        zmq::context_t context;
        zmq::socket_t pub_sock;
        zmq::socket_t rep_sock;
        bool s_interrupted;

        /* Signals that are handled by various threads */
        sigset_t sigset;

        vector<string> regexes;
        vector<PacketHistoryFilter> filters;
        PostcardList stage;
        PathTable path_table;
        pthread_mutex_t stage_lock;
        pthread_t postcard_t;
        pthread_t history_t;

        /* NetSight is a singleton class*/
        NetSight();
        NetSight(NetSight const&); // Don't Implement
        void operator=(NetSight const&); // Don't implement

        /* signal handler thread */
        static void sig_handler(int signum);

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
        void serve();

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

        void connect_db(string host);
};

#endif //NETSIGHT_HH
