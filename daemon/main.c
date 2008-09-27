/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "internals.h"
#include "irc.h"
#include <sys/un.h>
#include <pwd.h>
#include <glib/gstdio.h>
#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

struct ctrlproxyd_config;
struct daemon_client_data;
extern char my_hostname[];
static int log_level = 0;
static GMainLoop *main_loop;

static struct ctrlproxyd_config *global_daemon_config;

static GList *daemon_clients = NULL;

/* there are no hup signals here */
void register_hup_handler(hup_handler_fn fn, void *userdata) {}
static void daemon_client_kill(struct daemon_client_data *dc);
static gboolean daemon_client_check_pass(struct pending_client *pc, struct daemon_client_data *cd, const char *password,
										 gboolean (*) (struct pending_client *, gboolean));

struct ctrlproxyd_config {
	char *ctrlproxy_path;
	char *port;
	char *address;
	char *configdir;
};

struct daemon_client_user {
	 char *configdir;
	 char *pidpath;
	 char *socketpath;
	 char *username;
	 uid_t uid;
};

struct daemon_client_data {
	struct irc_transport *backend_transport;
	struct irc_transport *client_transport;
	gboolean (*pass_check_cb) (struct pending_client *, gboolean);
	struct pending_client *pending_client;
	struct daemon_client_user *user;
	struct ctrlproxyd_config *config;
	char *password;
	char *servername;
	char *servicename;
	char *nick;
	char *username;
	char *mode;
	char *unused;
	char *realname;
};

void listener_syslog(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	int syslog_level = -1;
	switch (l) {
	case 5: break;
	case 4: l = LOG_DEBUG; break;
	case 3: l = LOG_INFO; break;
	case 2: l = LOG_WARNING; break;
	case 1: l = LOG_ERR; break;
	}
	if (syslog_level == -1)
		return;
	syslog(syslog_level, "%s", ret);
}

void listener_stderr(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	fprintf(stderr, "[%d] %s\n",l, ret);
}

struct ctrlproxyd_config *read_config_file(const char *name)
{
	struct ctrlproxyd_config *config;
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, name, G_KEY_FILE_NONE, &error)) {
		fprintf(stderr, "Unable to load '%s': %s\n", name, error->message);
		g_key_file_free(kf);
		return NULL;
	}

	config = g_new0(struct ctrlproxyd_config, 1);
	if (g_key_file_has_key(kf, "settings", "ctrlproxy-path", NULL)) {
		config->ctrlproxy_path = g_key_file_get_string(kf, "settings", "ctrlproxy-path", NULL);
	} else {
		config->ctrlproxy_path = g_strdup("ctrlproxy");
	}
	config->configdir = g_key_file_get_string(kf, "settings", "configdir", NULL);
	config->port = g_key_file_get_string(kf, "settings", "port", NULL);
	config->address = g_key_file_get_string(kf, "settings", "address", NULL);

	g_key_file_free(kf);

	return config;
}

void signal_crash(int sig)
{
#ifdef HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;
#endif
	syslog(LOG_ERR, "Received SIGSEGV!");

#ifdef HAVE_BACKTRACE_SYMBOLS
	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	syslog(LOG_ALERT, "BACKTRACE: %ld stack frames:", (long)backtrace_size);
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			syslog(LOG_ALERT, " #%u %s", i, backtrace_strings[i]);

		g_free(backtrace_strings);
	}

#endif
	syslog(LOG_ALERT, "Please send a bug report to jelmer@samba.org.");
	syslog(LOG_ALERT, "A gdb backtrace is appreciated if you can reproduce this bug.");
	abort();
}

gboolean daemon_user_exists(struct daemon_client_user *user)
{
	return g_file_test(user->configdir, G_FILE_TEST_IS_DIR);
}

gboolean daemon_user_running(struct daemon_client_user *user)
{
	pid_t pid;
	
	pid = read_pidfile(user->pidpath);
	
	return (pid != -1);
}

void daemon_user_free(struct daemon_client_user *user)
{
	if (user == NULL)
		return;
	g_free(user->pidpath);
	g_free(user->socketpath);
	g_free(user->configdir);
	g_free(user->username);
	g_free(user);
}

struct daemon_client_user *daemon_user(struct ctrlproxyd_config *config, const char *username)
{
	struct daemon_client_user *user = g_new0(struct daemon_client_user, 1);
	struct passwd *pwd;

