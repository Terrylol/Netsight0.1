#ifndef NDB_HH
#define NDB_HH

#include <signal.h>
#include <iostream>
#include <string>
#include <vector>

#include "app.hh"

using namespace std;

class NDB: public NetSightApp {
    private:
        vector<string> regexes;
        pthread_t hearbeat_t;

        NDB(string host="localhost"):
            NetSightApp(host)
            { }

        ~NDB()
        {
            char *ret;
            s_interrupted = true;
            pthread_join(hearbeat_t, (void **)&ret);
        }

        /* signal handler thread */
        static void sig_handler(int signum)
        {
            NDB &n = NDB::get_instance();
            if(signum == SIGINT || signum == SIGTERM)
                n.s_interrupted = true;
        }

    public:
        static NDB &get_instance()
        {
            static NDB n;
            return n;
        }

        void start()
        {
            /* create a signal handler */
            signal(SIGINT, sig_handler);

            NetSightApp::start();
            pthread_create(&hearbeat_t, NULL, &NDB::heartbeat_thread, (void*)(&NDB::get_instance()));

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
                }
                else if(input == "")
                    continue;
                regexes.push_back(input);
            }
        }

};

#endif // NDB_HH
