#!/usr/bin/python
# Example script for use in ControlProxy
# (C) 2003 Daniel Poelzleithner <ctrlproxy@poelzi.org>

print "testscript"

import ctrlproxy

print ctrlproxy.__dict__

def rcv(line):
	print "GOT\n"
	print line.client.connect_time

def testAdmin(l,client):
	print l

ctrlproxy.add_admin_command("test",testAdmin)

print "BLUBB\nBLA";

d = ctrlproxy.Log(ctrlproxy.WARNING)
d.write("TEST nachricht")

print ctrlproxy.add_rcv_hook(rcv)

print ctrlproxy.list_modules()

# returns the config xml object
config = ctrlproxy.get_config("plugins")

print ctrlproxy.Network.__dict__

print ctrlproxy.Line.__dict__

print ctrlproxy.list_networks()

print "DONE\n"
