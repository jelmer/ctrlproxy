/*
	ctrlproxy: A modular IRC proxy
	(c) 2016 Jelmer Vernooij <jelmer@jelmer.uk>

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

#define SUPPORTED_FEATURES 0 /* For now, don't support compression or SSL */
#define QUASSEL_MAX_PROTOS 15

struct quassel_message {
	char *raw_data;
	uint32_t num_elements;
	struct quassel_message_element {
		uint32_t type;
		uint32_t length;
		char *data;
	} *elements;
};

static gboolean handle_client_quassel_negotiation(GIOChannel *ioc, struct pending_client *pc);
static void free_quassel_message(struct quassel_message *msg);

static gboolean quassel_message_get_item(struct irc_listener *listener, struct quassel_message *msg, const char *name, uint32_t required_type, char **value, uint32_t *value_length)
{
	int i;
	uint32_t num_elements = msg->num_elements;

	if (num_elements % 2 == 1) {
		listener_log(LOG_ERROR, listener, "Uneven number of elements (%d). Ignoring last element.",
					 num_elements);
		num_elements-=1;
	}

	for (i = 0; i < num_elements; i+=2) {
		if (msg->elements[i].type != QUASSEL_TYPE_BYTESTRING) {
			listener_log(LOG_ERROR, listener, "Expected type bytestring (%d) for element %d in message, got %d",
						 QUASSEL_TYPE_BYTESTRING, i, msg->elements[i].type);
			return FALSE;
		}

		if (strlen(name) != msg->elements[i].length) {
			continue;
		}

		if (strncmp(name, msg->elements[i].data, msg->elements[i].length) != 0) {
			continue;
		}

		if (msg->elements[i+1].type != required_type) {
			listener_log(LOG_WARNING, listener,
						 "Unexpected type %d for element %d (%s) while expecting %d, ignoring.",
						 msg->elements[i+1].type, i+1, name, required_type);
			continue;
		}

		*value = msg->elements[i+1].data;
		*value_length = msg->elements[i+1].length;
		return TRUE;
	}
	return FALSE;
}

