/* Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014-2023 the Claws Mail team and Charles Lehner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. 
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>

#include "defs.h"

#ifdef USE_LDAP
#include "ldapserver.h"
#include "ldapupdate.h"
#endif
#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addrmerge.h"
#include "alertpanel.h"
#include "gtkutils.h"
#include "file-utils.h"
#include "utils.h"
#include "prefs_common.h"

enum
{
	COL_DISPLAYNAME,
	COL_FIRSTNAME,
	COL_LASTNAME,
	COL_NICKNAME,
	N_NAME_COLS
};

enum
{
	SET_ICON,
	SET_PERSON,
	N_SET_COLUMNS
};

static void addrmerge_done(struct AddrMergePage *page)
{
	g_list_free(page->emails);
	g_list_free(page->persons);
	gtk_widget_destroy(GTK_WIDGET(page->dialog));
	g_free(page);
}

static void addrmerge_do_merge(struct AddrMergePage *page)
{
	GList *node;
	ItemEMail *email;
	ItemPerson *person;
	ItemPerson *target = page->target;
	ItemPerson *nameTarget = page->nameTarget;

	gtk_cmclist_freeze(GTK_CMCLIST(page->clist));

	/* Update target name */
	if (nameTarget && nameTarget != target) {
		target->status = UPDATE_ENTRY;
		addritem_person_set_first_name( target, nameTarget->firstName );
		addritem_person_set_last_name( target, nameTarget->lastName );
		addritem_person_set_nick_name( target, nameTarget->nickName );
		addritem_person_set_common_name( target, ADDRITEM_NAME(nameTarget ));
	}

	/* Merge emails into target */
	for (node = page->emails; node; node = node->next) {
		email = node->data;
		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		/* Remove the email from the person */
		email = addrbook_person_remove_email( page->abf, person, email );
		if( email ) {
			addrcache_remove_email( page->abf->addressCache, email );
			/* Add the email to the target */
			addrcache_person_add_email( page->abf->addressCache, target, email );
		}
		person->status = UPDATE_ENTRY;
		addressbook_folder_refresh_one_person( page->clist, person );
	}

	/* Merge persons into target */
	for (node = page->persons; node; node = node->next) {
		GList *nodeE, *nodeA;
		person = node->data;

		if (person == target) continue;
		person->status = DELETE_ENTRY;

		/* Move all emails to the target */
		for (nodeE = person->listEMail; nodeE; nodeE = nodeE->next) {
			email = nodeE->data;
			addritem_person_add_email( target, email );
		}
		g_list_free( person->listEMail );
		person->listEMail = NULL;

		/* Move all attributes to the target */
		for (nodeA = person->listAttrib; nodeA; nodeA = nodeA->next) {
			UserAttribute *attrib = nodeA->data;
			addritem_person_add_attribute( target, attrib );
		}
		g_list_free( person->listAttrib );
		person->listAttrib = NULL;

		/* Remove the person */
		addrselect_list_remove( page->addressSelect, (AddrItemObject *)person );
		addressbook_folder_remove_one_person( page->clist, person );
		if (page->pobj->type == ADDR_ITEM_FOLDER)
			addritem_folder_remove_person(ADAPTER_FOLDER(page->pobj)->itemFolder, person);
		person = addrbook_remove_person( page->abf, person );

		if( person ) {
			gchar *filename = addritem_person_get_picture(person);
			if ((g_strcmp0(person->picture, target->picture) &&
					filename && is_file_exist(filename)))
				claws_unlink(filename);
			if (filename)
				g_free(filename);
			addritem_free_item_person( person );
		}
	}

	addressbook_folder_refresh_one_person( page->clist, target );

	addrbook_set_dirty( page->abf, TRUE );
	addressbook_export_to_file();

#ifdef USE_LDAP
	if (page->ds && page->ds->type == ADDR_IF_LDAP) {
		LdapServer *server = page->ds->rawDataSource;
		ldapsvr_set_modified(server, TRUE);
		ldapsvr_update_book(server, NULL);
	}
#endif
	gtk_cmclist_thaw(GTK_CMCLIST(page->clist));

	addrmerge_done(page);
}

static void addrmerge_dialog_cb(GtkWidget* widget, gint action, gpointer data) {
	struct AddrMergePage* page = data;

	if (action != GTK_RESPONSE_ACCEPT)
		return addrmerge_done(page);

	addrmerge_do_merge(page);
}

static void addrmerge_update_dialog_sensitive( struct AddrMergePage *page )
{
	gboolean canMerge = (page->target && page->nameTarget);
	gtk_dialog_set_response_sensitive( GTK_DIALOG(page->dialog),
			GTK_RESPONSE_ACCEPT, canMerge );
}

static void addrmerge_name_selected( GtkCMCList *clist, gint row, gint column, GdkEvent *event, struct AddrMergePage *page )
{
	ItemPerson *person = gtk_cmclist_get_row_data( clist, row );
	page->nameTarget = person;
	addrmerge_update_dialog_sensitive(page);
}

