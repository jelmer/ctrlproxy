#!/usr/bin/python
print "testscript"

import ctrlproxy

print ctrlproxy.__dict__

class myEvent(ctrlproxy.Event):
	def onChannelJoin(self, line, data):
		print "Jear, i got a join :)\n"
		print data
	def onElse(self, line, data):
		print "something else\n"
		print data
	def onRcv(self, line, data):
		print "R: %s" %data
	#	print "got something"
	def onWho(self, line, data):
		print "who"
		print data
	def onWhois(self, line, data):
		print "whois ?"
		print data

n = myEvent()
ctrlproxy.add_event_object(n);
import sys
#sys.exit(0)
#return

def rcv(line):
	print "GOT\n"
	if(line.direction == ctrlproxy.TO_SERVER):
		print line.client.nick
		print line.client.connect_time
	if(line.direction == ctrlproxy.FROM_SERVER):
		print "FROM_SERVER"
	else:
		print "TO_SERVER"
	print line.args
	if(line.direction == ctrlproxy.TO_SERVER and line.args[0] == "PRIVMSG" and line.args[1] == "test1"):
		line.options = line.options | ctrlproxy.LINE_DONT_SEND | ctrlproxy.LINE_IS_PRIVATE
		print "THIS message is now hidden :)"
		line.client.send_notice("THIS message is now hidden :)");

def printNetwork(n):
	print "XMLconf:",n.xmlConf
	print "Mymodes:",n.mymodes
	print "Servers:",n.servers
	print "Hostmask:",n.hostmask
	print "Channels:",n.channels
	print "Authenticated:",n.authenticated
	print "Clients:",n.clients
	print "Current_server:",n.current_server
	print "Listen:",n.listen
	print "Supported_modes:",n.supported_modes
	print "Features:", n.features
	print "--END--"


def testAdmin(l,client):
	print "blubb"
	print l

def motd(net):
	return ["return","value"]

ctrlproxy.add_admin_command("test",testAdmin)

print "BLUBB\nBLA";

d = ctrlproxy.Log(ctrlproxy.WARNING)
d.write("TEST nachricht")

print ctrlproxy.add_rcv_hook(rcv)

def new_client(client):
	print "GOT NEW CLIENT"
	client.send_notice("TATA TUTU TITI")
	client.send_notice("TATA TUTU TITI")
	client.send_notice("TATA TUTU TITI")
	client.send_notice("TATA TUTU TITI")
	client.send_notice("TATA TUTU TITI")
	printNetwork(client.network)
	client.network.send("whois poelzi poelzi\n",ctrlproxy.TO_SERVER);

print ctrlproxy.add_new_client_hook(new_client)

#print ctrlproxy.add_motd_hook(motd)

print ctrlproxy.list_modules()

print ctrlproxy.list_networks()


for cur in ctrlproxy.list_networks():
	n = ctrlproxy.get_network(cur)
	printNetwork(n)

# returns the config xml object
config = ctrlproxy.get_config("plugins")

import time
import thread
import sys

def runme():
	time.sleep(10)
	print " ------------ I GOT A SLEEP -------------"
	# when the thread ends, it is a dead defunc thread (unsolved "problem")
	thread.exit()

print "THREAD"
#print thread.start_new(runme,())
n = myEvent();

#print ctrlproxy.Network.__dict__

print ctrlproxy.Line.__dict__

print ctrlproxy.list_networks()

print "DONE\n"
