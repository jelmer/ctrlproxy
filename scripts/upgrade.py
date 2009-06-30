#!/usr/bin/python
# Upgrade script for CtrlProxy
# Copyright (C) 2005-2006 Jelmer Vernooij <jelmer@samba.org>

import optparse
import sys
import os
from xml.dom import minidom, Node

class UnknownTagError(StandardError):

    def __init__(self,tag):
        StandardError.__init__(self, 'Unknown tag: %s' % tag.nodeName)


class Channel(object):

    def __init__(self, name=None):
        self.name = name
        self.autojoin = False
        self.key = None


class Network(object):

    def __init__(self, name=None):
        self.name = name
        self.nick = None
        self.fullname = None
        self.username = None
        self.autoconnect = None
        self.channels = {}
        self.servers = []


class Plugin(object):

    def __init__(self, name=None, autoload=True, config=None):
        if name.startswith('lib'):
            name = name[3:]

        self.name = name
        self.config = config
        self.autoload = autoload


class Config(object):

    def __init__(self):
        self.plugins = {}
        self.networks = {}
        self.listeners = []
        self.autosend_lines = []
        self.nickserv_nicks = []

    def has_plugin(self, name):
        return self.plugins.has_key(name)


class OldConfigFile(Config):

    def _parsePlugin(self,node):
        plugin = Plugin(name=node.attributes['file'].value, config=node.childNodes)
        if node.attributes.has_key('autoload') and \
                node.attributes['autoload'].value == "1":
            plugin.autoload = True

        if plugin.name == 'nickserv':
            for cn in node.childNodes:
                if cn.nodeName == 'nick':
                    self.nickserv_nicks.append(cn.attributes)
            plugin.config = None

        if plugin.name == 'autosend':
            for cn in node.childNodes:
                if cn.nodeName == 'nick':
                    self.autosend_lines.append({
                            'network': cn.attributes['network'].name,
                            'data': cn.toxml()})
            plugin.config = None

        if plugin.name == 'listener':
            for cn in node.childNodes:
                if cn.nodeName == 'listen':
                    self.listeners.append(cn.attributes)
            plugin.config = None
        
        self.plugins[plugin.name] = plugin
        
    def _parsePlugins(self,node):
        for cn in node.childNodes:
            if cn.nodeName == 'plugin':
                self._parsePlugin(cn)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def _parseServer(self,node,serverpass):
        return node.attributes #FIXME

    def _parseServers(self,node,serverpass):
        servers = []
        for cn in node.childNodes:
            if cn.nodeName == 'ipv4' or cn.nodeName == 'ipv6'  \
                or cn.nodeName == 'pipe' or cn.nodeName == 'server':
                servers.append(self._parseServer(cn,serverpass))
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def _parseChannel(self,node):
        channel = Channel()
        channel.name = node.attributes['name'].value
        if node.attributes.has_key('autojoin') or \
                node.attributes['autojoin'].value == '1':
            channel.autojoin = True
        if node.attributes.has_key('key'):
            channel.key = node.attributes['key'].value
        return channel

    def _parseNickserv(self,name,node):
        for cn in node.childNodes:
            if cn.nodeName == 'nick' or cn.nodeName == 'nickserv':
                nick = {
                        'network': name,
                        'password': cn.attributes['password'].value
                        }

                if cn.attributes.has_key('nick'):
                    nick['nick'] = cn.attributes['nick'].value
                elif cn.attributes.has_key('name'):
                    nick['nick'] = cn.attributes['name'].value

                self.nickserv_nicks.append(nick)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def _parseListeners(self,name,clientpass,node):
        for cn in node.childNodes:
            if cn.nodeName == 'ipv4' or cn.nodeName == 'ipv6':
                listener = {'network': name }
                if not clientpass is None:
                    listener['password'] = clientpass
                if cn.attributes.has_key('port'):
                    listener['port'] = cn.attributes['port'].value
                if cn.attributes.has_key('password'):
                    listener['password'] = cn.attributes['password'].value
                if cn.attributes.has_key('bind'):
                    listener['bind'] = cn.attributes['bind'].value
                if cn.attributes.has_key('ssl'):
                    listener['ssl'] = cn.attributes['ssl'].value
                self.listeners.append(listener)
            elif cn.nodeName == 'pipe':
                listener = {'network': name }
                if not clientpass is None:
                    listener['password'] = clientpass
                listener['path'] = cn.attributes['path'].value
                if cn.attributes.has_key('password'):
                    listener['password'] = cn.attributes['password'].value
                self.listeners.append(listener)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def _parseNetwork(self,node):
        network = Network()
        client_pass = None
        serverpass = None

        if node.attributes.has_key('name'):
            network.name = node.attributes['name'].value
        else:
            self.nameless_index += 1
            network.name = "nameless%d" % self.nameless_index
        if node.attributes.has_key('client_pass'):
            client_pass = node.attributes['client_pass'].value
        if node.attributes.has_key('nick'):
            network.nick = node.attributes['nick'].value
        if node.attributes.has_key('fullname'):
            network.fullname = node.attributes['fullname'].value
        if node.attributes.has_key('password'):
            serverpass = node.attributes['password'].value
        if node.attributes.has_key('autoconnect') and \
                node.attributes['autoconnect'].value == "1":
            network.autoconnect = True
        if node.attributes.has_key('username'):
            network.username = node.attributes['username'].value

        for cn in node.childNodes:
            if cn.nodeName == 'servers':
                network.servers.append(self._parseServers(cn,serverpass))
            elif cn.nodeName == 'channel':
                channel = self._parseChannel(cn)
                network.channels[channel.name] = channel
            elif cn.nodeName == 'program':
                network.program_path = cn.toxml()
            elif cn.nodeName == 'virtual':
                network.virtual_type = cn.attributes['type'].value
            elif cn.nodeName == 'listen':
                self._parseListeners(network.name, client_pass, cn)
            elif cn.nodeName == 'autosend':
                self.autosend_lines = {'network': network.name, 
                                       'data': cn.toxml()}
            elif cn.nodeName == 'nickserv':
                self._parseNickserv(network.name, cn) 
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

        self.networks[network.name] = network

    def _parseNetworks(self,node):
        for cn in node.childNodes:
            if cn.nodeName == 'network':
                self._parseNetwork(cn) 
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def __init__(self,filename):
        Config.__init__(self)
        self.doc = minidom.parse(filename)
        node = self.doc.documentElement
        self.nameless_index = 0
        if node.nodeName != 'ctrlproxy':
            raise UnknownTagError(cn)

        for cn in node.childNodes:
            if cn.nodeName == 'networks':
                self._parseNetworks(cn)
            elif cn.nodeName == 'plugins':
                self._parsePlugins(cn)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

