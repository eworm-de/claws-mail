/* select-keys.c - GTK+ based key selection
 *      Copyright (C) 2001 Werner Koch (dd9jn)
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
#  include <config.h>
#endif

#ifdef USE_GPGME
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkclist.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtksignal.h>

#include "intl.h"
#include "select-keys.h"
#include "utils.h"
#include "gtkutils.h"

enum col_titles { 
    COL_ALGO,
    COL_KEYID,
    COL_NAME,
    COL_EMAIL,
    COL_VALIDITY,

    N_COL_TITLES
};

static struct {
    int okay;
    GtkWidget *window;
    GtkLabel *toplabel;
    GtkCList *clist;
    GtkProgress *progress;
    const char *pattern;
    GpgmeRecipients rset;
} select_keys;


static void set_row (GtkCList *clist, GpgmeKey key );
static void fill_clist (GtkCList *clist, const char *pattern );
static void create_dialog (void);
static void open_dialog (void);
static void close_dialog (void);
static void key_pressed_cb (GtkWidget *widget,
                            GdkEventKey *event, gpointer data);
static void select_btn_cb (GtkWidget *widget, gpointer data);
static void cancel_btn_cb (GtkWidget *widget, gpointer data);


static void
update_progress (void)
{
    if (select_keys.progress) {
        gfloat val = gtk_progress_get_value (select_keys.progress);

        val += 1;
        gtk_progress_set_value (select_keys.progress, val);
        if ( !GTK_WIDGET_VISIBLE (select_keys.progress) )
            gtk_widget_show (GTK_WIDGET (select_keys.progress));
    }
}


/**
 * select_keys_get_recipients:
 * @recp_names: A list of email addresses
 * 
 * Select a list of recipients from a given list of email addresses.
 * This may pop up a window to present the user a choice, it will also
 * check that the recipients key are all valid.
 * 
 * Return value: NULL on error or a list of list of recipients.
 **/
GpgmeRecipients
gpgmegtk_recipient_selection (GSList *recp_names)
{
    GpgmeError err;

    err = gpgme_recipients_new (&select_keys.rset);
    if (err) {
        g_message ("** failed to allocate recipients set: %s",
                   gpgme_strerror (err) );
        return NULL;
    }
        
    open_dialog ();

    do {
        select_keys.pattern = recp_names? recp_names->data:NULL;
        gtk_label_set_text (select_keys.toplabel, select_keys.pattern );
        gtk_clist_clear (select_keys.clist);
        fill_clist (select_keys.clist, select_keys.pattern);
        gtk_main ();
        if (recp_names)
            recp_names = recp_names->next;
    } while (select_keys.okay && recp_names );

    close_dialog ();

    {
        GpgmeRecipients rset = select_keys.rset;
        select_keys.rset = NULL;
        if (!rset) {
            gpgme_recipients_release (rset);
            rset = NULL;
        }
        return rset;
    }
} 

static void
destroy_key (gpointer data)
{
    GpgmeKey key = data;
    gpgme_key_release (key);
}

static void
set_row (GtkCList *clist, GpgmeKey key )
{
    const char *s;
    const char *text[N_COL_TITLES];
    char *algo_buf;
    int row;
    
    algo_buf = g_strdup_printf ("%lu/%s", 
         gpgme_key_get_ulong_attr (key, GPGME_ATTR_LEN, NULL, 0 ),
         gpgme_key_get_string_attr (key, GPGME_ATTR_ALGO, NULL, 0 ) );
    text[COL_ALGO] = algo_buf;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_KEYID, NULL, 0 );
    if ( strlen (s) == 16 )
        s += 8; /* show only the short keyID */
    text[COL_KEYID] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_NAME, NULL, 0 );
    text[COL_NAME] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_EMAIL, NULL, 0 );
    text[COL_EMAIL] = s;

    s = gpgme_key_get_string_attr (key, GPGME_ATTR_VALIDITY, NULL, 0 );
    text[COL_VALIDITY] = s;

    row = gtk_clist_append (clist, (gchar**)text);
    g_free (algo_buf);

    gtk_clist_set_row_data_full (clist, row, key, destroy_key);
}


static void 
fill_clist (GtkCList *clist, const char *pattern )
{
    GpgmeCtx ctx;
    GpgmeError err;
    GpgmeKey key;

    debug_print ("select_keys:fill_clist:  pattern `%s'\n", pattern );

    /*gtk_clist_freeze (select_keys.clist);*/
    err = gpgme_new (&ctx);
    g_assert (!err);

    while (gtk_events_pending ())
        gtk_main_iteration ();

    err = gpgme_op_keylist_start (ctx, pattern, 0 );
    if (err) {
        g_message ("** gpgme_op_keylist_start(%s) failed: %s",
                   pattern, gpgme_strerror (err));
        return;
    }
    update_progress ();
    while ( !(err = gpgme_op_keylist_next ( ctx, &key )) ) {
        debug_print ("%% %s:%d:  insert\n", __FILE__ ,__LINE__ );
        set_row (clist, key ); key = NULL;
        update_progress ();
        while (gtk_events_pending ())
            gtk_main_iteration ();
    }
    debug_print ("%% %s:%d:  ready\n", __FILE__ ,__LINE__ );
    gtk_widget_hide (GTK_WIDGET (select_keys.progress));
    if ( err != GPGME_EOF )
        g_message ("** gpgme_op_keylist_next failed: %s",
                   gpgme_strerror (err));
    gpgme_release (ctx);
    /*gtk_clist_thaw (select_keys.clist);*/
}




