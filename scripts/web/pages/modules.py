###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy
import cgi
import urllib
from __init__ import *
import sys

def simple_transform(msg):
	msg = msg.replace("<file>","<i>")
	msg = msg.replace("</file>","</i>")
	return msg

class modules(page):
	title = "Modules"

	def load(self, name):
		xml = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="lib%s"]' %name)[0]
		try: ctrlproxy.load_module(xml)
		except Exception, e:
			self.t.write(self.tmpl.format("warning", msg=str(e)))
		self.index()

	def unload(self, name):
		ctrlproxy.list_modules()[name].unload()
		self.index()


	def move(self, fr, to):
		try:
			fr = int(fr)
			to = int(to)
		except:
			self.index()
			return

		lst = ctrlproxy.get_config("plugins").xpathEval('plugin')
		if to > len(lst)-1:
			lst[-1].addNextSibling(lst[fr])
		else:
			lst[to].addPrevSibling(lst[fr])
		self.sequence()

	def config(self, name):
		try:
			conf = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="lib%s"]' %name)[0]
		except:
			# create new plugin node
			conf = ctrlproxy.get_config("plugins").newChild(None,"plugin","")
			conf.setProp("autoload", "0")
			conf.setProp("file", "lib%s" %name)

		module = ctrlproxy.tools.Moduleinfo(name)
		f = self.t.open_layer("form", action="/modules/save/%s" %name)
		t = f.open_layer("infos", title="Configure %s" %name, colspan="2")
		t.write(self.tmpl.format("pair_row", title="Autoload",
						contents=self.tmpl.format("toggle", yes="Yes", no="No", name="autoload",
							on=sel(conf.prop("autoload") == "1"),
							off=sel(conf.prop("autoload") in ["0","",None]))))

		def showNode(j, vnode):
			desc = ""
			for de in j.xpathEval("description"):
				desc += de.content
			if desc != "":
				if vnode == None:
					value = ""
				else:
					value = vnode.content
				t2.write(self.tmpl.format("pair_row_desc", title=j.prop("name").capitalize(), description=j.content,
						contents=self._input(j, j.prop("name"), value)))

			for at in j.xpathEval("attribute"):
				if vnode and vnode.prop(at.prop("name")) != None:
					value = vnode.prop(at.prop("name"))
				else:
					value = ""
				t2.write(self.tmpl.format("pair_row_desc", title=at.prop("name").capitalize(), description=at.content,
							contents=self.tmpl.format("input", name="%s_%s" %(i.prop("name"),j.prop("name")), value=value)))

		def showNodes(i, conf):
			if i.prop("multiple") == "1":
				value = conf.xpathEval(i.prop("name"))
				for v in range(len(value)):
					showNode(i, value[v])
					t2.write(self.tmpl.format("empty_row", colspan="3"))
				showNode(i, None)
			else:
				try:
					value = conf.xpathEval(i.prop("name"))[0]
				except:
					value = None
				showNode(i, value)

		for i in module.doc.xpathEval("//configuration/*"):
			if name == "python" and i.prop("name") == "script":
				continue;
			c = t.open_layer("span_row_cl",cl="value", colspan="2")
			t2 = c.open_layer("infos", title=i.prop("name").capitalize(), colspan="2")
			if len(i.xpathEval("element")) == 0:
				showNodes(i, conf)
			else:
				for j in i.xpathEval("element"):
					showNodes(j, conf)

		t.write(self.tmpl.format("empty_row", colspan="2"))
		t.write(self.tmpl.format("span_row_cl", cl="value", colspan="2",
						contents=self.tmpl.format("submit", title="Save")))

		ex = module.doc.xpathEval("//example")
		if len(ex):
			x = self.t.open_layer("example")
			x.write(ex[0].content)


	def save(self, name):
		print self.handler.post_headers
		try:
			conf = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="lib%s"]' %name)[0]
		except:
			# create new plugin node
			conf = ctrlproxy.get_config("plugins").newChild(None,"plugin","")
			conf.setProp("autoload", "0")
			conf.setProp("file", "lib%s" %name)

		if self.handler.post_headers.has_key("autoload"):
			conf.setProp("autoload", self.handler.post_headers["autoload"][0])
		else:
			conf.setProp("autoload", "0")

		module = ctrlproxy.tools.Moduleinfo(name)
		for i in module.doc.xpathEval("//configuration/*"):
			if name == "python" and i.prop("name") == "script":
				continue;
			if len(i.xpathEval("element")) == 0:
				nname = i.prop("name")
				d = conf.xpathEval(nname)
				attrs = []
				for j in i.xpathEval("attribute"):
					attrs.append(j.prop("name"))

				if not self.handler.post_headers.has_key(nname):
					# delete all nodes
					for x in d: x.unlinkNode()
				else:
					# update all nodes and add new one
					for x in range(max(len(d), len(self.handler.post_headers[nname]))):
						if len(self.handler.post_headers[nname]) < x+1:
							d[x].unlinkNode()
						else:
							try:
								d[x].setContent(self.handler.post_headers[nname][x])
							except:
								conf.newChild(None, nname, self.handler.post_headers[nname][x])
			else:
				try:
					par = i.xpathEval(i.prop("name"))[0]
				except:
					par = conf.newChild(None,i.prop("name"), "")
				for j in i.xpathEval("element"):
					try:
						d = conf.xpathEval(par.prop("name"))[0]
						if self.handler.post_headers[j.prop("name")][0] in [None, ""]:
							d.unlink()
						else:
							d.setContent(self.handler.post_headers[j.prop("name")][0])
					except:
						if not self.handler.post_headers.has_key(j.prop("name")):
							conf.newChild(None,j.prop("name"), self.handler.post_headers[j.prop("name")][0])

		self.view(name)

	def infos(self, name):
		module = ctrlproxy.tools.Moduleinfo(name)
		t = self.t.open_layer("infos2", title=name, colspan="2")
		for i in module.doc.xpathEval("//modulemeta/*"):
			t.write(self.tmpl.format("pair_row",
						title=i.name.capitalize(), contents=i.content))

		t.write(self.tmpl.format("empty_row", colspan="2"))

		for i in module.doc.xpathEval("*/description/*"):
			if i.name == "note":
				cl = "note"
			else:
				cl = "description"

			t.write(self.tmpl.format("span_row_cl", cl=cl, colspan="2",
							contents=i.content))
			t.write(self.tmpl.format("empty_row", colspan="2"))

		t.write(self.tmpl.format("span_row_cl", cl="value", colspan="2",
						contents=self.tmpl.format("a", href="/modules/view/%s" %name, name="Back")))

	def view(self, name):
		try:
			module = ctrlproxy.tools.Moduleinfo(name)
		except:
			self.t.write(self.tmpl.format("warning", msg="Can't find infos for "+name))
			return
		t = self.t.open_layer("infos", title=name, colspan="2")
		for i in module.doc.xpathEval("//modulemeta/description"):
			t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1", contents=i.content))

		t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1",
						contents=self.tmpl.format("a", href="/modules/infos/%s" %name, name="Description")))

		t.write(self.tmpl.format("empty_row", colspan="1"))

		t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1",
						contents=self.tmpl.format("a", href="/modules/config/%s" %name, name="Configure")))

		t.write(self.tmpl.format("empty_row", colspan="1"))

		if name in ctrlproxy.list_modules():
			if name in ["python"]:
				t.write(self.tmpl.format("span_row_cl", cl="value", colspan="2",
							contents=self.tmpl.format("warning", msg="Unload the python module disables this web interface")))
			t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1",
							contents=self.tmpl.format("a", href="/confirm?msg=%s&page=/modules/unload/%s" %(urllib.quote("Do you really want to unload module " + name + " ?"),name), name="Unload module")))
		else:
			if len(ctrlproxy.get_config("plugins").xpathEval('plugin[@file="lib%s"]' %name)):
				t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1",
							contents=self.tmpl.format("a", href="/modules/load/%s" %name, name="Load plugin")))
			else:
				t.write(self.tmpl.format("span_row_cl", cl="value", colspan="1",
							contents=self.tmpl.format("info", msg="This Plugin is not configured")))



	def moduleslist(self):
		t = self.t.open_layer("infos", title="Modules", colspan="2")
		lst = ctrlproxy.tools.get_xml_module_names()
		lst.sort()
		for name in lst:
			if name in ctrlproxy.list_modules():
				css = "online"
				txt = "loaded"
			else:
				css = "offline"
				txt = "not loaded"
			r = t.open_layer("row")
			r.write(self.tmpl.format("cell_cl", cl="value",
					contents=self.tmpl.format("a", href="/modules/view/%s" %name, name=name)))
			r.write(self.tmpl.format("cell_cl", cl=css,
					contents=txt))

		t.write(self.tmpl.format("pair_row", title=templayer.RawHTML("&nbsp;"), contents=templayer.RawHTML("&nbsp;")))
		t.write(self.tmpl.format("row", contents=self.tmpl.format("cell_span_cl", colspan="2", cl="value",
				contents=self.tmpl.format("a", href="/modules/sequence", name="Change load sequence"))))

	def sequence(self):
		self.t.write(self.tmpl.format('warning', msg=templayer.RawHTML('Warning, this can break your configuration<br>Start sequence should be: <i>admin</i>,<i>socket</i>,...</p>')))
		t = self.t.open_layer("infos", title="Load squence", colspan="4")

		lst = ctrlproxy.get_config("plugins").xpathEval('plugin')
		for i in range(len(lst)):
			r = t.open_layer("row")
			r.write(self.tmpl.format("cell_cl", cl="value", contents=str(i+1)))
			r.write(self.tmpl.format("cell_cl", cl="value", contents=lst[i].prop("file").replace("lib","")))
			if i > 0:
				r.write(self.tmpl.format("cell_cl", cl="value",
						contents=self.tmpl.format("a", href="/modules/move/%i/%i" %(i,i-1), name="Up")))
			else:
				r.write(self.tmpl.format("cell_cl", cl="value", contents=""))
			if i < len(lst)-1:
				r.write(self.tmpl.format("cell_cl", cl="value",
						contents=self.tmpl.format("a", href="/modules/move/%i/%i" %(i,i+2), name="Down")))
			else:
				r.write(self.tmpl.format("cell_cl", cl="value", contents=""))


	def index(self):
		self.moduleslist()

	def send(self):
		pages = {
		"view":[1,self.view],
		"infos":[1,self.infos],
		"sequence":[1,self.sequence],
		"move":[2,self.move],
		"unload":[1,self.unload],
		"load":[1,self.load],
		"config":[1,self.config],
		"save":[2, self.save]
		#"shutdown":[0,self.shutdown],
		#"viewconfig":[0,self.viewconfig],
		}

		page.send_default(self, pages)

def init():
	add_menu("modules","Modules",15)
