# Copyright (C) 2005-2008 Jelmer Vernooij <jelmer@samba.org>
 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import irc
import unittest

class LineTestCase(unittest.TestCase):

    def test_create_str(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertTrue(l is not None)

    def test_create_other(self):
        l = irc.Line(":origin PRIVMSG bla")
        newl = irc.Line(l)
        self.assertEquals(str(l), str(newl))

    def test_create_inval(self):
        self.assertRaises(TypeError, irc.Line, 1)

    def test__str__(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertEquals(str(l), ":origin PRIVMSG bla")

    def test_repr(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertEquals(repr(l), "Line(':origin PRIVMSG bla')")

    def test_get_nick(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertEquals(l.get_nick(), "origin")

    def test_len(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertEquals(len(l), 2)

    def test_getitem(self):
        l = irc.Line(":origin PRIVMSG bla")
        self.assertEquals(l[0], "PRIVMSG")
        self.assertEquals(l[1], "bla")
        self.assertRaises(KeyError, lambda: l[2])


class ChannelStateTests(unittest.TestCase):

    def test_create(self):
        s = irc.ChannelState("#foo")
        self.assertTrue(s is not None)
        self.assertEquals("#foo", s.name)

    def test_set_key(self):
        s = irc.ChannelState("#foo")
        self.assertRaises(KeyError, lambda:s.mode_option['k'])
        s.mode_option['k'] = "bar"
        self.assertEquals("bar", s.mode_option['k'])

    def test_limit(self):
        s = irc.ChannelState("#foo")
        self.assertRaises(KeyError, lambda:s.mode_option['l'])
        s.mode_option['l'] = "42"
        self.assertEquals("42", s.mode_option['l'])

    def test_get_modes(self):
        s = irc.ChannelState("#foo")
        self.assertEquals("", s.modes)

    def test_get_topic(self):
        s = irc.ChannelState("#foo")
        self.assertEquals(None, s.topic)


class NetworkStateTests(unittest.TestCase):
    def test_create(self):
        s = irc.NetworkState("nick", "user", "host")
        self.assertTrue(s is not None)

    def test_info(self):
        s = irc.NetworkState("nick", "user", "host")
        self.assertTrue(s.info is not None)

    def test_handle_line(self):
        s = irc.NetworkState("nick", "user", "host")
        s.handle_line(irc.Line(":nick!user@host JOIN #foo"))

    def test_channels_empty(self):
        s = irc.NetworkState("nick", "user", "host")
        self.assertEquals([], s.channels)
        
    def test_channels(self):
        s = irc.NetworkState("nick", "user", "host")
        s.handle_line(irc.Line(":nick!user@host JOIN #foo"))
        channels = s.channels
        self.assertEquals(1, len(channels))
        self.assertEquals("#foo", channels[0].name)

    def test_get_channel(self):
        s = irc.NetworkState("nick", "user", "host")
        s.handle_line(irc.Line(":nick!user@host JOIN #foo"))
        self.assertEquals(s["#foo"].name, "#foo")


class NetworkInfoTests(unittest.TestCase):
    def test_create(self):
        s = irc.NetworkInfo()
        self.assertTrue(s is not None)

    def test_prefix_by_mode(self):
        s = irc.NetworkInfo()
        self.assertEquals("@", s.get_prefix_by_mode("o"))

    def test_irccmp_samecase(self):
        s = irc.NetworkInfo()
        self.assertEquals(s.irccmp("bla", "bla"), 0)

    def test_irccmp_diff(self):
        s = irc.NetworkInfo()
        self.assertNotEqual(s.irccmp("bla", "bloe"), 0)

    def test_irccmp_diffcase(self):
        s = irc.NetworkInfo()
        self.assertEqual(s.irccmp("BlA", "bla"), 0)

    def test_is_prefix(self):
        s = irc.NetworkInfo()
        self.assertTrue(s.is_prefix("@"))
        self.assertFalse(s.is_prefix("a"))

    def test_is_channelname(self):
        s = irc.NetworkInfo()
        self.assertTrue(s.is_channelname("#bla"))
        self.assertFalse(s.is_channelname("nietchannel"))
