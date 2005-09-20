/*
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#include "gtkmenutray.h"

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_ZERO = 0,
	PROP_BOX
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Internal Stuff
 *****************************************************************************/

/******************************************************************************
 * Item Stuff
 *****************************************************************************/
static void
gaim_gtk_menu_tray_select(GtkItem *item) {
	/* this may look like nothing, but it's really overriding the
	 * GtkMenuItem's select function so that it doesn't get highlighted like
	 * a normal menu item would.
	 */
}

static void
gaim_gtk_menu_tray_deselect(GtkItem *item) {
	/* Probably not necessary, but I'd rather be safe than sorry.  We're
	 * overridding the select, so it makes sense to override deselect as well.
	 */
}

/******************************************************************************
 * Widget Stuff
 *****************************************************************************/

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
gaim_gtk_menu_tray_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	GaimGtkMenuTray *menu_tray = GAIM_GTK_MENU_TRAY(obj);

	switch(param_id) {
		case PROP_BOX:
			g_value_set_object(value, gaim_gtk_menu_tray_get_box(menu_tray));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
gaim_gtk_menu_tray_finalize(GObject *obj) {
	GaimGtkMenuTray *tray = GAIM_GTK_MENU_TRAY(obj);
	if(GTK_IS_WIDGET(tray->tray))
		gtk_widget_destroy(GTK_WIDGET(tray->tray));

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gaim_gtk_menu_tray_class_init(GaimGtkMenuTrayClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkItemClass *item_class = GTK_ITEM_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	object_class->finalize = gaim_gtk_menu_tray_finalize;
	object_class->get_property = gaim_gtk_menu_tray_get_property;

	item_class->select = gaim_gtk_menu_tray_select;
	item_class->deselect = gaim_gtk_menu_tray_deselect;

	pspec = g_param_spec_object("box", "The box",
								"The box",
								GTK_TYPE_BOX,
								G_PARAM_READABLE);
	g_object_class_install_property(object_class, PROP_BOX, pspec);
}

static void
gaim_gtk_menu_tray_init(GaimGtkMenuTray *menu_tray) {
	gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menu_tray), TRUE);
	
	if(!GTK_IS_WIDGET(menu_tray->tray))
		menu_tray->tray = gtk_hbox_new(FALSE, 0);

	gtk_container_add(GTK_CONTAINER(menu_tray), menu_tray->tray);

	gtk_widget_show(menu_tray->tray);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
gaim_gtk_menu_tray_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GaimGtkMenuTrayClass),
			NULL,
			NULL,
			(GClassInitFunc)gaim_gtk_menu_tray_class_init,
			NULL,
			NULL,
			sizeof(GaimGtkMenuTray),
			0,
			(GInstanceInitFunc)gaim_gtk_menu_tray_init,
			NULL
		};

		type = g_type_register_static(GTK_TYPE_MENU_ITEM,
									  "GaimGtkMenuTray",
									  &info, 0);
	}

	return type;
}

GtkWidget *
gaim_gtk_menu_tray_new() {
	return g_object_new(GAIM_GTK_TYPE_MENU_TRAY, NULL);
}

GtkWidget *
gaim_gtk_menu_tray_get_box(GaimGtkMenuTray *menu_tray) {
	g_return_val_if_fail(GAIM_GTK_IS_MENU_TRAY(menu_tray), NULL);
	return menu_tray->tray;
}

void
gaim_gtk_menu_tray_append(GaimGtkMenuTray *menu_tray, GtkWidget *widget) {
	g_return_if_fail(GAIM_GTK_IS_MENU_TRAY(menu_tray));
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_box_pack_end(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
}

void
gaim_gtk_menu_tray_prepend(GaimGtkMenuTray *menu_tray, GtkWidget *widget) {
	g_return_if_fail(GAIM_GTK_IS_MENU_TRAY(menu_tray));
	g_return_if_fail(GTK_IS_WIDGET(widget));

	gtk_box_pack_start(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
}
