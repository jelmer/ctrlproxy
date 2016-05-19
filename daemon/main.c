/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2006 Jelmer Vernooij <jelmer@jelmer.uk>

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
#include <stdio.h>
#include <glib/gstdio.h>
#define BACKTRACE_STACK_SIZE 64

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifndef HAVE_DAEMON
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "daemon/daemon.h"
#include "daemon/user.h"
#include "daemon/client.h"
#include "daemon/backend.h"

#include "ssl.h"

#include "libirc/socks.h"

struct ctrlproxyd_config;
extern char my_hostname[];
static GMainLoop *main_loop;
static struct irc_listener *daemon_listener;
static gboolean inetd = FALSE;

static struct ctrlproxyd_config *global_daemon_config;

/* there are no hup signals here */
void register_hup_handler(hup_handler_fn fn, void *userdata) {}
static gboolean daemon_client_connect_backend(struct daemon_client *cd, struct pending_client *cl, const char *username);

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
		g_error_free(error);
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

	if (g_key_file_has_key(kf, "settings", "keytab", NULL)) {
		char *keytab;
		keytab = g_key_file_get_string(kf, "settings", "keytab", NULL);
#ifdef HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY
		gsskrb5_register_acceptor_identity(keytab);
#endif
		g_free(keytab);
	}

	if (g_key_file_has_key(kf, "settings", "ssl", NULL))
		config->ssl = g_key_file_get_boolean(kf, "settings", "ssl", NULL);

#ifdef HAVE_GNUTLS
	if (config->ssl)
		config->ssl_credentials = ssl_create_server_credentials(SSL_CREDENTIALS_DIR, kf, "ssl");
#endif

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
	syslog(LOG_ALERT, "Please send a bug report to jelmer@jelmer.uk.");
	syslog(LOG_ALERT, "A gdb backtrace is appreciated if you can reproduce this bug.");
	abort();
}

void signal_quit(int sig)
{
	listener_log(LOG_NOTICE, daemon_listener, "Received signal %d, exiting...", sig);

	g_main_loop_quit(main_loop);
}


static void charset_error_not_called(struct irc_transport *transport, const char *error_msg)
{
	g_assert_not_reached();
}

static gboolean daemon_client_recv(struct irc_transport *transport, const struct irc_line *line)
{
	struct daemon_client *cd = transport->userdata;

	if (cd->backend->authenticated) 
		return daemon_backend_send_line(cd->backend, line);
	else
		return TRUE;
}

static void on_daemon_client_disconnect(struct irc_transport *transport)
{
	struct daemon_client *cd = transport->userdata;

	listener_log(LOG_INFO, cd->listener, "Client %s disconnected", cd->description);

	daemon_client_kill(cd);
}

static gboolean daemon_client_error(struct irc_transport *transport, const char *message)
{
	struct daemon_client *dc = transport->userdata;
	daemon_client_kill(dc);
	return FALSE;
}

static const struct irc_transport_callbacks daemon_client_callbacks = {
	.hangup = irc_transport_disconnect,
	.log = NULL,
	.disconnect = on_daemon_client_disconnect,
	.recv = daemon_client_recv,
	.charset_error = charset_error_not_called,
	.error = daemon_client_error,
};

#ifdef HAVE_GSSAPI
static gboolean daemon_socks_gssapi (struct pending_client *pc, gss_name_t username)
{
	struct daemon_client *cd = pc->private_data;
	guint32 major_status, minor_status;
	gss_buffer_desc namebuf;

	namebuf.value = NULL;
	namebuf.length = 0;

	major_status = gss_export_name(&minor_status, username, &namebuf);
	if (GSS_ERROR(major_status)) {
		log_gssapi(pc->listener, LOG_WARNING, 
				   "releasing name", major_status, minor_status);
		return FALSE;
	}

	if (!daemon_client_connect_backend(cd, pc, namebuf.value))
		return FALSE;
	major_status = gss_release_buffer(&minor_status, &namebuf);

	/* FIXME: Let the backend know somehow authentication is already done */

	return TRUE; 
}
#endif

static gboolean backend_error(struct daemon_backend *backend, const char *error_message)
{
	struct daemon_client *cd = backend->userdata;
	listener_log(LOG_INFO, cd->listener, "%s", error_message);
	if (cd->client_transport != NULL)
		transport_send_args(cd->client_transport, NULL, "ERROR", error_message, NULL);
	daemon_client_kill(cd);
	return FALSE;
}

