/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs_gtk.h"
#include "prefs_filtering_action.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "matcher.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "description_window.h"
#include "addr_compl.h"

#include "matcher_parser.h"
#include "colorlabel.h"


static void prefs_filtering_action_create(void);
static void prefs_filtering_action_delete_cb(void);
static void prefs_filtering_action_substitute_cb(void);
static void prefs_filtering_action_register_cb(void);
static void prefs_filtering_action_reset_dialog(void);
static gboolean prefs_filtering_action_key_pressed(GtkWidget *widget,
    GdkEventKey *event, gpointer data);
static void prefs_filtering_action_cancel(void);
static void prefs_filtering_action_ok(void);
static gint prefs_filtering_action_deleted(GtkWidget *widget,
    GdkEventAny *event, gpointer data);
static void prefs_filtering_action_type_selection_changed(GtkList *list,
    gpointer user_data);
static void prefs_filtering_action_type_select(GtkList *list,
    GtkWidget *widget, gpointer user_data);
static void prefs_filtering_action_select_dest(void);
static void prefs_filtering_action_select(GtkCList *clist,
    gint row, gint column, GdkEvent *event);
static void prefs_filtering_action_up(void);
static void prefs_filtering_action_down(void);
static void prefs_filtering_action_set_dialog(GSList *action_list);
static GSList *prefs_filtering_action_get_list(void);


/*!
 *\brief	UI data for matcher dialog
 */
static struct FilteringAction {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *action_clist;
	GtkWidget *action_type_list;
	GtkWidget *action_combo;
	GtkWidget *account_label;
	GtkWidget *account_list;
	GtkWidget *account_combo;
	GtkWidget *dest_entry;
	GtkWidget *dest_btn;
	GtkWidget *dest_label;
	GtkWidget *recip_label;
	GtkWidget *exec_label;
	GtkWidget *exec_btn;
	GtkWidget *color_label;
	GtkWidget *color_optmenu;
	GtkWidget *score_label;

	gint current_action;
} filtering_action;


typedef enum Action_ {
	ACTION_MOVE,
	ACTION_COPY,
	ACTION_DELETE,
	ACTION_MARK,
	ACTION_UNMARK,
	ACTION_LOCK,
	ACTION_UNLOCK,
	ACTION_MARK_AS_READ,
	ACTION_MARK_AS_UNREAD,
	ACTION_FORWARD,
	ACTION_FORWARD_AS_ATTACHMENT,
	ACTION_REDIRECT,
	ACTION_EXECUTE,
	ACTION_COLOR,
	ACTION_CHANGE_SCORE,
	ACTION_SET_SCORE,
	ACTION_HIDE,
	ACTION_STOP,
	/* add other action constants */
} Action;

static struct {
	gchar *text;
	Action action;
} action_text [] = {
	{ N_("Move"),			ACTION_MOVE	}, 	
	{ N_("Copy"),			ACTION_COPY	},
	{ N_("Delete"),			ACTION_DELETE	},
	{ N_("Mark"),			ACTION_MARK	},
	{ N_("Unmark"),			ACTION_UNMARK   },
	{ N_("Lock"),			ACTION_LOCK	},
	{ N_("Unlock"),			ACTION_UNLOCK	},
	{ N_("Mark as read"),		ACTION_MARK_AS_READ },
	{ N_("Mark as unread"),		ACTION_MARK_AS_UNREAD },
	{ N_("Forward"),		ACTION_FORWARD  },
	{ N_("Forward as attachment"),	ACTION_FORWARD_AS_ATTACHMENT },
	{ N_("Redirect"),		ACTION_REDIRECT },
	{ N_("Execute"),		ACTION_EXECUTE	},
	{ N_("Color"),			ACTION_COLOR	},
	{ N_("Change score"),		ACTION_CHANGE_SCORE},
	{ N_("Set score"),		ACTION_SET_SCORE},
	{ N_("Hide"),		        ACTION_HIDE	},
	{ N_("Stop filter"),		ACTION_STOP	},
};


/*!
 *\brief	Hooks
 */
static PrefsFilteringActionSignal *filtering_action_callback;