if len(sys.argv) > 1:
    oldfilename = sys.argv[1]
else:
    oldfilename = os.path.join(os.getenv('HOME'), ".ctrlproxyrc")

try:
    oldfile = OldConfigFile(oldfilename)
except IOError, e:
    print "%s" % (e)
    print "Usage: %s [PATH-TO-CTRLPROXYRC]" % sys.argv[0]
    sys.exit(1)


class IniFile(object):

    def __init__(self,dict={}):
        self.conf = dict

    def write(self):
        if self.conf.has_key('global'):
            self.print_section("global")
            del self.conf["global"]
   
        for section in self.conf:
            print 
            self.print_section(section)

    def print_section(self,section):
        print "[%s]" % section
        for var in self.conf[section]:
            print "%s = %s" % (var, self.conf[section][var])

conf = IniFile({'global':{'autoconnect':""}})
networks = {}
listeners = IniFile()
warnings = []

def convert_report_time(plugin,conf):
    conf.conf['global']['report-time'] = 'true'

def convert_motd_file(plugin,conf):
    conf.conf["global"]["motd-file"] = 'FIXME'

def convert_linestack_file(plugin,conf):
    conf.conf["global"]["linestack"] = "file"

def convert_ctcp(plugin,conf):
    # Now integrated
    pass

def convert_strip(plugin,conf):
    # Now integrated
    pass

def convert_repl_lastdisconnect(plugin,conf):
    conf.conf["global"]["replication"] = "lastdisconnect"

def convert_repl_none(plugin,conf):
    conf.conf["global"]["replication"] = "none"

def convert_admin(plugin,conf):
    conf.conf["admin"] = {}

def convert_log_irssi(plugin,conf):
    conf.conf["log-irssi"] = {}
    for cn in plugin.config:
        if cn.nodeName == "logfile":
            conf.conf["log-irssi"]["logfile"] = cn.toxml()
    
def convert_log_custom(plugin,conf):
    conf.conf["log-custom"] = {}
    for cn in plugin.config:
        conf.conf["log-custom"][cn.nodeName] = cn.toxml()

def convert_nickserv(plugin,conf):
    conf.conf["nickserv"] = {}

