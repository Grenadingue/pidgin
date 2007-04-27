/**
 * @file gtkconv.c GTK+ Conversation API
 * @ingroup pidgin
 *
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 *
 */
#include "internal.h"
#include "pidgin.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#endif

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
# ifdef _WIN32
#  include "wspell.h"
# endif
#endif

#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "cmds.h"
#include "debug.h"
#include "idle.h"
#include "imgstore.h"
#include "log.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconvwin.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtklog.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "pidginstock.h"

#include "gtknickcolors.h"

#define AUTO_RESPONSE "&lt;AUTO-REPLY&gt; : "

typedef  enum
{
	PIDGIN_CONV_SET_TITLE 			= 1 << 0,
	PIDGIN_CONV_BUDDY_ICON			= 1 << 1,
	PIDGIN_CONV_MENU			= 1 << 2,
	PIDGIN_CONV_TAB_ICON			= 1 << 3,
	PIDGIN_CONV_TOPIC			= 1 << 4,
	PIDGIN_CONV_SMILEY_THEME		= 1 << 5,
	PIDGIN_CONV_COLORIZE_TITLE		= 1 << 6
}PidginConvFields;

#define	PIDGIN_CONV_ALL	((1 << 7) - 1)

#define SEND_COLOR "#204a87"
#define RECV_COLOR "#cc0000"
#define HIGHLIGHT_COLOR "#AF7F00"

/* Undef this to turn off "custom-smiley" debug messages */
#define DEBUG_CUSTOM_SMILEY

#define LUMINANCE(c) (float)((0.3*(c.red))+(0.59*(c.green))+(0.11*(c.blue)))

#if 0
/* These colors come from the default GNOME palette */
static GdkColor nick_colors[] = {
	{0, 47616, 46336, 43776},       /* Basic 3D Medium */
	{0, 32768, 32000, 29696},       /* Basic 3D Dark */
	{0, 22016, 20992, 18432},       /* 3D Shadow */
	{0, 33536, 42496, 32512},       /* Green Medium */
	{0, 23808, 29952, 21760},       /* Green Dark */
	{0, 17408, 22016, 12800},       /* Green Shadow */
	{0, 57344, 46592, 44800},       /* Red Hilight */
	{0, 49408, 26112, 23040},       /* Red Medium */
	{0, 34816, 17920, 12544},       /* Red Dark */
	{0, 49408, 14336,  8704},       /* Red Shadow */
	{0, 34816, 32512, 41728},       /* Purple Medium */
	{0, 25088, 23296, 33024},       /* Purple Dark */
	{0, 18688, 16384, 26112},       /* Purple Shadow */
	{0, 40192, 47104, 53760},       /* Blue Hilight */
	{0, 29952, 36864, 44544},       /* Blue Medium */
	{0, 57344, 49920, 40448},       /* Face Skin Medium */
	{0, 45824, 37120, 26880},       /* Face skin Dark */
	{0, 33280, 26112, 18176},       /* Face Skin Shadow */
	{0, 57088, 16896,  7680},       /* Accent Red */
	{0, 39168,     0,     0},       /* Accent Red Dark */
	{0, 17920, 40960, 17920},       /* Accent Green */
	{0,  9728, 50944,  9728}        /* Accent Green Dark */
};

#define NUM_NICK_COLORS (sizeof(nick_colors) / sizeof(*nick_colors))
#endif

/* From http://www.w3.org/TR/AERT#color-contrast */
#define MIN_BRIGHTNESS_CONTRAST 75
#define MIN_COLOR_CONTRAST 200

#define NUM_NICK_COLORS 220
static GdkColor *nick_colors = NULL;
static guint nbr_nick_colors;

typedef struct {
	GtkWidget *window;

	GtkWidget *entry;
	GtkWidget *message;

	PurpleConversation *conv;

} InviteBuddyInfo;

static GtkWidget *invite_dialog = NULL;
static GtkWidget *warn_close_dialog = NULL;

static PidginWindow *hidden_convwin = NULL;
static GList *window_list = NULL;

/* Lists of status icons at all available sizes for use as window icons */
static GList *available_list = NULL;
static GList *away_list = NULL;
static GList *busy_list = NULL;
static GList *xa_list = NULL;
static GList *login_list = NULL;
static GList *logout_list = NULL;
static GList *offline_list = NULL;
static GHashTable *prpl_lists = NULL;

static gboolean update_send_to_selection(PidginWindow *win);
static void generate_send_to_items(PidginWindow *win);

/* Prototypes. <-- because Paco-Paco hates this comment. */
static void got_typing_keypress(PidginConversation *gtkconv, gboolean first);
static void gray_stuff_out(PidginConversation *gtkconv);
static GList *generate_invite_user_names(PurpleConnection *gc);
static void add_chat_buddy_common(PurpleConversation *conv, PurpleConvChatBuddy *cb, const char *old_name);
static gboolean tab_complete(PurpleConversation *conv);
static void pidgin_conv_updated(PurpleConversation *conv, PurpleConvUpdateType type);
static void gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state);
static void update_typing_icon(PidginConversation *gtkconv);
static const char *item_factory_translate_func (const char *path, gpointer func_data);
gboolean pidgin_conv_has_focus(PurpleConversation *conv);
static void pidgin_conv_custom_smiley_allocated(GdkPixbufLoader *loader, gpointer user_data);
static void pidgin_conv_custom_smiley_closed(GdkPixbufLoader *loader, gpointer user_data);
static GdkColor* generate_nick_colors(guint *numcolors, GdkColor background);
static gboolean color_is_visible(GdkColor foreground, GdkColor background, int color_contrast, int brightness_contrast);
static void pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields);
static void focus_out_from_menubar(GtkWidget *wid, PidginWindow *win);

static GdkColor *get_nick_color(PidginConversation *gtkconv, const char *name) {
	static GdkColor col;
	GtkStyle *style = gtk_widget_get_style(gtkconv->imhtml);
	float scale;

	col = nick_colors[g_str_hash(name) % nbr_nick_colors];
	scale = ((1-(LUMINANCE(style->base[GTK_STATE_NORMAL]) / LUMINANCE(style->white))) *
		       (LUMINANCE(style->white)/MAX(MAX(col.red, col.blue), col.green)));

	/* The colors are chosen to look fine on white; we should never have to darken */
	if (scale > 1) {
		col.red   *= scale;
		col.green *= scale;
		col.blue  *= scale;
	}

	return &col;
}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gint
close_conv_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	GList *list = g_list_copy(gtkconv->convs);

	g_list_foreach(list, (GFunc)purple_conversation_destroy, NULL);
	g_list_free(list);

	return TRUE;
}

static gboolean
lbox_size_allocate_cb(GtkWidget *w, GtkAllocation *allocation, gpointer data)
{
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", allocation->width == 1 ? 0 : allocation->width);

	return FALSE;
}

static gboolean
size_allocate_cb(GtkWidget *w, GtkAllocation *allocation, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;

	if (!GTK_WIDGET_VISIBLE(w))
		return FALSE;

	if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		return FALSE;

	if (gtkconv->auto_resize) {
		return FALSE;
	}

	/* I find that I resize the window when it has a bunch of conversations in it, mostly so that the
	 * tab bar will fit, but then I don't want new windows taking up the entire screen.  I check to see
	 * if there is only one conversation in the window.  This way we'll be setting new windows to the
	 * size of the last resized new window. */
	/* I think that the above justification is not the majority, and that the new tab resizing should
	 * negate it anyway.  --luke */
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
	{
		if (w == gtkconv->imhtml) {
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/default_width", allocation->width);
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/default_height", allocation->height);
		}
		if (w == gtkconv->lower_hbox)
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height", allocation->height);
	}
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
	{
		if (w == gtkconv->imhtml) {
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_width", allocation->width);
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_height", allocation->height);
		}
		if (w == gtkconv->lower_hbox)
			purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height", allocation->height);
	}

	return FALSE;
}

static void
default_formatize(PidginConversation *c)
{
	PurpleConversation *conv = c->active_conv;

	if (conv->features & PURPLE_CONNECTION_HTML)
	{
		char color[8];
		GdkColor fg_color, bg_color;

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold") != GTK_IMHTML(c->entry)->edit.bold)
			gtk_imhtml_toggle_bold(GTK_IMHTML(c->entry));

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic") != GTK_IMHTML(c->entry)->edit.italic)
			gtk_imhtml_toggle_italic(GTK_IMHTML(c->entry));

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline") != GTK_IMHTML(c->entry)->edit.underline)
			gtk_imhtml_toggle_underline(GTK_IMHTML(c->entry));

		gtk_imhtml_toggle_fontface(GTK_IMHTML(c->entry),
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/font_face"));

		if (!(conv->features & PURPLE_CONNECTION_NO_FONTSIZE))
		{
			int size = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/font_size");

			/* 3 is the default. */
			if (size != 3)
				gtk_imhtml_font_set_size(GTK_IMHTML(c->entry), size);
		}

		if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"), "") != 0)
		{
			gdk_color_parse(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"),
							&fg_color);
			g_snprintf(color, sizeof(color), "#%02x%02x%02x",
									fg_color.red   / 256,
									fg_color.green / 256,
									fg_color.blue  / 256);
		} else
			strcpy(color, "");

		gtk_imhtml_toggle_forecolor(GTK_IMHTML(c->entry), color);

		if(!(conv->features & PURPLE_CONNECTION_NO_BGCOLOR) &&
		   strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"), "") != 0)
		{
			gdk_color_parse(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"),
							&bg_color);
			g_snprintf(color, sizeof(color), "#%02x%02x%02x",
									bg_color.red   / 256,
									bg_color.green / 256,
									bg_color.blue  / 256);
		} else
			strcpy(color, "");

		gtk_imhtml_toggle_background(GTK_IMHTML(c->entry), color);

		if (conv->features & PURPLE_CONNECTION_FORMATTING_WBFO)
			gtk_imhtml_set_whole_buffer_formatting_only(GTK_IMHTML(c->entry), TRUE);
		else
			gtk_imhtml_set_whole_buffer_formatting_only(GTK_IMHTML(c->entry), FALSE);
	}
}

static void
clear_formatting_cb(GtkIMHtml *imhtml, PidginConversation *gtkconv)
{
	default_formatize(gtkconv);
}

static const char *
pidgin_get_cmd_prefix(void)
{
	return "/";
}

static PurpleCmdRet
say_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		purple_conv_im_send(PURPLE_CONV_IM(conv), args[0]);
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		purple_conv_chat_send(PURPLE_CONV_CHAT(conv), args[0]);

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
me_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	char *tmp;

	tmp = g_strdup_printf("/me %s", args[0]);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		purple_conv_im_send(PURPLE_CONV_IM(conv), tmp);
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		purple_conv_chat_send(PURPLE_CONV_CHAT(conv), tmp);

	g_free(tmp);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
debug_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;
	PurpleCmdStatus status;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("me is using " PIDGIN_NAME " v%s.", VERSION);
		markup = g_markup_escape_text(tmp, -1);

		status = purple_cmd_do_command(conv, tmp, markup, error);

		g_free(tmp);
		g_free(markup);
		return status;
	} else {
		purple_conversation_write(conv, NULL, _("Supported debug options are:  version"),
		                        PURPLE_MESSAGE_NO_LOG|PURPLE_MESSAGE_ERROR, time(NULL));
		return PURPLE_CMD_STATUS_OK;
	}
}

static PurpleCmdRet
clear_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	PidginConversation *gtkconv = NULL;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_imhtml_clear(GTK_IMHTML(gtkconv->imhtml));
	return PURPLE_CMD_STATUS_OK;
}

static PurpleCmdRet
help_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l, *text;
	GString *s;

	if (args[0] != NULL) {
		s = g_string_new("");
		text = purple_cmd_help(conv, args[0]);

		if (text) {
			for (l = text; l; l = l->next)
				if (l->next)
					g_string_append_printf(s, "%s\n", (char *)l->data);
				else
					g_string_append_printf(s, "%s", (char *)l->data);
		} else {
			g_string_append(s, _("No such command (in this context)."));
		}
	} else {
		s = g_string_new(_("Use \"/help &lt;command&gt;\" for help on a specific command.\n"
											 "The following commands are available in this context:\n"));

		text = purple_cmd_list(conv);
		for (l = text; l; l = l->next)
			if (l->next)
				g_string_append_printf(s, "%s, ", (char *)l->data);
			else
				g_string_append_printf(s, "%s.", (char *)l->data);
		g_list_free(text);
	}

	purple_conversation_write(conv, NULL, s->str, PURPLE_MESSAGE_NO_LOG, time(NULL));
	g_string_free(s, TRUE);

	return PURPLE_CMD_STATUS_OK;
}

static void
send_history_add(PidginConversation *gtkconv, const char *message)
{
	GList *first;

	first = g_list_first(gtkconv->send_history);
	g_free(first->data);
	first->data = g_strdup(message);
	gtkconv->send_history = g_list_prepend(first, NULL);
}

static void
reset_default_size(PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		gtk_widget_set_size_request(gtkconv->lower_hbox, -1,
					    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height"));
	else
		gtk_widget_set_size_request(gtkconv->lower_hbox, -1,
					    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height"));
}

static gboolean
check_for_and_do_command(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	char *cmd;
	const char *prefix;
	GtkTextIter start;

	gtkconv = PIDGIN_CONVERSATION(conv);
	prefix = pidgin_get_cmd_prefix();

	cmd = gtk_imhtml_get_text(GTK_IMHTML(gtkconv->entry), NULL, NULL);
	gtk_text_buffer_get_start_iter(GTK_IMHTML(gtkconv->entry)->text_buffer, &start);

	if (cmd && (strncmp(cmd, prefix, strlen(prefix)) == 0)
	   && !gtk_text_iter_get_child_anchor(&start)) {
		PurpleCmdStatus status;
		char *error, *cmdline, *markup, *send_history;
		GtkTextIter end;

		send_history = gtk_imhtml_get_markup(GTK_IMHTML(gtkconv->entry));
		send_history_add(gtkconv, send_history);
		g_free(send_history);

		cmdline = cmd + strlen(prefix);

		gtk_text_iter_forward_chars(&start, g_utf8_strlen(prefix, -1));
		gtk_text_buffer_get_end_iter(GTK_IMHTML(gtkconv->entry)->text_buffer, &end);
		markup = gtk_imhtml_get_markup_range(GTK_IMHTML(gtkconv->entry), &start, &end);
		status = purple_cmd_do_command(conv, cmdline, markup, &error);
		g_free(cmd);
		g_free(markup);

		switch (status) {
			case PURPLE_CMD_STATUS_OK:
				return TRUE;
			case PURPLE_CMD_STATUS_NOT_FOUND:
				return FALSE;
			case PURPLE_CMD_STATUS_WRONG_ARGS:
				purple_conversation_write(conv, "", _("Syntax Error:  You typed the wrong number of arguments "
								    "to that command."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				return TRUE;
			case PURPLE_CMD_STATUS_FAILED:
				purple_conversation_write(conv, "", error ? error : _("Your command failed for an unknown reason."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				g_free(error);
				return TRUE;
			case PURPLE_CMD_STATUS_WRONG_TYPE:
				if(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
					purple_conversation_write(conv, "", _("That command only works in chats, not IMs."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				else
					purple_conversation_write(conv, "", _("That command only works in IMs, not chats."),
							PURPLE_MESSAGE_NO_LOG, time(NULL));
				return TRUE;
			case PURPLE_CMD_STATUS_WRONG_PRPL:
				purple_conversation_write(conv, "", _("That command doesn't work on this protocol."),
						PURPLE_MESSAGE_NO_LOG, time(NULL));
				return TRUE;
		}
	}

	g_free(cmd);
	return FALSE;
}

static void
send_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleMessageFlags flags = 0;
	char *buf, *clean;

	account = purple_conversation_get_account(conv);

	if (!purple_account_is_connected(account))
		return;

	if ((purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) &&
		purple_conv_chat_has_left(PURPLE_CONV_CHAT(conv)))
		return;

	if (check_for_and_do_command(conv)) {
		if (gtkconv->entry_growing) {
			reset_default_size(gtkconv);
			gtkconv->entry_growing = FALSE;
		}
		gtk_imhtml_clear(GTK_IMHTML(gtkconv->entry));
		return;
	}

	buf = gtk_imhtml_get_markup(GTK_IMHTML(gtkconv->entry));
	clean = gtk_imhtml_get_text(GTK_IMHTML(gtkconv->entry), NULL, NULL);

	gtk_widget_grab_focus(gtkconv->entry);

	if (strlen(clean) == 0) {
		g_free(buf);
		g_free(clean);
		return;
	}

	purple_idle_touch();

	/* XXX: is there a better way to tell if the message has images? */
	if (GTK_IMHTML(gtkconv->entry)->im_images != NULL)
		flags |= PURPLE_MESSAGE_IMAGES;

	gc = purple_account_get_connection(account);
	if (gc && (conv->features & PURPLE_CONNECTION_NO_NEWLINES)) {
		char **bufs;
		int i;

		bufs = gtk_imhtml_get_markup_lines(GTK_IMHTML(gtkconv->entry));
		for (i = 0; bufs[i]; i++) {
			send_history_add(gtkconv, bufs[i]);
			if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
				purple_conv_im_send_with_flags(PURPLE_CONV_IM(conv), bufs[i], flags);
			else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
				purple_conv_chat_send_with_flags(PURPLE_CONV_CHAT(conv), bufs[i], flags);
		}

		g_strfreev(bufs);

	} else {
		send_history_add(gtkconv, buf);
		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
			purple_conv_im_send_with_flags(PURPLE_CONV_IM(conv), buf, flags);
		else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
			purple_conv_chat_send_with_flags(PURPLE_CONV_CHAT(conv), buf, flags);
	}

	g_free(clean);
	g_free(buf);

	gtk_imhtml_clear(GTK_IMHTML(gtkconv->entry));
	if (gtkconv->entry_growing) {
		reset_default_size(gtkconv);
		gtkconv->entry_growing = FALSE;
	}
	gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
}

static void
add_remove_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleAccount *account;
	const char *name;
	PurpleConversation *conv = gtkconv->active_conv;

	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		PurpleBuddy *b;

		b = purple_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_remove_buddy(b);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_buddy(account, (char *)name, NULL, NULL);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_remove_chat(c);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_chat(account, NULL, NULL, name);
	}

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static void chat_do_info(PidginConversation *gtkconv, const char *who)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;

	if ((gc = purple_conversation_get_gc(conv))) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

		/*
		 * If there are special needs for getting info on users in
		 * buddy chat "rooms"...
		 */
		if (prpl_info->get_cb_info != NULL)
		{
			prpl_info->get_cb_info(gc,
				purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), who);
		}
		else
			prpl_info->get_info(gc, who);
	}
}


static void
info_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		serv_get_info(purple_conversation_get_gc(conv),
					  purple_conversation_get_name(conv));

		gtk_widget_grab_focus(gtkconv->entry);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		/* Get info of the person currently selected in the GtkTreeView */
		PidginChatPane *gtkchat;
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreeSelection *sel;
		char *name;

		gtkchat = gtkconv->u.chat;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

		if (gtk_tree_selection_get_selected(sel, NULL, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &name, -1);
		else
			return;

		chat_do_info(gtkconv, name);
		g_free(name);
	}
}

static void
block_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_block(account, purple_conversation_get_name(conv));

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static void
unblock_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_permit(account, purple_conversation_get_name(conv));

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static void
do_invite(GtkWidget *w, int resp, InviteBuddyInfo *info)
{
	const char *buddy, *message;
	PidginConversation *gtkconv;

	gtkconv = PIDGIN_CONVERSATION(info->conv);

	if (resp == GTK_RESPONSE_OK) {
		buddy   = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(info->entry)->entry));
		message = gtk_entry_get_text(GTK_ENTRY(info->message));

		if (!g_ascii_strcasecmp(buddy, ""))
			return;

		serv_chat_invite(purple_conversation_get_gc(info->conv),
						 purple_conv_chat_get_id(PURPLE_CONV_CHAT(info->conv)),
						 message, buddy);
	}

	gtk_widget_destroy(invite_dialog);
	invite_dialog = NULL;

	g_free(info);
}

static void
invite_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
				GtkSelectionData *sd, guint inf, guint t, gpointer data)
{
	InviteBuddyInfo *info = (InviteBuddyInfo *)data;
	const char *convprotocol;

	convprotocol = purple_account_get_protocol_id(purple_conversation_get_account(info->conv));

	if (sd->target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE))
	{
		PurpleBlistNode *node = NULL;
		PurpleBuddy *buddy;

		memcpy(&node, sd->data, sizeof(node));

		if (PURPLE_BLIST_NODE_IS_CONTACT(node))
			buddy = purple_contact_get_priority_buddy((PurpleContact *)node);
		else if (PURPLE_BLIST_NODE_IS_BUDDY(node))
			buddy = (PurpleBuddy *)node;
		else
			return;

		if (strcmp(convprotocol, purple_account_get_protocol_id(buddy->account)))
		{
			purple_notify_error(PIDGIN_CONVERSATION(info->conv), NULL,
							  _("That buddy is not on the same protocol as this "
								"chat."), NULL);
		}
		else
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(info->entry)->entry), buddy->name);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else if (sd->target == gdk_atom_intern("application/x-im-contact", FALSE))
	{
		char *protocol = NULL;
		char *username = NULL;
		PurpleAccount *account;

		if (pidgin_parse_x_im_contact((const char *)sd->data, FALSE, &account,
										&protocol, &username, NULL))
		{
			if (account == NULL)
			{
				purple_notify_error(PIDGIN_CONVERSATION(info->conv), NULL,
					_("You are not currently signed on with an account that "
					  "can invite that buddy."), NULL);
			}
			else if (strcmp(convprotocol, purple_account_get_protocol_id(account)))
			{
				purple_notify_error(PIDGIN_CONVERSATION(info->conv), NULL,
								  _("That buddy is not on the same protocol as this "
									"chat."), NULL);
			}
			else
			{
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(info->entry)->entry), username);
			}
		}

		g_free(username);
		g_free(protocol);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
}

static const GtkTargetEntry dnd_targets[] =
{
	{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, 0},
	{"application/x-im-contact", 0, 1}
};

static void
invite_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	InviteBuddyInfo *info = NULL;

	if (invite_dialog == NULL) {
		PurpleConnection *gc;
		PidginWindow *gtkwin;
		GtkWidget *label;
		GtkWidget *vbox, *hbox;
		GtkWidget *table;
		GtkWidget *img;

		img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
		                               gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));

		info = g_new0(InviteBuddyInfo, 1);
		info->conv = conv;

		gc        = purple_conversation_get_gc(conv);
		gtkwin    = pidgin_conv_get_window(gtkconv);

		/* Create the new dialog. */
		invite_dialog = gtk_dialog_new_with_buttons(
			_("Invite Buddy Into Chat Room"),
			GTK_WINDOW(gtkwin->window), 0,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			PIDGIN_STOCK_INVITE, GTK_RESPONSE_OK, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(invite_dialog),
		                                GTK_RESPONSE_OK);
		gtk_container_set_border_width(GTK_CONTAINER(invite_dialog), PIDGIN_HIG_BOX_SPACE);
		gtk_window_set_resizable(GTK_WINDOW(invite_dialog), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(invite_dialog), FALSE);

		info->window = GTK_WIDGET(invite_dialog);

		/* Setup the outside spacing. */
		vbox = GTK_DIALOG(invite_dialog)->vbox;

		gtk_box_set_spacing(GTK_BOX(vbox), PIDGIN_HIG_BORDER);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), PIDGIN_HIG_BOX_SPACE);

		/* Setup the inner hbox and put the dialog's icon in it. */
		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		/* Setup the right vbox. */
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		/* Put our happy label in it. */
		label = gtk_label_new(_("Please enter the name of the user you wish "
								"to invite, along with an optional invite "
								"message."));
		gtk_widget_set_size_request(label, 350, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		/* hbox for the table, and to give it some spacing on the left. */
		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		/* Setup the table we're going to use to lay stuff out. */
		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);
		gtk_table_set_col_spacings(GTK_TABLE(table), PIDGIN_HIG_BOX_SPACE);
		gtk_container_set_border_width(GTK_CONTAINER(table), PIDGIN_HIG_BORDER);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		/* Now the Buddy label */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Buddy:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		/* Now the Buddy drop-down entry field. */
		info->entry = gtk_combo_new();
		gtk_combo_set_case_sensitive(GTK_COMBO(info->entry), FALSE);
		gtk_entry_set_activates_default(
				GTK_ENTRY(GTK_COMBO(info->entry)->entry), TRUE);

		gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->entry);

		/* Fill in the names. */
		gtk_combo_set_popdown_strings(GTK_COMBO(info->entry),
									  generate_invite_user_names(gc));


		/* Now the label for "Message" */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Message:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


		/* And finally, the Message entry field. */
		info->message = gtk_entry_new();
		gtk_entry_set_activates_default(GTK_ENTRY(info->message), TRUE);

		gtk_table_attach_defaults(GTK_TABLE(table), info->message, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->message);

		/* Connect the signals. */
		g_signal_connect(G_OBJECT(invite_dialog), "response",
						 G_CALLBACK(do_invite), info);
		/* Setup drag-and-drop */
		gtk_drag_dest_set(info->window,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  dnd_targets,
						  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);
		gtk_drag_dest_set(info->entry,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  dnd_targets,
						  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);

		g_signal_connect(G_OBJECT(info->window), "drag_data_received",
						 G_CALLBACK(invite_dnd_recv), info);
		g_signal_connect(G_OBJECT(info->entry), "drag_data_received",
						 G_CALLBACK(invite_dnd_recv), info);

	}

	gtk_widget_show_all(invite_dialog);

	if (info != NULL)
		gtk_widget_grab_focus(GTK_COMBO(info->entry)->entry);
}

static void
menu_new_conv_cb(gpointer data, guint action, GtkWidget *widget)
{
	pidgin_dialogs_im();
}

static void
savelog_writefile_cb(void *user_data, const char *filename)
{
	PurpleConversation *conv = (PurpleConversation *)user_data;
	FILE *fp;
	const char *name;
	gchar *text;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(PIDGIN_CONVERSATION(conv), NULL, _("Unable to open file."), NULL);
		return;
	}

	name = purple_conversation_get_name(conv);
	fprintf(fp, "<html>\n<head><title>%s</title></head>\n<body>", name);
	fprintf(fp, _("<h1>Conversation with %s</h1>\n"), name);

	text = gtk_imhtml_get_markup(
		GTK_IMHTML(PIDGIN_CONVERSATION(conv)->imhtml));
	fprintf(fp, "%s", text);
	g_free(text);

	fprintf(fp, "\n</body>\n</html>\n");
	fclose(fp);
}

/*
 * It would be kinda cool if this gave the option of saving a
 * plaintext v. HTML file.
 */
static void
menu_save_as_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);
	gchar *buf;

	buf = g_strdup_printf("%s.html", purple_normalize(conv->account, conv->name));

	purple_request_file(PIDGIN_CONVERSATION(conv), _("Save Conversation"),
					  purple_escape_filename(buf),
					  TRUE, G_CALLBACK(savelog_writefile_cb), NULL, conv);

	g_free(buf);
}

static void
menu_view_log_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PurpleLogType type;
	PidginBuddyList *gtkblist;
	GdkCursor *cursor;
	const char *name;
	PurpleAccount *account;
	GSList *buddies;
	GSList *cur;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		type = PURPLE_LOG_IM;
	else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		type = PURPLE_LOG_CHAT;
	else
		return;

	gtkblist = pidgin_blist_get_default_gtk_blist();

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(gtkblist->window->window, cursor);
	gdk_window_set_cursor(win->window->window, cursor);
	gdk_cursor_unref(cursor);
#if GTK_CHECK_VERSION(2,4,0)
	gdk_display_flush(gdk_drawable_get_display(GDK_DRAWABLE(widget->window)));
#else
	gdk_flush();
#endif

	name = purple_conversation_get_name(conv);
	account = purple_conversation_get_account(conv);

	buddies = purple_find_buddies(account, name);
	for (cur = buddies; cur != NULL; cur = cur->next)
	{
		PurpleBlistNode *node = cur->data;
		if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
		{
			pidgin_log_show_contact((PurpleContact *)node->parent);
			g_slist_free(buddies);
			gdk_window_set_cursor(gtkblist->window->window, NULL);
			gdk_window_set_cursor(win->window->window, NULL);
			return;
		}
	}
	g_slist_free(buddies);

	pidgin_log_show(type, name, account);

	gdk_window_set_cursor(gtkblist->window->window, NULL);
	gdk_window_set_cursor(win->window->window, NULL);
}

static void
menu_clear_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = pidgin_conv_window_get_active_conversation(win);
	gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_imhtml_clear(GTK_IMHTML(gtkconv->imhtml));
}

struct _search {
	PidginWindow *gtkwin;
	GtkWidget *entry;
};

