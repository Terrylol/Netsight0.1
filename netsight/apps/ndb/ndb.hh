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
        vector<string> regexes;

        /* ZMQ Threads */
        pthread_t control_t;
        pthread_t history_t;

        /* ZMQ context (one per process) */
        zmq::context_t context;

        bool s_interrupted;

        string netsight_host;

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
