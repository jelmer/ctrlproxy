#!/usr/bin/python

"""
templayer.py - Layered Template Library for HTML

  http://excess.org/templayer

library to build dynamic html that provides clean separation of form
(html+css+javascript etc..) and function (python code) as well as
making cross-site scripting attacks and improperly generated html
very difficult.

Copyright (c) 2003-2004 Ian Ward

This module is free software, and you may redistribute it and/or modify
it under the same terms as Python itself, so long as this copyright message
and disclaimer are retained in their original form.

IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
THIS CODE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE.  THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
AND THERE IS NO OBLIGATION WHATSOEVER TO PROVIDE MAINTENANCE,
SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
"""

__author__ = "Ian Ward"
__version__ = "$Revision: 1.2 $"[11:-2]

import string
import sys


class Template:
	"""
	class Template - common functionality for Template subclasses
	"""
	def __init__( self, name ):
		"""
		__init__( path and name of template file )
		"""
		tmpl = open(name).read()
		self.cache = {}
		try:
			self.header, rest = tmpl.split("{contents}")
			self.body, self.footer = rest.split("{/contents}")
		except:
			raise Exception( "Missing contents!" )
		

	def layer( self, name ):
		"""
		layer( name of layer ) -> layer as text
		"""
		if self.cache.has_key(name): 
			return self.cache[name]
		try:
			ignore, rest = self.body.split("{%s}"%name)
			s, ignore = rest.split("{/%s}"%name)
			self.cache[name] = s
			return s
		except:
			raise Exception( "Missing layer '%s'"%name )

	def format_header_footer( self, **args ):
		"""
		format_header_footer( ** args ) -> header text, footer text

		fills slots in header and footer using args dictionary 
		"""
		header, footer = self.header, self.footer
		for k,v in args.items():
			if (header+footer).find("%"+k+"%") == -1:
				raise Exception( "Header/Footer missing variable '%s'" % k )
			k,v = self.pre_process(k,v)
			header = header.replace( "%"+k+"%", v )
			footer = footer.replace( "%"+k+"%", v )
		return self.post_process(header), self.post_process(footer)


	def format( self, layer_name, ** args ):
		"""
		format( layer name, ** args ) -> layer text

		fills slots in layer using args dictionary
		"""
		s = self.layer(layer_name)

		for k,v in args.items():
			if s.find("%"+k+"%") == -1:
				raise Exception( "Layer '%s' missing variable '%s'" % (layer_name, k) )
			k,v = self.pre_process(k,v)
			s = s.replace( "%"+k+"%", v )
		return self.post_process(s)

	def start_file( self, file = None ):
		"""
		start_file( output stream = None ) -> FileLayer object 
		
		if output stream is None, FileLayer object will use sys.stdout
		"""
		return FileLayer( file, self )

	def pre_process( self, key, value ):
		"""
		pre_process( key, value ) -> key, filtered/escaped value

		override this function to provide escaping or filtering of
		text sent from the module user 
		"""
		assert type(value) == type("")
		return key, value
	
	def post_process( self, value ):
		"""
		post_process( value ) -> wrapped value

		override to wrap processed text in order to mark it as processed
		"""
		return value
		
	def finalize( self, value ):
		"""
		finalize( value ) -> unwrapped value
		
		override this function to reverse wrapping applied before
		sending to the output file
		"""
		return value


class HtmlTemplate(Template):
	"""
	class HtmlTemplate - HTML Template class.  
	Treats input to write and write_layer as "markup" defined by _expand_html_markup
	"""
	def pre_process(self, key, value):
		"""
		pre_process( key, markup value ) -> key, expanded markup string
		"""
		return key, _expand_html_markup(value)
		
	def post_process(self, value ):
		"""
		post_process( value ) -> RawHTML object containing value
		"""
		return RawHTML( value )
		
	def finalize(self, value):
		"""
		finalize( RawHTML object ) -> contained html string
		"""
		assert isinstance(value,RawHTML)
		return value.value


class MarkupException(Exception):
	pass

MKE_BAD_TYPE = "Expecting string, list, tuple or RawHTML, but found %s"
MKE_INVALID_HREF = ("'href' must be in the form ('href',url,content) or " +
	"('href',('urljoin',head,tail),content)")
