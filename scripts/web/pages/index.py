###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

from __init__ import *
import ctrlproxy


class index(page):
	title = "Ctrlproxy"

	def send(self):
		self.header()
		self.openTemplate()
		self.html_header("Ctrlproxy configuration")
		self.t.write(self.tmpl.format('info',
					msg="""Welcome to ctrlproxy webconfig"""))
		self.footer()

def init():
	add_menu("","Start",0)