	user->username = g_strdup(username);

	if (config->configdir != NULL) {
		user->configdir = g_build_filename(config->configdir, username, NULL);
		user->uid = -1;
	} else {
		pwd = getpwnam(username);

		if (pwd == NULL) {
			g_free(user->username);
			g_free(user);
			return NULL;
		}

		user->configdir = g_build_filename(pwd->pw_dir, ".ctrlproxy", NULL);
		user->uid = pwd->pw_uid;
	}

	user->socketpath = g_build_filename(user->configdir, "socket", NULL);
	user->pidpath = g_build_filename(user->configdir, "pid", NULL);
	return user;
}

void signal_quit(int sig)
{
	syslog(LOG_NOTICE, "Received signal %d, exiting...", sig);

	g_main_loop_quit(main_loop);
}

struct spawn_data {
	struct daemon_client_user *user;
	struct irc_listener *listener;
};

static void user_setup(gpointer user_data)
{
	struct spawn_data *data = user_data;
	if (seteuid(data->user->uid) < 0) {
		listener_log(LOG_WARNING, data->listener, "Unable to change effective user id to %s (%d): %s", 
					 data->user->username, data->user->uid, 
					 strerror(errno));
		exit(1);
	}	
}

static gboolean daemon_user_start(struct ctrlproxyd_config *config, 
								struct irc_listener *l,
								struct daemon_client_user *user)
{
	GPid child_pid;
	char *command[] = {
		config->ctrlproxy_path,
		"--daemon",
		"--config-dir", g_strdup(user->configdir),
		NULL
	};
	GError *error = NULL;
	struct spawn_data spawn_data;
	spawn_data.listener = l;
	spawn_data.user = user;

	if (!g_spawn_async(NULL, command, NULL, G_SPAWN_SEARCH_PATH, user_setup, &spawn_data,
				  &child_pid, &error)) {
		listener_log(LOG_WARNING, l, "Unable to start ctrlproxy for %s (%s): %s", user->username, 
					 user->configdir, error->message);
		return FALSE;
	}

	listener_log(LOG_INFO, l, "Launched new ctrlproxy instance for %s at %s", 
				 user->username, user->configdir);

	g_spawn_close_pid(child_pid);

	return TRUE;
}


static GIOChannel *connect_user(struct ctrlproxyd_config *config, 
								struct pending_client *pc,
								struct daemon_client_user *user)
{
	int sock;
	struct sockaddr_un un;
	GIOChannel *ch;

	if (!daemon_user_running(user)) {
		if (!daemon_user_start(config, pc->listener, user)) {
			daemon_user_free(user);
			irc_sendf(pc->connection, pc->listener->iconv, NULL, "ERROR :Unable to start ctrlproxy for %s", 
					  user->username);
			return NULL;
		}
	}

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, user->socketpath, sizeof(un.sun_path));

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		listener_log(LOG_INFO, pc->listener, "unable to connect to %s: %s", un.sun_path, strerror(errno));
		close(sock);
		return NULL;
	}

	ch = g_io_channel_unix_new(sock);

	return ch;
}

static void daemon_client_kill(struct daemon_client_data *dc)
{
	daemon_clients = g_list_remove(daemon_clients, dc);

	if (dc->backend_transport != NULL) {
		irc_transport_disconnect(dc->backend_transport);
		free_irc_transport(dc->backend_transport);
	}

	if (dc->client_transport != NULL) {
		irc_transport_disconnect(dc->client_transport);
		free_irc_transport(dc->client_transport);
	}

	daemon_user_free(dc->user);
	g_free(dc->nick);
	g_free(dc->password);
	g_free(dc->realname);
	g_free(dc->servername);
	g_free(dc->servicename);
	g_free(dc->unused);
	g_free(dc->username);
	g_free(dc);
}

static void charset_error_not_called(struct irc_transport *transport, const char *error_msg)
{
	g_assert_not_reached();
}

static gboolean daemon_client_recv(struct irc_transport *transport, const struct irc_line *line)
{
	struct daemon_client_data *cd = transport->userdata;

	return transport_send_line(cd->backend_transport, line);
}