static void do_search_cb(GtkWidget *widget, gint resp, struct _search *s)
{
	PurpleConversation *conv;
	PidginConversation *gtk_active_conv;
	GList *iter;

	conv = pidgin_conv_window_get_active_conversation(s->gtkwin);
	gtk_active_conv = PIDGIN_CONVERSATION(conv);

	switch (resp)
	{
		case GTK_RESPONSE_OK:
			/* clear highlighting except the active conversation window
			 * highlight the keywords in the active conversation window */
			for (iter = pidgin_conv_window_get_gtkconvs(s->gtkwin) ; iter ; iter = iter->next)
			{
				PidginConversation *gtkconv = iter->data;

				if (gtkconv != gtk_active_conv)
				{
					gtk_imhtml_search_clear(GTK_IMHTML(gtkconv->imhtml));
				}
				else
				{
					gtk_imhtml_search_find(GTK_IMHTML(gtk_active_conv->imhtml),
					                       gtk_entry_get_text(GTK_ENTRY(s->entry)));
				}
			}
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
			/* clear the keyword highlighting in all the conversation windows */
			for (iter = pidgin_conv_window_get_gtkconvs(s->gtkwin); iter; iter=iter->next)
			{
				PidginConversation *gconv = iter->data;
				gtk_imhtml_search_clear(GTK_IMHTML(gconv->imhtml));
			}

			gtk_widget_destroy(s->gtkwin->dialogs.search);
			s->gtkwin->dialogs.search = NULL;
			g_free(s);
			break;
	}
}

static void
menu_find_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *gtkwin = data;
	GtkWidget *hbox;
	GtkWidget *img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
						  gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	GtkWidget *label;
	struct _search *s;

	if (gtkwin->dialogs.search) {
		gtk_window_present(GTK_WINDOW(gtkwin->dialogs.search));
		return;
	}

	s = g_malloc(sizeof(struct _search));
	s->gtkwin = gtkwin;

	gtkwin->dialogs.search = gtk_dialog_new_with_buttons(_("Find"),
			GTK_WINDOW(gtkwin->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			GTK_STOCK_FIND, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(gtkwin->dialogs.search),
									GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(gtkwin->dialogs.search), "response",
					 G_CALLBACK(do_search_cb), s);

	gtk_container_set_border_width(GTK_CONTAINER(gtkwin->dialogs.search), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(gtkwin->dialogs.search), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(gtkwin->dialogs.search), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(gtkwin->dialogs.search)->vbox), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(
		GTK_CONTAINER(GTK_DIALOG(gtkwin->dialogs.search)->vbox), PIDGIN_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(gtkwin->dialogs.search)->vbox),
					  hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(gtkwin->dialogs.search),
									  GTK_RESPONSE_OK, FALSE);

	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Search for:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	s->entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(s->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(s->entry));
	g_signal_connect(G_OBJECT(s->entry), "changed",
					 G_CALLBACK(pidgin_set_sensitive_if_input),
					 gtkwin->dialogs.search);
	gtk_box_pack_start(GTK_BOX(hbox), s->entry, FALSE, FALSE, 0);

	gtk_widget_show_all(gtkwin->dialogs.search);
	gtk_widget_grab_focus(s->entry);
}

static void
menu_send_file_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		serv_send_file(purple_conversation_get_gc(conv), purple_conversation_get_name(conv), NULL);
	}

}

static void
menu_add_pounce_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_gtkconv(win)->active_conv;

	pidgin_pounce_editor_show(purple_conversation_get_account(conv),
								purple_conversation_get_name(conv), NULL);
}

static void
menu_alias_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PurpleAccount *account;
	const char *name;

	conv    = pidgin_conv_window_get_active_conversation(win);
	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		PurpleBuddy *b;

		b = purple_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_alias_buddy(b);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_alias_chat(c);
	}
}

static void
menu_get_info_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	info_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_invite_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	invite_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_block_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	block_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_unblock_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	unblock_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_add_remove_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	add_remove_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_close_conv_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;

	close_conv_cb(NULL, PIDGIN_CONVERSATION(pidgin_conv_window_get_active_conversation(win)));
}

static void
menu_logging_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	gboolean logging;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return;

	logging = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	if (logging == purple_conversation_is_logging(conv))
		return;

	if (logging)
	{
		/* Enable logging first so the message below can be logged. */
		purple_conversation_set_logging(conv, TRUE);

		purple_conversation_write(conv, NULL,
								_("Logging started. Future messages in this conversation will be logged."),
								conv->logs ? (PURPLE_MESSAGE_SYSTEM) :
								             (PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG),
								time(NULL));
	}
	else
	{
		purple_conversation_write(conv, NULL,
								_("Logging stopped. Future messages in this conversation will not be logged."),
								conv->logs ? (PURPLE_MESSAGE_SYSTEM) :
								             (PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG),
								time(NULL));

		/* Disable the logging second, so that the above message can be logged. */
		purple_conversation_set_logging(conv, FALSE);
	}
}

static void
menu_toolbar_cb(gpointer data, guint action, GtkWidget *widget)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
	                    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)));
}

static void
menu_sounds_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->make_sound =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
}

static void
menu_timestamps_cb(gpointer data, guint action, GtkWidget *widget)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/show_timestamps",
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)));
}

static void
chat_do_im(PidginConversation *gtkconv, const char *who)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;
	char *real_who;

	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->get_cb_real_name)
		real_who = prpl_info->get_cb_real_name(gc,
				purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), who);
	else
		real_who = g_strdup(who);

	if(!real_who)
		return;

	pidgin_dialogs_im_with_user(account, real_who);

	g_free(real_who);
}

static void pidgin_conv_chat_update_user(PurpleConversation *conv, const char *user);

static void
ignore_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConvChat *chat;
	const char *name;

	chat = PURPLE_CONV_CHAT(conv);
	name = g_object_get_data(G_OBJECT(w), "user_data");

	if (name == NULL)
		return;

	if (purple_conv_chat_is_user_ignored(chat, name))
		purple_conv_chat_unignore(chat, name);
	else
		purple_conv_chat_ignore(chat, name);

	pidgin_conv_chat_update_user(conv, name);
}

static void
menu_chat_im_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_im(gtkconv, who);
}

static void
menu_chat_send_file_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");
	PurpleConnection *gc  = purple_conversation_get_gc(conv);

	serv_send_file(gc, who, NULL);
}

static void
menu_chat_info_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	char *who;

	who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_info(gtkconv, who);
}

static void
menu_chat_get_away_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;
	char *who;

	gc  = purple_conversation_get_gc(conv);
	who = g_object_get_data(G_OBJECT(w), "user_data");

	if (gc != NULL) {
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

		/*
		 * May want to expand this to work similarly to menu_info_cb?
		 */

		if (prpl_info->get_cb_away != NULL)
		{
			prpl_info->get_cb_away(gc,
				purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), who);
		}
	}
}

static void
menu_chat_add_remove_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleBuddy *b;
	char *name;

	account = purple_conversation_get_account(conv);
	name    = g_object_get_data(G_OBJECT(w), "user_data");
	b       = purple_find_buddy(account, name);

	if (b != NULL)
		pidgin_dialogs_remove_buddy(b);
	else if (account != NULL && purple_account_is_connected(account))
		purple_blist_request_add_buddy(account, name, NULL, NULL);

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static GtkTextMark *
get_mark_for_user(PidginConversation *gtkconv, const char *who)
{
	GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
	char *tmp = g_strconcat("user:", who, NULL);
	GtkTextMark *mark = gtk_text_buffer_get_mark(buf, tmp);

	g_free(tmp);
	return mark;
}

static void
menu_last_said_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	GtkTextMark *mark;
	const char *who;

	who = g_object_get_data(G_OBJECT(w), "user_data");
	mark = get_mark_for_user(gtkconv, who);

	if (mark != NULL)
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(gtkconv->imhtml), mark, 0.1, FALSE, 0, 0);
	else
		g_return_if_reached();
}

static GtkWidget *
create_chat_menu(PurpleConversation *conv, const char *who, PurpleConnection *gc)
{
	static GtkWidget *menu = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
	gboolean is_me = FALSE;
	GtkWidget *button;
	PurpleBuddy *buddy = NULL;

	if (gc != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu)
		gtk_widget_destroy(menu);

	if (!strcmp(chat->nick, purple_normalize(conv->account, who)))
		is_me = TRUE;

	menu = gtk_menu_new();

	if (!is_me) {
		button = pidgin_new_item_from_stock(menu, _("IM"), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW,
					G_CALLBACK(menu_chat_im_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);

		g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);


		if (prpl_info && prpl_info->send_file)
		{
			button = pidgin_new_item_from_stock(menu, _("Send File"),
				PIDGIN_STOCK_FILE_TRANSFER, G_CALLBACK(menu_chat_send_file_cb),
				PIDGIN_CONVERSATION(conv), 0, 0, NULL);

			if (gc == NULL || prpl_info == NULL ||
			    !(!prpl_info->can_receive_file || prpl_info->can_receive_file(gc, who)))
			{
				gtk_widget_set_sensitive(button, FALSE);
			}

			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
		}


		if (purple_conv_chat_is_user_ignored(PURPLE_CONV_CHAT(conv), who))
			button = pidgin_new_item_from_stock(menu, _("Un-Ignore"), PIDGIN_STOCK_IGNORE,
							G_CALLBACK(ignore_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
		else
			button = pidgin_new_item_from_stock(menu, _("Ignore"), PIDGIN_STOCK_IGNORE,
							G_CALLBACK(ignore_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);

		g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (prpl_info && (prpl_info->get_info || prpl_info->get_cb_info)) {
		button = pidgin_new_item_from_stock(menu, _("Info"), PIDGIN_STOCK_TOOLBAR_USER_INFO,
						G_CALLBACK(menu_chat_info_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);

		g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (prpl_info && prpl_info->get_cb_away) {
		button = pidgin_new_item_from_stock(menu, _("Get Away Message"), PIDGIN_STOCK_AWAY,
					G_CALLBACK(menu_chat_get_away_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);

		g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (!is_me && prpl_info && !(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		if ((buddy = purple_find_buddy(conv->account, who)) != NULL)
			button = pidgin_new_item_from_stock(menu, _("Remove"), GTK_STOCK_REMOVE,
						G_CALLBACK(menu_chat_add_remove_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
		else
			button = pidgin_new_item_from_stock(menu, _("Add"), GTK_STOCK_ADD,
						G_CALLBACK(menu_chat_add_remove_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);

		g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	button = pidgin_new_item_from_stock(menu, _("Last said"), GTK_STOCK_INDEX,
						G_CALLBACK(menu_last_said_cb), PIDGIN_CONVERSATION(conv), 0, 0, NULL);
	g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	if (!get_mark_for_user(PIDGIN_CONVERSATION(conv), who))
		gtk_widget_set_sensitive(button, FALSE);

	if (buddy != NULL)
	{
		if (purple_account_is_connected(conv->account))
			pidgin_append_blist_node_proto_menu(menu, conv->account->gc,
												  (PurpleBlistNode *)buddy);
		pidgin_append_blist_node_extended_menu(menu, (PurpleBlistNode *)buddy);
		gtk_widget_show_all(menu);
	}

	return menu;
}


static gint
gtkconv_chat_popup_menu_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *menu;
	gchar *who;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = account->gc;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);
	menu = create_chat_menu (conv, who, gc);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
				   pidgin_treeview_popup_menu_position_func, widget,
				   0, GDK_CURRENT_TIME);
	g_free(who);

	return TRUE;
}


static gint
right_click_chat_cb(GtkWidget *widget, GdkEventButton *event,
					PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	gchar *who;
	int x, y;

	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = account->gc;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(gtkchat->list),
								  event->x, event->y, &path, &column, &x, &y);

	if (path == NULL)
		return FALSE;

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list))), path);

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		chat_do_im(gtkconv, who);
	} else if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
		/* Move to user's anchor */
		GtkTextMark *mark = get_mark_for_user(gtkconv, who);

		if(mark != NULL)
			gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(gtkconv->imhtml), mark, 0.1, FALSE, 0, 0);
	} else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		GtkWidget *menu = create_chat_menu (conv, who, gc);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   event->button, event->time);
	}

	g_free(who);
	gtk_tree_path_free(path);

	return TRUE;
}

static void
move_to_next_unread_tab(PidginConversation *gtkconv, gboolean forward)
{
	PidginConversation *next_gtkconv = NULL;
	PidginWindow *win;
	int initial, i, total, diff;

	win   = gtkconv->win;
	initial = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
	                                gtkconv->tab_cont);
	total = pidgin_conv_window_get_gtkconv_count(win);
	/* By adding total here, the moduli calculated later will always have two
	 * positive arguments. x % y where x < 0 is not guaranteed to return a
	 * positive number.
	 */
	diff = (forward ? 1 : -1) + total;

	for (i = (initial + diff) % total; i != initial; i = (i + diff) % total) {
		next_gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, i);
		if (next_gtkconv->unseen_state > 0)
			break;
	}

	if (i == initial) { /* no new messages */
		i = (i + diff) % total;
		next_gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, i);
	}

	if (next_gtkconv != NULL && next_gtkconv != gtkconv)
		pidgin_conv_window_switch_gtkconv(win, next_gtkconv);
}

static gboolean
entry_key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
	PidginWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	int curconv;

	gtkconv  = (PidginConversation *)data;
	conv     = gtkconv->active_conv;
	win      = gtkconv->win;
	curconv = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));

	/* If CTRL was held down... */
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
			case GDK_Up:
				if (!gtkconv->send_history)
					break;

				if (!gtkconv->send_history->prev) {
					GtkTextIter start, end;

					g_free(gtkconv->send_history->data);

					gtk_text_buffer_get_start_iter(gtkconv->entry_buffer,
												   &start);
					gtk_text_buffer_get_end_iter(gtkconv->entry_buffer, &end);

					gtkconv->send_history->data =
						gtk_imhtml_get_markup(GTK_IMHTML(gtkconv->entry));
				}

				if (gtkconv->send_history->next && gtkconv->send_history->next->data) {
					GObject *object;
					GtkTextIter iter;
					GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));

					gtkconv->send_history = gtkconv->send_history->next;

					/* Block the signal to prevent application of default formatting. */
					object = g_object_ref(G_OBJECT(gtkconv->entry));
					g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
													NULL, gtkconv);
					/* Clear the formatting. */
					gtk_imhtml_clear_formatting(GTK_IMHTML(gtkconv->entry));
					/* Unblock the signal. */
					g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
													  NULL, gtkconv);
					g_object_unref(object);

					gtk_imhtml_clear(GTK_IMHTML(gtkconv->entry));
					gtk_imhtml_append_text_with_images(
						GTK_IMHTML(gtkconv->entry), gtkconv->send_history->data,
						0, NULL);
					/* this is mainly just a hack so the formatting at the
					 * cursor gets picked up. */
					gtk_text_buffer_get_end_iter(buffer, &iter);
					gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
				}

				return TRUE;
				break;

			case GDK_Down:
				if (!gtkconv->send_history)
					break;

				if (gtkconv->send_history->prev && gtkconv->send_history->prev->data) {
					GObject *object;
					GtkTextIter iter;
					GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));

					gtkconv->send_history = gtkconv->send_history->prev;

					/* Block the signal to prevent application of default formatting. */
					object = g_object_ref(G_OBJECT(gtkconv->entry));
					g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
													NULL, gtkconv);
					/* Clear the formatting. */
					gtk_imhtml_clear_formatting(GTK_IMHTML(gtkconv->entry));
					/* Unblock the signal. */
					g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
													  NULL, gtkconv);
					g_object_unref(object);

					gtk_imhtml_clear(GTK_IMHTML(gtkconv->entry));
					gtk_imhtml_append_text_with_images(
						GTK_IMHTML(gtkconv->entry), gtkconv->send_history->data,
						0, NULL);
					/* this is mainly just a hack so the formatting at the
					 * cursor gets picked up. */
					if (*(char *)gtkconv->send_history->data) {
						gtk_text_buffer_get_end_iter(buffer, &iter);
						gtk_text_buffer_move_mark_by_name(buffer, "insert", &iter);
					} else {
						/* Restore the default formatting */
						default_formatize(gtkconv);
					}
				}

				return TRUE;
				break;

			case GDK_Page_Down:
			case ']':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv + 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv + 1);
				return TRUE;
				break;

			case GDK_Page_Up:
			case '[':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv - 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), -1);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv - 1);
				return TRUE;
				break;

			case GDK_Tab:
			case GDK_ISO_Left_Tab:
				if (event->state & GDK_SHIFT_MASK) {
					move_to_next_unread_tab(gtkconv, FALSE);
				} else {
					move_to_next_unread_tab(gtkconv, TRUE);
				}

				return TRUE;
				break;

			case GDK_comma:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
						curconv - 1);
				break;

			case GDK_period:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
#if GTK_CHECK_VERSION(2,2,0)
						(curconv + 1) % gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)));
#else
						(curconv + 1) % g_list_length(GTK_NOTEBOOK(win->notebook)->children));
#endif
				break;

		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (event->state & GDK_MOD1_MASK)
	{
		if (event->keyval > '0' && event->keyval <= '9')
		{
			guint switchto = event->keyval - '1';
			if (switchto < pidgin_conv_window_get_gtkconv_count(win))
				gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), switchto);

			return TRUE;
		}
	}

	/* If neither CTRL nor ALT were held down... */
	else
	{
		switch (event->keyval)
		{
			case GDK_Tab:
				return tab_complete(conv);
				break;

			case GDK_Page_Up:
				gtk_imhtml_page_up(GTK_IMHTML(gtkconv->imhtml));
				return TRUE;
				break;

			case GDK_Page_Down:
				gtk_imhtml_page_down(GTK_IMHTML(gtkconv->imhtml));
				return TRUE;
				break;

		}
	}
	return FALSE;
}

/*
 * NOTE:
 *   This guy just kills a single right click from being propagated any
 *   further.  I  have no idea *why* we need this, but we do ...  It
 *   prevents right clicks on the GtkTextView in a convo dialog from
 *   going all the way down to the notebook.  I suspect a bug in
 *   GtkTextView, but I'm not ready to point any fingers yet.
 */
static gboolean
entry_stop_rclick_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* Right single click */
		g_signal_stop_emission_by_name(G_OBJECT(widget), "button_press_event");

		return TRUE;
	}

	return FALSE;
}

/*
 * If someone tries to type into the conversation backlog of a
 * conversation window then we yank focus from the conversation backlog
 * and give it to the text entry box so that people can type
 * all the live long day and it will get entered into the entry box.
 */
static gboolean
refocus_entry_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PidginConversation *gtkconv = data;

	/* If we have a valid key for the conversation display, then exit */
	if ((event->state & GDK_CONTROL_MASK) ||
		(event->keyval == GDK_F10) ||
		(event->keyval == GDK_Shift_L) ||
		(event->keyval == GDK_Shift_R) ||
		(event->keyval == GDK_Control_L) ||
		(event->keyval == GDK_Control_R) ||
		(event->keyval == GDK_Escape) ||
		(event->keyval == GDK_Up) ||
		(event->keyval == GDK_Down) ||
		(event->keyval == GDK_Left) ||
		(event->keyval == GDK_Right) ||
		(event->keyval == GDK_Home) ||
		(event->keyval == GDK_End) ||
		(event->keyval == GDK_Tab) ||
		(event->keyval == GDK_ISO_Left_Tab))
			return FALSE;

	if (event->type == GDK_KEY_RELEASE)
		gtk_widget_grab_focus(gtkconv->entry);

	gtk_widget_event(gtkconv->entry, (GdkEvent *)event);

	return TRUE;
}

void
pidgin_conv_switch_active_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PurpleConversation *old_conv;
	GtkIMHtml *entry;
	const char *protocol_name;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	old_conv = gtkconv->active_conv;

	if (old_conv == conv)
		return;

	purple_conversation_close_logs(old_conv);
	gtkconv->active_conv = conv;

	purple_conversation_set_logging(conv,
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(gtkconv->win->menu.logging)));

	entry = GTK_IMHTML(gtkconv->entry);
	protocol_name = purple_account_get_protocol_name(conv->account);
	gtk_imhtml_set_protocol_name(entry, protocol_name);
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->imhtml), protocol_name);

	if (!(conv->features & PURPLE_CONNECTION_HTML))
		gtk_imhtml_clear_formatting(GTK_IMHTML(gtkconv->entry));
	else if (conv->features & PURPLE_CONNECTION_FORMATTING_WBFO &&
	         !(old_conv->features & PURPLE_CONNECTION_FORMATTING_WBFO))
	{
		/* The old conversation allowed formatting on parts of the
		 * buffer, but the new one only allows it on the whole
		 * buffer.  This code saves the formatting from the current
		 * position of the cursor, clears the formatting, then
		 * applies the saved formatting to the entire buffer. */

		gboolean bold;
		gboolean italic;
		gboolean underline;
		char *fontface   = gtk_imhtml_get_current_fontface(entry);
		char *forecolor  = gtk_imhtml_get_current_forecolor(entry);
		char *backcolor  = gtk_imhtml_get_current_backcolor(entry);
		char *background = gtk_imhtml_get_current_background(entry);
		gint fontsize    = gtk_imhtml_get_current_fontsize(entry);
		gboolean bold2;
		gboolean italic2;
		gboolean underline2;

		gtk_imhtml_get_current_format(entry, &bold, &italic, &underline);

		/* Clear existing formatting */
		gtk_imhtml_clear_formatting(entry);

		/* Apply saved formatting to the whole buffer. */

		gtk_imhtml_get_current_format(entry, &bold2, &italic2, &underline2);

		if (bold != bold2)
			gtk_imhtml_toggle_bold(entry);

		if (italic != italic2)
			gtk_imhtml_toggle_italic(entry);

		if (underline != underline2)
			gtk_imhtml_toggle_underline(entry);

		gtk_imhtml_toggle_fontface(entry, fontface);

		if (!(conv->features & PURPLE_CONNECTION_NO_FONTSIZE))
			gtk_imhtml_font_set_size(entry, fontsize);

		gtk_imhtml_toggle_forecolor(entry, forecolor);

		if (!(conv->features & PURPLE_CONNECTION_NO_BGCOLOR))
		{
			gtk_imhtml_toggle_backcolor(entry, backcolor);
			gtk_imhtml_toggle_background(entry, background);
		}

		g_free(fontface);
		g_free(forecolor);
		g_free(backcolor);
		g_free(background);
	}
	else
	{
		/* This is done in default_formatize, which is called from clear_formatting_cb,
		 * which is (obviously) a clear_formatting signal handler.  However, if we're
		 * here, we didn't call gtk_imhtml_clear_formatting() (because we want to
		 * preserve the formatting exactly as it is), so we have to do this now. */
		gtk_imhtml_set_whole_buffer_formatting_only(entry,
			(conv->features & PURPLE_CONNECTION_FORMATTING_WBFO));
	}

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);

	gray_stuff_out(gtkconv);
	update_typing_icon(gtkconv);

	gtk_window_set_title(GTK_WINDOW(gtkconv->win->window),
	                     gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
}

static void
menu_conv_sel_send_cb(GObject *m, gpointer data)
{
	PurpleAccount *account = g_object_get_data(m, "purple_account");
	gchar *name = g_object_get_data(m, "purple_buddy_name");
	PurpleConversation *conv;

	if (gtk_check_menu_item_get_active((GtkCheckMenuItem*) m) == FALSE)
		return;

	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, name);
	pidgin_conv_switch_active_conversation(conv);
}

static void
insert_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *position,
			   gchar *new_text, gint new_text_length, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleConversation *conv;

	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;

	if (!purple_prefs_get_bool("/core/conversations/im/send_typing"))
		return;

	got_typing_keypress(gtkconv, (gtk_text_iter_is_start(position) &&
	                    gtk_text_iter_is_end(position)));
}

static void
delete_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *start_pos,
			   GtkTextIter *end_pos, gpointer user_data)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleConversation *conv;
	PurpleConvIm *im;

	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;

	if (!purple_prefs_get_bool("/core/conversations/im/send_typing"))
		return;

	im = PURPLE_CONV_IM(conv);

	if (gtk_text_iter_is_start(start_pos) && gtk_text_iter_is_end(end_pos)) {

		/* We deleted all the text, so turn off typing. */
		purple_conv_im_stop_send_typed_timeout(im);

		serv_send_typing(purple_conversation_get_gc(conv),
						 purple_conversation_get_name(conv),
						 PURPLE_NOT_TYPING);
	}
	else {
		/* We're deleting, but not all of it, so it counts as typing. */
		got_typing_keypress(gtkconv, FALSE);
	}
}

/**************************************************************************
 * A bunch of buddy icon functions
 **************************************************************************/

static GList *get_prpl_icon_list(PurpleAccount *account)
{
	GList *l = NULL;
	GdkPixbuf *pixbuf;
	PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(account)));
	const char *prpl = prpl_info->list_icon(account, NULL);
	char *filename, *path;
	l = g_hash_table_lookup(prpl_lists, prpl);
	if (l)
		return l;
	filename = g_strdup_printf("%s.png", prpl);

	path = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols", "16", filename, NULL);
	pixbuf = gdk_pixbuf_new_from_file(path, NULL);
	if (pixbuf)
		l = g_list_append(l, pixbuf);
	g_free(path);

	path = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols", "22", filename, NULL);
        pixbuf = gdk_pixbuf_new_from_file(path, NULL);
        if (pixbuf)
                l = g_list_append(l, pixbuf);
        g_free(path);

	path = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols", "48", filename, NULL);
        pixbuf = gdk_pixbuf_new_from_file(path, NULL);
        if (pixbuf)
                l = g_list_append(l, pixbuf);
        g_free(path);

	g_hash_table_insert(prpl_lists, g_strdup(prpl), l);
	return l;
}

static GList *
pidgin_conv_get_tab_icons(PurpleConversation *conv)
{
	PurpleAccount *account = NULL;
	const char *name = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		PurpleBuddy *b = purple_find_buddy(account, name);
		if (b != NULL) {
                	PurplePresence *p;
	                p = purple_buddy_get_presence(b);
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY))
				return away_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE))
				return busy_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY))
				return xa_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE))
				return offline_list;
			else
				return available_list;
		}
	}

	return get_prpl_icon_list(account);
}

GdkPixbuf *
pidgin_conv_get_tab_icon(PurpleConversation *conv, gboolean small_icon)
{
        PurpleAccount *account = NULL;
        const char *name = NULL;
        GdkPixbuf *status = NULL;
        PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

        g_return_val_if_fail(conv != NULL, NULL);

        account = purple_conversation_get_account(conv);
        name = purple_conversation_get_name(conv);

        g_return_val_if_fail(account != NULL, NULL);
        g_return_val_if_fail(name != NULL, NULL);

        /* Use the buddy icon, if possible */
        if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
                PurpleBuddy *b = purple_find_buddy(account, name);
                if (b != NULL) {
                        /* I hate this hack.  It fixes a bug where the pending message icon
                          * displays in the conv tab even though it shouldn't.
                          * A better solution would be great. */
                        if (ops && ops->update)
                                ops->update(NULL, (PurpleBlistNode*)b);

                        status = pidgin_blist_get_status_icon((PurpleBlistNode*)b,
                                (small_icon ? PIDGIN_STATUS_ICON_SMALL : PIDGIN_STATUS_ICON_LARGE));
                }
        }

        /* If they don't have a buddy icon, then use the PRPL icon */
        if (status == NULL)
                status = pidgin_create_prpl_icon(account, small_icon ? PIDGIN_PRPL_ICON_SMALL : PIDGIN_PRPL_ICON_LARGE);

        return status;
}


static void
update_tab_icon(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PidginWindow *win;
	GList *l;
	GdkPixbuf *status = NULL;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	status = pidgin_conv_get_tab_icon(conv, TRUE);

	g_return_if_fail(status != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->icon), status);
	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->menu_icon), status);

	if (status != NULL)
		g_object_unref(status);

	if (pidgin_conv_window_is_active_conversation(conv) &&
		(purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_IM ||
		 gtkconv->u.im->anim == NULL))
	{
		l = pidgin_conv_get_tab_icons(conv);

		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
	}
}

/* This gets added as an idle handler when doing something that
 * redraws the icon. It sets the auto_resize gboolean to TRUE.
 * This way, when the size_allocate callback gets triggered, it notices
 * that this is an autoresize, and after the main loop iterates, it
 * gets set back to FALSE
 */
static gboolean reset_auto_resize_cb(gpointer data)
{
	PidginConversation *gtkconv = (PidginConversation *)data;
	gtkconv->auto_resize = FALSE;
	return FALSE;
}

static gboolean
redraw_icon(gpointer data)
{
	PidginConversation *gtkconv = (PidginConversation *)data;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurplePluginProtocolInfo *prpl_info = NULL;

	GdkPixbuf *buf;
	GdkPixbuf *scale;
	gint delay;
	int scale_width, scale_height;

	gtkconv = PIDGIN_CONVERSATION(conv);
	account = purple_conversation_get_account(conv);
	if(account && account->gc)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

	gtkconv->auto_resize = TRUE;
	g_idle_add(reset_auto_resize_cb, gtkconv);

	gdk_pixbuf_animation_iter_advance(gtkconv->u.im->iter, NULL);
	buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);

	pidgin_buddy_icon_get_scale_size(buf, &prpl_info->icon_spec,
			PURPLE_ICON_SCALE_DISPLAY, &scale_width, &scale_height);

	/* this code is ugly, and scares me */
	scale = gdk_pixbuf_scale_simple(buf,
		MAX(gdk_pixbuf_get_width(buf) * scale_width /
		    gdk_pixbuf_animation_get_width(gtkconv->u.im->anim), 1),
		MAX(gdk_pixbuf_get_height(buf) * scale_height /
		    gdk_pixbuf_animation_get_height(gtkconv->u.im->anim), 1),
		GDK_INTERP_BILINEAR);

	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->u.im->icon), scale);
	g_object_unref(G_OBJECT(scale));
	gtk_widget_queue_draw(gtkconv->u.im->icon);

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);

	return FALSE;
}