/*!
 *\brief	Find index of list selection 
 *
 *\param	list GTK list widget
 *
 *\return	gint Selection index
 */
static gint get_sel_from_list(GtkList *list)
{
	gint row = 0;
	void * sel;
	GList * child;

	if (list->selection == NULL) 
		return -1;

	sel = list->selection->data;
	for (child = list->children; child != NULL; child = g_list_next(child)) {
		if (child->data == sel)
			return row;
		row ++;
	}
	
	return row;
}

/*!
 *\brief	Opens the filtering action dialog with a list of actions
 *
 *\param	matchers List of conditions
 *\param	cb Callback
 *
 */
void prefs_filtering_action_open(GSList *action_list,
    PrefsFilteringActionSignal *cb)
{
	inc_lock();

	if (!filtering_action.window) {
		prefs_filtering_action_create();
	}

	manage_window_set_transient(GTK_WINDOW(filtering_action.window));
	gtk_widget_grab_focus(filtering_action.ok_btn);

	filtering_action_callback = cb;

	prefs_filtering_action_set_dialog(action_list);

	gtk_widget_show(filtering_action.window);
}

/*!
 *\brief	Create the matcher dialog
 */
static void prefs_filtering_action_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;

	GtkWidget *hbox1;

	GtkWidget *action_label;
	GtkWidget *recip_label;
	GtkWidget *action_combo;
	GtkWidget *action_type_list;
	GtkWidget *account_list;
	GtkWidget *dest_label;
	GtkWidget *exec_label;
	GtkWidget *score_label;
	GtkWidget *color_label;
	GtkWidget *account_label;
	GtkWidget *account_combo;
	GtkWidget *dest_entry;
	GtkWidget *dest_btn;
        GList * cur;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *action_hbox;
	GtkWidget *action_scrolledwin;
	GtkWidget *action_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *exec_btn;

	GtkWidget *color_optmenu;

	GList *combo_items;
	gint i;

	gchar *title[1];
        GList * accounts;

	debug_print("Creating matcher configuration window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window),
			     _("Filtering action configuration"));
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_deleted), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_ok), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_cancel), NULL);

	vbox1 = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (vbox1), 2);

        /* action to be defined */

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	action_label = gtk_label_new (_("Action"));
	gtk_widget_show (action_label);
	gtk_misc_set_alignment (GTK_MISC (action_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), action_label, FALSE, FALSE, 0);

	action_combo = gtk_combo_new ();
	gtk_widget_show (action_combo);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(action_combo)->entry),
			       FALSE);

	combo_items = NULL;
	for (i = 0; i < sizeof action_text / sizeof action_text[0]; i++)
		combo_items = g_list_append
			(combo_items, (gpointer) _(action_text[i].text));
	gtk_combo_set_popdown_strings(GTK_COMBO(action_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox1), action_combo,
			    TRUE, TRUE, 0);
	action_type_list = GTK_COMBO(action_combo)->list;
	gtk_signal_connect (GTK_OBJECT (action_type_list), "select-child",
			    GTK_SIGNAL_FUNC (prefs_filtering_action_type_select),
			    NULL);

	gtk_signal_connect(GTK_OBJECT(action_type_list), "selection-changed",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_type_selection_changed),
			   NULL);

	/* accounts */

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	account_label = gtk_label_new (_("Account"));
	gtk_widget_show (account_label);
	gtk_misc_set_alignment (GTK_MISC (account_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), account_label, FALSE, FALSE, 0);

	account_combo = gtk_combo_new ();
	gtk_widget_set_usize (account_combo, 150, -1);
	gtk_widget_show (account_combo);

	combo_items = NULL;
	for (accounts = account_get_list() ; accounts != NULL;
	     accounts = accounts->next) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;
		gchar *name;

		name = g_strdup_printf("%s <%s> (%s)",
				       ac->name, ac->address,
				       ac->account_name);
		combo_items = g_list_append(combo_items, (gpointer) name);
	}

	gtk_combo_set_popdown_strings(GTK_COMBO(account_combo), combo_items);

	for(cur = g_list_first(combo_items) ; cur != NULL ;
	    cur = g_list_next(cur))
		g_free(cur->data);
	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox1), account_combo,
			    TRUE, TRUE, 0);
	account_list = GTK_COMBO(account_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(account_combo)->entry),
			       FALSE);

	/* destination */

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	dest_label = gtk_label_new (_("Destination"));
	gtk_widget_show (dest_label);
	gtk_misc_set_alignment (GTK_MISC (dest_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), dest_label, FALSE, FALSE, 0);

	recip_label = gtk_label_new (_("Recipient"));
	gtk_widget_show (recip_label);
	gtk_misc_set_alignment (GTK_MISC (recip_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), recip_label, FALSE, FALSE, 0);

	exec_label = gtk_label_new (_("Execute"));
	gtk_widget_show (exec_label);
	gtk_misc_set_alignment (GTK_MISC (exec_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), exec_label, FALSE, FALSE, 0);
	
	color_label = gtk_label_new (_("Color"));
	gtk_widget_show(color_label);
	gtk_misc_set_alignment(GTK_MISC(color_label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox1), color_label, FALSE, FALSE, 0);

	score_label = gtk_label_new (_("Score"));
	gtk_widget_show (score_label);
	gtk_misc_set_alignment (GTK_MISC (score_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), score_label, FALSE, FALSE, 0);

	dest_entry = gtk_entry_new ();
	gtk_widget_set_usize (dest_entry, 150, -1);
	gtk_widget_show (dest_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), dest_entry, TRUE, TRUE, 0);
	
	color_optmenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(color_optmenu),
				 colorlabel_create_color_menu());
	gtk_box_pack_start(GTK_BOX(hbox1), color_optmenu, TRUE, TRUE, 0);

	dest_btn = gtk_button_new_with_label (_("Select ..."));
	gtk_widget_show (dest_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), dest_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (dest_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_action_select_dest),
			    NULL);

	exec_btn = gtk_button_new_with_label (_("Info ..."));
	gtk_widget_show (exec_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), exec_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (exec_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filtering_action_exec_info),
			    NULL);

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(reg_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label(_("Add"));
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(reg_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_("  Replace  "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(subst_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_substitute_cb),
			   NULL);

	del_btn = gtk_button_new_with_label(_("Delete"));
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(del_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_delete_cb), NULL);

	action_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(action_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), action_hbox, TRUE, TRUE, 0);

	action_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(action_scrolledwin);
	gtk_widget_set_usize(action_scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(action_hbox), action_scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(action_scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	title[0] = _("Current action list");
	action_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show(action_clist);
	gtk_container_add(GTK_CONTAINER(action_scrolledwin), action_clist);
	gtk_clist_set_column_width(GTK_CLIST(action_clist), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST(action_clist),
				     GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(action_clist)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(action_clist), "select_row",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_select), NULL);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(action_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label(_("Up"));
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(up_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_up), NULL);

	down_btn = gtk_button_new_with_label(_("Down"));
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(down_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_filtering_action_down), NULL);

	gtk_widget_show_all(window);

	filtering_action.window    = window;
	filtering_action.action_type_list = action_type_list;
	filtering_action.action_combo = action_combo;
	filtering_action.account_label = account_label;
	filtering_action.account_list = account_list;
	filtering_action.account_combo = account_combo;
	filtering_action.dest_entry = dest_entry;
	filtering_action.dest_btn = dest_btn;
	filtering_action.dest_label = dest_label;
	filtering_action.recip_label = recip_label;
	filtering_action.exec_label = exec_label;
	filtering_action.exec_btn = exec_btn;
	filtering_action.color_label   = color_label;
	filtering_action.color_optmenu = color_optmenu;
	filtering_action.score_label = score_label;
	filtering_action.ok_btn = ok_btn;
	filtering_action.action_clist = action_clist;
}

