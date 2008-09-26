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

extern char my_hostname[];
static int log_level = 0;

/* there are no hup signals here */
void register_hup_handler(hup_handler_fn fn, void *userdata) {}

struct ctrlproxyd_config {
	const char *port;
	const char *address;
};

struct daemon_client_data {
	GIOChannel *connection;
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
	syslog(LOG_INFO, "%s", ret);
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

	syslog(LOG_ALERT, "BACKTRACE: %ld stack frames:", backtrace_size);
	
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

char *user_socket_path(const char *username)
{
	struct passwd *pwd;

	pwd = getpwnam(username);

	if (pwd == NULL)
		return NULL;

	return g_build_filename(pwd->pw_dir, ".ctrlproxy", "socket", NULL);
}

void signal_quit(int sig)
{
	syslog(LOG_NOTICE, "Received signal %d, exiting...", sig);
	exit(0);
}

static GIOChannel *connect_user(const char *username)
{
	char *path;
	int sock;
	struct sockaddr_un un;
	GIOChannel *ch;

	path = user_socket_path(username);
	if (path == NULL)
		return NULL;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	un.sun_family = AF_UNIX;
	strncpy(un.sun_path, path, sizeof(un.sun_path));

	g_free(path);

	if (connect(sock, (struct sockaddr *)&un, sizeof(un)) < 0) {
		syslog(LOG_INFO, "unable to connect to %s: %s", un.sun_path, strerror(errno));
		/* FIXME: Create new daemon ? */
		return FALSE;
	}

	ch = g_io_channel_unix_new(sock);

	g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);

	return ch;
}

/**
 * Callback when client is done authenticating.
 */
static void client_done(struct pending_client *pc)
{
	printf("DONE!\n");
	/* FIXME: Redirect pc->private_data->connection to pc->connection */

	/* FIXME: perhaps fork() if this is not a inetd server ? */
}

static gboolean daemon_socks_auth_simple(struct pending_client *cl, const char *username, const char *password)
{
	struct daemon_client_data *cd = cl->private_data;
	
	cd->connection = connect_user(username);
	if (cd->connection == NULL)
		return FALSE;

	irc_sendf(cd->connection, (GIConv)-1, NULL, "PASS %s", password);

	g_io_channel_flush(cd->connection, NULL);

	/* FIXME: Check whether the client doesn't return anything */

	return TRUE;
}

static gboolean daemon_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	struct daemon_client_data *cd = cl->private_data;

	/* Only called after authentication */

	irc_sendf(cd->connection, (GIConv)-1, NULL, "CONNECT %s %d", hostname, port);
	g_io_channel_flush(cd->connection, NULL);

	client_done(cl);

	return TRUE;
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
		cd->connection = connect_user(cd->username);
		if (cd->connection == NULL)
			return FALSE;
		
		irc_sendf(cd->connection, (GIConv)-1, NULL, "PASS %s", cd->password);
		irc_sendf(cd->connection, (GIConv)-1, NULL, "CONNECT %s %s", cd->servername, cd->servicename);
		irc_sendf(cd->connection, (GIConv)-1, NULL, "USER %s %s %s %s", cd->username, 
				  cd->mode, cd->unused, cd->realname);
		irc_sendf(cd->connection, (GIConv)-1, NULL, "NICK %s", cd->nick);
		g_io_channel_flush(cd->connection, NULL);
		client_done(pc);
		return FALSE;
	}

	return TRUE;
}

static void daemon_new_client(struct pending_client *pc)
{
	struct daemon_client_data *cd = g_new0(struct daemon_client_data, 1);
	pc->private_data = cd;
}

struct irc_listener_ops daemon_ops = {
	.new_client = daemon_new_client,
	.socks_auth_simple = daemon_socks_auth_simple,
	.socks_connect_fqdn = daemon_socks_connect_fqdn,
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

	config = read_config_file(config_file);
	if (config == NULL) {
		return 1;
	}

	pid = read_pidfile(PIDFILE);
	if (pid != -1) {
		fprintf(stderr, "ctrlproxyd already running at pid %d!\n", pid);
		return 1;
	}

	isdaemon = !foreground && !inetd;

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
