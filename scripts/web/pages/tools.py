###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy
from __init__ import *

class tools(page):
	title = "Tools"

	def save(self):
		ctrlproxy.save_config()
		self.t.write(self.tmpl.format("info", msg="Saved"))
		self.index()

	def viewconfig(self):
		node = ctrlproxy.get_config("")
		x = self.t.open_layer("example")
		x.write(node.serialize())

	def shutdown(self):
		ctrlproxy.exit(0)


	def index(self):

		self.t.write(self.tmpl.format("a_line", href="/tools/save", name="Save configuration"))
		self.t.write(self.tmpl.format("a_line", href="/tools/viewconfig", name="View configuration"))
		self.t.write(self.tmpl.format("a_line", href="/downloadconfig", name="Download configuration"))
		self.t.write(self.tmpl.format("br"))
		self.t.write(self.tmpl.format("a_line", href="/tools/shutdown", name="Shutdown ctrlproxy"))

	def send(self):
		pages = {
		"save":[0,self.save],
		"shutdown":[0,self.shutdown],
		"viewconfig":[0,self.viewconfig],
		}

		page.send_default(self, pages)

def init():
	add_menu("tools","Tools",20)