static void addrmerge_picture_selected(GtkTreeView *treeview,
		struct AddrMergePage *page)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *list;
	ItemPerson *pictureTarget;

	/* Get selected picture target */ 
	model = gtk_icon_view_get_model(GTK_ICON_VIEW(page->iconView));
	list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(page->iconView));
	page->target = NULL;
	if (list != NULL) {
		if (gtk_tree_model_get_iter(model, &iter, (GtkTreePath *)list->data)) {
			gtk_tree_model_get(model, &iter,
					SET_PERSON, &pictureTarget,
					-1);
			page->target = pictureTarget;
		}

		gtk_tree_path_free(list->data);
		g_list_free(list);
	}
	addrmerge_update_dialog_sensitive(page);
}

static void addrmerge_prompt( struct AddrMergePage *page )
{
	GtkWidget *dialog;
	GtkWidget *frame;
	GtkWidget *mvbox, *vbox, *hbox;
	GtkWidget *label;
	GtkWidget *iconView = NULL;
	GtkWidget *namesList = NULL;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GList *node;
	ItemPerson *person;
	GError *error = NULL;
	gchar *msg, *label_msg;

	dialog = page->dialog = gtk_dialog_new_with_buttons (
			_("Merge addresses"),
			GTK_WINDOW(mainwin->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("_Cancel"),
			GTK_RESPONSE_CANCEL,
			_("_Merge"),
			GTK_RESPONSE_ACCEPT,
			NULL);

	g_signal_connect ( dialog, "response",
			G_CALLBACK(addrmerge_dialog_cb), page);

	mvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add(GTK_CONTAINER(
			gtk_dialog_get_content_area(GTK_DIALOG(dialog))), mvbox);
	gtk_container_set_border_width(GTK_CONTAINER(mvbox), 8);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
	gtk_box_pack_start(GTK_BOX(mvbox),
			hbox, FALSE, FALSE, 0);

	msg = page->pickPicture || page->pickName ?
		_("Merging %u contacts." ) :
		_("Really merge these %u contacts?" );
	label_msg = g_strdup_printf(msg,
			g_list_length(page->addressSelect->listSelect));
	label = gtk_label_new( label_msg );
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_label_set_xalign(GTK_LABEL(label),0);
	gtk_label_set_yalign(GTK_LABEL(label),0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	g_free(label_msg);

	if (page->pickPicture) {
		GtkWidget *scrollwinPictures;

		store = gtk_list_store_new(N_SET_COLUMNS,
					GDK_TYPE_PIXBUF,
					G_TYPE_POINTER,
					-1);
		gtk_list_store_clear(store);

		vbox = gtkut_get_options_frame(mvbox, &frame,
				_("Keep which picture?"));
		gtk_container_set_border_width(GTK_CONTAINER(frame), 4);

		scrollwinPictures = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(scrollwinPictures), 1);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwinPictures),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwinPictures),
				GTK_SHADOW_IN);
		gtk_box_pack_start (GTK_BOX (vbox), scrollwinPictures, FALSE, FALSE, 0);
		gtk_widget_set_size_request(scrollwinPictures, 464, 192);

		iconView = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
		gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(iconView), GTK_SELECTION_SINGLE);
		gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(iconView), SET_ICON);
		gtk_container_add(GTK_CONTAINER(scrollwinPictures), GTK_WIDGET(iconView));
		g_signal_connect(G_OBJECT(iconView), "selection-changed",
				G_CALLBACK(addrmerge_picture_selected), page);

		/* Add pictures from persons */
		for (node = page->persons; node; node = node->next) {
			gchar *filename;
			person = node->data;
			filename = addritem_person_get_picture(person);
			if (filename && is_file_exist(filename)) {
				GdkPixbuf *pixbuf;
				GtkWidget *image;

				pixbuf = gdk_pixbuf_new_from_file(filename, &error);
				if (error) {
					debug_print("Failed to read image: \n%s",
							error->message);
					g_error_free(error);
					continue;
				}

				image = gtk_image_new();
				gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);

				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter,
						SET_ICON, pixbuf,
						SET_PERSON, person,
						-1);
			}
			if (filename)
				g_free(filename);
		}
	}

	if (page->pickName) {
		GtkWidget *scrollwinNames;
		gchar *name_titles[N_NAME_COLS];

		name_titles[COL_DISPLAYNAME] = _("Display Name");
		name_titles[COL_FIRSTNAME] = _("First Name");
		name_titles[COL_LASTNAME] = _("Last Name");
		name_titles[COL_NICKNAME] = _("Nickname");

		store = gtk_list_store_new(N_SET_COLUMNS,
					GDK_TYPE_PIXBUF,
					G_TYPE_POINTER,
					-1);
		gtk_list_store_clear(store);

		vbox = gtkut_get_options_frame(mvbox, &frame,
				_("Keep which name?"));
		gtk_container_set_border_width(GTK_CONTAINER(frame), 4);

		scrollwinNames = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(scrollwinNames), 1);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwinNames),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwinNames),
				GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scrollwinNames), FALSE, FALSE, 0);

		namesList = gtk_cmclist_new_with_titles(N_NAME_COLS, name_titles);
		gtk_widget_set_can_focus(GTK_CMCLIST(namesList)->column[0].button, FALSE);
		gtk_cmclist_set_selection_mode(GTK_CMCLIST(namesList), GTK_SELECTION_BROWSE);
		gtk_cmclist_set_column_width(GTK_CMCLIST(namesList), COL_DISPLAYNAME, 164);

		gtk_container_add(GTK_CONTAINER(scrollwinNames), namesList);

		/* Add names from persons */
		for (node = page->persons; node; node = node->next) {
			int row;
			person = node->data;
			gchar *text[N_NAME_COLS];
			text[COL_DISPLAYNAME] = ADDRITEM_NAME(person);
			text[COL_FIRSTNAME] = person->firstName;
			text[COL_LASTNAME] = person->lastName;
			text[COL_NICKNAME] = person->nickName;
			row = gtk_cmclist_insert( GTK_CMCLIST(namesList), -1, text );
			gtk_cmclist_set_row_data( GTK_CMCLIST(namesList), row, person );
		}

		g_signal_connect(G_OBJECT(namesList), "select_row",
				G_CALLBACK(addrmerge_name_selected), page);
	}

	page->iconView = iconView;
	page->namesList = namesList;

	addrmerge_update_dialog_sensitive(page);
	gtk_widget_show_all(dialog);
}