static void daemon_client_hangup (struct irc_transport *transport)
{
	struct daemon_client_data *cd = transport->userdata;

	daemon_client_kill(cd);
}

static void daemon_client_disconnect(struct irc_transport *transport)
{
}

static const struct irc_transport_callbacks daemon_client_callbacks = {
	.hangup = daemon_client_hangup,
	.log = NULL,
	.disconnect = daemon_client_disconnect,
	.recv = daemon_client_recv,
	.charset_error = charset_error_not_called,
	.error = NULL,
};

static gboolean daemon_backend_recv(struct irc_transport *transport, const struct irc_line *line)
{
	struct daemon_client_data *cd = transport->userdata;

	if (cd->pass_check_cb != NULL) {
		gboolean ok;
		if (atoi(line->args[0]) == ERR_PASSWDMISMATCH) {
			ok = FALSE;
		} else if (!strcmp(line->args[0], "NOTICE") && !strcmp(line->args[1], "AUTH") && !strcmp(line->args[2], "PASS OK")) {
			cd->pass_check_cb(cd->pending_client, TRUE);
			ok = TRUE;
		} else {
			listener_log(LOG_INFO, cd->pending_client->listener, "Unexpected response %s", line->args[0]);
			transport_send_args(cd->client_transport, "ERROR", "Internal error, unexpected response", NULL);
			ok = FALSE;
		}
		cd->pass_check_cb(cd->pending_client, ok);
		cd->pass_check_cb = NULL;
		cd->pending_client = NULL;
		return ok;
	}

	return transport_send_line(cd->client_transport, line);
}

static const struct irc_transport_callbacks daemon_backend_callbacks = {
	.hangup = daemon_client_hangup,
	.log = NULL,
	.disconnect = daemon_client_disconnect,
	.recv = daemon_backend_recv,
	.charset_error = charset_error_not_called,
	.error = NULL,
};

/**
 * Callback when client is done authenticating.
 */
static void client_done(struct pending_client *pc, struct daemon_client_data *dc)
{
	char *desc;
	
	if (dc->servername != NULL && dc->servicename != NULL)
		transport_send_args(dc->backend_transport, "CONNECT", dc->servername, dc->servicename, NULL);
	transport_send_args(dc->backend_transport, "USER", dc->username, dc->mode, dc->unused, dc->realname, NULL);
	transport_send_args(dc->backend_transport, "NICK", dc->nick, NULL);

	desc = g_io_channel_ip_get_description(pc->connection);
	listener_log(LOG_INFO, pc->listener, "Accepted new client %s for user %s", desc, dc->user->username);
	g_free(desc);

	dc->client_transport = irc_transport_new_iochannel(pc->connection);

	irc_transport_set_callbacks(dc->client_transport, &daemon_client_callbacks, dc);

	transport_parse_buffer(dc->client_transport);

	daemon_clients = g_list_append(daemon_clients, dc);
}

#ifdef HAVE_GSSAPI
static gboolean daemon_socks_gssapi (struct pending_client *pc, gss_name_t username)
{
	struct daemon_client_data *cd = pc->private_data;
	return FALSE; /* FIXME */
}
#endif

static gboolean daemon_socks_auth_simple(struct pending_client *cl, const char *username, const char *password,
										 gboolean (*on_finished) (struct pending_client *, gboolean))
{
	struct daemon_client_data *cd = cl->private_data;
	GIOChannel *ioc;
	
	daemon_user_free(cd->user);
	cd->user = daemon_user(cd->config, username);
	if (cd->user == NULL) {
		listener_log(LOG_INFO, cl->listener, "Unable to find user %s", username);
		irc_sendf(cl->connection, cl->listener->iconv, NULL, "ERROR :Unknown user %s", username);
		return FALSE;
	}

	ioc = connect_user(cd->config, cl, cd->user);
	if (ioc == NULL) {
		return FALSE;
	}

	cd->backend_transport = irc_transport_new_iochannel(ioc);
	if (cd->backend_transport == NULL) {
		return FALSE;
	}

	irc_transport_set_callbacks(cd->backend_transport, &daemon_backend_callbacks, cd);

	return daemon_client_check_pass(cl, cd, password, on_finished);
}

static gboolean daemon_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	struct daemon_client_data *cd = cl->private_data;

	/* Only called after authentication */

	cd->servername = g_strdup(hostname);
	cd->servicename = g_strdup_printf("%d", port);

	client_done(cl, cd);

	return FALSE;
}