MKE_INVALID_JOIN = "'join' must be in the form ('join',list,separator)"
MKE_INVALID_URLJOIN = ("'urljoin' must be in the form ('urljoin',safe_str," +
	"unsafe_str)")
MKE_INVALID_TARGET = "'target' must be in the form ('target',name,contents)"
MKE_INVALID_BR_P = "'%s' must be in the form ('%s',count)"
MKE_INVALID_B_I_U = "'%s' must be in the form ('%s',content)"
MKE_UNRECOGNISED = ("tuple must begin with one of the following strings: " +
	"'join', 'urljoin', 'href', 'target', 'br', 'p', 'i', 'b'")


def _expand_html_markup( v ):
	"""
	_expand_html_markup( string or list/tuple construct ) -> html string

	this function escapes all strings passed, and generates html
	based on the following:
	[item1, item2, ...]   ->  concatenate item1, 2, ...
	('join',list,sep)     ->  join items in list with seperator sep
	('urljoin',head,tail) ->  join safe url head with unsafe url-ending tail
	('href',link,content) ->  HTML href to link wrapped around content
	('target',name)       ->  HTML target name
	('br',count)          ->  HTML line break * count
	('p',count)           ->  HTML paragraph break * count
	('i',content)         ->  italics
	('b',content)         ->  bold
	('u',content)         ->  underline
	"""
	if isinstance(v,RawHTML):
		return v.value
		
	if type(v) == type(""): 
		return html_escape(v)
		
	if type(v) == type([]):
		l = []
		for x in v:
			l.append( _expand_html_markup( x ) )
		return "".join(l)

	if type(v) != type(()):
		raise MarkupException(MKE_BAD_TYPE % `v`)
	
	if v[0] == 'href':
		if len(v)!=3:
			raise MarkupException(MKE_INVALID_HREF)
		if type(v[1]) == type(""):
			v=[ v[0], ('urljoin',v[1],"") ,v[2] ]
		if type(v[1])!=type(()) or v[1][0]!='urljoin':
			raise MarkupException(MKE_INVALID_HREF)
		return html_href(_expand_html_markup(v[1]),
			_expand_html_markup(v[2]))
		
	if v[0] == 'join':
		if len(v)!=3 or type(v[1]) != type([]):
			raise MarkupException(MKE_INVALID_JOIN)
		sep = _expand_html_markup(v[2])
		l = []
		for x in v[1]:
			l.append( _expand_html_markup( x ) )
		return sep.join(l)
		
	if v[0] == 'urljoin':
		if len(v)!=3 or type(v[1])!=type("") or type(v[2])!=type(""):
			raise MarkupException(MKE_INVALID_URLJOIN)
		return v[1]+html_url_encode(v[2])
	
	if v[0] == 'target':
		if len(v)!=3 or type(v[1])!=type(""):
			raise MarkupException(MKE_INVALID_TARGET)
		return html_target(html_url_encode(v[1]),
			_expand_html_markup(v[2]))
			
	if v[0] == 'br' or v[0] == 'p':
		if len(v)!=2 or type(v[1])!=type(0):
			raise MarkupException(MKE_INVALID_BR_P %(v[0],v[0]))
		return ("<"+v[0]+">") * v[1]
		
	if v[0] == 'b' or v[0] == 'i' or v[0] == 'u':
		if len(v)!=2:
			raise MarkupException(MKE_INVALID_B_I_U %(v[0],v[0]))
		return ("<"+v[0]+">"+
			_expand_html_markup(v[1])+
			"</"+v[0]+">")

	raise MarkupException(MKE_UNRECOGNISED)
		


class RawHTML:
	"""
	class RawHTML - wrap strings of generated html that are passed 
	outside the module so that they aren't escaped when passed back in
	"""
	def __init__( self, value ):
		self.value = value
	def __repr__( self ):
		return 'RawHTML(%s)'%`self.value`


