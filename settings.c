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

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define __USE_POSIX
#include <netdb.h>
#include <sys/socket.h>

#define DTD_URL "http://ctrlproxy.vernstok.nl/2.7/ctrlproxyrc.dtd"

static xmlDtdPtr dtd;

static xmlNodePtr config_save_tcp_servers(struct network_config *n)
{
	GList *gl;
	xmlNodePtr s = xmlNewNode(NULL, "servers");
	for (gl = n->type_settings.tcp_servers; gl; gl = gl->next) {
		struct tcp_server_config *ts = gl->data;
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

static xmlNodePtr config_save_networks(GList *networks)
{
	xmlNodePtr ret = xmlNewNode(NULL, "networks");
	GList *gl;

	for (gl = networks; gl; gl = gl->next) {
		GList *gl1;
		struct network_config *n = gl->data;		
		xmlNodePtr p = xmlNewNode(NULL, "network"), p1;

		xmlSetProp(p, "autoconnect", n->autoconnect?"1":"0");
		if (n->name) 
			xmlSetProp(p, "name", n->name);
		xmlSetProp(p, "fullname", n->fullname);
		xmlSetProp(p, "nick", n->nick);
		xmlSetProp(p, "username", n->username);

		switch(n->type) {
		case NETWORK_VIRTUAL:
			p1 = xmlNewChild(p, NULL, "virtual", NULL);
			xmlSetProp(p1, "type", n->type_settings.virtual_type);
			break;
		case NETWORK_PROGRAM:
			p1 = xmlNewChild(p, NULL, "program", n->type_settings.program_location);
			break;
		case NETWORK_TCP:
			xmlAddChild(p, config_save_tcp_servers(n));
			break;
		default:break;
		}
		
		for (gl1 = n->channels; gl1; gl1 = gl1->next) {
			struct channel_config *c = gl1->data;
			xmlNodePtr x = xmlNewNode(NULL, "channel");
			xmlSetProp(x, "name", c->name);
			if (c->key) xmlSetProp(x, "key", c->key);
			xmlSetProp(x, "autojoin", c->autojoin?"1":"0");

			xmlAddChild(p, x);
		}

		xmlAddChild(ret, p);
	}

	return ret;
}

void save_configuration(struct ctrlproxy_config *cfg, const char *configuration_file)
{
	xmlNodePtr root;
	xmlDocPtr configuration = xmlNewDoc("1.0");

	g_assert(configuration_file);
	
	root = xmlNewNode(NULL, "ctrlproxy");

	xmlDocSetRootElement(configuration, root);

	xmlAddChild(root, config_save_networks(cfg->networks));

	xmlSaveFormatFile(configuration_file, configuration, 1);

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

void init_config()
{
	dtd = xmlParseDTD(DTD_URL, DTD_FILE);

	if (!dtd) {
		log_global(NULL, LOG_WARNING, "Can't load DTD file from %s", DTD_FILE);
	}
}

void fini_config()
{
	xmlFreeDtd(dtd);
}

static void config_load_channel(struct network_config *n, xmlNodePtr root)
{
	struct channel_config *ch = g_new0(struct channel_config, 1);
	char *tmp;

	ch->name = xmlGetProp(root, "name");
	ch->key = xmlGetProp(root, "key");

	if (xmlHasProp(root, "autojoin")) {
		tmp = xmlGetProp(root, "autojoin");
		if (atoi(tmp)) ch->autojoin = TRUE;
		xmlFree(tmp);
	}
	
	n->channels = g_list_append(n->channels, ch);
}

static void config_load_servers(struct network_config *n, xmlNodePtr root)
{
	xmlNodePtr cur;
	n->type = NETWORK_TCP;

	for (cur = root->children; cur; cur = cur->next) 
	{
		struct tcp_server_config *s;

		if (cur->type != XML_ELEMENT_NODE) continue;
		
		s = g_new0(struct tcp_server_config, 1);
		
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
			log_global(NULL, LOG_WARNING, "Network name \"%s\" contains spaces!", s->name);
		}

		n->type_settings.tcp_servers = g_list_append(n->type_settings.tcp_servers, s);
	}
}

static struct network_config *config_load_network(struct ctrlproxy_config *cfg, xmlNodePtr root)
{
	xmlNodePtr cur;
	char *tmp;
	struct network_config *n;
	n = network_config_init(cfg);

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
		} else if (!strcmp(cur->name, "program")) {
			n->type = NETWORK_PROGRAM;
			n->type_settings.program_location = xmlNodeGetContent(cur);
		} else if (!strcmp(cur->name, "virtual")) {
			n->type = NETWORK_VIRTUAL;
			n->type_settings.virtual_type = xmlGetProp(cur, "type");
		}
	}

	return n;
}

