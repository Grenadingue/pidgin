/* purple
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
 */

#ifndef _FACEBOOK_H_
#define _FACEBOOK_H_

#include <glib.h>

#include "protocol.h"

#define FACEBOOK_TYPE_PROTOCOL             (facebook_protocol_get_type())
#define FACEBOOK_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), FACEBOOK_TYPE_PROTOCOL, FacebookProtocol))
#define FACEBOOK_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), FACEBOOK_TYPE_PROTOCOL, FacebookProtocolClass))
#define FACEBOOK_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), FACEBOOK_TYPE_PROTOCOL))
#define FACEBOOK_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), FACEBOOK_TYPE_PROTOCOL))
#define FACEBOOK_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), FACEBOOK_TYPE_PROTOCOL, FacebookProtocolClass))

typedef struct _FacebookProtocol FacebookProtocol;
typedef struct _FacebookProtocolClass FacebookProtocolClass;

struct _FacebookProtocol
{
	PurpleProtocol parent;
};

struct _FacebookProtocolClass
{
	PurpleProtocolClass parent_class;
};

/**
 * Returns the GType for the FacebookProtocol object.
 */
G_MODULE_EXPORT GType facebook_protocol_get_type(void);

#endif /* _FACEBOOK_H_ */