/*!
 *\brief	Set the contents of a row
 *
 *\param	row Index of row to set
 *\param	prop Condition to set
 *
 *\return	gint Row index \a prop has been added
 */
static gint prefs_filtering_action_clist_set_row(gint row, FilteringAction *action)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	gchar *action_tab_str[1];
	gchar *action_str;
        gchar buf[256];

	if (action == NULL) {
		action_tab_str[0] = _("(New)");
		return gtk_clist_append(clist, action_tab_str);
	}

        filteringaction_to_string(buf, sizeof buf, action);
	action_str = g_strdup(buf);
        
	action_tab_str[0] = action_str;
	if (row < 0)
		row = gtk_clist_append(clist, action_tab_str);
	else
		gtk_clist_set_text(clist, row, 0, action_tab_str[0]);
	g_free(action_str);

	return row;
}

/*!
 *\brief	Update scrollbar
 */
static void prefs_filtering_action_update_hscrollbar(void)
{
	gint optwidth = gtk_clist_optimal_column_width(GTK_CLIST(filtering_action.action_clist), 0);
	gtk_clist_set_column_width(GTK_CLIST(filtering_action.action_clist), 0, optwidth);
}

/*!
 *\brief	Initializes dialog with a set of conditions
 *
 *\param	matchers List of conditions
 */
