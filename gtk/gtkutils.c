/**
 * @file gtkutils.h GTK+ utility functions
 * @ingroup gtkui
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
#include "internal.h"
#include "gtkgaim.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#else
# ifdef small
#  undef small
# endif
#endif /*_WIN32*/

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
# ifdef _WIN32
#  include "wspell.h"
# endif
#endif

#include <gdk/gdkkeysyms.h>

#include "conversation.h"
#include "debug.h"
#include "desktopitem.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gaimstock.h"
#include "gtkthemes.h"
#include "gtkutils.h"

static guint accels_save_timer = 0;

static gboolean
url_clicked_idle_cb(gpointer data)
{
	gaim_notify_uri(NULL, data);
	g_free(data);
	return FALSE;
}

static void
url_clicked_cb(GtkWidget *w, const char *uri)
{
	g_idle_add(url_clicked_idle_cb, g_strdup(uri));
}

static GtkIMHtmlFuncs gtkimhtml_cbs = {
	(GtkIMHtmlGetImageFunc)gaim_imgstore_get,
	(GtkIMHtmlGetImageDataFunc)gaim_imgstore_get_data,
	(GtkIMHtmlGetImageSizeFunc)gaim_imgstore_get_size,
	(GtkIMHtmlGetImageFilenameFunc)gaim_imgstore_get_filename,
	gaim_imgstore_ref,
	gaim_imgstore_unref,
};

void
gaim_setup_imhtml(GtkWidget *imhtml)
{
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	g_signal_connect(G_OBJECT(imhtml), "url_clicked",
					 G_CALLBACK(url_clicked_cb), NULL);

	gaim_gtkthemes_smiley_themeize(imhtml);

	gtk_imhtml_set_funcs(GTK_IMHTML(imhtml), &gtkimhtml_cbs);
}

GtkWidget *
gaim_gtk_create_imhtml(gboolean editable, GtkWidget **imhtml_ret, GtkWidget **toolbar_ret, GtkWidget **sw_ret)
{
	GtkWidget *frame;
	GtkWidget *imhtml;
	GtkWidget *sep;
	GtkWidget *sw;
	GtkWidget *toolbar = NULL;
	GtkWidget *vbox;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	if (editable) {
		toolbar = gtk_imhtmltoolbar_new();
		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
		gtk_widget_show(toolbar);

		sep = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
		gtk_widget_show(sep);
	}

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_imhtml_set_editable(GTK_IMHTML(imhtml), editable);
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml), GTK_IMHTML_ALL ^ GTK_IMHTML_IMAGE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD_CHAR);
#ifdef USE_GTKSPELL
	if (editable && gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(imhtml));
#endif
	gtk_widget_show(imhtml);

	if (editable) {
		gtk_imhtmltoolbar_attach(GTK_IMHTMLTOOLBAR(toolbar), imhtml);
		gtk_imhtmltoolbar_associate_smileys(GTK_IMHTMLTOOLBAR(toolbar), "default");
	}
	gaim_setup_imhtml(imhtml);

	gtk_container_add(GTK_CONTAINER(sw), imhtml);

	if (imhtml_ret != NULL)
		*imhtml_ret = imhtml;

	if (editable && (toolbar_ret != NULL))
		*toolbar_ret = toolbar;

	if (sw_ret != NULL)
		*sw_ret = sw;

	return frame;
}

void
gaim_gtk_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
									  (*text != '\0'));
}

void
gaim_gtk_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity;

	if (to_toggle == NULL)
		return;

	sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
gaim_gtk_toggle_sensitive_array(GtkWidget *w, GPtrArray *data)
{
	gboolean sensitivity;
	gpointer element;
	int i;

	for (i=0; i < data->len; i++) {
		element = g_ptr_array_index(data,i);
		if (element == NULL)
			continue;

		sensitivity = GTK_WIDGET_IS_SENSITIVE(element);

		gtk_widget_set_sensitive(element, !sensitivity);
	}
}

void
gaim_gtk_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle)
{
	if (to_toggle == NULL)
		return;

	if (GTK_WIDGET_VISIBLE(to_toggle))
		gtk_widget_hide(to_toggle);
	else
		gtk_widget_show(to_toggle);
}

void gaim_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

GtkWidget *gaim_new_item(GtkWidget *menu, const char *str)
{
	GtkWidget *menuitem;
	GtkWidget *label;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	label = gtk_label_new(str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);
/* FIXME: Go back and fix this
	gtk_widget_add_accelerator(menuitem, "activate", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
*/
	gaim_set_accessible_label (menuitem, label);
	return menuitem;
}

GtkWidget *gaim_new_check_item(GtkWidget *menu, const char *str,
		GtkSignalFunc sf, gpointer data, gboolean checked)
{
	GtkWidget *menuitem;
	menuitem = gtk_check_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), checked);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_pixbuf_toolbar_button_from_stock(const char *icon)
{
	GtkWidget *button, *image, *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	return button;
}

GtkWidget *
gaim_pixbuf_button_from_stock(const char *text, const char *icon,
							  GaimButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox = NULL;

	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL) {
		bbox = gtk_hbox_new(FALSE, 0);
		ibox = gtk_hbox_new(FALSE, 0);
		if (text)
			lbox = gtk_hbox_new(FALSE, 0);
	} else {
		bbox = gtk_vbox_new(FALSE, 0);
		ibox = gtk_vbox_new(FALSE, 0);
		if (text)
			lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add(GTK_CONTAINER(button), bbox);

	if (icon) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, TRUE, 0);
	}

	if (text) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, TRUE, 0);
		gaim_set_accessible_label (button, label);
	}

	gtk_widget_show_all(bbox);

	return button;
}


GtkWidget *gaim_new_item_from_stock(GtkWidget *menu, const char *str, const char *icon, GtkSignalFunc sf, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	/*
	GtkWidget *hbox;
	GtkWidget *label;
	*/
	GtkWidget *image;

	if (icon == NULL)
		menuitem = gtk_menu_item_new_with_mnemonic(str);
	else
		menuitem = gtk_image_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (sf)
		g_signal_connect(G_OBJECT(menuitem), "activate", sf, data);

	if (icon != NULL) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	}
/* FIXME: this isn't right
	if (mod) {
		label = gtk_label_new(mod);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}
*/
/*
	if (accel_key) {
		gtk_widget_add_accelerator(menuitem, "activate", accel, accel_key,
					   accel_mods, GTK_ACCEL_LOCKED);
	}
*/

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
gaim_gtk_make_frame(GtkWidget *parent, const char *title)
{
	GtkWidget *vbox, *label, *hbox;
	char *labeltitle;

	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(NULL);

	labeltitle = g_strdup_printf("<span weight=\"bold\">%s</span>", title);
	gtk_label_set_markup(GTK_LABEL(label), labeltitle);
	g_free(labeltitle);

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gaim_set_accessible_label (vbox, label);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	return vbox;
}

static void
protocol_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	const char *protocol;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	protocol = g_object_get_data(G_OBJECT(item), "protocol");
	user_data = (g_object_get_data(G_OBJECT(optmenu), "user_data"));

	if (cb != NULL)
		((void (*)(GtkWidget *, const char *, gpointer))cb)(item, protocol,
															user_data);
}

GtkWidget *
gaim_gtk_protocol_option_menu_new(const char *id, GCallback cb,
								  gpointer user_data)
{
	GaimPluginProtocolInfo *prpl_info;
	GaimPlugin *plugin;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = gaim_plugins_get_protocols(), i = 0;
		 p != NULL;
		 p = p->next, i++) {

		plugin = (GaimPlugin *)p->data;
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		proto_name = prpl_info->list_icon(NULL, NULL);
		g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

		filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
									"default", buf, NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);

		if (pixbuf != NULL) {
			/* Scale and insert the image */
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
											GDK_INTERP_BILINEAR);
			image = gtk_image_new_from_pixbuf(scale);

			g_object_unref(G_OBJECT(pixbuf));
			g_object_unref(G_OBJECT(scale));
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		/* Create the label. */
		label = gtk_label_new(plugin->info->name);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "protocol", plugin->info->id);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (id != NULL && !strcmp(plugin->info->id, id))
			selected_index = i;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);

	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(protocol_menu_cb), cb);

	g_object_unref(sg);

	return optmenu;
}

