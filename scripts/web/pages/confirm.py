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
		self.openTemplate()
		self.html_header("Confirm")

		self.t.write(templayer.RawHTML('<form action="%s">' %self.handler.args["page"]))
		self.t.write(self.tmpl.format("warning", msg=self.handler.args["msg"]))
		self.t.write(templayer.RawHTML('<input type="button" onClick="javascript:history.back()" value="Back"> <input type="submit" value="Ok">'))


		self.footer()