static void prefs_filtering_action_set_dialog(GSList *action_list)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	GSList *cur;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	prefs_filtering_action_clist_set_row(-1, NULL);
	if (action_list != NULL) {
		for (cur = action_list; cur != NULL;
		     cur = g_slist_next(cur)) {
			FilteringAction *action;
			action = (FilteringAction *) cur->data;
			prefs_filtering_action_clist_set_row(-1, action);
		}
	}
	
	prefs_filtering_action_update_hscrollbar();

	gtk_clist_thaw(clist);

        prefs_filtering_action_reset_dialog();
}

/*!
 *\brief	Converts current actions in list box in
 *		an action list used by the filtering system.
 *
 *\return	GSList * List of actions.
 */
static GSList *prefs_filtering_action_get_list(void)
{
	gchar *action_str;
	gint row = 1;
	GSList *action_list;

	action_list = NULL;

	while (gtk_clist_get_text(GTK_CLIST(filtering_action.action_clist),
				  row, 0, &action_str)) {

		if (strcmp(action_str, _("(New)")) != 0) {
                        GSList * tmp_action_list;
			tmp_action_list = matcher_parser_get_action_list(action_str);
			
			if (tmp_action_list == NULL)
				break;

			action_list = g_slist_concat(action_list,
                            tmp_action_list);
		}
		row ++;
	}

	return action_list;
}

/*!
 *\brief	Returns account ID from the given list index
 *
 *\return	gint account ID
 */
static gint get_account_id_from_list_id(gint list_id)
{
	GList * accounts;

	for (accounts = account_get_list() ; accounts != NULL;
	     accounts = accounts->next) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;

		if (list_id == 0)
			return ac->account_id;
		list_id--;
	}
	return 0;
}

/*!
 *\brief	Returns list index from the given account ID
 *
 *\return	gint list index
 */
static gint get_list_id_from_account_id(gint account_id)
{
	GList * accounts;
	gint list_id = 0;

	for (accounts = account_get_list() ; accounts != NULL;
	     accounts = accounts->next) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;

		if (account_id == ac->account_id)
			return list_id;
		list_id++;
	}
	return 0;
}


/*!
 *\brief	Returns parser action ID from internal action ID
 *
 *\return	gint parser action ID
 */
static gint prefs_filtering_action_get_matching_from_action(Action action_id)
{
	switch (action_id) {
	case ACTION_MOVE:
		return MATCHACTION_MOVE;
	case ACTION_COPY:
		return MATCHACTION_COPY;
	case ACTION_DELETE:
		return MATCHACTION_DELETE;
	case ACTION_MARK:
		return MATCHACTION_MARK;
	case ACTION_UNMARK:
		return MATCHACTION_UNMARK;
	case ACTION_LOCK:
		return MATCHACTION_LOCK;
	case ACTION_UNLOCK:
		return MATCHACTION_UNLOCK;
	case ACTION_MARK_AS_READ:
		return MATCHACTION_MARK_AS_READ;
	case ACTION_MARK_AS_UNREAD:
		return MATCHACTION_MARK_AS_UNREAD;
	case ACTION_FORWARD:
		return MATCHACTION_FORWARD;
	case ACTION_FORWARD_AS_ATTACHMENT:
		return MATCHACTION_FORWARD_AS_ATTACHMENT;
	case ACTION_REDIRECT:
		return MATCHACTION_REDIRECT;
	case ACTION_EXECUTE:
		return MATCHACTION_EXECUTE;
	case ACTION_COLOR:
		return MATCHACTION_COLOR;
	case ACTION_HIDE:
		return MATCHACTION_HIDE;
	case ACTION_STOP:
		return MATCHACTION_STOP;
	case ACTION_CHANGE_SCORE:
		return MATCHACTION_CHANGE_SCORE;
	case ACTION_SET_SCORE:
		return MATCHACTION_SET_SCORE;
	default:
		return -1;
	}
}

