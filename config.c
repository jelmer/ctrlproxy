/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2004 Jelmer Vernooij <jelmer@nl.linux.org>

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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define __USE_POSIX
#include <netdb.h>

static xmlDtdPtr dtd;
char *configuration_file;

void save_configuration()
{
	xmlDocPtr configuration = xmlNewDoc("1.0");

	/* FIXME: Build tree */

	xmlSaveFile(configuration_file, configuration);
}

static gboolean validate_config(xmlDocPtr configuration)
{
	gboolean ret;

	xmlValidCtxtPtr ctxt = xmlNewValidCtxt();

	ret = xmlValidateDtd(ctxt, configuration, dtd);

	xmlFreeValidCtxt(ctxt);

	return ret;
}

static GHashTable *plugin_nodes = NULL;

gboolean plugin_load_config(struct plugin *p) 
{
	xmlNodePtr cur;
	
	cur = g_hash_table_lookup(plugin_nodes, p);

	if (!p) 
		return FALSE;

	if (!p->load_config) 
		return TRUE;

	return p->load_config(p, cur);
}

xmlNodePtr plugin_save_config(struct plugin *p)
{
	if (!p) 
		return NULL;

	if (!p->save_config)
		return NULL;

	return p->save_config(p);
}

void init_config()
{
    LIBXML_TEST_VERSION
	
	dtd = xmlParseDTD(NULL, DTD_FILE);

	if (!dtd) {
		g_error("Can't load DTD file from %s", DTD_FILE);
	}
}

static void config_load_plugin(xmlNodePtr root)
{
	char *tmp;
	struct plugin *p;
	
	tmp = xmlGetProp(root, "file");
	p = new_plugin(tmp);
	xmlFree(tmp);

	if (xmlHasProp(root, "autoload")) {
		tmp = xmlGetProp(root, "autoload");
		if (atoi(tmp)) p->autoload = TRUE;
		xmlFree(tmp);
	}

	g_hash_table_insert(plugin_nodes, p, root);
}

static void config_load_plugins(xmlNodePtr root)
{
	xmlNodePtr cur;
	
	for (cur = root->children; cur; cur = cur->next) 
	{
		if (cur->type != XML_ELEMENT_NODE) continue;		

		g_assert(!strcmp(cur->name, "plugin"));
		config_load_plugin(cur);
	}
}

static void config_load_channel(struct network *n, xmlNodePtr root)
{
	struct channel *ch = g_new0(struct channel, 1);
	char *tmp;

	ch->name = xmlGetProp(root, "name");
	ch->key = xmlGetProp(root, "key");

	if (xmlHasProp(root, "autojoin")) {
		tmp = xmlGetProp(root, "autojoin");
		if (atoi(tmp)) ch->autojoin = TRUE;
		xmlFree(tmp);
	}
	
	ch->network = n;

	n->channels = g_list_append(n->channels, ch);
}

static void config_load_servers(struct network *n, xmlNodePtr root)
{
	xmlNodePtr cur;
	n->type = NETWORK_TCP;

	for (cur = root->children; cur; cur = cur->next) 
	{
		struct tcp_server *s;
		int error;
		struct addrinfo hints;

		if (cur->type != XML_ELEMENT_NODE) continue;
		
		s = g_new0(struct tcp_server, 1);
		
		s->host = xmlGetProp(cur, "host");
		s->password = xmlGetProp(cur, "password");

		if (xmlHasProp(cur, "port")) {
			s->port = xmlGetProp(cur, "port");
		} else { 
			s->port = g_strdup("6667");
		}

		if (xmlHasProp(cur, "ssl")) {
			char *tmp = xmlGetProp(cur, "ssl");
			if (atoi(tmp)) s->ssl = TRUE;
			xmlFree(tmp);
		}

		s->name = xmlGetProp(cur, "name");
		if (!s->name) s->name = xmlGetProp(cur, "host");

    	memset(&hints, 0, sizeof(hints));
	    hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		
		/* Lookup */
	 	error = getaddrinfo(s->host, s->port, &hints, &s->addrinfo);
	    if (error) {
			g_warning("Unable to lookup %s: %s", s->host, gai_strerror(error));
			continue;
		}

		n->connection.tcp.servers = g_list_append(n->connection.tcp.servers, s);
	}
}

static void config_load_network(xmlNodePtr root)
{
	xmlNodePtr cur;
	char *tmp;
	struct network *n;
	n = new_network();

	if (xmlHasProp(root, "autoconnect")) {
		tmp = xmlGetProp(root, "autoconnect");
		if (atoi(tmp)) n->autoconnect = TRUE;
		xmlFree(tmp);
	}

	if (xmlHasProp(root, "fullname")) {
		g_free(n->fullname);
		n->fullname = xmlGetProp(root, "fullname");
	}

	if (xmlHasProp(root, "nick")) {
		g_free(n->nick);
		n->nick = xmlGetProp(root, "nick");
	}

	if (xmlHasProp(root, "username")) {
		g_free(n->username);
		n->username = xmlGetProp(root, "username");
	}

	if (xmlHasProp(root, "ignore_first_nick")) {
		tmp = xmlGetProp(root, "ignore_first_nick");
		if (atoi(tmp)) n->ignore_first_nick = TRUE;
		xmlFree(tmp);
	}

	if (xmlHasProp(root, "password")) {
		g_free(n->password);
		n->password = xmlGetProp(root, "password");
	}

	if (xmlHasProp(root, "name")) {
		g_free(n->name);
		n->name = xmlGetProp(root, "name");
	}

	for (cur = root->children; cur; cur = cur->next)
	{
		if (cur->type != XML_ELEMENT_NODE) continue;

		if (!strcmp(cur->name, "servers")) {
			config_load_servers(n, cur);
		} else if (!strcmp(cur->name, "channel")) {
			config_load_channel(n, cur);
		} else if (!strcmp(cur->name, "autosend")) {
			n->autosend_lines = g_list_append(n->autosend_lines, xmlNodeGetContent(cur));
		} else if (!strcmp(cur->name, "program")) {
			n->type = NETWORK_PROGRAM;
			n->connection.program.location = xmlNodeGetContent(cur);
		} else if (!strcmp(cur->name, "virtual")) {
			n->type = NETWORK_VIRTUAL;
			n->connection.virtual.type = xmlGetProp(cur, "type");
		}
	}

}

static void config_load_networks(xmlNodePtr root)
{
	xmlNodePtr cur;
	
	for (cur = root->children; cur; cur = cur->next) 
	{
		if (cur->type != XML_ELEMENT_NODE) continue;		

		config_load_network(cur);
	}

}

gboolean load_configuration(const char *file) 
{
	xmlDocPtr configuration;
    xmlNodePtr root, cur;
	gboolean ret;

	configuration = xmlParseFile(file);
	if(!configuration) {
		g_error(_("Can't open configuration file '%s'"), file);
		return FALSE;
	}

	if (!validate_config(configuration)) {
		g_warning("Errors while parsing configuration file");
	}

	root = xmlDocGetRootElement(configuration);

	plugin_nodes = g_hash_table_new(NULL, NULL);
	for (cur = root->children; cur; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE) continue;

		if (!strcmp(cur->name, "plugins")) {
			config_load_plugins(cur);
		} else if (!strcmp(cur->name, "networks")) {
			config_load_networks(cur);
		}
	}

	ret = init_plugins();
	g_hash_table_destroy(plugin_nodes); plugin_nodes = NULL;

	xmlFreeDoc(configuration);

	ret &= init_networks();

	return ret;
}
