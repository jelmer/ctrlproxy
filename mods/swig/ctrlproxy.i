%module(directors="1") ctrlproxy
%{
#include "ctrlproxy.h"
%}
%include "common.i";
%feature("autodoc", 1);
%nodefault;

/* These are not useful to non-C programmers */
%ignore list_make_string;
%immutable channel::name;
%immutable network::connection;
%immutable channel::joined;
%immutable line::direction;
%immutable line::network;
%immutable line::client;
%immutable line::network_nick;
%ignore line::has_endcolon;
%immutable channel_nick::mode;
%immutable channel_nick::channel;
%immutable channel_nick::global_nick;
%immutable network_nick::refcount;
%immutable network_nick::name;
%immutable network_nick::hostmask;
%immutable network_nick::channels;
%immutable channel::topic;
%immutable channel::mode;
%immutable channel::modes;
%immutable channel::limit;
%ignore channel::namreply_started;
%ignore channel::introduced;
%ignore client::incoming_id;
%ignore client::incoming;
%ignore client::hangup_id;
%immutable client::network;
%immutable client::description;
%immutable client::connect_time;
%immutable client::nick;
%rename(get_version) ctrlproxy_version;
%rename(Line) line;
%rename(Plugin) plugin;
%rename(Network) network;
%rename(Channel) channel;
%rename(LineStack) linestack_context;

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

	const char *__str__() {
		return irc_line_string(self);
	}

	const char *getNick() {
		return line_get_nick(self);
	}
};

%extend network {
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

	void sendToServer(struct line *l)
	{
		network_send_line(self, l);
	}

	void sendToClients(struct line *l, struct client *exception = NULL) 
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

%makedefault;
