# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
#
# Daniel Poelzleithner <ctrlproxy@poelzi.org>
#

# some functions are taken from python-irclib-0.4.1

"""
This functions are accessable over

ctrlproxy.tools
"""

def ip_numstr_to_quad(num):
    """Convert an IP number as an integer given in ASCII
    representation (e.g. '3232235521') to an IP address string
    (e.g. '192.168.0.1')."""
    n = long(num)
    p = map(str, map(int, [n >> 24 & 0xFF, n >> 16 & 0xFF,
                           n >> 8 & 0xFF, n & 0xFF]))
    return string.join(p, ".")

def ip_quad_to_numstr(quad):
    """Convert an IP address string (e.g. '192.168.0.1') to an IP
    number as an integer given in ASCII representation
    (e.g. '3232235521')."""
    p = map(long, string.split(quad, "."))
    s = str((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3])
    if s[-1] == "L":
        s = s[:-1]
    return s

def nm_to_n(s):
    """Get the nick part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "!")[0]

def nm_to_uh(s):
    """Get the userhost part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "!")[1]

def nm_to_h(s):
    """Get the host part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "@")[1]

def nm_to_u(s):
    """Get the user part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    s = string.split(s, "!")[1]
    return string.split(s, "@")[0]

def parse_nick_modes(mode_string):
    """Parse a nick mode string.

    The function returns a list of lists with three members: sign,
    mode and argument.  The sign is \"+\" or \"-\".  The argument is
    always None.

    Example:

    >>> ctrlproxy.tools.parse_nick_modes(\"+ab-c\")
    [['+', 'a', None], ['+', 'b', None], ['-', 'c', None]]
    """

    return _parse_modes(mode_string, "")

def parse_channel_modes(mode_string):
    """Parse a channel mode string.

    The function returns a list of lists with three members: sign,
    mode and argument.  The sign is \"+\" or \"-\".  The argument is
    None if mode isn't one of \"b\", \"k\", \"l\", \"v\" or \"o\".

    Example:

    >>> ctrlproxy.tools.parse_channel_modes(\"+ab-c foo\")
    [['+', 'a', None], ['+', 'b', 'foo'], ['-', 'c', None]]
    """

    return _parse_modes(mode_string, "bklvo")

def _parse_modes(mode_string, unary_modes=""):
    """[Internal]"""
    modes = []
    arg_count = 0

    # State variable.
    sign = ""

    a = string.split(mode_string)
    if len(a) == 0:
        return []
    else:
        mode_part, args = a[0], a[1:]

    if mode_part[0] not in "+-":
        return []
    for ch in mode_part:
        if ch in "+-":
            sign = ch
        elif ch == " ":
            collecting_arguments = 1
        elif ch in unary_modes:
            if len(args) >= arg_count + 1:
                modes.append([sign, ch, args[arg_count]])
                arg_count = arg_count + 1
            else:
                modes.append([sign, ch, None])
        else:
            modes.append([sign, ch, None])
    return modes


def mkspaces(node):
	"""Generates a string for the configuration.

	This function creates a string that does not break
	the right layout of the configure when you create
	new nodes.

	>>> node.addPrevSibling(node.doc.newDocText(ctrlproxy.tools.mkspaces(node)))
	"""
	c = 0
	try:
		f = node.parent
		while f:
			c += 1
			f = f.parent
		if c:
			return "%s" %("\t" * c)
		return ""
	except:
		return ""


import ctrlproxy
import os
import os.path
import string
import libxml2

def get_xml_module_names(typ = None):
	"""Returns a list of modules for which a Moduleinfo is available"""
	path = os.path.join(ctrlproxy.get_path("share"),"ctrlproxy","help")
	dl = os.listdir(path)
	rv = []
	for i in dl:
		if i[-8:] == ".mod.xml":
			if typ != None:
				d = libxml2.parseFile(os.path.join(path, i))
				#d.dump(1)
				xl = d.xpathEval("//modulemeta/type")
				for node in xl:
					if node.content == typ:
						rv.append(i[:-8])
				d.freeDoc()
			else:
				rv.append(i[:-8])
	return rv


