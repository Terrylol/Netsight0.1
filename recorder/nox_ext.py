#!/usr/bin/python
'''Vendor Extension handler for NOX'''

from openfaucet import ofproto
from twisted.internet import reactor
from zope import interface

# NOX vendor ID
NOX_VENDOR_ID = 0x00002320
VENDOR_ID_LEN = 4

class NOXVendorHandler(object):
    '''Handle NOX vendor-specific messages'''

    interface.implements(ofproto.IOpenflowVendorHandler)

    vendor_id = NOX_VENDOR_ID

    def __init__(self, ndbproxy):
        super(NOXVendorHandler, self).__init__()
        self.ndbproxy = ndbproxy

    @staticmethod
    def serialize_vendor_action(action):
        pass

    def deserialize_vendor_action(action_length, buf):
        pass

    # Required methods.

    def connection_made(self):
        pass

    def connection_lost(self, reason):
        pass

    def handle_vendor_message(self, msg_length, xid, buf):
        '''Send vendor message to the peer'''
        if not self.ndbproxy.peer:
            reactor.callLater(1, self.handle_vendor_message, msg_length, xid, buf)
            return
        self.ndbproxy.logger.debug('handle_vendor_message: msg_length: %d, xid: %d' % (msg_length, xid))
        #to_read = msg_length - ofproto.OFP_HEADER_LENGTH - VENDOR_ID_LEN
        #data = (str(buf.read_bytes(to_read)), )
        data = (str(buf.read_bytes(buf.message_bytes_left)),)
        self.ndbproxy.peer.send_vendor(self.vendor_id, xid, data)

    def handle_vendor_stats_request(self, msg_length, xid, buf):
        '''Send vendor stats request to the peer'''
        if not self.ndbproxy.peer:
            reactor.callLater(1, self.handle_vendor_stats_request, msg_length, xid, buf)
            return
        self.ndbproxy.logger.debug('handle_vendor_stats_request: msg_length: %d, xid: %d' % (msg_length, xid))
        #to_read = msg_length - ofproto.OFP_HEADER_LENGTH - VENDOR_ID_LEN
        #data = (str(buf.read_bytes(to_read)), )
        data = (str(buf.read_bytes(buf.message_bytes_left)),)
        self.ndbproxy.peer.send_stats_request_vendor(xid, self.vendor_id, data)

    def handle_vendor_stats_reply(self, msg_length, xid, buf, reply_more):
        '''Send vendor stats reply to the peer'''
        if not self.ndbproxy.peer:
            reactor.callLater(1, self.handle_vendor_stats_reply, msg_length, xid, buf, reply_more)
            return
        self.ndbproxy.logger.debug('handle_vendor_stats_reply: msg_length: %d, xid: %d' % (msg_length, xid))
        #to_read = msg_length - ofproto.OFP_HEADER_LENGTH - VENDOR_ID_LEN
        #data = (str(buf.read_bytes(to_read)), )
        data = (str(buf.read_bytes(buf.message_bytes_left)),)
        self.ndbproxy.peer.send_stats_reply_vendor(xid, self.vendor_id, data, reply_more)