/*!
 *\brief	Returns action from the content of the dialog
 *
 *\param	alert specifies whether alert dialog boxes should be shown
 *                or not.
 *
 *\return	FilteringAction * action entered in the dialog box.
 */
static FilteringAction * prefs_filtering_action_dialog_to_action(gboolean alert)
{
	Action action_id;
	gint action_type;
	gint list_id;
	gint account_id;
	const gchar * destination;
	gint labelcolor = 0;
        FilteringAction * action;
        gchar * score_str;
        gint score;
        
	action_id = get_sel_from_list(GTK_LIST(filtering_action.action_type_list));
	action_type = prefs_filtering_action_get_matching_from_action(action_id);
	list_id = get_sel_from_list(GTK_LIST(filtering_action.account_list));
	account_id = get_account_id_from_list_id(list_id);
        score = 0;
        destination = NULL;
        
	switch (action_id) {
	case ACTION_MOVE:
	case ACTION_COPY:
	case ACTION_EXECUTE:
		destination = gtk_entry_get_text(GTK_ENTRY(filtering_action.dest_entry));
		if (*destination == '\0') {
			if (alert)
                                alertpanel_error(action_id == ACTION_EXECUTE 
						 ? _("Command line not set")
						 : _("Destination is not set."));
			return NULL;
		}
		break;
	case ACTION_FORWARD:
	case ACTION_FORWARD_AS_ATTACHMENT:
	case ACTION_REDIRECT:
		destination = gtk_entry_get_text(GTK_ENTRY(filtering_action.dest_entry));
		if (*destination == '\0') {
			if (alert)
                                alertpanel_error(_("Recipient is not set."));
			return NULL;
		}
		break;
	case ACTION_COLOR:
		labelcolor = colorlabel_get_color_menu_active_item(
			gtk_option_menu_get_menu(GTK_OPTION_MENU(filtering_action.color_optmenu)));
		destination = NULL;	
		break;
        case ACTION_CHANGE_SCORE:
        case ACTION_SET_SCORE:
		score_str = gtk_entry_get_text(GTK_ENTRY(filtering_action.dest_entry));
		if (*score_str == '\0') {
			if (alert)
                                alertpanel_error(_("Score is not set"));
			return NULL;
		}
                score = strtol(score_str, NULL, 10);
                break;
	case ACTION_STOP:
	case ACTION_HIDE:
        case ACTION_DELETE:
        case ACTION_MARK:
        case ACTION_UNMARK:
        case ACTION_LOCK:
        case ACTION_UNLOCK:
        case ACTION_MARK_AS_READ:
        case ACTION_MARK_AS_UNREAD:
	default:
		break;
	}
	
	action = filteringaction_new(action_type, account_id,
            destination, labelcolor, score);

        return action;
}

/*!
 *\brief	Signal handler for register button
 */
static void prefs_filtering_action_register_cb(void)
{
	FilteringAction *action;
	
	action = prefs_filtering_action_dialog_to_action(TRUE);
	if (action == NULL)
		return;

	prefs_filtering_action_clist_set_row(-1, action);

	filteringaction_free(action);
	/* presumably gtk_list_select_item(), called by 
	 * prefs_filtering_action_reset_dialog() activates 
	 * what seems to be a bug. this causes any other 
	 * list items to be unselectable */
	/* prefs_filtering_action_reset_dialog(); */
	gtk_list_select_item(GTK_LIST(filtering_action.account_list), 0);
	gtk_entry_set_text(GTK_ENTRY(filtering_action.dest_entry), "");
	prefs_filtering_action_update_hscrollbar();
}