def convert_auto_away(plugin,conf):
    conf.conf["auto-away"] = {}
    for cn in plugin.config:
        if cn.nodeName == 'message':
            conf.conf['auto-away']['message'] = cn.toxml()
            if cn.attributes.has_key('time'):
                conf.conf['auto-away']['time'] = cn.attributes['time'].value
        elif cn.nodeName == 'only_noclient':
            conf.conf['auto-away']['only_noclient'] = cn.toxml()

def convert_socket(plugin,conf):
    # Now integrated
    for cn in plugin.config:
        if cn.nodeName == "sslcertfile":
            conf.conf['global']['certfile'] = cn.toxml()
        elif cn.nodeName == "sslkeyfile":
            conf.conf['global']['keyfile'] = cn.toxml()
        elif cn.nodeName == "sslcafile":
            conf.conf['global']['cafile'] = cn.toxml()

def convert_stats(plugin,conf):
    warnings.append("stats module is no longer supported")

def convert_linestack_memory(plugin,conf):
    conf.conf['global']['linestack'] = 'file'

def convert_repl_command(plugin,conf):
    # Now integrated
    pass

def convert_repl_memory(plugin,conf):
    conf.conf['global']['linestack'] = 'file'
    conf.conf['global']['replication'] = 'simple'

def convert_antiflood(plugin,conf):
    conf.conf['antiflood'] = {}

def convert_repl_highlight(plugin,conf):
    conf.conf['global']['replication'] = 'highlight'
    matches = []
    for cn in plugin.config:
        if cn.nodeName == 'match':
            matches.append(cn.toxml())

    conf.conf['global']['matches'] = ",".join(matches)


def convert_repl_simple(plugin,conf):
    conf.conf['global']['replication'] = 'simple'

convert_plugin = {
    'report_time': convert_report_time,
    'motd_file': convert_motd_file,
    'linestack_file': convert_linestack_file,
    'ctcp': convert_ctcp,
    'strip': convert_strip,
    'repl_none': convert_repl_none,
    'admin': convert_admin,
    'nickserv': convert_nickserv,
    'log_irssi': convert_log_irssi,
    'log_custom': convert_log_custom,
    'stats': convert_stats,
    'socket': convert_socket,
    'auto_away': convert_auto_away,
    'linestack_memory': convert_linestack_memory,
    'repl_command': convert_repl_command,
    'repl_memory': convert_repl_memory,
    'repl_lastdisconnect': convert_repl_lastdisconnect,
    'antiflood': convert_antiflood,
    'repl_highlight': convert_repl_highlight,
    'repl_simple': convert_repl_simple,
    'auto-away': convert_auto_away,
}

if oldfile.autosend_lines:
    warnings.append("autosend data is no longer reported")

for plugin in oldfile.plugins.keys():
    convert_plugin[plugin](oldfile.plugins[plugin],conf)    
    del oldfile.plugins[plugin]

for net in oldfile.networks.keys():
    oldnet = oldfile.networks[net]
    networks[net] = IniFile({'global':{}})
    if oldnet.nick:
        networks[net].conf['global']['nick'] = oldnet.nick

    if oldnet.fullname:
        networks[net].conf['global']['fullname'] = oldnet.fullname

    if oldnet.username:
        networks[net].conf['global']['username'] = oldnet.username

    if oldnet.autoconnect:
        conf.conf['global']['autoconnect'] += "%s;" % oldnet.name

    for ch in oldnet.channels:
        oldch = oldnet.channels[ch]
        networks[net].conf[ch] = {}

        if oldch.key:
            networks[net].conf[ch]['key'] = oldch.key

        if oldch.autojoin:
            networks[net].conf[ch]['autojoin'] = oldch.autojoin

    del oldfile.networks[net]

for l in oldfile.listeners:
    key = l['port']
    if l.has_key('bind'):
        key = "%s:%d" % (l['bind'], l['port'])

    listeners.conf[key] = {}
    if l.has_key('network'):
        listeners.conf[key]['network'] = l['network']

    if l.has_key('password'):
        listeners.conf[key]['password'] = l['password']

    if l.has_key('ssl'):
        listeners.conf[key]['ssl'] = l['ssl']

conf.write()

#for i in networks:
#    print i
#    networks[i].write()

def write_nickserv_file():
    for entry in oldfile.nickserv_nicks:
        if entry.has_key('network'):
            entry['network'] = "*"
        print "%s\t%s\t%s" % (entry['nick'], entry['password'], entry['network'])

#listeners.write()

print "Warnings:"
for w in warnings:
    print w
