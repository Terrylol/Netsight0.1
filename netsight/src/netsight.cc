/*
 * Source: http://www.tcpdump.org/sniffex.c
 * */

#include <cstdio>
#include <vector>
#include <iostream>
#include <sys/time.h>
#include <pcap.h>
#include <pthread.h>

#include "netsight.hh"

#define SNAP_LEN 1514
#define POSTCARD_FILTER ""
#define DEBUG_VLAN 0x02
static int SKIP_ETHERNET = 0;

using namespace std;

NetSight::NetSight()
{
    /* Database handler initialization */
    topo_db.set_db("ndb");
    topo_db.set_coll("topo");

    ft_db.set_db("ndb");
    ft_db.set_coll("flow-tables");

    psid_db.set_db("ndb");
    psid_db.set_coll("psid-to-dpid");

    /* Initialize mutexes and condition variables */
    pthread_mutex_init(&stage_lock, NULL);

    /* set of signals that the signal handler thread should handle*/
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
}

void
NetSight::start()
{
    /* block out these signals 
     * any newly created threads will inherit this signal mask
     * */
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    /* Start postcard_worker thread */
    pthread_create(&postcard_t, NULL, &NetSight::postcard_worker, NULL);

    /* Start history worker thread */
    pthread_create(&history_t, NULL, &NetSight::history_worker, NULL);

    /* Unblock the signals */
    pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

    /* create a signal handler */
    signal(SIGINT, sig_handler);

    interact();
}

void
NetSight::interact()
{
    cout << "Enter a PHF to add or \"exit\" to exit.";
    while(true)
    {
        cout << ">>";
        string input;
        cin.clear();
        cin.sync();
        if (!getline(cin, input))
            fprintf(stderr, "Error: Reading user input\n");
        DBG("Got input\n");
        if(input == "exit") {
            cleanup();
            exit(EXIT_SUCCESS);
        }
        else if(input == "")
            continue;
        regexes.push_back(input);
        filters.push_back(PacketHistoryFilter(input.c_str()));
    }
}

void *
NetSight::postcard_worker(void *args)
{

    NetSight &n = NetSight::get_instance();
    return n.run_postcard_worker(args);
}

void*
NetSight::run_postcard_worker(void *args)
{
    sniff_pkts(sniff_dev.c_str());
}

void *
NetSight::history_worker(void *args)
{
    NetSight &n = NetSight::get_instance();
    return n.run_history_worker(args);
}

void*
NetSight::run_history_worker(void *args)
{
    struct timeval start_t, end_t;
    while(true) {
        pthread_testcancel();
        DBG("----------------- ROUND COMPLETE ------------------\n\n");
        gettimeofday(&start_t, NULL);

        pthread_mutex_lock(&stage_lock);

        // Empty the contents of stage into a local PostcardList
        PostcardList pl(stage);
        stage.head = stage.tail = NULL;
        stage.length = 0;

        pthread_mutex_unlock(&stage_lock);

        // Empty the local PostcardList and populate path_table
        PostcardNode *pn = pl.head;
        DBG("Going to empty local PostcardList with %d postcards\n", pl.length);
        for(int i = 0; i < pl.length; i++) {
            DBG("pl.remove()\n");
            PostcardNode *next_pn = pn->next;
            PostcardNode *p = pl.remove(pn);
            path_table.insert_postcard(p);
            pn = next_pn;
        }


        // For each PostcardList older than PACKET_HISTORY_PERIOD:
        // - topo-sort the PostcardList
        // - match against all the filters
        unordered_map<u64, PostcardList>::iterator it;
        for(it = path_table.table.begin(); it != path_table.table.end(); ) {
            PostcardNode *pn = it->second.head;
            struct timeval curr;
            gettimeofday(&curr, NULL);
            double ts_diff = diff_time_ms(curr, pn->pkt->ts);
            if(ts_diff > PACKET_HISTORY_PERIOD) {
                PostcardList &pl = it->second;
                topo_sort(&pl, topo);
                for(int i = 0; i < filters.size(); i++) {
                    if(filters[i].match(pl)) {
                        pl.print();
                        DBG("MATCHED REGEX: %s\n", filters[i].str().c_str());
                    }
                }
                pl.clear();
                it = path_table.table.erase(it);
            }
            else {
                it++;
            }
        }
        gettimeofday(&end_t, NULL);
        double diff_t = diff_time_ms(end_t, start_t);
        if(diff_t < ROUND_LENGTH) {
            usleep((long)(ROUND_LENGTH - diff_t) * 1000);
        }
    }
}

