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

static struct ctrlproxyd_config *global_daemon_config;

static GList *daemon_clients = NULL;

/* there are no hup signals here */
void register_hup_handler(hup_handler_fn fn, void *userdata) {}
static void daemon_client_kill(struct daemon_client_data *dc);

struct ctrlproxyd_config {
	const char *port;
	const char *address;
	const char *configdir;
};

struct daemon_client_data {
	GIOChannel *backend_connection;
	GIOChannel *client_connection;
	struct ctrlproxyd_config *config;
	char *password;
	char *servername;
	char *servicename;
	char *nick;
	char *username;
	char *mode;
	char *unused;
	char *realname;
	gint watch_backend;
	gint watch_client;
};

void listener_syslog(enum log_level l, const struct irc_listener *listener, const char *ret)
{
	syslog(LOG_INFO, "%s", ret);
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

gboolean user_running(const char *configdir)
{
	char *path;
	pid_t pid;
	
	path = g_build_filename(configdir, "pid", NULL);

	if (path == NULL)
		return FALSE;

	pid = read_pidfile(path);

	g_free(path);
	
	return (pid != -1);
}

char *user_config_path(struct ctrlproxyd_config *config, const char *username)
{
	struct passwd *pwd;

	if (config->configdir != NULL) {
		return g_build_filename(config->configdir, username, NULL);
	} else {
		pwd = getpwnam(username);

		if (pwd == NULL)
			return NULL;

		return g_build_filename(pwd->pw_dir, ".ctrlproxy", NULL);
	}
}

void signal_quit(int sig)
{
	syslog(LOG_NOTICE, "Received signal %d, exiting...", sig);
	while (daemon_clients != NULL) {
		struct daemon_client_data *dc = daemon_clients->data;
		irc_sendf(dc->client_connection, (GIConv)-1, NULL, "ERROR :Server exiting");
		daemon_client_kill(dc);
	}
	exit(0);
}

static gboolean launch_new_instance(struct ctrlproxyd_config *config, 
								struct irc_listener *l,
								const char *username,
								const char *configdir)
{
	GPid child_pid;
	char *command[] = {
		"ctrlproxy",
		"--daemon",
		"--config-dir", g_strdup(configdir),
		NULL
	};
	GError *error = NULL;

	if (!g_spawn_async(NULL, command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				  &child_pid, &error)) {
		listener_log(LOG_WARNING, l, "Unable to start ctrlproxy for %s: %s", configdir, 
					 error->message);
		return FALSE;
	}

	listener_log(LOG_INFO, l, "Launched new ctrlproxy instance for %s at %s", 
				 username, configdir);

	g_spawn_close_pid(child_pid);

	return TRUE;
}


static GIOChannel *connect_user(struct ctrlproxyd_config *config, 
								struct pending_client *pc,
								const char *username)
{
	char *path;
	int sock;
	struct sockaddr_un un;
	GIOChannel *ch;
	char *user_configdir;

	user_configdir = user_config_path(config, username);
	if (user_configdir == NULL) {
		listener_log(LOG_INFO, pc->listener, "Unable to find user %s", 
					 username);
		irc_sendf(pc->connection, pc->listener->iconv, NULL, "ERROR :Unknown user %s", username);
		return NULL;
	}

	if (!user_running(user_configdir)) {
		if (!launch_new_instance(config, pc->listener, username, user_configdir)) {
			g_free(user_configdir);
			irc_sendf(pc->connection, pc->listener->iconv, NULL, "ERROR :Unable to start ctrlproxy for %s", username);
			return NULL;
		}
	}

	path = g_build_filename(user_configdir, "socket", NULL);

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, path, sizeof(un.sun_path));

	g_free(user_configdir);
	g_free(path);

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		listener_log(LOG_INFO, pc->listener, "unable to connect to %s: %s", un.sun_path, strerror(errno));
		close(sock);
		return NULL;
	}

	ch = g_io_channel_unix_new(sock);
	g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ch, NULL, NULL);
	g_io_channel_set_buffered(ch, FALSE);

	return ch;
}

static void daemon_client_kill(struct daemon_client_data *dc)
{
	daemon_clients = g_list_remove(daemon_clients, dc);

	g_source_remove(dc->watch_client);
	g_source_remove(dc->watch_backend);

	g_io_channel_unref(dc->client_connection);
	g_io_channel_unref(dc->backend_connection);

	g_free(dc->nick);
	g_free(dc->password);
	g_free(dc->realname);
	g_free(dc->servername);
	g_free(dc->servicename);
	g_free(dc->unused);
	g_free(dc->username);
	g_free(dc);
}

