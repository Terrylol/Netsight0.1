#ifndef APP_HH
#define APP_HH

#include <signal.h>
#include <pthread.h>
#include <ctime>
#include <sstream>

#include "zhelpers.hh"
#include "api.hh"

using namespace std;

class NetSightApp {
    private:
    protected:
        /* API Sockets */
        zmq::context_t context;
        zmq::socket_t sub_sock;
        zmq::socket_t req_sock;
        bool s_interrupted;

        string netsight_host;

        NetSightApp(string host="localhost"):
            context(1),
            sub_sock(context, ZMQ_SUB), 
            req_sock(context, ZMQ_DEALER),
            netsight_host(host)
            { }

        ~NetSightApp()
        { }

        static void *heartbeat_thread(void *args)
        {
            NetSightApp *app = static_cast<NetSightApp*>(args);
            while(!app->s_interrupted) {
                stringstream ss;
                ss << time(NULL);
                EchoRequestMessage msg(ss.str());
                DBG("Sending ECHO_REQUEST with timestamp %s\n", ss.str().c_str());
                s_send(app->get_req_sock(), msg.serialize());
                s_sleep(HEARTBEAT_INTERVAL);
            }
        }

    public:
        virtual void start()
        {
            stringstream sub_ss, req_ss;
            req_ss << "tcp://" << netsight_host << ":" << NETSIGHT_CONTROL_PORT;
            sub_ss << "tcp://" << netsight_host << ":" << NETSIGHT_HISTORY_PORT;
            sub_sock.connect(sub_ss.str().c_str());
            req_sock.connect(req_ss.str().c_str());
        }

        zmq::socket_t &get_req_sock()
        {
            return req_sock;
        }

        zmq::socket_t &get_sub_sock()
        {
            return sub_sock;
        }
};

#endif
