/* 
	ctrlproxy: A modular IRC proxy
	(c) 2003 Jelmer Vernooij <jelmer@nl.linux.org>

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

#define _GNU_SOURCE
#include "ctrlproxy.h"
#include <string.h>
#include <time.h>

static gboolean report_time(gpointer user_data) {
	/* Loop thru all channels on all servers */
	GList *nl = networks;
	struct line *l;
	char *fmt = (char *)user_data;
	char stime[512];
	time_t cur = time(NULL);
	
	strftime(stime, sizeof(stime), fmt?fmt:"%H:%M:%S", localtime(&cur));
	
	l = irc_parse_line_args("timestamp", "PRIVMSG", "DEST", stime, NULL);
	l->direction = FROM_SERVER;
	l->options|=LINE_NO_LOGGING;

	while(nl) {
		struct network *n = (struct network *)nl->data;
		GList *cl = n->channels;
		while(cl) {
			struct channel *c = (struct channel *)cl->data;
			free(l->args[1]);
			l->args[1] = xmlGetProp(c->xmlConf, "name");
			l->network = n;
			filters_execute(l);
			cl = cl->next;
		}
		nl = nl->next;
	}

	free_line(l);

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	guint *timeout_id = (guint *)p->data;

	g_source_remove(*timeout_id);

	free(timeout_id);
	return TRUE;
}

gboolean init_plugin(struct plugin *p) {
	guint *timeout_id = malloc(sizeof(guint));
	char *format = NULL;
	int interval = 0;
	xmlNodePtr cur = xmlFindChildByElementName(p->xmlConf, "interval");

	if(cur && xmlNodeGetContent(cur))
		interval = atoi(xmlNodeGetContent(cur));

	if(interval == 0) interval = 60;

	cur = xmlFindChildByElementName(p->xmlConf, "format");
	if(cur) format = xmlNodeGetContent(cur);
	
	*timeout_id = g_timeout_add(1000 * interval, report_time, format);

	p->data = timeout_id;
	return TRUE;
}
