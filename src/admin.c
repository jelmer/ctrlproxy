/*
	ctrlproxy: A modular IRC proxy
	admin: module for remote administration. 

	(c) 2003-2006 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "internals.h"
#include <string.h>
#include <sys/un.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#include "admin.h"

#define ADMIN_CHANNEL "#ctrlproxy"

static GList *commands = NULL;
static guint longest_command = 0;

struct admin_handle
{
	struct global *global;
	struct client *client;
	struct network *network;
	void *user_data;
	void (*send_fn) (struct admin_handle *, const char *data);
};

static void privmsg_admin_out(admin_handle h, const char *data)
{
	struct client *c = h->client;
	char *nick = c->nick;
	char *hostmask;

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", c->network->name);
	if (c->network->state) nick = c->network->state->me.nick;
	client_send_args_ex(c, hostmask, "NOTICE", nick, data, NULL);

	g_free(hostmask);
}

static void network_admin_out(admin_handle h, const char *data)
{
	struct client *c = h->client;
	char *hostmask;

	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", c->network->name);
	virtual_network_recv_args(c->network, hostmask, "PRIVMSG", ADMIN_CHANNEL, 
							  data, NULL);

	g_free(hostmask);
}

struct client *admin_get_client(admin_handle h)
{
	return h->client;
}

struct global *admin_get_global(admin_handle h)
{
	return h->global;
}

struct network *admin_get_network(admin_handle h)
{
	return h->network;
}

void admin_out(admin_handle h, const char *fmt, ...)
{
	va_list ap;
	char *msg;
	va_start(ap, fmt);
	msg = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	h->send_fn(h, msg);

	g_free(msg);
}

static void add_network (admin_handle h, char **args, void *userdata)
{
	struct network_config *nc;

	if (args[1] == NULL) {
		admin_out(h, "No name specified");
		return;
	}

	nc = network_config_init(admin_get_global(h)->config);
	g_free(nc->name); nc->name = g_strdup(args[1]);
	load_network(admin_get_global(h), nc);

	admin_out(h, "Network `%s' added. Use ADDSERVER to add a server to this network.", args[1]);
}

static void del_network (admin_handle h, char **args, void *userdata)
{
	struct network *n;

	if (args[1] == NULL) {
		admin_out(h, "Not enough parameters");
		return;
	}

	n = find_network(admin_get_global(h), args[1]);
	if (n == NULL) {
		admin_out(h, "No such network `%s'", args[1]);
		return;
	}

	disconnect_network(n);

	admin_out(h, "Network `%s' deleted", args[1]);
}

static void add_server (admin_handle h, char **args, void *userdata)
{
	struct network *n;
	struct tcp_server_config *s;
	char *t;

	if(!args[1] || !args[2]) {
		admin_out(h, "Not enough parameters");
		return;
	}

	n = find_network(admin_get_global(h), args[1]);

	if (!n) {
		admin_out(h, "No such network '%s'", args[1]);
		return;
	}

	if (n->config->type != NETWORK_TCP) {
		admin_out(h, "Not a TCP/IP network!");
		return;
	}

	s = g_new0(struct tcp_server_config, 1);

	s->host = g_strdup(args[2]);
	if ((t = strchr(s->host, ':'))) {
		*t = '\0';
		s->port = g_strdup(t+1);
	} else {
		s->port = g_strdup("6667");
	}
	s->ssl = FALSE;
	s->password = args[3]?g_strdup(args[3]):NULL;

	n->config->type_settings.tcp_servers = g_list_append(n->config->type_settings.tcp_servers, s);

	admin_out(h, "Server added to `%s'", args[1]);
}

static void com_connect_network (admin_handle h, char **args, void *userdata)
{
	struct network *s;
	if(!args[1]) {
		 admin_out(h, "No network specified");
		 return;
	}

	s = find_network(admin_get_global(h), args[1]);

	if (!s) {
		admin_out(h, "No such network `%s'", args[1]);
		return;
	}

	switch (s->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			admin_out(h, "Connecting to `%s'", args[1]);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(h, "Forcing reconnect to `%s'", args[1]);
			disconnect_network(s);
			network_select_next_server(s);
			connect_network(s);
			break;
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
			admin_out(h, "Connect to `%s' already in progress", args[1]);
			break;
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, "Already connected to `%s'", args[1]);
			break;
	}
}

static void com_disconnect_network (admin_handle h, char **args, void *userdata)
{
	struct network *n;

	n = admin_get_network(h);

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h), args[1]);
		if(!n) {
			admin_out(h, "Can't find active network with that name");
			return;
		}
	}

	if (n->connection.state == NETWORK_CONNECTION_STATE_NOT_CONNECTED) {
		admin_out(h, "Already disconnected from `%s'", args[1]);
	} else {
		admin_out(h, "Disconnecting from `%s'", args[1]);
		disconnect_network(n);
	}
}

static void com_next_server (admin_handle h, char **args, void *userdata) 
{
	struct network *n;
	const char *name;


	if(args[1] != NULL) {
		name = args[1];
		n = find_network(admin_get_global(h), args[1]);
	} else {
		n = admin_get_network(h);
		name = n->name;
	}
	if(!n) {
		admin_out(h, "%s: Not connected", name);
	} else {
		admin_out(h, "%s: Reconnecting", name);
		disconnect_network(n);
		network_select_next_server(n);
		connect_network(n);
	}
}

static void com_save_config (admin_handle h, char **args, void *userdata)
{
	const char *adm_dir;
	global_update_config(admin_get_global(h));
	adm_dir = args[1]?args[1]:admin_get_global(h)->config->config_dir; 
	save_configuration(admin_get_global(h)->config, adm_dir);
	admin_out(h, "Configuration saved in %s", adm_dir);
}

static void help (admin_handle h, char **args, void *userdata)
{
	GList *gl = commands;
	char *tmp;
	char **details;
	int i;

	if(args[1]) {
		admin_out(h, "Details for command %s:", args[1]);
	} else {
		admin_out(h, "The following commands are available:");
	}
	while(gl) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(args[1]) {
			if(!g_strcasecmp(args[1], cmd->name)) {
				if(cmd->help_details != NULL) {
					details = g_strsplit(cmd->help_details, "\n", 0);
					for(i = 0; details[i] != NULL; i++) {
						admin_out(h, details[i]);
					}
					return;
				} else {
					admin_out(h, "Sorry, no help for %s available", args[1]);
				}
			}
		} else {
			if(cmd->help != NULL) {
				tmp = g_strdup_printf("%s%s     %s",cmd->name,g_strnfill(longest_command - strlen(cmd->name),' '),cmd->help);
				admin_out(h, tmp);
				g_free(tmp);
			} else {
				admin_out(h, cmd->name);
			}
		}
		gl = gl->next;
	}
	if(args[1]) {
		admin_out(h, "Unknown command");
	}
}

static void list_networks(admin_handle h, char **args, void *userdata)
{
	GList *gl;
	for (gl = admin_get_global(h)->networks; gl; gl = gl->next) {
		struct network *n = gl->data;

		switch (n->connection.state) {
		case NETWORK_CONNECTION_STATE_NOT_CONNECTED:
			admin_out(h, ("%s: Not connected"), n->name);
			break;
		case NETWORK_CONNECTION_STATE_RECONNECT_PENDING:
			admin_out(h, ("%s: Reconnecting"), n->name);
			break;
		case NETWORK_CONNECTION_STATE_LOGIN_SENT:
		case NETWORK_CONNECTION_STATE_MOTD_RECVD:
			admin_out(h, ("%s: connected"), n->name);
			break;
		}
	}
}

static void detach_client(admin_handle h, char **args, void *userdata)
{
	struct client *c = admin_get_client(h);

	disconnect_client(c, "Client exiting");
}

static void dump_joined_channels(admin_handle h, char **args, void *userdata)
{
	struct network *n;
	GList *gl;

	if (args[1] != NULL) {
		n = find_network(admin_get_global(h), args[1]);
		if(n == NULL) {
			admin_out(h, "Can't find network '%s'", args[1]);
			return;
		}
	} else {
		n = admin_get_network(h);
	}

	if (!n->state) {
		admin_out(h, "Network '%s' not connected", n->name);
		return;
	}

	for (gl = n->state->channels; gl; gl = gl->next) {
		struct channel_state *ch = (struct channel_state *)gl->data;
		admin_out(h, "%s", ch->name);
	}
}

#ifdef DEBUG
static void do_abort(admin_handle h, char **args, void *userdata)
{
	abort();
}
#endif

static void handle_die(admin_handle h, char **args, void *userdata)
{
	exit(0);
}

static GHashTable *markers = NULL;

static void repl_command(admin_handle h, char **args, void *userdata)
{
	struct linestack_marker *lm;
	struct network *n;
	
	n = admin_get_network(h);

	lm = g_hash_table_lookup(markers, n);

	if (n->linestack == NULL) {
		admin_out(h, "No backlog available. Perhaps the connection to the network is down?");
		return;
	}

	if(!args[1]) {
		admin_out(h, "Sending backlog for network '%s'", n->name);

		linestack_send(n->linestack, lm, NULL, admin_get_client(h));

		g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));

		return;
	} 

	/* Backlog for specific nick/channel */
	admin_out(h, "Sending backlog for channel %s", args[1]);

	if (n->global->config->report_time)
		linestack_send_object_timed(n->linestack, args[1], lm, NULL, 
									admin_get_client(h));
	else
		linestack_send_object(n->linestack, args[1], lm, NULL, 
							  admin_get_client(h));

	g_hash_table_replace(markers, n, linestack_get_marker(n->linestack));
}

