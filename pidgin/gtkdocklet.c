/*
 * System tray icon (aka docklet) plugin for Purple
 *
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "internal.h"
#include "pidgin.h"

#include "core.h"
#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "sound.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksound.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkdocklet.h"
#include "gtkdialogs.h"

#ifndef DOCKLET_TOOLTIP_LINE_LIMIT
#define DOCKLET_TOOLTIP_LINE_LIMIT 5
#endif

/* globals */
static struct docklet_ui_ops *ui_ops = NULL;
static DockletStatus status = DOCKLET_STATUS_OFFLINE;
static gboolean enable_join_chat = FALSE;
static guint docklet_blinking_timer = 0;
static gboolean visible = FALSE;
static gboolean visibility_manager = FALSE;

/**************************************************************************
 * docklet status and utility functions
 **************************************************************************/
static gboolean
docklet_blink_icon()
{
	static gboolean blinked = FALSE;
	gboolean ret = FALSE; /* by default, don't keep blinking */

	blinked = !blinked;

	switch (status) {
		case DOCKLET_STATUS_PENDING:
			if (blinked) {
				if (ui_ops && ui_ops->blank_icon)
					ui_ops->blank_icon();
			} else {
				if (ui_ops && ui_ops->update_icon)
					ui_ops->update_icon(status);
			}
			ret = TRUE; /* keep blinking */
			break;
		default:
			docklet_blinking_timer = 0;
			blinked = FALSE;
			break;
	}

	return ret;
}

static GList *
get_pending_list(guint max)
{
	GList *l_im = NULL;
	GList *l_chat = NULL;

	l_im = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_IM,
						       PIDGIN_UNSEEN_TEXT,
						       FALSE, max);

	l_chat = pidgin_conversations_find_unseen_list(PURPLE_CONV_TYPE_CHAT,
		 					 PIDGIN_UNSEEN_NICK,
							 FALSE, max);

	if (l_im != NULL && l_chat != NULL)
		return g_list_concat(l_im, l_chat);
	else if (l_im != NULL)
		return l_im;
	else
		return l_chat;
}

static gboolean
docklet_update_status()
{
	GList *convs, *l;
	int count;
	PurpleSavedStatus *saved_status;
	PurpleStatusPrimitive prim;
	DockletStatus newstatus = DOCKLET_STATUS_OFFLINE;
	gboolean pending = FALSE, connecting = FALSE;

	/* determine if any ims have unseen messages */
	convs = get_pending_list(DOCKLET_TOOLTIP_LINE_LIMIT);

	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "pending")) {
		if (convs && ui_ops->create && !visible) {
			g_list_free(convs);
			ui_ops->create();
			return FALSE;
		} else if (!convs && ui_ops->destroy && visible) {
			ui_ops->destroy();
			return FALSE;
		}
	}

	if (!visible) {
		g_list_free(convs);
		return FALSE;
	}

	if (convs != NULL) {
		pending = TRUE;

		/* set tooltip if messages are pending */
		if (ui_ops->set_tooltip) {
			GString *tooltip_text = g_string_new("");
			for (l = convs, count = 0 ; l != NULL ; l = l->next, count++) {
				if (PIDGIN_IS_PIDGIN_CONVERSATION(l->data)) {
					PidginConversation *gtkconv = PIDGIN_CONVERSATION((PurpleConversation *)l->data);
					if (count == DOCKLET_TOOLTIP_LINE_LIMIT - 1)
						g_string_append(tooltip_text, _("Right-click for more unread messages...\n"));
					else
						g_string_append_printf(tooltip_text,
							ngettext("%d unread message from %s\n", "%d unread messages from %s\n", gtkconv->unseen_count),
							gtkconv->unseen_count,
							gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
				}
			}

			/* get rid of the last newline */
			if (tooltip_text->len > 0)
				tooltip_text = g_string_truncate(tooltip_text, tooltip_text->len - 1);

			ui_ops->set_tooltip(tooltip_text->str);

			g_string_free(tooltip_text, TRUE);
		}

		g_list_free(convs);

	} else if (ui_ops->set_tooltip) {
		ui_ops->set_tooltip(PIDGIN_NAME);
	}

	for(l = purple_accounts_get_all(); l != NULL; l = l->next) {

		PurpleAccount *account = (PurpleAccount*)l->data;
		PurpleStatus *account_status;

		if (!purple_account_get_enabled(account, PIDGIN_UI))
			continue;

		if (purple_account_is_disconnected(account))
			continue;

		account_status = purple_account_get_active_status(account);
		if (purple_account_is_connecting(account))
			connecting = TRUE;
	}

	saved_status = purple_savedstatus_get_current();
	prim = purple_savedstatus_get_type(saved_status);
	if (pending)
		newstatus = DOCKLET_STATUS_PENDING;
	else if (connecting)
		newstatus = DOCKLET_STATUS_CONNECTING;
	else if (prim == PURPLE_STATUS_UNAVAILABLE)
		newstatus = DOCKLET_STATUS_BUSY;
	else if (prim == PURPLE_STATUS_AWAY)
		newstatus = DOCKLET_STATUS_AWAY;
	else if (prim == PURPLE_STATUS_EXTENDED_AWAY)
		newstatus = DOCKLET_STATUS_XA;
	else if (prim == PURPLE_STATUS_OFFLINE)
		newstatus = DOCKLET_STATUS_OFFLINE;
	else
		newstatus = DOCKLET_STATUS_AVAILABLE;

	/* update the icon if we changed status */
	if (status != newstatus) {
		status = newstatus;

		if (ui_ops && ui_ops->update_icon)
			ui_ops->update_icon(status);

		/* and schedule the blinker function if messages are pending */
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/blink") &&
		    status == DOCKLET_STATUS_PENDING
		    && docklet_blinking_timer == 0) {
			docklet_blinking_timer = g_timeout_add(500, docklet_blink_icon, NULL);
		}
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

static gboolean
online_account_supports_chat()
{
	GList *c = NULL;
	c = purple_connections_get_all();

	while(c != NULL) {
		PurpleConnection *gc = c->data;
		PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
		if (prpl_info != NULL && prpl_info->chat_info != NULL)
			return TRUE;
		c = c->next;
	}

	return FALSE;
}

/**************************************************************************
 * callbacks and signal handlers
 **************************************************************************/
#if 0
static void
pidgin_quit_cb()
{
	/* TODO: confirm quit while pending */
}
#endif

static void
docklet_update_status_cb(void *data)
{
	docklet_update_status();
}

static void
docklet_conv_updated_cb(PurpleConversation *conv, PurpleConvUpdateType type)
{
	if (type == PURPLE_CONV_UPDATE_UNSEEN)
		docklet_update_status();
}

static void
docklet_signed_on_cb(PurpleConnection *gc)
{
	if (!enable_join_chat) {
		if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
			enable_join_chat = TRUE;
	}
	docklet_update_status();
}

static void
docklet_signed_off_cb(PurpleConnection *gc)
{
	if (enable_join_chat) {
		if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL)
			enable_join_chat = online_account_supports_chat();
	}
	docklet_update_status();
}

