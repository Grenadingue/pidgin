/* purple
 *
 * STUN implementation inspired by jstun [http://jstun.javawi.de/]
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#ifndef _WIN32
#include <net/if.h>
#include <sys/ioctl.h>
#endif

#include <gio/gio.h>

/* Solaris */
#if defined (__SVR4) && defined (__sun)
#include <sys/sockio.h>
#endif

#include "debug.h"
#include "account.h"
#include "network.h"
#include "proxy.h"
#include "stun.h"
#include "prefs.h"

#define MSGTYPE_BINDINGREQUEST 0x0001
#define MSGTYPE_BINDINGRESPONSE 0x0101

#define ATTRIB_MAPPEDADDRESS 0x0001

#ifndef _SIZEOF_ADDR_IFREQ
#  define _SIZEOF_ADDR_IFREQ(a) sizeof(a)
#endif

struct stun_header {
	guint16 type;
	guint16 len;
	guint32 transid[4];
};

struct stun_attrib {
	guint16 type;
	guint16 len;
};

#if 0
struct stun_change {
	struct stun_header hdr;
	struct stun_attrib attrib;
	char value[4];
};
#endif	/* 0 */

struct stun_conn {
	int fd;
	struct sockaddr_in addr;
	int test;
	int retry;
	guint incb;
	guint timeout;
	struct stun_header *packet;
	size_t packetsize;
};

typedef struct {
	gint port;
	GList *addresses;
} StunHBNListenData;

static PurpleStunNatDiscovery nattype = {
	PURPLE_STUN_STATUS_UNDISCOVERED,
	PURPLE_STUN_NAT_TYPE_PUBLIC_IP,
	"\0", NULL, 0};

static GSList *callbacks = NULL;

static void close_stun_conn(struct stun_conn *sc) {

	if (sc->incb)
		purple_input_remove(sc->incb);

	if (sc->timeout)
		purple_timeout_remove(sc->timeout);

	if (sc->fd)
		close(sc->fd);

	g_free(sc);
}

static void do_callbacks(void) {
	while (callbacks) {
		PurpleStunCallback cb = callbacks->data;
		if (cb)
			cb(&nattype);
		callbacks = g_slist_delete_link(callbacks, callbacks);
	}
}

static gboolean timeoutfunc(gpointer data) {
	struct stun_conn *sc = data;
	if(sc->retry >= 2) {
		purple_debug_warning("stun", "request timed out, giving up.\n");
		if(sc->test == 2)
			nattype.type = PURPLE_STUN_NAT_TYPE_SYMMETRIC;

		/* set unknown */
		nattype.status = PURPLE_STUN_STATUS_UNKNOWN;

		nattype.lookup_time = time(NULL);

		/* callbacks */
		do_callbacks();

		/* we don't need to remove the timeout (returning FALSE) */
		sc->timeout = 0;
		close_stun_conn(sc);

		return FALSE;
	}
	purple_debug_info("stun", "request timed out, retrying.\n");
	sc->retry++;
	if (sendto(sc->fd, sc->packet, sc->packetsize, 0,
		(struct sockaddr *)&(sc->addr), sizeof(struct sockaddr_in)) !=
		(gssize)sc->packetsize)
	{
		purple_debug_warning("stun", "sendto failed\n");
		return FALSE;
	}
	return TRUE;
}

#if 0
static void do_test2(struct stun_conn *sc) {
	struct stun_change data;
	data.hdr.type = htons(0x0001);
	data.hdr.len = 0;
	data.hdr.transid[0] = rand();
	data.hdr.transid[1] = ntohl(((int)'g' << 24) + ((int)'a' << 16) + ((int)'i' << 8) + (int)'m');
	data.hdr.transid[2] = rand();
	data.hdr.transid[3] = rand();
	data.attrib.type = htons(0x003);
	data.attrib.len = htons(4);
	data.value[3] = 6;
	sc->packet = (struct stun_header*)&data;
	sc->packetsize = sizeof(struct stun_change);
	sc->retry = 0;
	sc->test = 2;
	sendto(sc->fd, sc->packet, sc->packetsize, 0, (struct sockaddr *)&(sc->addr), sizeof(struct sockaddr_in));
	sc->timeout = purple_timeout_add(500, (GSourceFunc) timeoutfunc, sc);
}
#endif	/* 0 */

