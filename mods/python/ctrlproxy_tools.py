# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
#
# Daniel Poelzleithner <ctrlproxy@poelzi.org>
#

# some functions are taken from python-irclib-0.4.1

"""
This functions are accessable over

ctrlproxy.tools
"""

def ip_numstr_to_quad(num):
    """Convert an IP number as an integer given in ASCII
    representation (e.g. '3232235521') to an IP address string
    (e.g. '192.168.0.1')."""
    n = long(num)
    p = map(str, map(int, [n >> 24 & 0xFF, n >> 16 & 0xFF,
                           n >> 8 & 0xFF, n & 0xFF]))
    return string.join(p, ".")

def ip_quad_to_numstr(quad):
    """Convert an IP address string (e.g. '192.168.0.1') to an IP
    number as an integer given in ASCII representation
    (e.g. '3232235521')."""
    p = map(long, string.split(quad, "."))
    s = str((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3])
    if s[-1] == "L":
        s = s[:-1]
    return s

def nm_to_n(s):
    """Get the nick part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "!")[0]

def nm_to_uh(s):
    """Get the userhost part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "!")[1]

def nm_to_h(s):
    """Get the host part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    return string.split(s, "@")[1]

def nm_to_u(s):
    """Get the user part of a nickmask.

    (The source of an Event is a nickmask.)
    """
    s = string.split(s, "!")[1]
    return string.split(s, "@")[0]

def parse_nick_modes(mode_string):
    """Parse a nick mode string.

    The function returns a list of lists with three members: sign,
    mode and argument.  The sign is \"+\" or \"-\".  The argument is
    always None.

    Example:

    >>> ctrlproxy.tools.parse_nick_modes(\"+ab-c\")
    [['+', 'a', None], ['+', 'b', None], ['-', 'c', None]]
    """

    return _parse_modes(mode_string, "")

def parse_channel_modes(mode_string):
    """Parse a channel mode string.

    The function returns a list of lists with three members: sign,
    mode and argument.  The sign is \"+\" or \"-\".  The argument is
    None if mode isn't one of \"b\", \"k\", \"l\", \"v\" or \"o\".

    Example:

    >>> ctrlproxy.tools.parse_channel_modes(\"+ab-c foo\")
    [['+', 'a', None], ['+', 'b', 'foo'], ['-', 'c', None]]
    """

    return _parse_modes(mode_string, "bklvo")

def _parse_modes(mode_string, unary_modes=""):
    """[Internal]"""
    modes = []
    arg_count = 0

    # State variable.
    sign = ""

    a = string.split(mode_string)
    if len(a) == 0:
        return []
    else:
        mode_part, args = a[0], a[1:]

    if mode_part[0] not in "+-":
        return []
    for ch in mode_part:
        if ch in "+-":
            sign = ch
        elif ch == " ":
            collecting_arguments = 1
        elif ch in unary_modes:
            if len(args) >= arg_count + 1:
                modes.append([sign, ch, args[arg_count]])
                arg_count = arg_count + 1
            else:
                modes.append([sign, ch, None])
        else:
            modes.append([sign, ch, None])
    return modes

import ctrlproxy
import os
import os.path
import libxml2

def get_xml_module_names(typ = None):
	"""Returns a list of modules for which a Moduleinfo is available"""
	path = os.path.join(ctrlproxy.get_path("prefix"),"share","ctrlproxy","help")
	dl = os.listdir(path)
	rv = []
	for i in dl:
		if i[-8:] == ".mod.xml":
			if typ != None:
				d = libxml2.parseFile(os.path.join(path, i))
				#d.dump(1)
				xl = d.xpathEval("//modulemeta/type")
				for node in xl:
					if node.content == typ:
						rv.append(i[:-8])
				d.freeDoc()
			else:
				rv.append(i[:-8])
	return rv


class Moduleinfo:
	"""Returns informations of a module"""
	def __init__(self, name):
		self.name = name
		self.path = os.path.join(ctrlproxy.get_path("prefix"),"share","ctrlproxy","help","%s.mod.xml" %name)
		self.doc = libxml2.parseFile(self.path)
		
