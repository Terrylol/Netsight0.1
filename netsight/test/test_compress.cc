/*
 * Copyright 2014, Stanford University. This file is licensed under Apache 2.0,
 * as described in included LICENSE.txt.
 *
 * Author: nikhilh@cs.stanford.com (Nikhil Handigol)
 *         jvimal@stanford.edu (Vimal Jeyakumar)
 *         brandonh@cs.stanford.edu (Brandon Heller)
 */

#include <iostream>
#include <cstdio>
#include <pcap.h>

#include "tut/tut.hpp"
#include "compress/compress.hh"
#include "packet.hh"
#include "helper.hh"
#include "sample_packets.hh"

namespace tut 
{
    using namespace std;

    struct compress_data {
    };
    
    typedef test_group<compress_data> compress_tg;
    typedef compress_tg::object compresstestobject;

    compress_tg compression_test_group("compress test");
    
    /*
    template<> 
    template<> 
    void 
    compresstestobject::test<1>()
    { 
        set_test_name("hexify_byteify");
        for(int i = 0; i < nelem(sample_packets_hex); i++) {
            u8 buf[MAX_PKT_SIZE];
            char hex[MAX_PKT_SIZE];
            size_t buflen;
            byteify_packet(sample_packets_hex[i], buf, &buflen);
            hexify_packet(buf, hex, buflen);
            ensure("Original == Byte-Hex", strcmp(sample_packets_hex[i], hex) == 0);
        }
    }

    template<> 
    template<> 
    void 
    compresstestobject::test<2>()
    { 
        set_test_name("compress_nocrash");
        Compressor c;
        int packet_number = 0;
	size_t uncomp_size = 0;

        for(int i = 0; i < nelem(sample_packets_hex); i++) {
            u8 buf[MAX_PKT_SIZE];
            size_t buflen;
            byteify_packet(sample_packets_hex[i], buf, &buflen);
            Packet p(buf, buflen, 0, packet_number++, buflen);
            c.write_pkt(p);
            uncomp_size += buflen;
        }
        c.flush();
        size_t comp_size = c.diff_csize + c.ts_delta_csize + c.firstpkt_csize;

        printf("Uncompressed size: %lu\n", uncomp_size);
        printf("Compressed size: %lu\n", comp_size);
        ensure("compression", comp_size <= uncomp_size);
    }

    template<> 
    template<> 
    void 
    compresstestobject::test<3>()
    { 
        set_test_name("compress_decompress");
        Compressor c;
        int packet_number = 0;
	size_t uncomp_size = 0;

        for(int i = 0; i < nelem(sample_packets_hex); i++) {
            u8 buf[MAX_PKT_SIZE];
            bzero(buf, MAX_PKT_SIZE);
            size_t buflen;
            byteify_packet(sample_packets_hex[i], buf, &buflen);
            Packet p(buf, buflen, 0, packet_number++, buflen);
            c.write_pkt(p);
            uncomp_size += buflen;
        }
        c.flush();
        size_t comp_size = c.diff_csize + c.ts_delta_csize + c.firstpkt_csize;
        printf("Done compression.\n");

        Decompressor d(fileno(c.fp_ts), fileno(c.fp_firstpkt), fileno(c.fp_diff));
        printf("Decompressor::Decompressor\n");
        int num_decomp_packets = 0;
        while(true) {
            char hex[MAX_PKT_SIZE];
            bzero(hex, MAX_PKT_SIZE);
            printf("d.read_pkt\n");
            Packet *p = d.read_pkt(NULL);
            if(!p)
                break;
            hexify_packet(p->buff, hex, p->caplen);
            printf("Packet no.: %d\n", num_decomp_packets);
            printf("caplen: %d\n", p->caplen);
            printf("Original: %s\n", sample_packets_hex[num_decomp_packets]);
            printf("Decomp  : %s\n", hex);
            //ensure("Original == Decompress", strncmp(sample_packets_hex[num_decomp_packets], hex, p->caplen) == 0);
            num_decomp_packets++;
        }
        ensure("Packet num", num_decomp_packets == nelem(sample_packets_hex));
    }
*/
}
