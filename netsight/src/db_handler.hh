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

        int connect(string host);
        void disconnect();
    public:
        MongoHandler(string host="localhost:27017", string db_name="ndb", string coll_name="flow-tables", bool flush_on_del=false);
        ~MongoHandler();
        void set_db(string s);
        void set_coll(string s);

        auto_ptr<mongo::DBClientCursor> get_record(const mongo::Query &query);
        void add_record(const mongo::BSONObj &obj);
        void remove_records(const mongo::Query &query); 
        void flush();
};

#endif  //__DB_HANDLER_HH__
