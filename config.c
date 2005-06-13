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
#include <sys/socket.h>

#define DTD_URL "http://ctrlproxy.vernstok.nl/2.7/ctrlproxyrc.dtd"

static xmlDtdPtr dtd;

static char *last_config_file = NULL;

static xmlNodePtr config_save_plugins()
{
	GList *gl;
	xmlNodePtr ret = xmlNewNode(NULL, "plugins");

	for (gl = get_plugin_list(); gl; gl = gl->next) {
		struct plugin *p = gl->data;
		xmlNodePtr n = xmlNewNode(NULL, "plugin");

		xmlSetProp(n, "autoload", p->autoload?"1":"0");
		xmlSetProp(n, "file", p->path);
		
		if (p->ops->save_config) {
			p->ops->save_config(p, n);
		}

		xmlAddChild(ret, n);
	}

	return ret;
}

static xmlNodePtr config_save_tcp_servers(struct network *n)
{
	GList *gl;
	xmlNodePtr s = xmlNewNode(NULL, "servers");
	for (gl = n->connection.data.tcp.servers; gl; gl = gl->next) {
		struct tcp_server *ts = gl->data;
		xmlNodePtr x = xmlNewNode(NULL, "server");
		if (ts->name) xmlSetProp(x, "name", ts->name);
		if (ts->host) xmlSetProp(x, "host", ts->host);
		if (ts->port) xmlSetProp(x, "port", ts->port);
		if (ts->ssl) xmlSetProp(x, "ssl", "1");
		if (ts->password) xmlSetProp(x, "password", ts->password);

		xmlAddChild(s, x);
	}

	return s;
}

static xmlNodePtr config_save_networks()
{
	xmlNodePtr ret = xmlNewNode(NULL, "networks");
	GList *gl;

	for (gl = get_network_list(); gl; gl = gl->next) {
		GList *gl1;
		struct network *n = gl->data;		
		xmlNodePtr p = xmlNewNode(NULL, "network"), p1;

		xmlSetProp(p, "autoconnect", n->autoconnect?"1":"0");
		if (!n->name_guessed)
			xmlSetProp(p, "name", n->name);
		xmlSetProp(p, "fullname", n->fullname);
		xmlSetProp(p, "nick", n->nick);
		xmlSetProp(p, "username", n->username);

		switch(n->connection.type) {
		case NETWORK_VIRTUAL:
			p1 = xmlNewChild(p, NULL, "virtual", NULL);
			xmlSetProp(p1, "type", n->connection.data.virtual.ops->name);
			break;
		case NETWORK_PROGRAM:
			p1 = xmlNewChild(p, NULL, "program", n->connection.data.program.location);
			break;
		case NETWORK_TCP:
			xmlAddChild(p, config_save_tcp_servers(n));
			break;
		default:break;
		}
		
		for (gl1 = n->state.channels; gl1; gl1 = gl1->next) {
			struct channel_state *c = gl1->data;
			xmlNodePtr x = xmlNewNode(NULL, "channel");
			xmlSetProp(x, "name", c->name);
			if (c->key) xmlSetProp(x, "key", c->key);
			xmlSetProp(x, "autojoin", c->autojoin?"1":"0");

			xmlAddChild(p, x);
		}

		for (gl1 = n->autosend_lines; gl1; gl1 = gl1->next) {
			xmlNewTextChild(p, NULL, "autosend", (char *)gl1->data);
		}
		
		xmlAddChild(ret, p);
	}

	return ret;
}

void save_configuration(const char *configuration_file)
{
	xmlNodePtr root;
	xmlDocPtr configuration = xmlNewDoc("1.0");
	
	root = xmlNewNode(NULL, "ctrlproxy");

	xmlDocSetRootElement(configuration, root);

	xmlAddChild(root, config_save_plugins());
	xmlAddChild(root, config_save_networks());

	xmlSaveFile(configuration_file?configuration_file:last_config_file, configuration);

	xmlFreeDoc(configuration);
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

	if (!p->ops->load_config) 
		return TRUE;

	return p->ops->load_config(p, cur);
}

void init_config()
{
	dtd = xmlParseDTD(DTD_URL, DTD_FILE);

	if (!dtd) {
		log_global(NULL, "Can't load DTD file from %s", DTD_FILE);
	}
}

void fini_config()
{
	g_free(last_config_file);
	xmlFreeDtd(dtd);
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
	struct channel_state *ch = g_new0(struct channel_state, 1);
	char *tmp;

	ch->name = xmlGetProp(root, "name");
	ch->key = xmlGetProp(root, "key");

	if (xmlHasProp(root, "autojoin")) {
		tmp = xmlGetProp(root, "autojoin");
		if (atoi(tmp)) ch->autojoin = TRUE;
		xmlFree(tmp);
	}
	
	ch->network = &n->state;

	n->state.channels = g_list_append(n->state.channels, ch);
}

static void config_load_servers(struct network *n, xmlNodePtr root)
{
	xmlNodePtr cur;
	n->connection.type = NETWORK_TCP;

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
		if (s->name && strchr(s->name, ' ')) {
			log_global(NULL, "Network name \"%s\" contains spaces!", s->name);
		}

    	memset(&hints, 0, sizeof(hints));
	    hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		
		/* Lookup */
	 	error = getaddrinfo(s->host, s->port, &hints, &s->addrinfo);
	    if (error) {
			log_global(NULL, "Unable to lookup %s: %s", s->host, gai_strerror(error));
			continue;
		}

		n->connection.data.tcp.servers = g_list_append(n->connection.data.tcp.servers, s);
	}
}

static void config_load_network(xmlNodePtr root)
{
	xmlNodePtr cur;
	char *tmp;
	struct network *n;
	n = new_network();

	if ((tmp = xmlGetProp(root, "autoconnect"))) {
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
			n->connection.type = NETWORK_PROGRAM;
			n->connection.data.program.location = xmlNodeGetContent(cur);
		} else if (!strcmp(cur->name, "virtual")) {
			n->connection.type = NETWORK_VIRTUAL;
			n->connection.data.virtual.type = xmlGetProp(cur, "type");
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

	g_free(last_config_file); last_config_file = g_strdup(file);

	configuration = xmlParseFile(file);
	if(!configuration) {
		log_global(NULL, "Can't open configuration file '%s'", file);
		return FALSE;
	}

	if (!validate_config(configuration)) {
		log_global(NULL, "Warnings while parsing configuration file");
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

	return ret & init_networks();
}