static gboolean forward_data(GIOChannel *from, GIOChannel *to)
{
	char buf[1024];
	gsize bytes_read, bytes_written;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_chars(from, buf, sizeof(buf), &bytes_read, &error);
	switch (status) {
		case G_IO_STATUS_NORMAL:
		case G_IO_STATUS_AGAIN:
			break;
		case G_IO_STATUS_ERROR:
		return FALSE;
		case G_IO_STATUS_EOF:
		return FALSE;
		default:
		g_assert_not_reached();
	}

	status = g_io_channel_write_chars(to, buf, bytes_read, &bytes_written, &error);
	switch (status) {
		case G_IO_STATUS_NORMAL:
		case G_IO_STATUS_AGAIN:
			break;
		case G_IO_STATUS_ERROR:
		return FALSE;
		case G_IO_STATUS_EOF:
		return FALSE;
		default:
		g_assert_not_reached();
	}
	g_assert(bytes_read == bytes_written);
	return TRUE;
}

static gboolean daemon_client_handle_client_event(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct daemon_client_data *dc = data;

	if (condition & G_IO_IN) {
		if (!forward_data(dc->client_connection, dc->backend_connection)) {
			daemon_client_kill(dc);
			return FALSE;
		}
	}

	if (condition & G_IO_HUP) {
		daemon_client_kill(dc);
		return FALSE;
	}

	return TRUE;
}

static gboolean daemon_client_handle_backend_event(GIOChannel *source, GIOCondition condition, gpointer data)
{
	struct daemon_client_data *dc = data;

	if (condition & G_IO_IN) {
		if (!forward_data(dc->backend_connection, dc->client_connection)) {
			daemon_client_kill(dc);
			return FALSE;
		}
	}

	if (condition & G_IO_HUP) {
		daemon_client_kill(dc);
		return FALSE;
	}

	return TRUE;
}

/**
 * Callback when client is done authenticating.
 */
static void client_done(struct pending_client *pc, struct daemon_client_data *dc)
{
	char *desc = g_io_channel_ip_get_description(pc->connection);
	listener_log(LOG_INFO, pc->listener, "Accepted new client %s", desc);
	g_free(desc);

	dc->client_connection = g_io_channel_ref(pc->connection);
	g_io_channel_set_encoding(dc->client_connection, NULL, NULL);
	while (g_io_channel_get_buffer_condition(dc->client_connection) & G_IO_IN) {
		if (!daemon_client_handle_client_event(dc->client_connection, 
									  g_io_channel_get_buffer_condition(dc->client_connection), dc)) {
			daemon_client_kill(dc);
			return;
		}
	}
	g_io_channel_set_buffered(dc->client_connection, FALSE);
	g_io_channel_set_flags(dc->client_connection, G_IO_FLAG_NONBLOCK, NULL);
	dc->watch_client = g_io_add_watch(dc->client_connection, G_IO_IN | G_IO_HUP, daemon_client_handle_client_event, dc);
	dc->watch_backend = g_io_add_watch(dc->backend_connection, G_IO_IN | G_IO_HUP, daemon_client_handle_backend_event, dc);

	daemon_clients = g_list_append(daemon_clients, dc);
}

#ifdef HAVE_GSSAPI
static gboolean daemon_socks_gssapi (struct pending_client *pc, gss_name_t username)
{
	struct daemon_client_data *cd = pc->private_data;
	return FALSE; /* FIXME */
}
#endif

static gboolean daemon_socks_auth_simple(struct pending_client *cl, const char *username, const char *password)
{
	struct daemon_client_data *cd = cl->private_data;
	
	cd->backend_connection = connect_user(cd->config, cl, username);
	if (cd->backend_connection == NULL) {
		return FALSE;
	}

	irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "PASS %s", password);

	g_io_channel_flush(cd->backend_connection, NULL);

	/* FIXME: Check whether the client doesn't return anything */

	return TRUE;
}

static gboolean daemon_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	struct daemon_client_data *cd = cl->private_data;

	/* Only called after authentication */

	irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "CONNECT %s %d", hostname, port);
	g_io_channel_flush(cd->backend_connection, NULL);

	client_done(cl, cd);

	return FALSE;
}

static gboolean handle_client_line(struct pending_client *pc, const struct irc_line *l)
{
	struct daemon_client_data *cd = pc->private_data;

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
		cd->backend_connection = connect_user(cd->config, pc, cd->username);
		if (cd->backend_connection == NULL) {
			return FALSE;
		}
		
		irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "PASS %s", cd->password);
		irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "CONNECT %s %s", cd->servername, cd->servicename);
		irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "USER %s %s %s %s", cd->username, 
				  cd->mode, cd->unused, cd->realname);
		irc_sendf(cd->backend_connection, (GIConv)-1, NULL, "NICK %s", cd->nick);
		g_io_channel_flush(cd->backend_connection, NULL);
		client_done(pc, cd);
		return FALSE;
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
	GMainLoop *main_loop;
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

		if (!listener_start(l, config->address, config->port))
			return 1;
	}

	g_main_loop_run(main_loop);

	return 0;
}