static void backend_disconnect(struct daemon_backend *backend)
{
	struct daemon_client *cd = backend->userdata;

	listener_log(LOG_INFO, cd->listener, "Backend of %s disconnected", cd->description);

	daemon_client_kill(cd);
}

static gboolean backend_recv(struct daemon_backend *backend, const struct irc_line *line)
{
	struct daemon_client *cd = backend->userdata;

	g_assert(line != NULL);

	if (cd->client_transport != NULL)
		return transport_send_line(cd->client_transport, line, NULL);
	return TRUE;
}

const static struct daemon_backend_callbacks backend_callbacks = {
	.disconnect = backend_disconnect,
	.error = backend_error,
	.recv = backend_recv,
};

static gboolean daemon_client_connect_backend(struct daemon_client *cd, struct pending_client *cl, const char *username)
{
	daemon_user_free(cd->user);
	cd->user = get_daemon_user(cd->config, username);
	if (cd->user == NULL) {
		listener_log(LOG_INFO, cd->listener, "Unable to find user %s", username);
		irc_sendf(cl->connection, cl->listener->iconv, NULL, "ERROR :Unknown user %s", username);
		return FALSE;
	}

	if (!daemon_user_running(cd->user)) {
		if (!daemon_user_start(cd->user, cd->config->ctrlproxy_path, cd->listener)) {
			irc_sendf(cl->connection, cl->listener->iconv, NULL, "ERROR :Unable to start ctrlproxy for %s", 
					  cd->user->username);
			return FALSE;
		}
	}

	cd->backend = daemon_backend_open(cd->user->socketpath, &backend_callbacks, cd, cl->listener);
	if (cd->backend == NULL)
		return FALSE;

	return TRUE;
}

static void daemon_backend_pass_checked(struct daemon_backend *backend, gboolean accepted)
{
	struct daemon_client *cd = backend->userdata;

	cd->socks_accept_fn(cd->pending_client, accepted);
}


static gboolean daemon_socks_auth_simple(struct pending_client *cl, const char *username, const char *password,
										 gboolean (*on_finished) (struct pending_client *, gboolean))
{
	struct daemon_client *cd = cl->private_data;
	
	if (!daemon_client_connect_backend(cd, cl, username))
		return FALSE;

	cd->socks_accept_fn = on_finished;

	return daemon_backend_authenticate(cd->backend, password, daemon_backend_pass_checked);
}

static gboolean daemon_socks_connect_fqdn (struct pending_client *cl, const char *hostname, uint16_t port)
{
	struct daemon_client *cd = cl->private_data;
	char portstr[20];
	char data[0x200];

	/* Only called after authentication */

	cd->description = g_io_channel_ip_get_description(cl->connection);
	listener_log(LOG_INFO, cl->listener, "Accepted new socks client %s for user %s", cd->description, cd->user->username);

	cd->client_transport = irc_transport_new_iochannel(cl->connection);
	irc_transport_set_callbacks(cd->client_transport, &daemon_client_callbacks, cd);

	snprintf(portstr, sizeof(portstr), "%d", port);
	transport_send_args(cd->backend->transport, NULL, "CONNECT", hostname, portstr, NULL);

	g_assert(strlen(hostname) < 0x100);

	data[0] = strlen(hostname);
	strncpy(data+1, hostname, data[0]);

	listener_socks_reply(cl, REP_OK, ATYP_FQDN, strlen(hostname)+2, data, port); 
	return FALSE;
}

static void plain_handle_auth_finish(struct daemon_backend *backend, gboolean accepted)
{
	struct daemon_client *dc = (struct daemon_client *)backend->userdata;

	if (!accepted) {
		transport_send_response(dc->client_transport, NULL, get_my_hostname(), "*", 
								ERR_PASSWDMISMATCH, "Password invalid", NULL);
		daemon_client_kill(dc);
	} else {
		daemon_client_forward_credentials(dc);
	}
}

