/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#include <getopt.h>
#include <vector>
#include <string>
#include <pthread.h>

#include "api.hh"
#include "ndb.hh"
#include "types.hh"
#include "helper.hh"
#include "postcard.hh"
#include "packet.hh"

using namespace std;

static struct option long_options[] =
{
    {"db", required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};

void
parse_args(int argc, char **argv, NDB &ndb)
{
    // loop over all of the options
    int option_index = 0;
    int ch;
    string db_host("localhost:27017");
    DBG("Parsing arguments...\n");
    while ((ch = getopt_long(argc, argv, "d:", 
                    long_options, &option_index)) != -1) {
        DBG("Got option: %c\n", ch);
        switch (ch) {
            case 'd':
                db_host = optarg;
                break;
            default:
                ERR("Unknown argument: %c:%s\n", ch, optarg);
                exit(EXIT_FAILURE);
        }
    }
    ndb.connect_db(db_host);
}

int 
main(int argc, char **argv)
{
    NDB &ndb = NDB::get_instance();
    parse_args(argc, argv, ndb);
    ndb.start();
    return 0;
}