static gboolean daemon_client_check_pass(struct pending_client *cl, struct daemon_client_data *cd, const char *password,
										 gboolean (*pass_check_cb) (struct pending_client *, gboolean))
{
	cd->pass_check_cb = pass_check_cb;
	cd->pending_client = cl;

	if (!transport_send_args(cd->backend_transport, "PASS", password, NULL))
		return FALSE;

	return TRUE; /* All good, so far */
}

static gboolean clear_handle_auth_finish(struct pending_client *pc, gboolean accepted)
{
	if (!accepted) {
		irc_sendf(pc->connection, pc->listener->iconv, NULL, 
				  ":%s %d %s :Password invalid", 
				  get_my_hostname(), ERR_PASSWDMISMATCH, "*");
		g_io_channel_flush(pc->connection, NULL);

		return FALSE;
	} else {
		client_done(pc, pc->private_data);
		return FALSE;
	}
}

static gboolean handle_client_line(struct pending_client *pc, const struct irc_line *l)
{
	struct daemon_client_data *cd = pc->private_data;
	GIOChannel *ioc;

	if (l == NULL || l->args[0] == NULL) { 
		return TRUE;
	}

	if (!g_strcasecmp(l->args[0], "PASS")) {
		cd->password = g_strdup(l->args[1]);
	} else if (!g_strcasecmp(l->args[0], "CONNECT")) {
		cd->servername = g_strdup(l->args[1]);
		cd->servicename = g_strdup(l->args[2]);
	} else if (!g_strcasecmp(l->args[0], "USER") && l->argc > 4) {
		cd->username = g_strdup(l->args[1]);
		cd->mode = g_strdup(l->args[2]);
		cd->unused = g_strdup(l->args[3]);
		cd->realname = g_strdup(l->args[4]);
	} else if (!g_strcasecmp(l->args[0], "NICK")) {
		cd->nick = g_strdup(l->args[1]);
	} else if (!g_strcasecmp(l->args[0], "QUIT")) {
		return FALSE;
	} else {
		irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :You are not registered. Did you specify a password?", 
				  get_my_hostname(), ERR_NOTREGISTERED, "*");
		g_io_channel_flush(pc->connection, NULL);
	}

	if (cd->username != NULL && cd->password != NULL && cd->nick != NULL) {
		daemon_user_free(cd->user);
		cd->user = daemon_user(cd->config, cd->username);
		if (cd->user == NULL) {
			listener_log(LOG_INFO, pc->listener, "Unable to find user %s", 
						 cd->username);
			irc_sendf(pc->connection, pc->listener->iconv, NULL, "ERROR :Unknown user %s", cd->username);
			return FALSE;
		}

		ioc = connect_user(cd->config, pc, cd->user);
		if (ioc == NULL) {
			return FALSE;
		}

		cd->backend_transport = irc_transport_new_iochannel(ioc);
		if (cd->backend_transport == NULL) {
			return FALSE;
		}
		irc_transport_set_callbacks(cd->backend_transport, &daemon_backend_callbacks, cd);

		return daemon_client_check_pass(pc, cd, cd->password, clear_handle_auth_finish);
	}

	return TRUE;
}

static void daemon_new_client(struct pending_client *pc)
{
	struct daemon_client_data *cd = g_new0(struct daemon_client_data, 1);
	cd->config = global_daemon_config;
	pc->private_data = cd;
}

struct irc_listener_ops daemon_ops = {
	.new_client = daemon_new_client,
	.socks_auth_simple = daemon_socks_auth_simple,
	.socks_connect_fqdn = daemon_socks_connect_fqdn,
#ifdef HAVE_GSSAPI
	.socks_gssapi = daemon_socks_gssapi,
#endif
	.handle_client_line = handle_client_line,
};

static void daemon_user_start_if_exists(struct ctrlproxyd_config *config, struct irc_listener *listener, 
										struct daemon_client_user *user)
{
	if (daemon_user_exists(user) && !daemon_user_running(user)) {
		daemon_user_start(config, listener, user);
	}
}


