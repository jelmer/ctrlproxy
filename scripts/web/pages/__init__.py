###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import includes.templayer as templayer
import os.path

version = 0.4

menu = []

def add_menu(site, name, priority):
	i = 0
	if (site,name,priority) in menu:
		return
	for (s,n,p) in menu:
		if p > priority:
			menu.insert(i,(site,name,priority))
			return
		i += 1
	menu.append((site,name,priority))

def nstr(value):
		if value != None:
			return str(value)
		return ""

def selectedSwitch(var):
	if var == '1' or var == 1:
		return ' selected="1" '
	return ""

def sel(test):
	if test:
		return templayer.RawHTML('selected="selected"')
	return ""

class page:
	def __init__(self, handler):
		self.handler = handler
		self.wfile = handler.wfile

	def html_header(self, title = None):
		global menu

		links = []
		try:	m = self.menu
		except:	m = menu
			
		for (p,n,pi) in m:
			links.append(self.tmpl.format("menu_links", link=p, name=n))

		if title == None:
			self.t = self.tmplf.open( title=self.title, links=links)
		else:
			self.t = self.tmplf.open( title=title, links=links)

	def html_footer(self):
		pass;

	def index(self):
		pass

	def header(self, ctype = "text/html"):
		self.handler.send_response(200)
		self.handler.send_header("Content-type", ctype)
		self.handler.end_headers()

	def openTemplate(self, name = "includes/default.tmpl"):
		self.tmpl = templayer.HtmlTemplate(os.path.join(os.path.dirname(__file__),"..","includes","default.tmpl"))
		self.tmplf = self.tmpl.start_file(self.wfile)

	def footer(self):
		self.tmplf.close()

	def send_default(self, pages):
		self.header()
		self.openTemplate()
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

		self.footer()

	def _input(self, dscnode, name, value):
		if dscnode.prop("type") == "bool":
			return self.tmpl.format("toggle", yes="Yes", no="No", name=name,
						on=sel(value == "1"),
						off=sel(value in ["0","",None]))
		return self.tmpl.format("input", name=name, value=value)

