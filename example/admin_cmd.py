import ctrlproxy
import ctrlproxy.admin

def command_handler(x1):
	print "Works!"

ctrlproxy.admin.register_command('mycommand', command_handler, 'short help', 'long help')