GaimAccount *
gaim_gtk_account_option_menu_get_selected(GtkWidget *optmenu)
{
	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	GtkWidget *item = gtk_menu_get_active(GTK_MENU(menu));
	return g_object_get_data(G_OBJECT(item), "account");
}

static void
account_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *menu;
	GtkWidget *item;
	GaimAccount *account;
	gpointer user_data;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));

	account   = g_object_get_data(G_OBJECT(item),    "account");
	user_data = g_object_get_data(G_OBJECT(optmenu), "user_data");

	if (cb != NULL)
		((void (*)(GtkWidget *, GaimAccount *, gpointer))cb)(item, account,
															 user_data);
}

static void
create_account_menu(GtkWidget *optmenu, GaimAccount *default_account,
					GaimFilterAccountFunc filter_func, gboolean show_all)
{
	GaimAccount *account;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	GtkWidget *hbox;
	GtkWidget *label;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;
	GList *list;
	GList *p;
	GtkSizeGroup *sg;
	char *filename;
	const char *proto_name;
	char buf[256];
	int i, selected_index = -1;

	if (show_all)
		list = gaim_accounts_get_all();
	else
		list = gaim_connections_get_all();

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		GaimPluginProtocolInfo *prpl_info = NULL;
		GaimPlugin *plugin;

		if (show_all)
			account = (GaimAccount *)p->data;
		else {
			GaimConnection *gc = (GaimConnection *)p->data;

			account = gaim_connection_get_account(gc);
		}

		if (filter_func && !filter_func(account)) {
			i--;
			continue;
		}

		plugin = gaim_find_prpl(gaim_account_get_protocol_id(account));

		if (plugin != NULL)
			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		/* Create the item. */
		item = gtk_menu_item_new();

		/* Create the hbox. */
		hbox = gtk_hbox_new(FALSE, 4);
		gtk_container_add(GTK_CONTAINER(item), hbox);
		gtk_widget_show(hbox);

		/* Load the image. */
		if (prpl_info != NULL) {
			proto_name = prpl_info->list_icon(account, NULL);
			g_snprintf(buf, sizeof(buf), "%s.png", proto_name);

			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
			                            "default", buf, NULL);
			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			if (pixbuf != NULL) {
				/* Scale and insert the image */
				scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
				                                GDK_INTERP_BILINEAR);

				if (gaim_account_is_disconnected(account) && show_all &&
						gaim_connections_get_all())
					gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);

				image = gtk_image_new_from_pixbuf(scale);

				g_object_unref(G_OBJECT(pixbuf));
				g_object_unref(G_OBJECT(scale));
			}
			else
				image = gtk_image_new();
		}
		else
			image = gtk_image_new();

		gtk_size_group_add_widget(sg, image);

		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);

		if (gaim_account_get_alias(account)) {
			g_snprintf(buf, sizeof(buf), "%s (%s) (%s)",
					   gaim_account_get_username(account),
					   gaim_account_get_alias(account),
					   gaim_account_get_protocol_name(account));
		} else {
			g_snprintf(buf, sizeof(buf), "%s (%s)",
					   gaim_account_get_username(account),
					   gaim_account_get_protocol_name(account));
		}

		/* Create the label. */
		label = gtk_label_new(buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		g_object_set_data(G_OBJECT(item), "account", account);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
		gaim_set_accessible_label (item, label);

		if (default_account != NULL && account == default_account)
			selected_index = i;
	}

	g_object_unref(sg);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);

	/* Set the place we should be at. */
	if (selected_index != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), selected_index);
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean show_all;
	GaimAccount *account;
	GaimFilterAccountFunc filter_func;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));
	account = g_object_get_data(G_OBJECT(item), "account");

	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu),
												 "show_all"));

	filter_func = g_object_get_data(G_OBJECT(optmenu),
										   "filter_func");

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	create_account_menu(optmenu, account, filter_func, show_all);
}

static void
account_menu_sign_on_off_cb(GaimConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(GaimAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event,
						  void *user_data)
{
	gaim_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

void
gaim_gtk_account_option_menu_set_selected(GtkWidget *optmenu, GaimAccount *account)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean show_all;
	GaimAccount *curaccount;
	GaimFilterAccountFunc filter_func;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	item = gtk_menu_get_active(GTK_MENU(menu));
	curaccount = g_object_get_data(G_OBJECT(item), "account");

	if (account == curaccount)
		return;

	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu),
												 "show_all"));

	filter_func = g_object_get_data(G_OBJECT(optmenu),
										   "filter_func");

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	create_account_menu(optmenu, account, filter_func, show_all);
}

GtkWidget *
gaim_gtk_account_option_menu_new(GaimAccount *default_account,
								 gboolean show_all, GCallback cb,
								 GaimFilterAccountFunc filter_func,
								 gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
					 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the gaim sign on/off event callbacks. */
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						optmenu, GAIM_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-added",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
						optmenu, GAIM_CALLBACK(account_menu_added_removed_cb),
						optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all", GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "filter_func",
					  filter_func);

	/* Create and set the actual menu. */
	create_account_menu(optmenu, default_account, filter_func, show_all);

	/* And now the last callback. */
	g_signal_connect(G_OBJECT(optmenu), "changed",
					 G_CALLBACK(account_menu_cb), cb);

	return optmenu;
}

gboolean
gaim_gtk_check_if_dir(const char *path, GtkFileSelection *filesel)
{
	char *dirname;

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != G_DIR_SEPARATOR) {
			dirname = g_strconcat(path, G_DIR_SEPARATOR_S, NULL);
		} else {
			dirname = g_strdup(path);
		}
		gtk_file_selection_set_filename(filesel, dirname);
		g_free(dirname);
		return TRUE;
	}

	return FALSE;
}

void
gaim_gtk_setup_gtkspell(GtkTextView *textview)
{
#ifdef USE_GTKSPELL
	GError *error = NULL;
	char *locale = NULL;

	g_return_if_fail(textview != NULL);
	g_return_if_fail(GTK_IS_TEXT_VIEW(textview));

	if (gtkspell_new_attach(textview, locale, &error) == NULL && error)
	{
		gaim_debug_warning("gtkspell", "Failed to setup GtkSpell: %s\n",
						   error->message);
		g_error_free(error);
	}
#endif /* USE_GTKSPELL */
}

void
gaim_gtk_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
														 GdkModifierType arg2, GClosure *arg3,
														 gpointer data)
{
	gaim_debug(GAIM_DEBUG_MISC, "accels", "accel changed, scheduling save.\n");

	if (!accels_save_timer)
		accels_save_timer = g_timeout_add(5000, gaim_gtk_save_accels, NULL);
}

gboolean
gaim_gtk_save_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gaim_debug(GAIM_DEBUG_MISC, "accels", "saving accels to %s\n", filename);
	gtk_accel_map_save(filename);
	g_free(filename);

	accels_save_timer = 0;
	return FALSE;
}

void
gaim_gtk_load_accels()
{
	char *filename = NULL;

	filename = g_build_filename(gaim_user_dir(), G_DIR_SEPARATOR_S,
															"accels", NULL);
	gtk_accel_map_load(filename);
	g_free(filename);
}

