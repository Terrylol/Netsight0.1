/*
 * Sources: 
 * http://www.tcpdump.org/sniffex.c
 * http://yuba.stanford.edu/~casado/pcap/
 * */

#include <signal.h>
#include <pcap.h>
#include <pthread.h>
#include "netsight.hh"

#define DEVICE "eth0"
#define SNAP_LEN 1514
#define POSTCARD_FILTER ""

using namespace std;
static int SKIP_ETHERNET = 0;

extern PostcardList stage;
extern pthread_mutex_t stage_lock;
extern pthread_cond_t round_cond;

void 
sig_handler(int signum)
{
    if(signum == SIGALRM) {
        // signal round_cond
        pthread_cond_signal(&round_cond);
    }
}

void 
sniff_pkts(const char *dev, pcap_handler cb)
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

    /* now we can set our callback function */
    pcap_loop(handle, num_packets, cb, NULL);

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
postcard_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    static u32 packet_number = 0;
    pthread_mutex_lock(&stage_lock);
    stage.push_back(new PostcardNode(new Packet(packet, header->len, 
                    SKIP_ETHERNET, packet_number++, header->caplen)));
    Packet *pkt = (stage.tail)->pkt;
    pkt->ts = header->ts;
    pthread_mutex_unlock(&stage_lock);
}

void*
run_postcard_worker(void *args)
{
    /* Setup round signal handler */
    struct sigaction sact;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    sact.sa_handler = sig_handler;
    sigaction(SIGALRM, &sact, NULL);

    /* Start packet capture */
    // TODO: take dev as input
    const char *dev = DEVICE;
    sniff_pkts(dev, postcard_handler);
}
