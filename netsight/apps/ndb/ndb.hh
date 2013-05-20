#ifndef NDB_HH
#define NDB_HH

#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

#include "api.hh"
#include "helper.hh"
#include "zhelpers.hh"

using namespace std;

class NDB {
    private:
        /* ZMQ Threads */
        pthread_t control_t;
        pthread_t history_t;

        /* ZMQ context (one per process) */
        zmq::context_t context;

        bool s_interrupted;

        string netsight_host;

        /* Shared datastructure and mutex between main and control threads */
        pthread_mutex_t ctrl_cmd_lock;
        vector<vector<string> > ctrl_cmd_queue;

        /* Shared datastructure and mutex between main and history threads */
        pthread_mutex_t hist_cmd_lock;
        vector<vector<string> > hist_cmd_queue;

        /* Signals that are handled by various threads */
        sigset_t sigset;

        NDB(string host="localhost");
        static void sig_handler(int signum);
        static void *control_channel_thread(void *args);
        static void *history_channel_thread(void *args);
        void cleanup();

    public:
        static NDB &get_instance()
        {
            static NDB n;
            return n;
        }

        void start();
        void interact();
};

#endif // NDB_HH
