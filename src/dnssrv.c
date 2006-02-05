/**
 * @file dnssrv.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "internal.h"

#ifndef _WIN32
#include <resolv.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#ifndef T_SRV
#define T_SRV	33
#endif
#else
#include <windns.h>
/* Missing from the mingw headers */
#ifndef DNS_TYPE_SRV
# define DNS_TYPE_SRV 33
#endif
#endif

#include "dnssrv.h"
#include "eventloop.h"
#include "debug.h"

#ifndef _WIN32
typedef union {
	HEADER hdr;
	u_char buf[1024];
} queryans;
#else
static DNS_STATUS WINAPI (*MyDnsQuery_UTF8) (
	PCSTR lpstrName, WORD wType, DWORD fOptions,
	PIP4_ARRAY aipServers, PDNS_RECORD* ppQueryResultsSet,
	PVOID* pReserved) = NULL;
static void WINAPI (*MyDnsRecordListFree) (PDNS_RECORD pRecordList,
	DNS_FREE_TYPE FreeType) = NULL;
#endif

struct resdata {
	GaimSRVCallback cb;
	gpointer extradata;
#ifndef _WIN32
	guint handle;
#else
	char *query;
	char *errmsg;
	GSList *results;
#endif
};

static gint responsecompare(gconstpointer ar, gconstpointer br) {
	GaimSrvResponse *a = (GaimSrvResponse*)ar;
	GaimSrvResponse *b = (GaimSrvResponse*)br;

	if(a->pref == b->pref) {
		if(a->weight == b->weight)
			return 0;
		if(a->weight < b->weight)
			return -1;
		return 1;
	}
	if(a->pref < b->pref)
		return -1;
	return 1;
}
#ifndef _WIN32
static void resolve(int in, int out) {
	GList *ret = NULL;
	GaimSrvResponse *srvres;
	queryans answer;
	int size;
	int qdcount;
	int ancount;
	guchar *end;
	guchar *cp;
	gchar name[256];
	guint16 type, dlen, pref, weight, port;
	gchar query[256];

	if(read(in, query, 256) <= 0) {
		_exit(0);
	}
	size = res_query( query, C_IN, T_SRV, (u_char*)&answer, sizeof( answer));

	qdcount = ntohs(answer.hdr.qdcount);
	ancount = ntohs(answer.hdr.ancount);


	cp = (guchar*)&answer + sizeof(HEADER);
	end = (guchar*)&answer + size;

	/* skip over unwanted stuff */
	while (qdcount-- > 0 && cp < end) {
		size = dn_expand( (unsigned char*)&answer, end, cp, name, 256);
		if(size < 0) goto end;
		cp += size + QFIXEDSZ;
	}

	while (ancount-- > 0 && cp < end) {
		size = dn_expand((unsigned char*)&answer, end, cp, name, 256);
		if(size < 0)
			goto end;

		cp += size;

		type = GUINT16_FROM_BE(*cp);
		cp += 2;

		/* skip ttl and class since we already know it */
		cp += 6;

		dlen = GUINT16_FROM_BE(*cp);
		cp += 2;

		if (type == T_SRV) {
			pref = GUINT16_FROM_BE(*cp);
			cp += 2;

			weight = GUINT16_FROM_BE(*cp);
			cp += 2;

			port = GUINT16_FROM_BE(*cp);
			cp += 2;

			size = dn_expand( (unsigned char*)&answer, end, cp, name, 256);
			if(size < 0 )
				goto end;

			cp += size;

			srvres = g_new0(GaimSrvResponse, 1);
			strcpy(srvres->hostname, name);
			srvres->pref = pref;
			srvres->port = port;
			srvres->weight = weight;

			ret = g_list_insert_sorted(ret, srvres, responsecompare);
		} else {
			cp += dlen;
		}
	}
end:	size = g_list_length(ret);
	write(out, &size, sizeof(int));
	while(g_list_first(ret)) {
		write(out, g_list_first(ret)->data, sizeof(GaimSrvResponse));
		g_free(g_list_first(ret)->data);
		ret = g_list_remove(ret, g_list_first(ret)->data);
	}

	/* Should the resolver be reused?
	 * There is most likely only 1 SRV queries per prpl...
	 */
	_exit(0);
}

static void resolved(gpointer data, gint source, GaimInputCondition cond) {
	int size;
	struct resdata *rdata = (struct resdata*)data;
	GaimSrvResponse *res;
	GaimSrvResponse *tmp;
	int i;
	GaimSRVCallback cb = rdata->cb;

	read(source, &size, sizeof(int));
	gaim_debug_info("srv","found %d SRV entries\n", size);
	tmp = res = g_new0(GaimSrvResponse, size);
	i = size;
	while(i) {
		read(source, tmp++, sizeof(GaimSrvResponse));
		i--;
	}
	cb(res, size, rdata->extradata);
	gaim_input_remove(rdata->handle);
	g_free(rdata);
}

#else /* _WIN32 */

/** The Jabber Server code was inspiration for parts of this. */

