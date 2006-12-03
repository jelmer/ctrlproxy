#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include "line.h"
#include "linestack.h"
#include "settings.h"
#include "state.h"

static struct linestack_context *ctx = NULL;
static struct network_state *state = NULL;
static struct ctrlproxy_config *cfg = NULL;

static void handle_exit(int, char **);
static void handle_insert(int, char **);

struct cmd {
	const char *name;
	void (*handler) (int argc, char **);
} cmds[] = {
	{ "exit", handle_exit },
	{ "insert", handle_insert },
};

static void handle_insert(int argc, char **argv)
{
	struct line *l;
	enum data_direction direction;

	if (argc < 2) {
		fprintf(stderr, "Usage: insert <client|server> <line>\n");
		return;
	}

	if (!g_strcasecmp(argv[1], "client")) 
		direction = TO_SERVER;
	if (!g_strcasecmp(argv[1], "server"))
		direction = FROM_SERVER;
	
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
	struct network_info *info;
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

	info = g_new0(struct network_info, 1);

	state = network_state_init(info, "nick", "username", "hostname");

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

	cfg = load_configuration(config_dir);
	if (cfg == NULL) {
		fprintf(stderr, "Unable to load configuration from `%s'", config_dir);
		return 1;
	}

	ctx = create_linestack(ops, argv[1], cfg, state);

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