static void foreach_daemon_user(struct ctrlproxyd_config *config, struct irc_listener *listener, 
								void (*fn) (struct ctrlproxyd_config *config, struct irc_listener *l, struct daemon_client_user *user))
{
	struct daemon_client_user *user;

	if (config->configdir == NULL) {
		struct passwd *pwent;
		setpwent();
		while ((pwent = getpwent()) != NULL) {
			user = daemon_user(config, pwent->pw_name);
			fn(config, listener, user);
		}
		endpwent();
	} else {
		const char *name;
		GDir *dir = g_dir_open(config->configdir, 0, NULL);
		if (dir == NULL)
			return;
		while ((name = g_dir_read_name(dir)) != NULL) {
			user = daemon_user(config, name);
			fn(config, listener, user);
		}
		g_dir_close(dir);
	}
}

int main(int argc, char **argv)
{
	struct ctrlproxyd_config *config;
	GOptionContext *pc;
	const char *config_file = DEFAULT_CONFIG_FILE;
	gboolean version = FALSE;
	gboolean inetd = FALSE;
	gboolean foreground = FALSE;
	gboolean isdaemon = TRUE;
	pid_t pid;
	GOptionEntry options[] = {
		{"config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file, "Configuration file", "CONFIGFILE"},
		{"debug-level", 'd', 'd', G_OPTION_ARG_INT, &log_level, ("Debug level [0-5]"), "LEVEL" },
		{"foreground", 'F', 0, G_OPTION_ARG_NONE, &foreground, ("Stay in the foreground") },
		{"inetd", 'I', 0, G_OPTION_ARG_NONE, &inetd, ("Run in inetd mode")},
		{"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("Show version information")},
		{ NULL }
	};
	GError *error = NULL;
	struct irc_listener *l = g_new0(struct irc_listener, 1);

	signal(SIGINT, signal_quit);
	signal(SIGTERM, signal_quit);
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGHUP
	signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGSEGV
	signal(SIGSEGV, signal_crash);
#endif

	main_loop = g_main_loop_new(NULL, FALSE);

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);

	if (!g_option_context_parse(pc, &argc, &argv, &error)) {
		fprintf(stderr, "%s\n", error->message);
		return 1;
	}

	if (version) {
		printf("ctrlproxy %s\n", VERSION);
		printf("(c) 2002-2008 Jelmer Vernooij et al. <jelmer@nl.linux.org>\n");
		return 0;
	}

	global_daemon_config = config = read_config_file(config_file);
	if (config == NULL) {
		return 1;
	}

	pid = read_pidfile(PIDFILE);
	if (pid != -1) {
		fprintf(stderr, "ctrlproxyd already running at pid %d!\n", pid);
		return 1;
	}

	isdaemon = (!foreground && !inetd);

	if (gethostname(my_hostname, NI_MAXHOST) != 0) {
		fprintf(stderr, "Can't figure out hostname of local host!\n");
		return 1;
	}

	if (isdaemon) {
#ifdef HAVE_DAEMON 
#ifdef SIGTTOU
		signal(SIGTTOU, SIG_IGN);
#endif

#ifdef SIGTTIN
		signal(SIGTTIN, SIG_IGN);
#endif

#ifdef SIGTSTP
		signal(SIGTSTP, SIG_IGN);
#endif
		if (daemon(1, 0) < 0) {
			fprintf(stderr, "Unable to daemonize\n");
			return -1;
		}
#else
		fprintf(stderr, "Daemon mode not compiled in\n");
		return -1;
#endif
	}

	openlog("ctrlproxyd", 0, LOG_DAEMON);

	l->iconv = (GIConv)-1;
	if (foreground) 
		l->log_fn = listener_stderr;
	else 
		l->log_fn = listener_syslog;
	l->ops = &daemon_ops;

	if (inetd) {
		GIOChannel *io = g_io_channel_unix_new(0);
		listener_new_pending_client(l, io);
	} else { 
		write_pidfile(PIDFILE);

		if (!listener_start_tcp(l, config->address, config->port))
			return 1;
	}

	foreach_daemon_user(config, l, daemon_user_start_if_exists);

	g_main_loop_run(main_loop);

	while (daemon_clients != NULL) {
		struct daemon_client_data *dc = daemon_clients->data;
		transport_send_args(dc->client_transport, "ERROR", "Server exiting", NULL);
		daemon_client_kill(dc);
	}

	return 0;
}