gboolean
gaim_gtk_parse_x_im_contact(const char *msg, gboolean all_accounts,
							GaimAccount **ret_account, char **ret_protocol,
							char **ret_username, char **ret_alias)
{
	char *protocol = NULL;
	char *username = NULL;
	char *alias    = NULL;
	char *str;
	char *c, *s;
	gboolean valid;

	g_return_val_if_fail(msg          != NULL, FALSE);
	g_return_val_if_fail(ret_protocol != NULL, FALSE);
	g_return_val_if_fail(ret_username != NULL, FALSE);

	s = str = g_strdup(msg);

	while (*s != '\r' && *s != '\n' && *s != '\0')
	{
		char *key, *value;

		key = s;

		/* Grab the key */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ' ')
			s++;

		if (*s == '\r') s++;

		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		/* Clear past any whitespace */
		while (*s != '\0' && *s == ' ')
			s++;

		/* Now let's grab until the end of the line. */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		if ((c = strchr(key, ':')) != NULL)
		{
			if (!g_ascii_strcasecmp(key, "X-IM-Username:"))
				username = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Protocol:"))
				protocol = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Alias:"))
				alias = g_strdup(value);
		}
	}

	if (username != NULL && protocol != NULL)
	{
		valid = TRUE;

		*ret_username = username;
		*ret_protocol = protocol;

		if (ret_alias != NULL)
			*ret_alias = alias;

		/* Check for a compatible account. */
		if (ret_account != NULL)
		{
			GList *list;
			GaimAccount *account = NULL;
			GList *l;
			const char *protoname;

			if (all_accounts)
				list = gaim_accounts_get_all();
			else
				list = gaim_connections_get_all();

			for (l = list; l != NULL; l = l->next)
			{
				GaimConnection *gc;
				GaimPluginProtocolInfo *prpl_info = NULL;
				GaimPlugin *plugin;

				if (all_accounts)
				{
					account = (GaimAccount *)l->data;

					plugin = gaim_plugins_find_with_id(
						gaim_account_get_protocol_id(account));

					if (plugin == NULL)
					{
						account = NULL;

						continue;
					}

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
				}
				else
				{
					gc = (GaimConnection *)l->data;
					account = gaim_connection_get_account(gc);

					prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
				}

				protoname = prpl_info->list_icon(account, NULL);

				if (!strcmp(protoname, protocol))
					break;

				account = NULL;
			}

			/* Special case for AIM and ICQ */
			if (account == NULL && (!strcmp(protocol, "aim") ||
									!strcmp(protocol, "icq")))
			{
				for (l = list; l != NULL; l = l->next)
				{
					GaimConnection *gc;
					GaimPluginProtocolInfo *prpl_info = NULL;
					GaimPlugin *plugin;

					if (all_accounts)
					{
						account = (GaimAccount *)l->data;

						plugin = gaim_plugins_find_with_id(
							gaim_account_get_protocol_id(account));

						if (plugin == NULL)
						{
							account = NULL;

							continue;
						}

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
					}
					else
					{
						gc = (GaimConnection *)l->data;
						account = gaim_connection_get_account(gc);

						prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
					}

					protoname = prpl_info->list_icon(account, NULL);

					if (!strcmp(protoname, "aim") || !strcmp(protoname, "icq"))
						break;

					account = NULL;
				}
			}

			*ret_account = account;
		}
	}
	else
	{
		valid = FALSE;

		g_free(username);
		g_free(protocol);
		g_free(alias);
	}

	g_free(str);

	return valid;
}

void
gaim_set_accessible_label (GtkWidget *w, GtkWidget *l)
{
	AtkObject *acc, *label;
	AtkObject *rel_obj[1];
	AtkRelationSet *set;
	AtkRelation *relation;
	const gchar *label_text;
	const gchar *existing_name;

	acc = gtk_widget_get_accessible (w);
	label = gtk_widget_get_accessible (l);

	/* If this object has no name, set it's name with the label text */
	existing_name = atk_object_get_name (acc);
	if (!existing_name) {
		label_text = gtk_label_get_text (GTK_LABEL(l));
		if (label_text)
			atk_object_set_name (acc, label_text);
	}

	/* Create the labeled-by relation */
	set = atk_object_ref_relation_set (acc);
	rel_obj[0] = label;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);

	/* Create the label-for relation */
	set = atk_object_ref_relation_set (label);
	rel_obj[0] = acc;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
}

#if GTK_CHECK_VERSION(2,2,0)
static void
gaim_gtk_menu_position_func(GtkMenu *menu,
							gint *x,
							gint *y,
							gboolean *push_in,
							gpointer data)
{
	GtkWidget *widget;
	GtkRequisition requisition;
	GdkScreen *screen;
	GdkRectangle monitor;
	gint monitor_num;
	gint space_left, space_right, space_above, space_below;
	gint needed_width;
	gint needed_height;
	gint xthickness;
	gint ythickness;
	gboolean rtl;

	g_return_if_fail(GTK_IS_MENU(menu));

	widget     = GTK_WIDGET(menu);
	screen     = gtk_widget_get_screen(widget);
	xthickness = widget->style->xthickness;
	ythickness = widget->style->ythickness;
	rtl        = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);

	/*
	 * We need the requisition to figure out the right place to
	 * popup the menu. In fact, we always need to ask here, since
	 * if a size_request was queued while we weren't popped up,
	 * the requisition won't have been recomputed yet.
	 */
	gtk_widget_size_request (widget, &requisition);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);

	push_in = FALSE;

	/*
	 * The placement of popup menus horizontally works like this (with
	 * RTL in parentheses)
	 *
	 * - If there is enough room to the right (left) of the mouse cursor,
	 *   position the menu there.
	 *
	 * - Otherwise, if if there is enough room to the left (right) of the
	 *   mouse cursor, position the menu there.
	 *
	 * - Otherwise if the menu is smaller than the monitor, position it
	 *   on the side of the mouse cursor that has the most space available
	 *
	 * - Otherwise (if there is simply not enough room for the menu on the
	 *   monitor), position it as far left (right) as possible.
	 *
	 * Positioning in the vertical direction is similar: first try below
	 * mouse cursor, then above.
	 */
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	space_left = *x - monitor.x;
	space_right = monitor.x + monitor.width - *x - 1;
	space_above = *y - monitor.y;
	space_below = monitor.y + monitor.height - *y - 1;

	/* position horizontally */

	/* the amount of space we need to position the menu. Note the
	 * menu is offset "xthickness" pixels
	 */
	needed_width = requisition.width - xthickness;

	if (needed_width <= space_left ||
	    needed_width <= space_right)
	{
		if ((rtl  && needed_width <= space_left) ||
		    (!rtl && needed_width >  space_right))
		{
			/* position left */
			*x = *x + xthickness - requisition.width + 1;
		}
		else
		{
			/* position right */
			*x = *x - xthickness;
		}

		/* x is clamped on-screen further down */
	}
	else if (requisition.width <= monitor.width)
	{
		/* the menu is too big to fit on either side of the mouse
		 * cursor, but smaller than the monitor. Position it on
		 * the side that has the most space
		 */
		if (space_left > space_right)
		{
			/* left justify */
			*x = monitor.x;
		}
		else
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
	}
	else /* menu is simply too big for the monitor */
	{
		if (rtl)
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
		else
		{
			/* left justify */
			*x = monitor.x;
		}
	}

	/* Position vertically. The algorithm is the same as above, but
	 * simpler because we don't have to take RTL into account.
	 */
	needed_height = requisition.height - ythickness;

	if (needed_height <= space_above ||
	    needed_height <= space_below)
	{
		if (needed_height <= space_below)
			*y = *y - ythickness;
		else
			*y = *y + ythickness - requisition.height + 1;

		*y = CLAMP (*y, monitor.y,
			   monitor.y + monitor.height - requisition.height);
	}
	else if (needed_height > space_below && needed_height > space_above)
	{
		if (space_below >= space_above)
			*y = monitor.y + monitor.height - requisition.height;
		else
			*y = monitor.y;
	}
	else
	{
		*y = monitor.y;
	}
}

#endif

void
gaim_gtk_treeview_popup_menu_position_func(GtkMenu *menu,
										   gint *x,
										   gint *y,
										   gboolean *push_in,
										   gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkTreeView *tv = GTK_TREE_VIEW(data);
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GdkRectangle rect;
	gint ythickness = GTK_WIDGET(menu)->style->ythickness;

	gdk_window_get_origin (widget->window, x, y);
	gtk_tree_view_get_cursor (tv, &path, &col);
	gtk_tree_view_get_cell_area (tv, path, col, &rect);

	*x += rect.x+rect.width;
	*y += rect.y+rect.height+ythickness;
#if GTK_CHECK_VERSION(2,2,0)
	gaim_gtk_menu_position_func (menu, x, y, push_in, data);
#endif
}

enum {
	DND_FILE_TRANSFER,
	DND_IM_IMAGE,
	DND_BUDDY_ICON
};

typedef struct {
	char *filename;
	GaimAccount *account;
	char *who;
} _DndData;

