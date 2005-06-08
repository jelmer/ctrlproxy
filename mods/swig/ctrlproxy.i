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
%immutable channel_state::topic;
%immutable channel_state::mode;
%immutable channel_state::modes;
%immutable channel_state::limit;
%ignore channel_state::namreply_started;
%ignore client::incoming_id;
%ignore client::incoming;
%ignore client::hangup_id;
%immutable client::network;
%immutable client::description;
%immutable client::connect_time;
%immutable client::nick;
%immutable NetworkState;
%immutable NetworkInfo;
%rename(get_version) ctrlproxy_version;
%rename(Line) line;
%rename(Plugin) plugin;
%rename(Network) network;
%rename(NetworkState) network_state;
%rename(NetworkInfo) network_info;
%rename(Channel) channel;
%rename(LineStack) linestack_context;

%include "../../line.h";
%include "../../network.h";
%include "../../state.h";
%include "../../ctrlproxy.h";

%extend line {
	line(const struct line *l) {
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
	network(struct network_config *nc) {
		return load_network(nc);
	}
	
	~network() {
		unload_network(self);
	}

	gboolean connect() {
		return connect_network(self);
	}

	gboolean disconnect() {
		return disconnect_network(self);
	}

	void selectNextServer() {
		network_select_next_server(self);
	}

	int contains(struct client *c) {
		return verify_client(self, c);
	}

	void sendToServer(struct client *from, struct line *l)
	{
		network_send_line(self, from, l);
	}

	void sendToClients(struct line *l, struct client *exception = NULL) 
	{
		clients_send(self, l, exception);
	}
};

%extend network_state
{
	network_state(const char *nick, const char *username, const char *hostname) 
	{
		return new_network_state(nick, username, hostname);
	}

	~network_state() {
		free_network_state(self);
	}
	
	struct channel_state *getChannel(const char *name) {
		return find_channel(self, name);
	}
};

%extend client
{
	void sendState(struct network_state *s)
	{
		client_send_state(self, s);
	}
}

%extend network_info 
{
	const char *getFeature(const char *feature)  {
		return get_network_feature(self, feature);
	}

	char getPrefixByMode(char mode) {
		return get_prefix_by_mode(mode, self);
	}

	int compareNicks(const char *n1, const char *n2) {
		return irccmp(self, n1, n2);
	}

	int isChannel(const char *name) {
		return is_channelname(name, self);
	}

	int isPrefix(char prefix) {
		return is_prefix(prefix, self);
	}
};

%makedefault;
