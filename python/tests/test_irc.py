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

    def test_nicks(self):
        s = irc.ChannelState("#foo")
        self.assertTrue(s.nicks is not None)
        self.assertEquals(0, len(s.nicks))
        self.assertRaises(KeyError, lambda: s.nicks["nick"])

    def test_add_nick(self):
        s = irc.ChannelState("#foo")
        s.nicks.add(irc.Nick("me!foo@bar"))
        self.assertEquals(1, len(s.nicks))
        self.assertEquals("me", s.nicks["me"].nick)

    def test_add_nick_mode(self):
        s = irc.ChannelState("#foo")
        s.nicks.add(irc.Nick("me!foo@bar"), "+o")
        self.assertEquals(1, len(s.nicks))
        self.assertEquals("me", s.nicks["me"].nick)
        self.assertEquals("+o", s.nicks.nick_mode("me"))

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

    def test_set_modes(self):
        s = irc.ChannelState("#foo")
        s.modes = "+n"
        self.assertEquals("+n", s.modes)

    def test_get_topic(self):
        s = irc.ChannelState("#foo")
        self.assertEquals(None, s.topic)

    def test_set_topic(self):
        s = irc.ChannelState("#foo")
        s.topic = None
        s.topic = "BLA"
        self.assertEquals("BLA", s.topic)

    def test_get_topic_time(self):
        s = irc.ChannelState("#foo")
        self.assertEquals(0, s.topic_set_time)

    def test_get_topic_time_set(self):
        s = irc.ChannelState("#foo")
        s.topic_set_time = 42
        self.assertEquals(42, s.topic_set_time)

    def test_creationtime(self):
        s = irc.ChannelState("#foo")
        self.assertEquals(0, s.creation_time)

    def test_creationtime_set(self):
        s = irc.ChannelState("#foo")
        s.creation_time = 423423
        self.assertEquals(423423, s.creation_time)



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

    def test_add_channel(self):
        c = irc.ChannelState("#foo")
        self.state.add(c)
        self.assertEquals(c.name, self.state["#foo"].name)

    def test_add_nick(self):
        n = irc.Nick("me!user@host")
        self.state.add(n)
        self.assertEquals(n.nick, self.state["me"].nick)

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

    def test_set_hostmask(self):
        nick = irc.Nick("nick", "user", "host")
        nick.hostmask = "my!new@mask"
        self.assertEquals("my", nick.nick)
        self.assertEquals("new", nick.username)
        self.assertEquals("mask", nick.hostname)
        self.assertEquals("my!new@mask", nick.hostmask)

    def test_set_nick(self):
        nick = irc.Nick("nick", "user", "host")
        nick.nick = "my"
        self.assertEquals("my", nick.nick)
        self.assertEquals("user", nick.username)
        self.assertEquals("host", nick.hostname)
        self.assertEquals("my!user@host", nick.hostmask)

    def test_set_username(self):
        nick = irc.Nick("nick", "user", "host")
        nick.username = "uname"
        self.assertEquals("nick", nick.nick)
        self.assertEquals("uname", nick.username)
        self.assertEquals("host", nick.hostname)
        self.assertEquals("nick!uname@host", nick.hostmask)

    def test_default_mode(self):
        nick = irc.Nick("nick", "user", "host")
        self.assertEquals(None, nick.modes)

    def test_set_mode(self):
        nick = irc.Nick("nick", "user", "host")
        nick.modes = "+o"
        self.assertEquals("+o", nick.modes)

    def test_remove_mode(self):
        nick = irc.Nick("nick", "user", "host")
        nick.modes = ""
        self.assertEquals(None, nick.modes)


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

    def test_send_luserchannels(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        c.send_luserchannels(42)
        self.assertEquals([':myorigin 254 * 42 :channels formed'],
                t.str_lines())

    def test_send_netsplit_none(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        c.send_netsplit("myserver")
        self.assertEquals([], t.str_lines())

    def test_send_channel_mode_none(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch = irc.ChannelState("#ch")
        c.send_channel_mode(ch)
        self.assertEquals([], t.str_lines())

    def test_send_banlist(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch = irc.ChannelState("#ch")
        c.send_banlist(ch)
        self.assertEquals([':myorigin 368 * #ch :End of channel ban list'],
            t.str_lines())

    def test_send_nameslist(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch = irc.ChannelState("#ch")
        c.send_nameslist(ch)
        self.assertEquals([':myorigin 366 * #ch :End of /NAMES list'],
            t.str_lines())

    def test_send_channel_state(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch = irc.ChannelState("#ch")
        c.send_channel_state(ch)
        self.assertEquals([
            'JOIN #ch', 
            ':myorigin 366 * #ch :End of /NAMES list'],
            t.str_lines())

    def test_send_channel_state_diff(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch1 = irc.ChannelState("#ch")
        ch2 = irc.ChannelState("#ch")
        c.send_channel_state_diff(ch1, ch2)
        self.assertEquals([], t.str_lines())

    def test_send_topic(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        ch = irc.ChannelState("#ch")
        c.send_topic(ch, False)
        self.assertEquals([], t.str_lines())
        c.send_topic(ch, True)
        self.assertEquals([':myorigin 331 * #ch :No topic set'], t.str_lines())
        t._sent_lines = []
        ch.topic = "LALALA"
        ch.topic_set_time = 4324324L
        c.send_topic(ch)
        self.assertEquals([':myorigin 332 * #ch :LALALA'], t.str_lines())

    def test_last_ping(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        self.assertEquals(0, c.last_ping)

    def test_last_pong(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        self.assertEquals(0, c.last_pong)

    def test_description(self):
        t = DummyTransport()
        c = irc.Client(t, "myorigin", "description")
        self.assertEquals("description", c.description)


class ClientSendStateTests(unittest.TestCase):

    def setUp(self):
        super(ClientSendStateTests, self).setUp()
        self.transport = DummyTransport()
        self.client = irc.Client(self.transport, "myorigin", "description")
        self.state = irc.NetworkState("nick", "user", "host")

    def assertLines(self, lines):
        self.assertEquals(lines, self.transport.str_lines())

    def test_empty(self):
        self.client.send_state(self.state)
        self.assertLines([])

    def test_with_channel(self):
        ch = irc.ChannelState("#foo")
        ch.topic = "bla la"
        self.state.add(ch)
        self.client.send_state(self.state)
        self.assertLines([
            'JOIN #foo', 
            ':myorigin 332 * #foo :bla la', 
            ':myorigin 366 * #foo :End of /NAMES list'])

    def test_diff_with_channel(self):
        ch1 = irc.ChannelState("#foo")
        ch2 = irc.ChannelState("#foo")
        ch2.topic = "bla la"
        self.state.add(ch1)
        state2 = irc.NetworkState("nick", "user", "host")
        state2.add(ch2)
        self.client.send_state_diff(self.state, state2)
        self.assertLines(['TOPIC #foo :bla la'])

    def test_empty_diff(self):
        self.client.send_state_diff(self.state, self.state)
        self.assertLines([])
