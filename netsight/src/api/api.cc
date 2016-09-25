/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

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
    bool ret = s_sendmore(ctrl_sock, "");
    ret &= s_send(ctrl_sock, msg.serialize());
    return ret;
}

/*
 * Non-Blocking call to delete filter
 * */
bool delete_filter_noblock(string filter, zmq::socket_t &ctrl_sock)
{
    DeleteFilterRequestMessage msg(filter);
    bool ret = s_sendmore(ctrl_sock, "");
    ret &= s_send(ctrl_sock, msg.serialize());
    return ret;
}

/*
 * Non-Blocking call to get the installed filters
 * */
bool get_filters_noblock(zmq::socket_t &ctrl_sock)
{
    vector<string> filters;
    GetFiltersRequestMessage req_msg;
    bool ret = s_sendmore(ctrl_sock, "");
    ret &= s_send(ctrl_sock, req_msg.serialize());
    return ret;
}