static void
start_anim(GtkObject *obj, PidginConversation *gtkconv)
{
	int delay;

	if (gtkconv->u.im->anim == NULL)
		return;

	if (gtkconv->u.im->icon_timer != 0)
		return;

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim))
		return;

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);
}

static void
remove_icon(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginWindow *gtkwin;

	g_return_if_fail(conv != NULL);

	if (gtkconv->u.im->icon_container != NULL)
		gtk_widget_destroy(gtkconv->u.im->icon_container);

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->icon_timer = 0;
	gtkconv->u.im->icon = NULL;
	gtkconv->u.im->anim = NULL;
	gtkconv->u.im->iter = NULL;
	gtkconv->u.im->icon_container = NULL;
	gtkconv->u.im->show_icon = FALSE;

	gtkwin = gtkconv->win;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkwin->menu.show_icon), FALSE);
}

static void
saveicon_writefile_cb(void *user_data, const char *filename)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleConversation *conv = gtkconv->active_conv;
	FILE *fp;
	PurpleBuddyIcon *icon;
	const void *data;
	size_t len;

	if ((fp = g_fopen(filename, "wb")) == NULL) {
		purple_notify_error(gtkconv, NULL, _("Unable to open file."), NULL);
		return;
	}

	icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
	data = purple_buddy_icon_get_data(icon, &len);

	if ((len <= 0) || (data == NULL) || (fwrite(data, 1, len, fp) != len)) {
		purple_notify_error(gtkconv, NULL, _("Unable to save icon file to disk."), NULL);
		fclose(fp);
		g_unlink(filename);
		return;
	}
	fclose(fp);
}

static const char *
custom_icon_pref_name(PidginConversation *gtkconv)
{
	PurpleConversation *conv;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	conv = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);
	buddy = purple_find_buddy(account, purple_conversation_get_name(conv));
	if (buddy) {
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		return purple_blist_node_get_string((PurpleBlistNode*)contact, "custom_buddy_icon");
	}
	return NULL;
}

static void
custom_icon_sel_cb(const char *filename, gpointer data)
{
	if (filename) {
		PidginConversation *gtkconv = data;
		PurpleConversation *conv = gtkconv->active_conv;
		PurpleAccount *account = purple_conversation_get_account(conv);
		pidgin_set_custom_buddy_icon(account, purple_conversation_get_name(conv), filename);
	}
}

static void
set_custom_icon_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	GtkWidget *win = pidgin_buddy_icon_chooser_new(GTK_WINDOW(gtkconv->win->window),
						custom_icon_sel_cb, gtkconv);
	gtk_widget_show_all(win);
}

static void
remove_custom_icon_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv;
	PurpleAccount *account;

	conv = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);
	pidgin_set_custom_buddy_icon(account, purple_conversation_get_name(conv), NULL);
}

static void
icon_menu_save_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	const gchar *ext;
	gchar *buf;

	g_return_if_fail(conv != NULL);

	ext = purple_buddy_icon_get_type(purple_conv_im_get_icon(PURPLE_CONV_IM(conv)));
	if (ext == NULL)
		ext = "icon";

	buf = g_strdup_printf("%s.%s", purple_normalize(conv->account, conv->name), ext);

	purple_request_file(gtkconv, _("Save Icon"), buf, TRUE,
					 G_CALLBACK(saveicon_writefile_cb), NULL, gtkconv);

	g_free(buf);
}

static void
stop_anim(GtkObject *obj, PidginConversation *gtkconv)
{
	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;
}


static void
toggle_icon_animate_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	gtkconv->u.im->animate =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));

	if (gtkconv->u.im->animate)
		start_anim(NULL, gtkconv);
	else
		stop_anim(NULL, gtkconv);
}

static gboolean
icon_menu(GtkObject *obj, GdkEventButton *e, PidginConversation *gtkconv)
{
	static GtkWidget *menu = NULL;
	const char *pref;

	if (e->button != 3 || e->type != GDK_BUTTON_PRESS)
		return FALSE;

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu != NULL)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	if (gtkconv->u.im->anim &&
		!(gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)))
	{
		pidgin_new_check_item(menu, _("Animate"),
							G_CALLBACK(toggle_icon_animate_cb), gtkconv,
							gtkconv->u.im->icon_timer);
	}

	pidgin_new_item_from_stock(menu, _("Hide Icon"), NULL, G_CALLBACK(remove_icon),
							 gtkconv, 0, 0, NULL);

	pidgin_new_item_from_stock(menu, _("Save Icon As..."), GTK_STOCK_SAVE_AS,
							 G_CALLBACK(icon_menu_save_cb), gtkconv,
							 0, 0, NULL);

	pidgin_new_item_from_stock(menu, _("Set Custom Icon..."), NULL,
							 G_CALLBACK(set_custom_icon_cb), gtkconv,
							 0, 0, NULL);

	/* Is there a custom icon for this person? */
	pref = custom_icon_pref_name(gtkconv);
	if (pref && *pref) {
		pidgin_new_item_from_stock(menu, _("Remove Custom Icon"), NULL,
							G_CALLBACK(remove_custom_icon_cb), gtkconv,
							0, 0, NULL);
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, e->button, e->time);

	return TRUE;
}

static void
menu_buddyicon_cb(gpointer data, guint action, GtkWidget *widget)
{
	PidginWindow *win = data;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	gboolean active;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (!conv)
		return;

	g_return_if_fail(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM);

	gtkconv = PIDGIN_CONVERSATION(conv);

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	gtkconv->u.im->show_icon = active;
	if (active)
		pidgin_conv_update_buddy_icon(conv);
	else
		remove_icon(NULL, gtkconv);
}

/**************************************************************************
 * End of the bunch of buddy icon functions
 **************************************************************************/
void
pidgin_conv_present_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	if(gtkconv->win==hidden_convwin) {
		pidgin_conv_window_remove_gtkconv(hidden_convwin, gtkconv);
		pidgin_conv_placement_place(gtkconv);
	}

	pidgin_conv_switch_active_conversation(conv);
	pidgin_conv_window_switch_gtkconv(gtkconv->win, gtkconv);
	gtk_window_present(GTK_WINDOW(gtkconv->win->window));
}

GList *
pidgin_conversations_find_unseen_list(PurpleConversationType type,
										PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	GList *l;
	GList *r = NULL;
	guint c = 0;

	if (type == PURPLE_CONV_TYPE_IM) {
		l = purple_get_ims();
	} else if (type == PURPLE_CONV_TYPE_CHAT) {
		l = purple_get_chats();
	} else {
		l = purple_get_conversations();
	}

	for (; l != NULL && (max_count == 0 || c < max_count); l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		if(gtkconv->active_conv != conv)
			continue;

		if (gtkconv->unseen_state >= min_state
			&& (!hidden_only ||
				(hidden_only && gtkconv->win == hidden_convwin))) {

			r = g_list_prepend(r, conv);
			c++;
		}
	}

	return r;
}

static void
unseen_conv_menu_cb(GtkMenuItem *item, PurpleConversation *conv)
{
	g_return_if_fail(conv != NULL);
	pidgin_conv_present_conversation(conv);
}

guint
pidgin_conversations_fill_menu(GtkWidget *menu, GList *convs)
{
	GList *l;
	guint ret=0;

	g_return_val_if_fail(menu != NULL, 0);
	g_return_val_if_fail(convs != NULL, 0);

	for (l = convs; l != NULL ; l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		GtkWidget *icon = gtk_image_new();
		GdkPixbuf *pbuf = pidgin_conv_get_tab_icon(conv, TRUE);
		GtkWidget *item;
		gchar *text = g_strdup_printf("%s (%d)",
				gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)),
				gtkconv->unseen_count);

		gtk_image_set_from_pixbuf(GTK_IMAGE(icon), pbuf);
		g_object_unref(pbuf);

		item = gtk_image_menu_item_new_with_label(text);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), icon);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(unseen_conv_menu_cb), conv);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_free(text);
		ret++;
	}

	return ret;
}

PidginWindow *
pidgin_conv_get_window(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);
	return gtkconv->win;
}

static GtkItemFactoryEntry menu_items[] =
{
	/* Conversation menu */
	{ N_("/_Conversation"), NULL, NULL, 0, "<Branch>", NULL },

	{ N_("/Conversation/New Instant _Message..."), "<CTL>M", menu_new_conv_cb,
			0, "<StockItem>", PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW },

	{ "/Conversation/sep0", NULL, NULL, 0, "<Separator>", NULL },

	{ N_("/Conversation/_Find..."), NULL, menu_find_cb, 0,
			"<StockItem>", GTK_STOCK_FIND },
	{ N_("/Conversation/View _Log"), NULL, menu_view_log_cb, 0, "<Item>", NULL },
	{ N_("/Conversation/_Save As..."), NULL, menu_save_as_cb, 0,
			"<StockItem>", GTK_STOCK_SAVE_AS },
	{ N_("/Conversation/Clea_r Scrollback"), "<CTL>L", menu_clear_cb, 0, "<StockItem>", GTK_STOCK_CLEAR },

	{ "/Conversation/sep1", NULL, NULL, 0, "<Separator>", NULL },

	{ N_("/Conversation/Se_nd File..."), NULL, menu_send_file_cb, 0, "<StockItem>", PIDGIN_STOCK_FILE_TRANSFER },
	{ N_("/Conversation/Add Buddy _Pounce..."), NULL, menu_add_pounce_cb,
			0, "<Item>", NULL },
	{ N_("/Conversation/_Get Info"), "<CTL>O", menu_get_info_cb, 0,
			"<StockItem>", PIDGIN_STOCK_TOOLBAR_USER_INFO },
	{ N_("/Conversation/In_vite..."), NULL, menu_invite_cb, 0,
			"<Item>", NULL },
	{ N_("/Conversation/M_ore"), NULL, NULL, 0, "<Branch>", NULL },

	{ "/Conversation/sep2", NULL, NULL, 0, "<Separator>", NULL },

	{ N_("/Conversation/Al_ias..."), NULL, menu_alias_cb, 0,
			"<Item>", NULL },
	{ N_("/Conversation/_Block..."), NULL, menu_block_cb, 0,
			"<StockItem>", PIDGIN_STOCK_TOOLBAR_BLOCK },
	{ N_("/Conversation/_Unblock..."), NULL, menu_unblock_cb, 0,
			"<StockItem>", PIDGIN_STOCK_TOOLBAR_UNBLOCK },
	{ N_("/Conversation/_Add..."), NULL, menu_add_remove_cb, 0,
			"<StockItem>", GTK_STOCK_ADD },
	{ N_("/Conversation/_Remove..."), NULL, menu_add_remove_cb, 0,
			"<StockItem>", GTK_STOCK_REMOVE },

	{ "/Conversation/sep3", NULL, NULL, 0, "<Separator>", NULL },

	{ N_("/Conversation/_Close"), NULL, menu_close_conv_cb, 0,
			"<StockItem>", GTK_STOCK_CLOSE },

	/* Options */
	{ N_("/_Options"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Options/Enable _Logging"), NULL, menu_logging_cb, 0, "<CheckItem>", NULL },
	{ N_("/Options/Enable _Sounds"), NULL, menu_sounds_cb, 0, "<CheckItem>", NULL },
	{ N_("/Options/Show Buddy _Icon"), NULL, menu_buddyicon_cb, 0, "<CheckItem>", NULL },
	{ "/Options/sep0", NULL, NULL, 0, "<Separator>", NULL },
	{ N_("/Options/Show Formatting _Toolbars"), NULL, menu_toolbar_cb, 0, "<CheckItem>", NULL },
	{ N_("/Options/Show Ti_mestamps"), "F2", menu_timestamps_cb, 0, "<CheckItem>", NULL },
};

static const int menu_item_count =
sizeof(menu_items) / sizeof(*menu_items);

static const char *
item_factory_translate_func (const char *path, gpointer func_data)
{
	return _(path);
}

static void
sound_method_pref_changed_cb(const char *name, PurplePrefType type,
							 gconstpointer value, gpointer data)
{
	PidginWindow *win = data;
	const char *method = value;

	if (!strcmp(method, "none"))
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.sounds),
		                               FALSE);
		gtk_widget_set_sensitive(win->menu.sounds, FALSE);
	}
	else
	{
		PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

		if (gtkconv != NULL)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.sounds),
			                               TRUE);
		gtk_widget_set_sensitive(win->menu.sounds, TRUE);

	}
}

static void
show_buddy_icons_pref_changed_cb(const char *name, PurplePrefType type,
								 gconstpointer value, gpointer data)
{
	PidginWindow *win = data;
	gboolean show_icons = GPOINTER_TO_INT(value);

	if (!show_icons)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_icon),
		                               FALSE);
		gtk_widget_set_sensitive(win->menu.show_icon, FALSE);
	}
	else
	{
		PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

		if (gtkconv != NULL)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_icon),
			                               TRUE);
		gtk_widget_set_sensitive(win->menu.show_icon, TRUE);

	}
}

static void
regenerate_options_items(PidginWindow *win)
{
	GtkWidget *menu;
	GList *list;
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PurpleBuddy *buddy;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	conv = gtkconv->active_conv;
	buddy = purple_find_buddy(conv->account, conv->name);

	menu = gtk_item_factory_get_widget(win->menu.item_factory, N_("/Conversation/More"));

	/* Remove the previous entries */
	for (list = gtk_container_get_children(GTK_CONTAINER(menu)); list; )
	{
		GtkWidget *w = list->data;
		list = list->next;
		gtk_widget_destroy(w);
	}

	/* Now add the stuff */
	if (buddy)
	{
		if (purple_account_is_connected(conv->account))
			pidgin_append_blist_node_proto_menu(menu, conv->account->gc,
												  (PurpleBlistNode *)buddy);
		pidgin_append_blist_node_extended_menu(menu, (PurpleBlistNode *)buddy);
	}

	if ((list = gtk_container_get_children(GTK_CONTAINER(menu))) == NULL)
	{
		GtkWidget *item = gtk_menu_item_new_with_label(_("No actions available"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_set_sensitive(item, FALSE);
	}

	gtk_widget_show_all(menu);
}

static void menubar_activated(GtkWidget *item, gpointer data)
{
	PidginWindow *win = data;
	regenerate_options_items(win);

	/* The following are to make sure the 'More' submenu is not regenerated every time
	 * the focus shifts from 'Conversations' to some other menu and back. */
	g_signal_handlers_block_by_func(G_OBJECT(item), G_CALLBACK(menubar_activated), data);
	g_signal_connect(G_OBJECT(win->menu.menubar), "deactivate", G_CALLBACK(focus_out_from_menubar), data);
}

static void
focus_out_from_menubar(GtkWidget *wid, PidginWindow *win)
{
	/* The menubar has been deactivated. Make sure the 'More' submenu is regenerated next time
	 * the 'Conversation' menu pops up. */
	GtkWidget *menuitem = gtk_item_factory_get_item(win->menu.item_factory, N_("/Conversation"));
	g_signal_handlers_unblock_by_func(G_OBJECT(menuitem), G_CALLBACK(menubar_activated), win);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->menu.menubar),
				G_CALLBACK(focus_out_from_menubar), win);
}

static GtkWidget *
setup_menubar(PidginWindow *win)
{
	GtkAccelGroup *accel_group;
	const char *method;
	GtkWidget *menuitem;

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group(GTK_WINDOW(win->window), accel_group);
	g_object_unref(accel_group);

	win->menu.item_factory =
		gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);

	gtk_item_factory_set_translate_func(win->menu.item_factory,
	                                    (GtkTranslateFunc)item_factory_translate_func,
	                                    NULL, NULL);

	gtk_item_factory_create_items(win->menu.item_factory, menu_item_count,
	                              menu_items, win);
	g_signal_connect(G_OBJECT(accel_group), "accel-changed",
	                 G_CALLBACK(pidgin_save_accels_cb), NULL);

	/* Make sure the 'Conversation -> More' menuitems are regenerated whenever
	 * the 'Conversation' menu pops up because the entries can change after the
	 * conversation is created. */
	menuitem = gtk_item_factory_get_item(win->menu.item_factory, N_("/Conversation"));
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(menubar_activated), win);

	win->menu.menubar =
		gtk_item_factory_get_widget(win->menu.item_factory, "<main>");

	win->menu.view_log =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/View Log"));

	/* --- */

	win->menu.send_file =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Send File..."));

	win->menu.add_pounce =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Add Buddy Pounce..."));

	/* --- */

	win->menu.get_info =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Get Info"));

	win->menu.invite =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Invite..."));

	/* --- */

	win->menu.alias =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Alias..."));

	win->menu.block =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Block..."));

	win->menu.unblock =
		gtk_item_factory_get_widget(win->menu.item_factory,
					    N_("/Conversation/Unblock..."));

	win->menu.add =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Add..."));

	win->menu.remove =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Conversation/Remove..."));

	win->menu.logging =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Options/Enable Logging"));
	win->menu.sounds =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Options/Enable Sounds"));
	method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (method != NULL && !strcmp(method, "none"))
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.sounds),
		                               FALSE);
		gtk_widget_set_sensitive(win->menu.sounds, FALSE);
	}
	purple_prefs_connect_callback(win, PIDGIN_PREFS_ROOT "/sound/method",
				    sound_method_pref_changed_cb, win);

	win->menu.show_formatting_toolbar =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Options/Show Formatting Toolbars"));
	win->menu.show_timestamps =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Options/Show Timestamps"));
	win->menu.show_icon =
		gtk_item_factory_get_widget(win->menu.item_factory,
		                            N_("/Options/Show Buddy Icon"));
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_icon),
		                               FALSE);
		gtk_widget_set_sensitive(win->menu.show_icon, FALSE);
	}
	purple_prefs_connect_callback(win, PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons",
				    show_buddy_icons_pref_changed_cb, win);

	win->menu.tray = pidgin_menu_tray_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(win->menu.menubar),
	                      win->menu.tray);
	gtk_widget_show(win->menu.tray);

	gtk_widget_show(win->menu.menubar);

	return win->menu.menubar;
}


/**************************************************************************
 * Utility functions
 **************************************************************************/

static void
got_typing_keypress(PidginConversation *gtkconv, gboolean first)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConvIm *im;

	/*
	 * We know we got something, so we at least have to make sure we don't
	 * send PURPLE_TYPED any time soon.
	 */

	im = PURPLE_CONV_IM(conv);

	purple_conv_im_stop_send_typed_timeout(im);
	purple_conv_im_start_send_typed_timeout(im);

	/* Check if we need to send another PURPLE_TYPING message */
	if (first || (purple_conv_im_get_type_again(im) != 0 &&
				  time(NULL) > purple_conv_im_get_type_again(im)))
	{
		unsigned int timeout;
		timeout = serv_send_typing(purple_conversation_get_gc(conv),
								   purple_conversation_get_name(conv),
								   PURPLE_TYPING);
		purple_conv_im_set_type_again(im, timeout);
	}
}

static gboolean
typing_animation(gpointer data) {
	PidginConversation *gtkconv = data;
	PidginWindow *gtkwin = gtkconv->win;
	const char *stock_id = NULL;

	if(gtkconv != pidgin_conv_window_get_active_gtkconv(gtkwin)) {
		return FALSE;
	}

	switch (rand() % 5) {
	case 0:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING0;
		break;
	case 1:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING1;
		break;
	case 2:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING2;
		break;
	case 3:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING3;
		break;
	case 4:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING4;
		break;
	}
	if (gtkwin->menu.typing_icon == NULL) {
		 gtkwin->menu.typing_icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
 		 pidgin_menu_tray_append(PIDGIN_MENU_TRAY(gtkwin->menu.tray),
                                                                  gtkwin->menu.typing_icon,
                                                                  _("User is typing..."));
	} else {
		gtk_image_set_from_stock(GTK_IMAGE(gtkwin->menu.typing_icon), stock_id, GTK_ICON_SIZE_MENU);
	}
	gtk_widget_show(gtkwin->menu.typing_icon);
	return TRUE;
}

static void
update_typing_icon(PidginConversation *gtkconv)
{
	PidginWindow *gtkwin;
	PurpleConvIm *im = NULL;
	PurpleConversation *conv = gtkconv->active_conv;
	char *stock_id;
	const char *tooltip;

	gtkwin = gtkconv->win;

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		im = PURPLE_CONV_IM(conv);

	if (gtkwin->menu.typing_icon) {
		gtk_widget_hide(gtkwin->menu.typing_icon);
	}

	if (!im || (purple_conv_im_get_typing_state(im) == PURPLE_NOT_TYPING)) {
		if (gtkconv->u.im->typing_timer != 0)
			g_source_remove(gtkconv->u.im->typing_timer);
		return;
	}

	if (purple_conv_im_get_typing_state(im) == PURPLE_TYPING) {
		if (gtkconv->u.im->typing_timer == 0) {
			gtkconv->u.im->typing_timer = g_timeout_add(250, typing_animation, gtkconv);
		}
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING1;
		tooltip = _("User is typing...");
	} else {
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING0;
		tooltip = _("User has typed something and stopped");
		g_source_remove(gtkconv->u.im->typing_timer);
		gtkconv->u.im->typing_timer = 0;
	}

	if (gtkwin->menu.typing_icon == NULL)
	{
		gtkwin->menu.typing_icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
		pidgin_menu_tray_append(PIDGIN_MENU_TRAY(gtkwin->menu.tray),
								  gtkwin->menu.typing_icon,
								  tooltip);
	}
	else
	{
		gtk_image_set_from_stock(GTK_IMAGE(gtkwin->menu.typing_icon), stock_id, GTK_ICON_SIZE_MENU);
		pidgin_menu_tray_set_tooltip(PIDGIN_MENU_TRAY(gtkwin->menu.tray),
									   gtkwin->menu.typing_icon,
									   tooltip);
	}

	gtk_widget_show(gtkwin->menu.typing_icon);
}

static gboolean
update_send_to_selection(PidginWindow *win)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	GtkWidget *menu;
	GList *child;
	PurpleBuddy *b;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return FALSE;

	account = purple_conversation_get_account(conv);

	if (account == NULL)
		return FALSE;

	if (win->menu.send_to == NULL)
		return FALSE;

	if (!(b = purple_find_buddy(account, conv->name)))
		return FALSE;


	gtk_widget_show(win->menu.send_to);

	menu = gtk_menu_item_get_submenu(
		GTK_MENU_ITEM(win->menu.send_to));

	for (child = gtk_container_get_children(GTK_CONTAINER(menu));
		 child != NULL;
		 child = child->next) {

		GtkWidget *item = child->data;
		PurpleBuddy *item_buddy;
		PurpleAccount *item_account = g_object_get_data(G_OBJECT(item), "purple_account");
		gchar *buddy_name = g_object_get_data(G_OBJECT(item),
		                                      "purple_buddy_name");
		item_buddy = purple_find_buddy(item_account, buddy_name);

		if (b == item_buddy) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
			break;
		}
	}

	return FALSE;
}

static gboolean
send_to_item_enter_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), TRUE);
	return FALSE;
}

static gboolean
send_to_item_leave_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	return FALSE;
}

static void
create_sendto_item(GtkWidget *menu, GtkSizeGroup *sg, GSList **group, PurpleBuddy *buddy, PurpleAccount *account, const char *name)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *menuitem;
	GdkPixbuf *pixbuf;
	gchar *text;

	/* Create a pixmap for the protocol icon. */
	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);

	/* Now convert it to GtkImage */
	if (pixbuf == NULL)
		image = gtk_image_new();
	else
	{
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
	}

	gtk_size_group_add_widget(sg, image);

	/* Make our menu item */
	text = g_strdup_printf("%s (%s)", name, purple_account_get_username(account));
	menuitem = gtk_radio_menu_item_new_with_label(*group, text);
	g_free(text);
	*group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

	/* Do some evil, see some evil, speak some evil. */
	box = gtk_hbox_new(FALSE, 0);

	label = gtk_bin_get_child(GTK_BIN(menuitem));
	g_object_ref(label);
	gtk_container_remove(GTK_CONTAINER(menuitem), label);

	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 4);

	if (buddy != NULL &&
	    !purple_presence_is_online(purple_buddy_get_presence(buddy)) &&
	    !purple_account_supports_offline_message(account, buddy))
	{
		gtk_widget_set_sensitive(label, FALSE);

		/* Set the label sensitive when the menuitem is highlighted and
		 * insensitive again when the mouse leaves it. This way, it
		 * doesn't appear weird from the highlighting of the embossed
		 * (insensitive style) text.*/
		g_signal_connect(menuitem, "enter-notify-event",
				 G_CALLBACK(send_to_item_enter_notify_cb), label);
		g_signal_connect(menuitem, "leave-notify-event",
				 G_CALLBACK(send_to_item_leave_notify_cb), label);
	}

	g_object_unref(label);

	gtk_container_add(GTK_CONTAINER(menuitem), box);

	gtk_widget_show(label);
	gtk_widget_show(image);
	gtk_widget_show(box);

	/* Set our data and callbacks. */
	g_object_set_data(G_OBJECT(menuitem), "purple_account", account);
	g_object_set_data_full(G_OBJECT(menuitem), "purple_buddy_name", g_strdup(name), g_free);

	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(menu_conv_sel_send_cb), NULL);

	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static void
generate_send_to_items(PidginWindow *win)
{
	GtkWidget *menu;
	GSList *group = NULL;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	PidginConversation *gtkconv;
	GSList *l, *buds;

	g_return_if_fail(win != NULL);

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	g_return_if_fail(gtkconv != NULL);

	if (win->menu.send_to != NULL)
		gtk_widget_destroy(win->menu.send_to);

	/* Build the Send To menu */
	win->menu.send_to = gtk_menu_item_new_with_mnemonic(_("_Send To"));
	gtk_widget_show(win->menu.send_to);

	menu = gtk_menu_new();
	gtk_menu_shell_insert(GTK_MENU_SHELL(win->menu.menubar),
	                      win->menu.send_to, 2);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(win->menu.send_to), menu);

	gtk_widget_show(menu);

	if (gtkconv->active_conv->type == PURPLE_CONV_TYPE_IM) {
		buds = purple_find_buddies(gtkconv->active_conv->account, gtkconv->active_conv->name);

		if (buds == NULL)
		{
			/* The user isn't on the buddy list. */
			create_sendto_item(menu, sg, &group, NULL, gtkconv->active_conv->account, gtkconv->active_conv->name);
		}
		else
		{
			GList *list = NULL, *iter;
			for (l = buds; l != NULL; l = l->next)
			{
				PurpleBlistNode *node;

				node = (PurpleBlistNode *) purple_buddy_get_contact((PurpleBuddy *)l->data);

				for (node = node->child; node != NULL; node = node->next)
				{
					PurpleBuddy *buddy = (PurpleBuddy *)node;
					PurpleAccount *account;

					if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
						continue;

					account = purple_buddy_get_account(buddy);
					if (purple_account_is_connected(account))
					{
						/* Use the PurplePresence to get unique buddies. */
						PurplePresence *presence = purple_buddy_get_presence(buddy);
						if (!g_list_find(list, presence))
							list = g_list_prepend(list, presence);
					}
				}
			}

			/* Loop over the list backwards so we get the items in the right order,
			 * since we did a g_list_prepend() earlier. */
			for (iter = g_list_last(list); iter != NULL; iter = iter->prev)
			{
				PurplePresence *pre = iter->data;
				PurpleBuddy *buddy = purple_presence_get_buddies(pre)->data;
				create_sendto_item(menu, sg, &group, buddy,
							purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
			}
			g_list_free(list);
			g_slist_free(buds);
		}
	}

	g_object_unref(sg);

	gtk_widget_show(win->menu.send_to);
	/* TODO: This should never be insensitive.  Possibly hidden or not. */
	if (!group)
		gtk_widget_set_sensitive(win->menu.send_to, FALSE);
	update_send_to_selection(win);
}

