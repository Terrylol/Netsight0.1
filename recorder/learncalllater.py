from twisted.internet import reactor
import time
def fun():
    while True:

        print 'Current time is',time.strftime("%H:%M:%S")
        reactor.callLater(2,fun)
        exit()

fun()