class FileLayer:
	"""
	class FileLayer - the layer within which all other layers nest,
	responsible for sending generated text to the output stream
	"""
	def __init__( self, file, template ):
		"""
		__init__( output stream, template object )

		if output stream is None, use sys.stdout
		"""
		if not file: file = sys.stdout
		self.out = file
		self.template = template
		self.child = None
	
	def open( self, ** args ):
		"""
		open( ** args ) -> child Layer object

		slots in the header and footer of the template are filled
		by using args
		"""
		assert self.child == None
		header, footer = self.template.format_header_footer(**args)
		
		self.child = Layer( self.template, header, footer )
		return self.child
	
	def flush( self ):
		"""
		flush()

		flush the output so far to the output stream
		"""
		assert self.child != None, "FileLayer already closed!"
		self.out.write( "".join(self.child.flush()) )
		self.out.flush()
	
	def close( self ):
		"""
		close()

		close all child layers and flush the output to the output stream
		"""
		assert self.child != None, "FileLayer already closed!"
		self.out.write( "".join(self.child.close()) )
		self.child = None
	

class Layer:
	"""
	class Layer - holds the state of one of the nested layers while its
	contents are being written to.  Layers are closed implicitly when
	a parent layer is written to, or explicitly when its parent's
	close_child is called.
	"""
	def __init__( self, template, header, footer ):
		"""
		__init__( Template object, header text, footer text )
		"""
		self.child = None
		self.template = template
		header = self.template.finalize(header)
		self.out = [header]
		self.footer = self.template.finalize(footer)
	
	def close_child(self):
		"""
		close_child()

		close open child if one exists, and append its output to
		what this layer will output
		"""
		if self.out == None:
			raise Exception("Layer is closed!")
		if self.child:
			self.out = self.out + self.child.close()
			self.child = None
	
	def open_layer( self, layer, ** args ):
		"""
		open_layer( layer name, ** args ) -> child Layer object

		open a layer as a child of this one, filling its slots with
		values from args
		"""
		self.close_child()
		block = self.template.format( layer, ** args )
		try:
			header, footer = block.value.split("%contents%")
		except:
			raise Exception( "Layer "+layer+" missing %contents%")
		header = self.template.post_process(header)
		footer = self.template.post_process(footer)
		self.child = Layer( self.template, header, footer)
		return self.child

	def write_layer( self, layer, ** args ):
		"""
		write_layer( layer name, ** args )

		fill the slots of a layer with args and include it as part
		of the contents of this layer
		"""
		self.close_child()
		result = self.template.format( layer, ** args )
		self.out.append( self.template.finalize(result) )
	
	def write( self, text ):
		"""
		write( text or markup )

		include text or markup as part of the contents of this layer
		"""
		ignore, result = self.template.pre_process("",text)
		result = self.template.post_process(result)
		self.close_child()
		self.out.append( self.template.finalize(result) )

	def flush( self ):
		"""
		flush() -> partial output text as list of strings

		return and flush all output completed so far
		"""
		if self.out == None:
			raise Exception("Layer is closed!")
		output = self.out 
		if self.child:
			output = output + self.child.flush()
		self.out = []
		return output

	def close( self ):
		"""
		close() -> output text as list of strings

		close child if any and return all remaining output
		"""
		self.close_child()
		final = self.out+[self.footer]
		self.out = None # break further outputting
		return final
		

_html_url_valid = string.letters + string.digits + "$-_.!*'()"
def html_url_encode(text, query=0):
	"""html_url_encode( text, query=0 ) -> html
	Encode urls according to RFC1738

	if query is set, use "+" instead of "%20" for spaces"""
	
	url = ""
	for c in text:
		if c in _html_url_valid:
			url += c
		elif query and c==" ":
			url += "+"
		else:
			url += "%" + "%02x" % ord(c)
	return url

def html_href(l,v):
	"""html_href(link, value) -> html
	return '<a href="link">value</a>'"""
	
	return '<a href="%s">%s</a>'%(l,v)

def html_target(target,caption):
	"""html_target(target,caption) -> html
	return '<a name="target">caption</a>'"""
	
	return '<a name="%s">%s</a>'%(target,caption)


def html_escape(text):
	"""html_escape(text) -> html
	Escape text so that it will be displayed safely within HTML"""
	text = string.replace(text,'&','&amp;')
	text = string.replace(text,'"','&quot;') # in case we're in an attrib.
	text = string.replace(text,'<','&lt;')
	text = string.replace(text,'>','&gt;')
	return text




def main():
	import sys
	print sys.argv


if __name__ == "__main__": main()