static GList *
generate_invite_user_names(PurpleConnection *gc)
{
	PurpleBlistNode *gnode,*cnode,*bnode;
	static GList *tmp = NULL;

	g_list_free(tmp);

	tmp = g_list_append(NULL, "");

	if (gc != NULL) {
		for(gnode = purple_get_blist()->root; gnode; gnode = gnode->next) {
			if(!PURPLE_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for(cnode = gnode->child; cnode; cnode = cnode->next) {
				if(!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for(bnode = cnode->child; bnode; bnode = bnode->next) {
					PurpleBuddy *buddy;

					if(!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
						continue;

					buddy = (PurpleBuddy *)bnode;

					if (buddy->account == gc->account &&
							PURPLE_BUDDY_IS_ONLINE(buddy))
						tmp = g_list_insert_sorted(tmp, buddy->name,
												   (GCompareFunc)g_utf8_collate);
				}
			}
		}
	}

	return tmp;
}

static GdkPixbuf *
get_chat_buddy_status_icon(PurpleConvChat *chat, const char *name, PurpleConvChatBuddyFlags flags)
{
        PidginConversation *gtkconv = PIDGIN_CONVERSATION(chat->conv);
	GdkPixbuf *pixbuf, *scale, *scale2;
	char *filename;
	const char *image = NULL;

	if (flags & PURPLE_CBFLAGS_FOUNDER) {
		image = PIDGIN_STOCK_STATUS_FOUNDER;
	} else if (flags & PURPLE_CBFLAGS_OP) {
		image = PIDGIN_STOCK_STATUS_OPERATOR;
	} else if (flags & PURPLE_CBFLAGS_HALFOP) {
		image = PIDGIN_STOCK_STATUS_HALFOP;
	} else if (flags & PURPLE_CBFLAGS_VOICE) {
		image = PIDGIN_STOCK_STATUS_VOICE;
	} else if ((!flags) && purple_conv_chat_is_user_ignored(chat, name)) {
		image = PIDGIN_STOCK_STATUS_IGNORED;
	} else {
		return NULL;
	}

	pixbuf = gtk_widget_render_icon (gtkconv->tab_cont, image, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL),
				 	 "GtkTreeView");

	if (!pixbuf)
		return NULL;

	scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
	g_object_unref(pixbuf);

	if (flags && purple_conv_chat_is_user_ignored(chat, name)) {
/* TODO: the .../status/default directory isn't installed, should it be? */
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "status", "default", "ignored.png", NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
		scale2 = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		gdk_pixbuf_composite(scale2, scale, 0, 0, 16, 16, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 192);
		g_object_unref(scale2);
	}

	return scale;
}

static void
add_chat_buddy_common(PurpleConversation *conv, PurpleConvChatBuddy *cb, const char *old_name)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	PurpleConvChat *chat;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	GtkListStore *ls;
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	gboolean is_me = FALSE;
	gboolean is_buddy;
	gchar *tmp, *alias_key, *name, *alias;
	int flags;

	alias = cb->alias;
	name  = cb->name;
	flags = GPOINTER_TO_INT(cb->flags);

	chat    = PURPLE_CONV_CHAT(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	gc      = purple_conversation_get_gc(conv);

	if (!gc || !(prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)))
		return;

	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)));

	pixbuf = get_chat_buddy_status_icon(chat, name, flags);

	if (!strcmp(chat->nick, purple_normalize(conv->account, old_name != NULL ? old_name : name)))
		is_me = TRUE;

	is_buddy = (purple_find_buddy(conv->account, name) != NULL);

	tmp = g_utf8_casefold(alias, -1);
	alias_key = g_utf8_collate_key(tmp, -1);
	g_free(tmp);

	if (is_me)
	{
		GdkColor send_color;
		gdk_color_parse(SEND_COLOR, &send_color);

#if GTK_CHECK_VERSION(2,6,0)
		gtk_list_store_insert_with_values(ls, &iter,
/*
 * The GTK docs are mute about the effects of the "row" value for performance.
 * X-Chat hardcodes their value to 0 (prepend) and -1 (append), so we will too.
 * It *might* be faster to search the gtk_list_store and set row accurately,
 * but no one in #gtk+ seems to know anything about it either.
 * Inserting in the "wrong" location has no visible ill effects. - F.P.
 */
		                                  -1, /* "row" */
		                                  CHAT_USERS_ICON_COLUMN,  pixbuf,
		                                  CHAT_USERS_ALIAS_COLUMN, alias,
		                                  CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
		                                  CHAT_USERS_NAME_COLUMN,  name,
		                                  CHAT_USERS_FLAGS_COLUMN, flags,
		                                  CHAT_USERS_COLOR_COLUMN, &send_color,
		                                  CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		                                  -1);
	}
	else
	{
		gtk_list_store_insert_with_values(ls, &iter,
		                                  -1, /* "row" */
		                                  CHAT_USERS_ICON_COLUMN,  pixbuf,
		                                  CHAT_USERS_ALIAS_COLUMN, alias,
		                                  CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
		                                  CHAT_USERS_NAME_COLUMN,  name,
		                                  CHAT_USERS_FLAGS_COLUMN, flags,
		                                  CHAT_USERS_COLOR_COLUMN, get_nick_color(gtkconv, name),
		                                  CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		                                  -1);
#else
		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter,
		                   CHAT_USERS_ICON_COLUMN,  pixbuf,
		                   CHAT_USERS_ALIAS_COLUMN, alias,
		                   CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
		                   CHAT_USERS_NAME_COLUMN,  name,
		                   CHAT_USERS_FLAGS_COLUMN, flags,
		                   CHAT_USERS_COLOR_COLUMN, &send_color,
		                   CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		                   -1);
	}
	else
	{
		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter,
		                   CHAT_USERS_ICON_COLUMN,  pixbuf,
		                   CHAT_USERS_ALIAS_COLUMN, alias,
		                   CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
		                   CHAT_USERS_NAME_COLUMN,  name,
		                   CHAT_USERS_FLAGS_COLUMN, flags,
		                   CHAT_USERS_COLOR_COLUMN, get_nick_color(gtkconv, name),
		                   CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		                   -1);
#endif
	}

	if (pixbuf)
		g_object_unref(pixbuf);
	g_free(alias_key);
}

static void
tab_complete_process_item(int *most_matched, char *entered, char **partial, char *nick_partial,
				  GList **matches, gboolean command, char *name)
{
	strncpy(nick_partial, name, strlen(entered));
	nick_partial[strlen(entered)] = '\0';
	if (purple_utf8_strcasecmp(nick_partial, entered))
		return;

	/* if we're here, it's a possible completion */

	if (*most_matched == -1) {
		/*
		 * this will only get called once, since from now
		 * on *most_matched is >= 0
		 */
		*most_matched = strlen(name);
		*partial = g_strdup(name);
	}
	else if (*most_matched) {
		char *tmp = g_strdup(name);

		while (purple_utf8_strcasecmp(tmp, *partial)) {
			(*partial)[*most_matched] = '\0';
			if (*most_matched < strlen(tmp))
				tmp[*most_matched] = '\0';
			(*most_matched)--;
		}
		(*most_matched)++;

		g_free(tmp);
	}

	*matches = g_list_insert_sorted(*matches, g_strdup(name),
								   (GCompareFunc)purple_utf8_strcasecmp);
}

static gboolean
tab_complete(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	GtkTextIter cursor, word_start, start_buffer;
	int start;
	int most_matched = -1;
	char *entered, *partial = NULL;
	char *text;
	char *nick_partial;
	const char *prefix;
	GList *matches = NULL;
	gboolean command = FALSE;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
	gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
			gtk_text_buffer_get_insert(gtkconv->entry_buffer));

	word_start = cursor;

	/* if there's nothing there just return */
	if (!gtk_text_iter_compare(&cursor, &start_buffer))
		return (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) ? TRUE : FALSE;

	text = gtk_text_buffer_get_text(gtkconv->entry_buffer, &start_buffer,
									&cursor, FALSE);

	/* if we're at the end of ": " we need to move back 2 spaces */
	start = strlen(text) - 1;

	if (strlen(text) >= 2 && !strncmp(&text[start-1], ": ", 2)) {
		gtk_text_iter_backward_chars(&word_start, 2);
		start-=2;
	}

	/* find the start of the word that we're tabbing */
	while (start >= 0 && text[start] != ' ') {
		gtk_text_iter_backward_char(&word_start);
		start--;
	}

	prefix = pidgin_get_cmd_prefix();
	if (start == -1 && (strlen(text) >= strlen(prefix)) && !strncmp(text, prefix, strlen(prefix))) {
		command = TRUE;
		gtk_text_iter_forward_chars(&word_start, strlen(prefix));
	}

	g_free(text);

	entered = gtk_text_buffer_get_text(gtkconv->entry_buffer, &word_start,
									   &cursor, FALSE);

	if (!g_utf8_strlen(entered, -1)) {
		g_free(entered);
		return (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) ? TRUE : FALSE;
	}

	nick_partial = g_malloc(strlen(entered)+1);

	if (command) {
		GList *list = purple_cmd_list(conv);
		GList *l;

		/* Commands */
		for (l = list; l != NULL; l = l->next) {
			tab_complete_process_item(&most_matched, entered, &partial, nick_partial,
									  &matches, TRUE, l->data);
		}
		g_list_free(list);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
		GList *l = purple_conv_chat_get_users(chat);
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(PIDGIN_CONVERSATION(conv)->u.chat->list));
		GtkTreeIter iter;
		int f;

		/* Users */
		for (; l != NULL; l = l->next) {
			tab_complete_process_item(&most_matched, entered, &partial, nick_partial,
									  &matches, TRUE, ((PurpleConvChatBuddy *)l->data)->name);
		}


		/* Aliases */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		{
			do {
				char *name;
				char *alias;

				gtk_tree_model_get(model, &iter,
						   CHAT_USERS_NAME_COLUMN, &name,
						   CHAT_USERS_ALIAS_COLUMN, &alias,
						   -1);

				if (strcmp(name, alias))
					tab_complete_process_item(&most_matched, entered, &partial, nick_partial,
										  &matches, FALSE, alias);
				g_free(name);
				g_free(alias);

				f = gtk_tree_model_iter_next(model, &iter);
			} while (f != 0);
		}
	} else {
		g_free(nick_partial);
		g_free(entered);
		return FALSE;
	}

	g_free(nick_partial);

	/* we're only here if we're doing new style */

	/* if there weren't any matches, return */
	if (!matches) {
		/* if matches isn't set partials won't be either */
		g_free(entered);
		return (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) ? TRUE : FALSE;
	}

	gtk_text_buffer_delete(gtkconv->entry_buffer, &word_start, &cursor);

	if (!matches->next) {
		/* there was only one match. fill it in. */
		gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
		gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
				gtk_text_buffer_get_insert(gtkconv->entry_buffer));

		if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
			char *tmp = g_strdup_printf("%s: ", (char *)matches->data);
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, tmp, -1);
			g_free(tmp);
		}
		else
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
											 matches->data, -1);

		g_free(matches->data);
		matches = g_list_remove(matches, matches->data);
	}
	else {
		/*
		 * there were lots of matches, fill in as much as possible
		 * and display all of them
		 */
		char *addthis = g_malloc0(1);

		while (matches) {
			char *tmp = addthis;
			addthis = g_strconcat(tmp, matches->data, " ", NULL);
			g_free(tmp);
			g_free(matches->data);
			matches = g_list_remove(matches, matches->data);
		}

		purple_conversation_write(conv, NULL, addthis, PURPLE_MESSAGE_NO_LOG,
								time(NULL));
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, partial, -1);
		g_free(addthis);
	}

	g_free(entered);
	g_free(partial);

	return TRUE;
}

static void topic_callback(GtkWidget *w, PidginConversation *gtkconv)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	char *new_topic;
	const char *current_topic;

	gc      = purple_conversation_get_gc(conv);

	if(!gc || !(prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)))
		return;

	if(prpl_info->set_chat_topic == NULL)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	new_topic = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkchat->topic_text)));
	current_topic = purple_conv_chat_get_topic(PURPLE_CONV_CHAT(conv));

	if(current_topic && !g_utf8_collate(new_topic, current_topic)){
		g_free(new_topic);
		return;
	}

	gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), current_topic);

	prpl_info->set_chat_topic(gc, purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)),
			new_topic);

	g_free(new_topic);
}

static gint
sort_chat_users(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata)
{
	PurpleConvChatBuddyFlags f1 = 0, f2 = 0;
	char *user1 = NULL, *user2 = NULL;
	gboolean buddy1 = FALSE, buddy2 = FALSE;
	gint ret = 0;

	gtk_tree_model_get(model, a,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user1,
	                   CHAT_USERS_FLAGS_COLUMN, &f1,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy1,
	                   -1);
	gtk_tree_model_get(model, b,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user2,
	                   CHAT_USERS_FLAGS_COLUMN, &f2,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy2,
	                   -1);

	if (user1 == NULL || user2 == NULL) {
		if (!(user1 == NULL && user2 == NULL))
			ret = (user1 == NULL) ? -1: 1;
	} else if (f1 != f2) {
		/* sort more important users first */
		ret = (f1 > f2) ? -1 : 1;
	} else if (buddy1 != buddy2) {
		ret = (buddy1 > buddy2) ? -1 : 1;
	} else {
		ret = strcmp(user1, user2);
	}

	g_free(user1);
	g_free(user2);

	return ret;
}

static void
update_chat_alias(PurpleBuddy *buddy, PurpleConversation *conv, PurpleConnection *gc, PurplePluginProtocolInfo *prpl_info)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(conv != NULL);

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkconv->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(conv->account, buddy->name));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (!strcmp(normalized_name, purple_normalize(conv->account, name))) {
			const char *alias = name;
			char *tmp;
			char *alias_key = NULL;
			PurpleBuddy *buddy2;

			if (strcmp(chat->nick, purple_normalize(conv->account, name))) {
				/* This user is not me, so look into updating the alias. */

				if ((buddy2 = purple_find_buddy(conv->account, name)) != NULL) {
					alias = purple_buddy_get_contact_alias(buddy2);
				}

				tmp = g_utf8_casefold(alias, -1);
				alias_key = g_utf8_collate_key(tmp, -1);
				g_free(tmp);

				gtk_list_store_set(GTK_LIST_STORE(model), &iter,
								CHAT_USERS_ALIAS_COLUMN, alias,
								CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
								-1);
				g_free(alias_key);
			}
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);
}

static void
blist_node_aliased_cb(PurpleBlistNode *node, const char *old_alias, PurpleConversation *conv)
{
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;

	g_return_if_fail(node != NULL);
	g_return_if_fail(conv != NULL);

	gc = purple_conversation_get_gc(conv);
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->prpl != NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)
		return;

	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
	{
		PurpleBlistNode *bnode;

		for(bnode = node->child; bnode; bnode = bnode->next) {

			if(!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
				continue;

			update_chat_alias((PurpleBuddy *)bnode, conv, gc, prpl_info);
		}
	}
	else if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		update_chat_alias((PurpleBuddy *)node, conv, gc, prpl_info);
}

static void
buddy_cb_common(PurpleBuddy *buddy, PurpleConversation *conv, gboolean is_buddy)
{
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(conv != NULL);

	/* Do nothing if the buddy does not belong to the conv's account */
	if (purple_buddy_get_account(buddy) != purple_conversation_get_account(conv))
		return;

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(PIDGIN_CONVERSATION(conv)->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(conv->account, buddy->name));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (!strcmp(normalized_name, purple_normalize(conv->account, name))) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			                   CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, -1);
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);

	blist_node_aliased_cb((PurpleBlistNode *)buddy, NULL, conv);
}

static void
buddy_added_cb(PurpleBuddy *buddy, PurpleConversation *conv)
{
	buddy_cb_common(buddy, conv, TRUE);
}

static void
buddy_removed_cb(PurpleBuddy *buddy, PurpleConversation *conv)
{
	/* If there's another buddy for the same "dude" on the list, do nothing. */
	if (purple_find_buddy(buddy->account, buddy->name) != NULL)
		return;

	buddy_cb_common(buddy, conv, FALSE);
}

static void send_menu_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	g_signal_emit_by_name(gtkconv->entry, "message_send");
}

static void
entry_popup_menu_cb(GtkIMHtml *imhtml, GtkMenu *menu, gpointer data)
{
	GtkWidget *menuitem;
	PidginConversation *gtkconv = data;

	g_return_if_fail(menu != NULL);
	g_return_if_fail(gtkconv != NULL);

	menuitem = pidgin_new_item_from_stock(NULL, _("_Send"), NULL,
										G_CALLBACK(send_menu_cb), gtkconv,
										0, 0, NULL);
	if (gtk_text_buffer_get_char_count(imhtml->text_buffer) == 0)
		gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 0);

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 1);
}


static void resize_imhtml_cb(PidginConversation *gtkconv)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
        int wrapped_lines;
        int lines;
        GdkRectangle oneline;
	GtkRequisition sr;
        int height;
        int pad_top, pad_inside, pad_bottom;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));

        wrapped_lines = 1;
        gtk_text_buffer_get_start_iter(buffer, &iter);
        gtk_text_view_get_iter_location(GTK_TEXT_VIEW(gtkconv->entry), &iter, &oneline);
        while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(gtkconv->entry), &iter))
                wrapped_lines++;

        lines = gtk_text_buffer_get_line_count(buffer);

        /* Show a maximum of 4 lines */
        lines = MIN(lines, 4);
        wrapped_lines = MIN(wrapped_lines, 4);

        pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(gtkconv->entry));
        pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(gtkconv->entry));
        pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(gtkconv->entry));

        height = (oneline.height + pad_top + pad_bottom) * lines;
        height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_size_request(gtkconv->lower_hbox, &sr);
	if (sr.height < height + PIDGIN_HIG_BOX_SPACE) {
		gtkconv->auto_resize = TRUE;
		gtkconv->entry_growing = TRUE;
	        gtk_widget_set_size_request(gtkconv->lower_hbox, -1, height + PIDGIN_HIG_BOX_SPACE);
	        g_idle_add(reset_auto_resize_cb, gtkconv);
	}
}

static GtkWidget *
setup_chat_pane(PidginConversation *gtkconv)
{
	PurplePluginProtocolInfo *prpl_info;
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	GtkWidget *vpaned, *hpaned;
	GtkWidget *vbox, *hbox, *frame;
	GtkWidget *imhtml_sw;
	GtkPolicyType imhtml_sw_hscroll;
	GtkWidget *lbox;
	GtkWidget *label;
	GtkWidget *list;
	GtkWidget *sw;
	GtkListStore *ls;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	void *blist_handle = purple_blist_get_handle();
	GList *focus_chain = NULL;

	gtkchat = gtkconv->u.chat;
	gc      = purple_conversation_get_gc(conv);
	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(gc->prpl != NULL, NULL);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	/* Setup the outer pane. */
	vpaned = gtk_vpaned_new();
	gtk_widget_show(vpaned);

	/* Setup the top part of the pane. */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack1(GTK_PANED(vpaned), vbox, TRUE, TRUE);
	gtk_widget_show(vbox);

	if (prpl_info->options & OPT_PROTO_CHAT_TOPIC)
	{
		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		label = gtk_label_new(_("Topic:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		gtkchat->topic_text = gtk_entry_new();

		if(prpl_info->set_chat_topic == NULL) {
			gtk_editable_set_editable(GTK_EDITABLE(gtkchat->topic_text), FALSE);
		} else {
			g_signal_connect(GTK_OBJECT(gtkchat->topic_text), "activate",
					G_CALLBACK(topic_callback), gtkconv);
		}

		gtk_box_pack_start(GTK_BOX(hbox), gtkchat->topic_text, TRUE, TRUE, 0);
		gtk_widget_show(gtkchat->topic_text);
	}

	/* Setup the horizontal pane. */
	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show(hpaned);

	/* Setup gtkihmtml. */
	frame = pidgin_create_imhtml(FALSE, &gtkconv->imhtml, NULL, &imhtml_sw);
	gtk_widget_set_name(gtkconv->imhtml, "pidgin_conv_imhtml");
	gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml), TRUE);
	gtk_paned_pack1(GTK_PANED(hpaned), frame, TRUE, TRUE);
	gtk_widget_show(frame);
	gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(imhtml_sw),
	                               &imhtml_sw_hscroll, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(imhtml_sw),
	                               imhtml_sw_hscroll, GTK_POLICY_ALWAYS);

	gtk_widget_set_size_request(gtkconv->imhtml,
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_width"),
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_height"));
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "size-allocate",
					 G_CALLBACK(size_allocate_cb), gtkconv);

	g_signal_connect_after(G_OBJECT(gtkconv->imhtml), "button_press_event",
						   G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "key_press_event",
						   G_CALLBACK(refocus_entry_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "key_release_event",
						   G_CALLBACK(refocus_entry_cb), gtkconv);

	/* Build the right pane. */
	lbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, FALSE, TRUE);
	gtk_widget_show(lbox);

	/* Setup the label telling how many people are in the room. */
	gtkchat->count = gtk_label_new(_("0 people in room"));
	gtk_box_pack_start(GTK_BOX(lbox), gtkchat->count, FALSE, FALSE, 0);
	gtk_widget_show(gtkchat->count);

	/* Setup the list of users. */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(lbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	ls = gtk_list_store_new(CHAT_USERS_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
							G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
							GDK_TYPE_COLOR, G_TYPE_INT);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(ls), CHAT_USERS_ALIAS_KEY_COLUMN,
									sort_chat_users, NULL, NULL);

	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	rend = gtk_cell_renderer_pixbuf_new();

	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
												   "pixbuf", CHAT_USERS_ICON_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);
	gtk_widget_set_size_request(lbox,
	                            purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width"), -1);

	g_signal_connect(G_OBJECT(list), "button_press_event",
					 G_CALLBACK(right_click_chat_cb), gtkconv);
	g_signal_connect(G_OBJECT(list), "popup-menu",
			 G_CALLBACK(gtkconv_chat_popup_menu_cb), gtkconv);
	g_signal_connect(G_OBJECT(lbox), "size-allocate", G_CALLBACK(lbox_size_allocate_cb), gtkconv);


	rend = gtk_cell_renderer_text_new();

	g_object_set(rend,
				 "foreground-set", TRUE,
				 "weight-set", TRUE,
				 NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
	                                               "text", CHAT_USERS_ALIAS_COLUMN,
	                                               "foreground-gdk", CHAT_USERS_COLOR_COLUMN,
	                                               "weight", CHAT_USERS_WEIGHT_COLUMN,
	                                               NULL);

	purple_signal_connect(blist_handle, "buddy-added",
						gtkchat, PURPLE_CALLBACK(buddy_added_cb), conv);
	purple_signal_connect(blist_handle, "buddy-removed",
						gtkchat, PURPLE_CALLBACK(buddy_removed_cb), conv);
	purple_signal_connect(blist_handle, "blist-node-aliased",
						gtkchat, PURPLE_CALLBACK(blist_node_aliased_cb), conv);

#if GTK_CHECK_VERSION(2,6,0)
	gtk_tree_view_column_set_expand(col, TRUE);
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_widget_show(list);

	gtkchat->list = list;

	gtk_container_add(GTK_CONTAINER(sw), list);

	/* Setup the bottom half of the conversation window */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox, FALSE, TRUE);
	gtk_widget_show(vbox);

	gtkconv->lower_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->lower_hbox, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->lower_hbox);

	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_end(GTK_BOX(gtkconv->lower_hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	/* Setup the toolbar, entry widget and all signals */
	frame = pidgin_create_imhtml(TRUE, &gtkconv->entry, &gtkconv->toolbar, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	g_signal_connect(G_OBJECT(gtkconv->entry), "populate-popup",
					 G_CALLBACK(entry_popup_menu_cb), gtkconv);

	gtk_widget_set_name(gtkconv->entry, "pidgin_conv_entry");
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->entry),
								 purple_account_get_protocol_name(conv->account));
	gtk_widget_set_size_request(gtkconv->lower_hbox, -1,
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height"));
	gtkconv->entry_buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));
	g_object_set_data(G_OBJECT(gtkconv->entry_buffer), "user_data", gtkconv);
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry_buffer), "changed",
                                 G_CALLBACK(resize_imhtml_cb), gtkconv);

	g_signal_connect(G_OBJECT(gtkconv->entry), "key_press_event",
	                 G_CALLBACK(entry_key_press_cb), gtkconv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "message_send",
	                       G_CALLBACK(send_cb), gtkconv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "button_press_event",
	                       G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->lower_hbox), "size-allocate",
	                 G_CALLBACK(size_allocate_cb), gtkconv);

	default_formatize(gtkconv);

	/*
	 * Focus for chat windows should be as follows:
	 * Tab title -> chat topic -> conversation scrollback -> user list ->
	 *   user list buttons -> entry -> buttons at bottom
	 */
	focus_chain = g_list_prepend(focus_chain, gtkconv->entry);
	gtk_container_set_focus_chain(GTK_CONTAINER(vbox), focus_chain);

	return vpaned;
}

static GtkWidget *
setup_im_pane(PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	GtkWidget *frame;
	GtkWidget *imhtml_sw;
	GtkPolicyType imhtml_sw_hscroll;
	GtkWidget *paned;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GList *focus_chain = NULL;

	/* Setup the outer pane */
	paned = gtk_vpaned_new();
	gtk_widget_show(paned);

	/* Setup the top part of the pane */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack1(GTK_PANED(paned), vbox, TRUE, TRUE);
	gtk_widget_show(vbox);

	/* Setup the gtkimhtml widget */
	frame = pidgin_create_imhtml(FALSE, &gtkconv->imhtml, NULL, &imhtml_sw);
	gtk_widget_set_name(gtkconv->imhtml, "pidgin_conv_imhtml");
	gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);
	gtk_scrolled_window_get_policy(GTK_SCROLLED_WINDOW(imhtml_sw),
	                               &imhtml_sw_hscroll, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(imhtml_sw),
	                               imhtml_sw_hscroll, GTK_POLICY_ALWAYS);

	gtk_widget_set_size_request(gtkconv->imhtml,
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/default_width"),
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/default_height"));
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "size-allocate",
	                 G_CALLBACK(size_allocate_cb), gtkconv);

	g_signal_connect_after(G_OBJECT(gtkconv->imhtml), "button_press_event",
	                       G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "key_press_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "key_release_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);

	/* Setup the bottom half of the conversation window */
	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack2(GTK_PANED(paned), vbox2, FALSE, TRUE);
	gtk_widget_show(vbox2);

	gtkconv->lower_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), gtkconv->lower_hbox, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->lower_hbox);

	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_end(GTK_BOX(gtkconv->lower_hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	/* Setup the toolbar, entry widget and all signals */
	frame = pidgin_create_imhtml(TRUE, &gtkconv->entry, &gtkconv->toolbar, NULL);
	gtk_box_pack_start(GTK_BOX(vbox2), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	g_signal_connect(G_OBJECT(gtkconv->entry), "populate-popup",
					 G_CALLBACK(entry_popup_menu_cb), gtkconv);

	gtk_widget_set_name(gtkconv->entry, "pidgin_conv_entry");
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->entry),
								 purple_account_get_protocol_name(conv->account));
	gtk_widget_set_size_request(gtkconv->lower_hbox, -1,
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height"));
	gtkconv->entry_buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->entry));
	g_object_set_data(G_OBJECT(gtkconv->entry_buffer), "user_data", gtkconv);

	g_signal_connect(G_OBJECT(gtkconv->entry), "key_press_event",
	                 G_CALLBACK(entry_key_press_cb), gtkconv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "message_send",
	                       G_CALLBACK(send_cb), gtkconv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "button_press_event",
	                       G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->lower_hbox), "size-allocate",
	                 G_CALLBACK(size_allocate_cb), gtkconv);

	g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "insert_text",
	                 G_CALLBACK(insert_text_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "delete_range",
	                 G_CALLBACK(delete_text_cb), gtkconv);
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry_buffer), "changed",
				 G_CALLBACK(resize_imhtml_cb), gtkconv);

	/* had to move this after the imtoolbar is attached so that the
	 * signals get fired to toggle the buttons on the toolbar as well.
	 */
	default_formatize(gtkconv);

	g_signal_connect_after(G_OBJECT(gtkconv->entry), "format_function_clear",
						   G_CALLBACK(clear_formatting_cb), gtkconv);

	gtkconv->u.im->animate = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons");
	gtkconv->u.im->show_icon = TRUE;

	/*
	 * Focus for IM windows should be as follows:
	 * Tab title -> conversation scrollback -> entry
	 */
	focus_chain = g_list_prepend(focus_chain, gtkconv->entry);
	gtk_container_set_focus_chain(GTK_CONTAINER(vbox2), focus_chain);

	gtkconv->u.im->typing_timer = 0;
	return paned;
}

