#ifndef POSTCARD_LIST_HH
#define POSTCARD_LIST_HH

#include "packet.hh"
#include "types.hh"
#include "helper.hh"
#include <string>
#include <sstream>

#define DPID_TAG_LEN 1
#define OUTPORT_TAG_LEN 2
#define VERSION_TAG_LEN 3

/* 
 * A PostcardList is essentially a doubly linked list of PostcardNodes
 * It's the caller's responsibility to allocate/deallocate memory for the Postcards
 * except in clear()
 * */
using namespace std;

class PostcardNode {
    public:
        PostcardNode *prev;
        PostcardNode *next;
        Packet *pkt;
        int dpid;
        int inport;
        int outport;
        int version;
        string match;
        vector<string> actions; 
        PostcardNode(Packet *p);
        int get_dpid();
        int get_outport();
        int get_version();
        string str();
        JSON json();
        static PostcardNode *decode_json(picojson::value &pn_j);
};

class PostcardList {
    public:
        PostcardNode *head;
        PostcardNode *tail;
        int length;

        /* Default constructor */
        PostcardList()
        {
            head = NULL;
            tail = NULL;
            length = 0;
            DBG("PostcardList constructor: length = 0x%x\n", length);
        }

        /* Shallow copy constructor */
        PostcardList(const PostcardList &pl)
        {
            head = pl.head;
            tail = pl.tail;
            length = pl.length;
        }

        void insert(PostcardNode *p, PostcardNode *loc);
        PostcardNode *remove(PostcardNode *p);
        void push_back(PostcardNode *p)
        { insert(p, tail); }
        void push_front(PostcardNode *p)
        { insert(p, NULL); }
        void clear();
        string str();
        JSON json();
        static PostcardList *decode_json(picojson::value &message_j);
};

#endif //POSTCARD_LIST_HH
