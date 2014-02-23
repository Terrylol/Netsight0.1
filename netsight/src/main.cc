/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#include <getopt.h>
#include "netsight.hh"

static struct option long_options[] =
{
    {"topo", required_argument, NULL, 't'},
    {"intf", required_argument, NULL, 'i'},
    {"db", required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};

void
parse_args(int argc, char **argv, NetSight &n)
{
    // loop over all of the options
    int option_index = 0;
    int ch;
    string db_host("localhost:27017");
    DBG("Parsing arguments...\n");
    while ((ch = getopt_long(argc, argv, "i:t:d:", 
                    long_options, &option_index)) != -1) {
        DBG("Got option: %c\n", ch);
        switch (ch) {
            case 'i':
                n.set_sniff_dev(optarg);
                break;
            case 't':
                n.set_topo_filename(optarg);
                break;
            case 'd':
                db_host = optarg;
                break;
            default:
                ERR("Unknown argument: %c:%s\n", ch, optarg);
                exit(EXIT_FAILURE);
        }
    }
    n.connect_db(db_host);
}

int 
main(int argc, char **argv)
{
    NetSight &n= NetSight::get_instance();
    parse_args(argc, argv, n);
    n.start();
    DBG("Bye!\n");
    return 0;
}
