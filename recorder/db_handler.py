#!/usr/bin/python

'''
A generic interface to handle flow table database.
Current version uses MongoDB as the backend database
'''

import pymongo
import logging as L

class MongoHandler(object):
    '''MongoDB handler'''

    def __init__(self, dpid=None, host='localhost', port=27017, db_name='ndb',
            coll_name='flow-tables', flush_on_del=False):
        self.dpid = dpid
        self.c = None
        self.db = None
        self.coll = None
        self.flush_on_del = flush_on_del
        self.logger = L.getLogger('MongoDB')
        self.connect(host, port)
        if self.c:
            self.set_db(db_name)
            self.set_coll(coll_name)

    def __del__(self):
        if self.flush_on_del:
            self.flush()
        self.disconnect()

    def connect(self, host, port):
        try:
            self.c = pymongo.connection.Connection(host=host, port=port)
        except pymongo.errors.ConnectionFailure:
            self.logger.error('connect: Could not connect to MongoDB server (%s:%d). Flow table updates will not be logged.' % (host, port))
            pass

    def disconnect(self):
        if self.c:
            self.c.disconnect()

    def set_db(self, db_name):
        self.db = self.c[db_name]

    def set_coll(self, collection_name):
        self.coll = self.db[collection_name]

    def add_record(self, rec):
        if self.coll:
            self.coll.insert(rec)

    def remove_records(self, rec_filter=None):
        if self.coll:
            if rec_filter:
                self.coll.remove(rec_filter)
            else:
                self.coll.remove()

    def get_records(self, rec_filter={}):
        if self.coll:
            return self.coll.find(rec_filter)
        return None

    def flush(self):
        if self.coll:
            self.remove_records({'dpid': self.dpid} if self.dpid else {})
