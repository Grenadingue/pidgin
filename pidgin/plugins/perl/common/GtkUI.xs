#include "gtkmodule.h"

/* Prototypes for the BOOT section below. */
PURPLE_PERL_BOOT_PROTO(GtkUI__Account);
PURPLE_PERL_BOOT_PROTO(GtkUI__BuddyList);
PURPLE_PERL_BOOT_PROTO(GtkUI__Connection);
PURPLE_PERL_BOOT_PROTO(GtkUI__Conversation);
PURPLE_PERL_BOOT_PROTO(GtkUI__Conversation__Window);
PURPLE_PERL_BOOT_PROTO(GtkUI__Debug);
PURPLE_PERL_BOOT_PROTO(GtkUI__Dialogs);
PURPLE_PERL_BOOT_PROTO(GtkUI__IMHtml);
PURPLE_PERL_BOOT_PROTO(GtkUI__IMHtmlToolbar);
PURPLE_PERL_BOOT_PROTO(GtkUI__Log);
PURPLE_PERL_BOOT_PROTO(GtkUI__MenuTray);
PURPLE_PERL_BOOT_PROTO(GtkUI__Plugin);
PURPLE_PERL_BOOT_PROTO(GtkUI__PluginPref);
PURPLE_PERL_BOOT_PROTO(GtkUI__Pounce);
PURPLE_PERL_BOOT_PROTO(GtkUI__Prefs);
PURPLE_PERL_BOOT_PROTO(GtkUI__Privacy);
PURPLE_PERL_BOOT_PROTO(GtkUI__Roomlist);
PURPLE_PERL_BOOT_PROTO(GtkUI__Status);
#ifndef _WIN32
PURPLE_PERL_BOOT_PROTO(GtkUI__Session);
#endif
PURPLE_PERL_BOOT_PROTO(GtkUI__Sound);
PURPLE_PERL_BOOT_PROTO(GtkUI__StatusBox);
PURPLE_PERL_BOOT_PROTO(GtkUI__Themes);
PURPLE_PERL_BOOT_PROTO(GtkUI__Utils);
PURPLE_PERL_BOOT_PROTO(GtkUI__Xfer);

MODULE = Purple::GtkUI  PACKAGE = Purple::GtkUI  PREFIX = pidgin_
PROTOTYPES: ENABLE

BOOT:
	PURPLE_PERL_BOOT(GtkUI__Account);
	PURPLE_PERL_BOOT(GtkUI__BuddyList);
	PURPLE_PERL_BOOT(GtkUI__Connection);
	PURPLE_PERL_BOOT(GtkUI__Conversation);
	PURPLE_PERL_BOOT(GtkUI__Conversation__Window);
	PURPLE_PERL_BOOT(GtkUI__Debug);
	PURPLE_PERL_BOOT(GtkUI__Dialogs);
	PURPLE_PERL_BOOT(GtkUI__IMHtml);
	PURPLE_PERL_BOOT(GtkUI__IMHtmlToolbar);
	PURPLE_PERL_BOOT(GtkUI__Log);
	PURPLE_PERL_BOOT(GtkUI__MenuTray);
	PURPLE_PERL_BOOT(GtkUI__Plugin);
	PURPLE_PERL_BOOT(GtkUI__PluginPref);
	PURPLE_PERL_BOOT(GtkUI__Pounce);
	PURPLE_PERL_BOOT(GtkUI__Prefs);
	PURPLE_PERL_BOOT(GtkUI__Privacy);
	PURPLE_PERL_BOOT(GtkUI__Roomlist);
	PURPLE_PERL_BOOT(GtkUI__Status);
#ifndef _WIN32
	PURPLE_PERL_BOOT(GtkUI__Session);
#endif
	PURPLE_PERL_BOOT(GtkUI__Sound);
	PURPLE_PERL_BOOT(GtkUI__StatusBox);
	PURPLE_PERL_BOOT(GtkUI__Themes);
	PURPLE_PERL_BOOT(GtkUI__Utils);
	PURPLE_PERL_BOOT(GtkUI__Xfer);
