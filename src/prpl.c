/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */

#include "prpl.h"

extern void toc_init(struct prpl *);
extern void oscar_init(struct prpl *);

GSList *protocols = NULL;

struct prpl *find_prpl(int prot)
{
	GSList *e = protocols;
	struct prpl *r;

	while (e) {
		r = (struct prpl *)e->data;
		if (r->protocol == prot)
			return r;
		e = e->next;
	}

	return NULL;
}

void load_protocol(proto_init pi)
{
	struct prpl *p = g_new0(struct prpl, 1);
	pi(p);
	if (find_prpl(p->protocol))
		g_free(p);
	else
		protocols = g_slist_append(protocols, p);
}

void static_proto_init()
{
	load_protocol(toc_init);
#ifndef DYNAMIC_OSCAR
	load_protocol(oscar_init);
#endif
}