class Info:
	"""internal config parser

	This class provides functions to parse configuration informations
	"""
	TYPE_TEXT = 1 << 1
	TYPE_BOOL = 1 << 2
	TYPE_INT  = 1 << 3
	TYPE_LIST = 1 << 4
	TYPE_PWD  = 1 << 5
	TYPE_EMPTY= 1 << 6

	ELEMENT   = 1 << 7
	ATTRIBUTE = 1 << 8
	SEPERATOR = 1 << 9

	MULTIPLE  = 1 << 10
	DISABLED  = 1 << 11
	OBSOLETE  = 1 << 12
	NEW		  = 1 << 13

	def findDataNode(self, names, parent, node):
		# returns a list of datanodes
		return parent.xpathEval(node.prop("name"))

	def mkvarlist(self, varnode):
		# build a value cache
		varlst = {}
		names = [self.name]
		def addvars(i):
			for x in i.xpathEval("*"):
				names.append(x.name)
				if not varlst.has_key(string.join(names, "__")):
					varlst[string.join(names, "__")] = []
				varlst[string.join(names, "__")].append(x)
				addvars(x)
				names.pop(-1)

		addvars(varnode)
		return varlst

	def getName(self, node):
		return node.name

	def list2config(self, lst, config, start=None):
		"""Parses the list and updates the xml config according the configure informations in the documentation
		   lst = [(name, value),(name, value),...]
		"""
		if not start:
			start = self.config
		varlst = {}
		rv = []

		if self.config == None:
			return

		import types
		# make a better form
		if type(lst) == types.DictType:
			varlst = lst
		elif type(lst) == types.ListType:
			for (name, value) in lst:
				varlst[name] = value
		else:
			raise ValueError, "lst must be a dictionary or list"

		def rec(names, node, dnode):
			# names is the recrusive list of parent
			# node is the current node we processing
			# dnode is the datanode
			xls = node.xpathEval("element")

			if not len(xls):
				xls = [node]
			elif node.prop("name"):
				names.append(node.prop("name"))

			for i in xls:
				if len(i.xpathEval("element")):
					# elements that has sub elements do not contains data, they are just parents
					dl = self.findDataNode(names, dnodeveloperde, i);
					names.append(i.prop("name"))
					if len(dl):
						dl = dl[0]
					else:
						if dnode.lastChild:
							l = dnode.lastChild()
							dl = dnode.newChild(None, i.prop("name"),None)
							l.addNextSibling(dl)
						else:
							dl = dnode.newChild(None, i.prop("name"),None)

					rec(names,i, dl)
					if dl.content == "" and dl.children == None :
						dl.unlinkNode()
						dl.freeNode()
					names.pop(-1)
				else:
					# this element will contain data
					clst = self.findDataNode(names, dnode, i)
					#clst = self.findDataNode(names, dnode, i)
					#clst = dnode.xpathEval(i.prop("name"))
					#if i.parent.name != "configuration":
					names.append(i.prop("name"))
					myname = string.join(names, "__")
					c = 0
					run = 1
					while run:
						try:
							cnode = clst[c]
						except:
							cnode = dnode.newChild(None, i.prop("name"),None)

						if cnode == None:
							break

						if varlst.has_key("%s_%s" %(myname, c)):
							if varlst["%s_%s" %(myname, c)] == "":
								cnode.setContent(None)
							else:
								cnode.setContent(varlst["%s_%s" %(myname, c)])
						else:
							cnode.setContent(None)

						# set the arguments
						attrs = i.xpathEval("attribute")
						# now we process every attribute if the node has one
						for j in attrs:
							if varlst.has_key("%s_%s__%s" %(myname, c,j.prop("name"))):
								if varlst["%s_%s__%s" %(myname, c,j.prop("name"))] != "":
									cnode.setProp(j.prop("name"), varlst["%s_%s__%s" %(myname, c,j.prop("name"))])
								elif cnode.prop(j.prop("name")):
									cnode.unsetProp(j.prop("name"))
							else:
								if cnode.prop(j.prop("name")):
									cnode.unsetProp(j.prop("name"))
								# we stop the loop, when there are no more nodes in the
								# config. so they can be deleted
								if c > len(clst):
									run = 0
						# when there are no attributes

						if not len(attrs):
							if not varlst.has_key("%s_%s" %(myname, c)):
								# unset deleted nodes
								cnode.unlinkNode()
								cnode.freeNode()
								c += 1
								if len(clst) < c:
									break
								continue
								#	continue
								#else:
								#	break

						# delete node if it has no content nor properties
						if (not cnode.properties) and cnode.content in ["", None]:
							cnode.unlinkNode()
							cnode.freeNode()
							if len(clst) < c:
								break
						else:
							# format the node
							if (cnode.next and not cnode.next.isText()) or not cnode.next:
								cnode.addNextSibling(cnode.doc.newDocText("\n" + mkspaces(cnode.parent)))

						c += 1
						run += 1
					if len(names):
						names.pop(-1)


		names = [self.name]

		# add namespaces to names when the is a subnode as start parameter
		if start.xpathCmpNodes(self.config):
			x = start
			while x.parent:
				if x.parent.name == "configuration":
					break
				if x.prop("name"):
					names.insert(1,x.prop("name"))
				x = x.parent

		rec(names,start,config)


	def config2list(self, varnode):
		"""return lst[(name, title, value, flags, doc), ...]"""#

		if self.config == None:
			# no configuration available
			return []

		rv = []
		varlst = {}
		namescount = {}

		if varnode:
			varlst = self.mkvarlist(varnode)


		def mknod(names, i, doc):
			name = ""
			value = ""
			title = i.prop("name")
			flags = 0
			name = string.join(names, "__")

			if i.name == "attribute":
				flags += self.ATTRIBUTE
				if varlst.has_key(name) and len(varlst[name]):
					if varlst[name][0].prop(i.prop("name")):
						value = varlst[name][0].prop(i.prop("name"))
				name += "_%s" %namescount[name]
				name += "__" + i.prop("name")

			else:
				name += "__" + i.prop("name")
				if namescount.has_key(name):
					namescount[name] += 1
				else:
					namescount[name] = 0
				if varlst.has_key(name) and len(varlst[name]):
					value =  varlst[name][0].content
				name += "_%s" %namescount[name]
				flags += self.ELEMENT

			if i.prop("multiple") == "1":
				flags += self.MULTIPLE

			if i.prop("type"):
				if i.prop("type") == "bool":
					flags += self.TYPE_BOOL
				elif i.prop("type") == "int":
					flags += self.TYPE_INT
				elif i.prop("type") == "pwd":
					flags += self.TYPE_PWD
				elif i.prop("type") == "empty":
					flags += self.TYPE_EMPTY
				else:
					flags += self.TYPE_TEXT
			else:
				flags += self.TYPE_TEXT
			return (name, title, value, flags, doc)

		def mkSeperator(names, i, doc):
			name = string.join(names, "__")
			name += "__" + i.prop("name")
			return (name, None, None, self.SEPERATOR, doc)

		def rec(names, node):
			for i in node.xpathEval("element"):
				try: doc = i.xpathEval("description")[0].content
				except: doc = ""
				if len(i.xpathEval("element")):
					# elements that has sub elements do not contains data, they are just parents
				#	rv.append(mkSeperator(names, i, doc))
					names.append(i.prop("name"))
					rec(names,i)
					names.pop(-1)
				else:
					# create documentation
					run = 3
					while run > 0:
						rv.append(mknod(names, i, doc))
						rtest = (rv[-1][3] & self.MULTIPLE)
						names.append(i.prop("name"))
						for j in i.xpathEval("attribute"):
							try: doc2 = j.xpathEval("description")[0].content
							except: doc2 = ""
							rv.append(mknod(names, j, doc2))
						tname = string.join(names, "__")
						if varlst.has_key(tname) and len(varlst[tname]):
							varlst[tname].pop(0)
						# multiple elements are drawn once more than values exist
						# this makes gui programming easier
						if rtest and varlst.has_key(tname) and len(varlst[tname]):
							pass
						elif rtest and varlst.has_key(tname) and not len(varlst[tname]):
							run -= 2
						elif rtest and not varlst.has_key(tname):
							names.pop(-1)
							break
						else:
							run -= 1
						names.pop(-1)
						# next element when multiple is not set
						if not rtest:
							break
						if run:
							rv.append(mkSeperator(names, i, doc))
				rv.append(mkSeperator(names, i, ""))
		rec([self.name],self.config)
		return rv


