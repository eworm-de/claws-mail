/*
 * Sylpheed templates subsystem 
 * Copyright (C) 2001 Alexander Barinov
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <dirent.h>
#include <sys/stat.h>

#include "intl.h"
#include "manage_window.h"
#include "main.h"
#include "utils.h"
#include "alertpanel.h"
#include "template.h"

static struct Templates {
        GtkWidget *window;
        GtkWidget *ok_btn;
	GtkWidget *clist_tmpls;
	GtkWidget *entry_name;
	GtkWidget *text_value;

	GSList *tmpl_list;
} templates;

/* widget creating functions */
static void prefs_templates_create_window(void);
static void prefs_templates_setup_window(void);

/* callbacks */
static gint prefs_templates_deleted_cb(GtkWidget *widget,
                                       GdkEventAny *event,
                                       gpointer data);
static void prefs_templates_key_pressed_cb(GtkWidget *widget,
					   GdkEventKey *event,
					   gpointer data);
static void prefs_templates_cancel_cb(void);
static void prefs_templates_ok_cb(void);
static void prefs_templates_select_cb(GtkCList *clist,
				      gint row,
				      gint column,
				      GdkEvent *event);
static void prefs_templates_register_cb (void);
static void prefs_templates_substitute_cb (void);
static void prefs_templates_delete_cb (void);

/* Called from mainwindow.c */
void prefs_templates_open(void)
{
	templates.tmpl_list = template_read_config();

	inc_autocheck_timer_remove();

	if(!templates.window){
		prefs_templates_create_window();
	}
	prefs_templates_setup_window();
	gtk_widget_show(templates.window);
}

void prefs_templates_create_window(void)
{
        /* window structure ;) */
        GtkWidget *window;
        GtkWidget   *vpaned;
        GtkWidget     *vbox1;
        GtkWidget       *hbox1;
        GtkWidget         *label1;
        GtkWidget         *entry_name;
        GtkWidget       *scroll2;
        GtkWidget         *text_value;
        GtkWidget     *vbox2;
        GtkWidget       *hbox2;
        GtkWidget         *arrow1;
        GtkWidget         *hbox3;
        GtkWidget           *reg_btn;
        GtkWidget           *subst_btn;
        GtkWidget           *del_btn;
        GtkWidget       *scroll1;
        GtkWidget         *clist_tmpls;
        GtkWidget       *confirm_area;
	GtkWidget         *ok_btn;
	GtkWidget         *cancel_btn;

	gchar *title[] = {_("Registered templates")};

        /* main window */
	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
        gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
        gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

        /* vpaned to separate template settings from templates list */
        vpaned = gtk_vpaned_new();
        gtk_widget_show(vpaned);
        gtk_container_add(GTK_CONTAINER(window), vpaned);

        /* vbox to handle template name and content */
	vbox1 = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox1);
	gtk_paned_pack1(GTK_PANED(vpaned), vbox1, FALSE, FALSE);

        /* hbox for a label and template name entry */
        hbox1 = gtk_hbox_new(FALSE, 12);
        gtk_widget_show(hbox1);
        gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(hbox1), 2);

        /* self-documenting */
        label1 = gtk_label_new(_("Template"));
        gtk_widget_show(label1);
        gtk_box_pack_start(GTK_BOX(hbox1), label1, FALSE, FALSE, 0);

        /* holds template name */
        entry_name = gtk_entry_new();
        gtk_widget_show(entry_name);
        gtk_box_pack_start(GTK_BOX(hbox1), entry_name, TRUE, TRUE, 0);

        /* template content */
        scroll2 = gtk_scrolled_window_new(NULL, NULL);
        gtk_widget_show(scroll2);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll2),
				       GTK_POLICY_NEVER,
                                       GTK_POLICY_ALWAYS);
        gtk_box_pack_start(GTK_BOX(vbox1), scroll2, TRUE, TRUE, 0);

        text_value = gtk_text_new(NULL, NULL);
        gtk_widget_show(text_value);
        gtk_container_add(GTK_CONTAINER(scroll2), text_value);
        gtk_text_set_editable(GTK_TEXT(text_value), TRUE);
        gtk_text_set_word_wrap(GTK_TEXT(text_value), TRUE);

        /* vbox for buttons and templates list */
	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox2);
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

	reg_btn = gtk_button_new_with_label(_("Register"));
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (reg_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_templates_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_(" Substitute "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT(subst_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_templates_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label(_("Delete"));
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(hbox3), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT(del_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_templates_delete_cb), NULL);

        /* templates list */
	scroll1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll1);
	gtk_widget_set_usize(scroll1, -1, 150);
	gtk_box_pack_start(GTK_BOX(vbox2), scroll1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll1),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	clist_tmpls = gtk_clist_new_with_titles(1, title);
	gtk_widget_show(clist_tmpls);
	gtk_container_add(GTK_CONTAINER(scroll1), clist_tmpls);
	gtk_clist_set_column_width(GTK_CLIST(clist_tmpls), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST(clist_tmpls), GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist_tmpls)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (clist_tmpls), "select_row",
			    GTK_SIGNAL_FUNC (prefs_templates_select_cb), NULL);

        /* ok | cancel */
	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox2), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Templates"));

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(prefs_templates_deleted_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefs_templates_key_pressed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_templates_ok_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_templates_cancel_cb), NULL);

	templates.window = window;
	templates.ok_btn = ok_btn;
	templates.clist_tmpls = clist_tmpls;
	templates.entry_name = entry_name;
	templates.text_value = text_value;
}

