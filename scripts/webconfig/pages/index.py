###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

from __init__ import *


class index(page):
	def send(self):
		self.header()
		self.html_header("Ctrlproxy configuration")
		self.wfile.write("""Welcome to ctrlproxy webconfig""")
		self.html_footer()

def init():
	add_menu("","Start",0)