/*!
 *\brief	Signal handler for substitute button
 */
static void prefs_filtering_action_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	gint row;
	FilteringAction *action;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;
	
	action = prefs_filtering_action_dialog_to_action(TRUE);
	if (action == NULL)
		return;

	prefs_filtering_action_clist_set_row(row, action);

	filteringaction_free(action);

	prefs_filtering_action_reset_dialog();
	
	prefs_filtering_action_update_hscrollbar();
}

/*!
 *\brief	Signal handler for delete button
 */
static void prefs_filtering_action_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;

	gtk_clist_remove(clist, row);

	prefs_filtering_action_reset_dialog();

	prefs_filtering_action_update_hscrollbar();
}

/*!
 *\brief	Signal handler for 'move up' button
 */
static void prefs_filtering_action_up(void)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1) {
		gtk_clist_row_move(clist, row, row - 1);
		if (gtk_clist_row_is_visible(clist, row - 1) != GTK_VISIBILITY_FULL)
			gtk_clist_moveto(clist, row - 1, 0, 0, 0);
	}
}

/*!
 *\brief	Signal handler for 'move down' button
 */
static void prefs_filtering_action_down(void)
{
	GtkCList *clist = GTK_CLIST(filtering_action.action_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row >= 1 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		if (gtk_clist_row_is_visible(clist, row + 1) != GTK_VISIBILITY_FULL)
			gtk_clist_moveto(clist, row + 1, 0, 1, 0);
	}
}

/*!
 *\brief	Signal handler for select row.
 *
 *\param	clist List widget
 *\param	row Selected row
 *\param	column Selected column
 *\param	event Event information
 */
static void prefs_filtering_action_select(GtkCList *clist,
    gint row, gint column, GdkEvent *event)
{
	gchar *action_str;
	FilteringAction *action;
        GSList * action_list;
	gint list_id;

	if (!gtk_clist_get_text(GTK_CLIST(filtering_action.action_clist),
				row, 0, &action_str))
		return;

	if (row == 0) {
		prefs_filtering_action_reset_dialog();
		return;
	}

	action_list = matcher_parser_get_action_list(action_str);
	if (action_list == NULL)
		return;

        action = action_list->data;
        g_slist_free(action_list);

	if (action->destination)
		gtk_entry_set_text(GTK_ENTRY(filtering_action.dest_entry), action->destination);
	else
		gtk_entry_set_text(GTK_ENTRY(filtering_action.dest_entry), "");

	switch(action->type) {
	case MATCHACTION_MOVE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_MOVE);
		break;
	case MATCHACTION_COPY:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_COPY);
		break;
	case MATCHACTION_DELETE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_DELETE);
		break;
	case MATCHACTION_MARK:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_MARK);
		break;
	case MATCHACTION_UNMARK:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_UNMARK);
		break;
	case MATCHACTION_LOCK:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_LOCK);
		break;
	case MATCHACTION_UNLOCK:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_UNLOCK);
		break;
	case MATCHACTION_MARK_AS_READ:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_MARK_AS_READ);
		break;
	case MATCHACTION_MARK_AS_UNREAD:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_MARK_AS_UNREAD);
		break;
	case MATCHACTION_FORWARD:
		list_id = get_list_id_from_account_id(action->account_id);
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_FORWARD);
		gtk_list_select_item(GTK_LIST(filtering_action.account_list),
				     list_id);
		break;
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
		list_id = get_list_id_from_account_id(action->account_id);
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_FORWARD_AS_ATTACHMENT);
		gtk_list_select_item(GTK_LIST(filtering_action.account_list),
				     list_id);
		break;
	case MATCHACTION_REDIRECT:
		list_id = get_list_id_from_account_id(action->account_id);
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_REDIRECT);
		gtk_list_select_item(GTK_LIST(filtering_action.account_list),
				     list_id);
		break;
	case MATCHACTION_EXECUTE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_EXECUTE);
		break;
	case MATCHACTION_COLOR:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_COLOR);
		gtk_option_menu_set_history(GTK_OPTION_MENU(filtering_action.color_optmenu), action->labelcolor);     
		break;
	case MATCHACTION_CHANGE_SCORE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_CHANGE_SCORE);
		break;
	case MATCHACTION_SET_SCORE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_SET_SCORE);
		break;
	case MATCHACTION_STOP:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_STOP);
		break;
	case MATCHACTION_HIDE:
		gtk_list_select_item(GTK_LIST(filtering_action.action_type_list),
				     ACTION_HIDE);
		break;
	}
}

