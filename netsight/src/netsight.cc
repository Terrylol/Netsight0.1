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
static int SKIP_ETHERNET = 0;

using namespace std;

NetSight::NetSight()
{
    /* Initialize mutexes and condition variables */
    pthread_mutex_init(&stage_lock, NULL);
    pthread_cond_init(&round_cond, NULL);

    /* set of signals that the main thread should handle*/
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGALRM);

    /* block out these signals 
     * any newly created threads will inherit this signal mask
     * */
    sigprocmask(SIG_BLOCK, &set, NULL);

    /* Start postcard_worker thread */
    pthread_create(&postcard_t, NULL, &NetSight::postcard_worker, this);

    /* Start history worker thread */
    pthread_create(&history_t, NULL, &NetSight::history_worker, this);

    /* unblock these signals so the main thread can handle it*/
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    /* Setup round signal handler */
    struct sigaction sact;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = NetSight::sig_handler;
    sigaction(SIGALRM, &sact, NULL);

    /* Start round timers */
    struct itimerval round_timer;
    round_timer.it_interval.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_interval.tv_sec = (int) (ROUND_LENGTH/1000);
    round_timer.it_value.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_value.tv_sec = (int) (ROUND_LENGTH/1000);
    setitimer (ITIMER_REAL, &round_timer, NULL);

}

void
NetSight::interact()
{
    string input;
    while(true)
    {
        cout << "Enter a PHF to add or \"exit\" to exit.";
        cout << ">>";
        cin >> input;
        if(input == "exit")
            return;
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
    while(true) {
        pthread_testcancel();
        pthread_mutex_lock(&stage_lock);
        pthread_cond_wait(&round_cond, &stage_lock);

        printf("----------------- ROUND COMPLETE ------------------\n\n");

        // Empty stage and populate path_table
        PostcardNode *pn = stage.head;
        for(int i = 0; i < stage.length; i++) {
            PostcardNode *p = stage.remove(pn);
            path_table.insert_postcard(p);
            pn = pn->next;
        }

        pthread_mutex_unlock(&stage_lock);

        //TODO:
        // For each old enough PostcardList:
        // - topo-sort the PostcardList
        // - create a PackedPostcardList, and
        // - match against all the filters

    }
}

void 
NetSight::sniff_pkts(const char *dev)
{
    char errbuf[PCAP_ERRBUF_SIZE];          /* error buffer */

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
        //exit(EXIT_FAILURE);
        return;
    }

    /* make sure we're capturing on an Ethernet device [2] */
    if (pcap_datalink(postcard_handle) != DLT_EN10MB) {
        fprintf(stderr, "%s is not an Ethernet\n", dev);
        //exit(EXIT_FAILURE);
        return;
    }

    /* compile the filter expression */
    if (pcap_compile(postcard_handle, &postcard_fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n",
                filter_exp, pcap_geterr(postcard_handle));
        //exit(EXIT_FAILURE);
        return;
    }

    /* apply the compiled filter */
    if (pcap_setfilter(postcard_handle, &postcard_fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n",
                filter_exp, pcap_geterr(postcard_handle));
        //exit(EXIT_FAILURE);
        return;
    }

    while(true) {
        pthread_testcancel();
        struct pcap_pkthdr hdr;
        const u_char *pkt = pcap_next(postcard_handle, &hdr);
        postcard_handler(&hdr, pkt);
    }

}

void
NetSight::cleanup()
{
    NetSight &n = NetSight::get_instance();
    /* cleanup */
    pcap_freecode(&n.postcard_fp);
    if(n.postcard_handle)
        pcap_close(n.postcard_handle);

    printf("\nCleanup complete.\n");
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
    stage.push_back(new PostcardNode(new Packet(packet, header->len, 
                    SKIP_ETHERNET, packet_number++, header->caplen)));
    Packet *pkt = (stage.tail)->pkt;
    pkt->ts = header->ts;
    pthread_mutex_unlock(&stage_lock);
}

void 
NetSight::sig_handler(int signum)
{
    NetSight &n = NetSight::get_instance();
    void* ret = NULL;
    switch(signum) {
        case SIGALRM:
            // signal round_cond
            pthread_cond_signal(&n.round_cond);
            break;
        case SIGINT:
            pthread_cancel(n.postcard_t);
            pthread_cancel(n.history_t);
            pthread_join(n.postcard_t, &ret);
            pthread_join(n.history_t, &ret);
            n.cleanup();
            break;
        default:
            printf("Unknown signal %d\n", signum);
    }
}