void NetSight::sniff_pkts(const char *dev) { char errbuf[PCAP_ERRBUF_SIZE];          /* error buffer */ 
    const char *filter_exp = POSTCARD_FILTER;/* filter expression [3] */
    bpf_u_int32 mask;                       /* subnet mask */
    bpf_u_int32 net;                        /* ip */
    int num_packets = -1;                   /* number of packets to capture */

    /* get network number and mask associated with capture device */
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n",
                dev, errbuf);
        net = 0;
        mask = 0;
    }

    /* print capture info */
    printf("Device: %s\n", dev);
    printf("Number of packets: %d\n", num_packets);
    printf("Filter expression: %s\n", filter_exp);

    /* open capture device */
    postcard_handle = pcap_open_live(dev, SNAP_LEN, 1, 100, errbuf);
    if (postcard_handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return;
    }

    /* make sure we're capturing on an Ethernet device [2] */
    if (pcap_datalink(postcard_handle) != DLT_EN10MB) {
        fprintf(stderr, "%s is not an Ethernet\n", dev);
        return;
    }

    /* compile the filter expression */
    if (pcap_compile(postcard_handle, &postcard_fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n",
                filter_exp, pcap_geterr(postcard_handle));
        return;
    }

    /* apply the compiled filter */
    if (pcap_setfilter(postcard_handle, &postcard_fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n",
                filter_exp, pcap_geterr(postcard_handle));
        return;
    }

    struct pcap_pkthdr hdr;
    while(true) {
        pthread_testcancel();
        const u_char *pkt = pcap_next(postcard_handle, &hdr);
        if(pkt == NULL)
            continue;
        DBG("Got postcard: %p\n", pkt);
        postcard_handler(&hdr, pkt);
    }
    DBG("Error! Should never come here...\n");

}

void
NetSight::cleanup()
{
    /* cleanup */
    void* ret = NULL;
    pthread_cancel(postcard_t);
    pthread_cancel(history_t);
    pthread_join(postcard_t, &ret);
    pthread_join(history_t, &ret);

    pcap_freecode(&postcard_fp);
    if(postcard_handle)
        pcap_close(postcard_handle);

    DBG("\nCleanup complete.\n");
}

/*
 * Add received postcard to stage
 * Use stage_lock to write in a thread-safe manner
 * */
void
NetSight::postcard_handler(const struct pcap_pkthdr *header, const u_char *packet)
{
    static u32 packet_number = 0;
    pthread_mutex_lock(&stage_lock);
    Packet *p = new Packet(packet, header->len, 
                    SKIP_ETHERNET, packet_number++, header->caplen);
    if(!p->eth.vlan == DEBUG_VLAN)
        return;
    PostcardNode *pn = new PostcardNode(p);
    assert(psid_to_dpid.find(pn->dpid) != psid_to_dpid.end());
    pn->dpid = psid_to_dpid[pn->dpid];
    stage.push_back(pn);
    DBG("Adding postcard to stage: length = %d\n", stage.length);
    Packet *pkt = (stage.tail)->pkt;
    pkt->ts = header->ts;
    pthread_mutex_unlock(&stage_lock);
}

void
NetSight::sig_handler(int signum)
{
    NetSight &n = NetSight::get_instance();

    switch(signum) {
        case SIGINT:
            DBG("Got SIGINT\n");
            n.cleanup();
            exit(EXIT_SUCCESS);
            break;
        default:
            printf("Unknown signal %d\n", signum);
    }
}

void 
NetSight::connect_db(string host)
{
    if(!topo_db.connect(host)) {
        fprintf(stderr, AT "Could not connect to MongoDB: %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }
    if(!ft_db.connect(host)) {
        fprintf(stderr, AT "Could not connect to MongoDB: %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }
    if(!psid_db.connect(host)) {
        fprintf(stderr, AT "Could not connect to MongoDB: %s\n", host.c_str());
        exit(EXIT_FAILURE);
    }

    read_topo_db();
    read_psid_db();
}

void 
NetSight::read_topo_db()
{
    DBG("Reading topology from the DB:\n");
    auto_ptr<mongo::DBClientCursor> cursor = topo_db.get_records(mongo::BSONObj());
    if(cursor->more()) {
        stringstream ss;
        mongo::BSONObj obj = cursor->next();
        DBG("%s\n", obj.jsonString().c_str());
        ss << obj.jsonString();
        topo.read_topo(ss);
    }
    else {
        DBG("No topology info in the DB\n");
    }
}

void 
NetSight::read_psid_db()
{
    DBG("Reading psid-to-dpid from the DB:\n");
    auto_ptr<mongo::DBClientCursor> cursor = psid_db.get_records(mongo::BSONObj());
    while(cursor->more()) {
        mongo::BSONObj obj = cursor->next();
        int psid = (int) obj["psid"].Int();
        int dpid = (int) obj["dpid"].Int();
        psid_to_dpid[psid] = dpid;
        DBG("%d -> %d\n", psid, dpid);
    }
}

void 
NetSight::get_flow_entry(int dpid, int version, string &match, vector<string> &actions)
{
    DBG("Reading flow table entry from the DB:\n");
    DBG("dpid:%d, version: %d\n", dpid, version);
    mongo::BSONObjBuilder query;
    query << "dpid" << dpid << "version" << version;
    auto_ptr<mongo::DBClientCursor> cursor = ft_db.get_records(query.obj());
    if(cursor->more()) {
        mongo::BSONObj obj = cursor->next();
        DBG("%s\n", obj.jsonString().c_str());
        match = obj["match"].toString();
        vector<mongo::BSONElement> action_obj = obj["action"].Array();
        for(int i = 0; i < action_obj.size(); i++) {
            actions.push_back(action_obj[i].toString());
        }
    }
    else {
        DBG("Error: No matching flow-entry found...\n");
    }
}

