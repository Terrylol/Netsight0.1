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

#define DEVICE "eth0"
#define SNAP_LEN 1514
#define POSTCARD_FILTER ""
static int SKIP_ETHERNET = 0;

using namespace std;

NetSight::NetSight()
{
    /* Initialize mutexes and condition variables */
    pthread_mutex_init(&stage_lock, NULL);
    pthread_cond_init(&round_cond, NULL);

    /* Start round timers */
    struct itimerval round_timer;
    round_timer.it_interval.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_interval.tv_sec = (int) (ROUND_LENGTH/1000);
    round_timer.it_value.tv_usec = (int)(ROUND_LENGTH % 1000) * 1000;
    round_timer.it_value.tv_sec = (int) (ROUND_LENGTH/1000);
    setitimer (ITIMER_REAL, &round_timer, NULL);

    /* Start postcard_worker thread */
    pthread_create(&postcard_t, NULL, &NetSight::postcard_worker, this);

    /* Start history worker thread */
    pthread_create(&history_t, NULL, &NetSight::history_worker, this);

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

void*
NetSight::run_postcard_worker(void *args)
{
    /* Setup round signal handler */
    struct sigaction sact;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = NetSight::sig_handler;
    sigaction(SIGALRM, &sact, NULL);

    /* Start packet capture */
    // TODO: take dev as input
    const char *dev = DEVICE;
    sniff_pkts(dev);
}

void*
NetSight::run_history_worker(void *args)
{
    while(true) {
        pthread_mutex_lock(&stage_lock);
        pthread_cond_wait(&round_cond, &stage_lock);

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
    pcap_t *handle;                         /* packet capture handle */

    const char *filter_exp = POSTCARD_FILTER;/* filter expression [3] */
    struct bpf_program fp;                  /* compiled filter program (expression) */
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
    handle = pcap_open_live(dev, SNAP_LEN, 1, 100, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        exit(EXIT_FAILURE);
    }

    /* make sure we're capturing on an Ethernet device [2] */
    if (pcap_datalink(handle) != DLT_EN10MB) {
        fprintf(stderr, "%s is not an Ethernet\n", dev);
        exit(EXIT_FAILURE);
    }

    /* compile the filter expression */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n",
                filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }

    /* apply the compiled filter */
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n",
                filter_exp, pcap_geterr(handle));
        exit(EXIT_FAILURE);
    }

    while(true) {
        struct pcap_pkthdr hdr;
        const u_char *pkt = pcap_next(handle, &hdr);
        postcard_handler(&hdr, pkt);
    }

    /* cleanup */
    pcap_freecode(&fp);
    pcap_close(handle);

    printf("\nCapture complete.\n");
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
    if(signum == SIGALRM) {
        // signal round_cond
        pthread_cond_signal(&n.round_cond);
    }
}

