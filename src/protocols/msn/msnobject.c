/**
 * @file msnobject.c MSNObject API
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "msnobject.h"

#define GET_STRING_TAG(field, id) \
	if ((tag = strstr(str, id "=\"")) != NULL) \
	{ \
		tag += strlen(id "=\""); \
		c = strchr(tag, '"'); \
		if (c != NULL) \
			obj->field = g_strndup(tag, c - tag); \
	}

#define GET_INT_TAG(field, id) \
	if ((tag = strstr(str, id "=\"")) != NULL) \
	{ \
		char buf[16]; \
		tag += strlen(id "=\""); \
		c = strchr(tag, '"'); \
		if (c != NULL) \
		{ \
			strncpy(buf, tag, c - tag); \
			buf[c - tag] = '\0'; \
			obj->field = atoi(buf); \
		} \
	}

MsnObject *
msn_object_new(void)
{
	MsnObject *obj;

	obj = g_new0(MsnObject, 1);

	msn_object_set_type(obj, MSN_OBJECT_UNKNOWN);
	msn_object_set_friendly(obj, "AAA=");

	return obj;
}

MsnObject *
msn_object_new_from_string(const char *str)
{
	MsnObject *obj;
	char *tag, *c;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(!strncmp(str, "<msnobj ", 8), NULL);

	obj = msn_object_new();

	GET_STRING_TAG(creator,  "Creator");
	GET_INT_TAG(size,        "Size");
	GET_INT_TAG(type,        "Type");
	GET_STRING_TAG(location, "Location");
	GET_STRING_TAG(friendly, "Friendly");
	GET_STRING_TAG(sha1d,    "SHA1D");
	GET_STRING_TAG(sha1c,    "SHA1C");

	return obj;
}

void
msn_object_destroy(MsnObject *obj)
{
	g_return_if_fail(obj != NULL);

	if (obj->creator != NULL)
		g_free(obj->creator);

	if (obj->location != NULL)
		g_free(obj->location);

	if (obj->friendly != NULL)
		g_free(obj->friendly);

	if (obj->sha1d != NULL)
		g_free(obj->sha1d);

	if (obj->sha1c != NULL)
		g_free(obj->sha1c);

	g_free(obj);
}

char *
msn_object_to_string(const MsnObject *obj)
{
	char *str;

	g_return_val_if_fail(obj != NULL, NULL);

	str = g_strdup_printf("<msnobj Creator=\"%s\" Size=\"%d\" Type=\"%d\" "
						  "Location=\"%s\" Friendly=\"%s\" SHA1D=\"%s\" "
						  "SHA1C=\"%s\"/>",
						  msn_object_get_creator(obj),
						  msn_object_get_size(obj),
						  msn_object_get_type(obj),
						  msn_object_get_location(obj),
						  msn_object_get_friendly(obj),
						  msn_object_get_sha1d(obj),
						  msn_object_get_sha1c(obj));

	return str;
}

void
msn_object_set_creator(MsnObject *obj, const char *creator)
{
	g_return_if_fail(obj != NULL);

	if (obj->creator != NULL)
		g_free(obj->creator);

	obj->creator = (creator == NULL ? NULL : g_strdup(creator));
}

void
msn_object_set_size(MsnObject *obj, int size)
{
	g_return_if_fail(obj != NULL);

	obj->size = size;
}

void
msn_object_set_type(MsnObject *obj, MsnObjectType type)
{
	g_return_if_fail(obj != NULL);

	obj->type = type;
}

void
msn_object_set_location(MsnObject *obj, const char *location)
{
	g_return_if_fail(obj != NULL);

	if (obj->location != NULL)
		g_free(obj->location);

	obj->location = (location == NULL ? NULL : g_strdup(location));
}

void
msn_object_set_friendly(MsnObject *obj, const char *friendly)
{
	g_return_if_fail(obj != NULL);

	if (obj->friendly != NULL)
		g_free(obj->friendly);

	obj->friendly = (friendly == NULL ? NULL : g_strdup(friendly));
}

void
msn_object_set_sha1d(MsnObject *obj, const char *sha1d)
{
	g_return_if_fail(obj != NULL);

	if (obj->sha1d != NULL)
		g_free(obj->sha1d);

	obj->sha1d = (sha1d == NULL ? NULL : g_strdup(sha1d));
}

void
msn_object_set_sha1c(MsnObject *obj, const char *sha1c)
{
	g_return_if_fail(obj != NULL);

	if (obj->sha1c != NULL)
		g_free(obj->sha1c);

	obj->sha1c = (sha1c == NULL ? NULL : g_strdup(sha1c));
}

const char *
msn_object_get_creator(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, NULL);

	return obj->creator;
}

int
msn_object_get_size(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, 0);

	return obj->size;
}

MsnObjectType
msn_object_get_type(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, MSN_OBJECT_UNKNOWN);

	return obj->type;
}

const char *
msn_object_get_location(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, NULL);

	return obj->location;
}

const char *
msn_object_get_friendly(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, NULL);

	return obj->friendly;
}

const char *
msn_object_get_sha1d(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, NULL);

	return obj->sha1d;
}

const char *
msn_object_get_sha1c(const MsnObject *obj)
{
	g_return_val_if_fail(obj != NULL, NULL);

	return obj->sha1c;
}
