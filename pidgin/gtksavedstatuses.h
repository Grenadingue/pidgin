/**
 * @file gtksavedstatuses.h GTK+ Saved Status Editor UI
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
#ifndef _GAIM_GTKSAVEDSTATUSES_H_
#define _GAIM_GTKSAVEDSTATUSES_H_

#include "savedstatuses.h"
#include "status.h"

/**
 * Shows the status window.
 */
void gaim_gtk_status_window_show(void);

/**
 * Hides the status window.
 */
void gaim_gtk_status_window_hide(void);

/**
 * Shows a status editor (used for adding a new saved status or
 * editing an already existing saved status).
 *
 * @param edit   TRUE if we want to edit an existing saved
 *               status or FALSE to create a new one.  You
 *               can not edit transient statuses--they don't
 *               have titles.  If you want to edit a transient
 *               status, set this to FALSE and seed the dialog
 *               with the transient status using the status
 *               parameter to this function.
 * @param status If edit is TRUE then this should be a
 *               pointer to the GaimSavedStatus to edit.
 *               If edit is FALSE then this can be NULL,
 *               or you can pass in a saved status to
 *               seed the initial values of the new status.
 */
void gaim_gtk_status_editor_show(gboolean edit, GaimSavedStatus *status);

/**
 * Creates a dropdown menu of saved statuses and calls a callback
 * when one is selected
 *
 * @param status   The default saved_status to show as 'selected'
 * @param callback The callback to call when the selection changes
 * @return         The menu widget
 */
GtkWidget *gaim_gtk_status_menu(GaimSavedStatus *status, GCallback callback);

/**
 * Returns the GTK+ status handle.
 *
 * @return The handle to the GTK+ status system.
 */
void *gaim_gtk_status_get_handle(void);

/**
 * Initializes the GTK+ status system.
 */
void gaim_gtk_status_init(void);

/**
 * Uninitializes the GTK+ status system.
 */
void gaim_gtk_status_uninit(void);

#endif /* _GAIM_GTKSAVEDSTATUSES_H_ */
