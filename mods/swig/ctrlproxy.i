%module ctrlproxy
%{
#include "ctrlproxy.h"
%}
%nodefault;

/* These are not useful to non-C programmers */
%ignore list_make_string;
%ignore xmlFindChildByName;
%ignore xmlFindChildByElementName;
%rename(get_version) ctrlproxy_version;
%rename(Line) line;
%rename(Plugin) plugin;
%rename(Network) network;
%rename(Channel) channel;
%rename(LineStack) linestack_context;
%rename(Transport) transport_context;
%include "../../ctrlproxy.h";


%extend line {
	line(struct line *l) {
		return linedup(l);
	}

	line(char *data) {
		return irc_parse_line(data);
	}

	line(char *origin, ...) {
		struct line *l;
		va_list ap;
		va_start(ap, origin);
		l = irc_parse_line_args(origin, ap);
		va_end(ap);
		return l;
	}

	line(char *origin, va_list ap) {
		return irc_parse_line_args(origin, ap);
	}

	~line() {
		free_line(self);
	}

	const char *toString() {
		return irc_line_string(self);
	}

	const char *getNick() {
		return line_get_nick(self);
	}
};

%extend network {
	void addListener(xmlNodePtr listener) {
		network_add_listen(self, listener);
	}

	const char *getFeature(const char *feature)  {
		return get_network_feature(self, feature);
	}

	char getPrefixByMode(char mode) {
		return get_prefix_by_mode(mode, self);
	}

	struct channel *getChannel(const char *name) {
		return find_channel(self, name);
	}

	~network() {
		close_network(self);
	}

	int disconnect() {
		return close_server(self);
	}

	gboolean changeNick(const char *new_nick) {
		return network_change_nick(self, new_nick);
	}

	int contains(struct client *c) {
		return verify_client(self, c);
	}

	int compareNicks(const char *n1, const char *n2) {
		return irccmp(self, n1, n2);
	}

	GSList *replicate() {
		return gen_replication_network(self);
	}

	int isChannel(const char *name) {
		return is_channelname(name, self);
	}

	int isPrefix(char prefix) {
		return is_prefix(prefix, self);
	}

	void sendLine(struct line *l, struct client *exception = NULL) 
	{
		clients_send(self, l, exception);
	}
};

%extend linestack_context {
	linestack_context(struct network *n) {
		return linestack_new_by_network(n);
	}

	linestack_context(const char *backend_name, const char *data = NULL) {
		return linestack_new(backend_name, data);
	}

	void clear() {
		linestack_clear(self);
	}

	void addLine(struct line *l) {
		linestack_add_line(self, l);
	}

	void addLineList(GSList *l) {
		/* FIXME */
	}

	GSList *getLineList() {
		/* FIXME */
		return NULL;
	}

	~linestack() {
		linestack_destroy(self);
	}
};

%extend channel {
	GSList *replicate(const char *hostmask, const char *nick) {
		return gen_replication_channel(self, hostmask, nick);
	}
};

%extend transport_context {
	transport_context(const char *name) {
		return transport_connect(name, NULL, NULL, NULL, NULL, NULL);
	}
	/* FIXME: What about transport_listen? */
	
	~transport_context() {
		transport_free(self);
	}

	int write(const char *l) {
		return transport_write(self, l);	
	}
};

%makedefault;
