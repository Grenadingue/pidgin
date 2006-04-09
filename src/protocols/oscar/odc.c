/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* From the oscar PRPL */
#include "oscar.h"
#include "peer.h"

/* From Gaim */
#include "conversation.h"
#include "imgstore.h"
#include "util.h"

/**
 * Free any ODC related data and print a message to the conversation
 * window based on conn->disconnect_reason.
 */
void
peer_odc_close(PeerConnection *conn)
{
	const gchar *tmp;

	if (conn->disconnect_reason == PEER_DISCONNECT_REMOTE_CLOSED)
	{
		tmp = _("Remote user closed the connection.");
	}
	else if (conn->disconnect_reason == PEER_DISCONNECT_REMOTE_REFUSED)
	{
		tmp = _("Remote user declined your request.");
	}
	else if (conn->disconnect_reason == PEER_DISCONNECT_LOST_CONNECTION)
	{
		tmp = _("Lost connection with remote user for an unknown reason.");
	}
	else if (conn->disconnect_reason == PEER_DISCONNECT_INVALID_DATA)
	{
		tmp = _("Received invalid data on connection.");
	}
	else if (conn->disconnect_reason == PEER_DISCONNECT_COULD_NOT_CONNECT)
	{
		tmp = _("Could not establish connection with remote user.");
	}
	else
		/*
		 * We shouldn't print a message for some disconnect_reasons.
		 * Like PEER_DISCONNECT_LOCAL_CLOSED.
		 */
		tmp = NULL;

	if (tmp != NULL)
	{
		GaimAccount *account;
		GaimConversation *conv;

		account = gaim_connection_get_account(conn->od->gc);
		conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, conn->sn);
		gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));
	}

	if (conn->frame != NULL)
	{
		OdcFrame *frame;
		frame = conn->frame;
		g_free(frame->payload.data);
		g_free(frame);
	}
}

/**
 * Write the given OdcFrame to a ByteStream and send it out
 * on the established PeerConnection.
 */
static void
peer_odc_send(PeerConnection *conn, OdcFrame *frame)
{
	GaimAccount *account;
	const char *username;
	size_t length;
	ByteStream bs;

	gaim_debug_info("oscar", "Outgoing ODC frame to %s with "
		"type=0x%04x, flags=0x%04x, payload length=%u\n",
		conn->sn, frame->type, frame->flags, frame->payload.len);

	account = gaim_connection_get_account(conn->od->gc);
	username = gaim_account_get_username(account);
	memcpy(frame->sn, username, strlen(username));
	memcpy(frame->cookie, conn->cookie, 8);

	length = 76;
	byte_stream_init(&bs, malloc(length + frame->payload.len),
			length + frame->payload.len);
	byte_stream_putraw(&bs, conn->magic, 4);
	byte_stream_put16(&bs, length);
	byte_stream_put16(&bs, frame->type);
	byte_stream_put16(&bs, frame->subtype);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_putraw(&bs, frame->cookie, 8);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put32(&bs, frame->payload.len);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, frame->encoding);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, frame->flags);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_putraw(&bs, frame->sn, 32);
	byte_stream_putraw(&bs, frame->payload.data, frame->payload.len);

	peer_connection_send(conn, &bs);

	free(bs.data);
}

/**
 * Send a very basic ODC frame (which contains the cookie) so that the
 * remote user can verify that we are the person they were expecting.
 * If we made an outgoing connection to then remote user, then we send
 * this immediately.  If the remote user connected to us, then we wait
 * for the other person to send this to us, then we send one to them.
 */
void
peer_odc_send_cookie(PeerConnection *conn)
{
	OdcFrame frame;

	memset(&frame, 0, sizeof(OdcFrame));
	frame.type = 0x0001;
	frame.subtype = 0x0006;
	frame.flags = 0x0060; /* Maybe this means "we're sending the cookie"? */

	peer_odc_send(conn, &frame);
}

/**
 * Send client-to-client typing notification over an established direct connection.
 */
void
peer_odc_send_typing(PeerConnection *conn, GaimTypingState typing)
{
	OdcFrame frame;

	memset(&frame, 0, sizeof(OdcFrame));
	frame.type = 0x0001;
	frame.subtype = 0x0006;
	if (typing == GAIM_TYPING)
		frame.flags = 0x0002 | 0x0008;
	else if (typing == GAIM_TYPED)
		frame.flags = 0x0002 | 0x0004;
	else
		frame.flags = 0x0002;

	peer_odc_send(conn, &frame);
}

