#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include "linestack.h"

static void handle_exit(int, char **);

struct cmd {
	const char *name;
	void (*handler) (int argc, char **);
} cmds[] = {
	{ "exit", handle_exit },
};

static void handle_exit(int argc, char **argv)
{
	exit(0);
}

int main(int argc, const char **argv)
{
	char *line;

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
			if (!strcmp(cmds[i].name, cargv[0]))
				cmds[i].handler(cargc, cargv);
		}

		fprintf(stderr, "Unknown command `%s'\n", cargv[0]);

next:
		g_strfreev(cargv);

		free(line);
	}

	return 0;
}