static gboolean res_main_thread_cb(gpointer data) {
	GaimSrvResponse *srvres = NULL;
	int size = 0;
	struct resdata *rdata = data;

	if (rdata->errmsg != NULL) {
		gaim_debug_error("srv", rdata->errmsg);
		g_free(rdata->errmsg);
	} else {
		GaimSrvResponse *srvres_tmp;
		GSList *lst = rdata->results;

		size = g_slist_length(rdata->results);

		srvres_tmp = srvres = g_new0(GaimSrvResponse, size);
		while (lst) {
			memcpy(srvres_tmp++, lst->data, sizeof(GaimSrvResponse));
			g_free(lst->data);
			lst = g_slist_remove(lst, lst->data);
		}

		rdata->results = lst;
	}

	gaim_debug_info("srv", "found %d SRV entries\n", size);

	rdata->cb(srvres, size, rdata->extradata);

	g_free(rdata->query);
	g_free(rdata);

	return FALSE;
}

static gpointer res_thread(gpointer data) {
	PDNS_RECORD dr = NULL;
	int type = DNS_TYPE_SRV;
	DNS_STATUS ds;
	struct resdata *rdata = data;

	ds = MyDnsQuery_UTF8(rdata->query, type, DNS_QUERY_STANDARD, NULL, &dr, NULL);
	if (ds != ERROR_SUCCESS) {
		gchar *msg = g_win32_error_message(ds);
		rdata->errmsg = g_strdup_printf("Couldn't look up SRV record. %s (%lu).\n", msg, ds);
		g_free(msg);
	} else {
		PDNS_RECORD dr_tmp;
		GSList *lst = NULL;
		DNS_SRV_DATA *srv_data;
		GaimSrvResponse *srvres;

		for (dr_tmp = dr; dr_tmp != NULL; dr_tmp = dr_tmp->pNext) {
			/* Discard any incorrect entries. I'm not sure if this is necessary */
			if (dr_tmp->wType != type || strcmp(dr_tmp->pName, rdata->query) != 0) {
				continue;
			}

			srv_data = &dr_tmp->Data.SRV;
			srvres = g_new0(GaimSrvResponse, 1);
			strncpy(srvres->hostname, srv_data->pNameTarget, 255);
			srvres->hostname[255] = '\0';
			srvres->pref = srv_data->wPriority;
			srvres->port = srv_data->wPort;
			srvres->weight = srv_data->wWeight;

			lst = g_slist_insert_sorted(lst, srvres, responsecompare);
		}

		MyDnsRecordListFree(dr, DnsFreeRecordList);
		rdata->results = lst;
	}

	/* back to main thread */
	g_idle_add(res_main_thread_cb, rdata);

	g_thread_exit(NULL);
	return NULL;
}

#endif

void gaim_srv_resolve(const char *protocol, const char *transport, const char *domain, GaimSRVCallback cb, gpointer extradata) {
	char *query = g_strdup_printf("_%s._%s.%s",protocol, transport, domain);
	struct resdata *rdata;
#ifndef _WIN32
	int in[2], out[2];
	int pid;
	gaim_debug_info("srv","querying SRV record for %s\n", query);
	if(pipe(in) || pipe(out)) {
		gaim_debug_error("srv", "Could not create pipe\n");
		g_free(query);
		cb(NULL, 0, extradata);
		return;
	}

	pid = fork();

	if(pid == -1) {
		gaim_debug_error("srv","Could not create process!\n");
		cb(NULL, 0, extradata);
		g_free(query);
		return;
	}
	/* Child */
	if( pid == 0 ) {
		close(out[0]);
		close(in[1]);
		resolve(in[0], out[1]);
	}

	close(out[1]);
	close(in[0]);

	if(write(in[1], query, strlen(query)+1)<0) {
		gaim_debug_error("srv", "Could not write to SRV resolver\n");
	}
	rdata = g_new0(struct resdata,1);
	rdata->cb = cb;
	rdata->extradata = extradata;
	rdata->handle = gaim_input_add(out[0], GAIM_INPUT_READ, resolved, rdata);

	g_free(query);
#else
	GError* err = NULL;

	static gboolean initialized = FALSE;

	gaim_debug_info("srv","querying SRV record for %s\n", query);

	if (!initialized) {
		MyDnsQuery_UTF8 = (void*) wgaim_find_and_loadproc("dnsapi.dll", "DnsQuery_UTF8");
		MyDnsRecordListFree = (void*) wgaim_find_and_loadproc(
			"dnsapi.dll", "DnsRecordListFree");
		initialized = TRUE;
	}

	if (!MyDnsQuery_UTF8 || !MyDnsRecordListFree) {
		gaim_debug_error("srv", "System missing DNS API (Requires W2K+)\n");
		g_free(query);
		cb(NULL, 0, extradata);
		return;
	}

	rdata = g_new0(struct resdata, 1);
	rdata->cb = cb;
	rdata->query = query;
	rdata->extradata = extradata;

	if (!g_thread_create(res_thread, rdata, FALSE, &err)) {
		rdata->errmsg = g_strdup_printf("SRV thread create failure: %s\n", err ? err->message : "");
		g_error_free(err);
		res_main_thread_cb(rdata);
	}
#endif
}

