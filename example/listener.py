import ctrlproxy
import ctrlproxy.listener

l = ctrlproxy.listener.Listener(3000)
l.password = "secret"
l.start()
print l.port
l.stop()
