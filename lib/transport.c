
/*
	ctrlproxy: A modular IRC proxy
	(c) 2002-2008 Jelmer Vernooij <jelmer@nl.linux.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "transport.h"
#include <glib.h>

struct irc_transport *irc_transport_new_iochannel(GIOChannel *iochannel)
{
	struct irc_transport *ret = g_new0(struct irc_transport, 1);

	ret->incoming = iochannel;
	g_io_channel_ref(ret->incoming);

	return ret;
}

void free_irc_transport(struct irc_transport *transport)
{
	g_io_channel_unref(transport->incoming);
	
	transport->incoming = NULL;

	if (transport->outgoing_iconv != (GIConv)-1)
		g_iconv_close(transport->outgoing_iconv);
	if (transport->incoming_iconv != (GIConv)-1)
		g_iconv_close(transport->incoming_iconv);

	g_free(transport);
}