static void reply_cb(gpointer data, gint source, PurpleInputCondition cond) {
	struct stun_conn *sc = data;
	guchar buffer[65536];
	struct ifreq buffer_ifr[1000];
	guchar *it, *it_end;
	gssize len;
	struct in_addr in;
	struct stun_attrib attrib;
	struct stun_header hdr;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_in *sinptr;

	memset(&in, 0, sizeof(in));

	len = recv(source, buffer, sizeof(buffer) - 1, 0);
	if (len <= 0) {
		purple_debug_warning("stun", "unable to read stun response\n");
		return;
	}
	buffer[len] = '\0';

	if ((gsize)len < sizeof(struct stun_header)) {
		purple_debug_warning("stun", "got invalid response\n");
		return;
	}

	memcpy(&hdr, buffer, sizeof(hdr));
	if ((gsize)len != (ntohs(hdr.len) + sizeof(struct stun_header))) {
		purple_debug_warning("stun", "got incomplete response\n");
		return;
	}

	/* wrong transaction */
	if(hdr.transid[0] != sc->packet->transid[0]
			|| hdr.transid[1] != sc->packet->transid[1]
			|| hdr.transid[2] != sc->packet->transid[2]
			|| hdr.transid[3] != sc->packet->transid[3]) {
		purple_debug_warning("stun", "got wrong transid\n");
		return;
	}

	if(sc->test==1) {
		if (hdr.type != MSGTYPE_BINDINGRESPONSE) {
			purple_debug_warning("stun",
				"Expected Binding Response, got %d\n",
				hdr.type);
			return;
		}

		it = buffer + sizeof(struct stun_header);
		while((buffer + len) > (it + sizeof(struct stun_attrib))) {
			memcpy(&attrib, it, sizeof(attrib));
			it += sizeof(struct stun_attrib);

			if (!((buffer + len) > (it + ntohs(attrib.len))))
				break;

			if(attrib.type == htons(ATTRIB_MAPPEDADDRESS)
					&& ntohs(attrib.len) == 8) {
				char *ip;
				/* Skip the first unused byte,
				 * the family(1 byte), and the port(2 bytes);
				 * then read the 4 byte IPv4 address */
				memcpy(&in.s_addr, it + 4, 4);
				ip = inet_ntoa(in);
				if(ip)
					g_strlcpy(nattype.publicip, ip, sizeof(nattype.publicip));
			}

			it += ntohs(attrib.len);
		}
		purple_debug_info("stun", "got public ip %s\n", nattype.publicip);
		nattype.status = PURPLE_STUN_STATUS_DISCOVERED;
		nattype.type = PURPLE_STUN_NAT_TYPE_UNKNOWN_NAT;
		nattype.lookup_time = time(NULL);

		/* is it a NAT? */

		ifc.ifc_len = sizeof(buffer_ifr);
		ifc.ifc_req = buffer_ifr;
		ioctl(source, SIOCGIFCONF, &ifc);

		it = buffer;
		it_end = it + ifc.ifc_len;
		while (it < it_end) {
			ifr = (struct ifreq*)(gpointer)it;
			it += _SIZEOF_ADDR_IFREQ(*ifr);

			if(ifr->ifr_addr.sa_family == AF_INET) {
				/* we only care about ipv4 interfaces */
				sinptr = (struct sockaddr_in *)(gpointer)&ifr->ifr_addr;
				if(sinptr->sin_addr.s_addr == in.s_addr) {
					/* no NAT */
					purple_debug_info("stun", "no nat\n");
					nattype.type = PURPLE_STUN_NAT_TYPE_PUBLIC_IP;
				}
			}
		}

#if 1
		close_stun_conn(sc);
		do_callbacks();
#else
		purple_timeout_remove(sc->timeout);
		sc->timeout = 0;

		do_test2(sc);
	} else if(sc->test == 2) {
		close_stun_conn(sc);
		nattype.type = PURPLE_STUN_NAT_TYPE_FULL_CONE;
		do_callbacks();
#endif	/* 1 */
	}
}


static void
hbn_listen_cb(int fd, gpointer data) {
	StunHBNListenData *ld = (StunHBNListenData *)data;
	GInetAddress *address = NULL;
	GSocketAddress *socket_address = NULL;
	struct stun_conn *sc;
	static struct stun_header hdr_data;

	if(fd < 0) {
		nattype.status = PURPLE_STUN_STATUS_UNKNOWN;
		nattype.lookup_time = time(NULL);
		do_callbacks();
		return;
	}

	sc = g_new0(struct stun_conn, 1);
	sc->fd = fd;

	sc->addr.sin_family = AF_INET;
	sc->addr.sin_port = htons(purple_network_get_port_from_fd(fd));
	sc->addr.sin_addr.s_addr = INADDR_ANY;

	sc->incb = purple_input_add(fd, PURPLE_INPUT_READ, reply_cb, sc);

	address = g_object_ref(G_INET_ADDRESS(ld->addresses->data));
	socket_address = g_inet_socket_address_new(address, ld->port);

	g_socket_address_to_native(socket_address, &(sc->addr), g_socket_address_get_native_size(socket_address), NULL);

	g_object_unref(G_OBJECT(address));
	g_object_unref(G_OBJECT(socket_address));
	g_resolver_free_addresses(ld->addresses);
	g_free(ld);

	hdr_data.type = htons(MSGTYPE_BINDINGREQUEST);
	hdr_data.len = 0;
	hdr_data.transid[0] = rand();
	hdr_data.transid[1] = ntohl(((int)'g' << 24) + ((int)'a' << 16) + ((int)'i' << 8) + (int)'m');
	hdr_data.transid[2] = rand();
	hdr_data.transid[3] = rand();

	if(sendto(sc->fd, &hdr_data, sizeof(struct stun_header), 0,
			(struct sockaddr *)&(sc->addr),
			sizeof(struct sockaddr_in)) < (gssize)sizeof(struct stun_header)) {
		nattype.status = PURPLE_STUN_STATUS_UNKNOWN;
		nattype.lookup_time = time(NULL);
		do_callbacks();
		close_stun_conn(sc);
		return;
	}
	sc->test = 1;
	sc->packet = &hdr_data;
	sc->packetsize = sizeof(struct stun_header);
	sc->timeout = purple_timeout_add(500, (GSourceFunc) timeoutfunc, sc);
}

