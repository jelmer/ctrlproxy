###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import thread
import BaseHTTPServer
import urlparse
import os.path
import cgi
import time
import mimetypes
from pages import *
import ctrlproxy
import traceback
import string
import cStringIO
import sys

version = 0.1

print ctrlproxy.args

pages = {}
statics = []

top = os.path.join(os.path.dirname(__file__),"pages")

class SendFile(page):
	"""Serves a File"""
	def __init__(self, handler, name):
		self.name = name
		page.__init__(self, handler)

	def send(self):
		#try:
		fp = file(self.name)
		#except:
		#	print "Webconfig: can't read file : %s" %self.name
		#	return
		self.handler.send_response(200)
		self.handler.send_header("Last-Modified", time.strftime("%a, %d %b %Y %H:%M:%S +0000", time.gmtime(os.path.getmtime(self.name))))
		self.handler.send_header("Content-type", mimetypes.guess_type(self.name))
		self.handler.end_headers()
		buf = fp.read(1024)
		while buf:
			self.wfile.write(buf)
			buf = fp.read(1024)
		fp.close()


class Webconf_css(SendFile):
	"""Sends Stylesheet"""
	def send(self):
		if ctrlproxy.args.has_key("css"):
			name = ctrlproxy.args["css"]
		else:
			name = os.path.join(os.path.dirname(__file__),"webconfig.css")
		self.send_file(name)


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
	server_version = "Ctrlproxy Webconfig Script %s" %version
	def do_GET(self):
		page = None
		print self.headers
		print self.path
		self.close_connection = 1
		self.parsed = urlparse.urlparse("http://localhost%s" %self.path)
		if self.parsed[4]:
			self.args = {}
			map(lambda a: self.args.__setitem__(a[0],a[1]), cgi.parse_qsl(self.parsed[4]))
		else:
			self.args = {}
		self.splited_path = self.parsed[2][1:].split("/")
		name = None
		page = None
		print self.parsed
		print pages

		if self.path == "/":
			name = "index"
			page = "index"
		else:
			# find closest module in the loaded pages object
			for i in range(len(self.splited_path),0,-1):
				if pages.has_key(string.join(self.splited_path[0:i],".")):
					page = string.join(self.splited_path[0:i],".")
					name = self.splited_path[i-1]

		print name
		print page

		# never serve __init__ files
		if name == "__init__":
			self.send_error(404,"File not found")
			return

		if page != None:
			try:
				# load module
				mod = __import__("pages.%s" %page,globals(),locals(),name)
				try:
					if int(ctrlproxy.args["developer"].content):
						reload(mod)
				except: pass
				npage = mod.__dict__[name](self)
				npage.send()
				# free module
				del mod
				del sys.modules["pages.%s" %page]
			except:
				buf = cStringIO.StringIO()
				traceback.print_exc(10,buf)
				self.send_error(500,"Internal error")
				self.wfile.write("<pre>%s</pre>" %buf.getvalue());
				buf.close()
		elif os.path.exists(os.path.join(top,self.path[1:])):
			page = SendFile(self, os.path.join(top,self.path[1:]))
			page.send()
		else:
			self.send_error(404,"File not found")

	def do_POST(self):
		length = int(self.headers.getheader('content-length')) #read data print
		postdata = self.rfile.read(length)
		self.post_headers = cgi.parse_qs(postdata)
		print self.post_headers
		self.do_GET()


def run(server_class=BaseHTTPServer.HTTPServer, handler_class=Handler):
	try:
		server_address = ('', int(ctrlproxy.args["port"].content))
	except:
		server_address = ('', 8888)

	httpd = server_class(server_address, handler_class)
	print "Webconfig listen at port %s" %server_address[1]
	httpd.serve_forever()

# load all scripts in pages that are python scripts

def walk_tree(arg, dirname, names):
	global pages
	imp = dirname[len(top)+1:].replace(os.sep,".")
	if imp:
		imp = "%s." %imp
	for name in names:
		nname = name[:-3]
		if name[-3:] == ".py":
			print "Load: %s%s" %(imp,name[:-3])
			try:
				mod = __import__("pages.%s%s" %(imp,nname),globals(),locals(),nname)
				if "init" in mod.__dict__:
					mod.init()
				# __init__ pages are never freed
				if nname == "__init__":
					pages["%s%s" %(imp,nname)] = mod
				else:
					# reference the static object that it will not be freed when the module is deleted
					if "static" in mod.__dict__:
						statics.append(mod.__dict__["static"])
					pages["%s%s" %(imp,nname)] = True
					del mod
					del sys.modules["pages.%s%s" %(imp,nname)]
			except:
				traceback.print_exc()

os.path.walk(os.path.join(os.path.dirname(__file__),"pages"), walk_tree, None)
print pages

thread.start_new_thread(run, ())
