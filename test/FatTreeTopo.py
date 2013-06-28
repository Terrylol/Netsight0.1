#!/usr/bin/python

from mininet.topo import Topo, Node, Edge, NodeID

class StructuredNodeSpec(object):
    '''Layer-specific vertex metadata for a StructuredTopo graph.'''

    def __init__(self, up_total, down_total, up_speed, down_speed,
                 type_str = None):
        '''Init.

        @param up_total number of up links
        @param down_total number of down links
        @param up_speed speed in Gbps of up links
        @param down_speed speed in Gbps of down links
        @param type_str string; model of switch or server
        '''
        self.up_total = up_total
        self.down_total = down_total
        self.up_speed = up_speed
        self.down_speed = down_speed
        self.type_str = type_str


class StructuredNode(Node):
    '''Node-specific vertex metadata for a StructuredTopo graph.'''

    def __init__(self, layer, connected = False, admin_on = True,
                 power_on = True, fault = False, is_switch = True):
        '''Init.

        @param layer layer within topo (int)
        @param connected actively connected to controller
        @param admin_on administratively on or off
        @param power_on powered on or off
        @param fault fault seen on node
        @param is_switch switch or host
        '''
        self.layer = layer
        super(StructuredNode, self).__init__(is_switch=is_switch)


class StructuredEdgeSpec(object):
    '''Static edge metadata for a StructuredTopo graph.'''

    def __init__(self, speed = 1.0):
        '''Init.

        @param speed bandwidth in Gbps
        '''
        self.speed = speed


class StructuredTopo(Topo):
    '''Data center network representation for structured multi-trees.'''

    def __init__(self, node_specs, edge_specs):
        '''Create StructuredTopo object.

        @param node_specs list of StructuredNodeSpec objects, one per layer
        @param edge_specs list of StructuredEdgeSpec objects for down-links,
            one per layer
        '''
        super(StructuredTopo, self).__init__()
        self.node_specs = node_specs
        self.edge_specs = edge_specs

    def layer(self, dpid):
        '''Return layer of a node

        @param dpid dpid of switch
        @return layer layer of switch
        '''
        return self.node_info[dpid].layer

    def isPortUp(self, port):
        ''' Returns whether port is facing up or down

        @param port port number
        @return portUp boolean is port facing up?
        '''
        return port % 2 == PORT_BASE

    def layer_nodes(self, layer, enabled = True):
        '''Return nodes at a provided layer.

        @param layer layer
        @param enabled only return enabled nodes?

        @return dpids list of dpids
        '''

        def is_layer(n):
            '''Returns true if node is at layer.'''
            return self.node_info[n].layer == layer

        nodes = [n for n in self.g.nodes() if is_layer(n)]
        return self.nodes_enabled(nodes, enabled)

    def up_nodes(self, dpid, enabled = True):
        '''Return edges one layer higher (closer to core).

        @param dpid dpid
        @param enabled only return enabled nodes?

        @return dpids list of dpids
        '''
        layer = self.node_info[dpid].layer - 1
        nodes = [n for n in self.g[dpid] if self.node_info[n].layer == layer]
        return self.nodes_enabled(nodes, enabled)

    def down_nodes(self, dpid, enabled = True):
        '''Return edges one layer higher (closer to hosts).

        @param dpid dpid
        @param enabled only return enabled nodes?

        @return dpids list of dpids
        '''
        layer = self.node_info[dpid].layer + 1
        nodes = [n for n in self.g[dpid] if self.node_info[n].layer == layer]
        return self.nodes_enabled(nodes, enabled)

    def up_edges(self, dpid, enabled = True):
        '''Return edges one layer higher (closer to core).

        @param dpid dpid
        @param enabled only return enabled edges?

        @return up_edges list of dpid pairs
        '''
        edges = [(dpid, n) for n in self.up_nodes(dpid, enabled)]
#        if enabled:
#            return [e for e in edges if self.edge_enabled(e)]
#        else:
#            return edges
        return self.edges_enabled(edges, enabled)

    def down_edges(self, dpid, enabled = True):
        '''Return edges one layer lower (closer to hosts).

        @param dpid dpid
        @param enabled only return enabled edges?

        @return down_edges list of dpid pairs
        '''
        edges = [(dpid, n) for n in self.down_nodes(dpid, enabled)]
#        if enabled:
#            return [e for e in edges if self.edge_enabled(e)]
#        else:
#            return edges
        return self.edges_enabled(edges, enabled)

