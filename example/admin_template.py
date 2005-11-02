import ctrlproxy.admin

def foo(a):
    print "Arguments: %s" % a
    
ctrlproxy.admin.register_admin_command("simple", foo, "help", "help advanced")
