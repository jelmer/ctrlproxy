from distutils.core import setup, Extension
from string import split
import os

setup (name = 'CtrlProxy',
	   version = '2.7',
	   author = 'Jelmer Vernooij',
	   author_email = 'jelmer@vernstok.nl',
	   url = 'http://ctrlproxy.vernstok.nl/',
	   description = 'CtrlProxy bindings',
	   ext_modules = [
	   		Extension('ctrlproxy', 
		         	  sources = ['ctrlproxy_wrap.c'],
					  extra_compile_args = split(os.environ['CFLAGS'],' '),
					  extra_link_args = split(os.environ['LDFLAGS'],' ')
					 )
			]
		)