static void dnd_image_ok_callback(_DndData *data, int choice)
{
	char *filedata;
	size_t size;
	GError *err = NULL;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GtkTextIter iter;
	int id;
	switch (choice) {
	case DND_BUDDY_ICON:
		if (!g_file_get_contents(data->filename, &filedata, &size,
					 &err)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"), data->filename, err->message);
			gaim_notify_error(NULL, NULL,
					  _("Failed to load image"),
					  str);

			g_error_free(err);
			g_free(str);

			return;
		}

		gaim_buddy_icons_set_for_user(data->account, data->who, filedata, size);
		g_free(filedata);
		break;
	case DND_FILE_TRANSFER:
		serv_send_file(gaim_account_get_connection(data->account), data->who, data->filename);
		break;
	case DND_IM_IMAGE:
		conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, data->account, data->who);
		gtkconv = GAIM_GTK_CONVERSATION(conv);

		if (!g_file_get_contents(data->filename, &filedata, &size,
					 &err)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"), data->filename, err->message);
			gaim_notify_error(NULL, NULL,
					  _("Failed to load image"),
					  str);

			g_error_free(err);
			g_free(str);

			return;
		}
		id = gaim_imgstore_add(filedata, size, data->filename);
		g_free(filedata);

		gtk_text_buffer_get_iter_at_mark(GTK_IMHTML(gtkconv->entry)->text_buffer, &iter,
						 gtk_text_buffer_get_insert(GTK_IMHTML(gtkconv->entry)->text_buffer));
		gtk_imhtml_insert_image_at_iter(GTK_IMHTML(gtkconv->entry), id, &iter);
		gaim_imgstore_unref(id);

		break;
	}
	free(data->filename);
	free(data->who);
	free(data);
}

static void dnd_image_cancel_callback(_DndData *data, int choice)
{
	free(data->filename);
	free(data->who);
	free(data);
}

static void dnd_set_icon_ok_cb(_DndData *data)
{
	dnd_image_ok_callback(data, DND_BUDDY_ICON);
}

static void dnd_set_icon_cancel_cb(_DndData *data)
{
	free(data->filename);
	free(data->who);
	free(data);
}

void
gaim_dnd_file_manage(GtkSelectionData *sd, GaimAccount *account, const char *who)
{
	GList *tmp;
	GdkPixbuf *pb;
	GList *files = gaim_uri_list_extract_filenames((const gchar *)sd->data);
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimPluginProtocolInfo *prpl_info = NULL;
	gboolean file_send_ok = FALSE;
#ifndef _WIN32
	GaimDesktopItem *item;
#endif

	g_return_if_fail(account != NULL);
	g_return_if_fail(who != NULL);

	for(tmp = files; tmp != NULL ; tmp = g_list_next(tmp)) {
		gchar *filename = tmp->data;
		gchar *basename = g_path_get_basename(filename);

		/* Set the default action: don't send anything */
		file_send_ok = FALSE;

		/* XXX - Make ft API support creating a transfer with more than one file */
		if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
			continue;
		}

		/* XXX - make ft api suupport sending a directory */
		/* Are we dealing with a directory? */
		if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
			char *str;

			str = g_strdup_printf(_("Cannot send folder %s."), basename);
			gaim_notify_error(NULL, NULL,
					  str,_("Gaim cannot transfer a folder. You will need to send the files within individually"));

			g_free(str);

			continue;
		}

		/* Are we dealing with an image? */
		pb = gdk_pixbuf_new_from_file(filename, NULL);
		if (pb) {
			_DndData *data = g_malloc(sizeof(_DndData));
			gboolean ft = FALSE, im = FALSE;

			data->who = g_strdup(who);
			data->filename = g_strdup(filename);
			data->account = account;

			if (gc)
				prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (prpl_info && prpl_info->options & OPT_PROTO_IM_IMAGE)
				im = TRUE;

			if (prpl_info && prpl_info->can_receive_file)
				ft = prpl_info->can_receive_file(gc, who);

			if (im && ft)
				gaim_request_choice(NULL, NULL,
						    _("You have dragged an image"),
						    _("You can send this image as a file transfer, "
						      "embed it into this message, or use it as the buddy icon for this user."),
						    DND_FILE_TRANSFER, "OK", (GCallback)dnd_image_ok_callback,
						    "Cancel", (GCallback)dnd_image_cancel_callback, data,
						    _("Set as buddy icon"), DND_BUDDY_ICON,
						    _("Send image file"), DND_FILE_TRANSFER,
						    _("Insert in message"), DND_IM_IMAGE, NULL);
			else if (!(im || ft))
				gaim_request_yes_no(NULL, NULL, _("You have dragged an image"),
						       _("Would you like to set it as the buddy icon for this user?"),
						    0, data, (GCallback)dnd_set_icon_ok_cb, (GCallback)dnd_set_icon_cancel_cb);
			else
				gaim_request_choice(NULL, NULL,
						    _("You have dragged an image"),
						    ft ? _("You can send this image as a file transfer or "
							   "embed it into this message, or use it as the buddy icon for this user.") :
						    _("You can insert this image into this message, or use it as the buddy icon for this user"),
						    ft ? DND_FILE_TRANSFER : DND_IM_IMAGE, "OK", (GCallback)dnd_image_ok_callback,
						    "Cancel", (GCallback)dnd_image_cancel_callback, data,
						    _("Set as buddy icon"), DND_BUDDY_ICON,
						    ft ? _("Send image file") : _("Insert in message"), ft ? DND_FILE_TRANSFER : DND_IM_IMAGE, NULL);
			return;
		}

#ifndef _WIN32
		/* Are we trying to send a .desktop file? */
		else if (gaim_str_has_suffix(basename, ".desktop") && (item = gaim_desktop_item_new_from_file(filename))) {
			GaimDesktopItemType dtype;
			char key[64];
			const char *itemname = NULL;

#if GTK_CHECK_VERSION(2,6,0)
			const char * const *langs;
			int i;
			langs = g_get_language_names();
			for (i = 0; langs[i]; i++) {
				g_snprintf(key, sizeof(key), "Name[%s]", langs[i]);
				itemname = gaim_desktop_item_get_string(item, key);
				break;
			}
#else
			const char *lang = g_getenv("LANG");
			char *dot;
			dot = strchr(lang, '.');
			if (dot)
				*dot = '\0';
			g_snprintf(key, sizeof(key), "Name[%s]", lang);
			itemname = gaim_desktop_item_get_string(item, key);
#endif
			if (!itemname)
				itemname = gaim_desktop_item_get_string(item, "Name");

			dtype = gaim_desktop_item_get_entry_type(item);
			switch (dtype) {
				GaimConversation *conv;
				GaimGtkConversation *gtkconv;

			case GAIM_DESKTOP_ITEM_TYPE_LINK:
				conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, who);
				gtkconv =  GAIM_GTK_CONVERSATION(conv);
				gtk_imhtml_insert_link(GTK_IMHTML(gtkconv->entry),
						       gtk_text_buffer_get_insert(GTK_IMHTML(gtkconv->entry)->text_buffer),
						       gaim_desktop_item_get_string(item, "URL"), itemname);
				break;
			default:
				/* I don't know if we really want to do anything here.  Most of the desktop item types are crap like
				 * "MIME Type" (I have no clue how that would be a desktop item) and "Comment"... nothing we can really
				 * send.  The only logical one is "Application," but do we really want to send a binary and nothing else?
				 * Probably not.  I'll just give an error and return. */
				/* The original patch sent the icon used by the launcher.  That's probably wrong */
				gaim_notify_error(NULL, NULL, _("Cannot send launcher"), _("You dragged a desktop launcher. "
											   "Most likely you wanted to send whatever this launcher points to instead of this launcher"
											   " itself."));
				break;
			}
			gaim_desktop_item_unref(item);
			return;
		}
#endif /* _WIN32 */

		/* Everything is fine, let's send */
		serv_send_file(gc, who, filename);
		g_free(filename);
	}
	g_list_free(files);
}

void gaim_gtk_buddy_icon_get_scale_size(GdkPixbuf *buf, GaimBuddyIconSpec *spec, int *width, int *height)
{
	*width = gdk_pixbuf_get_width(buf);
	*height = gdk_pixbuf_get_height(buf);

	gaim_buddy_icon_get_scale_size(spec, width, height);

	/* and now for some arbitrary sanity checks */
	if(*width > 100)
		*width = 100;
	if(*height > 100)
		*height = 100;
}

