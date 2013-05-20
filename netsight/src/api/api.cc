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

/*
 * Non-Blocking call to add filter
 * */
bool add_filter_noblock(string filter, zmq::socket_t &ctrl_sock)
{
    AddFilterRequestMessage msg(filter);
    bool ret = s_send(ctrl_sock, msg.serialize());
    return ret;
}

/*
 * Non-Blocking call to delete filter
 * */
bool delete_filter_noblock(string filter, zmq::socket_t &ctrl_sock)
{
    DeleteFilterRequestMessage msg(filter);
    bool ret = s_send(ctrl_sock, msg.serialize());
    return ret;
}

/*
 * Non-Blocking call to get the installed filters
 * */
bool get_filters_noblock(zmq::socket_t &ctrl_sock)
{
    vector<string> filters;
    GetFiltersRequestMessage req_msg;
    bool ret = s_send(ctrl_sock, req_msg.serialize());
    return ret;
}
