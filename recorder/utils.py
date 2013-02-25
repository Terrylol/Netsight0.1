import StringIO
import time
import sys
from pydoc import pager
import termcolor as T
from subprocess import Popen, PIPE

try:
    import dpkt
except:
    dpkt = False

def collect(ntuple, sep='\n'):
    lst = []
    for field in ntuple._fields:
        val = getattr(ntuple, field)
        lst.append("%s=%s" % (field, val))
    return sep.join(lst)

def indent(lst, w=1):
    return map(lambda l: ('\t'*w) + l, lst)

def indent_str(s,w=1):
    return indent(s.split("\n"), w)

def mac(addr):
    return ':'.join(map(lambda octet: "%02x" % ord(octet),
            addr))

def ipaddr(addr):
    return '.'.join(map(lambda octet: "%d" % ord(octet),
            addr))

def red(s):
    return T.colored(s, "red", attrs=['bold'])

def yellow(s):
    return T.colored(s, "yellow")

def green(s):
    return T.colored(s, "green")

def format_packet(bytearr):
    if dpkt:
        return dpkt.ethernet.Ethernet(str(bytearr))
    else:
        return bytearr

def waitListening(server, port, attempts=10):
    "Wait until server is listening on port"
    if not 'telnet' in Popen('which telnet', shell=True, stdout=PIPE).communicate()[0]:
        raise Exception('Could not find telnet')
    cmd = ('sh -c "echo A | telnet -e A %s %s"' % (server, port))
    print 'Waiting for', server, 'to listen on port', port
    for i in range(attempts):
        if 'Connected' not in Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE).communicate()[0]:
            print '.',
            sys.stdout.flush()
            time.sleep(.5)
        else:
            return
    raise Exception('Could not connect to the MongoDB server at %s:%s' %
            (server, port))

class MyDict(dict):
    def __init__(self, hsh={}, should_trim=False, indent_level=0):
        super(MyDict, self).__init__(hsh)
        self.should_trim = should_trim
        self.indent_level = indent_level

    def __repr__(self):
        def trim(s):
            if len(s) > 100:
                return s[0:100] + "..."
            return s
        rep = ["Properties:"]
        for k, v in self.iteritems():
            if type(v) == bytearray:
                v = format_packet(v)
            if self.should_trim:
                rep.append("\t"*self.indent_level + "%s=%s" % (k, trim(v.__repr__())))
            else:
                rep.append("Key: %s" % k)
                rep.append("\t"*self.indent_level + "%s" % v.__repr__())
        return "\n".join(rep)

    def __getattr__(self, name):
        return self.get(name, None)