GdkPixbuf *
gaim_gtk_create_prpl_icon(GaimAccount *account, double scale_factor)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	const char *protoname = NULL;
	char buf[256]; /* TODO: We should use a define for max file length */
	char *filename = NULL;
	GdkPixbuf *pixbuf, *scaled;

	g_return_val_if_fail(account != NULL, NULL);

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	if (prpl == NULL)
		return NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info->list_icon == NULL)
		return NULL;

	protoname = prpl_info->list_icon(account, NULL);
	if (protoname == NULL)
		return NULL;

	/*
	 * Status icons will be themeable too, and then it will look up
	 * protoname from the theme
	 */
	g_snprintf(buf, sizeof(buf), "%s.png", protoname);

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
								"default", buf, NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	scaled = gdk_pixbuf_scale_simple(pixbuf, 32*scale_factor,
				32*scale_factor, GDK_INTERP_BILINEAR);
	g_object_unref(pixbuf);

	return scaled;
}

static GdkPixbuf *
overlay_status_onto_icon(GdkPixbuf *pixbuf, GaimStatusPrimitive primitive)
{
	const char *type_name;
	char basename[256];
	char *filename;
	GdkPixbuf *emblem;

	type_name = gaim_primitive_get_id_from_type(primitive);

	g_snprintf(basename, sizeof(basename), "%s.png", type_name);
	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "status",
								"default", basename, NULL);
	emblem = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	if (emblem != NULL) {
		int width, height, emblem_width, emblem_height;
		int new_emblem_width, new_emblem_height;

		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
		emblem_width = gdk_pixbuf_get_width(emblem);
		emblem_height = gdk_pixbuf_get_height(emblem);

		/*
		 * Figure out how big to make the emblem.  Normally the emblem
		 * will have half the width of the pixbuf.  But we don't make
		 * an emblem any smaller than 10 pixels because it becomes
		 * unrecognizable, unless the width of the pixbuf is less than
		 * 10 pixels, in which case we make the emblem width the same
		 * as the pixbuf width.
		 */
		new_emblem_width = MAX(width / 2, MIN(width, 10));
		new_emblem_height = MAX(height / 2, MIN(height, 10));

		/* Overlay emblem onto the bottom right corner of pixbuf */
		gdk_pixbuf_composite(emblem, pixbuf,
				width - new_emblem_width, height - new_emblem_height,
				new_emblem_width, new_emblem_height,
				width - new_emblem_width, height - new_emblem_height,
				(double)new_emblem_width / (double)emblem_width,
				(double)new_emblem_height / (double)emblem_height,
				GDK_INTERP_BILINEAR,
				255);
		g_object_unref(emblem);
	}

	return pixbuf;
}

GdkPixbuf *
gaim_gtk_create_prpl_icon_with_status(GaimAccount *account, GaimStatusType *status_type, double scale_factor)
{
	GdkPixbuf *pixbuf;

	pixbuf = gaim_gtk_create_prpl_icon(account, scale_factor);
	if (pixbuf == NULL)
		return NULL;

	/*
	 * TODO: Let the prpl pick the emblem on a per status basis,
	 *       and only use the primitive as a fallback?
	 */

	return overlay_status_onto_icon(pixbuf,
				gaim_status_type_get_primitive(status_type));
}

GdkPixbuf *
gaim_gtk_create_gaim_icon_with_status(GaimStatusPrimitive primitive, double scale_factor)
{
	gchar *filename;
	GdkPixbuf *orig, *pixbuf;

	filename = g_build_filename(DATADIR, "pixmaps", "gaim.png", NULL);
	orig = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);
	if (orig == NULL)
		return NULL;

	pixbuf = gdk_pixbuf_scale_simple(orig, 32*scale_factor,
					32*scale_factor, GDK_INTERP_BILINEAR);
	g_object_unref(G_OBJECT(orig));

	return overlay_status_onto_icon(pixbuf, primitive);
}

static void
menu_action_cb(GtkMenuItem *item, gpointer object)
{
	gpointer data;
	void (*callback)(gpointer, gpointer);

	callback = g_object_get_data(G_OBJECT(item), "gaimcallback");
	data = g_object_get_data(G_OBJECT(item), "gaimcallbackdata");

	if (callback)
		callback(object, data);
}

void
gaim_gtk_append_menu_action(GtkWidget *menu, GaimMenuAction *act,
                            gpointer object)
{
	if (act == NULL) {
		gaim_separator(menu);
	} else {
		GtkWidget *menuitem;

		if (act->children == NULL) {
			menuitem = gtk_menu_item_new_with_mnemonic(act->label);

			if (act->callback != NULL) {
				g_object_set_data(G_OBJECT(menuitem),
				                  "gaimcallback",
				                  act->callback);
				g_object_set_data(G_OBJECT(menuitem),
				                  "gaimcallbackdata",
				                  act->data);
				g_signal_connect(G_OBJECT(menuitem), "activate",
				                 G_CALLBACK(menu_action_cb),
				                 object);
			} else {
				gtk_widget_set_sensitive(menuitem, FALSE);
			}

			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		} else {
			GList *l = NULL;
			GtkWidget *submenu = NULL;
			GtkAccelGroup *group;

			menuitem = gtk_menu_item_new_with_mnemonic(act->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

			group = gtk_menu_get_accel_group(GTK_MENU(menu));
			if (group) {
				char *path = g_strdup_printf("%s/%s", GTK_MENU_ITEM(menuitem)->accel_path, act->label);
				gtk_menu_set_accel_path(GTK_MENU(submenu), path);
				g_free(path);
				gtk_menu_set_accel_group(GTK_MENU(submenu), group);
			}

			for (l = act->children; l; l = l->next) {
				GaimMenuAction *act = (GaimMenuAction *)l->data;

				gaim_gtk_append_menu_action(submenu, act, object);
			}
			g_list_free(act->children);
			act->children = NULL;
		}
		gaim_menu_action_free(act);
	}
}

#if GTK_CHECK_VERSION(2,3,0)
# define NEW_STYLE_COMPLETION
#endif

#ifndef NEW_STYLE_COMPLETION
typedef struct
{
	GCompletion *completion;

	gboolean completion_started;
	gboolean all;

} GaimGtkCompletionData;
#endif

#ifndef NEW_STYLE_COMPLETION
static gboolean
completion_entry_event(GtkEditable *entry, GdkEventKey *event,
					   GaimGtkCompletionData *data)
{
	int pos, end_pos;

	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Tab)
	{
		gtk_editable_get_selection_bounds(entry, &pos, &end_pos);

		if (data->completion_started &&
			pos != end_pos && pos > 1 &&
			end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			gtk_editable_select_region(entry, 0, 0);
			gtk_editable_set_position(entry, -1);

			return TRUE;
		}
	}
	else if (event->type == GDK_KEY_PRESS && event->length > 0)
	{
		char *prefix, *nprefix;

		gtk_editable_get_selection_bounds(entry, &pos, &end_pos);

		if (data->completion_started &&
			pos != end_pos && pos > 1 &&
			end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			char *temp;

			temp = gtk_editable_get_chars(entry, 0, pos);
			prefix = g_strconcat(temp, event->string, NULL);
			g_free(temp);
		}
		else if (pos == end_pos && pos > 1 &&
				 end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			prefix = g_strconcat(gtk_entry_get_text(GTK_ENTRY(entry)),
								 event->string, NULL);
		}
		else
			return FALSE;

		pos = strlen(prefix);
		nprefix = NULL;

		g_completion_complete(data->completion, prefix, &nprefix);

		if (nprefix != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(entry), nprefix);
			gtk_editable_set_position(entry, pos);
			gtk_editable_select_region(entry, pos, -1);

			data->completion_started = TRUE;

			g_free(nprefix);
			g_free(prefix);

			return TRUE;
		}

		g_free(prefix);
	}

	return FALSE;
}

static void
destroy_completion_data(GtkWidget *w, GaimGtkCompletionData *data)
{
	g_list_foreach(data->completion->items, (GFunc)g_free, NULL);
	g_completion_free(data->completion);

	g_free(data);
}
#endif /* !NEW_STYLE_COMPLETION */

