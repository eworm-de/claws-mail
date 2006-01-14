/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
 * Copyright (C) 2001-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "template.h"
#include "main.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "compose.h"
#include "addr_compl.h"
#include "quote_fmt.h"
#include "prefs_common.h"

enum {
	TEMPL_TEXT,
	TEMPL_DATA,
	TEMPL_AUTO_DATA,	/*!< auto pointer */
	N_TEMPL_COLUMNS
};

static struct Templates {
	GtkWidget *window;
	GtkWidget *ok_btn;
	GtkWidget *list_view;
	GtkWidget *entry_name;
	GtkWidget *entry_subject;
	GtkWidget *entry_to;
	GtkWidget *entry_cc;	
	GtkWidget *entry_bcc;
	GtkWidget *text_value;
} templates;

static int modified = FALSE;

/* widget creating functions */
static void prefs_template_window_create	(void);
static void prefs_template_window_setup		(void);
static void prefs_template_clear		(void);

static GSList *prefs_template_get_list		(void);

/* callbacks */
static gint prefs_template_deleted_cb		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static gboolean prefs_template_key_pressed_cb	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_template_cancel_cb		(void);
static void prefs_template_ok_cb		(void);
static void prefs_template_register_cb		(void);
static void prefs_template_substitute_cb	(void);
static void prefs_template_delete_cb		(void);

static GtkListStore* prefs_template_create_data_store	(void);
static void prefs_template_list_view_insert_template	(GtkWidget *list_view,
							 GtkTreeIter *row_iter,
							 const gchar *template,
							 Template *data,
							 gboolean sorted);
static GtkWidget *prefs_template_list_view_create	(void);
static void prefs_template_create_list_view_columns	(GtkWidget *list_view);
static gboolean prefs_template_selected			(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

/* Called from mainwindow.c */
void prefs_template_open(void)
{
	inc_lock();

	if (!templates.window)
		prefs_template_window_create();

	prefs_template_window_setup();
	gtk_widget_show(templates.window);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void prefs_template_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.templateswin_width = allocation->width;
	prefs_common.templateswin_height = allocation->height;
}

#define ADD_ENTRY(entry, str, row) \
{ \
	label1 = gtk_label_new(str); \
	gtk_widget_show(label1); \
	gtk_table_attach(GTK_TABLE(table), label1, 0, 1, row, (row + 1), \
			 GTK_FILL, 0, 0, 0); \
	gtk_misc_set_alignment(GTK_MISC(label1), 1, 0.5); \
 \
	entry = gtk_entry_new(); \
	gtk_widget_show(entry); \
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, (row + 1), \
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0); \
}

static void prefs_template_window_create(void)
{
	/* window structure ;) */
	GtkWidget *window;
	GtkWidget   *vpaned;
	GtkWidget     *vbox1;
	GtkWidget       *hbox1;
	GtkWidget         *label1;
	GtkWidget         *entry_name;
	GtkWidget       *table;
	GtkWidget         *entry_to;
	GtkWidget         *entry_cc;
	GtkWidget         *entry_bcc;		
	GtkWidget         *entry_subject;
	GtkWidget       *scroll2;
	GtkWidget         *text_value;
	GtkWidget     *vbox2;
	GtkWidget       *hbox2;
	GtkWidget         *arrow1;
	GtkWidget         *hbox3;
	GtkWidget           *reg_btn;
	GtkWidget           *subst_btn;
	GtkWidget           *del_btn;
	GtkWidget         *desc_btn;
	GtkWidget       *scroll1;
	GtkWidget         *list_view;
	GtkWidget       *confirm_area;
	GtkWidget         *ok_btn;
	GtkWidget         *cancel_btn;
	static GdkGeometry geometry;

	debug_print("Creating templates configuration window...\n");

	/* main window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	/* vpaned to separate template settings from templates list */
	vpaned = gtk_vpaned_new();
	gtk_widget_show(vpaned);
	gtk_container_add(GTK_CONTAINER(window), vpaned);

	/* vbox to handle template name and content */
	vbox1 = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox1);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 8);
	gtk_paned_pack1(GTK_PANED(vpaned), vbox1, FALSE, FALSE);

	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

	label1 = gtk_label_new(_("Template name"));
	gtk_widget_show(label1);
	gtk_box_pack_start(GTK_BOX(hbox1), label1, FALSE, FALSE, 0);

	entry_name = gtk_entry_new();
	gtk_widget_show(entry_name);
	gtk_box_pack_start(GTK_BOX(hbox1), entry_name, TRUE, TRUE, 0);

	/* table for headers */
	table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	ADD_ENTRY(entry_to, _("To:"), 0);
	address_completion_register_entry(GTK_ENTRY(entry_to));
	ADD_ENTRY(entry_cc, _("Cc:"), 1)
	ADD_ENTRY(entry_bcc, _("Bcc:"), 2)	
	ADD_ENTRY(entry_subject, _("Subject:"), 3);