void addrmerge_merge(
		GtkCMCTree *clist,
		AddressObject *pobj,
		AddressDataSource *ds,
		AddrSelectList *list)
{
	struct AddrMergePage* page;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf;
	gboolean procFlag;
	GList *node;
	AddrSelectItem *item;
	AddrItemObject *aio;
	ItemPerson *person, *target = NULL, *nameTarget = NULL;
	GList *persons = NULL, *emails = NULL;
	gboolean pickPicture = FALSE, pickName = FALSE;

	/* Test for read only */
	if( ds->interface->readOnly ) {
		alertpanel_warning(_("This address data is read-only and cannot be deleted."));
		return;
	}

	/* Test whether Ok to proceed */
	procFlag = FALSE;
	if( pobj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(pobj);
		if( ads->subType == ADDR_BOOK ) procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		procFlag = TRUE;
	}
	if( ! procFlag ) return;
	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	/* Gather selected persons and emails */
	for (node = list->listSelect; node; node = node->next) {
		item = node->data;
		aio = ( AddrItemObject * ) item->addressItem;
		if( aio->type == ITEMTYPE_EMAIL ) {
			emails = g_list_prepend(emails, aio);
		} else if( aio->type == ITEMTYPE_PERSON ) {
			persons = g_list_prepend(persons, aio);
		}
	}

	/* Check if more than one person has a picture */
	for (node = persons; node; node = node->next) {
		gchar *filename;
		person = node->data;
		filename = addritem_person_get_picture(person);
		if (filename && is_file_exist(filename)) {
			if (target == NULL) {
				target = person;
			} else {
				pickPicture = TRUE;
				target = NULL;
				g_free(filename);
				break;
			}
		}
		if (filename)
			g_free(filename);
	}
	if (pickPicture || target) {
		/* At least one person had a picture */
	} else if (persons && persons->data) {
		/* No person had a picture. Use the first person as target */
		target = persons->data;
	} else {
		/* No persons in list. Abort */
		goto abort;
	}

	/* Pick which name to keep */
	for (node = persons; node; node = node->next) {
		person = node->data;
		if (nameTarget == NULL) {
			nameTarget = person;
		} else if (nameTarget == person) {
			continue;
		} else if (g_strcmp0(person->firstName, nameTarget->firstName) ||
				g_strcmp0(person->lastName, nameTarget->lastName) ||
				g_strcmp0(person->nickName, nameTarget->nickName) ||
				g_strcmp0(ADDRITEM_NAME(person), ADDRITEM_NAME(nameTarget))) {
			pickName = TRUE;
			break;
		}
	}
	if (!nameTarget) {
		/* No persons in list */
		goto abort;
	}

	/* Create object */
	page = g_new0(struct AddrMergePage, 1);
	page->pickPicture = pickPicture;
	page->pickName = pickName;
	page->target = target;
	page->nameTarget = nameTarget;
	page->addressSelect = list;
	page->persons = persons;
	page->emails = emails;
	page->clist = clist;
	page->pobj = pobj;
	page->abf = abf;
	page->ds = ds;

	addrmerge_prompt(page);
	return;

abort:
	g_list_free( emails );
	g_list_free( persons );
}