#    def draw(self, filename = None, edge_width = 1, node_size = 1,
#             node_color = 'g', edge_color = 'b'):
#        '''Generate image of Ripcord network.
#
#        @param filename filename w/ext to write; if None, show topo on screen
#        @param edge_width edge width in pixels
#        @param node_size node size in pixels
#        @param node_color node color (ex 'b' , 'green', or '#0000ff')
#        @param edge_color edge color
#        '''
#        import matplotlib.pyplot as plt
#
#        pos = {} # pos[vertex] = (x, y), where x, y in [0, 1]
#        for layer in range(len(self.node_specs)):
#            v_boxes = len(self.node_specs)
#            height = 1 - ((layer + 0.5) / v_boxes)
#
#            layer_nodes = sorted(self.layer_nodes(layer, False))
#            h_boxes = len(layer_nodes)
#            for j, dpid in enumerate(layer_nodes):
#                pos[dpid] = ((j + 0.5) / h_boxes, height)
#
#        fig = plt.figure(1)
#        fig.clf()
#        ax = fig.add_axes([0, 0, 1, 1], frameon = False)
#
#        draw_networkx_nodes(self.g, pos, ax = ax, node_size = node_size,
#                               node_color = node_color, with_labels = False)
#        # Work around networkx bug; does not handle color arrays properly
#        for edge in self.edges(False):
#            draw_networkx_edges(self.g, pos, [edge], ax = ax,
#                                edge_color = edge_color, width = edge_width)
#
#        # Work around networkx modifying axis limits
#        ax.set_xlim(0, 1.0)
#        ax.set_ylim(0, 1.0)
#        ax.set_axis_off()
#
#        if filename:
#            plt.savefig(filename)
#        else:
#            plt.show()