#ifdef NEW_STYLE_COMPLETION
static gboolean screenname_completion_match_func(GtkEntryCompletion *completion,
		const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	GtkTreeModel *model;
	GValue val1;
	GValue val2;
	const char *tmp;

	model = gtk_entry_completion_get_model (completion);

	val1.g_type = 0;
	gtk_tree_model_get_value(model, iter, 2, &val1);
	tmp = g_value_get_string(&val1);
	if (tmp != NULL && gaim_str_has_prefix(tmp, key))
	{
		g_value_unset(&val1);
		return TRUE;
	}
	g_value_unset(&val1);

	val2.g_type = 0;
	gtk_tree_model_get_value(model, iter, 3, &val2);
	tmp = g_value_get_string(&val2);
	if (tmp != NULL && gaim_str_has_prefix(tmp, key))
	{
		g_value_unset(&val2);
		return TRUE;
	}
	g_value_unset(&val2);

	return FALSE;
}

static gboolean screenname_completion_match_selected_cb(GtkEntryCompletion *completion,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer *user_data)
{
	GValue val;
	GtkWidget *optmenu = user_data[1];
	GaimAccount *account;

	val.g_type = 0;
	gtk_tree_model_get_value(model, iter, 1, &val);
	gtk_entry_set_text(GTK_ENTRY(user_data[0]), g_value_get_string(&val));
	g_value_unset(&val);

	gtk_tree_model_get_value(model, iter, 4, &val);
	account = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (account == NULL)
		return TRUE;

	if (optmenu != NULL) {
		GList *items;
		guint index = 0;
		gaim_gtk_account_option_menu_set_selected(optmenu, account);
		items = GTK_MENU_SHELL(gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu)))->children;

		do {
			if (account == g_object_get_data(G_OBJECT(items->data), "account")) {
				/* Set the account in the GUI. */
				gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), index);
				return TRUE;
			}
			index++;
		} while ((items = items->next) != NULL);
	}

	return TRUE;
}

static void
add_screenname_autocomplete_entry(GtkListStore *store, const char *buddy_alias, const char *contact_alias,
								  const GaimAccount *account, const char *screenname)
{
	GtkTreeIter iter;
	gboolean completion_added = FALSE;
	gchar *normalized_screenname;
	gchar *tmp;

	tmp = g_utf8_normalize(screenname, -1, G_NORMALIZE_DEFAULT);
	normalized_screenname = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the screenname and buddy alias match. */
	if (buddy_alias && strcmp(buddy_alias, screenname)) {
		char *completion_entry = g_strdup_printf("%s \"%s\"", screenname, buddy_alias);
		char *tmp2 = g_utf8_normalize(buddy_alias, -1, G_NORMALIZE_DEFAULT);

		tmp = g_utf8_casefold(tmp2, -1);
		g_free(tmp2);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				0, completion_entry,
				1, screenname,
				2, normalized_screenname,
				3, tmp,
				4, account,
				-1);
		g_free(completion_entry);
		g_free(tmp);
		completion_added = TRUE;
	}

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the screenname and contact alias match. */
	if (contact_alias && strcmp(contact_alias, screenname)) {
		/* We don't want duplicates when the contact and buddy alias match. */
		if (!buddy_alias || strcmp(contact_alias, buddy_alias)) {
			char *completion_entry = g_strdup_printf("%s \"%s\"",
							screenname, contact_alias);
			char *tmp2 = g_utf8_normalize(contact_alias, -1, G_NORMALIZE_DEFAULT);

			tmp = g_utf8_casefold(tmp2, -1);
			g_free(tmp2);

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
					0, completion_entry,
					1, screenname,
					2, normalized_screenname,
					3, tmp,
					4, account,
					-1);
			g_free(completion_entry);
			g_free(tmp);
			completion_added = TRUE;
		}
	}

	if (completion_added == FALSE) {
		/* Add the buddy's screenname. */
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				0, screenname,
				1, screenname,
				2, normalized_screenname,
				3, NULL,
				4, account,
				-1);
	}

	g_free(normalized_screenname);
}
#endif /* NEW_STYLE_COMPLETION */

static void get_log_set_name(GaimLogSet *set, gpointer value, gpointer **set_hash_data)
{
	/* 1. Don't show buddies because we will have gotten them already.
	 * 2. Only show those with non-NULL accounts that are currently connected.
	 * 3. The boxes that use this autocomplete code handle only IMs. */
	if (!set->buddy &&
	    (GPOINTER_TO_INT(set_hash_data[1]) ||
	     (set->account != NULL && gaim_account_is_connected(set->account))) &&
		set->type == GAIM_LOG_IM) {
#ifdef NEW_STYLE_COMPLETION
			add_screenname_autocomplete_entry((GtkListStore *)set_hash_data[0],
											  NULL, NULL, set->account, set->name);
#else
			GList **items = ((GList **)set_hash_data[0]);
			/* Steal the name for the GCompletion. */
			*items = g_list_append(*items, set->name);
			set->name = set->normalized_name = NULL;
#endif /* NEW_STYLE_COMPLETION */
	}
}

#ifdef NEW_STYLE_COMPLETION
static void
add_completion_list(GtkListStore *store)
{
	GaimBlistNode *gnode, *cnode, *bnode;
	gboolean all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(store), "screenname-all"));
	GHashTable *sets;
	gpointer set_hash_data[] = {store, GINT_TO_POINTER(all)};

	gtk_list_store_clear(store);

	for (gnode = gaim_get_blist()->root; gnode != NULL; gnode = gnode->next)
	{
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
			{
				GaimBuddy *buddy = (GaimBuddy *)bnode;

				if (!all && !gaim_account_is_connected(buddy->account))
					continue;

				add_screenname_autocomplete_entry(store,
												  ((GaimContact *)cnode)->alias,
												  gaim_buddy_get_contact_alias(buddy),
												  buddy->account,
												  buddy->name
												 );
			}
		}
	}

	sets = gaim_log_get_log_sets();
	g_hash_table_foreach(sets, (GHFunc)get_log_set_name, &set_hash_data);
	g_hash_table_destroy(sets);
}
#else
static void
add_completion_list(GaimGtkCompletionData *data)
{
	GaimBlistNode *gnode, *cnode, *bnode;
	GCompletion *completion;
	GList *item = g_list_append(NULL, NULL);
	GHashTable *sets;
	gpointer set_hash_data[2];

	completion = data->completion;

	g_list_foreach(completion->items, (GFunc)g_free, NULL);
	g_completion_clear_items(completion);

	for (gnode = gaim_get_blist()->root; gnode != NULL; gnode = gnode->next)
	{
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
			{
				GaimBuddy *buddy = (GaimBuddy *)bnode;

				if (!data->all && !gaim_account_is_connected(buddy->account))
					continue;

				item->data = g_strdup(buddy->name);
				g_completion_add_items(data->completion, item);
			}
		}
	}
	g_list_free(item);

	sets = gaim_log_get_log_sets();
	item = NULL;
	set_hash_data[0] = &item;
	set_hash_data[1] = GINT_TO_POINTER(data->all);
	g_hash_table_foreach(sets, (GHFunc)get_log_set_name, &set_hash_data);
	g_hash_table_destroy(sets);
	g_completion_add_items(data->completion, item);
	g_list_free(item);
}
#endif

static void
screenname_autocomplete_destroyed_cb(GtkWidget *widget, gpointer data)
{
	gaim_signals_disconnect_by_handle(widget);
}

static void
repopulate_autocomplete(gpointer something, gpointer data)
{
	add_completion_list(data);
}

void
gaim_gtk_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *accountopt, gboolean all)
{
	gpointer cb_data = NULL;

#ifdef NEW_STYLE_COMPLETION
	/* Store the displayed completion value, the screenname, the UTF-8 normalized & casefolded screenname,
	 * the UTF-8 normalized & casefolded value for comparison, and the account. */
	GtkListStore *store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	GtkEntryCompletion *completion;
	gpointer *data;

	g_object_set_data(G_OBJECT(store), "screenname-all", GINT_TO_POINTER(all));
	add_completion_list(store);

	cb_data = store;

	/* Sort the completion list by screenname. */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
	                                     1, GTK_SORT_ASCENDING);

	completion = gtk_entry_completion_new();
	gtk_entry_completion_set_match_func(completion, screenname_completion_match_func, NULL, NULL);

	data = g_new0(gpointer, 2);
	data[0] = entry;
	data[1] = accountopt;
	g_signal_connect(G_OBJECT(completion), "match-selected",
		G_CALLBACK(screenname_completion_match_selected_cb), data);

	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	g_object_unref(completion);

	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_entry_completion_set_text_column(completion, 0);

