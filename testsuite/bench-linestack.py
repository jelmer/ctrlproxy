#!/usr/bin/python

import irc
import time

state = irc.NetworkState("nick", "user", "host")
ls = irc.Linestack("/tmp/bench-linestack", state)

m1 = ls.get_marker()
t = time.time()
n = 100000
for i in xrange(n):
    ls.insert_line(":somebody!someuser@somehost PRIVMSG #bla :Data",
                   irc.FROM_SERVER, state)
print "Wrote %d lines in %f" % (n, time.time()-t)
m2 = ls.get_marker()

t = time.time()
i = 0
for x in ls.traverse(m1, m2):
    i += 1
print "Read %d lines in %f" % (i, time.time()-t)