static void
conv_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
              GtkSelectionData *sd, guint info, guint t,
              PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginWindow *win = gtkconv->win;
	PurpleConversation *c;
	if (sd->target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE))
	{
		PurpleBlistNode *n = NULL;
		PurpleBuddy *b;
		PidginConversation *gtkconv = NULL;

		n = *(PurpleBlistNode **)sd->data;

		if (PURPLE_BLIST_NODE_IS_CONTACT(n))
			b = purple_contact_get_priority_buddy((PurpleContact*)n);
		else if (PURPLE_BLIST_NODE_IS_BUDDY(n))
			b = (PurpleBuddy*)n;
		else
			return;

		/*
		 * If we already have an open conversation with this buddy, then
		 * just move the conv to this window.  Otherwise, create a new
		 * conv and add it to this window.
		 */
		c = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, b->name, b->account);
		if (c != NULL) {
			PidginWindow *oldwin;
			gtkconv = PIDGIN_CONVERSATION(c);
			oldwin = gtkconv->win;
			if (oldwin != win) {
				pidgin_conv_window_remove_gtkconv(oldwin, gtkconv);
				pidgin_conv_window_add_gtkconv(win, gtkconv);
			}
		} else {
			c = purple_conversation_new(PURPLE_CONV_TYPE_IM, b->account, b->name);
			gtkconv = PIDGIN_CONVERSATION(c);
			if (gtkconv->win != win)
			{
				pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
				pidgin_conv_window_add_gtkconv(win, gtkconv);
			}
		}

		/* Make this conversation the active conversation */
		pidgin_conv_window_switch_gtkconv(win, gtkconv);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else if (sd->target == gdk_atom_intern("application/x-im-contact", FALSE))
	{
		char *protocol = NULL;
		char *username = NULL;
		PurpleAccount *account;
		PidginConversation *gtkconv;

		if (pidgin_parse_x_im_contact((const char *)sd->data, FALSE, &account,
						&protocol, &username, NULL))
		{
			if (account == NULL)
			{
				purple_notify_error(win, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL);
			}
			else
			{
				c = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);
				gtkconv = PIDGIN_CONVERSATION(c);
				if (gtkconv->win != win)
				{
					pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
					pidgin_conv_window_add_gtkconv(win, gtkconv);
				}
			}
		}

		g_free(username);
		g_free(protocol);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else if (sd->target == gdk_atom_intern("text/uri-list", FALSE)) {
		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
			pidgin_dnd_file_manage(sd, purple_conversation_get_account(conv), purple_conversation_get_name(conv));
		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else
		gtk_drag_finish(dc, FALSE, FALSE, t);
}


static const GtkTargetEntry te[] =
{
	GTK_IMHTML_DND_TARGETS,
	{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, GTK_IMHTML_DRAG_NUM},
	{"application/x-im-contact", 0, GTK_IMHTML_DRAG_NUM + 1}
};

static PidginConversation *
pidgin_conv_find_gtkconv(PurpleConversation * conv)
{
	PurpleBuddy *bud = purple_find_buddy(conv->account, conv->name), *b;
	PurpleContact *c;
	PurpleBlistNode *cn;

	if (!bud)
		return NULL;

	if (!(c = purple_buddy_get_contact(bud)))
		return NULL;

	cn = (PurpleBlistNode *)c;
	for (b = (PurpleBuddy *)cn->child; b; b = (PurpleBuddy *) ((PurpleBlistNode *)b)->next) {
		PurpleConversation *conv;
		if ((conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, b->name, b->account))) {
			if (conv->ui_data)
				return conv->ui_data;
		}
	}

	return NULL;
}

static void
buddy_update_cb(PurpleBlistNode *bnode, gpointer null)
{
	GList *list;

	g_return_if_fail(bnode);
	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(bnode));

	for (list = pidgin_conv_windows_get_list(); list; list = list->next)
	{
		PidginWindow *win = list->data;
		PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

		if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_IM)
			continue;

		pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}
}

/**************************************************************************
 * Conversation UI operations
 **************************************************************************/
static void
private_gtkconv_new(PurpleConversation *conv, gboolean hidden)
{
	PidginConversation *gtkconv;
	PurpleConversationType conv_type = purple_conversation_get_type(conv);
	GtkWidget *pane = NULL;
	GtkWidget *tab_cont;

	if (conv_type == PURPLE_CONV_TYPE_IM && (gtkconv = pidgin_conv_find_gtkconv(conv))) {
		conv->ui_data = gtkconv;
		if (!g_list_find(gtkconv->convs, conv))
			gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
		pidgin_conv_switch_active_conversation(conv);
		return;
	}

	gtkconv = g_new0(PidginConversation, 1);
	conv->ui_data = gtkconv;
	gtkconv->active_conv = conv;
	gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
	gtkconv->send_history = g_list_append(NULL, NULL);

	/* Setup some initial variables. */
	gtkconv->sg       = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
	gtkconv->tooltips = gtk_tooltips_new();
	gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	gtkconv->unseen_count = 0;

	if (conv_type == PURPLE_CONV_TYPE_IM) {
		gtkconv->u.im = g_malloc0(sizeof(PidginImPane));

		pane = setup_im_pane(gtkconv);
	} else if (conv_type == PURPLE_CONV_TYPE_CHAT) {
		gtkconv->u.chat = g_malloc0(sizeof(PidginChatPane));
		pane = setup_chat_pane(gtkconv);
	}

	gtk_imhtml_set_format_functions(GTK_IMHTML(gtkconv->imhtml),
			gtk_imhtml_get_format_functions(GTK_IMHTML(gtkconv->imhtml)) | GTK_IMHTML_IMAGE);

	if (pane == NULL) {
		if (conv_type == PURPLE_CONV_TYPE_CHAT)
			g_free(gtkconv->u.chat);
		else if (conv_type == PURPLE_CONV_TYPE_IM)
			g_free(gtkconv->u.im);

		g_free(gtkconv);
		conv->ui_data = NULL;
		return;
	}

	/* Setup drag-and-drop */
	gtk_drag_dest_set(pane,
	                  GTK_DEST_DEFAULT_MOTION |
	                  GTK_DEST_DEFAULT_DROP,
	                  te, sizeof(te) / sizeof(GtkTargetEntry),
	                  GDK_ACTION_COPY);
	gtk_drag_dest_set(pane,
	                  GTK_DEST_DEFAULT_MOTION |
	                  GTK_DEST_DEFAULT_DROP,
	                  te, sizeof(te) / sizeof(GtkTargetEntry),
	                  GDK_ACTION_COPY);
	gtk_drag_dest_set(gtkconv->imhtml, 0,
	                  te, sizeof(te) / sizeof(GtkTargetEntry),
	                  GDK_ACTION_COPY);

	gtk_drag_dest_set(gtkconv->entry, 0,
	                  te, sizeof(te) / sizeof(GtkTargetEntry),
	                  GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(pane), "drag_data_received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "drag_data_received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->entry), "drag_data_received",
	                 G_CALLBACK(conv_dnd_recv), gtkconv);

	/* Setup the container for the tab. */
	gtkconv->tab_cont = tab_cont = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	g_object_set_data(G_OBJECT(tab_cont), "PidginConversation", gtkconv);
	gtk_container_set_border_width(GTK_CONTAINER(tab_cont), PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(tab_cont), pane);
	gtk_widget_show(pane);

	gtkconv->make_sound = TRUE;

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar"))
		gtk_widget_show(gtkconv->toolbar);
	else
		gtk_widget_hide(gtkconv->toolbar);

	gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),
		purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_timestamps"));
	gtk_imhtml_set_protocol_name(GTK_IMHTML(gtkconv->imhtml),
								 purple_account_get_protocol_name(conv->account));

	g_signal_connect_swapped(G_OBJECT(pane), "focus",
	                         G_CALLBACK(gtk_widget_grab_focus),
	                         gtkconv->entry);

	if (hidden)
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
	else
		pidgin_conv_placement_place(gtkconv);

	if (nick_colors == NULL) {
		nbr_nick_colors = NUM_NICK_COLORS;
		nick_colors = generate_nick_colors(&nbr_nick_colors, gtk_widget_get_style(gtkconv->imhtml)->base[GTK_STATE_NORMAL]);
	}
}

static void
pidgin_conv_new_hidden(PurpleConversation *conv)
{
	private_gtkconv_new(conv, TRUE);
}

void
pidgin_conv_new(PurpleConversation *conv)
{
	private_gtkconv_new(conv, FALSE);
}

static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags)
{
	PurpleConversationUiOps *ui_ops = pidgin_conversations_get_conv_ui_ops();
	if (conv != NULL)
		return;

	/* create hidden conv if hide_new pref is always */
	if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always") == 0)
	{
		ui_ops->create_conversation = pidgin_conv_new_hidden;
		purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
		ui_ops->create_conversation = pidgin_conv_new;
		return;
	}

	/* create hidden conv if hide_new pref is away and account is away */
	if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away") == 0 &&
	    !purple_status_is_available(purple_account_get_active_status(account)))
	{
		ui_ops->create_conversation = pidgin_conv_new_hidden;
		purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
		ui_ops->create_conversation = pidgin_conv_new;
		return;
	}
}

static void
pidgin_conv_destroy(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->convs = g_list_remove(gtkconv->convs, conv);
	/* Don't destroy ourselves until all our convos are gone */
	if (gtkconv->convs)
		return;

	pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);

	/* If the "Save Conversation" or "Save Icon" dialogs are open then close them */
	purple_request_close_with_handle(gtkconv);
	purple_notify_close_with_handle(gtkconv);

	gtk_widget_destroy(gtkconv->tab_cont);
	g_object_unref(gtkconv->tab_cont);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		if (gtkconv->u.im->icon_timer != 0)
			g_source_remove(gtkconv->u.im->icon_timer);

		if (gtkconv->u.im->anim != NULL)
			g_object_unref(G_OBJECT(gtkconv->u.im->anim));

		if (gtkconv->u.im->typing_timer != 0)
			g_source_remove(gtkconv->u.im->typing_timer);

		g_free(gtkconv->u.im);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		purple_signals_disconnect_by_handle(gtkconv->u.chat);
		g_free(gtkconv->u.chat);
	}

	gtk_object_sink(GTK_OBJECT(gtkconv->tooltips));

	gtkconv->send_history = g_list_first(gtkconv->send_history);
	g_list_foreach(gtkconv->send_history, (GFunc)g_free, NULL);
	g_list_free(gtkconv->send_history);

	g_free(gtkconv);
}


static void
pidgin_conv_write_im(PurpleConversation *conv, const char *who,
					  const char *message, PurpleMessageFlags flags,
					  time_t mtime)
{
	PidginConversation *gtkconv;

	gtkconv = PIDGIN_CONVERSATION(conv);

	if (conv != gtkconv->active_conv &&
	    flags & PURPLE_MESSAGE_ACTIVE_ONLY)
	{
		/* Plugins that want these messages suppressed should be
		 * calling purple_conv_im_write(), so they get suppressed here,
		 * before being written to the log. */
		purple_debug_info("gtkconv",
		                "Suppressing message for an inactive conversation in pidgin_conv_write_im()\n");
		return;
	}

	purple_conversation_write(conv, who, message, flags, mtime);
}

/* The callback for an event on a link tag. */
static gboolean buddytag_event(GtkTextTag *tag, GObject *imhtml,
		GdkEvent *event, GtkTextIter *arg2, gpointer data) {
	if (event->type == GDK_BUTTON_PRESS
			|| event->type == GDK_2BUTTON_PRESS) {
		GdkEventButton *btn_event = (GdkEventButton*) event;
		PurpleConversation *conv = data;
		char *buddyname;

		/* strlen("BUDDY ") == 6 */
		g_return_val_if_fail((tag->name != NULL)
				&& (strlen(tag->name) > 6), FALSE);

		buddyname = (tag->name) + 6;

		if (btn_event->button == 2
				&& event->type == GDK_2BUTTON_PRESS) {
			chat_do_info(PIDGIN_CONVERSATION(conv), buddyname);

			return TRUE;
		} else if (btn_event->button == 3
				&& event->type == GDK_BUTTON_PRESS) {
			GtkTextIter start, end;

			/* we shouldn't display the popup
			 * if the user has selected something: */
			if (!gtk_text_buffer_get_selection_bounds(
						gtk_text_iter_get_buffer(arg2),
						&start, &end)) {
				GtkWidget *menu = NULL;
				PurpleConnection *gc =
					purple_conversation_get_gc(conv);


				menu = create_chat_menu(conv, buddyname, gc);
				gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
						NULL, GTK_WIDGET(imhtml),
						btn_event->button,
						btn_event->time);

				/* Don't propagate the event any further */
				return TRUE;
			}
		}
	}

	return FALSE;
}

static GtkTextTag *get_buddy_tag(PurpleConversation *conv, const char *who) {
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	GtkTextTag *buddytag;
	gchar *str;

	str = g_strdup_printf("BUDDY %s", who);

	buddytag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(
				GTK_IMHTML(gtkconv->imhtml)->text_buffer), str);

	if (buddytag == NULL) {
		buddytag = gtk_text_buffer_create_tag(
				GTK_IMHTML(gtkconv->imhtml)->text_buffer, str, NULL);

		g_signal_connect(G_OBJECT(buddytag), "event",
				G_CALLBACK(buddytag_event), conv);
	}

	g_free(str);

	return buddytag;
}

static void pidgin_conv_calculate_newday(PidginConversation *gtkconv, time_t mtime)
{
	struct tm *tm = localtime(&mtime);

	tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
	tm->tm_mday++;

	gtkconv->newday = mktime(tm);
}

/* Detect string direction and encapsulate the string in RLE/LRE/PDF unicode characters 
   str - pointer to string (string is re-allocated and the pointer updated) */
static void
str_embed_direction_chars(char **str)
{
#ifdef HAVE_PANGO14
	char pre_str[4];
	char post_str[10];
	char *ret;

	if (PANGO_DIRECTION_RTL == pango_find_base_dir(*str, -1))
	{
		sprintf(pre_str, "%c%c%c",
				0xE2, 0x80, 0xAB);	/* RLE */
		sprintf(post_str, "%c%c%c%c%c%c%c%c%c",
				0xE2, 0x80, 0xAC,	/* PDF */
				0xE2, 0x80, 0x8E,	/* LRM */
				0xE2, 0x80, 0xAC);	/* PDF */
	}
	else
	{
		sprintf(pre_str, "%c%c%c",
				0xE2, 0x80, 0xAA);	/* LRE */
		sprintf(post_str, "%c%c%c%c%c%c%c%c%c",
				0xE2, 0x80, 0xAC,	/* PDF */
				0xE2, 0x80, 0x8F,	/* RLM */
				0xE2, 0x80, 0xAC);	/* PDF */
	}

	ret = g_strconcat(pre_str, *str, post_str, NULL);

	g_free(*str);
	*str = ret;
#endif
}

/* Returns true if the given HTML contains RTL text */
static gboolean 
html_is_rtl(const char *html)
{
	GData *attributes;
	const gchar *start, *end;
	gboolean res = FALSE;

	if (purple_markup_find_tag("span", html, &start, &end, &attributes)) 
	{
		/* tmp is a member of attributes and is free with g_datalist_clear call */
		const char *tmp = g_datalist_get_data(&attributes, "dir");
		if (tmp && !g_ascii_strcasecmp(tmp, "RTL"))
			res = TRUE;
		if (!res)
		{
			tmp = g_datalist_get_data(&attributes, "style");
			if (tmp)
			{
				char *tmp2 = purple_markup_get_css_property(tmp, "direction");
				if (tmp2 && !g_ascii_strcasecmp(tmp2, "RTL"))
					res = TRUE;
				g_free(tmp2);
			}

		}
		g_datalist_clear(&attributes);
	}
	return res;
}


static void
pidgin_conv_write_conv(PurpleConversation *conv, const char *name, const char *alias,
						const char *message, PurpleMessageFlags flags,
						time_t mtime)
{
	PidginConversation *gtkconv;
	PidginWindow *win;
	PurpleConnection *gc;
	PurpleAccount *account;
	PurplePluginProtocolInfo *prpl_info;
	int gtk_font_options = 0;
	int gtk_font_options_all = 0;
	int max_scrollback_lines;
	int line_count;
	char buf2[BUF_LONG];
	gboolean show_date;
	char *mdate;
	char color[10];
	char *str;
	char *with_font_tag;
	char *sml_attrib = NULL;
	size_t length;
	PurpleConversationType type;
	char *displaying;
	gboolean plugin_return;
	char *bracket;
	int tag_count = 0;
	gboolean is_rtl_message = FALSE;

	g_return_if_fail(conv != NULL);
	gtkconv = PIDGIN_CONVERSATION(conv);
	g_return_if_fail(gtkconv != NULL);

	if (conv != gtkconv->active_conv)
	{
		if (flags & PURPLE_MESSAGE_ACTIVE_ONLY)
		{
			/* Unless this had PURPLE_MESSAGE_NO_LOG, this message
			 * was logged.  Plugin writers: if this isn't what
			 * you wanted, call purple_conv_im_write() instead of
			 * purple_conversation_write(). */
			purple_debug_info("gtkconv",
			                "Suppressing message for an inactive conversation in pidgin_conv_write_conv()\n");
			return;
		}

		/* Set the active conversation to the one that just messaged us. */
		/* TODO: consider not doing this if the account is offline or something */
		if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV))
			pidgin_conv_switch_active_conversation(conv);
	}

	type = purple_conversation_get_type(conv);
	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);
	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	/* Make sure URLs are clickable */
	displaying = purple_markup_linkify(message);
	plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
							pidgin_conversations_get_handle(), (type == PURPLE_CONV_TYPE_IM ?
							"displaying-im-msg" : "displaying-chat-msg"),
							account, name, &displaying, conv, flags));
	if (plugin_return)
	{
		g_free(displaying);
		return;
	}
	length = strlen(displaying) + 1;

	/* Awful hack to work around GtkIMHtml's inefficient rendering of messages with lots of formatting changes.
	 * If a message has over 100 '<' characters, strip formatting before appending it. Hopefully nobody actually
	 * needs that much formatting, anyway.
	 */
	for (bracket = strchr(displaying, '<'); bracket && *(bracket + 1); bracket = strchr(bracket + 1, '<'))
		tag_count++;

	if (tag_count > 100) {
		char *tmp = displaying;
		displaying = purple_markup_strip_html(tmp);
		g_free(tmp);
	}

	win = gtkconv->win;
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	line_count = gtk_text_buffer_get_line_count(
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(
				gtkconv->imhtml)));

	max_scrollback_lines = purple_prefs_get_int(
		PIDGIN_PREFS_ROOT "/conversations/scrollback_lines");
	/* If we're sitting at more than 100 lines more than the
	   max scrollback, trim down to max scrollback */
	if (max_scrollback_lines > 0
			&& line_count > (max_scrollback_lines + 100)) {
		GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(gtkconv->imhtml));
		GtkTextIter start, end;

		gtk_text_buffer_get_start_iter(text_buffer, &start);
		gtk_text_buffer_get_iter_at_line(text_buffer, &end,
			(line_count - max_scrollback_lines));
		gtk_imhtml_delete(GTK_IMHTML(gtkconv->imhtml), &start, &end);
	}

	if (type == PURPLE_CONV_TYPE_CHAT)
	{
		/* Create anchor for user */
		GtkTextIter iter;
		char *tmp = g_strconcat("user:", name, NULL);

		gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml)), &iter);
		gtk_text_buffer_create_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml)),
								tmp, &iter, TRUE);
		g_free(tmp);
	}

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling"))
		gtk_font_options_all |= GTK_IMHTML_USE_SMOOTHSCROLLING;

	if (gtk_text_buffer_get_char_count(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml))))
		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<BR>", gtk_font_options_all);

	/* First message in a conversation. */
	if (gtkconv->newday == 0)
		pidgin_conv_calculate_newday(gtkconv, mtime);

	/* Show the date on the first message in a new day, or if the message is
	 * older than 20 minutes. */
	show_date = (mtime >= gtkconv->newday) || (time(NULL) > mtime + 20*60);

	mdate = purple_signal_emit_return_1(pidgin_conversations_get_handle(),
	                                  "conversation-timestamp",
	                                  conv, mtime, show_date);

	if (mdate == NULL)
	{
		struct tm *tm = localtime(&mtime);
		const char *tmp;
		if (show_date)
			tmp = purple_date_format_long(tm);
		else
			tmp = purple_time_format(tm);
		mdate = g_strdup_printf("(%s)", tmp);
	}

	/* Bi-Directional support - set timestamp direction using unicode characters */
	is_rtl_message = html_is_rtl(message);
	/* Enforce direction only if message is RTL - doesn't effect LTR users */
	if (is_rtl_message)
		str_embed_direction_chars(&mdate);

	if (mtime >= gtkconv->newday)
		pidgin_conv_calculate_newday(gtkconv, mtime);

	sml_attrib = g_strdup_printf("sml=\"%s\"", purple_account_get_protocol_name(account));

	gtk_font_options |= GTK_IMHTML_NO_COMMENTS;

	if ((flags & PURPLE_MESSAGE_RECV) &&
			!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting"))
		gtk_font_options |= GTK_IMHTML_NO_COLOURS | GTK_IMHTML_NO_FONTS | GTK_IMHTML_NO_SIZES | GTK_IMHTML_NO_FORMATTING;

	/* this is gonna crash one day, I can feel it. */
	if (PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(conv->account)))->options &
	    OPT_PROTO_USE_POINTSIZE) {
		gtk_font_options |= GTK_IMHTML_USE_POINTSIZE;
	}


	/* TODO: These colors should not be hardcoded so log.c can use them */
	if (flags & PURPLE_MESSAGE_RAW) {
		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), message, gtk_font_options_all);
	} else if (flags & PURPLE_MESSAGE_SYSTEM) {
		g_snprintf(buf2, sizeof(buf2),
			   "<FONT %s><FONT SIZE=\"2\"><!--%s --></FONT><B>%s</B></FONT>",
			   sml_attrib ? sml_attrib : "", mdate, displaying);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, gtk_font_options_all);

	} else if (flags & PURPLE_MESSAGE_ERROR) {
		g_snprintf(buf2, sizeof(buf2),
			   "<FONT COLOR=\"#ff0000\"><FONT %s><FONT SIZE=\"2\"><!--%s --></FONT><B>%s</B></FONT></FONT>",
			   sml_attrib ? sml_attrib : "", mdate, displaying);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, gtk_font_options_all);

	} else if (flags & PURPLE_MESSAGE_NO_LOG) {
		g_snprintf(buf2, BUF_LONG,
			   "<B><FONT %s COLOR=\"#777777\">%s</FONT></B>",
			   sml_attrib ? sml_attrib : "", displaying);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, gtk_font_options_all);
	} else {
		char *new_message = g_memdup(displaying, length);
		char *alias_escaped = (alias ? g_markup_escape_text(alias, strlen(alias)) : g_strdup(""));
		/* The initial offset is to deal with
		 * escaped entities making the string longer */
		int tag_start_offset = alias ? (strlen(alias_escaped) - strlen(alias)) : 0;
		int tag_end_offset = 0;
		GtkSmileyTree *tree = NULL;
		GHashTable *smiley_data = NULL;

		/* Enforce direction on alias */
		if (is_rtl_message)
			str_embed_direction_chars(&alias_escaped);

		if (flags & PURPLE_MESSAGE_SEND)
		{
			/* Temporarily revert to the original smiley-data to avoid showing up
			 * custom smileys of the buddy when sending message
			 */
			tree = GTK_IMHTML(gtkconv->imhtml)->default_smilies;
			GTK_IMHTML(gtkconv->imhtml)->default_smilies =
									GTK_IMHTML(gtkconv->entry)->default_smilies;
			smiley_data = GTK_IMHTML(gtkconv->imhtml)->smiley_data;
			GTK_IMHTML(gtkconv->imhtml)->smiley_data = GTK_IMHTML(gtkconv->entry)->smiley_data;
		}

		if (flags & PURPLE_MESSAGE_WHISPER) {
			str = g_malloc(1024);

			/* If we're whispering, it's not an autoresponse. */
			if (purple_message_meify(new_message, -1 )) {
				g_snprintf(str, 1024, "***%s", alias_escaped);
				strcpy(color, "#6C2585");
				tag_start_offset += 3;
			}
			else {
				g_snprintf(str, 1024, "*%s*:", alias_escaped);
				tag_start_offset += 1;
				tag_end_offset = 2;
				strcpy(color, "#00FF00");
			}
		}
		else {
			if (purple_message_meify(new_message, -1)) {
				str = g_malloc(1024);

				if (flags & PURPLE_MESSAGE_AUTO_RESP) {
					g_snprintf(str, 1024, "%s ***%s", AUTO_RESPONSE, alias_escaped);
					tag_start_offset += 4
						+ strlen(AUTO_RESPONSE);
				} else {
					g_snprintf(str, 1024, "***%s", alias_escaped);
					tag_start_offset += 3;
				}

				if (flags & PURPLE_MESSAGE_NICK)
					strcpy(color, HIGHLIGHT_COLOR);
				else
					strcpy(color, "#062585");
			}
			else {
				str = g_malloc(1024);
				if (flags & PURPLE_MESSAGE_AUTO_RESP) {
					g_snprintf(str, 1024, "%s %s", alias_escaped, AUTO_RESPONSE);
					tag_start_offset += 1
						+ strlen(AUTO_RESPONSE);
				} else {
					g_snprintf(str, 1024, "%s:", alias_escaped);
					tag_end_offset = 1;
				}
				if (flags & PURPLE_MESSAGE_NICK)
					strcpy(color, HIGHLIGHT_COLOR);
				else if (flags & PURPLE_MESSAGE_RECV) {
					if (type == PURPLE_CONV_TYPE_CHAT) {
						GdkColor *col = get_nick_color(gtkconv, name);

						g_snprintf(color, sizeof(color), "#%02X%02X%02X",
							   col->red >> 8, col->green >> 8, col->blue >> 8);
					} else
						strcpy(color, RECV_COLOR);
				}
				else if (flags & PURPLE_MESSAGE_SEND)
					strcpy(color, SEND_COLOR);
				else {
					purple_debug_error("gtkconv", "message missing flags\n");
					strcpy(color, "#000000");
				}
			}
		}

		g_free(alias_escaped);

		/* Are we in a chat where we can tell which users are buddies? */
		if  (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME) &&
		     purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {

			/* Bold buddies to make them stand out from non-buddies. */
			if (flags & PURPLE_MESSAGE_SEND ||
			    flags & PURPLE_MESSAGE_NICK ||
			    purple_find_buddy(account, name) != NULL) {
				g_snprintf(buf2, BUF_LONG,
					   "<FONT COLOR=\"%s\" %s><FONT SIZE=\"2\"><!--%s --></FONT>"
					   "<B>%s</B></FONT> ",
					   color, sml_attrib ? sml_attrib : "", mdate, str);
			} else {
				g_snprintf(buf2, BUF_LONG,
					   "<FONT COLOR=\"%s\" %s><FONT SIZE=\"2\"><!--%s --></FONT>"
					   "%s</FONT> ",
					   color, sml_attrib ? sml_attrib : "", mdate, str);

			}
		} else {
			/* Bold everyone's name to make the name stand out from the message. */
			g_snprintf(buf2, BUF_LONG,
				   "<FONT COLOR=\"%s\" %s><FONT SIZE=\"2\"><!--%s --></FONT>"
				   "<B>%s</B></FONT> ",
				   color, sml_attrib ? sml_attrib : "", mdate, str);
		}

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, gtk_font_options_all);

		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT &&
		    !(flags & PURPLE_MESSAGE_SEND)) {

			GtkTextIter start, end;
			GtkTextTag *buddytag = get_buddy_tag(conv, name);

			gtk_text_buffer_get_end_iter(
					GTK_IMHTML(gtkconv->imhtml)->text_buffer,
					&end);
			gtk_text_iter_backward_chars(&end,
					tag_end_offset + 1);

			gtk_text_buffer_get_end_iter(
					GTK_IMHTML(gtkconv->imhtml)->text_buffer,
					&start);
			gtk_text_iter_backward_chars(&start,
					strlen(str) + 1 - tag_start_offset);

			gtk_text_buffer_apply_tag(
					GTK_IMHTML(gtkconv->imhtml)->text_buffer,
					buddytag, &start, &end);
		}

		g_free(str);

		if(gc){
			char *pre = g_strdup_printf("<font %s>", sml_attrib ? sml_attrib : "");
			char *post = "</font>";
			int pre_len = strlen(pre);
			int post_len = strlen(post);

			with_font_tag = g_malloc(length + pre_len + post_len + 1);

			strcpy(with_font_tag, pre);
			memcpy(with_font_tag + pre_len, new_message, length);
			strcpy(with_font_tag + pre_len + length, post);

			length += pre_len + post_len;
			g_free(pre);
		}
		else
			with_font_tag = g_memdup(new_message, length);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml),
							 with_font_tag, gtk_font_options | gtk_font_options_all);

		if (flags & PURPLE_MESSAGE_SEND)
		{
			/* Restore the smiley-data */
			GTK_IMHTML(gtkconv->imhtml)->default_smilies = tree;
			GTK_IMHTML(gtkconv->imhtml)->smiley_data = smiley_data;
		}

		g_free(with_font_tag);
		g_free(new_message);
	}

	g_free(mdate);
	g_free(sml_attrib);

	/* Tab highlighting stuff */
	if (!(flags & PURPLE_MESSAGE_SEND) && !pidgin_conv_has_focus(conv))
	{
		PidginUnseenState unseen = PIDGIN_UNSEEN_NONE;

		if ((flags & PURPLE_MESSAGE_NICK) == PURPLE_MESSAGE_NICK)
			unseen = PIDGIN_UNSEEN_NICK;
		else if (((flags & PURPLE_MESSAGE_SYSTEM) == PURPLE_MESSAGE_SYSTEM) ||
			  ((flags & PURPLE_MESSAGE_ERROR) == PURPLE_MESSAGE_ERROR))
			unseen = PIDGIN_UNSEEN_EVENT;
		else if ((flags & PURPLE_MESSAGE_NO_LOG) == PURPLE_MESSAGE_NO_LOG)
			unseen = PIDGIN_UNSEEN_NO_LOG;
		else
			unseen = PIDGIN_UNSEEN_TEXT;

		gtkconv_set_unseen(gtkconv, unseen);
	}

	purple_signal_emit(pidgin_conversations_get_handle(),
		(type == PURPLE_CONV_TYPE_IM ? "displayed-im-msg" : "displayed-chat-msg"),
		account, name, displaying, conv, flags);
	g_free(displaying);
}
static void
pidgin_conv_chat_add_users(PurpleConversation *conv, GList *cbuddies, gboolean new_arrivals)
{
	PurpleConvChat *chat;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkListStore *ls;
	GList *l;

	char tmp[BUF_LONG];
	int num_users;

	chat    = PURPLE_CONV_CHAT(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(purple_conv_chat_get_users(chat));

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users),
			   num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);

	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)));

