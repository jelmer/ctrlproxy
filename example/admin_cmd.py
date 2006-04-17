import ctrlproxy

def command_handler(x1):
	print "Works!"

ctrlproxy.register_command('mycommand', command_handler, 'short help', 'long help')
