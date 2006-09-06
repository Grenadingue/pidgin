#include "gtkmodule.h"

MODULE = Gaim::Gtk::Log  PACKAGE = Gaim::Gtk::Log  PREFIX = gaim_gtk_log_
PROTOTYPES: ENABLE

void *
gaim_gtk_log_get_handle()

void
gaim_gtk_log_show(type, screenname, account)
	Gaim::LogType type
	const char * screenname
	Gaim::Account account

void
gaim_gtk_log_show_contact(contact)
	Gaim::BuddyList::Contact contact

MODULE = Gaim::Gtk::Log  PACKAGE = Gaim::Gtk::SysLog  PREFIX = gaim_gtk_syslog_
PROTOTYPES: ENABLE

void
gaim_gtk_syslog_show()