static void 
create_dialog ()
{
    GtkWidget *window;
    GtkWidget *vbox, *vbox2, *hbox;
    GtkWidget *bbox;
    GtkWidget *scrolledwin;
    GtkWidget *clist;
    GtkWidget *label;
    GtkWidget *progress;
    GtkWidget *select_btn, *cancel_btn;
    gchar *titles[N_COL_TITLES];

    g_assert (!select_keys.window);
    window = gtk_window_new (GTK_WINDOW_DIALOG);
    gtk_widget_set_usize (window, 500, 320);
    gtk_container_set_border_width (GTK_CONTAINER (window), 8);
    gtk_window_set_title (GTK_WINDOW (window), _("Select Keys"));
    gtk_window_set_modal (GTK_WINDOW (window), TRUE);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                        GTK_SIGNAL_FUNC (close_dialog), NULL);
    gtk_signal_connect (GTK_OBJECT (window), "key_press_event",
                        GTK_SIGNAL_FUNC (key_pressed_cb), NULL);

    vbox = gtk_vbox_new (FALSE, 8);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox  = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new ( _("Select key for: ") );
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    label = gtk_label_new ( "" );
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);


    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);


    scrolledwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    titles[COL_ALGO]     = _("Size");
    titles[COL_KEYID]    = _("Key ID");
    titles[COL_NAME]     = _("Name");
    titles[COL_EMAIL]    = _("Address");
    titles[COL_VALIDITY] = _("Val");

    clist = gtk_clist_new_with_titles (N_COL_TITLES, titles);
    gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_ALGO,      40);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_KEYID,     60);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_NAME,     100);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_EMAIL,    100);
    gtk_clist_set_column_width (GTK_CLIST(clist), COL_VALIDITY,  20);
    gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);

    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    gtkut_button_set_create (&bbox, &select_btn, _("Select"),
                             &cancel_btn, _("Cancel"), NULL, NULL);
    gtk_box_pack_end (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
    gtk_widget_grab_default (select_btn);

    gtk_signal_connect (GTK_OBJECT (select_btn), "clicked",
                        GTK_SIGNAL_FUNC (select_btn_cb), clist);
    gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
                        GTK_SIGNAL_FUNC (cancel_btn_cb), NULL);

    vbox2 = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

    progress = gtk_progress_bar_new ();
    gtk_box_pack_start (GTK_BOX (vbox2), progress, FALSE, FALSE, 4);
    gtk_progress_set_activity_mode (GTK_PROGRESS (progress), 1);

    gtk_widget_show_all (window);
    select_keys.window = window;
    select_keys.toplabel = GTK_LABEL (label);
    select_keys.clist  = GTK_CLIST (clist);
    select_keys.progress = GTK_PROGRESS (progress);
}


static void
open_dialog ()
{
    if ( !select_keys.window )
        create_dialog ();
    select_keys.okay = 0;
    gtk_widget_show (select_keys.window);
}


static void
close_dialog ()
{
    gtk_widget_destroy (select_keys.window);
    select_keys.window = NULL;
}


static void 
key_pressed_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event && event->keyval == GDK_Escape) {
        select_keys.okay = 0;
        gtk_main_quit ();
    }
}


static void 
select_btn_cb (GtkWidget *widget, gpointer data)
{
    GtkCList *clist = GTK_CLIST (data);
    int row;
    GpgmeKey key;

    if (!clist->selection) {
        g_message ("** nothing selected");
        return;
    }
    row = GPOINTER_TO_INT(clist->selection->data);
    key = gtk_clist_get_row_data(clist, row);
    if (key) {
        const char *s = gpgme_key_get_string_attr (key,
                                                   GPGME_ATTR_FPR,
                                                   NULL, 0 );
        g_message ("** FIXME: we are faking the trust calculation");
        if (!gpgme_recipients_add_name_with_validity (select_keys.rset, s,
                                                      GPGME_VALIDITY_FULL) ) {
            select_keys.okay = 1;
            gtk_main_quit ();
        }
    }
}


static void 
cancel_btn_cb (GtkWidget *widget, gpointer data)
{
    select_keys.okay = 0;
    gtk_main_quit ();
}

#endif /*USE_GPGME*/
