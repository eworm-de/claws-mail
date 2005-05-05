/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>

#include "progressdialog.h"
#include "gtkutils.h"
#include "utils.h"

enum {
	PROGRESS_IMAGE,
	PROGRESS_ACCOUNT,
	PROGRESS_STATE,
	N_PROGRESS_COLUMNS
};


static GtkListStore* progress_dialog_create_data_store(void);
static gint progress_dialog_list_view_insert_account(GtkWidget   *list_view,
						     gint	  row,
						     const gchar *account,
						     const gchar *status,
						     GdkPixbuf	 *image);
static GtkWidget *progress_dialog_list_view_create(void);
static void progress_dialog_create_list_view_columns(GtkTreeView *list_view);

ProgressDialog *progress_dialog_create(void)
{
	ProgressDialog *progress;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *cancel_btn;
	GtkWidget *progressbar;
	GtkWidget *scrolledwin;
	GtkWidget *clist;
	GtkWidget *list_view;
	gchar *text[] = {NULL, NULL, NULL};

	text[1] = _("Account");
	text[2] = _("Status");

	debug_print("Creating progress dialog...\n");
	progress = g_new0(ProgressDialog, 1);

	dialog = gtk_dialog_new();
	gtk_widget_set_size_request(dialog, 460, -1);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, TRUE);
	gtk_widget_realize(dialog);

	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 0);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 8);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
			   FALSE, FALSE, 8);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);
	gtk_widget_show(label);

	cancel_btn = gtk_dialog_add_button(GTK_DIALOG(dialog),
					   GTK_STOCK_CANCEL,
					   GTK_RESPONSE_NONE);
	gtk_widget_grab_default(cancel_btn);
	gtk_widget_grab_focus(cancel_btn);

	progressbar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), progressbar,
			   FALSE, FALSE, 0);
	gtk_widget_show(progressbar);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	/* GTK2: we hide the clist, but it is available for migration
	 * purposes. now if there only was a way to catch "set clist 
	 * things"!.. */
	clist = gtk_clist_new_with_titles(3, text);
	gtk_widget_hide(clist);
	/* gtk_container_add(GTK_CONTAINER(scrolledwin), clist); */
	/* gtk_widget_set_size_request(clist, -1, 120); */
	gtk_clist_set_column_justification(GTK_CLIST(clist), 0,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_width(GTK_CLIST(clist), 0, 16);
	gtk_clist_set_column_width(GTK_CLIST(clist), 1, 160);

	list_view = progress_dialog_list_view_create();
	gtk_widget_show(list_view);
	gtk_container_add(GTK_CONTAINER(scrolledwin), list_view);
	gtk_widget_set_size_request(list_view, -1, 120);

	progress->window      = dialog;
	progress->label       = label;
	progress->cancel_btn  = cancel_btn;
	progress->progressbar = progressbar;
	progress->clist       = clist;
	progress->list_view   = list_view;

	return progress;
}

void progress_dialog_set_label(ProgressDialog *progress, gchar *str)
{
	gtk_label_set_text(GTK_LABEL(progress->label), str);
}

void progress_dialog_get_fraction(ProgressDialog *progress)
{
	gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(progress->progressbar));
}

void progress_dialog_set_fraction(ProgressDialog *progress,
				  gfloat percentage)
{
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress->progressbar),
				      percentage);
}

void progress_dialog_destroy(ProgressDialog *progress)
{
	if (progress) {
		gtk_widget_destroy(progress->window);
		g_free(progress);
	}
}

/*!
 *\return	gint Row where account was set
 */
gint progress_dialog_list_set_account(ProgressDialog *progress,
				      gint	      row,
				      const gchar    *account_name)
{
	return progress_dialog_list_view_insert_account(progress->list_view,
							row, account_name, NULL, 
							NULL);
}

/*!
 *\return	gint Row where image was set
 */
gint progress_dialog_list_set_image(ProgressDialog *progress,
				    gint	    row,
				    GdkPixbuf	   *image)
{
	return progress_dialog_list_view_insert_account(progress->list_view,
							row, NULL, NULL, 
							image);
}

/*!
 *\return	gint Row where status was set
 */
gint progress_dialog_list_set_status(ProgressDialog *progress,
				     gint	     row,
				     const gchar    *status)
{
	return progress_dialog_list_view_insert_account(progress->list_view,
							row, NULL, status, 
							NULL);
}

/*!
 *\return	gint Row where data were set 
 */
gint progress_dialog_list_set(ProgressDialog	*progress,
			      gint		 row,
			      GdkPixbuf		*image,
			      const gchar	*account_name,
			      const gchar	*status)
{
	return progress_dialog_list_view_insert_account(progress->list_view,
							row,  account_name, 
							status, image);
}

/* XXX: maybe scroll into view, but leaving that for someone else to
 * pickup: I don't have that many accounts... */
gboolean progress_dialog_list_select_row(ProgressDialog *progress,
					 gint		 row)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection
					(GTK_TREE_VIEW(progress->list_view));
	GtkTreeIter iter;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(progress->list_view));

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return FALSE;

	gtk_tree_selection_select_iter(selection, &iter);		

	return TRUE;
}

static GtkListStore* progress_dialog_create_data_store(void)
{
	return gtk_list_store_new(N_PROGRESS_COLUMNS,
				  GDK_TYPE_PIXBUF,
				  G_TYPE_STRING,
				  G_TYPE_STRING,	
				  -1);
}

static gint progress_dialog_list_view_insert_account(GtkWidget   *list_view,
						     gint	  row,
						     const gchar *account,
						     const gchar *status,
						     GdkPixbuf	 *image)
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));
	gint result = -1;					
	
	if (account == NULL && status == NULL && image == NULL)
		return -1;

	/* see if row exists, if not append */
	if (row < 0 || !gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), 
						      &iter, NULL, row)) {
		result = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store),
							NULL);
		gtk_list_store_append(store, &iter);
	} else
		result = row;

	/*
	 * XXX: uhm, when does the iter invalidate? sure not while
	 * just setting a row's column i hope?
	 */
	
	if (account)
		gtk_list_store_set(store, &iter, 
				   PROGRESS_ACCOUNT, account,
				   -1);
	if (status)
		gtk_list_store_set(store, &iter,
				   PROGRESS_STATE, status,
				   -1);
	if (image)
		gtk_list_store_set(store, &iter,
				   PROGRESS_IMAGE, image,
				   -1);

	return result;
}

static GtkWidget *progress_dialog_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(progress_dialog_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, TRUE);
	
	/* create the columns */
	progress_dialog_create_list_view_columns(list_view);

	return GTK_WIDGET(list_view);
}

static void progress_dialog_create_list_view_columns(GtkTreeView *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes
			("", renderer, 
			 "pixbuf", PROGRESS_IMAGE,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);			 

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Account"),
		 renderer,
		 "text", PROGRESS_ACCOUNT,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Status"),
		 renderer,
		 "text", PROGRESS_STATE,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

