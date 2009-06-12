#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "line.h"
#include "linestack.h"
#include "cache.h"
#include "settings.h"
#include "state.h"
#include <readline/readline.h>

static struct linestack_context *ctx = NULL;
static struct irc_network_state *state = NULL;
static struct ctrlproxy_config *cfg = NULL;

static void handle_help(int, char **);
static void handle_mark(int, char **);
static void handle_replay(int, char **);
static void handle_state(int, char **);
static void handle_exit(int, char **);
static void handle_insert(int, char **);
static GHashTable *markers = NULL;

/* there are no hup signals here */
void register_hup_handler(void *fn, void *data) {}

/**
 * Shell command.
 */
struct cmd {
	const char *name;
	void (*handler) (int argc, char **);
} cmds[] = {
	{ "help", handle_help },
	{ "mark", handle_mark },
	{ "replay", handle_replay },
	{ "state", handle_state },
	{ "exit", handle_exit },
	{ "insert", handle_insert },
};


static gboolean line_printer (struct irc_line *l, time_t time, void *f) 
{
	fprintf((FILE *)f, "[%lu] %s\n", time, irc_line_string(l));
	return TRUE;
}

static void handle_state(int argc, char **argv)
{
	struct linestack_marker *pos;
	struct irc_network_state *state;
	GList *gl;

	if (argc > 2) {
		fprintf(stderr, "Usage: state [<pos>]\n");
		return;
	}

	if (argv[1] != NULL) {
		pos = g_hash_table_lookup(markers, argv[1]);

		if (pos == NULL) {
			fprintf(stderr, "Unable to find marker '%s'\n", argv[1]);
			return;
		}
	} else {
		pos = NULL;
	}

	state = linestack_get_state(ctx, pos);
	if (state == NULL) {
		fprintf(stderr, "Unable to retrieve network state for marker '%s'\n", argv[1]);
		return;
	}

	printf("Channels:\n");

	for (gl = state->channels; gl; gl = gl->next) {
		struct irc_channel_state *cs = gl->data;
		printf("\t%s\n", cs->name);
	}
}

static void handle_replay(int argc, char **argv)
{
	struct linestack_marker *from, *to = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: replay <from> [<to>]\n");
		return;
	}

	from = g_hash_table_lookup(markers, argv[1]);

	if (from == NULL) {
		fprintf(stderr, "Unable to find marker '%s'\n", argv[1]);
		return;
	}

	if (argv[2] != NULL)
		to = g_hash_table_lookup(markers, argv[2]);

	linestack_traverse(ctx, from, to, line_printer, stdout);
}

static void handle_help(int argc, char **argv)
{
	fprintf(stderr, "Available commands:\n"
					"help\n"
					"insert\n"
					"mark\n"
					"state\n"
					"replay\n"
					"exit\n");
}

static void handle_mark(int argc, char **argv)
{
	struct linestack_marker *marker;

	if (argc < 2) {
		fprintf(stderr, "Usage: mark <name>\n");
		return;
	}

	marker = linestack_get_marker(ctx);

	g_assert(marker != NULL);

	fprintf(stderr, "Marker '%s' added\n", argv[1]);

	g_hash_table_insert(markers, g_strdup(argv[1]), marker);
}

static void handle_insert(int argc, char **argv)
{
	struct irc_line *l;
	enum data_direction direction;

	if (argc < 2) {
		fprintf(stderr, "Usage: insert <client|server> <line>\n");
		return;
	}

	if (!g_strcasecmp(argv[1], "client")) 
		direction = TO_SERVER;
	else if (!g_strcasecmp(argv[1], "server"))
		direction = FROM_SERVER;
	else {
		fprintf(stderr, "Unable to parse direction `%s'\n", argv[1]);
		return;
	}
	
	l = irc_parse_line(argv[2]);
	if (l == NULL) {
		fprintf(stderr, "Error parsing line\n");
		return;
	}

	linestack_insert_line(ctx, l, direction, state);
}

static void handle_exit(int argc, char **argv)
{
	exit(0);
}

static void freels(void)
{
	free_linestack_context(ctx);
}

int main(int argc, char **argv)
{
	GOptionContext *pc;
	char *line;
	const struct linestack_ops *ops;
	struct irc_network_info *info;
	char *config_dir = NULL;
	char *backend = NULL;
	GOptionEntry options[] = {
		{"backend", 'b', 0, G_OPTION_ARG_STRING, &backend, "Backend to use [default: file]" },
		{"config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, ("Override configuration directory"), ("DIR")},
		{ NULL }
	};

	pc = g_option_context_new("");
	g_option_context_add_main_entries(pc, options, NULL);
	if(!g_option_context_parse(pc, &argc, &argv, NULL))
		return 1;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <name>\n", argv[0]);
		return 1;
	}

	info = g_new0(struct irc_network_info, 1);

	state = network_state_init("nick", "username", "hostname");

	g_assert(state != NULL);

	if (backend != NULL) {
		ops = linestack_find_ops(backend);
		if (ops == NULL) {
			fprintf(stderr, "No such backend `%s'\n", backend);
			return 1;
		}
	} else {
		ops = &linestack_file;
	}

	if (config_dir == NULL) 
		config_dir = g_build_filename(g_get_home_dir(), ".ctrlproxy", NULL);

	cfg = load_configuration(config_dir, FALSE);
	if (cfg == NULL) {
		fprintf(stderr, "Unable to load configuration from `%s'", config_dir);
		return 1;
	}

	markers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, 
									(GDestroyNotify)linestack_free_marker);

	ctx = create_linestack(ops, argv[1], TRUE, cfg, state);

	atexit(freels);

	if (ctx == NULL) {
		fprintf(stderr, "Unable to open linestack context\n");
		return 1;
	}

	while (1) {
		gint cargc;
		GError *error = NULL;
		gchar **cargv = NULL;
		int i;

		line = readline("linestack> ");	

		if (line == NULL)
			return 0;

		if (!g_shell_parse_argv(line, &cargc, &cargv, &error)) {
			fprintf(stderr, "Error parsing: %s\n", error->message);
			g_error_free(error);
			goto next;
		}

		if (cargc == 0) {
			goto next;
		}

		for (i = 0; cmds[i].name; i++) {
			if (!strcmp(cmds[i].name, cargv[0])) {
				cmds[i].handler(cargc, cargv);
				goto next;
			}
		}

		fprintf(stderr, "Unknown command `%s'\n", cargv[0]);

next:
		g_strfreev(cargv);

		free(line);
	}
	g_option_context_free(pc);

	return 0;
}