/**
 * Send client-to-client IM over an established direct connection.
 * To send a direct IM, call this just like you would aim_send_im.
 *
 * @param conn The already-connected ODC connection.
 * @param msg Null-terminated string to send.
 * @param len The length of the message to send, including binary data.
 * @param encoding See the AIM_CHARSET_* defines in oscar.h
 * @param autoreply TRUE if this is any auto-reply.
 */
void
peer_odc_send_im(PeerConnection *conn, const char *msg, int len, int encoding, gboolean autoreply)
{
	OdcFrame frame;

	g_return_if_fail(msg != NULL);
	g_return_if_fail(len > 0);

	memset(&frame, 0, sizeof(OdcFrame));
	frame.type = 0x0001;
	frame.subtype = 0x0006;
	frame.payload.len = len;
	frame.encoding = encoding;
	frame.flags = autoreply;
	byte_stream_init(&frame.payload, malloc(len), len);
	byte_stream_putraw(&frame.payload, (guint8 *)msg, len);

	peer_odc_send(conn, &frame);

	g_free(frame.payload.data);
}

struct embedded_data
{
	size_t size;
	const guint8 *data;
};

/**
 * This is called after a direct IM has been received in its entirety.  This
 * function is passed a long chunk of data which contains the IM with any
 * data chunks (images) appended to it.
 *
 * This function rips out all the data chunks and creates an imgstore for
 * each one.  In order to do this, it first goes through the IM and takes
 * out all the IMG tags.  When doing so, it rewrites the original IMG tag
 * with one compatible with the imgstore Gaim core code. For each one, we
 * then read in chunks of data from the end of the message and actually
 * create the img store using the given data.
 *
 * For somewhat easy reference, here's a sample message
 * (with added whitespace):
 *
 * <HTML><BODY BGCOLOR="#ffffff">
 *     <FONT LANG="0">
 *     This is a really stupid picture:<BR>
 *     <IMG SRC="Sample.jpg" ID="1" WIDTH="283" HEIGHT="212" DATASIZE="9894"><BR>
 *     Yeah it is<BR>
 *     Here is another one:<BR>
 *     <IMG SRC="Soap Bubbles.bmp" ID="2" WIDTH="256" HEIGHT="256" DATASIZE="65978">
 *     </FONT>
 * </BODY></HTML>
 * <BINARY>
 *     <DATA ID="1" SIZE="9894">datadatadatadata</DATA>
 *     <DATA ID="2" SIZE="65978">datadatadatadata</DATA>
 * </BINARY>
 */
