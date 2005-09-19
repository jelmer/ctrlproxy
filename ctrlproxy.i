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
%ignore channel_state::banlist_started;
%ignore channel_state::ignorelist_started;
%ignore channel_state::exceptlist_started;
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

%include "../../line.h";
%include "../../network.h";
%include "../../client.h";
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

	gboolean send(struct line *l)
	{
		return network_send_line(self, NULL, l);
	}

	void sendToClients(struct line *l, struct client *exception = NULL) 
	{
		clients_send(self, l, exception);
	}
};

%rename(_new_network_state) new_network_state;

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

	gboolean send(const struct line *l) 
	{
		return client_send_line(self, l);
	}
}

%extend network_info 
{
	const char *getFeature(const char *feature)  {
		return g_hash_table_lookup(self->features, feature);
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

%{
struct linestack
{
	struct network *n;
	linestack_marker *m;
};
%}

%extend linestack
{
	linestack(struct network *n, int lines)
	{
		struct linestack *ls = g_new0(struct linestack, 1);
		ls->n = n;
		ls->m = linestack_get_marker_numlines(n, lines);
		return ls;
	}

	linestack(struct network *n) 
	{
		return linestack_get_marker(n);
	}

	struct network_state *getState()
	{
		return linestack_get_state(self->n, self->m);
	}

	gboolean traverse()
	{
		return linestack_traverse(self->n, self->m, NULL, NULL, NULL); /* FIXME */
	}

	gboolean traverse(const char *obj)
	{
		return linestack_traverse_object(self->n, obj, self->m, NULL, NULL, NULL); /* FIXME */
	}

	gboolean send(const struct client *c)
	{
		return linestack_send(self->n, self->m, NULL, c);
	}
	
	gboolean send(const struct client *c, const char *obj)
	{
		return linestack_send_object(self->n, obj, self->m, NULL, c);
	}

	gboolean replay(struct network_state *st)
	{
		return linestack_replay(self->n, self->m, NULL, st);
	}

	~linestack()
	{
		linestack_free_marker(self->m);
		g_free(self);
	}
};

%makedefault;
