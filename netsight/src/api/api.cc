#include <sstream>

#include "api.hh"

void subscribe_filter(string &filter, zmq::socket_t &sub_sock)
{
    //NOTE: It's important to include the terminating NULL character in the
    //filter
    sub_sock.setsockopt(ZMQ_SUBSCRIBE, filter.c_str(), filter.size() + 1);
}

void unsubscribe_filter(string &filter, zmq::socket_t &sub_sock)
{
    //NOTE: It's important to include the terminating NULL character in the
    //filter
    sub_sock.setsockopt(ZMQ_UNSUBSCRIBE, filter.c_str(), filter.size() + 1);
}

bool add_filter(string filter, zmq::socket_t &ctrl_sock, zmq::socket_t &sub_sock)
{
    AddFilterRequestMessage msg(filter);
    bool ret = s_send(ctrl_sock, msg.serialize());
    subscribe_filter(filter, sub_sock);
    return ret;
}

bool delete_filter(string filter, zmq::socket_t &ctrl_sock, zmq::socket_t &sub_sock)
{
    DeleteFilterRequestMessage msg(filter);
    bool ret = s_send(ctrl_sock, msg.serialize());
    unsubscribe_filter(filter, sub_sock);
    return ret;
}

/*
 * Blocking call to get the installed filters
 * */
vector<string> get_filters(zmq::socket_t &ctrl_sock)
{
    vector<string> filters;
    GetFiltersRequestMessage req_msg;
    bool ret = s_send(ctrl_sock, req_msg.serialize());
    zmq::pollitem_t items [] = {
        { ctrl_sock, 0, ZMQ_POLLIN, 0 },
    };
    zmq::poll (&items[0], 1, -1);
    if(items[0].revents & ZMQ_POLLIN) {
        string filters_str = s_recv(ctrl_sock);
        stringstream ss(filters_str);
        picojson::value filters_j;
        string err = picojson::parse(filters_j, ss);
        picojson::array &filters_list = filters_j.get<picojson::array>();
        for (picojson::array::iterator iter = filters_list.begin(); iter != filters_list.end(); ++iter) {
            string f = (*iter).get<string>();
            filters.push_back(f);
        }
    }
    return filters;
}
