###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

from __init__ import *

class confirm(page):
	def send(self):
		self.header()
		self.html_header("Confirm")
		self.wfile.write('<form action="%s">' %self.handler.args["page"])
		self.wfile.write('<p class="warning">%s</p>' %self.handler.args["msg"])
		self.wfile.write('<input type="button" onClick="javascript:history.back()" value="Back"> <input type="submit" value="Ok"></form>')

		self.html_footer()