/*!
 *\brief	Handle key press
 *
 *\param	widget Widget receiving key press
 *\param	event Key event
 *\param	data User data
 */
static gboolean prefs_filtering_action_key_pressed(GtkWidget *widget,
    GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_filtering_action_cancel();
	return TRUE;		
}

/*!
 *\brief	Cancel matcher dialog
 */
static void prefs_filtering_action_cancel(void)
{
	gtk_widget_hide(filtering_action.window);
	inc_unlock();
}

/*!
 *\brief	Accept current matchers
 */
static void prefs_filtering_action_ok(void)
{
        GSList * action_list;
        GSList * cur;

	action_list = prefs_filtering_action_get_list();

        if (action_list == NULL) {
                alertpanel_error(_("No action was defined."));
                return;
        }

        if (filtering_action_callback != NULL)
                filtering_action_callback(action_list);
        for(cur = action_list ; cur != NULL ; cur = cur->next) {
                filteringaction_free(cur->data);
        }
        g_slist_free(action_list);

	gtk_widget_hide(filtering_action.window);
        inc_unlock();
}

/*!
 *\brief	Called when closing dialog box
 *
 *\param	widget Dialog widget
 *\param	event Event info
 *\param	data User data
 *
 *\return	gint TRUE
 */
static gint prefs_filtering_action_deleted(GtkWidget *widget,
    GdkEventAny *event, gpointer data)
{
	prefs_filtering_action_cancel();
	return TRUE;
}

/*
 * Strings describing exec format strings
 * 
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *exec_desc_strings[] = {
	"%%",	"%",
	"%s",	N_("Subject"),
	"%f",	N_("From"),
	"%t",	N_("To"),
	"%c",	N_("Cc"),
	"%d",	N_("Date"),
	"%i",	N_("Message-ID"),
	"%n",	N_("Newsgroups"),
	"%r",	N_("References"),
	"%F",	N_("Filename - should not be modified"),
	"\\n",	N_("new line"),
	"\\",	N_("escape character for quotes"),
	"\\\"",N_("quote character"),
	NULL, NULL
};

static DescriptionWindow exec_desc_win = { 
        NULL, 
        2,
        N_("Description of symbols"),
        exec_desc_strings
};



/*!
 *\brief	Show Execute action's info
 */
void prefs_filtering_action_exec_info(void)
{
	description_window_create(&exec_desc_win);
}

static void prefs_filtering_action_select_dest(void)
{
	FolderItem *dest;
	gchar * path;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (!dest) return;

	path = folder_item_get_identifier(dest);

	gtk_entry_set_text(GTK_ENTRY(filtering_action.dest_entry), path);
	g_free(path);
}

static void prefs_filtering_action_type_selection_changed(GtkList *list,
    gpointer user_data)
{
	gint value;

	value = get_sel_from_list(GTK_LIST(filtering_action.action_type_list));

	if (filtering_action.current_action != value) {
		if (filtering_action.current_action == ACTION_FORWARD 
		||  filtering_action.current_action == ACTION_FORWARD_AS_ATTACHMENT
		||  filtering_action.current_action == ACTION_REDIRECT) {
			debug_print("unregistering address completion entry\n");
			address_completion_unregister_entry(GTK_ENTRY(filtering_action.dest_entry));
		}
		if (value == ACTION_FORWARD || value == ACTION_FORWARD_AS_ATTACHMENT
		||  value == ACTION_REDIRECT) {
			debug_print("registering address completion entry\n");
			address_completion_register_entry(GTK_ENTRY(filtering_action.dest_entry));
		}
		filtering_action.current_action = value;
	}
}

