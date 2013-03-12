#ifndef NDB_HH
#define NDB_HH

#include <signal.h>
#include <iostream>
#include <string>
#include <vector>

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

        NDB(string host="localhost"):
            context(1),
            netsight_host(host)
            { 
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGINT);
            }

        /* signal handler */
        static void sig_handler(int signum)
        {
            NDB &n = NDB::get_instance();
            switch(signum) {
                case SIGINT:
                    n.cleanup();
                    DBG("Bye!\n");
                    exit(EXIT_SUCCESS);
                    break;
                default:
                    DBG("Unknexpected signal: %d\n", signum);
            }
        }

        static void *control_channel_thread(void *args)
        {
            NDB &n = NDB::get_instance();

            // ZMQ socket (private to thread)
            zmq::socket_t req_sock(n.context, ZMQ_DEALER);
            //  Configure socket to not wait at close time
            int linger = 0;
            req_sock.setsockopt (ZMQ_LINGER, &linger, sizeof(linger));
            

            stringstream req_ss;
            req_ss << "tcp://" << n.netsight_host << ":" << NETSIGHT_CONTROL_PORT;
            req_sock.connect(req_ss.str().c_str());

            zmq::pollitem_t items [] = {
                { req_sock, 0, ZMQ_POLLIN, 0 },
            };
            while(!(n.s_interrupted)) {
                pthread_testcancel();

                struct timeval start_t, end_t;
                gettimeofday(&start_t, NULL);

                // Poll with a timeout of HEARTBEAT_INTERVAL
                zmq::poll (&items[0], 1, HEARTBEAT_INTERVAL);

                if(items[0].revents & ZMQ_POLLIN) {
                    // TODO: handle incoming control messages
                }

                // Sleep for the remainder of the period
                gettimeofday(&end_t, NULL);
                double sleep_time = HEARTBEAT_INTERVAL - diff_time_ms(end_t, start_t);
                s_sleep((int)sleep_time);

                // Send out an ECHO_REQUEST
                stringstream ss;
                ss << time(NULL);
                EchoRequestMessage msg(ss.str());
                DBG("Sending ECHO_REQUEST with timestamp %s\n", ss.str().c_str());
                bool ret = s_send(req_sock, msg.serialize());
            }
        }

        static void *history_channel_thread(void *args)
        {
            NDB &n = NDB::get_instance();

            // ZMQ socket (private to thread)
            zmq::socket_t sub_sock(n.context, ZMQ_SUB);
            //  Configure socket to not wait at close time
            int linger = 0;
            sub_sock.setsockopt (ZMQ_LINGER, &linger, sizeof(linger));

            stringstream sub_ss;
            sub_ss << "tcp://" << n.netsight_host << ":" << NETSIGHT_HISTORY_PORT;
            sub_sock.connect(sub_ss.str().c_str());

            zmq::pollitem_t items [] = {
                { sub_sock , 0, ZMQ_POLLIN, 0 },
            };
            while(!(n.s_interrupted)) {
                pthread_testcancel();

                // Poll with a timeout of HEARTBEAT_INTERVAL
                zmq::poll (&items[0], 1, HEARTBEAT_INTERVAL);

                if(items[0].revents & ZMQ_POLLIN) {
                    //TODO: Handle matching packet histories
                }
            }
        }

        void cleanup()
        {
            void* ret = NULL;
            pthread_cancel(control_t);
            pthread_cancel(history_t);
            pthread_join(control_t, &ret);
            pthread_join(history_t, &ret);

            DBG("\nCleanup complete.\n");
        }

    public:
        static NDB &get_instance()
        {
            static NDB n;
            return n;
        }

        void start()
        {
            /* block out these signals 
             * any newly created threads will inherit this signal mask
             * */
            pthread_sigmask(SIG_BLOCK, &sigset, NULL);

            pthread_create(&control_t, NULL, &NDB::control_channel_thread, NULL);
            pthread_create(&history_t, NULL, &NDB::history_channel_thread, NULL);

            /* Unblock the signals */
            pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

            /* create a signal handler */
            signal(SIGINT, sig_handler);

            interact();
        }

        void interact()
        {
            cout << "Enter a PHF to add or \"exit\" to exit.";
            while(!s_interrupted)
            {
                cout << ">>";
                string input;
                cin.clear();
                cin.sync();
                if (!getline(cin, input)) {
                    ERR("Error: Reading user input\n");
                }
                DBG("Got input\n");
                if(input == "exit") {
                    s_interrupted = true;
                    cleanup();
                    continue;
                }
                else if(input == "") {
                    continue;
                }
                regexes.push_back(input);
            }
        }

};

#endif // NDB_HH
