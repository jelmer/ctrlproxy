###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy
import cgi
from __init__ import *

class tools(page):
	title = "Tools"

	def save(self):
		ctrlproxy.save_config()
		self.wfile.write("""<p class="info">Saved</p>""")

	def viewconfig(self):
		node = ctrlproxy.get_config("")
		self.wfile.write("""<pre>""")
		self.wfile.write(cgi.escape(node.serialize()))
		self.wfile.write("""</pre>""")

	def shutdown(self):
		ctrlproxy.exit(0)


	def index(self):
		self.wfile.write("""
		<a href="/tools/save">Save configuration</a><br />
		<a href="/tools/viewconfig">View configuration</a><br />
		<a href="/downloadconfig">Download configuration</a><p />
		<a href="/tools/shutdown">Shutdown Ctrlproxy</a><br />""")

	def send(self):
		pages = {
		"save":[0,self.save],
		"shutdown":[0,self.shutdown],
		"viewconfig":[0,self.viewconfig],
		}

		page.send_default(self, pages)

def init():
	add_menu("tools","Tools",20)