#undef ADD_ENTRY

	/* template content */
	scroll2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll2);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll2),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll2),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox1), scroll2, TRUE, TRUE, 0);

	text_value = gtk_text_view_new();
	gtk_widget_show(text_value);
	gtk_widget_set_size_request(text_value, -1, 120);
	gtk_container_add(GTK_CONTAINER(scroll2), text_value);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_value), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_value), GTK_WRAP_WORD);

	/* vbox for buttons and templates list */
	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox2);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox2, TRUE, FALSE);

	/* register | substitute | delete */
	hbox2 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox2);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, 0);

	arrow1 = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow1);
	gtk_box_pack_start(GTK_BOX(hbox2), arrow1, FALSE, FALSE, 0);
	gtk_widget_set_size_request(arrow1, -1, 16);

	hbox3 = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(hbox3);
	gtk_box_pack_start(GTK_BOX(hbox2), hbox3, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT (reg_btn), "clicked",
			 G_CALLBACK (prefs_template_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_("  Replace  "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(subst_btn), "clicked",
			 G_CALLBACK(prefs_template_substitute_cb),
			 NULL);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_template_delete_cb), NULL);

	desc_btn = gtk_button_new_with_label(_(" Symbols... "));
	gtk_widget_show(desc_btn);
	gtk_box_pack_end(GTK_BOX(hbox2), desc_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(desc_btn), "clicked",
			 G_CALLBACK(quote_fmt_quote_description), NULL);

	/* templates list */
	scroll1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll1);
	gtk_box_pack_start(GTK_BOX(vbox2), scroll1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll1),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
				       
	list_view = prefs_template_list_view_create();
	gtk_widget_show(list_view);
	gtk_widget_set_size_request(scroll1, -1, 140);
	gtk_container_add(GTK_CONTAINER(scroll1), list_view);

	/* ok | cancel */
	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, GTK_STOCK_CANCEL,
				      &ok_btn, GTK_STOCK_OK, NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox2), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Template configuration"));

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_template_deleted_cb), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(prefs_template_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_template_key_pressed_cb), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_template_ok_cb), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(prefs_template_cancel_cb), NULL);

	address_completion_start(window);

	if (!geometry.min_height) {
		geometry.min_width = 440;
		geometry.min_height = 500;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_widget_set_size_request(window, prefs_common.templateswin_width,
				    prefs_common.templateswin_height);

	templates.window = window;
	templates.ok_btn = ok_btn;
	templates.list_view = list_view;
	templates.entry_name = entry_name;
	templates.entry_subject = entry_subject;
	templates.entry_to = entry_to;
	templates.entry_cc = entry_cc;
	templates.entry_bcc = entry_bcc;	
	templates.text_value = text_value;
}

static void prefs_template_window_setup(void)
{
	GSList *tmpl_list;
	GSList *cur;
	Template *tmpl;
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW
				(templates.list_view)));

	manage_window_set_transient(GTK_WINDOW(templates.window));
	gtk_widget_grab_focus(templates.ok_btn);

	gtk_list_store_clear(store);

	prefs_template_list_view_insert_template(templates.list_view,
						 NULL, _("(New)"),
						 NULL, FALSE);
	
	tmpl_list = template_read_config();

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = (Template *)cur->data;
		prefs_template_list_view_insert_template(templates.list_view,
							 NULL, tmpl->name, 
							 tmpl, FALSE);
	}

	g_slist_free(tmpl_list);
}

static void prefs_template_clear(void)
{
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW
				(templates.list_view)));
	gtk_list_store_clear(store);
}

static gint prefs_template_deleted_cb(GtkWidget *widget, GdkEventAny *event,
				      gpointer data)
{
	prefs_template_cancel_cb();
	return TRUE;
}

static gboolean prefs_template_key_pressed_cb(GtkWidget *widget,
					      GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_template_cancel_cb();
	else {
		GtkWidget *focused = gtkut_get_focused_child(
					GTK_CONTAINER(widget));
		if (focused && GTK_IS_EDITABLE(focused)) {
			modified = TRUE;
		}
	}
	return FALSE;
}

static void prefs_template_ok_cb(void)
{
	GSList *tmpl_list;

	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_NO, GTK_STOCK_YES,
				 NULL) != G_ALERTALTERNATE) {
		return;
	}
	modified = FALSE;
	tmpl_list = prefs_template_get_list();
	template_set_config(tmpl_list);
	compose_reflect_prefs_all();
	prefs_template_clear();
	gtk_widget_hide(templates.window);
	inc_unlock();
}

