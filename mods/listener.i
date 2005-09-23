%module(package="ctrlproxy") listener
%{
#include "listener.h"
%}
%include "common.i";
%immutable listener::incoming;
%immutable listener::port;
%nodefault;
%rename(Listener) listener;
%include "../listener.h";
%extend listener {
	listener(int port, const char *address = NULL) { 
		return listener_init(address, port);
	}
	
	gboolean start() {
		return start_listener(self);
	}

	gboolean stop() {
		return stop_listener(self);
	}
};
