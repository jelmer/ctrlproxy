###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy
import urllib
from __init__ import *

class networks(page):
	title = "Network"

	def save(self, name):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			net = None
			xml = ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)[0]
		uname = urllib.quote(name)
		info = 0
		for (namek, value) in self.handler.post_headers.items():
			if namek == "namek" and value[0] != name:
				if len(ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %value[0])):
					self.wfile.write('<p class="error">Network name already exists</p>')
					break;
			if namek == "nick" and net and xml.prop("nick") != value[0]:
				net.send("NICK %s\r\n" %value[0])
			else:
				if xml.prop(namek) != value[0]:
					xml.setProp(namek, value[0])
					if net and namek in ["username","fullname"]:
						info = 1

		if info:
			self.wfile.write('<p class="info">Some changes require</p>')
		self.list_networks()

	def new(self):
		name = self.handler.post_headers["name"][0]
		if len(ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)):
			self.wfile.write('<p class="error">' + name + ' already exists</p>')
			self.list_networks()
			return
		node = ctrlproxy.get_config("networks").newChild(None, "network", "")
		node.setProp("name", name)
		node.newChild(None, "servers", "")
		node.newChild(None, "listen", "")
		self.edit(name)

	def delete(self, name):
		net = ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)
		if not len(net):
			self.wfile.write('<p class="error">' + name + " doesn't exist</p>")
		else:
			if name in ctrlproxy.list_networks():
				self.close(name)
			net[0].unlinkNode()
			net[0].freeNode()
		self.list_networks()

	def edit(self, name):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)[0]

		uname = urllib.quote(name)
		self.wfile.write('<form method="post" action="/networks/save/%s">' %(uname))
		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="3">Edit Server</th></tr>')
		for n in ['name',"nick","username","fullname"]:
			value = xml.prop(n)
			if value == None:
				value = ""
			self.wfile.write('<tr><td class="title">%s</td><td class="value"><input name="%s" value="%s" /></td></tr>' %(n.capitalize(),n,value))
		value = xml.prop("client_pass")
		if value == None:
			value = ""
		self.wfile.write('<tr><td class="title">Client_pass</td><td class="value"><input type="password" name="client_pass" value="%s"></td></tr>' %value)
		self.wfile.write('<tr><td class="title">Autoconnect</td><td class="value"><select name="autoconnect"><option value="0">No</option><option value="1"%s>Yes</option> </td></tr>' %selectedSwitch(xml.prop('autoconnect')))
		self.wfile.write('<tr><td></td><td><input type="submit" value="Save"></td></tr>')
		self.wfile.write('</table></form>')

	def view(self, name):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)
		self.wfile.write('<table class="infos">')
		if net:
			self.wfile.write('<tr><th class="topic" colspan="2">%s (online)</th></tr>' %xml.prop("name"))
		else:
			self.wfile.write('<tr><th class="topic" colspan="2">%s (offline)</th></tr>' %xml.prop("name"))
		if net:
			if net.current_server.prop("port"): port = ":" + net.current_server.prop("port")
			else: port = ""
			self.wfile.write('<tr><td class="title">Server</td><td class="value">%s%s</td></tr>' %(net.current_server.prop("host"),port))
			self.wfile.write('<tr><td class="title">Hostmask</td><td class="value">%s</td></tr>' %(net.hostmask))
			self.wfile.write('<tr><td class="title">Modes</td><td class="value">%s</td></tr>' %(net.mymodes))
			self.wfile.write('<tr><td class="title">Clients</td><td class="value">%i</td></tr>' %len(net.clients))
		for n in ('autoconnect','name','nick','username','fullname'):
			self.wfile.write('<tr><td class="title">%s</td><td class="value">%s</td></tr>' %(n.capitalize(),xml.prop(n)))
		title = "Channels"
		for i in xml.xpathEval("//channel"):
			if i.prop("autojoin") == "1":
				self.wfile.write('<tr><td class="title">%s</td><td class="active">%s</td></tr>' %(title,i.prop("name")))
			else:
				self.wfile.write('<tr><td class="title">%s</td><td class="inactive">%s</td></tr>' %(title,i.prop("name")))
			title = ""
		self.wfile.write('</td></tr>')
		self.wfile.write('</table><br>')
		self.wfile.write('<a class="func" href="/networks/edit/' + uname + '">Edit</a>')
		if net:
			self.wfile.write('<a class="func" href="/networks/close/' + uname + '">Close</a>')
		else:
			self.wfile.write('<a class="func" href="/networks/open/' + uname + '">Open</a>')
		self.wfile.write('<a class="func" href="/confirm/?msg=' + urllib.quote("Really delete Network %s ?" %name) + '&page=/networks/delete/' + uname + '">Delete</a><p />')

		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="5">Servers</th></tr>')
		self.wfile.write('<tr><td class="title">#</td><td class="title">Type</td><td class="title">Info</td><td></td>')
		lines = xml.xpathEval('servers/*')
		c = 1
		for s in lines:
			hinfo = ""
			if s.properties:
				for attr in s.properties:
					if attr.name != "text":
						hinfo += " %s=%s" %(attr.name,attr.content)
				hinfo += " %s" %s.content
			else:
				hinfo += " %s" %s.content
			hinfo = hinfo.strip()
			if net and not s.xpathCmpNodes(net.current_server):
				self.wfile.write('<tr><td class="active">%s</td><td class="active">%s</td><td class="active">%s</td><td><a href="/networks/editserver/%s/%i">Edit</a> <a href="/networks/delserver/%s/%i">Del</a></td>' %(c, s.name, hinfo,uname,c,uname,c))
				#self.wfile.write('<tr><td class="active">%s</td><td class="active">%s</td><td class="active">%s</td><td><a href="/networks/jumpserver/%s/%i">Jump</a> <a href="/networks/editserver/%s/%i">Edit</a> <a href="/networks/delserver/%s/%i">Del</a></td>' %(c, s.name, hinfo,uname,c,uname,c,uname,c))
			else:
				self.wfile.write('<tr><td class="inactive">%s</td><td class="inactive">%s</td><td class="inactive">%s</td><td><a href="/networks/editserver/%s/%i">Edit</a> <a href="/confirm/?msg=%s&page=/networks/delserver/%s/%i">Del</a></td>' %(c, s.name,hinfo,uname,c,urllib.quote("Really delete this server ?"),uname,c))
				#self.wfile.write('<tr><td class="inactive">%s</td><td class="inactive">%s</td><td class="inactive">%s</td><td><a href="/networks/jumpserver/%s/%i">Jump</a> <a href="/networks/editserver/%s/%i">Edit</a> <a href="/networks/delserver/%s/%i">Del</a></td>' %(c, s.name,hinfo,uname,c,uname,c,uname,c))
			c = c+1
		self.wfile.write('</table><br />')
		self.wfile.write('<a class="func" href="/networks/reconnect/' + uname + '">Reconnect</a>')
		self.wfile.write('<a class="func" href="/networks/nextserver/' + uname + '">Next Server</a>')
		self.wfile.write('<a class="func" href="/networks/newserver/%s">New Server</a><br />' %uname)

	def newserver(self, name):
		uname = urllib.quote(name)
		self.wfile.write('<form method="post" action="/networks/editserver/%s/0">' %uname)
		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="5">New Server</th></tr>')
		self.wfile.write('<tr><td class="topic">Transport</td><td>')
		tl = ctrlproxy.list_transports()
		self.wfile.write('<select name="transport">')
		for i,p in tl.items():
			self.wfile.write('<option>%s</option>' %i)
		self.wfile.write('</select>')
		self.wfile.write('</td></tr>')
		self.wfile.write('<tr><td></td><td><input type="submit" value="Next"></td></tr>')
		self.wfile.write('</table></form><br />')

	def delserver(self, name, server):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)

		server = xml.xpathEval("servers/*[%s]" %server)[0]
		if net and not server.xpathCmpNodes(net.current_server):
			net.disconnect()
		server.unlinkNode()
		server.freeNode()
		self.view(name)

	def editserver(self, name, server):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
			#self.wfile.write('<p class="error">' + name + ' is not online</p>')
			#xml =
			#return
		uname = urllib.quote(name)
		if server == '0':
			try:
				snode = xml.xpathEval('servers')[0]
			except:
				snode = xml.newChild(None,"servers","")
			line = snode.newChild(None,self.handler.post_headers["transport"][0],"")

			server = 0
			for i in xml.xpathEval('servers/*'):
				if not i.xpathCmpNodes(line):
					break
				server += 1
		else:
			try:
				line = xml.xpathEval('servers/*[%s]' %server)[0]
			except:
				self.wfile.write('<p class="error">Error access transport</p>')
				return
		tl = ctrlproxy.list_transports()
		plugin = ctrlproxy.tools.Moduleinfo(tl[line.name].name)
		trans = plugin.doc.xpathEval("//transports/transport[@name='%s']" %line.name)[0]
		self.wfile.write('<form method="post" action="/networks/saveserver/%s/%s">' %(uname,server))
		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="3">Edit Server</th></tr>')
		self.wfile.write('<tr><td class="topic">Transport</td><td>')
		# ipv4 and ipv6 can be changed
		if line.name == "ipv4" or line.name == "ipv6":
			self.wfile.write('<select name="transport">')
			self.wfile.write('<option>ipv4</option>')
			if line.name == "ipv6":
				self.wfile.write('<option selected="1">ipv6</option>')
			else:
			 	self.wfile.write('<option>ipv6</option>')
			self.wfile.write('</select>')
		elif line:
			self.wfile.write(line.name)
		print "//transports/transport[@name='%s']" %line.name
		for i in trans.children:
			print i
			if i.name == "attribute" or i.name == "content":
				txt = ""
				value = None
				if i.name == "attribute":
				 	value = line.prop(i.prop("name"))
					if value == None:
						value = ""
				else:
					value = line.xpathEval("//%s" %i.prop("name"))
					if len(value):
						value = value[0].content
					else:
						value = ""

				if i.prop("type") == "bool":
					txt = '<select name="%s"><option value="0">No</option>' %i.prop("name")
					if line.prop(i.prop("name")) not in (None,"") and line.prop(i.prop("name")) == '1':
						txt += '<option value="1" selected="1">Yes</option></select>'
					else:
						txt += '<option value="1">Yes</option></select>'
				else:
					txt = '<input type="text" name="%s" value="%s" />' %(i.prop("name"), value)

				self.wfile.write('<tr><td>%s</td><td>%s</td><td><i>%s</i></td></tr>' %(i.prop("name"),txt,i.content))

		self.wfile.write('</td></tr>')
		self.wfile.write('<tr><td></td><td><input type="submit" value="Save"></td></tr>')
		self.wfile.write('</table></form>')

	def saveserver(self, name, server):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)
		try:
			line = xml.xpathEval('servers/*[%s]' %server)[0]
		except:
			self.wfile.write('<p class="error">Error access transport</p>')
			return
		tl = ctrlproxy.list_transports()
		plugin = ctrlproxy.tools.Moduleinfo(tl[line.name].name)
		trans = plugin.doc.xpathEval("//transports/transport[@name='%s']" %line.name)[0]
		for i in trans.children:
			if i.name == "attribute" or i.name == "content":
				if i.name == "attribute":
					if self.handler.post_headers.has_key(i.prop("name")):
						if i.prop("type") == "bool" and self.handler.post_headers[i.prop("name")][0] == '0':
							line.unsetProp(i.prop("name"))
						else:
							line.setProp(i.prop("name"), self.handler.post_headers[i.prop("name")][0])
					elif line.prop(i.prop("name")):
						line.unsetProp(i.prop("name"))
				else:
					xnodes = line.xpathEval("//%s" %i.prop("name"))
					if self.handler.post_headers.has_key(i.prop("name")):
						if len(xnodes):
							xnodes[0].setContent = self.handler.post_headers[i.prop("name")][0]
						else:
							nnode = line.newChild(None,i.prop("name"), self.handler.post_headers[i.prop("name")][0])
					else:
						if len(xnodes):
							xnodes[0].unlinkNode()
							xnodes[0].free()
		self.wfile.write('<p class="info">Saved</p>')
		self.view(name)

	def reconnect(self, name):
		if not name in ctrlproxy.list_networks():
			self.wfile.write('<p class="error">' + name + ' is offline</p>')
		else:
			self.wfile.write('<p class="info">Reconnecting ' + name + '</p>')
			net = ctrlproxy.get_network(name)
			net.reconnect()
			self.list_networks()

	def nextserver(self, name):
		if not name in ctrlproxy.list_networks():
			self.wfile.write('<p class="error">' + name + ' is offline</p>')
		else:
			self.wfile.write('<p class="info">Cycle server at ' + name + '</p>')
			net = ctrlproxy.get_network(name)
			net.next_server()
			self.list_networks()

	def jumpserver(self, name, server):
		if not name in ctrlproxy.list_networks():
			self.wfile.write('<p class="error">' + name + ' is offline</p>')
		else:
			net = ctrlproxy.get_network(name)
			try:
				net.disconnect()
			except: pass
			print net.xmlConf.xpathEval("//servers/*[%s]" %server)[0]
			# FIXME: Something wrong in python.c
			#net.current_server = net.xmlConf.xpathEval("//servers/*[%s]" %server)[0]
			#net.current_server = None
			net.connect()
			self.list_networks()

	def open(self, name):
		if name in ctrlproxy.list_networks():
			self.wfile.write('<p class="error">' + name + ' is already online</p>')
		else:
			self.wfile.write('<p class="info">Start ' + name + '</p>')
			ctrlproxy.connect_network(name)
			self.list_networks()

	def close(self, name):
		if name in ctrlproxy.list_networks():
			self.wfile.write('<p class="info">Shutdown ' + name + '</p>')
			ctrlproxy.disconnect_network(name)
			self.list_networks()
		else:
			self.wfile.write('<p class="error">' + name + ' is not online</p>')

	def nextserver(self, name):
		if not name in ctrlproxy.list_networks():
			self.wfile.write('<p class="error">' + name + ' is not online</p>')
		else:
			net = ctrlproxy.get_network(name)
			net.next_server()
			self.wfile.write('<p class="info">Cycle server at' + name + '</p>')
		self.list_networks()

	def index(self):
		self.list_networks()

	def list_networks(self):
		net = ctrlproxy.get_config("networks")
		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="5">networks</th></tr>')
		for i in net.children:
			if i.name == "network":
				if i.prop("name"):
					a = i.prop("name")
					uname = urllib.quote(a)
					self.wfile.write('<tr><td class="title"><a href="/networks/view/' + uname + '">' + a + '</a></td>')
					if a in ctrlproxy.list_networks():
						self.wfile.write('<td class="online">connected</td>')
						self.wfile.write('<td class="value"><a href="/networks/close/' + uname + '">close</a></td>')
					else:
						self.wfile.write('<td class="offline">offline</td>')
						self.wfile.write('<td class="value"><a href="/networks/open/' + uname + '">connect</a></td>')
					self.wfile.write('<td class="value"><a href="/networks/view/' + uname + '">view</a></td><td class="value"><a href="/networks/edit/' + uname + '">edit</a></td>')
					self.wfile.write('</tr>')
		self.wfile.write('<tr><td class="title">New</td><td colspan="4" type="value"><form action="/networks/new" method="post"><input name="name"></form></td></tr>')
		self.wfile.write('</table>')


	def send(self):
		pages = {
		# first url:argnumbers, function
		"editserver":[2,self.editserver],
		"saveserver":[2,self.saveserver],
		"jumpserver":[2,self.jumpserver],
		"delserver":[2,self.delserver],
		"edit":[1,self.edit],
		"open":[1,self.open],
		"close":[1,self.close],
		"reconnect":[1,self.reconnect],
		"nextserver":[1,self.nextserver],
		"view":[1,self.view],
		"newserver":[1,self.newserver],
		"save":[1,self.save],
		"delete":[1,self.delete],
		"new":[0,self.new]
		}

		page.send_default(self, pages)


def init():
	add_menu("networks","Networks",10)
