from distutils.core import setup, Extension

setup (name = 'CtrlProxy',
	   version = '2.7',
	   author = 'Jelmer Vernooij',
	   author_email = 'jelmer@vernstok.nl',
	   url = 'http://ctrlproxy.vernstok.nl/',
	   description = 'CtrlProxy bindings',
	   license = 'GPL',
	   ext_modules = [
	   		Extension('_ctrlproxy', 
		         	  sources = ['ctrlproxy_wrap.c']
					 )
			],
		py_modules = [ 'ctrlproxy' ]
		)