static gboolean handle_client_line(struct pending_client *pc, const struct irc_line *l)
{
	struct daemon_client *cd = pc->private_data;

	if (l == NULL || l->args[0] == NULL) { 
		return TRUE;
	}

	if (!base_strcmp(l->args[0], "PASS")) {
		cd->login_details->password = g_strdup(l->args[1]);
	} else if (!base_strcmp(l->args[0], "CONNECT")) {
		cd->servername = g_strdup(l->args[1]);
		cd->servicename = g_strdup(l->args[2]);
	} else if (!base_strcmp(l->args[0], "USER") && l->argc > 4) {
		cd->login_details->username = g_strdup(l->args[1]);
		cd->login_details->mode = g_strdup(l->args[2]);
		cd->login_details->unused = g_strdup(l->args[3]);
		cd->login_details->realname = g_strdup(l->args[4]);
	} else if (!base_strcmp(l->args[0], "NICK")) {
		cd->login_details->nick = g_strdup(l->args[1]);
	} else if (!base_strcmp(l->args[0], "QUIT")) {
		return FALSE;
	} else {
		irc_sendf(pc->connection, pc->listener->iconv, NULL, ":%s %d %s :You are not registered. Did you specify a password?", 
				  get_my_hostname(), ERR_NOTREGISTERED, "*");
		g_io_channel_flush(pc->connection, NULL);
	}

	if (cd->login_details->username != NULL && cd->login_details->password != NULL && cd->login_details->nick != NULL) {
		if (!daemon_client_connect_backend(cd, pc, cd->login_details->username))
			return FALSE;

		cd->description = g_io_channel_ip_get_description(pc->connection);
		listener_log(LOG_INFO, pc->listener, "Accepted new client %s for user %s", cd->description, cd->user->username);

		cd->client_transport = irc_transport_new_iochannel(pc->connection);

		daemon_backend_authenticate(cd->backend, cd->login_details->password, plain_handle_auth_finish);

		irc_transport_set_callbacks(cd->client_transport, &daemon_client_callbacks, cd);

		return FALSE;
	}

	return TRUE;
}

static void daemon_new_client(struct pending_client *pc)
{
	struct daemon_client *cd = g_new0(struct daemon_client, 1);
	cd->login_details = g_new0(struct irc_login_details, 1);
	cd->pending_client = pc;
	cd->listener = pc->listener;
	cd->config = global_daemon_config;
	pc->private_data = cd;
	cd->inetd = inetd;
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

static void daemon_user_start_if_exists(struct daemon_user *user, const char *ctrlproxy_path, 
										struct irc_listener *listener)
{
	if (daemon_user_exists(user) && !daemon_user_running(user)) {
		daemon_user_start(user, ctrlproxy_path, listener);
	}
}

int main(int argc, char **argv)
{
	struct ctrlproxyd_config *config;
	GOptionContext *pc;
	const char *config_file = DEFAULT_CONFIG_FILE;
	gboolean version = FALSE;
	gboolean foreground = FALSE;
	gboolean isdaemon = TRUE;
	pid_t pid;
	GOptionEntry options[] = {
		{"config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file, "Configuration file", "CONFIGFILE"},
		{"foreground", 'F', 0, G_OPTION_ARG_NONE, &foreground, ("Stay in the foreground") },
		{"inetd", 'I', 0, G_OPTION_ARG_NONE, &inetd, ("Run in inetd mode")},
		{"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("Show version information")},
		{ NULL }
	};
	GError *error = NULL;
	daemon_listener = g_new0(struct irc_listener, 1);

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
		g_error_free(error);
		return 1;
	}

	g_option_context_free(pc);

	if (version) {
		printf("ctrlproxy %s\n", VERSION);
		printf("(c) 2002-2009 Jelmer Vernooij et al. <jelmer@jelmer.uk>\n");
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
#if defined(HAVE_DAEMON) || defined(HAVE_FORK)
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

	daemon_listener->iconv = (GIConv)-1;
	if (foreground) 
		daemon_listener->log_fn = listener_stderr;
	else 
		daemon_listener->log_fn = listener_syslog;
	daemon_listener->ops = &daemon_ops;
	daemon_listener->ssl = config->ssl;
	daemon_listener->ssl_credentials = config->ssl_credentials;

	if (inetd) {
		GIOChannel *io = g_io_channel_unix_new(0);
		listener_new_pending_client(daemon_listener, io);
	} else { 
		write_pidfile(PIDFILE);

		if (!listener_start_tcp(daemon_listener, config->address, config->port))
			return 1;
	}

	foreach_daemon_user(config, daemon_listener, daemon_user_start_if_exists);

	g_main_loop_run(main_loop);

	daemon_clients_exit();

	return 0;
}
