/*
	ctrlproxy: A modular IRC proxy
	(c) 2005-2006 Jelmer Vernooij <jelmer@jelmer.uk>

	Quassel listen on ports

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

#include "internals.h"
#include "libirc/listener.h"
#include "ssl.h"
#include "quassel.h"

#define QUASSEL_MAX_PROTOS 15

struct quassel_message {
	struct quassel_message_entry {
		uint32_t type;
		uint32_t length;
		char *data;
	} entry;
};

static gboolean handle_client_quassel_negotiation(GIOChannel *ioc, struct pending_client *pc);

static gboolean handle_quassel_msg(struct pending_client *pc, struct quassel_message *msg) {
	/* FIXME */
	listener_log(LOG_WARNING, pc->listener, "Ignoring Quassel message");
	return FALSE;
}

static gboolean read_quassel_response(GIOChannel *ioc, uint32_t *response_len, uint8_t **response) {
	gsize read;
	GIOStatus status;

	status = g_io_channel_read_chars(ioc, (char *)response_len, 4, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	*response_len = ntohl(*response_len);

	*response = g_malloc(*response_len);

	status = g_io_channel_read_chars(ioc, (char *)*response, *response_len, &read,
									 NULL);
	if (status != G_IO_STATUS_NORMAL) {
		g_free(*response);
		return FALSE;
	}

	return TRUE;
}

gboolean handle_client_quassel_data(GIOChannel *ioc, struct pending_client *cl)
{
	if (cl->quassel.state == QUASSEL_STATE_NEW) {
		return handle_client_quassel_negotiation(ioc, cl);
	} else {
		uint32_t response_len;
		uint8_t *response;
		int offset = 0;
		uint32_t num_elements, i;
		struct quassel_message msg;

		if (!read_quassel_response(ioc, &response_len, &response)) {
			return FALSE;
		}
		num_elements = ntohl(*(uint32_t *)(response));
		offset += 4;
		listener_log(LOG_TRACE, cl->listener, "Number of elements in list: %d", num_elements);
		for (i = 0; i < num_elements; i++) {
			uint32_t el_type = ntohl(*(uint32_t *)(response+offset));
			uint32_t el_len;
			offset += 4;
			offset += 1; /* Unknown byte */
			el_len = ntohl(*(uint32_t *)(response+offset));
			listener_log(LOG_TRACE, cl->listener, "Entry[%d]: type: %d, length: %d", i, el_type, el_len);
			offset += el_len;
		}

		handle_quassel_msg(cl, &msg);

		/* TODO(jelmer) */
		g_free(response);
	}

	return TRUE;
}

gboolean listener_probe_quassel(gchar *header, size_t len)
{
	if (len < 1) {
		return FALSE;
	}

	if (header[0] != QUASSEL_MAGIC1) {
		return FALSE;
	}

	return TRUE;
}

static gboolean handle_client_quassel_negotiation(GIOChannel *ioc, struct pending_client *cl)
{
	int i;
	uint32_t protos[QUASSEL_MAX_PROTOS+1];
	uint32_t response;
	uint8_t features;
	gsize read;
	GIOStatus status;
	gchar header[4];
	GError *error = NULL;

	status = g_io_channel_read_chars(ioc, header, 3, &read, &error);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	if (header[0] != QUASSEL_MAGIC2 && header[1] != QUASSEL_MAGIC3) {
		listener_log(LOG_TRACE, cl->listener, "Almost detected Quassel Client.");
		return FALSE;
	}

	features = header[2];

	listener_log(LOG_TRACE, cl->listener, "Detected Quassel Client. Options: %s%s.",
				 (features & QUASSEL_MAGIC_OPT_SSL)?"ssl, ":"",
				 (features & QUASSEL_MAGIC_OPT_COMPRESSION)?"compression":"");

	for (i = 0; i < QUASSEL_MAX_PROTOS; i++) {
		status = g_io_channel_read_chars(ioc, (char *)&protos[i], 4, &read, &error);

		if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
			if (error != NULL)
				g_error_free(error);
			return FALSE;
		}

		protos[i] = ntohl(protos[i]);
		listener_log(LOG_TRACE, cl->listener, "Supported quassel protocol: %d.",
					 (protos[i] &~ QUASSEL_PROTO_SENTINEL));

		if (protos[i] & QUASSEL_PROTO_SENTINEL) {
			protos[i] &= ~QUASSEL_PROTO_SENTINEL;
			break;
		}
	}

	/* FIXME: Support picking something other than this. Actually negotiate. */
	cl->quassel.options = QUASSEL_PROTO_DATA_STREAM;
	response = htonl(cl->quassel.options);
	status = g_io_channel_write_chars(cl->connection, (char *)&response, 4, &read, NULL);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	g_io_channel_flush(cl->connection, NULL);

	cl->quassel.state = QUASSEL_STATE_NEGOTIATED;

	return TRUE;
}