static void
hbn_cb(GObject *sender, GAsyncResult *res, gpointer data) {
	StunHBNListenData *ld = NULL;
	GError *error = NULL;

	ld = g_new0(StunHBNListenData, 1);

	ld->addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(sender),
			res, &error);
	if(error != NULL) {
		nattype.status = PURPLE_STUN_STATUS_UNDISCOVERED;
		nattype.lookup_time = time(NULL);

		do_callbacks();

		return;
	}

	ld->port = GPOINTER_TO_INT(data);
	if (!purple_network_listen_range(12108, 12208, AF_UNSPEC, SOCK_DGRAM, TRUE, hbn_listen_cb, ld)) {
		nattype.status = PURPLE_STUN_STATUS_UNKNOWN;
		nattype.lookup_time = time(NULL);

		do_callbacks();

		return;
	}
}

static void
do_test1(GObject *sender, GAsyncResult *res, gpointer data) {
	GList *services = NULL;
	GError *error = NULL;
	GResolver *resolver;
	const char *servername = data;
	int port = 3478;

	services = g_resolver_lookup_service_finish(G_RESOLVER(sender),
			res, &error);
	if(error != NULL) {
		purple_debug_info("stun", "Failed to look up srv record : %s\n", error->message);

		g_error_free(error);
	} else {
		servername = g_srv_target_get_hostname((GSrvTarget *)services->data);
		port = g_srv_target_get_port((GSrvTarget *)services->data);
	}

	purple_debug_info("stun", "connecting to %s:%d\n", servername, port);

	resolver = g_resolver_get_default();
	g_resolver_lookup_by_name_async(resolver,
	                                servername,
	                                NULL,
	                                hbn_cb,
	                                GINT_TO_POINTER(port));
	g_object_unref(resolver);

	g_resolver_free_targets(services);
}

static gboolean call_callback(gpointer data) {
	PurpleStunCallback cb = data;
	cb(&nattype);
	return FALSE;
}

PurpleStunNatDiscovery *purple_stun_discover(PurpleStunCallback cb) {
	const char *servername = purple_prefs_get_string("/purple/network/stun_server");
	GResolver *resolver;

	purple_debug_info("stun", "using server %s\n", servername);

	if(nattype.status == PURPLE_STUN_STATUS_DISCOVERING) {
		if(cb)
			callbacks = g_slist_append(callbacks, cb);
		return &nattype;
	}

	if(nattype.status != PURPLE_STUN_STATUS_UNDISCOVERED) {
		gboolean use_cached_result = TRUE;

		/* Deal with the server name having changed since we did the
		   lookup */
		if (servername && strlen(servername) > 1
				&& !purple_strequal(servername, nattype.servername)) {
			use_cached_result = FALSE;
		}

		/* If we don't have a successful status and it has been 5
		   minutes since we last did a lookup, redo the lookup */
		if (nattype.status != PURPLE_STUN_STATUS_DISCOVERED
				&& (time(NULL) - nattype.lookup_time) > 300) {
			use_cached_result = FALSE;
		}

		if (use_cached_result) {
			if(cb)
				purple_timeout_add(10, call_callback, cb);
			return &nattype;
		}
	}

	if(!servername || (strlen(servername) < 2)) {
		nattype.status = PURPLE_STUN_STATUS_UNKNOWN;
		nattype.lookup_time = time(NULL);
		if(cb)
			purple_timeout_add(10, call_callback, cb);
		return &nattype;
	}

	nattype.status = PURPLE_STUN_STATUS_DISCOVERING;
	nattype.publicip[0] = '\0';
	g_free(nattype.servername);
	nattype.servername = g_strdup(servername);

	callbacks = g_slist_append(callbacks, cb);

	resolver = g_resolver_get_default();
	g_resolver_lookup_service_async(resolver,
	                                "stun",
	                                "udp",
	                                servername,
	                                NULL,
	                                do_test1,
	                                (gpointer)servername);
	g_object_unref(resolver);

	return &nattype;
}

void purple_stun_init() {
	purple_prefs_add_string("/purple/network/stun_server", "");
}