static void
peer_odc_handle_payload(PeerConnection *conn, const char *msg, size_t len, int encoding, gboolean autoreply)
{
	GaimConnection *gc;
	GaimAccount *account;
	const char *msgend, *binary_start, *dataend;
	const char *tmp, *start, *end, *idstr, *src, *sizestr;
	GData *attributes;
	GHashTable *embedded_datas;
	struct embedded_data *embedded_data;
	GSList *images;
	gchar *utf8;
	GString *newmsg;
	GaimMessageFlags imflags;

	gc = conn->od->gc;
	account = gaim_connection_get_account(gc);

	dataend = msg + len;

	/*
	 * Create a hash table containing references to each embedded
	 * data chunk.  The key is the "ID" and the value is an
	 * embedded_data struct.
	 */
	embedded_datas = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, g_free);

	/*
	 * Create an index of any binary chunks.  If we run into any
	 * problems while parsing the binary data section then we stop
	 * parsing it, and the local user will see broken image icons.
	 */
	/* TODO: Use a length argument when looking for the <binary> tag! */
	binary_start = gaim_strcasestr(msg, "<binary>");
	if (binary_start == NULL)
		msgend = dataend;
	else
	{
		msgend = binary_start;

		/* Move our pointer to immediately after the <binary> tag */
		tmp = binary_start + 8;

		/* The embedded binary markup has a mimimum length of 29 bytes */
		/* TODO: Use a length argument when looking for the <data> tag! */
		while ((tmp + 29 <= dataend) &&
				gaim_markup_find_tag("data", tmp, &start, &tmp, &attributes))
		{
			unsigned int id;
			size_t size;

			/* Move the binary pointer from ">" to the start of the data */
			tmp++;

			/* Get the ID */
			idstr = g_datalist_get_data(&attributes, "id");
			if (idstr == NULL)
			{
				g_datalist_clear(&attributes);
				break;
			}
			id = atoi(idstr);

			/* Get the size */
			sizestr = g_datalist_get_data(&attributes, "size");
			if (sizestr == NULL)
			{
				g_datalist_clear(&attributes);
				break;
			}
			size = atol(sizestr);

			g_datalist_clear(&attributes);

			if ((size > 0) && (tmp + size > dataend))
				break;

			embedded_data = g_new(struct embedded_data, 1);
			embedded_data->size = size;
			embedded_data->data = (const guint8 *)tmp;
			tmp += size;

			/* Skip past the closing </data> tag */
			if (strncasecmp(tmp, "</data>", 7))
			{
				g_free(embedded_data);
				break;
			}
			tmp += 7;

			g_hash_table_insert(embedded_datas,
					GINT_TO_POINTER(id), embedded_data);
		}
	}

	/*
	 * Loop through the message, replacing OSCAR img tags with the
	 * equivalent Gaim img tag.
	 */
	images = NULL;
	newmsg = g_string_new("");
	tmp = msg;
	while (gaim_markup_find_tag("img", tmp, &start, &end, &attributes))
	{
		int imgid = 0;

		idstr   = g_datalist_get_data(&attributes, "id");
		src     = g_datalist_get_data(&attributes, "src");
		sizestr = g_datalist_get_data(&attributes, "datasize");

		if ((idstr != NULL) && (src != NULL) && (sizestr!= NULL))
		{
			unsigned int id;
			size_t size;

			id = atoi(idstr);
			size = atol(sizestr);
			embedded_data = g_hash_table_lookup(embedded_datas,
					GINT_TO_POINTER(id));

			if ((embedded_data != NULL) && (embedded_data->size == size))
			{
				imgid = gaim_imgstore_add(embedded_data->data, size, src);

				/* Record the image number */
				images = g_slist_append(images, GINT_TO_POINTER(imgid));
			}
		}

		/* Delete the attribute list */
		g_datalist_clear(&attributes);

		/* Append the message up to the tag */
		utf8 = gaim_plugin_oscar_decode_im_part(account, conn->sn,
				encoding, 0x0000, tmp, start - tmp);
		if (utf8 != NULL) {
			g_string_append(newmsg, utf8);
			g_free(utf8);
		}

		if (imgid != 0)
		{
			/* Write the new image tag */
			g_string_append_printf(newmsg, "<IMG ID=\"%d\">", imgid);
		}

		/* Continue from the end of the tag */
		tmp = end + 1;
	}

	/* Append any remaining message data */
	if (tmp <= msgend)
	{
		utf8 = gaim_plugin_oscar_decode_im_part(account, conn->sn,
				encoding, 0x0000, tmp, msgend - tmp);
		if (utf8 != NULL) {
			g_string_append(newmsg, utf8);
			g_free(utf8);
		}
	}

	/* Send the message */
	imflags = 0;
	if (images != NULL)
		imflags |= GAIM_MESSAGE_IMAGES;
	if (autoreply)
		imflags |= GAIM_MESSAGE_AUTO_RESP;
	serv_got_im(gc, conn->sn, newmsg->str, imflags, time(NULL));
	g_string_free(newmsg, TRUE);

	/* unref any images we allocated */
	if (images)
	{
		GSList *l;
		for (l = images; l != NULL; l = l->next)
			gaim_imgstore_unref(GPOINTER_TO_INT(l->data));
		g_slist_free(images);
	}

	/* Delete our list of pointers to embedded images */
	g_hash_table_destroy(embedded_datas);
}

/**
 * This is a gaim_input_add() watcher callback function for reading
 * direct IM payload data.  "Payload data" is always an IM and
 * maybe some embedded images or files or something.  The actual
 * ODC frame is read using peer_connection_recv_cb().  We temporarily
 * switch to this watcher callback ONLY to read the payload, and we
 * switch back once we're done.
 */
static void
peer_odc_recv_cb(gpointer data, gint source, GaimInputCondition cond)
{
	PeerConnection *conn;
	OdcFrame *frame;
	ByteStream *bs;
	ssize_t read;

	conn = data;
	frame = conn->frame;
	bs = &frame->payload;

	/* Read data into the temporary buffer until it is complete */
	read = recv(conn->fd,
				&bs->data[bs->offset],
				bs->len - bs->offset,
				0);

	/* Check if the remote user closed the connection */
	if (read == 0)
	{
		peer_connection_destroy(conn, PEER_DISCONNECT_REMOTE_CLOSED);
		return;
	}

	if (read == -1)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			/* No worries */
			return;

		peer_connection_destroy(conn, PEER_DISCONNECT_LOST_CONNECTION);
		return;
	}

	bs->offset += read;
	if (bs->offset < bs->len)
		/* Waiting for more data to arrive */
		return;

	/* We have a complete ODC/OFT frame!  Handle it and continue reading */
	byte_stream_rewind(bs);
	peer_odc_handle_payload(conn, (const char *)bs->data,
			bs->len, frame->encoding, frame->flags & 0x0001);
	g_free(bs->data);
	bs->data = NULL;
	g_free(frame);
	conn->frame = NULL;

	gaim_input_remove(conn->watcher_incoming);
	conn->watcher_incoming = gaim_input_add(conn->fd,
			GAIM_INPUT_READ, peer_connection_recv_cb, conn);
}

