###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy
import urllib
import binascii
import os
import os.path
import libxml2
from __init__ import *

class scripts(page):
	title = "Scripts"

	def _getdoc(self, cid):
		if cid[0] == "o":
			for i in ctrlproxy.list_scripts():
				if i.id == int(cid[1:]):
					script = i
					try: x = i.xmlConf["__config__"]
					except: x = None
					try: d = ctrlproxy.tools.Scriptinfo(i.path)
					except: d = None
					return (i, x, d)
		else:
			lst = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="libpython"]')[0].xpathEval('script')
			try:	s = ctrlproxy.tools.Scriptinfo(lst[int(cid)-1].xpathEval('file')[0].content)
			except: s = None
			return (None, lst[int(cid)-1], s)

		return (None, None, None)


	def index(self):
		llst = ctrlproxy.list_scripts()
		lst = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="libpython"]')[0].xpathEval('script')
		t = self.t.open_layer("infos", title="Scripts", colspan="4")
		n = 1
		def run(lst, n):
			for i in lst:
				cl = "offline"
				txt = "not loaded"
				cid = "%s" %n
				path = ""
				f = self.tmpl.format("a_func", href="/scripts/load/%s" %cid, name="load")
				for j in llst:
					if j.xmlConf.has_key("__config__") and not i.xpathCmpNodes(j.xmlConf["__config__"]):
						cl = "online"
						txt = "loaded"
						cid = "o%s" %j.id
						path = j.path
						f = self.tmpl.format("a_func", href="/scripts/unload/%s" %cid, name="unload")
						del llst[llst.index(j)]
						break
				if not path:
					path = i.xpathEval("file")[0].content

				t.write(self.tmpl.format("row", contents=[
					self.tmpl.format("cell_cl", contents=str(n), cl=cl),
					self.tmpl.format("cell", contents=path),
					self.tmpl.format("cell_cl", contents=txt, cl=cl),
					self.tmpl.format("cell", contents=[
						self.tmpl.format("a_func", href="/scripts/info/%s" %cid, name="view"),
						self.tmpl.format("a_func", href="/scripts/config/%s" %cid, name="config"),
						self.tmpl.format("a_func", href="/confirm?page=/scripts/delete/%s&msg=%s" %(cid,urllib.quote("Do you really wan't to delete this script ?")), name="delete"),
						f
						])]
					))
				n += 1
		run(lst, n)
		run(llst, n)

		self.t.write(self.tmpl.format("p"))
		self.t.write(self.tmpl.format("a_func", href="/scripts/listnew/", name="Load"))

	def delete(self, cid):
		(script, xml, cdoc) = self._getdoc(cid)
		if not xml:
			self.index()
			return
		if script:
			script.unload()
		xml.unlinkNode()
		xml.freeNode()

		self.index()

	def saveconfig(self, cid, name):
		(script, xml, cdoc) = self._getdoc(cid)
		
		if name == "autoload":
			if xml:
				xml.setProp("autoload", self.handler.post_headers["autoload"][0])
		else:
			lst = {}
			for (n, v) in self.handler.post_headers.items():
				lst[n] = v[0]

			for i in cdoc.config.xpathEval("element"):
				if i.prop("name") == name:
					cdoc.list2config(lst, xml.xpathEval('argument[@name="%s"]' %name)[0], start=i)

		self.config(cid)


	def configitem(self, cid, name):
		(script, xml, cdoc) = self._getdoc(cid)
		if not cdoc:
			self.index()
			return
		if script:
			if script.xmlConf.has_key(name):
				node = script.xmlConf[name]
				# make sure its liked into the config
				if script.xmlConf.has_key("__config__"):
					script.xmlConf["__config__"].addChild(node)
			elif script.xmlConf.has_key("__config__"):
				node = script.xmlConf["__config__"].newChild(None,"argument","")
				node.setProp("name", name)
				node.addPrevSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node.parent)))
				script.xmlConf[name] = node
			else:
				node = libxml2.newNode(name)
				script.xmlConf[name] = node
		elif xml:
			try:
				node = xml.xpathEval('argument[@name="%s"]' %name)[0]
			except:
				node = xml.newChild(None, "argument", "")
				node.setProp("name", name)
		
		else:
			self.t.write(self.tmpl.format("error", msg="Error, no access to configuration"))

		print node.serialize()
		f = self.t.open_layer("form", action="/scripts/saveconfig/%s/%s" %(cid, name))
		t = f.open_layer("infos2", title="Config %s" %name, colspan="3")

		for (nname, title, value, flags, doc) in cdoc.config2list(node):
			print nname[:len("script__%s" %name)]
			if nname[:len("script__%s" %name)] == "script__%s" %name:
				print nname
				print value
				if flags & cdoc.TYPE_EMPTY:
					#t.write(self.tmpl.format("hidden", name=nname, value=" "))
					pass
				elif flags & cdoc.SEPERATOR:
					t.write(self.tmpl.format("empty_row", colspan="3"))
				else:
					l = t.open_layer("pair_row_desc", title=title.capitalize(), description=doc)
					if flags & cdoc.TYPE_BOOL:
						l.write(self.tmpl.format("toggle", name=nname, yes="Yes", no="No",
														on = sel(value == "1"),
														off = sel(value in ["0", "", None])))
					elif flags & cdoc.TYPE_PWD:
						l.write(self.tmpl.format("password", name=nname, value=value))
					else:
						l.write(self.tmpl.format("input", name=nname, value=value))

		t.write(self.tmpl.format("empty_row", colspan="2"))
		t.write(self.tmpl.format("span_row_cl", cl="value", colspan="2",
						contents=self.tmpl.format("submit", title="Save")))


	def config(self, cid):
		(script, xml,  doc) = self._getdoc(cid)
		if not doc:
			self.t.write(self.tmpl.format("warning", msg="No configuration available"))			

		if script and not xml:
			self.t.write(self.tmpl.format("warning", msg="Script is not present in the config, changes are not saved nor may they have affects"))

		t = self.t.open_layer("infos2", title="Config script", colspan="2")

		if xml:
			value = xml.prop("autoload")
			t.write(self.tmpl.format("pair_row", title="Autoload",
				contents=self.tmpl.format("form", action="/scripts/saveconfig/%s/autoload" %cid,
					contents=[	self.tmpl.format("toggle", name="autoload", yes="Yes", no="No",
													on = sel(value == "1"),
													off = sel(value in ["0", "", None])),
						 		self.tmpl.format("submit", title="Save")])))

		if doc:
			for i in doc.config.xpathEval("*"):
				try:	cd=i.xpathEval("description")[0].content
				except:	cd = ""
				t.write(self.tmpl.format("pair_row",
					title=self.tmpl.format("a", href="/scripts/configitem/%s/%s" %(cid,urllib.quote(i.prop("name"))), name=i.prop("name")),
					contents=self.tmpl.format("description",contents=cd)))



	def info(self, cid):
		(script, xml, doc) = self._getdoc(cid)
		if not doc:
			self.index()
			return

		t = self.t.open_layer("infos2", title="Script info", colspan="1")
		e = self.t.open_layer("documentation")

		e.write(doc.doc)

	def load(self, cid):
		(script, xml, doc) = self._getdoc(cid)
		
		if xml:
			path = xml.xpathEval("file")[0].content
			print xml.serialize()
			print path
			ctrlproxy.load_script(path, xml)
		
		self.index()

		
		
	def unload(self, cid):
		(script, xml, doc) = self._getdoc(cid)
		
		if not script:
			return self.index()
			
		script.unload()
		
		self.index()
	
		
		
	def add(self, bpath):
		if bpath == "0":
			if not self.handler.has_key("path") or not len(self.handler.post_headers["path"]):
				self.t.write(self.tmpl.format("error", msg="Path is required"))
				self.index()
				return

			path = self.handler.post_headers["path"][0]
		else:
			path = binascii.a2b_hex(bpath)

		print path
		doc = ctrlproxy.tools.Scriptinfo(path)

		p = ctrlproxy.get_config("plugins").xpathEval('plugin[@file="libpython"]')[0]
		node = p.newChild(None,"script",None)
		node.addPrevSibling(node.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node.parent)))
		node.setProp("autoload", "0")
		f = node.newChild(None, "file", path)
		f.addPrevSibling(f.doc.newDocText("\n" + ctrlproxy.tools.mkspaces(node)))

		c = 0
		for i in p.xpathEval("script"):
			c += 1
		print c
		self.config(str(c))



	def listnew(self):
		def wlk(path, small=0):
			for root, dirs, files in os.walk(path):
				if 'CVS' in dirs:
					dirs.remove('CVS')  # don't visit CVS directories
				for name in files:
					if name[-3:] == ".py":
						try:
							doc = ctrlproxy.tools.Scriptinfo(os.path.join(root, name))
							if doc.config:
								if small:	p = os.path.join(root, name)[len(path)+1:]
								else:		p = os.path.join(root, name)
								t.write(self.tmpl.format("span_row_cl", colspan="1", cl="value", contents=
									self.tmpl.format("a", href="/scripts/add/%s" %urllib.quote(binascii.b2a_hex(p)),
										name=os.path.join(root, name)[len(path)+1:])))
						except Exception, e:
							print e
							pass

		t = self.t.open_layer("infos2", title="Home", colspan="1")
		wlk(os.path.expanduser("~/.ctrlproxy"))

		self.t.write(self.tmpl.format("p"))

		t = self.t.open_layer("infos2", title="Library", colspan="1")
		wlk(os.path.expanduser(os.path.join(ctrlproxy.get_path("share"),"scripts")),1)

		self.t.write(self.tmpl.format("p"))
		f = self.t.open_layer("form", action="/scripts/load/0")
		f.write(self.tmpl.format("info", msg="Custom path:"))
		f.write(self.tmpl.format("input", name="path", value=""))
		f.write(self.tmpl.format("submit", title="Next"))


	def send(self):
		reload(ctrlproxy.tools)
		pages = {
		# first url:argnumbers, function
		"listnew":[0,self.listnew],
		"add":[1,self.add],
		"config":[1,self.config],
		"configitem":[2,self.configitem],
		"saveconfig":[2,self.saveconfig],
		"info":[1,self.info],
		"load":[1, self.load],
		"unload":[1, self.unload],
		"delete":[1, self.delete]
		}

		page.send_default(self, pages)


def init():
	add_menu("scripts","Scripts",15)
