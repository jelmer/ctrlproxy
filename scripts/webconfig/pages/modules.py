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

def simple_transform(msg):
	msg = msg.replace("<file>","<i>")
	msg = msg.replace("</file>","</i>")
	return msg

class modules(page):
	title = "Modules"

	def load(self, name):
		pass

	def unload(self, name):
		pass


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

	def infos(self, name):
		module = ctrlproxy.tools.Moduleinfo(name)
		self.wfile.write('<table class="infos2"><tr><th class="topic" colspan="2"><a href="/modules/view/%s">%s</a></th></tr>' %(module.name,module.name))
		for i in module.doc.xpathEval("//modulemeta/*"):
			self.wfile.write('<tr><td class="title">%s</td><td class="value"><i>%s</i></td></tr>' %(i.name.capitalize(), i.content))

		self.wfile.write('<tr><td colspan="2">&nbsp;</td></tr>')
		for i in module.doc.xpathEval("*/description/*"):
			if i.name == "note":
				self.wfile.write('<tr><td colspan="2" class="note">%s</td></tr>' %(i.content))
			else:
				self.wfile.write('<tr><td colspan="2" class="value">%s</td></tr>' %(i.content))
			self.wfile.write('<tr><td colspan="2">&nbsp;</td></tr>')

		self.wfile.write("</table>")

	def view(self, name):
		module = ctrlproxy.tools.Moduleinfo(name)
		self.wfile.write('<table class="infos2"><tr><th class="topic">%s</th></tr>' %module.name)
		for i in module.doc.xpathEval("//modulemeta/description"):
			self.wfile.write('<tr><td class="value"><i>%s</i></td></tr>' %i.content)

		self.wfile.write('<tr><td class="value">&nbsp;</td></tr>')
		self.wfile.write('<tr><td class="value"><a href="/modules/infos/%s">Description</a></td></tr>' %name)
		self.wfile.write('<tr><td class="value">&nbsp;</td></tr>')

		self.wfile.write('<tr><td class="value">&nbsp;</td></tr>')

		if name in ctrlproxy.list_modules():
			if name in ["python", "socket"]:
				self.wfile.write('<tr><td class="value">The %s plugin should not be unloaded</td></tr>' %(name))
			else:
				self.wfile.write('<tr><td class="value"><a href="/confirm?msg=%s&page=/modules/unload/%s">Unload plugin</a></td></tr>' %(urllib.quote("Do you really want to unload module " + name + " ?"),name))
		else:
			if len(ctrlproxy.get_config("plugins").xpathEval('plugin[@file="lib%s"]' %name)):
				self.wfile.write('<tr><td class="value"><a href="/modules/load/%s">Load plugin</a></td></tr>' %name)
			else:
				self.wfile.write('<tr><td class="value">This Plugin is not configured</td></tr>')


		self.wfile.write("</table>")


	def moduleslist(self):
		self.wfile.write('<table class="infos">')
		self.wfile.write('<tr><th class="topic" colspan="2">Modules</th></tr>')
		lst = ctrlproxy.tools.get_xml_module_names()
		lst.sort()
		for name in lst:
			if name in ctrlproxy.list_modules():
				css = "online"
				txt = "loaded"
			else:
				css = "offline"
				txt = "not loaded"
			self.wfile.write('<tr><td><a href="/modules/view/%s">%s</td><td class="%s">%s</td></tr>' %(name,name,css,txt))
		self.wfile.write('<tr><td colspan="2">&nbsp;</td></tr>')
		self.wfile.write('<tr><td colspan="2"><a href="/modules/sequence">Change load sequence</a></td></tr>')
		self.wfile.write("</table>")

	def sequence(self):
		self.wfile.write('<table class="infos">')
		self.wfile.write('<p class="warning">Warning, this can break your configuration<br>Start sequence should be: <i>admin</i>,<i>socket</i>,...</p>')
		self.wfile.write('<tr><th class="topic" colspan="4">Load sequence</th></tr>')

		lst = ctrlproxy.get_config("plugins").xpathEval('plugin')
		for i in range(len(lst)):
			self.wfile.write('<tr><td>%i</td><td>%s</td>' %(i+1,lst[i].prop("file").replace("lib","")))
			if i > 0:
				self.wfile.write('<td><a href="/modules/move/%i/%i">Up</a></td>' %(i,i-1))
			else:
				self.wfile.write('<td></td>')
			if i < len(lst)-1:
				self.wfile.write('<td><a href="/modules/move/%i/%i">Down</a></td>' %(i,i+2))
			else:
				self.wfile.write('<td></td>')
		self.wfile.write("</table>")


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
		#"shutdown":[0,self.shutdown],
		#"viewconfig":[0,self.viewconfig],
		}

		page.send_default(self, pages)

def init():
	add_menu("modules","Modules",15)