/**
 * Handle an incoming OdcFrame.  If there is a payload associated
 * with this frame, then we remove the old watcher and add the
 * ODC watcher to read in the payload.
 */
void
peer_odc_recv_frame(PeerConnection *conn, ByteStream *bs)
{
	GaimConnection *gc;
	OdcFrame *frame;

	gc = conn->od->gc;

	frame = g_new0(OdcFrame, 1);
	frame->type = byte_stream_get16(bs);
	frame->subtype = byte_stream_get16(bs);
	byte_stream_advance(bs, 2);
	byte_stream_getrawbuf(bs, frame->cookie, 8);
	byte_stream_advance(bs, 8);
	frame->payload.len = byte_stream_get32(bs);
	frame->encoding = byte_stream_get16(bs);
	byte_stream_advance(bs, 4);
	frame->flags = byte_stream_get16(bs);
	byte_stream_advance(bs, 4);
	byte_stream_getrawbuf(bs, frame->sn, 32);

	gaim_debug_info("oscar", "Incoming ODC frame from %s with "
			"type=0x%04x, flags=0x%04x, payload length=%u\n",
			frame->sn, frame->type, frame->flags, frame->payload.len);

	if (!conn->ready)
	{
		/*
		 * We need to verify the cookie so that we know we are
		 * connected to our friend and not a malicious middle man.
		 */

		GaimAccount *account;
		GaimConversation *conv;

		if (conn->flags & PEER_CONNECTION_FLAG_IS_INCOMING)
		{
			if (memcmp(conn->cookie, frame->cookie, 8))
			{
				/*
				 * Oh no!  The user that connected to us did not send
				 * the correct cookie!  They are not our friend.  Go try
				 * to accept another connection?
				 */
				gaim_debug_info("oscar", "Received an incorrect cookie.  "
					"Closing connection.\n");
				peer_connection_destroy(conn, PEER_DISCONNECT_INVALID_DATA);
				g_free(frame);
				return;
			}

			/*
			 * Ok, we know they are legit.  Now be courteous and
			 * send them our cookie.  Note: This doesn't seem
			 * to be necessary, but it also doesn't seem to hurt.
			 */
			peer_odc_send_cookie(conn);
		}

		conn->ready = TRUE;

		/*
		 * If they connected to us then close the listener socket
		 * and send them our cookie.
		 */
		if (conn->listenerfd != -1)
		{
			close(conn->listenerfd);
			conn->listenerfd = -1;
		}

		/* Tell the local user that we are connected */
		account = gaim_connection_get_account(gc);
		conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, conn->sn);
		gaim_conversation_write(conv, NULL, _("Direct IM established"),
				GAIM_MESSAGE_SYSTEM, time(NULL));
	}

	if ((frame->type != 0x0001) && (frame->subtype != 0x0006))
	{
		gaim_debug_info("oscar", "Unknown ODC frame type 0x%04hx, "
				"subtype 0x%04hx.\n", frame->type, frame->subtype);
		return;
	}

	if (frame->flags & 0x0008)
	{
		/* I had to leave this. It's just too funny. It reminds me of my sister. */
		gaim_debug_info("oscar", "ohmigod! %s has started typing "
			"(DirectIM). He's going to send you a message! "
			"*squeal*\n", conn->sn);
		serv_got_typing(gc, conn->sn, 0, GAIM_TYPING);
	}
	else if (frame->flags & 0x0004)
	{
		serv_got_typing(gc, conn->sn, 0, GAIM_TYPED);
	}
	else
	{
		serv_got_typing_stopped(gc, conn->sn);
	}

	if (frame->payload.len > 0)
	{
		/* We have payload data!  Switch to the ODC watcher to read it. */
		frame->payload.data = g_new(guint8, frame->payload.len);
		frame->payload.offset = 0;
		conn->frame = frame;
		gaim_input_remove(conn->watcher_incoming);
		conn->watcher_incoming = gaim_input_add(conn->fd,
				GAIM_INPUT_READ, peer_odc_recv_cb, conn);
		return;
	}

	g_free(frame);
}