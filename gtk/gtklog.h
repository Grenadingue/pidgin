/**
 * @file gtklog.h GTK+ Log viewer
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
#ifndef _GAIM_GTKLOG_H_
#define _GAIM_GTKLOG_H_

#include "gtkgaim.h"
#include "log.h"

#include "account.h"

void gaim_gtk_log_show(GaimLogType type, const char *screenname, GaimAccount *account);
void gaim_gtk_log_show_contact(GaimContact *contact);

void gaim_gtk_syslog_show(void);

/**************************************************************************/
/** @name GTK+ Log Subsystem                                              */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GTK+ log subsystem.
 */
void gaim_gtk_log_init(void);

/**
 * Returns the GTK+ log subsystem handle.
 *
 * @return The GTK+ log subsystem handle.
 */
void *gaim_gtk_log_get_handle(void);

/**
 * Uninitializes the GTK+ log subsystem.
 */
void gaim_gtk_log_uninit(void);

/*@}*/

#endif