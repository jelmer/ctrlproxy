#!/usr/bin/python
# Upgrade script for CtrlProxy
# Copyright (C) 2005 Jelmer Vernooij <jelmer@samba.org>

import sys
import os
from xml.dom import minidom, Node

class UnknownTagError(StandardError):
    def __init__(self,tag):
        StandardError.__init__(self, 'Unknown tag: %s' % tag.nodeName)

class Channel:
    def __init__(self, name=None):
        self.name = name
        self.autojoin = False
        self.key = None

class Network:
    def __init__(self, name=None):
        self.name = name
        self.channels = {}
        self.servers = []

class Plugin:
    def __init__(self, name=None, autoload=True, config=None):
        if name.startswith('lib'):
            name = name[3:]

        self.name = name
        self.config = config
        self.autoload = autoload

class Config:
    def __init__(self):
        self.plugins = {}
        self.networks = {}
        self.listeners = []
        self.autosend_lines = []
        self.nickserv_nicks = []
        self.required_plugins = []

    def upgrade(self):
        # Throw out obsolete modules
        for pl in ['socket','strip','repl_memory']:
            if self.plugins.has_key(pl):
                self.plugins.pop(pl)

        if 'linestack_memory' in self.plugins:
            self.plugins.pop('linestack_memory')
            self.plugins['linestack_memory'] = Plugin(name='linestack_file')


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
                self.required_plugins.append('listener')
                self.listeners.append(listener)
            elif cn.nodeName == 'pipe':
                self.required_plugins.append('pipe')
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
                self.required_plugins.append('autosend')
                self.autosend_lines = {'network': network.name, 
                                       'data': cn.toxml()}
            elif cn.nodeName == 'nickserv':
                self.required_plugins.append('nickserv')
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
    oldfile = OldConfigFile(sys.argv[1])
else:
    oldfile = OldConfigFile("%s/.ctrlproxyrc" % os.getenv('HOME'))

oldfile.upgrade()

print oldfile.__dict__