static void handle_charset(admin_handle h, char **args, void *userdata)
{
	GError *error = NULL;
	struct client *c;

	c = admin_get_client(h);

	if (args[1] == NULL) {
		admin_out(h, "No charset specified");
		return;
	}

	if (!client_set_charset(c, args[1])) {
		admin_out(h, "Error setting charset: %s", error->message);
	}
}

static gint cmp_cmd(gconstpointer a, gconstpointer b)
{
	const struct admin_command *cmda = a, *cmdb = b;

	return g_strcasecmp(cmda->name, cmdb->name);
}

void register_admin_command(const struct admin_command *cmd)
{
	commands = g_list_insert_sorted(commands, g_memdup(cmd, sizeof(*cmd)), cmp_cmd);
	if (strlen(cmd->name) > longest_command) longest_command = strlen(cmd->name);
}

void unregister_admin_command(const struct admin_command *cmd)
{
	commands = g_list_remove(commands, cmd);
}

static gboolean process_cmd(admin_handle h, const char *cmd)
{
	char **args = NULL;
	GList *gl;

	if (!cmd) {
		admin_out(h, "Please specify a command. Use the 'help' command to get a list of available commands");
		return TRUE;
	}

	args = g_strsplit(cmd, " ", 0);

	/* Ok, arguments are processed now. Execute the corresponding command */
	for (gl = commands; gl; gl = gl->next) {
		struct admin_command *cmd = (struct admin_command *)gl->data;
		if(!g_strcasecmp(cmd->name, args[0])) {
			cmd->handler(h, args, cmd->userdata);
			g_strfreev(args);
			return TRUE;
		}
	}

	admin_out(h, "Can't find command '%s'. Type 'help' for a list of available commands. ", args[0]);

	g_strfreev(args);

	return TRUE;
}

