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
        PostcardNode(Packet *p)
        {
            prev = next = NULL;
            pkt = p;
            dpid = get_dpid();
            inport = -1;
            outport = get_outport();
            version = get_version();
        }

        int get_dpid()
        {
            u8 dpid = 0; 
            memcpy(&dpid, &(pkt->eth.dst[5]), DPID_TAG_LEN);
            return (int)dpid;
        }

        int get_outport()
        {
            u16 outport = 0;
            memcpy(&outport, &(pkt->eth.dst[3]), OUTPORT_TAG_LEN);
            outport = ntohs(outport);
            return (int)outport;
        }

        int get_version()
        {
            u32 version = 0;
            memcpy(&version, &(pkt->eth.dst[0]), VERSION_TAG_LEN);
            version = ntohl(version);
            version = version >> ((sizeof(version) - VERSION_TAG_LEN)*8);
            return (int)version;
        }
        void print()
        {
            printf("{dpid: %d, inport: %d, outport: %d, version: %d}", dpid, inport, outport, version);
        }

        string str()
        {
            stringstream ss;
            ss << "{dpid: " << dpid << ", inport: " << inport << ", outport: " << outport << ", version: " << version << "}";
            return ss.str();
        }
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
        void print();
};

#endif //POSTCARD_LIST_HH
