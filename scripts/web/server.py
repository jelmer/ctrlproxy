###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner
#    <poelzi@poelzi.org>
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

"""

Webserver script


This script extends ctrlproxy with a webserver that can serve files
and run specific python scripts.



"""


"""CTRLPROXY CONFIG

<configuration>
	<element name="port" type="int">
		<description>Port to listen for connections. Default is 8888</description>
	</element>

	<element name="users">
		<description>Controls Webserver access</description>
		<element name="user" multiple="1" type="empty">
			<attribute name="name">
				<description>Username for login</description>
			</attribute>
			<attribute name="pass" type="pwd">
				<description>Password</description>
			</attribute>
		</element>
	</element>
	
	<element name="allowed">
		<description>This option controlls which pages will be served. When this element is empty, all files excluding everything from "denied" will be served. CSS files and logout.py are always allowed.</description>
		<element name="file" multiple="1" type="text">
			<description>file/page to allow (ex: modules; test.ext)</description>
		</element>
	</element>
	
	<element name="denied">
		<description>This option controlls which pages will never be served. When this element is empty.</description>
		<element name="file" multiple="1" type="text">
				<description>file/page to deny (ex: modules; test.ext)</description>
		</element>
	</element>
	
	
	<element name="developer" type="bool">
		<description>This option enables developer functions</description>
	</element>
	
</configuration>

CTRLPROXY CONFIG"""


import thread
import BaseHTTPServer
import urlparse
import os.path
import cgi
import time
import mimetypes
import ctrlproxy
import traceback
import string
import cStringIO
import sys
import SocketServer
from pages import *
from pages.__init__ import menu
import base64
import libxml2

version = 0.1

ctrl_args = ctrlproxy.get_args()

print ctrl_args

pages = {}
statics = []

server = None

sys.path.append(os.path.join(os.path.dirname(__file__),"includes"))

top = os.path.join(os.path.dirname(__file__),"pages")

run_script = 1

class ThreadHTTPServer(SocketServer.ThreadingTCPServer, BaseHTTPServer.HTTPServer):
    def serve_forever(self):
        """Handle one request at a time until doomsday."""
        while run_script:
            self.handle_request()

			
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
		if self.handler.headers.has_key("If-Modified-Since"):
			try:
				if int(time.mktime(time.strptime(self.handler.headers["If-Modified-Since"], "%a, %d %b %Y %H:%M:%S +0000" ))) == int(os.path.getmtime(self.name) + time.timezone):
					self.handler.send_response(304)
					return
			except: pass
		self.handler.send_response(200)
		self.handler.send_header("Last-Modified", time.strftime("%a, %d %b %Y %H:%M:%S +0000", time.gmtime(os.path.getmtime(self.name))))
		self.handler.send_header("Content-type", mimetypes.guess_type(self.name))
		self.handler.end_headers()
		buf = fp.read(1024)
		while buf:
			self.wfile.write(buf)
			buf = fp.read(1024)
		fp.close()

class AccessDenied(page):
	def send(self):
		self.openTemplate()
		self.t = self.tmplf.open(title="Access Denied", links="")
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

class PageDenied(page):
	title = "Access denied"
	menu = menu
	def index(self):
		self.t.write(self.tmpl.format('warning',
					msg="""Access to this page denied"""))
		
	def send(self):
		self.send_default({})

		
class Webconf_css(SendFile):
	"""Sends Stylesheet"""
	def send(self):
		if ctrl_args.has_key("css"):
			name = ctrl_args["css"]
		else:
			name = os.path.join(os.path.dirname(__file__),"webconfig.css")
		self.send_file(name)


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
	server_version = "Ctrlproxy Webconfig Script %s" %version

	def auth_required(self):
		self.send_response(401,"Unauth")
		self.send_header('WWW-authenticate', 'Basic realm=ctrlproxy"%s"' %("/"))
		self.send_header("Content-type", "text/html")
		self.end_headers()
		npage = AccessDenied(self)
		npage.send()
	
	def access_denied(self):
		npage = PageDenied(self)
		npage.send()

	def do_GET(self):
		print __doc__
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
		fpage = None
		print self.parsed
		print pages

		# support for user authentication, skip for css files
		if (self.splited_path[0][-4:] != ".css" and self.splited_path[0] != "logout" and
			ctrl_args.has_key("users") and len(ctrl_args["users"].xpathEval('user'))):
			if self.headers.has_key("Authorization") and ctrl_args.has_key("users"):
				key = self.headers["Authorization"][self.headers["Authorization"].index(" "):]
				key = base64.decodestring(key)
				user = key[:key.index(":")]
				pwd =  key[key.index(":")+1:]
				try:
					test = ctrl_args["users"].xpathEval('user[@name="%s"]' %user)[0]
					if test.prop("pass") != pwd:
						self.auth_required()
				except:
					self.auth_required()

			elif ctrl_args.has_key("users"):
				self.auth_required()
				return
		
		if self.path == "/":
			name = "index"
			page = "index"
		else:
			# find closest module in the loaded pages object
			for i in range(len(self.splited_path),0,-1):
				if pages.has_key(string.join(self.splited_path[0:i],".")):
					page = string.join(self.splited_path[0:i],".")
					fpage =  string.join(self.splited_path[0:i],"/")
					name = self.splited_path[i-1]
		if (self.splited_path[0][-4:] != ".css" and self.splited_path[0] != "logout"):
			if ctrl_args.has_key("allowed"):
				if len(ctrl_args["allowed"].xpathEval("file")):
					for tst in ctrl_args["allowed"].xpathEval("file/text()"):
						if tst.content == fpage:
							break
					else:
						return self.access_denied()
	
			if ctrl_args.has_key("denied"):
				if len(ctrl_args["denied"].xpathEval("file")):
					for tst in ctrl_args["denied"].xpathEval("file/text()"):
						if tst.content == fpage:
							return self.access_denied()

					
		# never serve __init__ files
		if name == "__init__":
			self.send_error(404,"File not found")
			return

		if page != None:
			try:
				# load module
				mod = __import__("pages.%s" %page,globals(),locals(),name)
				try:
					if int(ctrl_args["developer"].content):
						try: reload(mod)
						except Exception, e: print e
				except: pass
				npage = mod.__dict__[name](self)
				if npage == None:
					raise RuntimeError, "Internal error"
				npage.send()
				# free module
				del mod
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

	def log_request(self, code='-', size='-'):
		try:
			self.log_message('"%s" %s %s',
							 self.requestline, str(code), str(size))
		except:
			self.log_error("Error")

	def log_message(self, format, *args):
		sys.stdout.write("Web: %s - - [%s] %s\n" %
						 (self.address_string(),
						  self.log_date_time_string(),
						  format%args))


#def run(server_class=ThreadHTTPServer, handler_class=Handler):
def run(server_class=BaseHTTPServer.HTTPServer, handler_class=Handler):
	global server
	print "Starting up"
	print ctrl_args["port"]
	print ctrl_args["port"].content
	try:
	#print ctrl_args["port"].content
		server_address = ('', int(ctrl_args["port"].content))
	#	print "done"
	except:
		server_address = ('', 8889)
	print "HOHO"
	while 1:
		print "try"
		try:
			server = server_class(server_address, handler_class)
			print "Webserver listen at port %s" %server_address[1]
			server.serve_forever()
			break
		except:
			print "Can't start Webserver retry in 1 Minute"
			time.sleep(60)


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

def unload_script():
	print "UNLOAD WEBSERVER"
	server.socket.shutdown(2)
	server.server_close()

ctrlproxy.at_exit(unload_script)

thread.start_new_thread(run, ())