gboolean admin_process_command(struct client *c, struct line *l, int cmdoffset)
{
	int i;
	char *tmp = g_strdup(l->args[cmdoffset]);
	gboolean ret;
	struct admin_handle ah;

	/* Add everything after l->args[cmdoffset] to tmp */
	for(i = cmdoffset+1; l->args[i]; i++) {
		char *oldtmp = tmp;
		tmp = g_strdup_printf("%s %s", oldtmp, l->args[i]);
		g_free(oldtmp);
	}
	l->is_private = 1;

	ah.send_fn = privmsg_admin_out;
	ah.client = c;
	ah.network = c->network;
	ah.global = c->network->global;
	ret = process_cmd(&ah, tmp);

	g_free(tmp);

	return ret;
}

static gboolean admin_net_init(struct network *n)
{
	struct channel_state *cs = g_new0(struct channel_state, 1);
	struct channel_nick *user_nick = g_new0(struct channel_nick, 1);
	struct channel_nick *admin_nick = g_new0(struct channel_nick, 1);
	struct network_nick *admin_nnick = g_new0(struct network_nick, 1);
	char *hostmask;
	
	/* Channel */
	cs->name = g_strdup(ADMIN_CHANNEL);
	cs->topic = g_strdup("CtrlProxy administration channel | Type `help' for more information");
	cs->network = n->state;

	/* The users' user */
	user_nick->global_nick = &n->state->me;
	user_nick->channel = cs;
	user_nick->mode = ' ';

	cs->nicks = g_list_append(cs->nicks, user_nick);
	n->state->me.channel_nicks = g_list_append(n->state->me.channel_nicks, user_nick);

	/* CtrlProxy administrator */
	/* global */
	hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", n->name);
	network_nick_set_hostmask(admin_nnick, hostmask);
	g_free(hostmask);

	admin_nnick->fullname = g_strdup("CtrlProxy Admin Tool");
	admin_nnick->channel_nicks = g_list_append(admin_nnick->channel_nicks, admin_nick);

	n->state->nicks = g_list_append(n->state->nicks, admin_nnick);

	/* channel */
	admin_nick->global_nick = admin_nnick;
	admin_nick->channel = cs;
	admin_nick->mode = '@';

	cs->nicks = g_list_append(cs->nicks, admin_nick);
	
	n->state->channels = g_list_append(n->state->channels, cs);

	return TRUE;
}

