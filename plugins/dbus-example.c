/*
 *   This is an example of a gaim dbus plugin.  After enabling this
 *   plugin, the following commands should work from the command line:
 *
 *   prompt$ gaim-send DbusExampleGetHelloObject
 *
 *     returns, say: int32 74
 *
 *   prompt$ gaim-send DbusExampleGetText int32:74
 *
 *     returns: string "Hello."
 *
 *   prompt$ gaim-send DbusExampleSetText int32:74 string:Bye!
 *
 *   prompt$ gaim-send DbusExampleGetText int32:74
 *
 *     returns: string "Bye!"
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

#include "plugin.h"
#include "blist.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include "dbus-maybe.h"
#include "dbus-bindings.h"

typedef struct {
    char *text;
} GaimText;

/* This makes the structure GaimText visible to the gaim-dbus type
   system.  It defines GaimText as a type with no parent.  From now
   on, we will be able to register pointers to structures of this
   type.  You to dbus-define types you want to be directly accessible
   by external applications. */
GAIM_DBUS_DEFINE_TYPE(GaimText)	

/* Here we make four functions accessible to other applications by
   DBus.  These functions can access types defined in gaim proper
   (GaimBuddy) as well as the types defined in the plugin (GaimText).  */
DBUS_EXPORT GaimText* dbus_example_get_hello_object(void);
DBUS_EXPORT void dbus_example_set_text(GaimText *obj, const char *text);
DBUS_EXPORT const char *dbus_example_get_text(GaimText *obj);
DBUS_EXPORT const char *dbus_example_get_buddy_name(GaimBuddy *buddy);

/* This file has been generated by the #dbus-analize-functions.py
   script.  It contains dbus wrappers for the four functions declared
   above. */
#include "dbus-example-bindings.c"

/* This is the GaimText object we want to make publicly visible. */
static GaimText hello;

/* Here come the definitions of the four exported functions. */
GaimText* dbus_example_get_hello_object(void) 
{
    return &hello;
}

void dbus_example_set_text(GaimText *obj, const char *text) 
{
    if (obj != NULL) {
	g_free(obj->text);
	obj->text = g_strdup(text);
    }
}

const char *dbus_example_get_text(GaimText *obj) 
{
    if (obj != NULL) 
	return obj->text;
    else
	return NULL;
}

const char *dbus_example_get_buddy_name(GaimBuddy *buddy) 
{
    return gaim_buddy_get_name(buddy);
}

/* And now standard plugin stuff */

static gboolean
plugin_load(GaimPlugin *plugin)
{
    /* First, we have to register our four exported functions with the
       main gaim dbus loop.  Without this statement, the gaim dbus
       code wouldn't know about our functions. */
    GAIM_DBUS_REGISTER_BINDINGS(plugin);

    /* Then, we register the hello object of type GaimText.  Note that
       pointer registrations / unregistrations are completely dynamic;
       they don't have to be made when the plugin is loaded /
       unloaded.  Without this statement the dbus gaim code wouldn't
       know about the hello object.  */
    GAIM_DBUS_REGISTER_POINTER(&hello, GaimText);

    hello.text = g_strdup("Hello.");
    
    return TRUE;
}


static gboolean
plugin_unload(GaimPlugin *plugin)
{
    g_free(hello.text);

    /* It is necessary to unregister all pointers registered by the module. */
    GAIM_DBUS_UNREGISTER_POINTER(&hello);

    return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"dbus-example",                                   /**< id             */
	N_("DBus"),                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("DBus Plugin Example"),
	                                                  /**  description    */
	N_("DBus Plugin Example"),
	"Piotr Zielinski (http://cl.cam.ac.uk/~pz215)",   /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                       /**< prefs_info     */
	NULL
};

static void init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(dbus_example, init_plugin, info)