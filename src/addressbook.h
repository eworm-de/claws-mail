/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __ADDRESSBOOK_H__
#define __ADDRESSBOOK_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkctree.h>

#include "addressitem.h"
#include "vcard.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#endif

#ifdef USE_LDAP
#include "syldap.h"
#endif

#define ADDRESS_GROUP(obj)		((AddressGroup *)obj)
#define ADDRESS_FOLDER(obj)		((AddressFolder *)obj)
#define ADDRESS_VCARD(obj)		((AddressVCard *)obj)
#define ADDRESS_JPILOT(obj)		((AddressJPilot *)obj)
#define ADDRESS_CATEGORY(obj)		((AddressCategory *)obj)
#define ADDRESS_LDAP(obj)		((AddressLDAP *)obj)


#include "compose.h"

typedef struct _AddressBook	AddressBook;
struct _AddressBook
{
	GtkWidget *window;
	GtkWidget *menubar;
	GtkWidget *ctree;
	GtkWidget *clist;
	GtkWidget *entry;
	GtkWidget *statusbar;

	GtkWidget *del_btn;
	GtkWidget *reg_btn;
	GtkWidget *lup_btn;
	GtkWidget *to_btn;
	GtkWidget *cc_btn;
	GtkWidget *bcc_btn;

	GtkWidget *tree_popup;
	GtkWidget *list_popup;
	GtkItemFactory *tree_factory;
	GtkItemFactory *list_factory;
	GtkItemFactory *menu_factory;

	GtkCTreeNode *common;
	GtkCTreeNode *personal;
	GtkCTreeNode *vcard;
	GtkCTreeNode *jpilot;
	GtkCTreeNode *ldap;
	GtkCTreeNode *selected;
	GtkCTreeNode *opened;

	gboolean open_folder;

	Compose *target_compose;
	gint status_cid;
};

typedef struct _AddressGroup	AddressGroup;
struct _AddressGroup
{
	AddressObject obj;

	gchar *name;

	/* Group contains only Items */
	GList *items;
};

typedef struct _AddressFolder	AddressFolder;
struct _AddressFolder
{
	AddressObject obj;

	gchar *name;

	/* Folder contains Groups and Items */
	GList *items;
};

typedef struct _AddressVCard	AddressVCard;
struct _AddressVCard
{
	AddressObject obj;

	gchar *name;
	VCardFile *cardFile;

	/* Folder contains only VCards */
	GList *items;
};

#ifdef USE_JPILOT
typedef struct _AddressJPilot	AddressJPilot;
struct _AddressJPilot
{
	AddressObject obj;

	gchar *name;
	JPilotFile *pilotFile;

	/* Folder contains only JPilotFiles */
	/* Folder contains only Items for each category */
	GList *items;
};

typedef struct _AddressCategory	AddressCategory;
struct _AddressCategory
{
	AddressObject obj;

	gchar *name;
	JPilotFile *pilotFile;
	AddressItem *category;

	/* Category contains only Items */
	GList *items;
};
#endif

#ifdef USE_LDAP
typedef struct _AddressLDAP	AddressLDAP;
struct _AddressLDAP
{
	AddressObject obj;

	gchar *name;
	SyldapServer *ldapServer;

	/* Folder contains only SyldapServers */
	GList *items;
};
#endif

struct _AddressFileSelection {
	GtkWidget *fileSelector;
	gboolean cancelled;
};
typedef struct _AddressFileSelection AddressFileSelection;

void addressbook_open			(Compose	*target);
void addressbook_set_target_compose	(Compose	*target);
Compose *addressbook_get_target_compose	(void);
void addressbook_export_to_file		(void);

/* provisional API for accessing the address book */

void addressbook_access (void);
void addressbook_unaccess (void);

const gchar *addressbook_get_personal_folder_name (void);
const gchar *addressbook_get_common_folder_name (void);

AddressObject *addressbook_find_group_by_name (const gchar *name);
AddressObject *addressbook_find_contact (const gchar *name, const gchar *address);
GList *addressbook_get_group_list (void);
gint addressbook_add_contact  (const gchar *group, const gchar *name, 
                               const gchar *address, const gchar *remarks); 

gboolean addressbook_add_submenu(GtkWidget   *submenu,
				 const gchar *name,
				 const gchar *address,
				 const gchar *remarks);

#endif /* __ADDRESSBOOK_H__ */