static void prefs_template_cancel_cb(void)
{
	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_NO, GTK_STOCK_YES,
				 NULL) != G_ALERTALTERNATE) {
		return;
	}
	modified = FALSE;
	prefs_template_clear();
	gtk_widget_hide(templates.window);
	inc_unlock();
}

/*!
 *\brief	Request list for storage. New list is owned
 *		by template.c...
 */
static GSList *prefs_template_get_list(void)
{
	GSList *tmpl_list = NULL;
	Template *tmpl;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(templates.list_view));
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return NULL;

	do {
		gtk_tree_model_get(model, &iter,
				   TEMPL_DATA, &tmpl,
				   -1);
		
		if (tmpl) {
			Template *ntmpl;
			
			ntmpl = g_new(Template, 1);
			ntmpl->name    = tmpl->name && *(tmpl->name) 
					 ? g_strdup(tmpl->name) 
					 : NULL;
			ntmpl->subject = tmpl->subject && *(tmpl->subject) 
					 ? g_strdup(tmpl->subject) 
					 : NULL;
			ntmpl->to      = tmpl->to && *(tmpl->to)
					 ? g_strdup(tmpl->to)
					 : NULL;
			ntmpl->cc      = tmpl->cc && *(tmpl->cc)
					 ? g_strdup(tmpl->cc)
					 : NULL;
			ntmpl->bcc     = tmpl->bcc && *(tmpl->bcc)
					 ? g_strdup(tmpl->bcc)
					 : NULL;	
			ntmpl->value   = tmpl->value && *(tmpl->value)
			                 ? g_strdup(tmpl->value)
					 : NULL;
			tmpl_list = g_slist_append(tmpl_list, ntmpl);
		}			
	
	} while (gtk_tree_model_iter_next(model, &iter)); 

	return tmpl_list;
}

static gboolean prefs_template_list_view_set_row(GtkTreeIter *row)
/* return TRUE if the row could be modified */
{
	Template *tmpl;
	gchar *name;
	gchar *subject;
	gchar *to;
	gchar *cc;
	gchar *bcc;	
	gchar *value;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(templates.list_view));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(templates.text_value));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end, -1);
	value = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	if (value && *value != '\0') {
		gchar *parsed_buf;
		MsgInfo dummyinfo;

		memset(&dummyinfo, 0, sizeof(MsgInfo));
		quote_fmt_init(&dummyinfo, NULL, NULL, TRUE);
		quote_fmt_scan_string(value);
		quote_fmt_parse();
		parsed_buf = quote_fmt_get_buffer();
		if (!parsed_buf) {
			alertpanel_error(_("Template format error."));
			g_free(value);
			return FALSE;
		}
	}

	name = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_name),
				      0, -1);
	if (name[0] == '\0') {
		alertpanel_error(_("Template name is not set."));
		return FALSE;
	}
	subject = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_subject),
					 0, -1);
	to = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_to),
				    0, -1);
	cc = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_cc),
				    0, -1);
	bcc = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_bcc),
				    0, -1);

	if (subject && *subject == '\0') {
		g_free(subject);
		subject = NULL;
	}
	if (to && *to == '\0') {
		g_free(to);
		to = NULL;
	}
	if (cc && *cc == '\0') {
		g_free(cc);
		cc = NULL;
	}
	if (bcc && *bcc == '\0') {
		g_free(bcc);
		bcc = NULL;
	}
	
	tmpl = g_new(Template, 1);
	tmpl->name = name;
	tmpl->subject = subject;
	tmpl->to = to;
	tmpl->cc = cc;
	tmpl->bcc = bcc;	
	tmpl->value = value;

	prefs_template_list_view_insert_template(templates.list_view,
						 row, tmpl->name, tmpl, TRUE);
	return TRUE;
}

static void prefs_template_register_cb(void)
{
	modified = !prefs_template_list_view_set_row(NULL);
}

static void prefs_template_substitute_cb(void)
{
	Template *tmpl;
	GtkTreeIter row;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW(templates.list_view));
	
	if (!gtk_tree_selection_get_selected(selection, &model, &row))
		return;

	gtk_tree_model_get(model, &row, 
			   TEMPL_DATA, &tmpl,
			   -1);

	if (!tmpl) return;

	modified = !prefs_template_list_view_set_row(&row);
}

static void prefs_template_delete_cb(void)
{
	Template *tmpl;
	GtkTreeIter row;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW(templates.list_view));
	
	if (!gtk_tree_selection_get_selected(selection, &model, &row))
		return;

	gtk_tree_model_get(model, &row, 
			   TEMPL_DATA, &tmpl,
			   -1);

	if (!tmpl) 
		return;

	if (alertpanel(_("Delete template"),
		       _("Do you really want to delete this template?"),
		       GTK_STOCK_NO, GTK_STOCK_YES,
		       NULL) != G_ALERTALTERNATE)
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &row);		
}

