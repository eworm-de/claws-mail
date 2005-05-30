/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
 * Copyright (C) 2001-2002 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "intl.h"
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

static struct Templates {
	GtkWidget *window;
	GtkWidget *ok_btn;
	GtkWidget *clist_tmpls;
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
static void prefs_template_key_pressed_cb	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_template_cancel_cb		(void);
static void prefs_template_ok_cb		(void);
static void prefs_template_select_cb		(GtkCList	*clist,
						 gint		 row,
						 gint		 column,
						 GdkEvent	*event);
static void prefs_template_register_cb		(void);
static void prefs_template_substitute_cb	(void);
static void prefs_template_delete_cb		(void);

/* Called from mainwindow.c */
void prefs_template_open(void)
{
	inc_lock();

	if (!templates.window)
		prefs_template_window_create();

	prefs_template_window_setup();
	gtk_widget_show(templates.window);
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
	GtkWidget         *clist_tmpls;
	GtkWidget       *confirm_area;
	GtkWidget         *ok_btn;
	GtkWidget         *cancel_btn;

	gchar *title[1];

	/* main window */
	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, -1);

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
				       GTK_POLICY_NEVER,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox1), scroll2, TRUE, TRUE, 0);

	text_value = gtk_text_new(NULL, NULL);
	gtk_widget_show(text_value);
	gtk_widget_set_usize(text_value, -1, 120);
	gtk_container_add(GTK_CONTAINER(scroll2), text_value);
	gtk_text_set_editable(GTK_TEXT(text_value), TRUE);
	gtk_text_set_word_wrap(GTK_TEXT(text_value), TRUE);

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
	gtk_widget_set_usize(arrow1, -1, 16);

	hbox3 = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(hbox3);
	gtk_box_pack_start(GTK_BOX(hbox2), hbox3, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label(_("Add"));
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT (reg_btn), "clicked",
			   GTK_SIGNAL_FUNC (prefs_template_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_("  Replace  "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(subst_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_template_substitute_cb),
			   NULL);

	del_btn = gtk_button_new_with_label(_("Delete"));
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(del_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_template_delete_cb), NULL);

	desc_btn = gtk_button_new_with_label(_(" Symbols "));
	gtk_widget_show(desc_btn);
	gtk_box_pack_end(GTK_BOX(hbox2), desc_btn, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(desc_btn), "clicked",
			   GTK_SIGNAL_FUNC(quote_fmt_quote_description), NULL);

	/* templates list */
	scroll1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll1);
	gtk_box_pack_start(GTK_BOX(vbox2), scroll1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll1),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	title[0] = _("Current templates");
	clist_tmpls = gtk_clist_new_with_titles(1, title);
	gtk_widget_show(clist_tmpls);
	gtk_widget_set_usize(scroll1, -1, 140);
	gtk_container_add(GTK_CONTAINER(scroll1), clist_tmpls);
	gtk_clist_set_column_width(GTK_CLIST(clist_tmpls), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST(clist_tmpls),
				     GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist_tmpls)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT (clist_tmpls), "select_row",
			   GTK_SIGNAL_FUNC (prefs_template_select_cb), NULL);

	/* ok | cancel */
	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox2), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Template configuration"));

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(prefs_template_deleted_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefs_template_key_pressed_cb), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_template_ok_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_template_cancel_cb), NULL);

	address_completion_start(window);

	templates.window = window;
	templates.ok_btn = ok_btn;
	templates.clist_tmpls = clist_tmpls;
	templates.entry_name = entry_name;
	templates.entry_subject = entry_subject;
	templates.entry_to = entry_to;
	templates.entry_cc = entry_cc;
	templates.entry_bcc = entry_bcc;	
	templates.text_value = text_value;
}

static void prefs_template_window_setup(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	GSList *tmpl_list;
	GSList *cur;
	gchar *title[1];
	gint row;
	Template *tmpl;

	manage_window_set_transient(GTK_WINDOW(templates.window));
	gtk_widget_grab_focus(templates.ok_btn);

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	title[0] = _("(New)");
	row = gtk_clist_append(clist, title);
	gtk_clist_set_row_data(clist, row, NULL);

	tmpl_list = template_read_config();

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = (Template *)cur->data;
		title[0] = tmpl->name;
		row = gtk_clist_append(clist, title);
		gtk_clist_set_row_data(clist, row, tmpl);
	}

	g_slist_free(tmpl_list);

	gtk_clist_thaw(clist);
}