static void
docklet_show_pref_changed_cb(const char *name, PurplePrefType type,
			     gconstpointer value, gpointer data)
{
	const char *val = value;
	if (!strcmp(val, "always")) {
		if (ui_ops->create) {
			if (!visible)
				ui_ops->create();
			else if (!visibility_manager) {
				pidgin_blist_visibility_manager_add();
				visibility_manager = TRUE;
			}
		}
	} else if (!strcmp(val, "never")) {
		if (visible && ui_ops->destroy)
			ui_ops->destroy();
	} else {
		if (visibility_manager) {
			pidgin_blist_visibility_manager_remove();
			visibility_manager = FALSE;
		}
		docklet_update_status();
	}

}

/**************************************************************************
 * docklet pop-up menu
 **************************************************************************/
static void
docklet_toggle_mute(GtkWidget *toggle, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", GTK_CHECK_MENU_ITEM(toggle)->active);
}

static void
docklet_toggle_blink(GtkWidget *toggle, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/blink", GTK_CHECK_MENU_ITEM(toggle)->active);
}

static void
docklet_toggle_blist(GtkWidget *toggle, void *data)
{
	purple_blist_set_visible(GTK_CHECK_MENU_ITEM(toggle)->active);
}

#ifdef _WIN32
/* This is a workaround for a bug in windows GTK+. Clicking outside of the
   menu does not get rid of it, so instead we get rid of it as soon as the
   pointer leaves the menu. */
static gboolean
hide_docklet_menu(gpointer data)
{
	if (data != NULL) {
		gtk_menu_popdown(GTK_MENU(data));
	}
	return FALSE;
}

static gboolean
docklet_menu_leave_enter(GtkWidget *menu, GdkEventCrossing *event, void *data)
{
	static guint hide_docklet_timer = 0;
	if (event->type == GDK_LEAVE_NOTIFY && event->detail == GDK_NOTIFY_ANCESTOR) {
		purple_debug(PURPLE_DEBUG_INFO, "docklet", "menu leave-notify-event\n");
		/* Add some slop so that the menu doesn't annoyingly disappear when mousing around */
		if (hide_docklet_timer == 0) {
			hide_docklet_timer = purple_timeout_add(500,
					hide_docklet_menu, menu);
		}
	} else if (event->type == GDK_ENTER_NOTIFY && event->detail == GDK_NOTIFY_ANCESTOR) {
		purple_debug(PURPLE_DEBUG_INFO, "docklet", "menu enter-notify-event\n");
		if (hide_docklet_timer != 0) {
			/* Cancel the hiding if we reenter */

			purple_timeout_remove(hide_docklet_timer);
			hide_docklet_timer = 0;
		}
	}
	return FALSE;
}
#endif

