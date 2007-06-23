/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2007, Andreas Monitzer <andy@monitzer.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	 02111-1307	 USA
 *
 */

#include "adhoccommands.h"
#include <assert.h>
#include <string.h>
#include "internal.h"
#include "xdata.h"
#include "iq.h"
#include "request.h"

static void do_adhoc_ignoreme(JabberStream *js, ...) {
	/* we don't have to do anything */
}

typedef struct _JabberAdHocActionInfo {
	char *sessionid;
	char *who;
	char *node;
	GList *actionslist;
} JabberAdHocActionInfo;

void jabber_adhoc_disco_result_cb(JabberStream *js, xmlnode *packet, gpointer data) {
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *node;
	xmlnode *query, *item;
	JabberID *jabberid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	
	if(strcmp(type, "result"))
		return;
	
	query = xmlnode_get_child_with_namespace(packet,"query","http://jabber.org/protocol/disco#items");
	if(!query)
		return;
	node = xmlnode_get_attrib(query,"node");
	if(!node || strcmp(node, "http://jabber.org/protocol/commands"))
		return;
	
	if((jabberid = jabber_id_new(from))) {
		if(jabberid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
			jbr = jabber_buddy_find_resource(jb, jabberid->resource);
		jabber_id_free(jabberid);
	}
	
	if(!jbr)
		return;
	
	if(jbr->commands) {
		/* since the list we just received is complete, wipe the old one */
		while(jbr->commands) {
			JabberAdHocCommands *cmd = jbr->commands->data;
			g_free(cmd->jid);
			g_free(cmd->node);
			g_free(cmd->name);
			g_free(cmd);
			jbr->commands = g_list_delete_link(jbr->commands, jbr->commands);
		}
	}
	
	for(item = query->child; item; item = item->next) {
		JabberAdHocCommands *cmd;
		if(item->type != XMLNODE_TYPE_TAG)
			continue;
		if(strcmp(item->name, "item"))
			continue;
		cmd = g_new0(JabberAdHocCommands, 1);
		
		cmd->jid = g_strdup(xmlnode_get_attrib(item,"jid"));
		cmd->node = g_strdup(xmlnode_get_attrib(item,"node"));
		cmd->name = g_strdup(xmlnode_get_attrib(item,"name"));
		
		jbr->commands = g_list_append(jbr->commands,cmd);
	}
}

static void do_adhoc_action_cb(JabberStream *js, xmlnode *result, const char *actionhandle, gpointer user_data) {
	xmlnode *command;
	GList *action;
	JabberAdHocActionInfo *actionInfo = user_data;
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
/*	jabber_iq_set_callback(iq, do_adhoc_parse_iq, NULL);*/
	
	xmlnode_set_attrib(iq->node, "to", actionInfo->who);
	command = xmlnode_new_child(iq->node,"command");
	xmlnode_set_namespace(command,"http://jabber.org/protocol/commands");
	xmlnode_set_attrib(command,"sessionid",actionInfo->sessionid);
	xmlnode_set_attrib(command,"node",actionInfo->node);
	if(actionhandle)
		xmlnode_set_attrib(command,"action",actionhandle);
	xmlnode_insert_child(command,result);
	
	for(action = actionInfo->actionslist; action; action = g_list_next(action)) {
		char *handle = action->data;
		g_free(handle);
	}
	g_list_free(actionInfo->actionslist);
	g_free(actionInfo->sessionid);
	g_free(actionInfo->who);
	g_free(actionInfo->node);
	
	jabber_iq_send(iq);
}

void jabber_adhoc_parse(JabberStream *js, xmlnode *packet) {
	xmlnode *command = xmlnode_get_child_with_namespace(packet, "command", "http://jabber.org/protocol/commands");
	const char *status = xmlnode_get_attrib(command,"status");
	xmlnode *xdata = xmlnode_get_child_with_namespace(command,"x","jabber:x:data");
	
	if(!status)
		return;
	
	if(!strcmp(status,"completed")) {
		/* display result */
		xmlnode *note = xmlnode_get_child(command,"note");
		
		if(note)
			purple_notify_info(NULL, xmlnode_get_attrib(packet, "from"), xmlnode_get_data(note), NULL);
		
		if(xdata)
			jabber_x_data_request(js, xdata, (jabber_x_data_cb)do_adhoc_ignoreme, NULL);
		return;
	}
	if(!strcmp(status,"executing")) {
		/* this command needs more steps */
		xmlnode *actions, *action;
		int actionindex = 0;
		GList *actionslist = NULL;
		JabberAdHocActionInfo *actionInfo;
		if(!xdata)
			return; /* shouldn't happen */
		
		actions = xmlnode_get_child(command,"actions");
		if(!actions) {
			JabberXDataAction *defaultaction = g_new0(JabberXDataAction, 1);
			defaultaction->name = g_strdup(_("execute"));
			defaultaction->handle = g_strdup("execute");
			actionslist = g_list_append(actionslist, defaultaction);
		} else {
			const char *defaultactionhandle = xmlnode_get_attrib(actions, "execute");
			int index = 0;
			for(action = actions->child; action; action = action->next, ++index) {
				if(action->type == XMLNODE_TYPE_TAG) {
					JabberXDataAction *newaction = g_new0(JabberXDataAction, 1);
					newaction->name = g_strdup(_(action->name));
					newaction->handle = g_strdup(action->name);
					actionslist = g_list_append(actionslist, newaction);
					if(defaultactionhandle && !strcmp(defaultactionhandle, action->name))
						actionindex = index;
				}
			}
		}
		
		actionInfo = g_new0(JabberAdHocActionInfo, 1);
		actionInfo->sessionid = g_strdup(xmlnode_get_attrib(command,"sessionid"));
		actionInfo->who = g_strdup(xmlnode_get_attrib(packet,"from"));
		actionInfo->node = g_strdup(xmlnode_get_attrib(command,"node"));
		actionInfo->actionslist = actionslist;
		
		jabber_x_data_request_with_actions(js,xdata,actionslist,actionindex,do_adhoc_action_cb,actionInfo);
	}
}

void jabber_adhoc_execute(PurpleBlistNode *node, gpointer data) {
	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		JabberAdHocCommands *cmd = data;
		PurpleBuddy *buddy = (PurpleBuddy *) node;
		JabberStream *js = purple_account_get_connection(buddy->account)->proto_data;
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
		xmlnode *command = xmlnode_new_child(iq->node,"command");
		xmlnode_set_attrib(iq->node,"to",cmd->jid);
		xmlnode_set_namespace(command,"http://jabber.org/protocol/commands");
		xmlnode_set_attrib(command,"node",cmd->node);
		xmlnode_set_attrib(command,"action","execute");
		
		/* we don't need to set a callback, since jabber_adhoc_parse is run for all replies */
		
		jabber_iq_send(iq);
	}
}