static void prefs_filtering_action_type_select(GtkList *list,
    GtkWidget *widget, gpointer user_data)
{
	Action value;

	value = (Action) get_sel_from_list(GTK_LIST(filtering_action.action_type_list));

	switch (value) {
	case ACTION_MOVE:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, TRUE);
		gtk_widget_show(filtering_action.dest_label);
		gtk_widget_set_sensitive(filtering_action.dest_label, TRUE);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_COPY:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, TRUE);
		gtk_widget_show(filtering_action.dest_label);
		gtk_widget_set_sensitive(filtering_action.dest_label, TRUE);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_DELETE:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, FALSE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, FALSE);
		gtk_widget_show(filtering_action.dest_label);
		gtk_widget_set_sensitive(filtering_action.dest_label, FALSE);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_MARK:
	case ACTION_UNMARK:
	case ACTION_LOCK:
	case ACTION_UNLOCK:
	case ACTION_MARK_AS_READ:
	case ACTION_MARK_AS_UNREAD:
        case ACTION_STOP:
        case ACTION_HIDE:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, FALSE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, FALSE);
		gtk_widget_show(filtering_action.dest_label);
		gtk_widget_set_sensitive(filtering_action.dest_label, FALSE);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_FORWARD:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, TRUE);
		gtk_widget_set_sensitive(filtering_action.account_combo, TRUE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, FALSE);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_show(filtering_action.recip_label);
		gtk_widget_set_sensitive(filtering_action.recip_label, TRUE);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_FORWARD_AS_ATTACHMENT:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, TRUE);
		gtk_widget_set_sensitive(filtering_action.account_combo, TRUE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, FALSE);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_show(filtering_action.recip_label);
		gtk_widget_set_sensitive(filtering_action.recip_label, TRUE);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_REDIRECT:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, TRUE);
		gtk_widget_set_sensitive(filtering_action.account_combo, TRUE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_show(filtering_action.dest_btn);
		gtk_widget_set_sensitive(filtering_action.dest_btn, FALSE);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_show(filtering_action.recip_label);
		gtk_widget_set_sensitive(filtering_action.recip_label, TRUE);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_hide(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_EXECUTE:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_hide(filtering_action.dest_btn);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_show(filtering_action.exec_label);
		gtk_widget_set_sensitive(filtering_action.exec_btn, TRUE);
		gtk_widget_show(filtering_action.exec_btn);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_COLOR:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_hide(filtering_action.dest_entry);
		gtk_widget_hide(filtering_action.dest_btn);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_show(filtering_action.exec_btn);
		gtk_widget_set_sensitive(filtering_action.exec_btn, FALSE);
		gtk_widget_show(filtering_action.color_optmenu);
		gtk_widget_show(filtering_action.color_label);
		gtk_widget_hide(filtering_action.score_label);
		break;
	case ACTION_CHANGE_SCORE:
	case ACTION_SET_SCORE:
		gtk_widget_show(filtering_action.account_label);
		gtk_widget_set_sensitive(filtering_action.account_label, FALSE);
		gtk_widget_set_sensitive(filtering_action.account_combo, FALSE);
		gtk_widget_show(filtering_action.dest_entry);
		gtk_widget_set_sensitive(filtering_action.dest_entry, TRUE);
		gtk_widget_hide(filtering_action.dest_btn);
		gtk_widget_hide(filtering_action.dest_label);
		gtk_widget_hide(filtering_action.recip_label);
		gtk_widget_hide(filtering_action.exec_label);
		gtk_widget_show(filtering_action.exec_btn);
		gtk_widget_set_sensitive(filtering_action.exec_btn, FALSE);
		gtk_widget_hide(filtering_action.color_optmenu);
		gtk_widget_hide(filtering_action.color_label);
		gtk_widget_show(filtering_action.score_label);
		break;
	}
}

static void prefs_filtering_action_reset_dialog(void)
{
	gtk_list_select_item(GTK_LIST(filtering_action.action_type_list), 0);
	gtk_list_select_item(GTK_LIST(filtering_action.account_list), 0);
	gtk_entry_set_text(GTK_ENTRY(filtering_action.dest_entry), "");
}
