#include "db_handler.hh"
#include "helper.hh"

MongoHandler::MongoHandler(string db_name, string coll_name, bool flush_on_del)
{
    set_db(db_name);
    set_coll(coll_name);
}

MongoHandler::~MongoHandler()
{
    disconnect();
    if(flush_on_del) {
        flush();
    }
}

int MongoHandler::connect(string host)
{
    DBG(AT, "Connecting to MongoDB at %s\n", host.c_str());
    db_host = host;
    try {
        db_conn.connect(host);
    }
    catch(const mongo::DBException &e) {
        fprintf(stderr, "Error in connect(): Could not connect to %s\n", host.c_str());
        return 0;
    }

    DBG(AT, "Successfully connected to MongoDB\n");
    return 1;
}

void MongoHandler::disconnect()
{
}

void MongoHandler::set_db(string s)
{
    db_name = s;
    ns = db_name + "." + coll_name;
}

void MongoHandler::set_coll(string s)
{
    coll_name = s;
    ns = db_name + "." + coll_name;
}

auto_ptr<mongo::DBClientCursor> MongoHandler::get_record(const mongo::Query &query)
{
    return db_conn.query(ns, query);
}

void MongoHandler::add_record(const mongo::BSONObj &obj)
{
    db_conn.insert(ns, obj);
}

void MongoHandler::remove_records(const mongo::Query &query)
{
    db_conn.remove(ns, query);
}

void MongoHandler::flush()
{
    mongo::BSONObjBuilder query;
    remove_records(query.obj());
}