void prefs_templates_setup_window(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	GSList *cur;
	gchar *cond_str[1];
	gint row;
	Template *tmpl;

	manage_window_set_transient(GTK_WINDOW(templates.window));
	gtk_widget_grab_focus(templates.ok_btn);

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	cond_str[0] = _("(New)");
	row = gtk_clist_append(clist, cond_str);
	gtk_clist_set_row_data(clist, row, NULL);

	for (cur = templates.tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = cur->data;
		cond_str[0] = tmpl->name;
		row = gtk_clist_append(clist, cond_str);
		gtk_clist_set_row_data(clist, row, tmpl);
	}

	gtk_clist_thaw(clist);
}

static gint prefs_templates_deleted_cb(GtkWidget *widget, GdkEventAny *event,
                                       gpointer data)
{
	prefs_templates_cancel_cb();
	return TRUE;
}

static void prefs_templates_key_pressed_cb(GtkWidget *widget, GdkEventKey *event,
				           gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_templates_cancel_cb();
}

static void prefs_templates_ok_cb(void)
{
	template_write_config(templates.tmpl_list);
	template_clear_config(templates.tmpl_list);
	gtk_widget_hide(templates.window);
	inc_autocheck_timer_set();
}

static void prefs_templates_cancel_cb(void)
{
	template_clear_config(templates.tmpl_list);
	gtk_widget_hide(templates.window);
	inc_autocheck_timer_set();
}

static void prefs_templates_select_cb(GtkCList *clist, gint row, gint column,
				      GdkEvent *event)
{
	Template *tmpl;
	Template tmpl_def = {_("Template name"), _("(empty)")};

	if (!(tmpl = gtk_clist_get_row_data(clist, row)))
		tmpl = &tmpl_def;

	gtk_entry_set_text(GTK_ENTRY(templates.entry_name), tmpl->name);
	
	gtk_text_freeze(GTK_TEXT(templates.text_value));
	gtk_text_set_point(GTK_TEXT(templates.text_value), 0);
	gtk_text_forward_delete(GTK_TEXT(templates.text_value), 
	    			gtk_text_get_length(GTK_TEXT(templates.text_value)));
	gtk_text_insert(GTK_TEXT(templates.text_value), NULL, NULL, NULL,
	                tmpl->value, -1);
	gtk_text_thaw(GTK_TEXT(templates.text_value));
}

static void prefs_templates_create_list(void)
{
	gint row = 1;
	Template *tmpl;

	g_slist_free(templates.tmpl_list);
	templates.tmpl_list = NULL;

	while ((tmpl = gtk_clist_get_row_data(GTK_CLIST(templates.clist_tmpls),row)) != NULL) {
		templates.tmpl_list = g_slist_append(templates.tmpl_list, tmpl);
		row++;
	}
}

static gint prefs_templates_clist_set_row(gint row)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	Template *tmp_tmpl;
	gchar *name;
	gchar *value;
	gchar *tmpl_str[1];

	g_return_val_if_fail(row != 0, -1);

	name = gtk_editable_get_chars(GTK_EDITABLE(templates.entry_name),
	                               0, -1);
	value = gtk_editable_get_chars(GTK_EDITABLE(templates.text_value),
	                               0, -1);


	tmpl = g_new(Template, 1);
	tmpl->name = name;
	tmpl->value = value;

	tmpl_str[0] = name;

	if (row < 0) {
		row = gtk_clist_append(clist, tmpl_str);
	} else {
		gtk_clist_set_text(clist, row, 0, name);
		tmp_tmpl = gtk_clist_get_row_data(clist, row);
		if (tmp_tmpl)
			template_free(tmp_tmpl);
	}

	gtk_clist_set_row_data(clist, row, tmpl);
	prefs_templates_create_list();
	return row;
}

static void prefs_templates_register_cb(void)
{
	prefs_templates_clist_set_row(-1);
}

static void prefs_templates_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	tmpl = gtk_clist_get_row_data(clist, row);
	if (!tmpl) return;

	prefs_templates_clist_set_row(row);
}

static void prefs_templates_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(templates.clist_tmpls);
	Template *tmpl;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete template"),
		       _("Do you really want to delete this template?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	tmpl = gtk_clist_get_row_data(clist, row);
	template_free(tmpl);
	gtk_clist_remove(clist, row);
	templates.tmpl_list = g_slist_remove(templates.tmpl_list, tmpl);
}