static gboolean quassel_message_get_string(struct irc_listener *listener, struct quassel_message *msg, const char *name, char **value)
{
	char *raw_data;
	uint32_t len;
	GError *error = NULL;
	gsize bytes_read, bytes_written;

	if (!quassel_message_get_item(listener, msg, name, QUASSEL_TYPE_STRING, &raw_data, &len)) {
		return FALSE;
	}

	/* TODO(jelmer): Create an iconv handle for this conversion */
	/* TODO(jelmer): Convert to locale rather than UTF-8 */
	*value = g_convert(raw_data, len, "UTF-8", "UTF-16BE", &bytes_read, &bytes_written, &error);

	if (*value == NULL) {
		listener_log(LOG_ERROR, listener, "Unable to convert UCS-2 string for %s to UTF-8.",
			name);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static void quassel_message_add_item(struct quassel_message *msg, uint32_t type, char *data, size_t length)
{
	msg->elements = g_renew(struct quassel_message_element, msg->elements, msg->num_elements+1);
	msg->elements[msg->num_elements].type = type;
	msg->elements[msg->num_elements].length = length;
	msg->elements[msg->num_elements].data = data;
	msg->num_elements += 1;
}

static void quassel_message_add_bytestring(struct quassel_message *msg, const char *data)
{
	g_assert(msg->raw_data == NULL);
	quassel_message_add_item(msg, QUASSEL_TYPE_BYTESTRING, g_strdup(data), strlen(data));
}

static void quassel_message_add_string(struct quassel_message *msg, const char *data)
{
	gchar *encoded;
	gsize bytes_read, bytes_written;
	GError *error = NULL;

	g_assert(msg->raw_data == NULL);

	/* TODO(jelmer): Create an iconv handle for this conversion */
	/* TODO(jelmer): Convert to locale rather than UTF-8 */
	encoded = g_convert(data, strlen(data), "UTF-16BE", "UTF-8", &bytes_read, &bytes_written, &error);
	g_assert(encoded != NULL);
	quassel_message_add_item(msg, QUASSEL_TYPE_STRING, encoded, bytes_written);
}

static void quassel_message_add_void(struct quassel_message *msg)
{
	g_assert(msg->raw_data == NULL);
	quassel_message_add_item(msg, QUASSEL_TYPE_VOID, g_new0(char, 1), 0);
}

static void quassel_message_add_uint(struct quassel_message *msg, uint32_t data)
{
	guint32 *raw_data = g_new0(uint32_t, 1);
	g_assert(msg->raw_data == NULL);
	*raw_data = htonl(data);
	quassel_message_add_item(msg, QUASSEL_TYPE_UINT, (char *)raw_data, sizeof(uint32_t));
}

static void quassel_message_add_bool(struct quassel_message *msg, gboolean data)
{
	gboolean *raw_data = g_new0(gboolean, 1);
	g_assert(msg->raw_data == NULL);
	*raw_data = data;
	quassel_message_add_item(msg, QUASSEL_TYPE_BOOL, (char *)raw_data, 1);
}

static gboolean write_quassel_data(GIOChannel *ioc, char *data, gsize length)
{
	guint32 encoded = htonl(length);
	gsize written;
	GError *error = NULL;
	GIOStatus status;

	status = g_io_channel_write_chars(ioc, (char *)&encoded, 4, &written, NULL);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	g_assert(written == 4);

	status = g_io_channel_write_chars(ioc, (char *)data, length, &written, NULL);

	if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
		if (error != NULL)
			g_error_free(error);
		return FALSE;
	}

	g_io_channel_flush(ioc, NULL);

	return TRUE;
}

static gboolean write_quassel_message(GIOChannel *ioc, struct quassel_message *msg)
{
	guint32 size, offset = 0, i;
	char *data;
	gboolean ret;

	size = 4; /* Number of elements */

	for (i = 0; i < msg->num_elements; i++) {
		size += 4 + 4 + 1 + msg->elements[i].length;
	}

	data = g_new0(char, size);
	*((uint32_t *)data) = htonl(msg->num_elements);
	offset += 4;

	for (i = 0; i < msg->num_elements; i++) {
		*((uint32_t *)data+offset) = htonl(msg->elements[i].type);
		offset += 4;
		offset += 1; /* unknown byte */
		*((uint32_t *)data+offset) = htonl(msg->elements[i].length);
		offset += 4;
		memcpy(data+offset, msg->elements[i].data, msg->elements[i].length);
		offset += msg->elements[i].length;
	}
	g_assert(offset == size);

	ret = write_quassel_data(ioc, data, size);

	g_free(data);

	return ret;
}

static gboolean handle_ClientInit(struct pending_client *pc, struct quassel_message *msg)
{
	struct quassel_message reply;

	if (!quassel_message_get_string(pc->listener, msg, "ClientVersion", &pc->quassel.client_version)) {
		listener_log(LOG_WARNING, pc->listener, "Unable to get ClientVersion.");
	} else {
		listener_log(LOG_INFO, pc->listener, "Client is using %s", pc->quassel.client_version);
	}

	/* TODO(jelmer): Could also send ClientInitReject here. */

	reply.raw_data = NULL;
	reply.num_elements = 0;
	reply.elements = NULL;

	quassel_message_add_bytestring(&reply, "MsgType");
	quassel_message_add_string(&reply, "ClientInitAck");

	quassel_message_add_bytestring(&reply, "CoreFeatures");
	quassel_message_add_uint(&reply, SUPPORTED_FEATURES);

	quassel_message_add_bytestring(&reply, "StorageBackends");
	quassel_message_add_void(&reply);

	quassel_message_add_bytestring(&reply, "LoginEnabled");
	quassel_message_add_bool(&reply, TRUE);

	quassel_message_add_bytestring(&reply, "Configured");
	quassel_message_add_bool(&reply, TRUE);

	write_quassel_message(pc->connection, &reply);

	free_quassel_message(&reply);

	return TRUE;
}

static gboolean handle_quassel_msg(struct pending_client *pc, struct quassel_message *msg)
{
	char *msgtype = NULL;
	gboolean ret = FALSE;

	if (!quassel_message_get_string(pc->listener, msg, "MsgType", &msgtype)) {
		listener_log(LOG_ERROR, pc->listener, "Message without MsgType element");
		return FALSE;
	}

	if (!strcmp(msgtype, "ClientInit")) {
		ret = handle_ClientInit(pc, msg);
	} else {
		listener_log(LOG_WARNING, pc->listener, "Ignoring Quassel message with type %s", msgtype);
	}

	g_free(msgtype);
	return ret;
}

static gboolean read_quassel_response(struct irc_listener *listener, GIOChannel *ioc, uint32_t *response_len, char **response) {
	gsize read;
	GIOStatus status;

	status = g_io_channel_read_chars(ioc, (char *)response_len, 4, &read, NULL);
	if (status != G_IO_STATUS_NORMAL) {
		return FALSE;
	}

	*response_len = ntohl(*response_len);

	*response = g_new0(char, *response_len);

	status = g_io_channel_read_chars(ioc, *response, *response_len, &read,
									 NULL);
	if (status != G_IO_STATUS_NORMAL) {
		g_free(*response);
		return FALSE;
	}

	if (*response_len != read) {
		listener_log(LOG_TRACE, listener, "Expected %d bytes, only read %d",
					 *response_len, read);
		g_free(*response);
		return FALSE;
	}

	return TRUE;
}

static void free_quassel_message(struct quassel_message *msg)
{
	if (msg->raw_data != NULL) {
		g_free(msg->raw_data);
	} else {
		int i;
		for (i = 0; i < msg->num_elements; i++) {
			g_free(msg->elements[i].data);
		}
	}
	g_free(msg->elements);
}

static gboolean read_quassel_message(struct irc_listener *listener, GIOChannel *ioc, struct quassel_message *msg)
{
	uint32_t response_len = 0;
	char *response = NULL;
	uint32_t offset = 0;
	uint32_t i;

	if (!read_quassel_response(listener, ioc, &response_len, &response)) {
		return FALSE;
	}

	if (response_len < 4) {
		return FALSE;
	}

	msg->raw_data = response;
	msg->num_elements = ntohl(*(uint32_t *)(response));
	offset += 4;

	msg->elements = g_new0(struct quassel_message_element, msg->num_elements);

	for (i = 0; i < msg->num_elements; i++) {
		if (response_len - offset < 9) {
			listener_log(LOG_ERROR, listener, "Not enough data reading message");
			free_quassel_message(msg);
			return FALSE;
		}
		msg->elements[i].type = ntohl(*(uint32_t *)(response+offset));
		offset += 4;
		offset += 1; /* Unknown byte */
		msg->elements[i].length = ntohl(*(uint32_t *)(response+offset));
		offset += 4;
		if (response_len - offset < msg->elements[i].length) {
			listener_log(LOG_ERROR, listener, "Not enough data reading message contents");
			free_quassel_message(msg);
			return FALSE;
		}
		msg->elements[i].data = msg->raw_data + offset;
		offset += msg->elements[i].length;
	}

	return TRUE;
}

gboolean handle_client_quassel_data(GIOChannel *ioc, struct pending_client *cl)
{
	if (cl->quassel.state == QUASSEL_STATE_NEW) {
		return handle_client_quassel_negotiation(ioc, cl);
	} else if (cl->quassel.state == QUASSEL_STATE_NEGOTIATED) {
		struct quassel_message msg;

		if (!read_quassel_message(cl->listener, ioc, &msg)) {
			return FALSE;
		}

		handle_quassel_msg(cl, &msg);

		free_quassel_message(&msg);
	} else {
		listener_log(LOG_ERROR, cl->listener, "Unknown quassel client state %d",
					 cl->quassel.state);
		return FALSE;
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
	uint32_t num_protos;
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

	num_protos = 0;
	for (i = 0; i < QUASSEL_MAX_PROTOS; i++) {
		status = g_io_channel_read_chars(ioc, (char *)&protos[i], 4, &read, &error);

		if (status != G_IO_STATUS_NORMAL && status != G_IO_STATUS_AGAIN) {
			if (error != NULL)
				g_error_free(error);
			return FALSE;
		}

		protos[i] = ntohl(protos[i]);
		num_protos++;
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

	cl->quassel.state = QUASSEL_STATE_NEGOTIATED;

	g_io_channel_flush(cl->connection, NULL);

	return TRUE;
}