#if GTK_CHECK_VERSION(2,6,0)
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
										 GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);
#endif

	l = cbuddies;
	while (l != NULL) {
		add_chat_buddy_common(conv, (PurpleConvChatBuddy *)l->data, NULL);
		l = l->next;
	}

	/* Currently GTK+ maintains our sorted list after it's in the tree.
	 * This may change if it turns out we can manage it faster ourselves.
 	 */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  CHAT_USERS_ALIAS_KEY_COLUMN,
										 GTK_SORT_ASCENDING);
}

static void
pidgin_conv_chat_rename_user(PurpleConversation *conv, const char *old_name,
			      const char *new_name, const char *new_alias)
{
	PurpleConvChat *chat;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	PurpleConvChatBuddyFlags flags;
	PurpleConvChatBuddy *cbuddy;
	GtkTreeIter iter;
	GtkTreeModel *model;
	int f = 1;

	chat    = PURPLE_CONV_CHAT(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	while (f != 0) {
		char *val;

		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &val, CHAT_USERS_FLAGS_COLUMN, &flags, -1);

		if (!purple_utf8_strcasecmp(old_name, val)) {
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			g_free(val);
			break;
		}

		f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

		g_free(val);
	}

	if (!purple_conv_chat_find_user(chat, old_name))
		return;

	g_return_if_fail(new_alias != NULL);

	cbuddy = purple_conv_chat_cb_new(new_name, new_alias, flags);

	add_chat_buddy_common(conv, cbuddy, old_name);
}

static void
pidgin_conv_chat_remove_users(PurpleConversation *conv, GList *users)
{
	PurpleConvChat *chat;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *l;
	char tmp[BUF_LONG];
	int num_users;
	gboolean f;

	chat    = PURPLE_CONV_CHAT(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(purple_conv_chat_get_users(chat));

	for (l = users; l != NULL; l = l->next) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

		if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
			continue;

		do {
			char *val;

			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
							   CHAT_USERS_NAME_COLUMN, &val, -1);

			if (!purple_utf8_strcasecmp((char *)l->data, val)) {
#if GTK_CHECK_VERSION(2,2,0)
				f = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
#else
				gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);
#endif
			}
			else
				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

			g_free(val);
		} while (f);
	}

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users), num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);
}

static void
pidgin_conv_chat_update_user(PurpleConversation *conv, const char *user)
{
	PurpleConvChat *chat;
	PurpleConvChatBuddyFlags flags;
	PurpleConvChatBuddy *cbuddy;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	int f = 1;
	char *alias = NULL;

	chat    = PURPLE_CONV_CHAT(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	while (f != 0) {
		char *val;

		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &val, -1);

		if (!purple_utf8_strcasecmp(user, val)) {
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_ALIAS_COLUMN, &alias, -1);
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			g_free(val);
			break;
		}

		f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

		g_free(val);
	}

	if (!purple_conv_chat_find_user(chat, user))
	{
		g_free(alias);
		return;
	}

	g_return_if_fail(alias != NULL);

	flags = purple_conv_chat_user_get_flags(chat, user);

	cbuddy = purple_conv_chat_cb_new(user, alias, flags);

	add_chat_buddy_common(conv, cbuddy, NULL);
	g_free(alias);
}

gboolean
pidgin_conv_has_focus(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	PidginWindow *win;
	gboolean has_focus;

	win = gtkconv->win;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	if (has_focus && pidgin_conv_window_is_active_conversation(conv))
		return TRUE;

	return FALSE;
}

static void pidgin_conv_custom_smiley_allocated(GdkPixbufLoader *loader, gpointer user_data)
{
	GtkIMHtmlSmiley *smiley;

	smiley = (GtkIMHtmlSmiley *)user_data;
	smiley->icon = gdk_pixbuf_loader_get_animation(loader);

	if (smiley->icon)
		g_object_ref(G_OBJECT(smiley->icon));
#ifdef DEBUG_CUSTOM_SMILEY
	purple_debug_info("custom-smiley", "pidgin_conv_custom_smiley_allocated(): got GdkPixbufAnimation %p for smiley '%s'\n", smiley->icon, smiley->smile);
#endif
}

static void pidgin_conv_custom_smiley_closed(GdkPixbufLoader *loader, gpointer user_data)
{
	GtkIMHtmlSmiley *smiley;
	GtkWidget *icon = NULL;
	GtkTextChildAnchor *anchor = NULL;
	GSList *current = NULL;

	smiley = (GtkIMHtmlSmiley *)user_data;
	if (!smiley->imhtml) {
#ifdef DEBUG_CUSTOM_SMILEY
		purple_debug_error("custom-smiley", "pidgin_conv_custom_smiley_closed(): orphan smiley found: %p\n", smiley);
#endif
		g_object_unref(G_OBJECT(loader));
		smiley->loader = NULL;
		return;
	}

	for (current = smiley->anchors; current; current = g_slist_next(current)) {

		icon = gtk_image_new_from_animation(smiley->icon);

#ifdef DEBUG_CUSTOM_SMILEY
		purple_debug_info("custom-smiley", "pidgin_conv_custom_smiley_closed(): got GtkImage %p from GtkPixbufAnimation %p for smiley '%s'\n",
				icon, smiley->icon, smiley->smile);
#endif
		if (icon) {
			gtk_widget_show(icon);

			anchor = GTK_TEXT_CHILD_ANCHOR(current->data);

			g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_plaintext", purple_unescape_html(smiley->smile), g_free);
			g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_htmltext", g_strdup(smiley->smile), g_free);

			if (smiley->imhtml)
				gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(smiley->imhtml), icon, anchor);
		}

	}

	g_slist_free(smiley->anchors);
	smiley->anchors = NULL;

	g_object_unref(G_OBJECT(loader));
	smiley->loader = NULL;
}

static gboolean
add_custom_smiley_for_imhtml(GtkIMHtml *imhtml, const char *sml, const char *smile)
{
	GtkIMHtmlSmiley *smiley;
	GdkPixbufLoader *loader;

	smiley = gtk_imhtml_smiley_get(imhtml, sml, smile);

	if (smiley) {

		if (!(smiley->flags & GTK_IMHTML_SMILEY_CUSTOM)) {
			return FALSE;
		}

		/* Close the old GdkPixbufAnimation, then create a new one for
		 * the smiley we are about to receive */
		g_object_unref(G_OBJECT(smiley->icon));

		/* XXX: Is it necessary to _unref the loader first? */
		smiley->loader = gdk_pixbuf_loader_new();
		smiley->icon = NULL;

		g_signal_connect(smiley->loader, "area_prepared", G_CALLBACK(pidgin_conv_custom_smiley_allocated), smiley);
		g_signal_connect(smiley->loader, "closed", G_CALLBACK(pidgin_conv_custom_smiley_closed), smiley);

		return TRUE;
	}

	loader = gdk_pixbuf_loader_new();

	/* this is wrong, this file ought not call g_new on GtkIMHtmlSmiley */
	/* Let gtk_imhtml have a gtk_imhtml_smiley_new function, and let
	   GtkIMHtmlSmiley by opaque */
	smiley = g_new0(GtkIMHtmlSmiley, 1);
	smiley->file   = NULL;
	smiley->smile  = g_strdup(smile);
	smiley->loader = loader;
	smiley->flags  = smiley->flags | GTK_IMHTML_SMILEY_CUSTOM;

	g_signal_connect(smiley->loader, "area_prepared", G_CALLBACK(pidgin_conv_custom_smiley_allocated), smiley);
	g_signal_connect(smiley->loader, "closed", G_CALLBACK(pidgin_conv_custom_smiley_closed), smiley);

	gtk_imhtml_associate_smiley(imhtml, sml, smiley);

	return TRUE;
}

static gboolean
pidgin_conv_custom_smiley_add(PurpleConversation *conv, const char *smile, gboolean remote)
{
	PidginConversation *gtkconv;
	struct smiley_list *list;
	const char *sml = NULL, *conv_sml;

	if (!conv || !smile || !*smile) {
		return FALSE;
	}

	/* If smileys are off, return false */
	if (pidgin_themes_smileys_disabled())
		return FALSE;

	/* If possible add this smiley to the current theme.
	 * The addition is only temporary: custom smilies aren't saved to disk. */
	conv_sml = purple_account_get_protocol_name(conv->account);
	gtkconv = PIDGIN_CONVERSATION(conv);

	for (list = (struct smiley_list *)current_smiley_theme->list; list; list = list->next) {
		if (!strcmp(list->sml, conv_sml)) {
			sml = list->sml;
			break;
		}
	}

	if (!add_custom_smiley_for_imhtml(GTK_IMHTML(gtkconv->imhtml), sml, smile))
		return FALSE;

	if (!remote)	/* If it's a local custom smiley, then add it for the entry */
		if (!add_custom_smiley_for_imhtml(GTK_IMHTML(gtkconv->entry), sml, smile))
			return FALSE;

	return TRUE;
}

static void
pidgin_conv_custom_smiley_write(PurpleConversation *conv, const char *smile,
                                      const guchar *data, gsize size)
{
	PidginConversation *gtkconv;
	GtkIMHtmlSmiley *smiley;
	GdkPixbufLoader *loader;
	const char *sml;

	sml = purple_account_get_protocol_name(conv->account);
	gtkconv = PIDGIN_CONVERSATION(conv);
	smiley = gtk_imhtml_smiley_get(GTK_IMHTML(gtkconv->imhtml), sml, smile);

	if (!smiley)
		return;

	loader = smiley->loader;
	if (!loader)
		return;

	gdk_pixbuf_loader_write(loader, data, size, NULL);
}

static void
pidgin_conv_custom_smiley_close(PurpleConversation *conv, const char *smile)
{
	PidginConversation *gtkconv;
	GtkIMHtmlSmiley *smiley;
	GdkPixbufLoader *loader;
	const char *sml;

	g_return_if_fail(conv  != NULL);
	g_return_if_fail(smile != NULL);

	sml = purple_account_get_protocol_name(conv->account);
	gtkconv = PIDGIN_CONVERSATION(conv);
	smiley = gtk_imhtml_smiley_get(GTK_IMHTML(gtkconv->imhtml), sml, smile);

	if (!smiley)
		return;

	loader = smiley->loader;

	if (!loader)
		return;



	purple_debug_info("gtkconv", "About to close the smiley pixbuf\n");

	gdk_pixbuf_loader_close(loader, NULL);

}

static void
pidgin_conv_send_confirm(PurpleConversation *conv, const char *message)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->entry), message, 0);
}

/*
 * Makes sure all the menu items and all the buttons are hidden/shown and
 * sensitive/insensitive.  This is called after changing tabs and when an
 * account signs on or off.
 */
static void
gray_stuff_out(PidginConversation *gtkconv)
{
	PidginWindow *win;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;
	GdkPixbuf *window_icon = NULL;
	GtkIMHtmlButtons buttons;
	PurpleAccount *account;

	win     = pidgin_conv_get_window(gtkconv);
	gc      = purple_conversation_get_gc(conv);
	account = purple_conversation_get_account(conv);

	if (gc != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (win->menu.send_to != NULL)
		update_send_to_selection(win);

	/*
	 * Handle hiding and showing stuff based on what type of conv this is.
	 * Stuff that Purple IMs support in general should be shown for IM
	 * conversations.  Stuff that Purple chats support in general should be
	 * shown for chat conversations.  It doesn't matter whether the PRPL
	 * supports it or not--that only affects if the button or menu item
	 * is sensitive or not.
	 */
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		/* Show stuff that applies to IMs, hide stuff that applies to chats */

		/* Deal with menu items */
		gtk_widget_show(win->menu.view_log);
		gtk_widget_show(win->menu.send_file);
		gtk_widget_show(win->menu.add_pounce);
		gtk_widget_show(win->menu.get_info);
		gtk_widget_hide(win->menu.invite);
		gtk_widget_show(win->menu.alias);
 		if (purple_privacy_check(account, purple_conversation_get_name(conv))) {
 			gtk_widget_hide(win->menu.unblock);
 			gtk_widget_show(win->menu.block);
 		} else {
 			gtk_widget_hide(win->menu.block);
 			gtk_widget_show(win->menu.unblock);
 		}

		if ((account == NULL) || purple_find_buddy(account, purple_conversation_get_name(conv)) == NULL) {
			gtk_widget_show(win->menu.add);
			gtk_widget_hide(win->menu.remove);
		} else {
			gtk_widget_show(win->menu.remove);
			gtk_widget_hide(win->menu.add);
		}

		gtk_widget_show(win->menu.show_icon);
	} else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
		/* Show stuff that applies to Chats, hide stuff that applies to IMs */

		/* Deal with menu items */
		gtk_widget_show(win->menu.view_log);
		gtk_widget_hide(win->menu.send_file);
		gtk_widget_hide(win->menu.add_pounce);
		gtk_widget_hide(win->menu.get_info);
		gtk_widget_show(win->menu.invite);
		gtk_widget_show(win->menu.alias);
		gtk_widget_hide(win->menu.block);
		gtk_widget_hide(win->menu.unblock);
		gtk_widget_hide(win->menu.show_icon);

		if ((account == NULL) || purple_blist_find_chat(account, purple_conversation_get_name(conv)) == NULL) {
			/* If the chat is NOT in the buddy list */
			gtk_widget_show(win->menu.add);
			gtk_widget_hide(win->menu.remove);
		} else {
			/* If the chat IS in the buddy list */
			gtk_widget_hide(win->menu.add);
			gtk_widget_show(win->menu.remove);
		}

	}

	/*
	 * Handle graying stuff out based on whether an account is connected
	 * and what features that account supports.
	 */
	if ((gc != NULL) &&
		((purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_CHAT) ||
		 !purple_conv_chat_has_left(PURPLE_CONV_CHAT(conv)) ))
	{
		/* Account is online */
		/* Deal with the toolbar */
		if (conv->features & PURPLE_CONNECTION_HTML)
		{
			buttons = GTK_IMHTML_ALL; /* Everything on */
			if (conv->features & PURPLE_CONNECTION_NO_BGCOLOR)
				buttons &= ~GTK_IMHTML_BACKCOLOR;
			if (conv->features & PURPLE_CONNECTION_NO_FONTSIZE)
			{
				buttons &= ~GTK_IMHTML_GROW;
				buttons &= ~GTK_IMHTML_SHRINK;
			}
			if (conv->features & PURPLE_CONNECTION_NO_URLDESC)
				buttons &= ~GTK_IMHTML_LINKDESC;
		} else {
			buttons = GTK_IMHTML_SMILEY | GTK_IMHTML_IMAGE;
		}

		if (!(prpl_info->options & OPT_PROTO_IM_IMAGE) ||
				conv->features & PURPLE_CONNECTION_NO_IMAGES)
			buttons &= ~GTK_IMHTML_IMAGE;

		gtk_imhtml_set_format_functions(GTK_IMHTML(gtkconv->entry), buttons);
		if (account != NULL)
			gtk_imhtmltoolbar_associate_smileys(GTK_IMHTMLTOOLBAR(gtkconv->toolbar), purple_account_get_protocol_id(account));

		/* Deal with menu items */
		gtk_widget_set_sensitive(win->menu.view_log, TRUE);
		gtk_widget_set_sensitive(win->menu.add_pounce, TRUE);
		gtk_widget_set_sensitive(win->menu.get_info, (prpl_info->get_info != NULL));
		gtk_widget_set_sensitive(win->menu.invite, (prpl_info->chat_invite != NULL));

		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		{
			gtk_widget_set_sensitive(win->menu.add, (prpl_info->add_buddy != NULL));
			gtk_widget_set_sensitive(win->menu.remove, (prpl_info->remove_buddy != NULL));
			gtk_widget_set_sensitive(win->menu.send_file,
									 (prpl_info->send_file != NULL && (!prpl_info->can_receive_file ||
									  prpl_info->can_receive_file(gc, purple_conversation_get_name(conv)))));
			gtk_widget_set_sensitive(win->menu.alias,
									 (account != NULL) &&
									 (purple_find_buddy(account, purple_conversation_get_name(conv)) != NULL));
		}
		else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		{
			gtk_widget_set_sensitive(win->menu.add, (prpl_info->join_chat != NULL));
			gtk_widget_set_sensitive(win->menu.remove, (prpl_info->join_chat != NULL));
			gtk_widget_set_sensitive(win->menu.alias,
									 (account != NULL) &&
									 (purple_blist_find_chat(account, purple_conversation_get_name(conv)) != NULL));
		}

	} else {
		/* Account is offline */
		/* Or it's a chat that we've left. */

		/* Then deal with menu items */
		gtk_widget_set_sensitive(win->menu.view_log, TRUE);
		gtk_widget_set_sensitive(win->menu.send_file, FALSE);
		gtk_widget_set_sensitive(win->menu.add_pounce, TRUE);
		gtk_widget_set_sensitive(win->menu.get_info, FALSE);
		gtk_widget_set_sensitive(win->menu.invite, FALSE);
		gtk_widget_set_sensitive(win->menu.alias, FALSE);
		gtk_widget_set_sensitive(win->menu.add, FALSE);
		gtk_widget_set_sensitive(win->menu.remove, FALSE);
	}

	/*
	 * Update the window's icon
	 */
	if (pidgin_conv_window_is_active_conversation(conv))
	{
		GList *l = NULL;
		if ((purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) &&
				(gtkconv->u.im->anim))
		{
			window_icon =
				gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
			g_object_ref(window_icon);
			l = g_list_append(l, window_icon);
		} else {
			l = pidgin_conv_get_tab_icons(conv);
		}
		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
		if (window_icon != NULL) {
			g_object_unref(G_OBJECT(window_icon));
			g_list_free(l);
		}
	}
}

static void
pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields)
{
	PidginConversation *gtkconv;
	PidginWindow *win;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return;
	win = pidgin_conv_get_window(gtkconv);
	if (!win)
		return;

	if (fields & PIDGIN_CONV_SET_TITLE)
	{
		purple_conversation_autoset_title(conv);
	}

	if (fields & PIDGIN_CONV_BUDDY_ICON)
	{
		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
			pidgin_conv_update_buddy_icon(conv);
	}

	if (fields & PIDGIN_CONV_MENU)
	{
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
		generate_send_to_items(win);
	}

	if (fields & PIDGIN_CONV_TAB_ICON)
	{
		update_tab_icon(conv);
		generate_send_to_items(win);		/* To update the icons in SendTo menu */
	}

	if ((fields & PIDGIN_CONV_TOPIC) &&
				purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
	{
		const char *topic;
		PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
		PidginChatPane *gtkchat = gtkconv->u.chat;

		if (gtkchat->topic_text != NULL)
		{
			topic = purple_conv_chat_get_topic(chat);

			gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), topic ? topic : "");
			gtk_tooltips_set_tip(gtkconv->tooltips, gtkchat->topic_text,
			                     topic ? topic : "", NULL);
		}
	}

	if (fields & PIDGIN_CONV_SMILEY_THEME)
		pidgin_themes_smiley_themeize(PIDGIN_CONVERSATION(conv)->imhtml);

	if ((fields & PIDGIN_CONV_COLORIZE_TITLE) ||
			(fields & PIDGIN_CONV_SET_TITLE))
	{
		char *title;
		PurpleConvIm *im = NULL;
		PurpleAccount *account = purple_conversation_get_account(conv);
		AtkObject *accessibility_obj;
		/* I think this is a little longer than it needs to be but I'm lazy. */
		char style[51];

		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
			im = PURPLE_CONV_IM(conv);

		if ((account == NULL) ||
			!purple_account_is_connected(account) ||
			((purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
				&& purple_conv_chat_has_left(PURPLE_CONV_CHAT(conv))))
			title = g_strdup_printf("(%s)", purple_conversation_get_title(conv));
		else
			title = g_strdup(purple_conversation_get_title(conv));

		*style = '\0';

		if (!GTK_WIDGET_REALIZED(gtkconv->tab_label))
			gtk_widget_realize(gtkconv->tab_label);

		accessibility_obj = gtk_widget_get_accessible(gtkconv->tab_cont);
		if (im != NULL &&
		    purple_conv_im_get_typing_state(im) == PURPLE_TYPING)
		{
			atk_object_set_description(accessibility_obj, _("Typing"));
			strncpy(style, "color=\"#4e9a06\"", sizeof(style));
		}
		else if (im != NULL &&
		         purple_conv_im_get_typing_state(im) == PURPLE_TYPED)
		{
			atk_object_set_description(accessibility_obj, _("Stopped Typing"));
			strncpy(style, "color=\"#c4a000\"", sizeof(style));
		}
		else if (gtkconv->unseen_state == PIDGIN_UNSEEN_NICK)
		{
			atk_object_set_description(accessibility_obj, _("Nick Said"));
			strncpy(style, "color=\"#204a87\" style=\"italic\" weight=\"bold\"", sizeof(style));
		}
		else if (gtkconv->unseen_state == PIDGIN_UNSEEN_TEXT)
		{
			atk_object_set_description(accessibility_obj, _("Unread Messages"));
			strncpy(style, "color=\"#a40000\" weight=\"bold\"", sizeof(style));
		}
		else if (gtkconv->unseen_state == PIDGIN_UNSEEN_EVENT)
		{
			atk_object_set_description(accessibility_obj, _("New Event"));
			strncpy(style, "color=\"#888a85\" style=\"italic\"", sizeof(style));
		}

		if (*style != '\0')
		{
			char *html_title,*label;

			html_title = g_markup_escape_text(title, -1);

			label = g_strdup_printf("<span %s>%s</span>",
			                        style, html_title);
			g_free(html_title);
			gtk_label_set_markup(GTK_LABEL(gtkconv->tab_label), label);
			g_free(label);
		}
		else
			gtk_label_set_text(GTK_LABEL(gtkconv->tab_label), title);

		if (pidgin_conv_window_is_active_conversation(conv))
			update_typing_icon(gtkconv);

		gtk_label_set_text(GTK_LABEL(gtkconv->menu_label), title);
		if (pidgin_conv_window_is_active_conversation(conv))
			gtk_window_set_title(GTK_WINDOW(win->window), title);

		g_free(title);
	}
}

static void
pidgin_conv_updated(PurpleConversation *conv, PurpleConvUpdateType type)
{
	PidginConvFields flags = 0;

	g_return_if_fail(conv != NULL);

	if (type == PURPLE_CONV_UPDATE_ACCOUNT)
	{
		flags = PIDGIN_CONV_ALL;
	}
	else if (type == PURPLE_CONV_UPDATE_TYPING ||
	         type == PURPLE_CONV_UPDATE_UNSEEN ||
	         type == PURPLE_CONV_UPDATE_TITLE)
	{
		flags = PIDGIN_CONV_COLORIZE_TITLE;
	}
	else if (type == PURPLE_CONV_UPDATE_TOPIC)
	{
		flags = PIDGIN_CONV_TOPIC;
	}
	else if (type == PURPLE_CONV_ACCOUNT_ONLINE ||
	         type == PURPLE_CONV_ACCOUNT_OFFLINE)
	{
		flags = PIDGIN_CONV_MENU | PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_SET_TITLE;
	}
	else if (type == PURPLE_CONV_UPDATE_AWAY)
	{
		flags = PIDGIN_CONV_TAB_ICON;
	}
	else if (type == PURPLE_CONV_UPDATE_ADD ||
	         type == PURPLE_CONV_UPDATE_REMOVE ||
	         type == PURPLE_CONV_UPDATE_CHATLEFT)
	{
		flags = PIDGIN_CONV_SET_TITLE | PIDGIN_CONV_MENU;
	}
	else if (type == PURPLE_CONV_UPDATE_ICON)
	{
		flags = PIDGIN_CONV_BUDDY_ICON;
	}
	else if (type == PURPLE_CONV_UPDATE_FEATURES)
	{
		flags = PIDGIN_CONV_MENU;
	}

	pidgin_conv_update_fields(conv, flags);
}

static PurpleConversationUiOps conversation_ui_ops =
{
	pidgin_conv_new,
	pidgin_conv_destroy,              /* destroy_conversation */
	NULL,                              /* write_chat           */
	pidgin_conv_write_im,             /* write_im             */
	pidgin_conv_write_conv,           /* write_conv           */
	pidgin_conv_chat_add_users,       /* chat_add_users       */
	pidgin_conv_chat_rename_user,     /* chat_rename_user     */
	pidgin_conv_chat_remove_users,    /* chat_remove_users    */
	pidgin_conv_chat_update_user,     /* chat_update_user     */
	pidgin_conv_present_conversation, /* present              */
	pidgin_conv_has_focus,            /* has_focus            */
	pidgin_conv_custom_smiley_add,    /* custom_smiley_add    */
	pidgin_conv_custom_smiley_write,  /* custom_smiley_write  */
	pidgin_conv_custom_smiley_close,  /* custom_smiley_close  */
	pidgin_conv_send_confirm,         /* send_confirm         */
};

PurpleConversationUiOps *
pidgin_conversations_get_conv_ui_ops(void)
{
	return &conversation_ui_ops;
}

/**************************************************************************
 * Public conversation utility functions
 **************************************************************************/
void
pidgin_conv_update_buddy_icon(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PidginWindow *win;

	GdkPixbufLoader *loader;
	GdkPixbufAnimation *anim;
	GError *err = NULL;

	const char *custom = NULL;
	const void *data = NULL;
	size_t len;

	GdkPixbuf *buf;

	GtkWidget *event;
	GtkWidget *frame;
	GdkPixbuf *scale;
	int scale_width, scale_height;

	PurpleAccount *account;
	PurplePluginProtocolInfo *prpl_info = NULL;

	PurpleBuddyIcon *icon;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(PIDGIN_IS_PIDGIN_CONVERSATION(conv));
	g_return_if_fail(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM);

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	if (!gtkconv->u.im->show_icon)
		return;

	account = purple_conversation_get_account(conv);
	if(account && account->gc)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

	/* Remove the current icon stuff */
	if (gtkconv->u.im->icon_container != NULL)
		gtk_widget_destroy(gtkconv->u.im->icon_container);
	gtkconv->u.im->icon_container = NULL;

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	gtkconv->u.im->anim = NULL;

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->iter = NULL;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	if (purple_conversation_get_gc(conv) == NULL)
		return;

	custom = custom_icon_pref_name(gtkconv);
	if (custom) {
		/* There is a custom icon for this user */
		char *contents = NULL;
		if (!g_file_get_contents(custom, &contents, &len, &err)) {
			purple_debug_warning("custom icon", "could not load custom icon %s for %s\n",
						custom, purple_conversation_get_name(conv));
			g_error_free(err);
			err = NULL;
		} else
			data = contents;
	}

	if (data == NULL) {
		icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));

		if (icon == NULL)
			return;

		data = purple_buddy_icon_get_data(icon, &len);
		custom = NULL;
	}

	loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(loader, data, len, NULL);
	gdk_pixbuf_loader_close(loader, &err);
	anim = gdk_pixbuf_loader_get_animation(loader);
	if (anim)
		g_object_ref(G_OBJECT(anim));
	g_object_unref(loader);

	if (custom)
		g_free((void*)data);

	if (!anim)
		return;
	gtkconv->u.im->anim = anim;

	if (err) {
		purple_debug(PURPLE_DEBUG_ERROR, "gtkconv",
				   "Buddy icon error: %s\n", err->message);
		g_error_free(err);
	}

	if (!gtkconv->u.im->anim)
		return;

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)) {
		gtkconv->u.im->iter = NULL;
		buf = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
	} else {
		gtkconv->u.im->iter =
			gdk_pixbuf_animation_get_iter(gtkconv->u.im->anim, NULL); /* LEAK */
		buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);
		if (gtkconv->u.im->animate)
			start_anim(NULL, gtkconv);
	}

	pidgin_buddy_icon_get_scale_size(buf, &prpl_info->icon_spec,
			PURPLE_ICON_SCALE_DISPLAY, &scale_width, &scale_height);
	scale = gdk_pixbuf_scale_simple(buf,
				MAX(gdk_pixbuf_get_width(buf) * scale_width /
				    gdk_pixbuf_animation_get_width(gtkconv->u.im->anim), 1),
				MAX(gdk_pixbuf_get_height(buf) * scale_height /
				    gdk_pixbuf_animation_get_height(gtkconv->u.im->anim), 1),
				GDK_INTERP_BILINEAR);

	gtkconv->u.im->icon_container = gtk_vbox_new(FALSE, 0);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(gtkconv->u.im->icon_container), frame,
					   FALSE, FALSE, 0);

	event = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(frame), event);
	g_signal_connect(G_OBJECT(event), "button-press-event",
					 G_CALLBACK(icon_menu), gtkconv);
	gtk_widget_show(event);

	gtkconv->u.im->icon = gtk_image_new_from_pixbuf(scale);
	gtkconv->auto_resize = TRUE;
	/* Reset the size request to allow the buddy icon to resize */
	gtk_widget_set_size_request(gtkconv->lower_hbox, -1, -1);
	g_idle_add(reset_auto_resize_cb, gtkconv);
	gtk_widget_set_size_request(gtkconv->u.im->icon, scale_width, scale_height);
	gtk_container_add(GTK_CONTAINER(event), gtkconv->u.im->icon);
	gtk_widget_show(gtkconv->u.im->icon);

	g_object_unref(G_OBJECT(scale));

	gtk_box_pack_start(GTK_BOX(gtkconv->lower_hbox),
			   gtkconv->u.im->icon_container, FALSE, FALSE, 0);

	gtk_widget_show(gtkconv->u.im->icon_container);
	gtk_widget_show(frame);

	/* The buddy icon code needs badly to be fixed. */
	if(pidgin_conv_window_is_active_conversation(conv))
	{
		buf = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
		gtk_window_set_icon(GTK_WINDOW(win->window), buf);
	}
}