static GtkListStore* prefs_template_create_data_store(void)
{
	return gtk_list_store_new(N_TEMPL_COLUMNS,
				  G_TYPE_STRING,	
				  G_TYPE_POINTER,
				  G_TYPE_AUTO_POINTER,
				  -1);
}

static gboolean prefs_template_list_view_find_insertion_point(GtkTreeModel* model,
							const gchar* template,
							GtkTreeIter* iter)
{
	GtkTreeIter i;
	gchar *text = NULL;

	if ((gtk_tree_model_get_iter_first(model, &i))) {
		for ( ; gtk_tree_model_iter_next(model, &i); ) {
			gtk_tree_model_get(model, &i, TEMPL_TEXT, &text, -1);
			if (strcmp(text, template) >= 0) {
				g_free(text);
				*iter = i;
				return TRUE;
			}
			g_free(text);
		}
	}
	/* cannot add before (need to append) */
	return FALSE;
}

static void prefs_template_list_view_insert_template(GtkWidget *list_view,
						     GtkTreeIter *row_iter,
						     const gchar *template,
						     Template *data,
							 gboolean sorted)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view));
	GtkListStore *list_store = GTK_LIST_STORE(model);
	GAuto *auto_data;					

	if (row_iter == NULL) {
		if (sorted) {
			GtkTreeIter insertion_point;

			/* insert sorted */
			if (prefs_template_list_view_find_insertion_point(model,
					template, &insertion_point)) {
				gtk_list_store_insert_before(list_store, &iter,
						&insertion_point);
			} else {
				gtk_list_store_append(list_store, &iter);
			}
		} else {
			/* append new */
			gtk_list_store_append(list_store, &iter);
		}
	} else {
		/* modify the existing */
		iter = *row_iter;

		if (sorted) {
			GtkTreeIter insertion_point;
			gchar *text = NULL;

			/* force re-sorting if template's name changed */
			gtk_tree_model_get(model, row_iter, TEMPL_TEXT, &text, -1);
			if (strcmp(text, template) != 0) {

				/* move the modified template */
				if (prefs_template_list_view_find_insertion_point(model,
						template, &insertion_point)) {
					gtk_list_store_move_before(list_store, row_iter,
							&insertion_point);
				} else {
					/* move to the end */
					gtk_list_store_move_before(list_store, row_iter,
							NULL);
				}
			}
			g_free(text);
		}
	}
		
	auto_data = g_auto_pointer_new_with_free(data, (GFreeFunc) template_free);  

	/* if replacing data in an existing row, the auto pointer takes care
	 * of destroying the Template data */
	gtk_list_store_set(list_store, &iter,
			   TEMPL_TEXT, template,
			   TEMPL_DATA, data,
			   TEMPL_AUTO_DATA, auto_data,
			   -1);

	g_auto_pointer_free(auto_data);			   
}

static GtkWidget *prefs_template_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(prefs_template_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.enable_rules_hint);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefs_template_selected,
					       NULL, NULL);

	/* create the columns */
	prefs_template_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_template_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Current templates"),
			 renderer,
			 "text", TEMPL_TEXT,
			 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

static gboolean prefs_template_selected(GtkTreeSelection *selector,
				        GtkTreeModel *model, 
				        GtkTreePath *path,
				        gboolean currently_selected,
				        gpointer data)
{
	Template *tmpl;
	Template tmpl_def;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTreeIter titer;

	if (currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &titer, path))
		return TRUE;

	tmpl_def.name = _("Template");
	tmpl_def.subject = "";
	tmpl_def.to = "";
	tmpl_def.cc = "";
	tmpl_def.bcc = "";	
	tmpl_def.value = "";

	gtk_tree_model_get(model, &titer,
			   TEMPL_DATA, &tmpl,
			   -1);

	if (!tmpl) 
		tmpl = &tmpl_def;

	gtk_entry_set_text(GTK_ENTRY(templates.entry_name), tmpl->name);
	gtk_entry_set_text(GTK_ENTRY(templates.entry_to),
			   tmpl->to ? tmpl->to : "");
	gtk_entry_set_text(GTK_ENTRY(templates.entry_cc),
			   tmpl->cc ? tmpl->cc : "");
	gtk_entry_set_text(GTK_ENTRY(templates.entry_bcc),
			   tmpl->bcc ? tmpl->bcc : "");			
	gtk_entry_set_text(GTK_ENTRY(templates.entry_subject),
			   tmpl->subject ? tmpl->subject : "");
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(templates.text_value));
	gtk_text_buffer_set_text(buffer, "", -1);
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_insert(buffer, &iter, tmpl->value, -1);

	return TRUE;
}
