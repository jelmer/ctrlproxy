###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

version = 0.1

menu = []

def add_menu(site, name, priority):
	i = 0
	for (s,n,p) in menu:
		if p > priority:
			menu.insert(i,(site,name,priority))
			return
		i += 1
	menu.append((site,name,priority))

def selectedSwitch(var):
	if var == '1' or var == 1:
		return ' selected="1" '
	return ""

class page:
	def __init__(self, handler):
		self.handler = handler
		self.wfile = handler.wfile

	def html_header(self, title):
		self.wfile.write("""
		<?xml version="1.0" encoding="iso-8859-1"?>
		<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
		<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
		<html>
			<header>
			<title>""" + title + """</title>
			<link rel="stylesheet" type="text/css" media="screen" href="/default.css" />
			</header>
			<body>
			<table class="head">
				<tr>
					<td><p class="title">Ctrlproxy webconfig</p></td>
					<td><p class="mirror"><a href="http://ctrlproxy.vernstok.nl/">homepage</a></p></td>
				</tr>
			</table>
			<table class="main">
				<tr><td><table class="menu">""")
		for (p,n,pi) in menu:
			self.wfile.write('<tr><td><a class="menu" href="/%s">%s</a></td></tr>\n' %(p,n))
		self.wfile.write("""
		</table></td><td class="content">
		""")

	def html_footer(self):
		self.wfile.write("""
		</td></tr>
		</table>
		</body></html>
		""")

	def index(self):
		pass

	def header(self, ctype = "text/html"):
		self.handler.send_response(200)
		self.handler.send_header("Content-type", ctype)
		self.handler.end_headers()

	def send_default(self, pages):
		self.header()
		self.html_header(self.title)

		if len(self.handler.splited_path) == 1:
			name = self.handler.splited_path[0]
		else:
			name = self.handler.splited_path[1]
		if pages.has_key(name):
			if pages[name][0]+1 <= len(self.handler.splited_path):
				pages[name][1](*self.handler.splited_path[2:pages[name][0]+2])
			else:
				self.index()
		else:
			self.index()
		self.html_footer()