static gboolean admin_to_server (struct network *n, struct client *c, const struct line *l)
{
	struct admin_handle ah;

	if (g_strcasecmp(l->args[0], "PRIVMSG") && g_strcasecmp(l->args[0], "NOTICE"))
		return TRUE;

	if (g_strcasecmp(l->args[0], n->state->me.nick) == 0) {
		virtual_network_recv_args(c->network, n->state->me.hostmask, l->args[0], l->args[1], NULL);
		return TRUE;
	}

	if (g_strcasecmp(l->args[1], ADMIN_CHANNEL) && 
		g_strcasecmp(l->args[1], "ctrlproxy")) {
		virtual_network_recv_args(c->network, NULL, "401", l->args[1], "No such nick/channel", NULL);
		return TRUE;
	}

	ah.send_fn = network_admin_out;
	ah.user_data = NULL;
	ah.client = c;
	ah.network = n;
	ah.global = n->global;

	return process_cmd(&ah, l->args[2]);
}

struct virtual_network_ops admin_network = {
	"admin", admin_net_init, admin_to_server, NULL
};

void admin_log(enum log_level level, const struct network *n, const struct client *c, const char *data)
{
	extern struct global *my_global;
	struct line *l;
	char *tmp, *hostmask;
	GList *gl;
	static gboolean entered = FALSE;

	if (!my_global || !my_global->config || 
		!my_global->config->admin_log) 
		return;

	if (level < LOG_INFO)
		return;

	if (entered)
		return; /* Prevent inifinite recursion.. */

	entered = TRUE;

	tmp = g_strdup_printf("%s%s%s%s%s%s", 
						  data, 
						  n?" (":"",
						  n?n->name:"", 
						  c?"/":"",
						  c?c->description:"",
						  n?")":"");

	for (gl = my_global->networks; gl; gl = gl->next) {
		struct network *network = gl->data;

		if (network->connection.data.virtual.ops != &admin_network)
			continue;

		hostmask = g_strdup_printf("ctrlproxy!ctrlproxy@%s", network->name);
		l = irc_parse_line_args(hostmask, "PRIVMSG", ADMIN_CHANNEL, tmp, NULL); 
		g_free(hostmask);
		
		virtual_network_recv_line(network, l);

		free_line(l);
	}

	g_free(tmp);

	entered = FALSE;
}



const static struct admin_command builtin_commands[] = {
	{ "ADDNETWORK", add_network, "<name>", "Add new network with specified name" },
	{ "ADDSERVER", add_server, "<network> <host>[:<port>] [<password>]", "Add server to network" },
	{ "BACKLOG", repl_command, "[channel]", "Send backlogs for this network or a channel, if specified" },
	{ "CONNECT", com_connect_network, "<network>", "Connect to specified network. Forces reconnect when waiting." },
	{ "DELNETWORK", del_network, "<network>", "Remove specified network" },
	{ "NEXTSERVER", com_next_server, "[network]", "Disconnect and use to the next server in the list" },
	{ "CHARSET", handle_charset, "<charset>", "Change client charset" },
	{ "DIE", handle_die, "", "Exit ctrlproxy" },
	{ "DISCONNECT", com_disconnect_network, "<network>", "Disconnect specified network" },
	{ "LISTNETWORKS", list_networks, "", "List current networks and their status" },
	{ "SAVECONFIG", com_save_config, "<name>", "Save current XML configuration to specified file" },
	{ "DETACH", detach_client, "", "Detach current client" },
	{ "HELP", help, "[command]", "This help command" },
	{ "DUMPJOINEDCHANNELS", dump_joined_channels, "[network]", NULL, NULL },
#ifdef DEBUG
	{ "ABORT", do_abort, "", NULL, NULL },
#endif
	{ NULL }
};

void init_admin(void) 
{
	int i;
	for(i = 0; builtin_commands[i].name; i++) {
		register_admin_command(&builtin_commands[i]);
	}

	register_virtual_network(&admin_network);

	markers = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)linestack_free_marker);
}

static void iochannel_admin_out(admin_handle h, const char *data)
{
	gsize bytes_written;
	GError *error = NULL;
	GIOStatus status;

	status = g_io_channel_write_chars(h->user_data, data, -1, &bytes_written, &error);

	status = g_io_channel_write_chars(h->user_data, "\n", -1, &bytes_written, &error);

	status = g_io_channel_flush(h->user_data, &error);
}