#else /* !NEW_STYLE_COMPLETION */
	GaimGtkCompletionData *data;

	data = g_new0(GaimGtkCompletionData, 1);

	data->completion = g_completion_new(NULL);
	data->all = all;

	g_completion_set_compare(data->completion, g_ascii_strncasecmp);

	add_completion_list(data);
	cb_data = data;

	g_signal_connect(G_OBJECT(entry), "event",
					 G_CALLBACK(completion_entry_event), data);
	g_signal_connect(G_OBJECT(entry), "destroy",
					 G_CALLBACK(destroy_completion_data), data);

#endif /* !NEW_STYLE_COMPLETION */

	if (!all)
	{
		gaim_signal_connect(gaim_connections_get_handle(), "signed-on", entry,
							GAIM_CALLBACK(repopulate_autocomplete), cb_data);
		gaim_signal_connect(gaim_connections_get_handle(), "signed-off", entry,
							GAIM_CALLBACK(repopulate_autocomplete), cb_data);
	}

	gaim_signal_connect(gaim_accounts_get_handle(), "account-added", entry,
						GAIM_CALLBACK(repopulate_autocomplete), cb_data);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed", entry,
						GAIM_CALLBACK(repopulate_autocomplete), cb_data);

	g_signal_connect(G_OBJECT(entry), "destroy", G_CALLBACK(screenname_autocomplete_destroyed_cb), NULL);
}

void gaim_gtk_set_cursor(GtkWidget *widget, GdkCursorType cursor_type)
{
	GdkCursor *cursor;

	g_return_if_fail(widget != NULL);
	if (widget->window == NULL)
		return;

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(widget->window, cursor);
	gdk_cursor_unref(cursor);

#if GTK_CHECK_VERSION(2,4,0)
	gdk_display_flush(gdk_drawable_get_display(GDK_DRAWABLE(widget->window)));
#else
	gdk_flush();
#endif
}

void gaim_gtk_clear_cursor(GtkWidget *widget)
{
	g_return_if_fail(widget != NULL);
	if (widget->window == NULL)
		return;

	gdk_window_set_cursor(widget->window, NULL);
}

struct _icon_chooser {
	GtkWidget *icon_filesel;
	GtkWidget *icon_preview;
	GtkWidget *icon_text;
	
	void (*callback)(const char*,gpointer);
	gpointer data;
};

#if !GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
static void
icon_filesel_delete_cb(GtkWidget *w, struct _icon_chooser *dialog)
{
	if (dialog->icon_filesel != NULL)
		gtk_widget_destroy(dialog->icon_filesel);

	if (dialog->callback)
		dialog->callback(NULL, data);

	g_free(dialog);
}
#endif /* FILECHOOSER */



#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
static void
icon_filesel_choose_cb(GtkWidget *widget, gint response, struct _icon_chooser *dialog)
{
	char *filename, *current_folder;

	if (response != GTK_RESPONSE_ACCEPT) {
		if (response == GTK_RESPONSE_CANCEL) {
			gtk_widget_destroy(dialog->icon_filesel);
		}
		dialog->icon_filesel = NULL;
		if (dialog->callback)
			dialog->callback(NULL, dialog->data);
		g_free(dialog);
		return;
	}

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->icon_filesel));
	current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel));
	if (current_folder != NULL) {
		gaim_prefs_set_string("/gaim/gtk/filelocations/last_icon_folder", current_folder);
		g_free(current_folder);
	}

#else /* FILECHOOSER */
static void
icon_filesel_choose_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	char *filename, *current_folder;

	filename = g_strdup(gtk_file_selection_get_filename(
				GTK_FILE_SELECTION(dialog->icon_filesel)));

	/* If they typed in a directory, change there */
	if (gaim_gtk_check_if_dir(filename,
				GTK_FILE_SELECTION(dialog->icon_filesel)))
	{
		g_free(filename);
		return;
	}

	current_folder = g_path_get_dirname(filename);
	if (current_folder != NULL) {
		gaim_prefs_set_string("/gaim/gtk/filelocations/last_icon_folder", current_folder);
		g_free(current_folder);
	}

#endif /* FILECHOOSER */
	if (dialog->callback)
		dialog->callback(filename, dialog->data);
	gtk_widget_destroy(dialog->icon_filesel);
	g_free(filename);
	g_free(dialog);
 }


static void
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
icon_preview_change_cb(GtkFileChooser *widget, struct _icon_chooser *dialog)
#else /* FILECHOOSER */
icon_preview_change_cb(GtkTreeSelection *sel, struct _icon_chooser *dialog)
#endif /* FILECHOOSER */
{
	GdkPixbuf *pixbuf, *scale;
	int height, width;
	char *basename, *markup, *size;
	struct stat st;
	char *filename;

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	filename = gtk_file_chooser_get_preview_filename(
					GTK_FILE_CHOOSER(dialog->icon_filesel));
#else /* FILECHOOSER */
	filename = g_strdup(gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(dialog->icon_filesel)));
#endif /* FILECHOOSER */

	if (!filename || g_stat(filename, &st))
	{
		g_free(filename);
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	if (!pixbuf) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), NULL);
		gtk_label_set_markup(GTK_LABEL(dialog->icon_text), "");
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
		gtk_file_chooser_set_preview_widget_active(
					GTK_FILE_CHOOSER(dialog->icon_filesel), FALSE);
#endif /* FILECHOOSER */
		g_free(filename);
		return;
	}

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	basename = g_path_get_basename(filename);
	size = gaim_str_size_to_units(st.st_size);
	markup = g_strdup_printf(_("<b>File:</b> %s\n"
							   "<b>File size:</b> %s\n"
							   "<b>Image size:</b> %dx%d"),
							 basename, size, width, height);

	scale = gdk_pixbuf_scale_simple(pixbuf, width * 50 / height,
									50, GDK_INTERP_BILINEAR);
	gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), scale);
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	gtk_file_chooser_set_preview_widget_active(
					GTK_FILE_CHOOSER(dialog->icon_filesel), TRUE);
#endif /* FILECHOOSER */
	gtk_label_set_markup(GTK_LABEL(dialog->icon_text), markup);

	g_object_unref(G_OBJECT(pixbuf));
	g_object_unref(G_OBJECT(scale));
	g_free(filename);
	g_free(basename);
	g_free(size);
	g_free(markup);
}


GtkWidget *gaim_gtk_buddy_icon_chooser_new(GtkWindow *parent, void(*callback)(const char *, gpointer), gpointer data) {
	struct _icon_chooser *dialog = g_new0(struct _icon_chooser, 1);

#if !GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	GtkWidget *hbox;
	GtkWidget *tv;
	GtkTreeSelection *sel;
#endif /* FILECHOOSER */
	const char *current_folder;

	dialog->callback = callback;
	dialog->data = data;

	if (dialog->icon_filesel != NULL) {
		gtk_window_present(GTK_WINDOW(dialog->icon_filesel));
		return NULL;
	}

	current_folder = gaim_prefs_get_string("/gaim/gtk/filelocations/last_icon_folder");
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */

	dialog->icon_filesel = gtk_file_chooser_dialog_new(_("Buddy Icon"),
							   parent,
							   GTK_FILE_CHOOSER_ACTION_OPEN,
							   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							   NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog->icon_filesel), GTK_RESPONSE_ACCEPT);
	if ((current_folder != NULL) && (*current_folder != '\0'))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel),
						    current_folder);

	dialog->icon_preview = gtk_image_new();
	dialog->icon_text = gtk_label_new(NULL);
	gtk_widget_set_size_request(GTK_WIDGET(dialog->icon_preview), -1, 50);
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog->icon_filesel),
					    GTK_WIDGET(dialog->icon_preview));
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "update-preview",
					 G_CALLBACK(icon_preview_change_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "response",
					 G_CALLBACK(icon_filesel_choose_cb), dialog);
	icon_preview_change_cb(NULL, dialog);