static void config_load_networks(struct ctrlproxy_config *cfg, xmlNodePtr root)
{
	xmlNodePtr cur;
	
	for (cur = root->children; cur; cur = cur->next) 
	{
		if (cur->type != XML_ELEMENT_NODE) continue;		

		config_load_network(cfg, cur);
	}
}

struct ctrlproxy_config *load_configuration(const char *file) 
{
	xmlDocPtr configuration;
    xmlNodePtr root, cur;
	struct ctrlproxy_config *cfg = g_new0(struct ctrlproxy_config, 1);

	cfg->shared_path = g_strdup(SHAREDIR);

	configuration = xmlParseFile(file);
	if(!configuration) {
		log_global(NULL, LOG_ERROR, "Can't open configuration file '%s'", file);
		g_free(cfg);
		return NULL;
	}

	if (!validate_config(configuration)) {
		log_global(NULL, LOG_ERROR, "Warnings while parsing configuration file");
	}

	root = xmlDocGetRootElement(configuration);

	for (cur = root->children; cur; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE) continue;

		if (!strcmp(cur->name, "networks")) {
			config_load_networks(cfg, cur);
		} else if (!strcmp(cur->name, "replication")) {
			cfg->replication = xmlNodeGetContent(cur);
		}
	}

	xmlFreeDoc(configuration);

	return cfg;
}

struct network_config *network_config_init(struct ctrlproxy_config *cfg) 
{
	struct network_config *s = g_new0(struct network_config, 1);

	s->autoconnect = FALSE;
	s->nick = g_strdup(g_get_user_name());
	s->username = g_strdup(g_get_user_name());
	s->fullname = g_strdup(g_get_real_name());
	s->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;

	if (cfg) 
		cfg->networks = g_list_append(cfg->networks, s);
	return s;
}

void free_config(struct ctrlproxy_config *cfg)
{
	while (cfg->networks) {
		struct network_config *nc = cfg->networks->data;
		g_free(nc->name);
		g_free(nc->nick);
		g_free(nc->fullname);
		g_free(nc->username);
		g_free(nc->password);
		while (nc->channels) {
			struct channel_config *cc = nc->channels->data;
			g_free(cc->name);
			g_free(cc->key);
			nc->channels = g_list_remove(nc->channels, cc);	
			g_free(cc);
		}
		switch (nc->type) {
		case NETWORK_TCP: 
			while (nc->type_settings.tcp_servers) {
				struct tcp_server_config *tc = nc->type_settings.tcp_servers->data;
				g_free(tc->name);
				g_free(tc->host);
				g_free(tc->port);
				g_free(tc->password);
				nc->type_settings.tcp_servers = g_list_remove(nc->type_settings.tcp_servers, tc);
				g_free(tc);
			}
			break;
		case NETWORK_VIRTUAL:
			g_free(nc->type_settings.virtual_type);
			break;
		case NETWORK_PROGRAM:
			g_free(nc->type_settings.program_location);
			break;
		}
		cfg->networks = g_list_remove(cfg->networks, nc);
		g_free(nc);
	}
	g_free(cfg->modules_path);
	g_free(cfg->shared_path);
	g_free(cfg);
}
