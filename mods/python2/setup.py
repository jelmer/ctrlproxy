from distutils.core import setup, Extension

ctrlproxy = Extension('ctrlproxy', 
		         	  sources = ['ctrlproxy_wrap.c'])

setup (name = 'CtrlProxy',
	   version = '2.7',
	   author = 'Jelmer Vernooij',
	   author_email = 'jelmer@vernstok.nl',
	   url = 'http://ctrlproxy.vernstok.nl/',
	   description = 'CtrlProxy bindings',
	   ext_modules = [ctrlproxy])
