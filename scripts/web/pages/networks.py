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
			if namek == "name" and value[0] != name:
				if len(ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %value[0])):
					self.t.write(self.tmpl.format('info',
						msg='Network name already exists'))
					break;
			if namek == "nick" and net and xml.prop("nick") != value[0]:
				net.send("NICK %s\r\n" %value[0])
			elif namek[0:4] != "mod_":
				if xml.prop(namek) != value[0]:
					xml.setProp(namek, value[0])
					if net and namek in ["username","fullname"]:
						info = 1
			else:
				try:
					cur = xml.xpathEval(namek[4:])
					cur[0].setContent(value[0])
				except:
					cur = xml.newChild(None, namek[4:], value[0])
					cur.addPrevSibling(cur.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(cur)))

		if info:
			self.t.write(self.tmpl.format('info',
				msg='Some changes require a reconnect'))
			self.list_networks()
		self.list_networks()

	def new(self):
		name = self.handler.post_headers["name"][0]
		if len(ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)):
			self.t.write(self.tmpl.format('warning',
				msg=name+' already exists'))
			self.list_networks()
			return
		node = ctrlproxy.get_config("networks").newChild(None, "network", "")
		node.addPrevSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node.parent)))
		node.setProp("name", name)
		x = node.newChild(None, "servers", "")
		x.addPrevSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node)))
		x = node.newChild(None, "listen", "")
		x.addPrevSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node)))
		x.addNextSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node.parent)))
		self.edit(name)

	def delete(self, name):
		net = ctrlproxy.get_config("networks").xpathEval("//network[@name='%s']" %name)
		if not len(net):
			self.t.write(self.tmpl.format("error", msg="%s doesn't exist" %name))
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

		f = self.t.open_layer("form", action="/networks/save/%s" %uname)
		t = f.open_layer("infos", title="Edit Network", colspan="2")

		for n in ['name',"nick","username","fullname"]:
			value = xml.prop(n)
			if value == None:
				value = ""
			t.write(self.tmpl.format("pair_row", title=n.capitalize(),
					contents=self.tmpl.format("input", name=n, value=value)))
		value = xml.prop("client_pass")
		if value == None:
			value = ""
		t.write(self.tmpl.format("pair_row", title="Client_pass",
				contents=self.tmpl.format("password", name=n, value=value)))
		t.write(self.tmpl.format("pair_row", title="Autoconnect",
				contents=self.tmpl.format("toggle", name="autoconnect", yes="Yes", no="No",
											on = sel(xml.prop('autoconnect') == "1"),
											off = sel(xml.prop('autoconnect') in ["0","",None])
											)))

		t.write(self.tmpl.format("pair_row", title=templayer.RawHTML("&nbsp;"), contents=templayer.RawHTML("&nbsp;")))

		t.write(self.tmpl.format("topic", colspan="2", contents="Module options"))

		lst = ctrlproxy.tools.get_xml_module_names()
		for i in lst:
			module = ctrlproxy.tools.Moduleinfo(i)
			for j in module.doc.xpathEval('//networks/element'):
				try: value = xml.xpathEval(j.prop("name"))[0].content
				except: value = ""
				t.write(self.tmpl.format("row", contents=[
					self.tmpl.format("cell_cl", cl="title", contents=j.prop("name").capitalize()),
					self.tmpl.format("cell_cl", cl="title", contents=self.tmpl.format("input", name="mod_"+j.prop("name"), value=value)),
					self.tmpl.format("cell_cl", cl="description", contents=j.content)
					]))

		t.write(self.tmpl.format("pair_row", title=templayer.RawHTML("&nbsp;"), contents=templayer.RawHTML("&nbsp;")))

		t.write(self.tmpl.format("pair_row", title="",
			contents=self.tmpl.format("submit", title="Save")))


	def view(self, name):
		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)
		if net:
			title = "%s (online)" %xml.prop("name")
		else:
			title = "%s (offline)" %xml.prop("name")

		c = self.t.open_layer("infos", title=title, colspan="2")

		if net:
			if net.current_server.prop("port"): port = ":" + net.current_server.prop("port")
			else: port = ""
			c.write(self.tmpl.format("pair_row", title="Server", contents=net.current_server.prop("host")+port))
			c.write(self.tmpl.format("pair_row", title="Hostmask", contents=str(net.hostmask)))
			c.write(self.tmpl.format("pair_row", title="Modes", contents=net.mymodes))
			c.write(self.tmpl.format("pair_row", title="Clients", contents=str(len(net.clients))))
			#c.write(self.tmpl.format("pair_row", title="Server", value=net.current_server.prop("host"),port))
			#c.write(self.tmpl.format("pair_row", title="Server", value=net.current_server.prop("host"),port))
		for n in ('autoconnect','name','nick','username','fullname'):
			c.write(self.tmpl.format("pair_row", title=n.capitalize(), contents=nstr(xml.prop(n))))
		title = "Channels"
		for i in xml.xpathEval("channel"):
			if i.prop("autojoin") == "1":
				cl = "active"
			else:
				cl = "inactive"
			c.write(self.tmpl.format("row", contents=[self.tmpl.format("cell_cl", cl="title", contents=title),
													self.tmpl.format("cell_cl", contents=i.prop("name"), cl=cl)]))
			title = ""

		self.t.write(self.tmpl.format("a_func", href='/networks/edit/'+uname, name="Edit"))
		if net: self.t.write(self.tmpl.format("a_func", href='/networks/close/'+uname, name="Close"))
		else:	self.t.write(self.tmpl.format("a_func", href='/networks/open/'+uname, name="Open"))
		self.t.write(self.tmpl.format("a_func", href='/confirm/?msg='+urllib.quote("Really delete Network %s ?" %name)+'&page=/networks/delete/%s' %uname, name="Delete"))

		self.t.write(templayer.RawHTML("<p />"))

		c = self.t.open_layer("infos2", title="Server", colspan="4")

		#self.wfile.write('<tr><td class="title">#</td><td class="title">Type</td><td class="title">Info</td><td></td>')
		lines = xml.xpathEval('servers/*')
		i = 1
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
				cl = "active"
				de = ""
			else:
				cl = "inactive"
				de = self.tmpl.format("a_func", href='/networks/delserver/%s/%i' %(uname,i), name="Del")
			c.write(self.tmpl.format("row", contents=[
									self.tmpl.format("cell_cl", cl=cl, contents=str(i)),
									self.tmpl.format("cell_cl", cl=cl, contents=s.name),
									self.tmpl.format("cell_cl", cl=cl, contents=hinfo),
									self.tmpl.format("cell_cl", cl=cl, contents=[
										self.tmpl.format("a_func", href='/networks/jumpserver/%s/%i' %(uname,i), name="Jump"),
										self.tmpl.format("a_func", href='/networks/editserver/%s/%i' %(uname,i), name="Edit"),
										de
										])
									]))

			i = i+1
		self.t.write(self.tmpl.format("a_func", href='/networks/reconnect/'+uname, name="Reconnect"))
		self.t.write(self.tmpl.format("a_func", href='/networks/nextserver/'+uname, name="Next Server"))
		self.t.write(self.tmpl.format("a_func", href='/networks/newserver/'+uname, name="New Server"))

		self.t.write(templayer.RawHTML("<p />"))

		c = self.t.open_layer("infos2", title="Listen", colspan="4")

		#self.wfile.write('<tr><td class="title">#</td><td class="title">Type</td><td class="title">Info</td><td></td>')
		lines = xml.xpathEval('listen/*')
		i = 1
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
			cl = "active"
			c.write(self.tmpl.format("row", contents=[
									self.tmpl.format("cell_cl", cl=cl, contents=str(i)),
									self.tmpl.format("cell_cl", cl=cl, contents=s.name),
									self.tmpl.format("cell_cl", cl=cl, contents=hinfo),
									self.tmpl.format("cell_cl", cl=cl, contents=[
										self.tmpl.format("a_func", href='/networks/editlisten/%s/%i' %(uname,i), name="Edit"),
										self.tmpl.format("a_func", href='/networks/dellisten/%s/%i' %(uname,i), name="Del")
										])
									]))

			i = i+1
		self.t.write(self.tmpl.format("a_func", href='/networks/newlisten/'+uname, name="New Listen"))



	def newserver(self, name):
		self._newconnection(name, "server")

	def newlisten(self, name):
		self._newconnection(name, "listen")


	def _newconnection(self, name, typ):
		uname = urllib.quote(name)
		if typ == "server":
			f = self.t.open_layer("form", action="/networks/editserver/%s/0" %uname)
		else:
			f = self.t.open_layer("form", action="/networks/editlisten/%s/0" %uname)
		i = f.open_layer("infos", title="New %s" %typ, colspan="4")
		opt = []
		for v,p in ctrlproxy.list_transports().items():
			mod = ctrlproxy.tools.Moduleinfo(p.name)
			tr = mod.doc.xpathEval('//transports/transport[@name="%s"]' %v)
			if len(tr) and tr[0].prop("only") != None and tr[0].prop("only") != typ:
				pass
			else:
				opt.append(self.tmpl.format("option", value=v, name=v, selected=""))
		#i.open_layer("select", name="transport")
		i.write(self.tmpl.format("pair_row", title="Transport",
				contents=self.tmpl.format("select", name="transport", contents=opt)))
		i.write(self.tmpl.format("pair_row", title="",
				contents=self.tmpl.format("submit", title="Next")))


	def dellisten(self, name, server):
		xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		server = xml.xpathEval("listen/*[%s]" %server)[0]
		server.unlinkNode()
		server.freeNode()
		self.t.write(self.tmpl.format("warning", msg="Changes need restart of network"))
		self.view(name)


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
		self._editconnection(name, server, "server")

	def editlisten(self, name, server):
		self._editconnection(name, server, "listen")

	def _editconnection(self, name, cid, typ):

		xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)

		if typ == "server":
			space = "servers"
		else:
			space = "listen"

		if cid == '0':
			try:
				snode = xml.xpathEval(space)[0]
			except:
				snode = xml.newChild(None,typ,"")
				snode.addPrevSibling(snode.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(snode)))
			print self.handler.post_headers["transport"]
			line = snode.newChild(None,self.handler.post_headers["transport"][0],None)
			print line
			line.addPrevSibling(line.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(line)))

			cid = 1
			for i in xml.xpathEval('%s/*' %space):
				if not i.xpathCmpNodes(line):
					break
				cid += 1
		else:
			try:
				line = xml.xpathEval('%s/*[%s]' %(space,cid))[0]
			except:
				self.wfile.write('<p class="error">Error access transport</p>')
				return
		tl = ctrlproxy.list_transports()
		plugin = ctrlproxy.tools.Moduleinfo(tl[line.name].name)
		trans = plugin.doc.xpathEval("//transports/transport[@name='%s']" %line.name)[0]
		if typ == "server":
			f = self.t.open_layer("form", action="/networks/saveserver/%s/%s" %(uname, cid))
		else:
			f = self.t.open_layer("form", action="/networks/savelisten/%s/%s" %(uname, cid))
		t = f.open_layer("infos", title="Edit %s" %typ, colspan="4")

		# ipv4 and ipv6 can be changed
		if line.name == "ipv4" or line.name == "ipv6":
			opt = []
			for x in ["ipv4", "ipv6"]:
				if x == line.name:
					opt.append(self.tmpl.format("option", value=x, name=x, selected=sel(1)))
				else:
					opt.append(self.tmpl.format("option", value=x, name=x, selected=""))

			t.write(self.tmpl.format("pair_row", title="Transport",
					contents=self.tmpl.format("select", name="transport", contents=opt)))
		elif line:
			t.write(self.tmpl.format("pair_row", title="Transport",
					contents=line.name))
		for i in trans.children:
			if i.name == "attribute" or i.name == "content":
				txt = ""
				value = None

				if i.prop("only") and i.prop("only") != typ:
					continue

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

				r = t.open_layer("pair_row", title=i.prop("name"))

				if i.prop("type") == "bool":
					opt = []
					r.write(self.tmpl.format("select", name=i.prop("name"), contents=[
							self.tmpl.format("option", name="No", value="0", selected=sel(line.prop(i.prop("name")) in (None,"") or line.prop(i.prop("name")) == '0')),
							self.tmpl.format("option", name="Yes", value="1", selected=sel(line.prop(i.prop("name")) not in (None,"") and line.prop(i.prop("name")) == '1'))
							]))
				else:
					r.write(self.tmpl.format("input", name=i.prop("name"), value=value))

		t.write(self.tmpl.format("pair_row", title="",
			contents=self.tmpl.format("submit", title="Save")))

	def saveserver(self, name, server):
		self._saveconnection(name, server, "server")
		self.view(name)

	def savelisten(self, name, server):
		self._saveconnection(name, server, "listen")
		self.view(name)


	def _saveconnection(self, name, server, typ):
		if typ == "server":
			space = "servers"
		else:
			space = "listen"

		try:
			net = ctrlproxy.get_network(name)
			xml = net.xmlConf
		except:
			xml = ctrlproxy.get_config("networks").xpathEval("network[@name='%s']" %name)[0]
		uname = urllib.quote(name)
		try:
			line = xml.xpathEval('%s/*[%s]' %(space,server))[0]
		except:
			self.t.write(self.tmpl.format('info',
				msg='Error accessing transport node'))
			self.list_networks()
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
							nnode.addPrevSibling(nnode.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(nnode)))
					else:
						if len(xnodes):
							xnodes[0].unlinkNode()
							xnodes[0].free()
		self.t.write(self.tmpl.format('info',
			msg='Saved '+name))

	def reconnect(self, name):
		if not name in ctrlproxy.list_networks():
			self.t.write(self.tmpl.format('info',
				msg=name+' is offline'))
			self.list_networks()
		else:
			self.t.write(self.tmpl.format('info',
				msg='Reconnecting '+name))
			net = ctrlproxy.get_network(name)
			net.reconnect()
			self.list_networks()

	def nextserver(self, name):
		if not name in ctrlproxy.list_networks():
			self.t.write(self.tmpl.format('info',
				msg=name+' is offline'))
			self.list_networks()
		else:
			self.t.write(self.tmpl.format('info',
				msg='Cycle server at '+name))
			net = ctrlproxy.get_network(name)
			net.next_server()
			self.list_networks()

	def jumpserver(self, name, server):
		if not name in ctrlproxy.list_networks():
			self.t.write(self.tmpl.format('info',
				msg=name+' is offline'))
			self.list_networks()
		else:
			net = ctrlproxy.get_network(name)
			try:
				net.disconnect()
			except: pass
			print net.xmlConf.xpathEval("//servers/*[%s]" %server)[0]
			net.current_server = net.xmlConf.xpathEval("//servers/*[%s]" %server)[0]
			net.connect()
			self.list_networks()

	def open(self, name):
		if name in ctrlproxy.list_networks():
			self.t.write(self.tmpl.format('info',
				msg=name+' is already online'))
		else:
			self.t.write(self.tmpl.format('info',
				msg='Start '+name))
			ctrlproxy.connect_network(name)
			self.list_networks()

	def close(self, name):
		if name in ctrlproxy.list_networks():
			self.t.write(self.tmpl.format('info',
				msg='Shutdown '+name))
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
			self.t.write(self.tmpl.format('info',
				msg='Cycle server at '+name))
		self.list_networks()

	def index(self):
		self.list_networks()

	def list_networks(self):
		net = ctrlproxy.get_config("networks")

		c = self.t.open_layer("networks")

		for i in net.xpathEval("network"):
			if i.prop("name"):
				a = i.prop("name")
				uname = urllib.quote(a)
				if a in ctrlproxy.list_networks():
					status = "online"
					cl = "online"
					do = "close"
					dotxt = "close"
				else:
					status = "offline"
					cl = "offline"
					do = "open"
					dotxt = "connect"
				c.write(self.tmpl.format("networks_row",
														name=a,
														uname=uname,
														cl=cl,
														status=status,
														do = do,
														dotxt = dotxt))

		#self.t.write(self.tmpl.format("networks", contents=lst))

	def send(self):
		pages = {
		# first url:argnumbers, function
		"editserver":[2,self.editserver],
		"saveserver":[2,self.saveserver],
		"jumpserver":[2,self.jumpserver],
		"delserver":[2,self.delserver],
		"newserver":[1,self.newserver],
		"editlisten":[2,self.editlisten],
		"savelisten":[2,self.savelisten],
		"newlisten":[1,self.newlisten],
		"dellisten":[2,self.dellisten],
		"edit":[1,self.edit],
		"open":[1,self.open],
		"close":[1,self.close],
		"reconnect":[1,self.reconnect],
		"nextserver":[1,self.nextserver],
		"view":[1,self.view],
		"save":[1,self.save],
		"delete":[1,self.delete],
		"new":[0,self.new]
		}

		page.send_default(self, pages)


def init():
	add_menu("networks","Networks",10)