static void prefs_template_clear(void)
{
	Template *tmpl;
	gint row = 1;

	while ((tmpl = gtk_clist_get_row_data
		(GTK_CLIST(templates.clist_tmpls), row)) != NULL) {
		template_free(tmpl);
		row++;
	}

	gtk_clist_clear(GTK_CLIST(templates.clist_tmpls));
}

static gint prefs_template_deleted_cb(GtkWidget *widget, GdkEventAny *event,
				      gpointer data)
{
	prefs_template_cancel_cb();
	return TRUE;
}

static void prefs_template_key_pressed_cb(GtkWidget *widget,
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
	return;
}

static void prefs_template_ok_cb(void)
{
	GSList *tmpl_list;

	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 _("Yes"), _("No"), NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	tmpl_list = prefs_template_get_list();
	template_set_config(tmpl_list);
	compose_reflect_prefs_all();
	gtk_clist_clear(GTK_CLIST(templates.clist_tmpls));
	gtk_widget_hide(templates.window);
	inc_unlock();
}

static void prefs_template_cancel_cb(void)
{
	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 _("Yes"), _("No"), NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	prefs_template_clear();
	gtk_widget_hide(templates.window);
	inc_unlock();
}

static void prefs_template_select_cb(GtkCList *clist, gint row, gint column,
				     GdkEvent *event)
{
	Template *tmpl;
	Template tmpl_def;

	tmpl_def.name = _("Template");
	tmpl_def.subject = "";
	tmpl_def.to = "";
	tmpl_def.cc = "";
	tmpl_def.bcc = "";	
	tmpl_def.value = "";

	if (!(tmpl = gtk_clist_get_row_data(clist, row)))
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
	
	gtk_text_freeze(GTK_TEXT(templates.text_value));
	gtk_text_set_point(GTK_TEXT(templates.text_value), 0);
	gtk_text_forward_delete
		(GTK_TEXT(templates.text_value), 
		 gtk_text_get_length(GTK_TEXT(templates.text_value)));
	gtk_text_insert(GTK_TEXT(templates.text_value), NULL, NULL, NULL,
	                tmpl->value, -1);
	gtk_text_thaw(GTK_TEXT(templates.text_value));
}

static GSList *prefs_template_get_list(void)
{
	gint row = 1;
	GSList *tmpl_list = NULL;
	Template *tmpl;

	while ((tmpl = gtk_clist_get_row_data
		(GTK_CLIST(templates.clist_tmpls), row)) != NULL) {
		tmpl_list = g_slist_append(tmpl_list, tmpl);
		row++;
	}

	return tmpl_list;
}

static gint prefs_template_clist_set_row(gint row)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	Template *tmp_tmpl;
	gchar *name;
	gchar *subject;
	gchar *to;
	gchar *cc;
	gchar *bcc;	
	gchar *value;
	gchar *title[1];

	g_return_val_if_fail(row != 0, -1);

	value = gtk_editable_get_chars(GTK_EDITABLE(templates.text_value),
				       0, -1);

	if (value && *value != '\0') {
		gchar *parsed_buf;
		MsgInfo dummyinfo;

		memset(&dummyinfo, 0, sizeof(MsgInfo));
		quote_fmt_init(&dummyinfo, NULL, NULL);
		quote_fmt_scan_string(value);
		quote_fmt_parse();
		parsed_buf = quote_fmt_get_buffer();
		if (!parsed_buf) {
			alertpanel_error(_("Template format error."));
			g_free(value);
			return -1;
		}
	}

	name = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_name),
				      0, -1);
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

	title[0] = name;

	if (row < 0) {
		row = gtk_clist_append(clist, title);
	} else {
		gtk_clist_set_text(clist, row, 0, name);
		tmp_tmpl = gtk_clist_get_row_data(clist, row);
		if (tmp_tmpl)
			template_free(tmp_tmpl);
	}

	gtk_clist_set_row_data(clist, row, tmpl);
	return row;
}

static void prefs_template_register_cb(void)
{
	prefs_template_clist_set_row(-1);
}

static void prefs_template_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	tmpl = gtk_clist_get_row_data(clist, row);
	if (!tmpl) return;

	prefs_template_clist_set_row(row);
}

static void prefs_template_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete template"),
		       _("Do you really want to delete this template?"),
		       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	tmpl = gtk_clist_get_row_data(clist, row);
	template_free(tmpl);
	gtk_clist_remove(clist, row);
}