void
pidgin_conv_update_buttons_by_protocol(PurpleConversation *conv)
{
	PidginWindow *win;

	if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		return;

	win = PIDGIN_CONVERSATION(conv)->win;

	if (win != NULL && pidgin_conv_window_is_active_conversation(conv))
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
}

int
pidgin_conv_get_tab_at_xy(PidginWindow *win, int x, int y, gboolean *to_right)
{
	gint nb_x, nb_y, x_rel, y_rel;
	GtkNotebook *notebook;
	GtkWidget *page, *tab;
	gint i, page_num = -1;
	gint count;
	gboolean horiz;

	if (to_right)
		*to_right = FALSE;

	notebook = GTK_NOTEBOOK(win->notebook);

	gdk_window_get_origin(win->notebook->window, &nb_x, &nb_y);
	x_rel = x - nb_x;
	y_rel = y - nb_y;

	horiz = (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(notebook) == GTK_POS_BOTTOM);

#if GTK_CHECK_VERSION(2,2,0)
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
#else
	/* this is hacky, but it's only for Gtk 2.0.0... */
	count = g_list_length(GTK_NOTEBOOK(notebook)->children);
#endif

	for (i = 0; i < count; i++) {

		page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
		tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), page);

		/* Make sure the tab is not hidden beyond an arrow */
		if (!GTK_WIDGET_DRAWABLE(tab))
			continue;

		if (horiz) {
			if (x_rel >= tab->allocation.x - PIDGIN_HIG_BOX_SPACE &&
					x_rel <= tab->allocation.x + tab->allocation.width + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && x_rel >= tab->allocation.x + tab->allocation.width/2)
					*to_right = TRUE;

				break;
			}
		} else {
			if (y_rel >= tab->allocation.y - PIDGIN_HIG_BOX_SPACE &&
					y_rel <= tab->allocation.y + tab->allocation.height + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && y_rel >= tab->allocation.y + tab->allocation.height/2)
					*to_right = TRUE;

				break;
			}
		}
	}

	if (page_num == -1) {
		/* Add after the last tab */
		page_num = count - 1;
	}

	return page_num;
}

static void
close_on_tabs_pref_cb(const char *name, PurplePrefType type,
					  gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	for (l = purple_get_conversations(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		if (value)
			gtk_widget_show(gtkconv->close);
		else
			gtk_widget_hide(gtkconv->close);
	}
}

static void
spellcheck_pref_cb(const char *name, PurplePrefType type,
				   gconstpointer value, gpointer data)
{
#ifdef USE_GTKSPELL
	GList *cl;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	GtkSpell *spell;

	for (cl = purple_get_conversations(); cl != NULL; cl = cl->next) {

		conv = (PurpleConversation *)cl->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		if (value)
			pidgin_setup_gtkspell(GTK_TEXT_VIEW(gtkconv->entry));
		else {
			spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(gtkconv->entry));
			gtkspell_detach(spell);
		}
	}
#endif
}

static void
tab_side_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *l;
	GtkPositionType pos;
	PidginWindow *win;

	pos = GPOINTER_TO_INT(value);

	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;

		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(win->notebook), pos&~8);
	}
}

static void
show_timestamps_pref_cb(const char *name, PurplePrefType type,
						gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	for (l = purple_get_conversations(); l != NULL; l = l->next)
	{
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);
		win     = gtkconv->win;

		gtk_check_menu_item_set_active(
		        GTK_CHECK_MENU_ITEM(win->menu.show_timestamps),
		        (gboolean)GPOINTER_TO_INT(value));

		gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),
			(gboolean)GPOINTER_TO_INT(value));
	}
}

static void
show_formatting_toolbar_pref_cb(const char *name, PurplePrefType type,
								gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	for (l = purple_get_conversations(); l != NULL; l = l->next)
	{
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);
		win     = gtkconv->win;

		gtk_check_menu_item_set_active(
		        GTK_CHECK_MENU_ITEM(win->menu.show_formatting_toolbar),
		        (gboolean)GPOINTER_TO_INT(value));

		if ((gboolean)GPOINTER_TO_INT(value))
			gtk_widget_show(gtkconv->toolbar);
		else
			gtk_widget_hide(gtkconv->toolbar);
	}
}

static void
animate_buddy_icons_pref_cb(const char *name, PurplePrefType type,
							gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	/* Set the "animate" flag for each icon based on the new preference */
	for (l = purple_get_ims(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;
		gtkconv = PIDGIN_CONVERSATION(conv);
		gtkconv->u.im->animate = GPOINTER_TO_INT(value);
	}

	/* Now either stop or start animation for the active conversation in each window */
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;
		conv = pidgin_conv_window_get_active_conversation(win);
		pidgin_conv_update_buddy_icon(conv);
	}
}

static void
show_buddy_icons_pref_cb(const char *name, PurplePrefType type,
						 gconstpointer value, gpointer data)
{
	GList *l;

	for (l = purple_get_conversations(); l != NULL; l = l->next) {
		PurpleConversation *conv = l->data;

		if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
			pidgin_conv_update_buddy_icon(conv);
	}
}

static void
conv_placement_usetabs_cb(const char *name, PurplePrefType type,
						  gconstpointer value, gpointer data)
{
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
}

static void
account_status_changed_cb(PurpleAccount *account, PurpleStatus *oldstatus,
                          PurpleStatus *newstatus)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away")!=0)
		return;

	if(purple_status_is_available(oldstatus) || !purple_status_is_available(newstatus))
		return;

	while ((l = hidden_convwin->gtkconvs) != NULL)
	{
		gtkconv = l->data;

		conv = gtkconv->active_conv;

		while(l && !purple_status_is_available(
					purple_account_get_active_status(
					purple_conversation_get_account(conv))))
			l = l->next;
		if (!l)
			break;

		pidgin_conv_window_remove_gtkconv(hidden_convwin, gtkconv);
		pidgin_conv_placement_place(gtkconv);

		/* TODO: do we need to do anything for any other conversations that are in the same gtkconv here?
		 * I'm a little concerned that not doing so will cause the "pending" indicator in the gtkblist not to be cleared. -DAA*/
		purple_conversation_update(conv, PURPLE_CONV_UPDATE_UNSEEN);
	}
}

static void
hide_new_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;
	gboolean when_away = FALSE;

	if(!hidden_convwin)
		return;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always")==0)
		return;

	if(strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away")==0)
		when_away = TRUE;

	while ((l = hidden_convwin->gtkconvs) != NULL)
	{
		gtkconv = l->data;

		conv = gtkconv->active_conv;

		if(when_away && !purple_status_is_available(
							purple_account_get_active_status(
							purple_conversation_get_account(conv))))
			continue;

		pidgin_conv_window_remove_gtkconv(hidden_convwin, gtkconv);
		pidgin_conv_placement_place(gtkconv);
	}
}


static void
conv_placement_pref_cb(const char *name, PurplePrefType type,
					   gconstpointer value, gpointer data)
{
	PidginConvPlacementFunc func;

	if (strcmp(name, PIDGIN_PREFS_ROOT "/conversations/placement"))
		return;

	func = pidgin_conv_placement_get_fnc(value);

	if (func == NULL)
		return;

	pidgin_conv_placement_set_current_func(func);
}

static PidginConversation *
get_gtkconv_with_contact(PurpleContact *contact)
{
	PurpleBlistNode *node;

	node = ((PurpleBlistNode*)contact)->child;

	for (; node; node = node->next)
	{
		PurpleBuddy *buddy = (PurpleBuddy*)node;
		PurpleConversation *conv;
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, buddy->name, buddy->account);
		if (conv)
			return PIDGIN_CONVERSATION(conv);
	}
	return NULL;
}

static void
account_signed_off_cb(PurpleConnection *gc, gpointer event)
{
	GList *iter;

	for (iter = purple_get_conversations(); iter; iter = iter->next)
	{
		PurpleConversation *conv = iter->data;

		/* This seems fine in theory, but we also need to cover the
		 * case of this account matching one of the other buddies in
		 * one of the contacts containing the buddy corresponding to
		 * a conversation.  It's easier to just update them all. */
		/* if (purple_conversation_get_account(conv) == account) */
			pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON |
							PIDGIN_CONV_MENU | PIDGIN_CONV_COLORIZE_TITLE);
	}
}

static gboolean
update_buddy_status_timeout(PurpleBuddy *buddy)
{
	/* To remove the signing-on/off door icon */
	PurpleConversation *conv;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, buddy->name, buddy->account);
	if (conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON);

	return FALSE;
}

static void
update_buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old, PurpleStatus *newstatus)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv)
	{
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_COLORIZE_TITLE);
		if ((purple_status_is_online(old) ^ purple_status_is_online(newstatus)) != 0)
			pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}

	/* In case a conversation is started after the buddy has signed-on/off */
	g_timeout_add(11000, (GSourceFunc)update_buddy_status_timeout, buddy);
}

static void
update_buddy_privacy_changed(PurpleBuddy *buddy)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv) {
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_MENU);
	}
}

static void
update_buddy_idle_changed(PurpleBuddy *buddy, gboolean old, gboolean newidle)
{
	PurpleConversation *conv;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, buddy->name, buddy->account);
	if (conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON);
}

static void
update_buddy_icon(PurpleBuddy *buddy)
{
	PurpleConversation *conv;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, buddy->name, buddy->account);
	if (conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_BUDDY_ICON);
}

static void
update_buddy_sign(PurpleBuddy *buddy, const char *which)
{
	PurplePresence *presence;
	PurpleStatus *on, *off;

	presence = purple_buddy_get_presence(buddy);
	if (!presence)
		return;
	off = purple_presence_get_status(presence, "offline");
	on = purple_presence_get_status(presence, "available");

	if (*(which+1) == 'f')
		update_buddy_status_changed(buddy, on, off);
	else
		update_buddy_status_changed(buddy, off, on);
}

static void
update_conversation_switched(PurpleConversation *conv)
{
	pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_SET_TITLE |
					PIDGIN_CONV_MENU | PIDGIN_CONV_BUDDY_ICON);
}

static void
update_buddy_typing(PurpleAccount *account, const char *who)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, account);
	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (gtkconv && gtkconv->active_conv == conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_COLORIZE_TITLE);
}

static void
update_chat(PurpleConversation *conv)
{
	pidgin_conv_update_fields(conv, PIDGIN_CONV_TOPIC |
					PIDGIN_CONV_MENU | PIDGIN_CONV_SET_TITLE);
}

static void
update_chat_topic(PurpleConversation *conv, const char *old, const char *new)
{
	pidgin_conv_update_fields(conv, PIDGIN_CONV_TOPIC);
}

void *
pidgin_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

void
pidgin_conversations_init(void)
{
	void *handle = pidgin_conversations_get_handle();
	void *blist_handle = purple_blist_get_handle();

	/* Conversations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting", TRUE);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_timestamps", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/placement", "last");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/placement_number", 1);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/font_size", 3);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/tabs", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/tab_side", GTK_POS_TOP);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/scrollback_lines", 4000);

	/* Conversations -> Chat */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/chat");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_width", 410);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/default_height", 160);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height", 50);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", 80);
	/* Conversations -> IM */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/im");

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons", TRUE);

	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/default_width", 410);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/default_height", 160);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height", 50);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "never");

	/* Connect callbacks. */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/close_on_tabs",
								close_on_tabs_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/show_timestamps",
								show_timestamps_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
								show_formatting_toolbar_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/spellcheck",
								spellcheck_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tab_side",
								tab_side_pref_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tabs",
								conv_placement_usetabs_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/placement",
								conv_placement_pref_cb, NULL);
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");

	/* IM callbacks */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons",
								animate_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons",
								show_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
                                hide_new_pref_cb, NULL);



	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	purple_signal_register(handle, "conversation-dragging",
	                     purple_marshal_VOID__POINTER_POINTER, NULL, 2,
	                     purple_value_new(PURPLE_TYPE_BOXED,
	                                    "PidginWindow *"),
	                     purple_value_new(PURPLE_TYPE_BOXED,
	                                    "PidginWindow *"));

	purple_signal_register(handle, "conversation-timestamp",
#if SIZEOF_TIME_T == 4
	                     purple_marshal_POINTER__POINTER_INT_BOOLEAN,
#elif SIZEOF_TIME_T == 8
			     purple_marshal_POINTER__POINTER_INT64_BOOLEAN,
#else
#error Unkown size of time_t
#endif
	                     purple_value_new(PURPLE_TYPE_STRING), 3,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_CONVERSATION),
#if SIZEOF_TIME_T == 4
	                     purple_value_new(PURPLE_TYPE_INT),
#elif SIZEOF_TIME_T == 8
	                     purple_value_new(PURPLE_TYPE_INT64),
#else
# error Unknown size of time_t
#endif
	                     purple_value_new(PURPLE_TYPE_BOOLEAN));

	purple_signal_register(handle, "displaying-im-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_INT));

	purple_signal_register(handle, "displayed-im-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_INT));

	purple_signal_register(handle, "displaying-chat-msg",
						 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
						 purple_value_new(PURPLE_TYPE_BOOLEAN), 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new_outgoing(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_INT));

	purple_signal_register(handle, "displayed-chat-msg",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 NULL, 5,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_STRING),
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION),
						 purple_value_new(PURPLE_TYPE_INT));

	purple_signal_register(handle, "conversation-switched",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONVERSATION));

	/**********************************************************************
	 * Register commands
	 **********************************************************************/
	purple_cmd_register("say", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  say_command_cb, _("say &lt;message&gt;:  Send a message normally as if you weren't using a command."), NULL);
	purple_cmd_register("me", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  me_command_cb, _("me &lt;action&gt;:  Send an IRC style action to a buddy or chat."), NULL);
	purple_cmd_register("debug", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  debug_command_cb, _("debug &lt;option&gt;:  Send various debug information to the current conversation."), NULL);
	purple_cmd_register("clear", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  clear_command_cb, _("clear: Clears the conversation scrollback."), NULL);
	purple_cmd_register("help", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  help_command_cb, _("help &lt;command&gt;:  Help on a specific command."), NULL);

	/**********************************************************************
	 * UI operations
	 **********************************************************************/

	purple_signal_connect(purple_connections_get_handle(), "signed-on", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONV_ACCOUNT_ONLINE));
	purple_signal_connect(purple_connections_get_handle(), "signed-off", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONV_ACCOUNT_OFFLINE));

	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg",
						handle, G_CALLBACK(received_im_msg_cb), NULL);

	purple_conversations_set_ui_ops(&conversation_ui_ops);

	hidden_convwin = pidgin_conv_window_new();
	window_list = g_list_remove(window_list, hidden_convwin);

	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed",
                        handle, PURPLE_CALLBACK(account_status_changed_cb), NULL);

	/* Callbacks to update a conversation */
	purple_signal_connect(blist_handle, "buddy-added", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-removed", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-on",
						handle, PURPLE_CALLBACK(update_buddy_sign), "on");
	purple_signal_connect(blist_handle, "buddy-signed-off",
						handle, PURPLE_CALLBACK(update_buddy_sign), "off");
	purple_signal_connect(blist_handle, "buddy-status-changed",
						handle, PURPLE_CALLBACK(update_buddy_status_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-privacy-changed",
						handle, PURPLE_CALLBACK(update_buddy_privacy_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-idle-changed",
						handle, PURPLE_CALLBACK(update_buddy_idle_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-icon-changed",
						handle, PURPLE_CALLBACK(update_buddy_icon), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing-stopped",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-switched",
						handle, PURPLE_CALLBACK(update_conversation_switched), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-left", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-joined", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", handle,
						PURPLE_CALLBACK(update_chat_topic), NULL);
	purple_signal_connect_priority(purple_conversations_get_handle(), "conversation-updated", handle,
						PURPLE_CALLBACK(pidgin_conv_updated), NULL,
						PURPLE_SIGNAL_PRIORITY_LOWEST);
}

void
pidgin_conversations_uninit(void)
{
	purple_prefs_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_unregister_by_instance(pidgin_conversations_get_handle());
	pidgin_conv_window_destroy(hidden_convwin);
	hidden_convwin=NULL;
}
















/* down here is where gtkconvwin.c ought to start. except they share like every freaking function,
 * and touch each others' private members all day long */

/**
 * @file gtkconvwin.c GTK+ Conversation Window API
 * @ingroup pidgin
 *
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 *
 */
#include "internal.h"
#include "pidgin.h"


#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "cmds.h"
#include "debug.h"
#include "imgstore.h"
#include "log.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"

static void
do_close(GtkWidget *w, int resp, PidginWindow *win)
{
	gtk_widget_destroy(warn_close_dialog);
	warn_close_dialog = NULL;

	if (resp == GTK_RESPONSE_OK)
		pidgin_conv_window_destroy(win);
}

static void
build_warn_close_dialog(PidginWindow *gtkwin)
{
	GtkWidget *label;
	GtkWidget *vbox, *hbox;
	GtkWidget *img;

	g_return_if_fail(warn_close_dialog == NULL);


	warn_close_dialog = gtk_dialog_new_with_buttons(
							_("Confirm close"),
							GTK_WINDOW(gtkwin->window), GTK_DIALOG_MODAL,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							PIDGIN_STOCK_CLOSE_TABS, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(warn_close_dialog),
	                                GTK_RESPONSE_OK);

	gtk_container_set_border_width(GTK_CONTAINER(warn_close_dialog),
	                               6);
	gtk_window_set_resizable(GTK_WINDOW(warn_close_dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(warn_close_dialog),
	                             FALSE);

	/* Setup the outside spacing. */
	vbox = GTK_DIALOG(warn_close_dialog)->vbox;

	gtk_box_set_spacing(GTK_BOX(vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_WARNING,
	                               gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	/* Setup the inner hbox and put the dialog's icon in it. */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	/* Setup the right vbox. */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("You have unread messages. Are you sure you want to close the window?"));
	gtk_widget_set_size_request(label, 350, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* Connect the signals. */
	g_signal_connect(G_OBJECT(warn_close_dialog), "response",
	                 G_CALLBACK(do_close), gtkwin);

}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gboolean
close_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginWindow *win = d;
	GList *l;

	/* If there are unread messages then show a warning dialog */
	for (l = pidgin_conv_window_get_gtkconvs(win);
	     l != NULL; l = l->next)
	{
		PidginConversation *gtkconv = l->data;
		if (purple_conversation_get_type(gtkconv->active_conv) == PURPLE_CONV_TYPE_IM &&
				gtkconv->unseen_state >= PIDGIN_UNSEEN_TEXT)
		{
			build_warn_close_dialog(win);
			gtk_widget_show_all(warn_close_dialog);

			return TRUE;
		}
	}

	pidgin_conv_window_destroy(win);

	return TRUE;
}

static void
gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state)
{
	if (state == PIDGIN_UNSEEN_NONE)
	{
		gtkconv->unseen_count = 0;
		gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	}
	else
	{
		if (state >= PIDGIN_UNSEEN_TEXT)
			gtkconv->unseen_count++;

		if (state > gtkconv->unseen_state)
			gtkconv->unseen_state = state;
	}

	purple_conversation_update(gtkconv->active_conv, PURPLE_CONV_UPDATE_UNSEEN);
}

/*
 * When a conversation window is focused, we know the user
 * has looked at it so we know there are no longer unseen
 * messages.
 */
static gint
focus_win_cb(GtkWidget *w, GdkEventFocus *e, gpointer d)
{
	PidginWindow *win = d;
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);

	return FALSE;
}

#if !GTK_CHECK_VERSION(2,6,0)
/* Courtesy of Galeon! */
static void
tab_close_button_state_changed_cb(GtkWidget *widget, GtkStateType prev_state)
{
	if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
		gtk_widget_set_state(widget, GTK_STATE_NORMAL);
}
#endif

static void
notebook_init_grab(PidginWindow *gtkwin, GtkWidget *widget)
{
	static GdkCursor *cursor = NULL;

	gtkwin->in_drag = TRUE;

	if (gtkwin->drag_leave_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            gtkwin->drag_leave_signal);
		gtkwin->drag_leave_signal = 0;
	}

	if (cursor == NULL)
		cursor = gdk_cursor_new(GDK_FLEUR);

	/* Grab the pointer */
	gtk_grab_add(gtkwin->notebook);
#ifndef _WIN32
	/* Currently for win32 GTK+ (as of 2.2.1), gdk_pointer_is_grabbed will
	   always be true after a button press. */
	if (!gdk_pointer_is_grabbed())
#endif
		gdk_pointer_grab(gtkwin->notebook->window, FALSE,
		                 GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
		                 NULL, cursor, GDK_CURRENT_TIME);
}

static gboolean
notebook_motion_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{

	/*
	* Make sure the user moved the mouse far enough for the
	* drag to be initiated.
	*/
	if (win->in_predrag) {
		if (e->x_root <  win->drag_min_x ||
		    e->x_root >= win->drag_max_x ||
		    e->y_root <  win->drag_min_y ||
		    e->y_root >= win->drag_max_y) {

			    win->in_predrag = FALSE;
			    notebook_init_grab(win, widget);
		    }
	}
	else { /* Otherwise, draw the arrows. */
		PidginWindow *dest_win;
		GtkNotebook *dest_notebook;
		GtkWidget *tab;
		gint nb_x, nb_y, page_num;
		gint arrow1_x, arrow1_y, arrow2_x, arrow2_y;
		gboolean horiz_tabs = FALSE;
		PidginConversation *gtkconv;
		gboolean to_right = FALSE;

		/* Get the window that the cursor is over. */
		dest_win = pidgin_conv_window_get_at_xy(e->x_root, e->y_root);

		if (dest_win == NULL) {
			dnd_hints_hide_all();

			return TRUE;
		}

		dest_notebook = GTK_NOTEBOOK(dest_win->notebook);

		gdk_window_get_origin(GTK_WIDGET(dest_notebook)->window, &nb_x, &nb_y);

		arrow1_x = arrow2_x = nb_x;
		arrow1_y = arrow2_y = nb_y;

		page_num = pidgin_conv_get_tab_at_xy(dest_win,
		                                      e->x_root, e->y_root, &to_right);
		to_right = to_right && (win != dest_win);

		if (gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_TOP ||
		    gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_BOTTOM) {

			    horiz_tabs = TRUE;
		    }

		gtkconv = pidgin_conv_window_get_gtkconv_at_index(dest_win, page_num);
		tab = gtkconv->tabby;

		if (horiz_tabs) {
			arrow1_x = arrow2_x = nb_x + tab->allocation.x;

			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				arrow1_x += tab->allocation.width;
				arrow2_x += tab->allocation.width;
			}

			arrow1_y = nb_y + tab->allocation.y;
			arrow2_y = nb_y + tab->allocation.y + tab->allocation.height;
		} else {
			arrow1_x = nb_x + tab->allocation.x;
			arrow2_x = nb_x + tab->allocation.x + tab->allocation.width;
			arrow1_y = arrow2_y = nb_y + tab->allocation.y;

			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				arrow1_y += tab->allocation.height;
				arrow2_y += tab->allocation.height;
			}
		}

		if (horiz_tabs) {
			dnd_hints_show(HINT_ARROW_DOWN, arrow1_x, arrow1_y);
			dnd_hints_show(HINT_ARROW_UP,   arrow2_x, arrow2_y);
		} else {
			dnd_hints_show(HINT_ARROW_RIGHT, arrow1_x, arrow1_y);
			dnd_hints_show(HINT_ARROW_LEFT,  arrow2_x, arrow2_y);
		}
	}

	return TRUE;
}

static gboolean
notebook_leave_cb(GtkWidget *widget, GdkEventCrossing *e, PidginWindow *win)
{
	if (win->in_drag)
		return FALSE;

	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		    win->in_predrag = FALSE;
		    notebook_init_grab(win, widget);
	    }

	return TRUE;
}

/*
 * THANK YOU GALEON!
 */
static gboolean
notebook_press_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{
	gint nb_x, nb_y, x_rel, y_rel;
	int tab_clicked;
	GtkWidget *page;
	GtkWidget *tab;

	if (e->button == 2 && e->type == GDK_BUTTON_PRESS) {
		PidginConversation *gtkconv;
		tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

		if (tab_clicked == -1)
			return FALSE;

		gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, tab_clicked);
		close_conv_cb(NULL, gtkconv);
		return TRUE;
	}


	if (e->button != 1 || e->type != GDK_BUTTON_PRESS)
		return FALSE;


	if (win->in_drag) {
		purple_debug(PURPLE_DEBUG_WARNING, "gtkconv",
		           "Already in the middle of a window drag at tab_press_cb\n");
		return TRUE;
	}

	/*
	* Make sure a tab was actually clicked. The arrow buttons
	* mess things up.
	*/
	tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

	if (tab_clicked == -1)
		return FALSE;

	/*
	* Get the relative position of the press event, with regards to
	* the position of the notebook.
	*/
	gdk_window_get_origin(win->notebook->window, &nb_x, &nb_y);

	x_rel = e->x_root - nb_x;
	y_rel = e->y_root - nb_y;

	/* Reset the min/max x/y */
	win->drag_min_x = 0;
	win->drag_min_y = 0;
	win->drag_max_x = 0;
	win->drag_max_y = 0;

	/* Find out which tab was dragged. */
	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), tab_clicked);
	tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(win->notebook), page);

	win->drag_min_x = tab->allocation.x      + nb_x;
	win->drag_min_y = tab->allocation.y      + nb_y;
	win->drag_max_x = tab->allocation.width  + win->drag_min_x;
	win->drag_max_y = tab->allocation.height + win->drag_min_y;

	/* Make sure the click occurred in the tab. */
	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		    return FALSE;
	    }

	win->in_predrag = TRUE;
	win->drag_tab = tab_clicked;

	/* Connect the new motion signals. */
	win->drag_motion_signal =
		g_signal_connect(G_OBJECT(widget), "motion_notify_event",
		                 G_CALLBACK(notebook_motion_cb), win);

	win->drag_leave_signal =
		g_signal_connect(G_OBJECT(widget), "leave_notify_event",
		                 G_CALLBACK(notebook_leave_cb), win);

	return FALSE;
}

static gboolean
notebook_release_cb(GtkWidget *widget, GdkEventButton *e, PidginWindow *win)
{
	PidginWindow *dest_win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	gint dest_page_num = 0;
	gboolean new_window = FALSE;
	gboolean to_right = FALSE;

	/*
	* Don't check to make sure that the event's window matches the
	* widget's, because we may be getting an event passed on from the
	* close button.
	*/
	if (e->button != 1 && e->type != GDK_BUTTON_RELEASE)
		return FALSE;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		gtk_grab_remove(widget);
	}

	if (!win->in_predrag && !win->in_drag)
		return FALSE;

	/* Disconnect the motion signal. */
	if (win->drag_motion_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            win->drag_motion_signal);

		win->drag_motion_signal = 0;
	}

	/*
	* If we're in a pre-drag, we'll also need to disconnect the leave
	* signal.
	*/
	if (win->in_predrag) {
		win->in_predrag = FALSE;

		if (win->drag_leave_signal) {
			g_signal_handler_disconnect(G_OBJECT(widget),
			                            win->drag_leave_signal);

			win->drag_leave_signal = 0;
		}
	}

	/* If we're not in drag...        */
	/* We're perfectly normal people! */
	if (!win->in_drag)
		return FALSE;

	win->in_drag = FALSE;

	dnd_hints_hide_all();

	dest_win = pidgin_conv_window_get_at_xy(e->x_root, e->y_root);

	conv = pidgin_conv_window_get_active_conversation(win);

	if (dest_win == NULL) {
		/* If the current window doesn't have any other conversations,
		* there isn't much point transferring the conv to a new window. */
		if (pidgin_conv_window_get_gtkconv_count(win) > 1) {
			/* Make a new window to stick this to. */
			dest_win = pidgin_conv_window_new();
			new_window = TRUE;
		}
	}

	if (dest_win == NULL)
		return FALSE;

	purple_signal_emit(pidgin_conversations_get_handle(),
	                 "conversation-dragging", win, dest_win);

	/* Get the destination page number. */
	if (!new_window)
		dest_page_num = pidgin_conv_get_tab_at_xy(dest_win,
		                                           e->x_root, e->y_root, &to_right);

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, win->drag_tab);

	if (win == dest_win) {
		gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, dest_page_num);
	} else {
		pidgin_conv_window_remove_gtkconv(win, gtkconv);
		pidgin_conv_window_add_gtkconv(dest_win, gtkconv);
		gtk_notebook_reorder_child(GTK_NOTEBOOK(dest_win->notebook), gtkconv->tab_cont, dest_page_num + to_right);
		pidgin_conv_window_switch_gtkconv(dest_win, gtkconv);
		if (new_window) {
			gint win_width, win_height;

			gtk_window_get_size(GTK_WINDOW(dest_win->window),
			                    &win_width, &win_height);

			gtk_window_move(GTK_WINDOW(dest_win->window),
			                e->x_root - (win_width  / 2),
			                e->y_root - (win_height / 2));

			pidgin_conv_window_show(dest_win);
		}
	}

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);

	return TRUE;
}


