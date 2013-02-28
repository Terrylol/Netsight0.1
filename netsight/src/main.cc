#include <getopt.h>
#include "netsight.hh"

static struct option long_options[] =
{
    {"topo", required_argument, NULL, 't'},
    {"intf", required_argument, NULL, 'i'},
    {NULL, 0, NULL, 0}
};

void
parse_args(int argc, char **argv, NetSight &n)
{
    // loop over all of the options
    int option_index = 0;
    int ch;
    printf("Parsing arguments...\n");
    while ((ch = getopt_long(argc, argv, "i:t:", 
                    long_options, &option_index)) != -1) {
        switch (ch) {
            case 'i':
                n.set_sniff_dev(optarg);
                break;
            case 't':
                n.set_topo_filename(optarg);
                break;
            default:
                abort ();
        }
    }
}

int 
main(int argc, char **argv)
{
    NetSight &netsight = NetSight::get_instance();
    netsight.start();

    return 0;
}
