[![Build Status](https://travis-ci.org/jelmer/ctrlproxy.png?branch=master)](https://travis-ci.org/jelmer/ctrlproxy)

ctrlproxy
=========

For a quick start, see the bottom of this file.

If you are upgrading from an older version of ctrlproxy, please read the
file UPGRADING.

Introduction
------------
CtrlProxy is a lightweight IRC proxy. It allows you to keep a permanent
connection open to an IRC server and using that connection from wherever
you would like to use IRC, without the need to disconnect and connect to IRC
continuously while missing text.

Information for users upgrading from 2.6 or 2.7-preXX:
------------------------------------------------------

The configuration is now maintained as a set of flat-text files in
~/.ctrlproxy/. Existing configuration files can be upgraded by
running the experimental upgrade utility available in the directory 'scripts'
in the source tarball.

Features
--------
 * Connect to one server with many clients under one nick transparently
 * Connect to multiple servers using only one process
 * CTCP support when no client is attached
 * Transparent detaching and attaching of clients
 * Password support
 * Replication support
 * Auto-Away support
 * Keeping track of events occurring
 * Direct, inetd-style interfacing with local IRC servers (such as BitlBee)
 * Works transparently
 * SSL support
 * Custom logging in any format you specify
 * Flood protection
 * IPv6 Support
 * Automatic NickServ support
 * Low memory, CPU and bandwidth requirements
 * Management of running instances using a command-line tool or
   over IRC.

Dependencies
------------
 * GNU glib
 * GNU TLS (optional, for SSL support)
 * Python (optional, for Python scripting)

Building
--------

ctrlproxy can be installed by running::

	$ ./configure
	$ make
	$ make install

If you have a git checkout, run::

	$ ./autogen.sh

before running ./configure, make and make install

Quick start
-----------

1. Install ctrlproxy

2. Run ctrlproxy --init

Enter a password and a port to ctrlproxy should listen on.

CtrlProxy will write a configuration to ~/.ctrlproxy. You may
want to edit ~/.ctrlproxy/config.

3. Run ctrlproxy --daemon or ctrlproxy

4. Connect to ctrlproxy from your IRC client on the port you specified earlier.
For more information on connecting with your favorite IRC client, see the
user guide.

Documentation
-------------
Most documentation is in the user guide and the
manpages: ctrlproxy(1) and ctrlproxy\_config(5).
The example config.example file might also be of some use.

Reporting Bugs
--------------
Bugs can be reported in GitHub at git://github.com/jelmer/ctrlproxy.
