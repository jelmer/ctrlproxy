from distutils.core import setup, Extension

setup (name = 'ctrlproxy',
	   version = '2.7',
	   author = 'Jelmer Vernooij',
	   author_email = 'jelmer@vernstok.nl',
	   url = 'http://ctrlproxy.vernstok.nl/',
	   description = 'CtrlProxy bindings',
	   license = 'GPL',
	   package_dir = {"ctrlproxy":""},
	   ext_modules = [
	   		Extension('_ctrlproxy', 
		         	  sources = ['ctrlproxy_wrap.c']
					 ),
			Extension('ctrlproxy._admin',
					  sources = ['admin_wrap.c']
					  ),
			Extension('ctrlproxy._listener',
					  sources = ['listener_wrap.c']
					  )
			],
		py_modules = [ 'ctrlproxy', 'ctrlproxy.admin', 'ctrlproxy.listener' ]
		)