class FatTreeTopo(StructuredTopo):
    '''Three-layer homogeneous Fat Tree.

    From "A scalable, commodity data center network architecture, M. Fares et
    al. SIGCOMM 2008."
    '''
    LAYER_CORE = 0
    LAYER_AGG = 1
    LAYER_EDGE = 2
    LAYER_HOST = 3

    class FatTreeNodeID(NodeID):
        '''Fat Tree-specific node.'''

        def __init__(self, pod = 0, sw = 0, host = 0, dpid = None):
            '''Create FatTreeNodeID object from custom params.

            Either (pod, sw, host) or dpid must be passed in.

            @param pod pod ID
            @param sw switch ID
            @param host host ID
            @param dpid optional dpid
            '''
            if dpid:
                self.pod = (dpid & 0xff0000) >> 16
                self.sw = (dpid & 0xff00) >> 8
                self.host = (dpid & 0xff)
                self.dpid = dpid
            else:
                self.pod = pod
                self.sw = sw
                self.host = host
                self.dpid = (pod << 16) + (sw << 8) + host

        def __str__(self):
            return "(%i, %i, %i)" % (self.pod, self.sw, self.host)

        def name_str(self):
            '''Return name string'''
            return "%i_%i_%i" % (self.pod, self.sw, self.host)

        def ip_str(self):
            '''Return IP string'''
            return "10.%i.%i.%i" % (self.pod, self.sw, self.host)
    """
    def _add_port(self, src, dst):
        '''Generate port mapping for new edge.

        Since Node IDs are assumed hierarchical and unique, we don't need to
        maintain a port mapping.  Instead, compute port values directly from
        node IDs and topology knowledge, statelessly, for calls to self.port.

        @param src source switch DPID
        @param dst destination switch DPID
        '''
        pass
    """
    def __init__(self, k = 4, speed = 1.0, cpu=-1, enable_all = True):
        '''Init.

        @param k switch degree
        @param speed bandwidth in Gbps
        @param enable_all enables all nodes and switches?
        '''
        core = StructuredNodeSpec(0, k, None, speed, type_str = 'core')
        agg = StructuredNodeSpec(k / 2, k / 2, speed, speed, type_str = 'agg')
        edge = StructuredNodeSpec(k / 2, k / 2, speed, speed,
                                  type_str = 'edge')
        host = StructuredNodeSpec(1, 0, speed, None, type_str = 'host')
        node_specs = [core, agg, edge, host]
        edge_specs = [StructuredEdgeSpec(speed)] * 3
        super(FatTreeTopo, self).__init__(node_specs, edge_specs)

        self.k = k
        self.id_gen = FatTreeTopo.FatTreeNodeID
        self.numPods = k
        self.aggPerPod = k / 2

        pods = range(0, k)
        core_sws = range(1, k / 2 + 1)
        agg_sws = range(k / 2, k)
        edge_sws = range(0, k / 2)
        hosts = range(2, k / 2 + 2)

        self.template_edge = Edge(bw = int(1000*speed))

        for p in pods:
            for e in edge_sws:
                edge_id = str(self.id_gen(p, e, 1).dpid)
                edge_node = StructuredNode(self.LAYER_EDGE, is_switch = True)
                self.add_node(edge_id, edge_node)

                for h in hosts:
                    host_id = str(self.id_gen(p, e, h).dpid)
                    host_node = StructuredNode(self.LAYER_HOST, 
                                               is_switch = False)
                    host_node.cpu = cpu
                    self.add_node(host_id, host_node)
                    self.add_edge(host_id, edge_id, self.template_edge)

                for a in agg_sws:
                    agg_id = str(self.id_gen(p, a, 1).dpid)
                    agg_node = StructuredNode(self.LAYER_AGG, is_switch = True)
                    self.add_node(agg_id, agg_node)
                    self.add_edge(edge_id, agg_id, self.template_edge)

            for a in agg_sws:
                agg_id = str(self.id_gen(p, a, 1).dpid)
                c_index = a - k / 2 + 1
                for c in core_sws:
                    core_id = str(self.id_gen(k, c_index, c).dpid)
                    core_node = StructuredNode(self.LAYER_CORE,
                                               is_switch = True)
                    self.add_node(core_id, core_node)
                    self.add_edge(core_id, agg_id, self.template_edge)

        if enable_all:
            self.enable_all()

    def port(self, src, dst):
        '''Get port number (optional)

        Note that the topological significance of DPIDs in FatTreeTopo enables
        this function to be implemented statelessly.

        @param src source switch DPID
        @param dst destination switch DPID
        @return tuple (src_port, dst_port):
            src_port: port on source switch leading to the destination switch
            dst_port: port on destination switch leading to the source switch
        '''

        # If there are non-FatTree nodes, query the superclass for ports
        if not (hasattr(self.node_info[src], 'layer') and hasattr(self.node_info[dst], 'layer')):
            return super(FatTreeTopo, self).port(src, dst)

        src_layer = self.node_info[src].layer
        dst_layer = self.node_info[dst].layer

        src_id = self.id_gen(dpid = int(src))
        dst_id = self.id_gen(dpid = int(dst))

        LAYER_CORE = 0
        LAYER_AGG = 1
        LAYER_EDGE = 2
        LAYER_HOST = 3

        if src_layer == LAYER_HOST and dst_layer == LAYER_EDGE:
            src_port = 0
            dst_port = (src_id.host - 2) * 2 + 1
        elif src_layer == LAYER_EDGE and dst_layer == LAYER_CORE:
            src_port = (dst_id.sw - 2) * 2
            dst_port = src_id.pod
        elif src_layer == LAYER_EDGE and dst_layer == LAYER_AGG:
            src_port = (dst_id.sw - self.k / 2) * 2
            dst_port = src_id.sw * 2 + 1
        elif src_layer == LAYER_AGG and dst_layer == LAYER_CORE:
            src_port = (dst_id.host - 1) * 2
            dst_port = src_id.pod
        elif src_layer == LAYER_CORE and dst_layer == LAYER_AGG:
            src_port = dst_id.pod
            dst_port = (src_id.host - 1) * 2
        elif src_layer == LAYER_AGG and dst_layer == LAYER_EDGE:
            src_port = dst_id.sw * 2 + 1
            dst_port = (src_id.sw - self.k / 2) * 2
        elif src_layer == LAYER_CORE and dst_layer == LAYER_EDGE:
            src_port = dst_id.pod
            dst_port = (src_id.sw - 2) * 2
        elif src_layer == LAYER_EDGE and dst_layer == LAYER_HOST:
            src_port = (dst_id.host - 2) * 2 + 1
            dst_port = 0
        else:
            raise Exception("Could not find port leading to given dst switch")

        # Shift by one; as of v0.9, OpenFlow ports are 1-indexed.
        if src_layer != LAYER_HOST:
            src_port += 1
        if dst_layer != LAYER_HOST:
            dst_port += 1

        return (src_port, dst_port)