#else /* FILECHOOSER */
	dialog->icon_filesel = gtk_file_selection_new(_("Buddy Icon"));
	dialog->icon_preview = gtk_image_new();
	dialog->icon_text = gtk_label_new(NULL);
	if ((current_folder != NULL) && (*current_folder != '\0'))
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog->icon_filesel),
										current_folder);

	gtk_widget_set_size_request(GTK_WIDGET(dialog->icon_preview), -1, 50);
	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(
		GTK_BOX(GTK_FILE_SELECTION(dialog->icon_filesel)->main_vbox),
		hbox, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), dialog->icon_preview,
			 FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), dialog->icon_text, FALSE, FALSE, 0);

	tv = GTK_FILE_SELECTION(dialog->icon_filesel)->file_list;
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));

	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(icon_preview_change_cb), dialog);
	g_signal_connect(
		G_OBJECT(GTK_FILE_SELECTION(dialog->icon_filesel)->ok_button),
		"clicked",
		G_CALLBACK(icon_filesel_choose_cb), dialog);
	g_signal_connect(
		G_OBJECT(GTK_FILE_SELECTION(dialog->icon_filesel)->cancel_button),
		"clicked",
		G_CALLBACK(icon_filesel_delete_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "destroy",
					 G_CALLBACK(icon_filesel_delete_cb), dialog);
#endif /* FILECHOOSER */
	return 	dialog->icon_filesel;
}


#if GTK_CHECK_VERSION(2,2,0)
static gboolean
str_array_match(char **a, char **b)
{
	int i, j;

	if (!a || !b)
		return FALSE;
	for (i = 0; a[i] != NULL; i++)
		for (j = 0; b[j] != NULL; j++)
			if (!g_ascii_strcasecmp(a[i], b[j]))
				return TRUE;
	return FALSE;
}
#endif

char *
gaim_gtk_convert_buddy_icon(GaimPlugin *plugin, const char *path)
{
#if GTK_CHECK_VERSION(2,2,0)
	int width, height;
	char **pixbuf_formats = NULL;
	GdkPixbufFormat *format;
	GdkPixbuf *pixbuf;
	GaimPluginProtocolInfo *prpl_info;
	char **prpl_formats;
#if !GTK_CHECK_VERSION(2,4,0)
	GdkPixbufLoader *loader;
	FILE *file;
	struct stat st;
	void *data = NULL;
#endif
#endif
	const char *dirname;
	char *random;
	char *filename;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

	g_return_val_if_fail(prpl_info->icon_spec.format != NULL, NULL);

	prpl_formats = g_strsplit(prpl_info->icon_spec.format,",",0);
	dirname = gaim_buddy_icons_get_cache_dir();
	random = g_strdup_printf("%x", g_random_int());
	filename = g_build_filename(dirname, random, NULL);

	if (!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
		gaim_debug_info("buddyicon", "Creating icon cache directory.\n");

		if (g_mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
			gaim_debug_error("buddyicon",
							 "Unable to create directory %s: %s\n",
							 dirname, strerror(errno));
#if GTK_CHECK_VERSION(2,2,0)
			g_strfreev(prpl_formats);
#endif
			g_free(random);
			g_free(filename);
			return NULL;
		}
	}

#if GTK_CHECK_VERSION(2,2,0)
#if GTK_CHECK_VERSION(2,4,0)
	format = gdk_pixbuf_get_file_info (path, &width, &height);
#else
	loader = gdk_pixbuf_loader_new();
	if (!g_stat(path, &st) && (file = g_fopen(path, "rb")) != NULL) {
		data = g_malloc(st.st_size);
		fread(data, 1, st.st_size, file);
		fclose(file);
		gdk_pixbuf_loader_write(loader, data, st.st_size, NULL);
		g_free(data);
	}
	gdk_pixbuf_loader_close(loader, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	format = gdk_pixbuf_loader_get_format(loader);
	g_object_unref(G_OBJECT(loader));
#endif
	if (format == NULL)
		return NULL;
	pixbuf_formats =  gdk_pixbuf_format_get_extensions(format);

	if (str_array_match(pixbuf_formats, prpl_formats) &&                  /* This is an acceptable format AND */
		 (!(prpl_info->icon_spec.scale_rules & GAIM_ICON_SCALE_SEND) ||   /* The prpl doesn't scale before it sends OR */
		  (prpl_info->icon_spec.min_width <= width &&
		   prpl_info->icon_spec.max_width >= width &&
		   prpl_info->icon_spec.min_height <= height &&
		   prpl_info->icon_spec.max_height >= height)))                   /* The icon is the correct size */
#endif
	{
		gchar *contents;
		gsize length;
		FILE *image;

#if GTK_CHECK_VERSION(2,2,0)
		g_strfreev(prpl_formats);
		g_strfreev(pixbuf_formats);
#endif

		/* Copy the image to the cache folder as "filename". */

		if (!g_file_get_contents(path, &contents, &length, NULL) ||
		    (image = g_fopen(filename, "wb")) == NULL)
		{
			g_free(random);
			g_free(filename);
#if GTK_CHECK_VERSION(2,2,0) && !GTK_CHECK_VERSION(2,4,0)
			g_object_unref(G_OBJECT(pixbuf));
#endif
			return NULL;
		}

		if (fwrite(contents, 1, length, image) != length)
		{
			fclose(image);
			g_unlink(filename);

			g_free(random);
			g_free(filename);
#if GTK_CHECK_VERSION(2,2,0) && !GTK_CHECK_VERSION(2,4,0)
			g_object_unref(G_OBJECT(pixbuf));
#endif
			return NULL;
		}
		fclose(image);

#if GTK_CHECK_VERSION(2,2,0) && !GTK_CHECK_VERSION(2,4,0)
		g_object_unref(G_OBJECT(pixbuf));
#endif

		g_free(filename);
		return random;
	}
#if GTK_CHECK_VERSION(2,2,0)
	else
	{
		int i;
		GError *error = NULL;
		GdkPixbuf *scale;
		pixbuf = gdk_pixbuf_new_from_file(path, &error);
		g_strfreev(pixbuf_formats);
		if (!error && (prpl_info->icon_spec.scale_rules & GAIM_ICON_SCALE_SEND) &&
			(width < prpl_info->icon_spec.min_width ||
			 width > prpl_info->icon_spec.max_width ||
			 height < prpl_info->icon_spec.min_height ||
			 height > prpl_info->icon_spec.max_height))
		{
			int new_width = width;
			int new_height = height;

			if(new_width > prpl_info->icon_spec.max_width)
				new_width = prpl_info->icon_spec.max_width;
			else if(new_width < prpl_info->icon_spec.min_width)
				new_width = prpl_info->icon_spec.min_width;
			if(new_height > prpl_info->icon_spec.max_height)
				new_height = prpl_info->icon_spec.max_height;
			else if(new_height < prpl_info->icon_spec.min_height)
				new_height = prpl_info->icon_spec.min_height;

			/* preserve aspect ratio */
			if ((double)height * (double)new_width >
				(double)width * (double)new_height) {
					new_width = 0.5 + (double)width * (double)new_height / (double)height;
			} else {
					new_height = 0.5 + (double)height * (double)new_width / (double)width;
			}

			scale = gdk_pixbuf_scale_simple (pixbuf, new_width, new_height,
					GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(pixbuf));
			pixbuf = scale;
		}
		if (error) {
			g_free(random);
			g_free(filename);
			gaim_debug_error("buddyicon", "Could not open icon for conversion: %s\n", error->message);
			g_error_free(error);
			g_strfreev(prpl_formats);
			return NULL;
		}

		for (i = 0; prpl_formats[i]; i++) {
			gaim_debug_info("buddyicon", "Converting buddy icon to %s as %s\n", prpl_formats[i], filename);
			/* The gdk-pixbuf documentation is wrong. gdk_pixbuf_save returns TRUE if it was successful,
			 * FALSE if an error was set. */
			if (gdk_pixbuf_save (pixbuf, filename, prpl_formats[i], &error, NULL) == TRUE)
					break;
			gaim_debug_warning("buddyicon", "Could not convert to %s: %s\n", prpl_formats[i], error->message);
			g_error_free(error);
			error = NULL;
		}
		g_strfreev(prpl_formats);
		if (!error) {
			g_object_unref(G_OBJECT(pixbuf));
			g_free(filename);
			return random;
		} else {
			gaim_debug_error("buddyicon", "Could not convert icon to usable format: %s\n", error->message);
			g_error_free(error);
		}
		g_free(random);
		g_free(filename);
		g_object_unref(G_OBJECT(pixbuf));
	}
	return NULL;
#endif
}