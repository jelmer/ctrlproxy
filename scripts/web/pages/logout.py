###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner                                      
#    <poelzi@poelzi.org>                                                             
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

from __init__ import *

class logout(page):
	title = "Access"
	def send(self):
		if self.handler.headers.has_key("Authorization"):
			self.openTemplate()
			self.handler.send_response(401,"Unauth")
			self.handler.send_header('WWW-authenticate', 'Basic realm=ctrlproxy"%s"' %("/"))
			self.handler.send_header("Content-type", "text/html")
			self.handler.end_headers()
			self.html_header("Access denied")
			self.t.write(self.tmpl.format('h1',msg="Authorization Required"))
			self.t.write(self.tmpl.format('warning',
						msg="""
	Ctrlproxy could not verify that you
	are authorized to access the document
	requested.  Either you supplied the wrong
	credentials (e.g., bad password), or your
	browser doesn't understand how to supply
	the credentials required."""))
			self.footer()
		else:
			self.handler.send_response(307,"Redirect")
			self.handler.send_header('Location', '/')
			self.handler.send_header("Content-type", "text/html")
			self.handler.end_headers()
			return

###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner                                      
#    <poelzi@poelzi.org>                                                             
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

from __init__ import *

class logout(page):
	title = "Access"
	def send(self):
		if self.handler.headers.has_key("Authorization"):
			self.openTemplate()
			self.handler.send_response(401,"Unauth")
			self.handler.send_header('WWW-authenticate', 'Basic realm=ctrlproxy"%s"' %("/"))
			self.handler.send_header("Content-type", "text/html")
			self.handler.end_headers()
			self.html_header("Access denied")
			self.t.write(self.tmpl.format('h1',msg="Authorization Required"))
			self.t.write(self.tmpl.format('warning',
						msg="""
	Ctrlproxy could not verify that you
	are authorized to access the document
	requested.  Either you supplied the wrong
	credentials (e.g., bad password), or your
	browser doesn't understand how to supply
	the credentials required."""))
			self.footer()
		else:
			self.handler.send_response(307,"Redirect")
			self.handler.send_header('Location', '/')
			self.handler.send_header("Content-type", "text/html")
			self.handler.end_headers()
			return

