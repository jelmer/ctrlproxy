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

static char *format = NULL;

static gboolean report_time(struct line *l) {
	/* Loop thru all channels on all servers */
	char stime[512];
	char *tmp;
	time_t cur = time(NULL);

	if(strcasecmp(l->args[0], "PRIVMSG") && strcasecmp(l->args[0], "NOTICE")) 
		return TRUE;

	/* Don't add time in CTCP requests */
	if(l->args[2][0] == '\001') return TRUE;
	
	strftime(stime, sizeof(stime), format?format:"%H:%M:%S", localtime(&cur));
	
	asprintf(&tmp, "[%s] %s", stime, l->args[2]);
	free(l->args[2]);
	l->args[2] = tmp;

	return TRUE;
}

gboolean fini_plugin(struct plugin *p) {
	guint *timeout_id = (guint *)p->data;

	g_source_remove(*timeout_id);

	free(timeout_id);
	return TRUE;
}

char *name_plugin = "report_time";

gboolean init_plugin(struct plugin *p) {
	guint *timeout_id = malloc(sizeof(guint));
	xmlNodePtr cur;

	cur = xmlFindChildByElementName(p->xmlConf, "format");
	if(cur) format = xmlNodeGetContent(cur);
	
	add_filter_ex("report_time", report_time, "replicate", 50);
	p->data = timeout_id;
	return TRUE;
}
