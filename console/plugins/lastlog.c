/**
 * @file lastlog.c Lastlog plugin for gaim-text.
 *
 * Copyright (C) 2006 Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>
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


#define PLUGIN_STATIC_NAME	"GntLastlog"

#include "internal.h"

#include <plugin.h>
#include <version.h>

#include <cmds.h>

#include <gnt.h>
#include <gnttextview.h>
#include <gntwindow.h>

#include <gntconv.h>
#include <gntplugin.h>

static GaimCmdId cmd;

static gboolean
window_kpress_cb(GntWidget *wid, const char *key, GntTextView *view)
{
	if (key[0] == 27)
	{
		if (strcmp(key+1, GNT_KEY_DOWN) == 0)
			gnt_text_view_scroll(view, 1);
		else if (strcmp(key+1, GNT_KEY_UP) == 0)
			gnt_text_view_scroll(view, -1);
		else if (strcmp(key+1, GNT_KEY_PGDOWN) == 0)
			gnt_text_view_scroll(view, wid->priv.height - 2);
		else if (strcmp(key+1, GNT_KEY_PGUP) == 0)
			gnt_text_view_scroll(view, -(wid->priv.height - 2));
		else
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

static GaimCmdRet
lastlog_cb(GaimConversation *conv, const char *cmd, char **args, char **error, gpointer null)
{
	GGConv *ggconv = conv->ui_data;
	char **strings = g_strsplit(GNT_TEXT_VIEW(ggconv->tv)->string->str, "\n", 0);
	GntWidget *win, *tv;
	int i, j;

	win = gnt_window_new();
	gnt_box_set_title(GNT_BOX(win), _("Lastlog"));

	tv = gnt_text_view_new();
	gnt_box_add_widget(GNT_BOX(win), tv);

	gnt_widget_show(win);

	for (i = 0; strings[i]; i++) {
		if (strstr(strings[i], args[0]) != NULL) {
			char **finds = g_strsplit(strings[i], args[0], 0);
			for (j = 0; finds[j]; j++) {
				if (j)
					gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(tv), args[0], GNT_TEXT_FLAG_BOLD);
				gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(tv), finds[j], GNT_TEXT_FLAG_NORMAL);
			}
			g_strfreev(finds);
			gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(tv), "\n", GNT_TEXT_FLAG_NORMAL);
		}
	}

	g_signal_connect(G_OBJECT(win), "key_pressed", G_CALLBACK(window_kpress_cb), tv);
	g_strfreev(strings);
	return GAIM_CMD_STATUS_OK;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	cmd = gaim_cmd_register("lastlog", "s", GAIM_CMD_P_DEFAULT,
			GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_IM, NULL,
			lastlog_cb, _("lastlog: Searches for a substring in the backlog."), NULL);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_cmd_unregister(cmd);
	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GNT_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	"gntlastlog",
	N_("GntLastlog"),
	VERSION,
	N_("Lastlog plugin for gaim-text."),
	N_("Lastlog plugin for gaim-text."),
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	"http://gaim.sourceforge.net",
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
