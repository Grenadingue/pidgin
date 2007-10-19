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
 *
 */

/* libaim is the AIM protocol plugin. It is linked against liboscarcommon,
 * which contains all the shared implementation code with libicq
 */

#include "oscarcommon.h"

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_IM_IMAGE,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	/* The mimimum icon size below is not needed in AIM 6.0 */
	{"gif,jpeg,bmp,ico", 48, 48, 50, 50, 7168,
		PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY},	/* icon_spec */
	oscar_list_icon_aim,		/* list_icon */
	oscar_list_emblem,		/* list_emblems */
	oscar_status_text,		/* status_text */
	oscar_tooltip_text,		/* tooltip_text */
	oscar_status_types,		/* status_types */
	oscar_blist_node_menu,	/* blist_node_menu */
	oscar_chat_info,		/* chat_info */
	oscar_chat_info_defaults, /* chat_info_defaults */
	oscar_login,			/* login */
	oscar_close,			/* close */
	oscar_send_im,			/* send_im */
	oscar_set_info,			/* set_info */
	oscar_send_typing,		/* send_typing */
	oscar_get_info,			/* get_info */
	oscar_set_status,		/* set_status */
	oscar_set_idle,			/* set_idle */
	oscar_change_passwd,	/* change_passwd */
	oscar_add_buddy,		/* add_buddy */
	NULL,					/* add_buddies */
	oscar_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	oscar_add_permit,		/* add_permit */
	oscar_add_deny,			/* add_deny */
	oscar_rem_permit,		/* rem_permit */
	oscar_rem_deny,			/* rem_deny */
	oscar_set_permit_deny,	/* set_permit_deny */
	oscar_join_chat,		/* join_chat */
	NULL,					/* reject_chat */
	oscar_get_chat_name,	/* get_chat_name */
	oscar_chat_invite,		/* chat_invite */
	oscar_chat_leave,		/* chat_leave */
	NULL,					/* chat_whisper */
	oscar_send_chat,		/* chat_send */
	oscar_keepalive,		/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	oscar_alias_buddy,		/* alias_buddy */
	oscar_move_buddy,		/* group_buddy */
	oscar_rename_group,		/* rename_group */
	NULL,					/* buddy_free */
	oscar_convo_closed,		/* convo_closed */
	oscar_normalize,		/* normalize */
	oscar_set_icon,			/* set_buddy_icon */
	oscar_remove_group,		/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	oscar_can_receive_file,	/* can_receive_file */
	oscar_send_file,		/* send_file */
	oscar_new_xfer,			/* new_xfer */
	oscar_offline_message,	/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,					/* send_raw */
	NULL,					/* roomlist_room_serialize */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-aim",                                       /**< id             */
	"AIM",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("AIM Protocol Plugin"),
	                                                  /**  description    */
	N_("AIM Protocol Plugin"),
	NULL,                                             /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	oscar_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	oscar_init(PURPLE_PLUGIN_PROTOCOL_INFO(plugin));
}

PURPLE_INIT_PLUGIN(aim, init_plugin, info);