class Scriptinfo(Info):
	"""Script informations

	This class provides informations about a script

	>>> m = ctrlproxy.tools.Scriptinfo("/path/to/script")

	"""
	def __init__(self, name):
		self.path = name
		self.name = "script"

		self.doc = None
		self.config = None

		if not os.path.exists(name):
			name = os.path.join(ctrlproxy.get_path("share"),"ctrlproxy","scripts",name)
		f = open(name,"r")
		f.seek(0)
		fs = f.read(-1)
		try:
			fs.index('"""CTRLPROXY CONFIG')
		except:
			raise ValueError, "No configure information in the script"

		import parser
		import pprint
		ast = parser.suite(fs)
		tup = ast.totuple()
		#pprint.pprint(tup)
		from types import ListType, TupleType

		def match(pattern, data, vars=None):
			if vars is None:
				vars = {}
			if type(pattern) is ListType:
				vars[pattern[0]] = data
				return 1, vars
			if type(pattern) is not TupleType:
				return (pattern == data), vars
			if len(data) != len(pattern):
				return 0, vars
			for pattern, data in map(None, pattern, data):
				same, vars = match(pattern, data, vars)
				if not same:
					break
			return same, vars

		import symbol
		import token

		DOCSTRING_STMT_PATTERN = (
			symbol.stmt,
			(symbol.simple_stmt,
			 (symbol.small_stmt,
			  (symbol.expr_stmt,
			   (symbol.testlist,
				(symbol.test,
				 (symbol.and_test,
				  (symbol.not_test,
				   (symbol.comparison,
					(symbol.expr,
					 (symbol.xor_expr,
					  (symbol.and_expr,
					   (symbol.shift_expr,
						(symbol.arith_expr,
						 (symbol.term,
						  (symbol.factor,
						   (symbol.power,
							(symbol.atom,
							 (token.STRING, ['docstring'])
							 )))))))))))))))),
			 (token.NEWLINE, '')
			 ))

		# search the first tupels the first doc and config
		for i in range(1,5):
			found, vars = match(DOCSTRING_STMT_PATTERN, tup[i])
			if found:
				if not self.config and vars['docstring'][0:19] == '"""CTRLPROXY CONFIG':
					try:
						self.xml = libxml2.parseDoc(vars['docstring'][20:-20])
						self.config = self.xml.xpathEval("//configuration")[0]
					except:
						raise ValueError, "Can't parse config informations"
				elif not self.doc:
					self.doc = vars['docstring'][3:-3]
			if self.doc and self.config:
				break

		del tup
		f.close()
		del fs

	def mkvarlist(self, varnode):
		# return value list
		varlst = {}
		names = [self.name]

		def addnode(x):
			if x.name == "argument":
				names.append(x.prop("name"))
			else:
				names.append(x.name)
			if not varlst.has_key(string.join(names, "__")):
				varlst[string.join(names, "__")] = []
			varlst[string.join(names, "__")].append(x)

		def addvars(i):
			for x in i.xpathEval("*"):
				addnode(x)
				addvars(x)
				names.pop(-1)
			addnode(i)
			names.pop(-1)

		addnode(varnode)
		addvars(varnode)
		return varlst

	def getName(self, node):
		if node.name == "attribute":
			return node.prop("name")
		return node.name

	def findDataNode(self, names, dnode, node):
		if node.parent.name == "configuration":
			return [dnode, None]
		#if len(names) == 1:
		#	return dnode.xpathEval('argument[@name="%s"]' %node.prop("name"))
			#return [parent]
		lst =  dnode.xpathEval(node.prop("name"))
		return lst



class Moduleinfo(Info):
	"""Module informations

	This class provides informations about ctrlproxy modules.
	It extends the standard config.

	>>> m = ctrlproxy.tools.Moduleinfo("python")

	"""
	def __init__(self, name):
		self.name = name
		self.path = os.path.join(ctrlproxy.get_path("share"),"ctrlproxy","help","%s.mod.xml" %name)
		try:
			self.doc = libxml2.parseFile(self.path)
		except:
			raise ValueError, "Can't parse infos for %s in %s" %(name, os.path.join(ctrlproxy.get_path("share"),"ctrlproxy","help"))

		try:	self.config = self.doc.xpathEval("//configuration")[0]
		except: self.config = None