static void
show_custom_status_editor_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	PurpleSavedStatus *saved_status;
	saved_status = purple_savedstatus_get_current();

	if (purple_savedstatus_get_type(saved_status) == PURPLE_STATUS_AVAILABLE)
		saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AWAY);

	pidgin_status_editor_show(FALSE,
		purple_savedstatus_is_transient(saved_status) ? saved_status : NULL);
}

static void
activate_status_primitive_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	PurpleStatusPrimitive primitive;
	PurpleSavedStatus *saved_status;

	primitive = GPOINTER_TO_INT(user_data);

	/* Try to lookup an already existing transient saved status */
	saved_status = purple_savedstatus_find_transient_by_type_and_message(primitive, NULL);

	/* Create a new transient saved status if we weren't able to find one */
	if (saved_status == NULL)
		saved_status = purple_savedstatus_new(NULL, primitive);

	/* Set the status for each account */
	purple_savedstatus_activate(saved_status);
}

static void
activate_saved_status_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	time_t creation_time;
	PurpleSavedStatus *saved_status;

	creation_time = GPOINTER_TO_INT(user_data);
	saved_status = purple_savedstatus_find_by_creation_time(creation_time);
	if (saved_status != NULL)
		purple_savedstatus_activate(saved_status);
}

static GtkWidget *
new_menu_item_with_status_icon(GtkWidget *menu, const char *str, PurpleStatusPrimitive primitive, GtkSignalFunc sf, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	menuitem = gtk_image_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	pixbuf = pidgin_create_status_icon(primitive, menu, PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

static GtkWidget *
docklet_status_submenu()
{
	GtkWidget *submenu, *menuitem;
	GList *popular_statuses, *cur;

	submenu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_label(_("Change Status"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

	new_menu_item_with_status_icon(submenu, _("Available"),
		PURPLE_STATUS_AVAILABLE, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(PURPLE_STATUS_AVAILABLE), 0, 0, NULL);

	new_menu_item_with_status_icon(submenu, _("Away"),
		PURPLE_STATUS_AWAY, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(PURPLE_STATUS_AWAY), 0, 0, NULL);

	new_menu_item_with_status_icon(submenu, _("Invisible"),
		PURPLE_STATUS_INVISIBLE, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(PURPLE_STATUS_INVISIBLE), 0, 0, NULL);

	new_menu_item_with_status_icon(submenu, _("Offline"),
		PURPLE_STATUS_OFFLINE, G_CALLBACK(activate_status_primitive_cb),
		GINT_TO_POINTER(PURPLE_STATUS_OFFLINE), 0, 0, NULL);

	popular_statuses = purple_savedstatuses_get_popular(6);
	if (popular_statuses != NULL)
		pidgin_separator(submenu);
	for (cur = popular_statuses; cur != NULL; cur = cur->next)
	{
		PurpleSavedStatus *saved_status = cur->data;
		time_t creation_time = purple_savedstatus_get_creation_time(saved_status);
		new_menu_item_with_status_icon(submenu,
			purple_savedstatus_get_title(saved_status),
			purple_savedstatus_get_type(saved_status), G_CALLBACK(activate_saved_status_cb),
			GINT_TO_POINTER(creation_time), 0, 0, NULL);
	}
	g_list_free(popular_statuses);

	pidgin_separator(submenu);

	new_menu_item_with_status_icon(submenu, _("New..."), PURPLE_STATUS_AVAILABLE, G_CALLBACK(show_custom_status_editor_cb), NULL, 0, 0, NULL);
	new_menu_item_with_status_icon(submenu, _("Saved..."), PURPLE_STATUS_AVAILABLE, G_CALLBACK(pidgin_status_window_show), NULL, 0, 0, NULL);

	return menuitem;
}

static void
docklet_menu() {
	static GtkWidget *menu = NULL;
	GtkWidget *menuitem;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	menuitem = gtk_check_menu_item_new_with_label(_("Show Buddy List"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_blist), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_menu_item_new_with_label(_("Unread Messages"));

	if (status == DOCKLET_STATUS_PENDING) {
		GtkWidget *submenu = gtk_menu_new();
		GList *l = get_pending_list(0);
		if (l == NULL) {
			gtk_widget_set_sensitive(menuitem, FALSE);
			purple_debug_warning("docklet",
				"status indicates messages pending, but no conversations with unseen messages were found.");
		} else {
			pidgin_conversations_fill_menu(submenu, l);
			g_list_free(l);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		}
	} else {
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	pidgin_separator(menu);

	menuitem = pidgin_new_item_from_stock(menu, _("New Message..."), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, G_CALLBACK(pidgin_dialogs_im), NULL, 0, 0, NULL);
	if (status == DOCKLET_STATUS_OFFLINE)
		gtk_widget_set_sensitive(menuitem, FALSE);

	menuitem = docklet_status_submenu();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	pidgin_separator(menu);

	pidgin_new_item_from_stock(menu, _("Accounts"), NULL, G_CALLBACK(pidgin_accounts_window_show), NULL, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("Plugins"), PIDGIN_STOCK_TOOLBAR_PLUGINS, G_CALLBACK(pidgin_plugin_dialog_show), NULL, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("Preferences"), GTK_STOCK_PREFERENCES, G_CALLBACK(pidgin_prefs_show), NULL, 0, 0, NULL);

	pidgin_separator(menu);

	menuitem = gtk_check_menu_item_new_with_label(_("Mute Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute"));
	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), "none"))
		gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_check_menu_item_new_with_label(_("Blink on new message"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/blink"));
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_blink), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	pidgin_separator(menu);

	/* TODO: need a submenu to change status, this needs to "link"
	 * to the status in the buddy list gtkstatusbox
	 */

	pidgin_new_item_from_stock(menu, _("Quit"), GTK_STOCK_QUIT, G_CALLBACK(purple_core_quit), NULL, 0, 0, NULL);

#ifdef _WIN32
	g_signal_connect(menu, "leave-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
	g_signal_connect(menu, "enter-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
#endif
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       ui_ops->position_menu,
		       NULL, 0, gtk_get_current_event_time());
}

/**************************************************************************
 * public api for ui_ops
 **************************************************************************/
void
pidgin_docklet_clicked(int button_type)
{
	switch (button_type) {
		case 1:
			if (status == DOCKLET_STATUS_PENDING) {
				GList *l = get_pending_list(1);
				if (l != NULL) {
					purple_conversation_present((PurpleConversation *)l->data);
					g_list_free(l);
				}
			} else {
				pidgin_blist_toggle_visibility();
			}
			break;
		case 3:
			docklet_menu();
			break;
	}
}

void
pidgin_docklet_embedded()
{
	if (!visibility_manager
	    && strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "pending")) {
		pidgin_blist_visibility_manager_add();
		visibility_manager = TRUE;
	}
	visible = TRUE;
	docklet_update_status();
	if (ui_ops && ui_ops->update_icon)
		ui_ops->update_icon(status);
}

void
pidgin_docklet_remove()
{
	if (visible) {
		if (visibility_manager) {
			pidgin_blist_visibility_manager_remove();
			visibility_manager = FALSE;
		}
		if (docklet_blinking_timer) {
			g_source_remove(docklet_blinking_timer);
			docklet_blinking_timer = 0;
		}
		visible = FALSE;
		status = DOCKLET_STATUS_OFFLINE;
	}
}

void
pidgin_docklet_set_ui_ops(struct docklet_ui_ops *ops)
{
	ui_ops = ops;
}

void*
pidgin_docklet_get_handle()
{
	static int i;
	return &i;
}

void
pidgin_docklet_init()
{
	void *conn_handle = purple_connections_get_handle();
	void *conv_handle = purple_conversations_get_handle();
	void *accounts_handle = purple_accounts_get_handle();
	void *docklet_handle = pidgin_docklet_get_handle();

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/docklet");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/blink", FALSE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/docklet/show", "always");
	purple_prefs_connect_callback(docklet_handle, PIDGIN_PREFS_ROOT "/docklet/show",
				    docklet_show_pref_changed_cb, NULL);

	docklet_ui_init();
	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "always") && ui_ops && ui_ops->create)
		ui_ops->create();

	purple_signal_connect(conn_handle, "signed-on",
			    docklet_handle, PURPLE_CALLBACK(docklet_signed_on_cb), NULL);
	purple_signal_connect(conn_handle, "signed-off",
			    docklet_handle, PURPLE_CALLBACK(docklet_signed_off_cb), NULL);
	purple_signal_connect(accounts_handle, "account-status-changed",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "received-im-msg",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "conversation-created",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "deleting-conversation",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "conversation-updated",
			    docklet_handle, PURPLE_CALLBACK(docklet_conv_updated_cb), NULL);
#if 0
	purple_signal_connect(purple_get_core(), "quitting",
			    docklet_handle, PURPLE_CALLBACK(purple_quit_cb), NULL);
#endif

	enable_join_chat = online_account_supports_chat();
}

void
pidgin_docklet_uninit()
{
	if (visible && ui_ops && ui_ops->destroy)
		ui_ops->destroy();
}