static void
before_switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
                      gpointer user_data)
{
	PidginWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	win = user_data;
	conv = pidgin_conv_window_get_active_conversation(win);

	g_return_if_fail(conv != NULL);

	if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_IM)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	stop_anim(NULL, gtkconv);
}
static void
close_window(GtkWidget *w, PidginWindow *win)
{
	close_win_cb(w, NULL, win);
}

static void
detach_tab_cb(GtkWidget *w, GObject *menu)
{
	PidginWindow *win, *new_window;
	PidginConversation *gtkconv;

	gtkconv = g_object_get_data(menu, "clicked_tab");

	if (!gtkconv)
		return;

	win = pidgin_conv_get_window(gtkconv);
	/* Nothing to do if there's only one tab in the window */
	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		return;

	pidgin_conv_window_remove_gtkconv(win, gtkconv);

	new_window = pidgin_conv_window_new();
	pidgin_conv_window_add_gtkconv(new_window, gtkconv);
	pidgin_conv_window_show(new_window);
}

static void
close_others_cb(GtkWidget *w, GObject *menu)
{
	GList *iter;
	PidginConversation *gtkconv;
	PidginWindow *win;

	gtkconv = g_object_get_data(menu, "clicked_tab");

	if (!gtkconv)
		return;

	win = pidgin_conv_get_window(gtkconv);

	for (iter = pidgin_conv_window_get_gtkconvs(win); iter; )
	{
		PidginConversation *gconv = iter->data;
		iter = iter->next;

		if (gconv != gtkconv)
		{
			close_conv_cb(NULL, gconv);
		}
	}
}

static void close_tab_cb(GtkWidget *w, GObject *menu)
{
	PidginConversation *gtkconv;

	gtkconv = g_object_get_data(menu, "clicked_tab");

	if (gtkconv)
		close_conv_cb(NULL, gtkconv);
}

static gboolean
right_click_menu_cb(GtkNotebook *notebook, GdkEventButton *event, PidginWindow *win)
{
	GtkWidget *item, *menu;
	PidginConversation *gtkconv;

	if (event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win,
			pidgin_conv_get_tab_at_xy(win, event->x_root, event->y_root, NULL));

	if (g_object_get_data(G_OBJECT(notebook->menu), "clicked_tab"))
	{
		g_object_set_data(G_OBJECT(notebook->menu), "clicked_tab", gtkconv);
		return FALSE;
	}

	g_object_set_data(G_OBJECT(notebook->menu), "clicked_tab", gtkconv);

	menu = notebook->menu;
	pidgin_separator(GTK_WIDGET(menu));

	item = gtk_menu_item_new_with_label(_("Close other tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_others_cb), menu);

	item = gtk_menu_item_new_with_label(_("Close all tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_window), win);

	pidgin_separator(menu);

	item = gtk_menu_item_new_with_label(_("Detach this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(detach_tab_cb), menu);

	item = gtk_menu_item_new_with_label(_("Close this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_tab_cb), menu);

	return FALSE;
}

static void
switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
               gpointer user_data)
{
	PidginWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	const char *sound_method;

	win = user_data;
	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, page_num);
	conv = gtkconv->active_conv;

	g_return_if_fail(conv != NULL);

	/* clear unseen flag if conversation is not hidden */
	if(!pidgin_conv_is_hidden(gtkconv)) {
		gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
	}

	/* Update the menubar */

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkconv->win->menu.logging),
	                               purple_conversation_is_logging(conv));

	generate_send_to_items(win);
	regenerate_options_items(win);

	pidgin_conv_switch_active_conversation(conv);

	sound_method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (strcmp(sound_method, "none") != 0)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.sounds),
		                               gtkconv->make_sound);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_formatting_toolbar),
	                               purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_timestamps),
	                               purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_timestamps"));

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM &&
	    purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(win->menu.show_icon),
		                               gtkconv->u.im->show_icon);
	}

	/*
	 * We pause icons when they are not visible.  If this icon should
	 * be animated then start it back up again.
	 */
	if ((purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) &&
	    (gtkconv->u.im->animate))
		start_anim(NULL, gtkconv);

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);
}

/**************************************************************************
 * GTK+ window ops
 **************************************************************************/

GList *
pidgin_conv_windows_get_list()
{
	return window_list;
}

static GList*
make_status_icon_list(const char *stock, GtkWidget *w)
{
	GList *l = NULL;
	l = g_list_append(l, gtk_widget_render_icon (w, stock,
                                       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL), "GtkWindow"));
	l = g_list_append(l, gtk_widget_render_icon (w, stock,
                                       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL), "GtkWindow"));
	l = g_list_append(l, gtk_widget_render_icon (w, stock,
                                       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MEDIUM), "GtkWindow"));
	l = g_list_append(l, gtk_widget_render_icon (w, stock,
                                       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE), "GtkWindow"));
	return l;
}

static void
create_icon_lists(GtkWidget *w)
{
	available_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AVAILABLE, w);
	busy_list = make_status_icon_list(PIDGIN_STOCK_STATUS_BUSY, w);
	xa_list = make_status_icon_list(PIDGIN_STOCK_STATUS_XA, w);
	login_list = make_status_icon_list(PIDGIN_STOCK_STATUS_LOGIN, w);
	logout_list = make_status_icon_list(PIDGIN_STOCK_STATUS_LOGOUT, w);
	offline_list = make_status_icon_list(PIDGIN_STOCK_STATUS_OFFLINE, w);
	away_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AWAY, w);
	prpl_lists = g_hash_table_new(g_str_hash, g_str_equal);
}

PidginWindow *
pidgin_conv_window_new()
{
	PidginWindow *win;
	GtkPositionType pos;
	GtkWidget *testidea;
	GtkWidget *menubar;

	win = g_malloc0(sizeof(PidginWindow));

	window_list = g_list_append(window_list, win);

	/* Create the window. */
	win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(win->window), "conversation");
	gtk_window_set_resizable(GTK_WINDOW(win->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(win->window), 0);
	GTK_WINDOW(win->window)->allow_shrink = TRUE;

	if (available_list == NULL) {
		create_icon_lists(win->window);
	}

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(close_win_cb), win);

	g_signal_connect(G_OBJECT(win->window), "focus_in_event",
	                 G_CALLBACK(focus_win_cb), win);

	/* Create the notebook. */
	win->notebook = gtk_notebook_new();

	pos = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side");

#if 0
	gtk_notebook_set_tab_hborder(GTK_NOTEBOOK(win->notebook), 0);
	gtk_notebook_set_tab_vborder(GTK_NOTEBOOK(win->notebook), 0);
#endif
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(win->notebook), pos);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(win->notebook), TRUE);
	gtk_notebook_popup_enable(GTK_NOTEBOOK(win->notebook));
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook), FALSE);

	g_signal_connect(G_OBJECT(win->notebook), "button-press-event",
					G_CALLBACK(right_click_menu_cb), win);

	gtk_widget_show(win->notebook);

	g_signal_connect(G_OBJECT(win->notebook), "switch_page",
	                 G_CALLBACK(before_switch_conv_cb), win);
	g_signal_connect_after(G_OBJECT(win->notebook), "switch_page",
	                       G_CALLBACK(switch_conv_cb), win);

	/* Setup the tab drag and drop signals. */
	gtk_widget_add_events(win->notebook,
	                      GDK_BUTTON1_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(win->notebook), "button_press_event",
	                 G_CALLBACK(notebook_press_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "button_release_event",
	                 G_CALLBACK(notebook_release_cb), win);

	testidea = gtk_vbox_new(FALSE, 0);

	/* Setup the menubar. */
	menubar = setup_menubar(win);
	gtk_box_pack_start(GTK_BOX(testidea), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(testidea), win->notebook, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(win->window), testidea);

	gtk_widget_show(testidea);

#ifdef _WIN32
	g_signal_connect(G_OBJECT(win->window), "show",
	                 G_CALLBACK(winpidgin_ensure_onscreen), win->window);
#endif

	return win;
}

void
pidgin_conv_window_destroy(PidginWindow *win)
{
	purple_prefs_disconnect_by_handle(win);
	window_list = g_list_remove(window_list, win);

	/* Close the "Find" dialog if it's open */
	if (win->dialogs.search)
		gtk_widget_destroy(win->dialogs.search);

	gtk_widget_hide_all(win->window);

	if (win->gtkconvs) {
		while (win->gtkconvs) {
			GList *nextgtk = win->gtkconvs->next;
			PidginConversation *gtkconv = win->gtkconvs->data;
			GList *nextcore = gtkconv->convs->next;
			PurpleConversation *conv = gtkconv->convs->data;
			purple_conversation_destroy(conv);
			if (!nextgtk && !nextcore)
			/* we'll end up invoking ourselves when we destroy our last child */
			/* so don't destroy ourselves right now */
				return;
		}
		return;
	}
	gtk_widget_destroy(win->window);

	g_object_unref(G_OBJECT(win->menu.item_factory));

	purple_notify_close_with_handle(win);

	g_free(win);
}

void
pidgin_conv_window_show(PidginWindow *win)
{
	gtk_widget_show(win->window);
}

void
pidgin_conv_window_hide(PidginWindow *win)
{
	gtk_widget_hide(win->window);
}

void
pidgin_conv_window_raise(PidginWindow *win)
{
	gdk_window_raise(GDK_WINDOW(win->window->window));
}

void
pidgin_conv_window_switch_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
	                              gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
		                              gtkconv->tab_cont));
}

void
pidgin_conv_window_add_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginConversation *focus_gtkconv;
	GtkWidget *tabby, *menu_tabby;
	GtkWidget *tab_cont = gtkconv->tab_cont;
	GtkWidget *close_image;
	PurpleConversationType conv_type;
	const gchar *tmp_lab;
	gint close_button_width, close_button_height, focus_width, focus_pad;
	gboolean tabs_side = FALSE;
	gint angle = 0;

	conv_type = purple_conversation_get_type(conv);


	win->gtkconvs = g_list_append(win->gtkconvs, gtkconv);
	gtkconv->win = win;

	if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_LEFT ||
	    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_RIGHT)
		tabs_side = TRUE;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_LEFT|8))
		angle = 90;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_RIGHT|8))
		angle = 270;

	if (angle)
		gtkconv->tabby = tabby = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	else
		gtkconv->tabby = tabby = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtkconv->menu_tabby = menu_tabby = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

	/* Close button. */
	gtkconv->close = gtk_button_new();
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &close_button_width, &close_button_height);
	if (gtk_check_version(2, 4, 2) == NULL) {
		/* Need to account for extra padding around the gtkbutton */
		gtk_widget_style_get(GTK_WIDGET(gtkconv->close),
		                     "focus-line-width", &focus_width,
		                     "focus-padding", &focus_pad,
		                     NULL);
		close_button_width += (focus_width + focus_pad) * 2;
		close_button_height += (focus_width + focus_pad) * 2;
	}
	gtk_widget_set_size_request(GTK_WIDGET(gtkconv->close),
	                            close_button_width, close_button_height);

	gtk_button_set_relief(GTK_BUTTON(gtkconv->close), GTK_RELIEF_NONE);
	close_image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(close_image);
	gtk_container_add(GTK_CONTAINER(gtkconv->close), close_image);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->close,
	                     _("Close conversation"), NULL);

	g_signal_connect(G_OBJECT(gtkconv->close), "clicked",
	                 G_CALLBACK(close_conv_cb), gtkconv);

#if !GTK_CHECK_VERSION(2,6,0)
	/*
	* I love Galeon. They have a fix for that stupid annoying visible
	* border bug. I love you guys! -- ChipX86
	*/
	/* This is fixed properly in some version of Gtk before 2.6.0  */
	g_signal_connect(G_OBJECT(gtkconv->close), "state_changed",
	                 G_CALLBACK(tab_close_button_state_changed_cb), NULL);
#endif

	/* Status icon. */
	gtkconv->icon = gtk_image_new();
	gtkconv->menu_icon = gtk_image_new();
	update_tab_icon(conv);

	/* Tab label. */
	gtkconv->tab_label = gtk_label_new(tmp_lab = purple_conversation_get_title(conv));

#if GTK_CHECK_VERSION(2,6,0)
	if (!angle)
		g_object_set(G_OBJECT(gtkconv->tab_label), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), 6);
	if (tabs_side) {
		gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), MIN(g_utf8_strlen(tmp_lab, -1), 12));
	}
	if (angle)
		gtk_label_set_angle(GTK_LABEL(gtkconv->tab_label), angle);
#endif
	gtkconv->menu_label = gtk_label_new(purple_conversation_get_title(conv));
#if 0
	gtk_misc_set_alignment(GTK_MISC(gtkconv->tab_label), 0.00, 0.5);
	gtk_misc_set_padding(GTK_MISC(gtkconv->tab_label), 4, 0);
#endif

	/* Pack it all together. */
	if (angle == 90)
		gtk_box_pack_start(GTK_BOX(tabby), gtkconv->close, FALSE, FALSE, 0);
	else
		gtk_box_pack_start(GTK_BOX(tabby), gtkconv->icon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(menu_tabby), gtkconv->menu_icon,
	                   FALSE, FALSE, 0);

	gtk_widget_show_all(gtkconv->icon);
	gtk_widget_show_all(gtkconv->menu_icon);

	gtk_box_pack_start(GTK_BOX(tabby), gtkconv->tab_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(menu_tabby), gtkconv->menu_label, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->tab_label);
	gtk_widget_show(gtkconv->menu_label);
	gtk_misc_set_alignment(GTK_MISC(gtkconv->menu_label), 0, 0);

	if (angle == 90)
		gtk_box_pack_start(GTK_BOX(tabby), gtkconv->icon, FALSE, FALSE, 0);
	else
		gtk_box_pack_start(GTK_BOX(tabby), gtkconv->close, FALSE, FALSE, 0);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs"))
		gtk_widget_show(gtkconv->close);

	gtk_widget_show(tabby);
	gtk_widget_show(menu_tabby);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
		pidgin_conv_update_buddy_icon(conv);

	/* Add this pane to the conversation's notebook. */
	gtk_notebook_append_page_menu(GTK_NOTEBOOK(win->notebook), tab_cont, tabby, menu_tabby);
	gtk_notebook_set_tab_label_packing(GTK_NOTEBOOK(win->notebook), tab_cont, !tabs_side && !angle, TRUE, GTK_PACK_START);


	gtk_widget_show(tab_cont);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1) {
		/* Er, bug in notebooks? Switch to the page manually. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);

		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
		                           purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"));
	} else
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), TRUE);

	focus_gtkconv = g_list_nth_data(pidgin_conv_window_get_gtkconvs(win),
	                             gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook)));
	gtk_widget_grab_focus(focus_gtkconv->entry);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		update_send_to_selection(win);
}

void
pidgin_conv_window_remove_gtkconv(PidginWindow *win, PidginConversation *gtkconv)
{
	unsigned int index;
	PurpleConversationType conv_type;

	conv_type = purple_conversation_get_type(gtkconv->active_conv);
	index = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont);

	g_object_ref(gtkconv->tab_cont);
	gtk_object_sink(GTK_OBJECT(gtkconv->tab_cont));

	gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), index);

	/* go back to tabless if need be */
	if (pidgin_conv_window_get_gtkconv_count(win) <= 2) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
		                           purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"));
	}

	win->gtkconvs = g_list_remove(win->gtkconvs, gtkconv);

	if (!win->gtkconvs && win != hidden_convwin)
		pidgin_conv_window_destroy(win);
}

PidginConversation *
pidgin_conv_window_get_gtkconv_at_index(const PidginWindow *win, int index)
{
	GtkWidget *tab_cont;

	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	return tab_cont ? g_object_get_data(G_OBJECT(tab_cont), "PidginConversation") : NULL;
}

PidginConversation *
pidgin_conv_window_get_active_gtkconv(const PidginWindow *win)
{
	int index;
	GtkWidget *tab_cont;

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));
	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	if (!tab_cont)
		return NULL;
	return g_object_get_data(G_OBJECT(tab_cont), "PidginConversation");
}


PurpleConversation *
pidgin_conv_window_get_active_conversation(const PidginWindow *win)
{
	PidginConversation *gtkconv;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	return gtkconv ? gtkconv->active_conv : NULL;
}

gboolean
pidgin_conv_window_is_active_conversation(const PurpleConversation *conv)
{
	return conv == pidgin_conv_window_get_active_conversation(PIDGIN_CONVERSATION(conv)->win);
}

gboolean
pidgin_conv_window_has_focus(PidginWindow *win)
{
	gboolean has_focus = FALSE;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}

PidginWindow *
pidgin_conv_window_get_at_xy(int x, int y)
{
	PidginWindow *win;
	GdkWindow *gdkwin;
	GList *l;

	gdkwin = gdk_window_at_pointer(&x, &y);

	if (gdkwin)
		gdkwin = gdk_window_get_toplevel(gdkwin);

	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;

		if (gdkwin == win->window->window)
			return win;
	}

	return NULL;
}

GList *
pidgin_conv_window_get_gtkconvs(PidginWindow *win)
{
	return win->gtkconvs;
}

guint
pidgin_conv_window_get_gtkconv_count(PidginWindow *win)
{
	return g_list_length(win->gtkconvs);
}

PidginWindow *
pidgin_conv_window_first_with_type(PurpleConversationType type)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	if (type == PURPLE_CONV_TYPE_UNKNOWN)
		return NULL;

	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (purple_conversation_get_type(conv->active_conv) == type)
				return win;
		}
	}

	return NULL;
}

PidginWindow *
pidgin_conv_window_last_with_type(PurpleConversationType type)
{
	GList *wins, *convs;
	PidginWindow *win;
	PidginConversation *conv;

	if (type == PURPLE_CONV_TYPE_UNKNOWN)
		return NULL;

	for (wins = g_list_last(pidgin_conv_windows_get_list());
	     wins != NULL;
	     wins = wins->prev) {

		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (purple_conversation_get_type(conv->active_conv) == type)
				return win;
		}
	}

	return NULL;
}


/**************************************************************************
 * Conversation placement functions
 **************************************************************************/
typedef struct
{
	char *id;
	char *name;
	PidginConvPlacementFunc fnc;

} ConvPlacementData;

static GList *conv_placement_fncs = NULL;
static PidginConvPlacementFunc place_conv = NULL;

/* This one places conversations in the last made window. */
static void
conv_placement_last_created_win(PidginConversation *conv)
{
	PidginWindow *win;

	GList *l = g_list_last(pidgin_conv_windows_get_list());
	win = l ? l->data : NULL;;

	if (win == NULL) {
		win = pidgin_conv_window_new();

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);
	} else {
		pidgin_conv_window_add_gtkconv(win, conv);
	}
}

/* This one places conversations in the last made window of the same type. */
static void
conv_placement_last_created_win_type(PidginConversation *conv)
{
	PidginWindow *win;

	win = pidgin_conv_window_last_with_type(purple_conversation_get_type(conv->active_conv));

	if (win == NULL) {
		win = pidgin_conv_window_new();

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);
	} else
		pidgin_conv_window_add_gtkconv(win, conv);
}

/* This one places each conversation in its own window. */
static void
conv_placement_new_window(PidginConversation *conv)
{
	PidginWindow *win;

	win = pidgin_conv_window_new();

	pidgin_conv_window_add_gtkconv(win, conv);

	pidgin_conv_window_show(win);
}

static PurpleGroup *
conv_get_group(PidginConversation *conv)
{
	PurpleGroup *group = NULL;

	if (purple_conversation_get_type(conv->active_conv) == PURPLE_CONV_TYPE_IM) {
		PurpleBuddy *buddy;

		buddy = purple_find_buddy(purple_conversation_get_account(conv->active_conv),
		                        purple_conversation_get_name(conv->active_conv));

		if (buddy != NULL)
			group = purple_buddy_get_group(buddy);

	} else if (purple_conversation_get_type(conv->active_conv) == PURPLE_CONV_TYPE_CHAT) {
		PurpleChat *chat;

		chat = purple_blist_find_chat(purple_conversation_get_account(conv->active_conv),
		                            purple_conversation_get_name(conv->active_conv));

		if (chat != NULL)
			group = purple_chat_get_group(chat);
	}

	return group;
}

/*
 * This groups things by, well, group. Buddies from groups will always be
 * grouped together, and a buddy from a group not belonging to any currently
 * open windows will get a new window.
 */
static void
conv_placement_by_group(PidginConversation *conv)
{
	PurpleConversationType type;
	PurpleGroup *group = NULL;
	GList *wl, *cl;

	type = purple_conversation_get_type(conv->active_conv);

	group = conv_get_group(conv);

	/* Go through the list of IMs and find one with this group. */
	for (wl = pidgin_conv_windows_get_list(); wl != NULL; wl = wl->next) {
		PidginWindow *win2;
		PidginConversation *conv2;
		PurpleGroup *group2 = NULL;

		win2 = wl->data;

		for (cl = win2->gtkconvs;
		     cl != NULL;
		     cl = cl->next) {
			conv2 = cl->data;

			group2 = conv_get_group(conv2);

			if (group == group2) {
				pidgin_conv_window_add_gtkconv(win2, conv);

				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

/* This groups things by account.  Otherwise, the same semantics as above */
static void
conv_placement_by_account(PidginConversation *conv)
{
	PurpleConversationType type;
	GList *wins, *convs;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv->active_conv);
	type = purple_conversation_get_type(conv->active_conv);

	/* Go through the list of IMs and find one with this group. */
	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		PidginWindow *win2;
		PidginConversation *conv2;

		win2 = wins->data;

		for (convs = win2->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {
			conv2 = convs->data;

			if (account == purple_conversation_get_account(conv2->active_conv)) {
				pidgin_conv_window_add_gtkconv(win2, conv);
				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

static ConvPlacementData *
get_conv_placement_data(const char *id)
{
	ConvPlacementData *data = NULL;
	GList *n;

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		if (!strcmp(data->id, id))
			return data;
	}

	return NULL;
}

static void
add_conv_placement_fnc(const char *id, const char *name,
                       PidginConvPlacementFunc fnc)
{
	ConvPlacementData *data;

	data = g_new(ConvPlacementData, 1);

	data->id = g_strdup(id);
	data->name = g_strdup(name);
	data->fnc  = fnc;

	conv_placement_fncs = g_list_append(conv_placement_fncs, data);
}

static void
ensure_default_funcs(void)
{
	if (conv_placement_fncs == NULL) {
		add_conv_placement_fnc("last", _("Last created window"),
		                       conv_placement_last_created_win);
		add_conv_placement_fnc("im_chat", _("Separate IM and Chat windows"),
		                       conv_placement_last_created_win_type);
		add_conv_placement_fnc("new", _("New window"),
		                       conv_placement_new_window);
		add_conv_placement_fnc("group", _("By group"),
		                       conv_placement_by_group);
		add_conv_placement_fnc("account", _("By account"),
		                       conv_placement_by_account);
	}
}

GList *
pidgin_conv_placement_get_options(void)
{
	GList *n, *list = NULL;
	ConvPlacementData *data;

	ensure_default_funcs();

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		list = g_list_append(list, data->name);
		list = g_list_append(list, data->id);
	}

	return list;
}


void
pidgin_conv_placement_add_fnc(const char *id, const char *name,
                            PidginConvPlacementFunc fnc)
{
	g_return_if_fail(id   != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fnc  != NULL);

	ensure_default_funcs();

	add_conv_placement_fnc(id, name, fnc);
}

void
pidgin_conv_placement_remove_fnc(const char *id)
{
	ConvPlacementData *data = get_conv_placement_data(id);

	if (data == NULL)
		return;

	conv_placement_fncs = g_list_remove(conv_placement_fncs, data);

	g_free(data->id);
	g_free(data->name);
	g_free(data);
}

const char *
pidgin_conv_placement_get_name(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->name;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_fnc(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->fnc;
}

void
pidgin_conv_placement_set_current_func(PidginConvPlacementFunc func)
{
	g_return_if_fail(func != NULL);

	/* If tabs are enabled, set the function, otherwise, NULL it out. */
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"))
		place_conv = func;
	else
		place_conv = NULL;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_current_func(void)
{
	return place_conv;
}

void
pidgin_conv_placement_place(PidginConversation *gtkconv)
{
	if (place_conv)
		place_conv(gtkconv);
	else
		conv_placement_new_window(gtkconv);
}

gboolean
pidgin_conv_is_hidden(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, FALSE);

	return (gtkconv->win == hidden_convwin);
}


/* Algorithm from http://www.w3.org/TR/AERT#color-contrast */
static gboolean
color_is_visible(GdkColor foreground, GdkColor background, int color_contrast, int brightness_contrast)
{
	gulong fg_brightness;
	gulong bg_brightness;
	gulong br_diff;
	gulong col_diff;
	int fred, fgreen, fblue, bred, bgreen, bblue;

	/* this algorithm expects colors between 0 and 255 for each of red green and blue.
	 * GTK on the other hand has values between 0 and 65535
	 * Err suggested I >> 8, which grabbed the high bits.
	 */

	fred = foreground.red >> 8 ;
	fgreen = foreground.green >> 8 ;
	fblue = foreground.blue >> 8 ;


	bred = background.red >> 8 ;
	bgreen = background.green >> 8 ;
	bblue = background.blue >> 8 ;

	fg_brightness = (fred * 299 + fgreen * 587 + fblue * 114) / 1000;
	bg_brightness = (bred * 299 + bgreen * 587 + bblue * 114) / 1000;
	br_diff = abs(fg_brightness - bg_brightness);

	col_diff = abs(fred - bred) + abs(fgreen - bgreen) + abs(fblue - bblue);

	return ((col_diff > color_contrast) && (br_diff > brightness_contrast));
}


static GdkColor*
generate_nick_colors(guint *color_count, GdkColor background)
{
	guint numcolors = *color_count;
	guint i = 0, j = 0;
	GdkColor *colors = g_new(GdkColor, numcolors);
	GdkColor nick_highlight;
	GdkColor send_color;
	time_t breakout_time;

	gdk_color_parse(HIGHLIGHT_COLOR, &nick_highlight);
	gdk_color_parse(SEND_COLOR, &send_color);

	srand(background.red + background.green + background.blue + 1);

	breakout_time = time(NULL) + 3;

	/* first we look through the list of "good" colors: colors that differ from every other color in the
	 * list.  only some of them will differ from the background color though. lets see if we can find
	 * numcolors of them that do
	 */
	while (i < numcolors && j < NUM_NICK_SEED_COLORS && time(NULL) < breakout_time)
	{
		GdkColor color = nick_seed_colors[j];

		if (color_is_visible(color, background,     MIN_COLOR_CONTRAST,     MIN_BRIGHTNESS_CONTRAST) &&
			color_is_visible(color, nick_highlight, MIN_COLOR_CONTRAST / 2, 0) &&
			color_is_visible(color, send_color,     MIN_COLOR_CONTRAST / 4, 0))
		{
			colors[i] = color;
			i++;
		}
		j++;
	}

	/* we might not have found numcolors in the last loop.  if we did, we'll never enter this one.
	 * if we did not, lets just find some colors that don't conflict with the background.  its
	 * expensive to find colors that not only don't conflict with the background, but also do not
	 * conflict with each other.
	 */
	while(i < numcolors && time(NULL) < breakout_time)
	{
		GdkColor color = { 0, rand() % 65536, rand() % 65536, rand() % 65536 };

		if (color_is_visible(color, background,     MIN_COLOR_CONTRAST,     MIN_BRIGHTNESS_CONTRAST) &&
			color_is_visible(color, nick_highlight, MIN_COLOR_CONTRAST / 2, 0) &&
			color_is_visible(color, send_color,     MIN_COLOR_CONTRAST / 4, 0))
		{
			colors[i] = color;
			i++;
		}
	}

	if (i < numcolors) {
		GdkColor *c = colors;
		purple_debug_warning("gtkconv", "Unable to generate enough random colors before timeout. %u colors found.\n", i);
		colors = g_memdup(c, i * sizeof(GdkColor));
		g_free(c);
		*color_count = i;
	}

	return colors;
}