###########################################################################
#    Copyright (C) 2004 by daniel.poelzleithner                                      
#    <poelzi@poelzi.org>                                                             
#
# Copyright: See COPYING file that comes with this distribution
#
###########################################################################

import ctrlproxy

static = {}
static["test"] = 1

def test(line):
	print "bla"
	print static["test"]
	static["test"] += 1

def init():
#	ctrlproxy.add_rcv_hook(test)
#	static["test"] += 1
