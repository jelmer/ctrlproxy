###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy

from __init__ import *

class downloadconfig(page):
	"""Sends Stylesheet"""
	def send(self):
		self.handler.send_response(200)
		self.handler.send_header("Content-type", "text/xml")
		self.handler.end_headers()

		self.wfile.write(ctrlproxy.get_config("").serialize())
