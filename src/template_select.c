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

static struct Template_select {
        GtkWidget *window;
        GtkWidget *ok_btn;
	GtkWidget *clist_tmpls;

	GSList *tmpl_list;
	void (*template_cb)(gchar *s, gpointer data);
	gpointer save_data;
} tmplsl;

/* widget creating functions */
static void template_select_create_window(void);
static void template_select_setup_window(void);

/* callbacks */
static gint template_select_deleted_cb(GtkWidget *widget,
                                       GdkEventAny *event,
                                       gpointer data);
static void template_select_key_pressed_cb(GtkWidget *widget,
					   GdkEventKey *event,
					   gpointer data);
static void template_select_cancel_cb(void);
static void template_select_ok_cb(void);

/* Called from elsewhere */
void template_select (void (*template_apply_cb)(gchar *s, gpointer data), 
                      gpointer data)
{
	tmplsl.template_cb = template_apply_cb;
	tmplsl.save_data = data;

	tmplsl.tmpl_list = template_read_config();

	inc_autocheck_timer_remove();

	if(!tmplsl.window){
		template_select_create_window();
	}
	template_select_setup_window();
	gtk_widget_show(tmplsl.window);
}

void template_select_create_window(void)
{
        /* window structure ;) */
        GtkWidget *window;
        GtkWidget   *vbox1;
        GtkWidget     *scroll1;
        GtkWidget       *clist_tmpls;
        GtkWidget     *confirm_area;
	GtkWidget       *ok_btn;
	GtkWidget       *cancel_btn;

	gchar *title[] = {_("Registered templates")};

        /* main window */
	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
        gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
        gtk_window_set_default_size(GTK_WINDOW(window), 200, 300);

        /* vbox to handle template name and content */
	vbox1 = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox1);
        gtk_container_add(GTK_CONTAINER(window), vbox1);

        /* templates list */
	scroll1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll1);
	gtk_widget_set_usize(scroll1, -1, 150);
	gtk_box_pack_start(GTK_BOX(vbox1), scroll1, TRUE, TRUE, 0);
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

        /* ok | cancel */
	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Templates"));

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(template_select_deleted_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(template_select_key_pressed_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(template_select_ok_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(template_select_cancel_cb), NULL);

	tmplsl.window = window;
	tmplsl.ok_btn = ok_btn;
	tmplsl.clist_tmpls = clist_tmpls;
}

void template_select_setup_window(void)
{
	GtkCList *clist = GTK_CLIST(tmplsl.clist_tmpls);
	GSList *cur;
	gint row;
	Template *tmpl;
	gchar *cond_str[1];

	manage_window_set_transient(GTK_WINDOW(tmplsl.window));
	gtk_widget_grab_focus(tmplsl.ok_btn);

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	for (cur = tmplsl.tmpl_list; cur != NULL; cur = cur->next) {
		tmpl = cur->data;
		cond_str[0] = tmpl->name;
		row = gtk_clist_append(clist, cond_str);
		gtk_clist_set_row_data(clist, row, tmpl);
	}

	gtk_clist_thaw(clist);
}

static gint template_select_deleted_cb(GtkWidget *widget, GdkEventAny *event,
                                       gpointer data)
{
	template_select_cancel_cb();
	return TRUE;
}

static void template_select_key_pressed_cb(GtkWidget *widget, GdkEventKey *event,
				           gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		template_select_cancel_cb();
}

static void template_select_ok_cb(void)
{
	GtkCList *clist = GTK_CLIST(tmplsl.clist_tmpls);
	gchar *s;
	Template *tmpl;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	tmpl = gtk_clist_get_row_data(clist, row);
	s = g_strdup(tmpl->value);

	template_clear_config(tmplsl.tmpl_list);
	gtk_widget_hide(tmplsl.window);
	inc_autocheck_timer_set();
	tmplsl.template_cb(s, tmplsl.save_data);
}

static void template_select_cancel_cb(void)
{
	template_clear_config(tmplsl.tmpl_list);
	gtk_widget_hide(tmplsl.window);
	inc_autocheck_timer_set();
	tmplsl.template_cb(NULL, tmplsl.save_data);
}
