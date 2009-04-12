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
        self.assertEquals(str(l), ":origin PRIVMSG bla")

    def test_create_list_origin(self):
        l = irc.Line([":origin", "PRIVMSG", "bla"])
        self.assertTrue(l is not None)
        self.assertEquals("origin", l.origin)
        self.assertEquals(2, len(l))
        self.assertEquals("PRIVMSG", l[0])
        self.assertEquals("bla", l[1])
        self.assertEqual(str(l), ":origin PRIVMSG :bla")

    def test_create_list_no_origin(self):
        l = irc.Line(["PRIVMSG", "bla"])
        self.assertTrue(l is not None)
        self.assertEquals(None, l.origin)
        self.assertEquals(2, len(l))
        self.assertEquals("PRIVMSG", l[0])
        self.assertEquals("bla", l[1])
        self.assertEqual(str(l), "PRIVMSG :bla")

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


class BaseNetworkStateTests(unittest.TestCase):

    def test_create(self):
        s = irc.NetworkState("nick", "user", "host")
        self.assertTrue(s is not None)

    def test_info(self):
        s = irc.NetworkState("nick", "user", "host")
        self.assertTrue(s.info is not None)


class NetworkStateTestCase(unittest.TestCase):

    def setUp(self):
        super(NetworkStateTestCase, self).setUp()
        self.state = irc.NetworkState("nick", "user", "host")

    def test_handle_line(self):
        self.state.handle_line(":nick!user@host JOIN #foo")

    def test_channels_empty(self):
        self.assertEquals([], list(self.state.channels))
        
    def test_channels(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        channels = self.state.channels
        self.assertEquals(1, len(channels))
        self.assertEquals("#foo", list(channels)[0].name)
        self.assertEquals("#foo", channels["#foo"].name)

    def test_get_channel(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.assertEquals(self.state["#foo"].name, "#foo")

    def test_leave_channel(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.assertEquals(["#foo"], [c.name for c in self.state.channels])
        self.state.handle_line(":nick!user@host PART #foo")
        self.assertEquals([], [c.name for c in self.state.channels])
        self.state.handle_line(":nick!user@host PART #ba")

    def test_kicked_off_channel(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.assertEquals(["#foo"], [c.name for c in self.state.channels])
        self.state.handle_line(":othernick!user@host KICK #foo :nick")
        self.assertEquals([], [c.name for c in self.state.channels])

    def test_channel_mode_option(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.state.handle_line(":somenick MODE #foo +l 42")
        self.assertEquals("42", self.state["#foo"].mode_option["l"])

    def test_channel_mode_two_options(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.state.handle_line(":somenick MODE #foo +kl bla 42")
        self.assertEquals("42", self.state["#foo"].mode_option["l"])
        self.assertEquals("bla", self.state["#foo"].mode_option["k"])

    def test_channel_topic(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.state.handle_line(":someserver 332 nick #foo :The TOPIC")
        self.assertEquals("The TOPIC", self.state["#foo"].topic)

    def test_nick_channels(self):
        self.state.handle_line(":nick!user@host JOIN #foo")
        self.assertEquals(["#foo"], list(self.state["nick"].channels))


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


class NetworkNickTests(unittest.TestCase):

    def test_create(self):
        nick = irc.Nick("nick", "user", "host")
        self.assertEquals("nick", nick.nick)
        self.assertEquals("user", nick.username)
        self.assertEquals("host", nick.hostname)
        self.assertEquals("nick!user@host", nick.hostmask)


class QueryStackTests(unittest.TestCase):

    def setUp(self):
        self.stack = irc.QueryStack()

    def test_empty(self):
        self.assertEquals([], list(self.stack))

    def test_privmsg(self):
        self.stack.record("token", "PRIVMSG joe :bla")
        entries = list(self.stack)
        self.assertEquals(1, len(entries))
        self.assertEquals("token", entries[0][0])
        self.assertEquals("PRIVMSG", entries[0][1])

    def test_privmsg2(self):
        self.stack.record("token", "PRIVMSG joe :bla")
        self.stack.record("token", "PRIVMSG bla :bar")
        entries = list(self.stack)
        self.assertEquals(2, len(entries))
        self.assertEquals("token", entries[0][0])
        self.assertEquals("PRIVMSG", entries[0][1])
        self.assertEquals("token", entries[1][0])
        self.assertEquals("PRIVMSG", entries[1][1])

    def test_unknown_command(self):
        self.stack.record("token", "IDONTKNOW joe :bla")
        entries = list(self.stack)
        self.assertEquals(1, len(entries))
        self.assertEquals("token", entries[0][0])
        self.assertEquals(None, entries[0][1])
        self.assertEquals("token",
            self.stack.response(":server 421 user :No such command"))
        self.assertEquals([], list(self.stack))

    def test_removes_old_unreplied(self):
        self.stack.record("token1", "PRIVMSG joe :bla")
        self.stack.record("token2", "UNKNOWN joe :bla")
        entries = list(self.stack)
        self.assertEquals(2, len(entries))
        self.assertEquals("token1", entries[0][0])
        self.assertEquals("token2", entries[1][0])
        self.assertEquals("token2",
            self.stack.response(":server 421 user :No such command"))
        # FIXME: self.assertEquals([], list(self.stack))


class TransportTests(unittest.TestCase):

    def test_create(self):
        t = irc.Transport()


class DummyTransport(object):
    """Trivial Transport."""

    def __init__(self):
        self._sent_lines = []

    def str_lines(self):
        return [str(l) for l in self._sent_lines]

    def send_line(self, line):
        self._sent_lines.append(line)

    def is_connected(self):
        return True


class ClientTests(unittest.TestCase):

    def test_create(self):
        c = irc.Client(DummyTransport(), "myorigin", "description")

    def test_state(self):
        c = irc.Client(DummyTransport(), "myorigin", "description")
        self.assertTrue(c.state is not None)

    def test_send_line(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        c.send_line(":server NOTICE mynick :bla")
        self.assertEquals(1, len(t._sent_lines))
        self.assertEquals(":server NOTICE mynick :bla", str(t._sent_lines[0]))

    def test_default_target(self):
        c = irc.Client(DummyTransport(), "myorigin", "description")
        self.assertEquals("*", c.default_target)

    def test_default_origin(self):
        c = irc.Client(DummyTransport(), "myorigin", "description")
        self.assertEquals("myorigin", c.default_origin)

    def test_own_hostmask_not_known(self):
        c = irc.Client(DummyTransport(), "myorigin", "description")
        self.assertEquals(None, c.own_hostmask)

    def test_send_motd(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        c.send_motd(["bla", "blie bloe"])
        self.assertEquals([
            ':myorigin 375 * :Start of MOTD',
            ':myorigin 372 * :bla',
            ':myorigin 372 * :blie bloe',
            ':myorigin 376 * :End of MOTD'], t.str_lines())


class ClientSendStateTests(unittest.TestCase):

    def setUp(self):
        super(ClientSendStateTests, self).setUp()
        self.transport = DummyTransport()
        self.client = irc.Client(self.transport, "myorigin", "description")
        self.state = irc.NetworkState("nick", "user", "host")

    def test_empty(self):
        self.client.send_state(self.state)
        self.assertEquals([], self.transport._sent_lines)
