#!/usr/bin/python
# Upgrade script for CtrlProxy
# Copyright (C) 2005 Jelmer Vernooij <jelmer@samba.org>

import sys
import os
from xml.dom import minidom, Node

class UnknownTagError(StandardError):
    def __init__(self,tag):
        StandardError.__init__(self, 'Unknown tag: %s' % tag.nodeName)

class OldConfigFile:
    def parsePlugins(self,node):
        for cn in node.childNodes:
            if cn.nodeName == 'plugin':
                self.plugins[cn.nodeName] = cn
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def parseServer(self,node):
        return node.attributes

    def parseServers(self,node):
        servers = []
        for cn in node.childNodes:
            if cn.nodeName == 'server':
                servers.add(self.parseServer(cn))
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def parseChannel(self,node):
        return node.attributes

    def parseListeners(self,name,node):
        for cn in node.childNodes:
            if cn.nodeName == 'ipv4' or cn.nodeName == 'ipv6':
                listener = {'network': name, 'port': cn.attributes['port'] }
                network.listeners.add(listener)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def parseNetwork(self,node):
        network = {'servers':[],'channels':[]}

        print node.attributes['name']

        for cn in node.childNodes:
            if cn.nodeName == 'servers':
                network.servers.add(self.parseServers(cn))
            elif cn.nodeName == 'channel':
                network.channels.add(self.parseChannel(cn))
            elif cn.nodeName == 'listen':
                self.parseListeners(network.name, cn)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

        self.networks[network.name] = network

    def parseNetworks(self,node):
        for cn in node.childNodes:
            if cn.nodeName == 'network':
                self.parseNetwork(cn) 
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

    def __init__(self,filename):
        self.doc = minidom.parse(filename)
        self.plugins = {}
        self.networks = {}
        node = self.doc.documentElement
        if node.nodeName != 'ctrlproxy':
            raise UnknownTagError(cn)

        for cn in node.childNodes:
            if cn.nodeName == 'networks':
                self.parseNetworks(cn)
            elif cn.nodeName == 'plugins':
                self.parsePlugins(cn)
            elif cn.nodeType == Node.ELEMENT_NODE:
                raise UnknownTagError(cn)

if len(sys.argv) > 1:
    oldfile = OldConfigFile(sys.argv[1])
else:
    oldfile = OldConfigFile("%s/.ctrlproxyrc" % os.getenv('HOME'))