static gboolean handle_client_data(GIOChannel *channel, 
								  GIOCondition condition, void *_global)
{
	char *raw;
	GError *error = NULL;
	GIOStatus status;
	struct admin_handle ah;
	gsize eol;

	ah.global = _global;
	ah.user_data = channel;
	ah.send_fn = iochannel_admin_out;

	if (condition & G_IO_IN) {
		status = g_io_channel_read_line(channel, &raw, NULL, &eol, &error);
		if (status == G_IO_STATUS_NORMAL) {
			raw[eol] = '\0';
			process_cmd(&ah, raw);
			g_free(raw);
		}
	}

	if (condition & G_IO_HUP) {
		return FALSE;
	}

	if (condition & G_IO_ERR) {
		log_global(LOG_WARNING, "Error from admin client");
		return FALSE;
	}

	return TRUE;
}

static gboolean handle_new_client(GIOChannel *c_server, 
								  GIOCondition condition, void *_global)
{
	GIOChannel *c;
	int sock = accept(g_io_channel_unix_get_fd(c_server), NULL, 0);

	if (sock < 0) {
		log_global(LOG_WARNING, "Error accepting new connection: %s", strerror(errno));
		return TRUE;
	}

	c = g_io_channel_unix_new(sock);

	g_io_channel_set_close_on_unref(c, TRUE);
	g_io_channel_set_encoding(c, NULL, NULL);
	g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	
	g_io_add_watch(c, G_IO_IN | G_IO_ERR | G_IO_HUP, handle_client_data, _global);

	g_io_channel_unref(c);

	return TRUE;
}

gboolean start_admin_socket(struct global *global)
{
	int sock;
	struct sockaddr_un un;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		log_global(LOG_ERROR, "error creating unix socket: %s", strerror(errno));
		return FALSE;
	}
	
	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, global->config->admin_socket, sizeof(un.sun_path));
	unlink(un.sun_path);

	if (bind(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(LOG_ERROR, "unable to bind to %s: %s", un.sun_path, strerror(errno));
		return FALSE;
	}
	
	if (listen(sock, 5) < 0) {
		log_global(LOG_ERROR, "error listening on socket: %s", strerror(errno));
		return FALSE;
	}

	global->admin_incoming = g_io_channel_unix_new(sock);

	if (!global->admin_incoming) {
		log_global(LOG_ERROR, "Unable to create GIOChannel for unix server socket");
		return FALSE;
	}

	g_io_channel_set_close_on_unref(global->admin_incoming, TRUE);

	g_io_channel_set_encoding(global->admin_incoming, NULL, NULL);
	global->admin_incoming_id = g_io_add_watch(global->admin_incoming, G_IO_IN, handle_new_client, global);
	g_io_channel_unref(global->admin_incoming);

	return TRUE;
}

gboolean stop_admin_socket(struct global *global)
{
	if (global->admin_incoming_id > 0)
		g_source_remove(global->admin_incoming_id);
	unlink(global->config->admin_socket);
	return TRUE;
}

gboolean admin_socket_prompt(const char *config_dir)
{
	char *admin_dir = g_build_filename(config_dir, "admin", NULL);
	int sock = socket(PF_UNIX, SOCK_STREAM, 0);
	GIOChannel *ch;
	GError *error = NULL;
	GIOStatus status;
	struct sockaddr_un un;

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, admin_dir, sizeof(un.sun_path));

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		log_global(LOG_ERROR, "unable to connect to %s: %s", un.sun_path, strerror(errno));
		g_free(admin_dir);
		return FALSE;
	}

	ch = g_io_channel_unix_new(sock);
	
#ifdef HAVE_READLINE
	while (1) {
		char *data = readline("ctrlproxy> ");
		char *raw;

		if (data == NULL)
			break;
		
		status = g_io_channel_write_chars(ch, data, -1, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			fprintf(stderr, "Error writing to admin socket: %s\n", error->message);
			return FALSE;
		}

		status = g_io_channel_write_chars(ch, "\n", -1, NULL, &error);
		if (status != G_IO_STATUS_NORMAL) {
			fprintf(stderr, "Error writing to admin socket: %s\n", error->message);
			return FALSE;
		}

		g_io_channel_flush(ch, &error);

		g_free(data);

		while (g_io_channel_read_line(ch, &raw, NULL, NULL, &error) == G_IO_STATUS_NORMAL) 
		{
			printf("%s", raw);
		}
	}
#endif
	g_free(admin_dir);

	return TRUE;
}
