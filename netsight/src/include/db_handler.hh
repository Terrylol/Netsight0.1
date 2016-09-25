/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#ifndef __DB_HANDLER_HH__
#define __DB_HANDLER_HH__

#include <mongo/client/dbclient.h>
#include <string>

using namespace std;

class MongoHandler {
    private:
        mongo::DBClientConnection db_conn;
        string db_host;
        string db_name;
        string coll_name;
        string ns;
        bool flush_on_del;

        void disconnect();
    public:
        MongoHandler(string db_name="ndb", string coll_name="flow-tables", bool flush_on_del=false);
        ~MongoHandler();
        int connect(string host);
        void set_db(string s);
        void set_coll(string s);

        auto_ptr<mongo::DBClientCursor> get_records(const mongo::Query &query);
        void add_record(const mongo::BSONObj &obj);
        void remove_records(const mongo::Query &query); 
        void flush();
};

#endif  //__DB_HANDLER_HH__
