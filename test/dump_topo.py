#!/usr/bin/python
import sys
import json
import re

# borrowed from Mininet code
def get_dpid(topo, sw_name):
    '''Get dpid of a switch from a topo object'''
    if 'dpid' in topo.node_info[sw_name]:
        return int(topo.node_info[sw_name]['dpid'], 16)
    else:
        try:
            dpidLen = 12
            dpid = int( re.findall( '\d+', sw_name )[ 0 ] )
            dpid = hex( dpid )[ 2: ]
            dpid = '0' * ( dpidLen - len( dpid ) ) + dpid
            return int(dpid)
        except IndexError:
            raise Exception( 'Unable to derive default datapath ID - '
                             'please either specify a dpid or use a '
                             'canonical switch name such as s23.' )

def dump_out_band_topo(topo, outfile=None, collector_sw=['s0',]):
    '''Dump out-of-band topology to be used as ndb input
    Assumption: s0 is the collector switch 
    to which all switches are directly connected'''

    f = open(outfile, 'w') if outfile else sys.stdout

    # output json object
    topo_j = {'topo': {'nodes': [], 
                        'links': []
                    },
            'collector': []
            }

    # list all switches except s0
    node_to_dpid = {}
    switches = list(set(topo.switches()).difference(set(collector_sw))) 
    for s in switches:
        node_to_dpid[s] = get_dpid(topo, s)

    # list links
    links = []
    for (u,v) in topo.links():
        # consider only sw-sw links
        if u in topo.hosts() or v in topo.hosts():
            continue
        if u in collector_sw or v in collector_sw:
            continue

        dpid1 = node_to_dpid[u]
        dpid2 = node_to_dpid[v]
        port1, port2 = topo.port(u, v)
        links.append({'src_dpid':dpid1, 'src_port':port1, 'dst_dpid':dpid2, 'dst_port':port2})

    topo_j['topo']['nodes'] = [{"name":k, "dpid":v} for (k, v) in node_to_dpid.iteritems()]
    topo_j['topo']['links'] = links

    # list collector location
    collector_loc = []
    for s in switches:
        s_coll = False
        for cs in collector_sw:
            if cs in topo.g[s]:
                dpid = node_to_dpid[s]
                port = topo.port(s, cs)[0]
                collector_loc.append({'dpid':dpid, 'port':port})
                s_coll = True
                break

        if not s_coll:
            print 'Warning: Switch %s does not have a link to the collector switch' % s
    topo_j['collector'] = collector_loc

    print >>f, json.dumps(topo_j)
    if outfile:
        f.close()

    return topo_j


def dump_in_band_topo(topo, outfile=None):
    '''Dump in-band topology to be used as ndb input
    Assumption: h0 is the collector host'''

    collector_host = 'h0'
    collector_sw = 's0'
    f = open(outfile, 'w') if outfile else sys.stdout

    # output json object
    topo_j = {'topo': {'nodes': [], 
                        'links': []
                    },
            'collector': []
            }

    # list all switches
    node_to_dpid = {}
    switches = topo.switches()
    for s in switches:
        node_to_dpid[s] = get_dpid(topo, s)

    # list links
    links = []
    for (u,v) in topo.links():
        # consider only sw-sw links
        if u in topo.hosts() or v in topo.hosts():
            continue

        dpid1 = node_to_dpid[u]
        dpid2 = node_to_dpid[v]
        port1, port2 = topo.port(u, v)
        links.append({'src_dpid':dpid1, 'src_port':port1, 'dst_dpid':dpid2, 'dst_port':port2})

    topo_j['topo']['nodes'] = [{"name":k, "dpid":v} for (k, v) in node_to_dpid.iteritems()]
    topo_j['topo']['links'] = links

    # collector location
    collector_sw = topo.g[collector_host].keys()[0]
    collector_dpid = node_to_dpid[collector_sw]
    collector_port, _ = topo.port(collector_sw, collector_host)
    topo_j['collector'] = {'dpid':collector_dpid, 'port':collector_port}

    print >>f, json.dumps(topo_j)
    if outfile:
        f.close()

    return topo_j

