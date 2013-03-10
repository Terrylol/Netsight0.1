#ifndef API_HH
#define API_HH

#include "types.hh"
#include "helper.hh"
#include "zhelpers.hh"
#include "postcard.hh"

#include <vector>
#include <string>

/*

NetSight maintains two ZeroMQ sockets:
- A REQ-REP control socket
- A PUB-SUB history socket

* Apps connect to NetSight by sending an ECHO_REQUEST message.
* The apps need to stay alive by sending an ECHO_REQUEST every second.
* For every ECHO_REQUEST message, NetSight sends an ECHO_REPLY message.
* After 3 rounds of idleness, NetSight clears all state associated with the app
* Apps can add a PHF by sending an ADD_FILTER message
* Apps can delete a PHF by sending a DELETE_FILTER message
* Apps can query the list of installed PHFs using a GET_FILTERS message
* For each packet history matching a PHF, NetSight publishes a history message


Control message json format:
{
    "type": MessageType
    "data": json object
}

History message format:
Frame1: Matched PHF (including the terminating NULL character)
Frame2: JSON-serialized Packet History
*/

using namespace std;

enum MessageType {
    ECHO_REQUEST = 1,
    ECHO_REPLY,
    ADD_FILTER_REQUEST,
    ADD_FILTER_REPLY,
    DELETE_FILTER_REQUEST,
    DELETE_FILTER_REPLY,
    GET_FILTERS_REQUEST,
    GET_FILTERS_REPLY
};

class Message {
    private:
    protected:
        MessageType type;
        JSON msg_j;
    public:
        string serialize()
        {
            return V(msg_j).serialize();
        }
};

class EchoRequestMessage: public Message {
    private:
    public:
        EchoRequestMessage()
        {
            type = ECHO_REQUEST;
            msg_j["type"] = V((u64)type);
        }
};

class EchoReplyMessage: public Message {
    private:
    public:
        EchoReplyMessage()
        {
            type = ECHO_REPLY;
            msg_j["type"] = V((u64)type);
        }
};

class AddFilterRequestMessage: public Message {
    private:
    public:
        AddFilterRequestMessage(string &filter)
        {
            type = ADD_FILTER_REQUEST;
            msg_j["type"] = V((u64)type);
            msg_j["data"] = V(filter);
        }
};

class AddFilterReplyMessage: public Message {
    private:
    public:
        AddFilterReplyMessage(string &filter, bool result)
        {
            type = ADD_FILTER_REPLY;
            JSON data;
            data["filter"] = V(filter);
            data["result"] = V(result);

            msg_j["type"] = V((u64)type);
            msg_j["data"] = V(data);
        }
};

class DeleteFilterRequestMessage: public Message {
    private:
    public:
        DeleteFilterRequestMessage(string &filter)
        {
            type = DELETE_FILTER_REQUEST;
            msg_j["type"] = V((u64)type);
            msg_j["data"] = V(filter);
        }
};

class DeleteFilterReplyMessage: public Message {
    private:
    public:
        DeleteFilterReplyMessage(string &filter, bool result)
        {
            type = DELETE_FILTER_REPLY;
            JSON data;
            data["filter"] = V(filter);
            data["result"] = V(result);

            msg_j["type"] = V((u64)type);
            msg_j["data"] = V(data);
        }
};

class GetFiltersRequestMessage: public Message {
    private:
    public:
        GetFiltersRequestMessage()
        {
            type = GET_FILTERS_REQUEST;
            msg_j["type"] = V((u64)type);
        }
};

class GetFiltersReplyMessage: public Message {
    private:
    public:
        GetFiltersReplyMessage(vector<string> &filters)
        {
            type = GET_FILTERS_REPLY;
            vector<picojson::value> data;
            for(int i = 0; i < filters.size(); i++) {
                data.push_back(V(filters[i]));
            }
            msg_j["data"] = V(data);
        }
};

void subscribe_filter(string &filter, zmq::socket_t &sub_sock);
void unsubscribe_filter(string &filter, zmq::socket_t &sub_sock);
bool add_filter(string filter, zmq::socket_t &ctrl_sock, zmq::socket_t &sub_sock);
bool delete_filter(string filter, zmq::socket_t &ctrl_sock, zmq::socket_t &sub_sock);
vector<string> get_filters(zmq::socket_t &ctrl_sock);

#endif //API_HH
