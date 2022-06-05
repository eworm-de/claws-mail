/* select-keys.c - GTK based key selection
 * Copyright (C) 2001-2022 Werner Koch (dd9jn) and the Claws Mail team
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
#  include <config.h>
#endif

#ifdef USE_GPGME
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "select-keys.h"
#include "utils.h"
#include "gtkutils.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "alertpanel.h"

#define DIM(v) (sizeof(v)/sizeof((v)[0]))

enum col_titles {
    COL_ALGO,
    COL_KEYID,
    COL_NAME,
    COL_ADDRESS,
    COL_TRUST,
    COL_PTR,

    N_COL_TITLES
};

#define COL_ALGO_WIDTH 70
#define COL_KEYID_WIDTH 120
#define COL_NAME_WIDTH 115
#define COL_ADDRESS_WIDTH 140
#define COL_TRUST_WIDTH 20

struct select_keys_s {
    int okay;
    GtkWidget *window;
    GtkLabel *toplabel;
    GtkWidget *view;
    const char *pattern;
    unsigned int num_keys;
    gpgme_key_t *kset;
    gpgme_ctx_t select_ctx;
    gpgme_protocol_t proto;
    GtkSortType sort_type;
    enum col_titles sort_column;
    SelectionResult result;
};


static void set_row (GtkListStore *store, gpgme_key_t key, gpgme_protocol_t proto);
static gpgme_key_t fill_view (struct select_keys_s *sk, const char *pattern,
			gpgme_protocol_t proto);
static void create_dialog (struct select_keys_s *sk);
static void open_dialog (struct select_keys_s *sk);
static void close_dialog (struct select_keys_s *sk);
static gint delete_event_cb (GtkWidget *widget,
                             GdkEventAny *event, gpointer data);
static gboolean key_pressed_cb (GtkWidget *widget,
                                GdkEventKey *event, gpointer data);
static void select_btn_cb (GtkWidget *widget, gpointer data);
static void cancel_btn_cb (GtkWidget *widget, gpointer data);
static void dont_encrypt_btn_cb (GtkWidget *widget, gpointer data);
static void other_btn_cb (GtkWidget *widget, gpointer data);

static gboolean use_untrusted (gpgme_key_t, gpgme_user_id_t uid, gpgme_protocol_t proto);

static void
update_progress (struct select_keys_s *sk, int running, const char *pattern)
{
    static int windmill[] = { '-', '\\', '|', '/' };
    char *buf;

    if (!running)
        buf = g_strdup_printf (_("No exact match for '%s'; please select the key."),
                               pattern);
    else 
        buf = g_strdup_printf (_("Collecting info for '%s' ... %c"),
                               pattern,
                               windmill[running%DIM(windmill)]);
    gtk_label_set_text (sk->toplabel, buf);
    g_free (buf);
}


/**
 * gpgmegtk_recipient_selection:
 * @recp_names: A list of email addresses
 * 
 * Select a list of recipients from a given list of email addresses.
 * This may pop up a window to present the user a choice, it will also
 * check that the recipients key are all valid.
 * 
 * Return value: NULL on error or a list of list of recipients.
 **/
gpgme_key_t *
gpgmegtk_recipient_selection (GSList *recp_names, SelectionResult *result,
				gpgme_protocol_t proto)
{
    struct select_keys_s sk;
    gpgme_key_t key = NULL;
    memset (&sk, 0, sizeof sk);

    open_dialog (&sk);

    do {
        sk.pattern = recp_names? recp_names->data:NULL;
        sk.proto = proto;
        if (sk.view != NULL) {
            GtkTreeModel *model =
                gtk_tree_view_get_model(GTK_TREE_VIEW(sk.view));
            gtk_list_store_clear(GTK_LIST_STORE(model));
        }
        key = fill_view (&sk, sk.pattern, proto);
        update_progress (&sk, 0, sk.pattern ? sk.pattern : "NULL");
	if (!key) {
    		gtk_widget_show_all (sk.window);
	        gtk_main ();
	} else {
     		gtk_widget_hide (sk.window);
	       	sk.kset = g_realloc(sk.kset,
                	sizeof(gpgme_key_t) * (sk.num_keys + 1));
        	gpgme_key_ref(key);
        	sk.kset[sk.num_keys] = key;
        	sk.num_keys++;
        	sk.okay = 1;
		sk.result = KEY_SELECTION_OK;
		gpgme_release (sk.select_ctx);
		sk.select_ctx = NULL;
		debug_print("used %s\n", key->uids->email);
	}
	key = NULL;
        if (recp_names)
            recp_names = recp_names->next;
    } while (sk.okay && recp_names);

    close_dialog (&sk);

    if (!sk.okay) {
        g_free(sk.kset);
        sk.kset = NULL;
    } else {
        sk.kset = g_realloc(sk.kset, sizeof(gpgme_key_t) * (sk.num_keys + 1));
        sk.kset[sk.num_keys] = NULL;
    }
    if (result)
	    *result = sk.result;
    return sk.kset;
} 

static void
set_row (GtkListStore *store, gpgme_key_t key, gpgme_protocol_t proto)
{
    const gchar *s;
    gchar *algo_buf, *name, *address;
    GtkTreeIter iter;
    gsize by_read = 0, by_written = 0;
    gchar *ret_str = NULL;

    /* first check whether the key is capable of encryption which is not
     * the case for revoked, expired or sign-only keys */
    if (!key->can_encrypt || key->revoked || key->expired || key->disabled)
        return;

    algo_buf = g_strdup_printf ("%du/%s", 
         key->subkeys->length,
         gpgme_pubkey_algo_name(key->subkeys->pubkey_algo) );

    s = key->uids->name;
    if (!s || !*s)
        s = key->uids->uid;
    if (proto == GPGME_PROTOCOL_CMS) {
	if (strstr(s, ",CN="))
		s = strstr(s, ",CN=")+4;
	else if (strstr(s, "CN="))
		s = strstr(s, "CN=")+3;
    } 
    
    ret_str = NULL;
    if (!g_utf8_validate(s, -1, NULL))
	    ret_str = g_locale_to_utf8 (s, strlen(s), &by_read, &by_written, NULL);
    if (ret_str && by_written) {
        s = ret_str;
    }
    name = g_strdup(s);
    if (ret_str)
        g_free(ret_str);

    if (proto == GPGME_PROTOCOL_CMS && (!key->uids->email || !*key->uids->email)) {
        gpgme_user_id_t uid = key->uids->next;
        if (uid)
            s = uid->email;
        else
            s = key->uids->email;
    } else {
        s = key->uids->email;
    }
    
    ret_str = NULL;
    if (!g_utf8_validate(s, -1, NULL))
	    ret_str = g_locale_to_utf8 (s, strlen(s), &by_read, &by_written, NULL);
    if (ret_str && by_written) {
        s = ret_str;
    }
    address = g_strdup(s);
    if (ret_str)
        g_free(ret_str);

    switch (key->uids->validity)
      {
      case GPGME_VALIDITY_UNDEFINED:
        s = _("Undefined");
        break;
      case GPGME_VALIDITY_NEVER:
        s = _("Never");
        break;
      case GPGME_VALIDITY_MARGINAL:
        s = _("Marginal");
        break;
      case GPGME_VALIDITY_FULL:
        s = _("Full");
        break;
      case GPGME_VALIDITY_ULTIMATE:
        s = _("Ultimate");
        break;
      case GPGME_VALIDITY_UNKNOWN:
      default:
        s = _("Unknown");
        break;
      }

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
        COL_ALGO, algo_buf,
        COL_KEYID, key->subkeys->keyid,
        COL_NAME, name,
        COL_ADDRESS, address,
        COL_TRUST, s,
        COL_PTR, key,
        -1);
    gpgme_key_ref(key);

    g_free(name);
    g_free(address);
    g_free(algo_buf);
}

static gpgme_key_t 
fill_view (struct select_keys_s *sk, const char *pattern, gpgme_protocol_t proto)
{
    GtkWidget *view;
    GtkTreeModel *model;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gpgme_ctx_t ctx;
    gpgme_error_t err;
    gpgme_key_t key;
    int running=0;
    int num_results = 0;
    gboolean exact_match = FALSE;
    gpgme_key_t last_key = NULL;
    gpgme_user_id_t last_uid = NULL;

    cm_return_val_if_fail (sk, NULL);

    view = sk->view;
    cm_return_val_if_fail (view, NULL);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));

    debug_print ("select_keys:fill_view:  pattern '%s' proto %d\n", pattern != NULL ? pattern : "NULL", proto);

    err = gpgme_new (&ctx);
    g_assert (!err);

    gpgme_set_protocol(ctx, proto);
    sk->select_ctx = ctx;

    update_progress (sk, ++running, pattern);
    while (gtk_events_pending ())
        gtk_main_iteration ();

    err = gpgme_op_keylist_start (ctx, pattern, 0);
    if (err) {
        debug_print ("** gpgme_op_keylist_start(%s) failed: %s\n",
                     pattern != NULL ? pattern : "NULL", gpgme_strerror (err));
        sk->select_ctx = NULL;
        gpgme_release(ctx);
        return NULL;
    }
    update_progress (sk, ++running, pattern);
    while ( !(err = gpgme_op_keylist_next ( ctx, &key )) ) {
	gpgme_user_id_t uid = key->uids;
	if (!key->can_encrypt || key->revoked || key->expired || key->disabled) {
		gpgme_key_unref(key);
		continue;
	}
        debug_print ("%% %s:%d:  insert\n", __FILE__ ,__LINE__ );
        set_row (GTK_LIST_STORE(model), key, proto );
	for (; uid; uid = uid->next) {
		gchar *raw_mail = NULL;

		if (!uid->email)
			continue;
		if (uid->revoked || uid->invalid)
			continue;
		raw_mail = g_strdup(uid->email);
		extract_address(raw_mail);
		if (pattern != NULL && !strcasecmp(pattern, raw_mail)) {
			exact_match = TRUE;
			last_uid = uid;
			g_free(raw_mail);
			break;
		}
		g_free(raw_mail);
	}

	/* Select the first row */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	if (gtk_tree_model_get_iter_first(model, &iter))
		gtk_tree_selection_select_iter(sel, &iter);

	num_results++;
	if (last_key != NULL)
		gpgme_key_unref(last_key);
	last_key = key;
	key = NULL;
        update_progress (sk, ++running, pattern);
        while (gtk_events_pending ())
            gtk_main_iteration ();
    }
 
    if (exact_match == TRUE && num_results == 1) {
	    if (last_key->uids->validity < GPGME_VALIDITY_FULL && 
		!use_untrusted(last_key, last_uid, proto))
		    exact_match = FALSE;
    }

    debug_print ("%% %s:%d:  ready\n", __FILE__ ,__LINE__ );
    if (gpgme_err_code(err) != GPG_ERR_EOF) {
        debug_print ("** gpgme_op_keylist_next failed: %s\n",
                     gpgme_strerror (err));
        gpgme_op_keylist_end(ctx);
    }
    if (!exact_match || num_results != 1) {
	    sk->select_ctx = NULL;
	    gpgme_release (ctx);
    }

    if (exact_match && num_results == 1)
	    return last_key;

    if (last_key != NULL)
        gpgme_key_unref(last_key);

    return NULL;
}


static void
view_row_activated_cb(GtkTreeView *view,
		GtkTreePath *path,
		GtkTreeViewColumn *column,
		gpointer user_data)
{
	select_btn_cb(NULL, user_data);
}


static void 
create_dialog (struct select_keys_s *sk)
{
    GtkWidget *window;
    GtkWidget *vbox, *vbox2, *hbox;
    GtkWidget *bbox;
    GtkWidget *scrolledwin;
    GtkWidget *view;
    GtkWidget *label;
    GtkWidget *select_btn, *cancel_btn, *dont_encrypt_btn, *other_btn;
    GtkListStore *store;
    GtkCellRenderer *rdr;
    GtkTreeViewColumn *col;
    GtkTreeSelection *sel;
    gint i = 0;

    g_assert (!sk->window);
    window = gtkut_window_new (GTK_WINDOW_TOPLEVEL, "select-keys");
    gtk_widget_set_size_request (window, 560, 280);
    gtk_container_set_border_width (GTK_CONTAINER (window), 8);
    gtk_window_set_title (GTK_WINDOW (window), _("Select Keys"));
    gtk_window_set_modal (GTK_WINDOW (window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_cb), sk);
    g_signal_connect (G_OBJECT (window), "key_press_event",
                      G_CALLBACK (key_pressed_cb), sk);
    MANAGE_WINDOW_SIGNALS_CONNECT (window);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    hbox  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new ( "" );
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

    scrolledwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

		store = gtk_list_store_new(N_COL_TITLES,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_STRING,
				G_TYPE_POINTER,
				-1);

		view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
		g_object_unref(store);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), FALSE);
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);

		rdr = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Size"), rdr,
				"markup", COL_ALGO, NULL);
		gtk_tree_view_column_set_min_width(col, COL_ALGO_WIDTH);
		gtk_tree_view_column_set_sort_column_id(col, i++);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		col = gtk_tree_view_column_new_with_attributes(_("Key ID"), rdr,
				"markup", COL_KEYID, NULL);
		gtk_tree_view_column_set_min_width(col, COL_KEYID_WIDTH);
		gtk_tree_view_column_set_sort_column_id(col, i++);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		col = gtk_tree_view_column_new_with_attributes(_("Name"), rdr,
				"markup", COL_NAME, NULL);
		gtk_tree_view_column_set_min_width(col, COL_NAME_WIDTH);
		gtk_tree_view_column_set_sort_column_id(col, i++);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		col = gtk_tree_view_column_new_with_attributes(_("Address"), rdr,
				"markup", COL_ADDRESS, NULL);
		gtk_tree_view_column_set_min_width(col, COL_ADDRESS_WIDTH);
		gtk_tree_view_column_set_sort_column_id(col, i++);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		col = gtk_tree_view_column_new_with_attributes(_("Trust"), rdr,
				"markup", COL_TRUST, NULL);
		gtk_tree_view_column_set_min_width(col, COL_TRUST_WIDTH);
		gtk_tree_view_column_set_sort_column_id(col, i++);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		g_signal_connect(G_OBJECT(view), "row-activated",
				G_CALLBACK(view_row_activated_cb), sk);

    gtk_container_add (GTK_CONTAINER (scrolledwin), view);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    /* TRANSLATORS: check that the accelerators in _Select, _Other and
     * Do_n't encrypt are different than the one in the stock Cancel
     * button */
    gtkut_stock_button_set_create (&bbox, 
                                   &select_btn, NULL, _("_Select"),
		   		   &other_btn, NULL, _("_Other"),
		    		   &dont_encrypt_btn, NULL, _("Do_n't encrypt"));
    
    cancel_btn = gtk_button_new_with_mnemonic("_Cancel");
    gtk_widget_set_can_default(cancel_btn, TRUE);
    gtk_box_pack_start(GTK_BOX(bbox), cancel_btn, TRUE, TRUE, 0);
    gtk_widget_show(cancel_btn);
    gtk_box_pack_end (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
    gtk_widget_grab_default (select_btn);

    g_signal_connect (G_OBJECT (select_btn), "clicked",
                      G_CALLBACK (select_btn_cb), sk);
    g_signal_connect (G_OBJECT(cancel_btn), "clicked",
                      G_CALLBACK (cancel_btn_cb), sk);
    g_signal_connect (G_OBJECT(dont_encrypt_btn), "clicked",
                      G_CALLBACK (dont_encrypt_btn_cb), sk);
    g_signal_connect (G_OBJECT (other_btn), "clicked",
                      G_CALLBACK (other_btn_cb), sk);

    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

    sk->window = window;
    sk->toplabel = GTK_LABEL (label);
    sk->view  = view;
}


/* Function called by gtk_tree_model_foreach() upon dialog close,
 * which unrefs the gpgme_key_t pointer from each model line */
static gboolean
close_dialog_foreach_func(GtkTreeModel *model,
		GtkTreePath *path,
		GtkTreeIter *iter,
		gpointer user_data)
{
	gpgme_key_t key;

	gtk_tree_model_get(model, iter, COL_PTR, &key, -1);
	gpgme_key_unref(key);
	return FALSE;
}


static void
open_dialog (struct select_keys_s *sk)
{
    if (!sk->window)
        create_dialog (sk);
    manage_window_set_transient (GTK_WINDOW (sk->window));
    sk->okay = 0;
    sk->sort_column = N_COL_TITLES; /* use an invalid value */
    sk->sort_type = GTK_SORT_ASCENDING;
}


static void
close_dialog (struct select_keys_s *sk)
{
    GtkTreeModel *model;
    cm_return_if_fail (sk);

    debug_print("pgpcore select-keys dialog closing\n");
    if (sk->view != NULL) {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(sk->view));
        gtk_tree_model_foreach(model, close_dialog_foreach_func, NULL);
        gtk_list_store_clear(GTK_LIST_STORE(model));
    }

    gtk_widget_destroy (sk->window);
    sk->window = NULL;
}


static gint
delete_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    struct select_keys_s *sk = data;

    sk->okay = 0;
    gtk_main_quit ();

    return TRUE;
}


static gboolean
key_pressed_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    struct select_keys_s *sk = data;

    cm_return_val_if_fail (sk, FALSE);
    if (event && event->keyval == GDK_KEY_Escape) {
        sk->okay = 0;
        gtk_main_quit ();
    }
    return FALSE;
}


static void 
select_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;
    gboolean use_key;
    gpgme_key_t key;

    cm_return_if_fail (sk);

    key = gtkut_tree_view_get_selected_pointer(
        GTK_TREE_VIEW(sk->view), COL_PTR,
				NULL, NULL, NULL);
    if (key) {
        gpgme_user_id_t uid;
	for (uid = key->uids; uid; uid = uid->next) {
		gchar *raw_mail = NULL;

		if (!uid->email)
			continue;
		raw_mail = g_strdup(uid->email);
		extract_address(raw_mail);
		if (sk->pattern && !strcasecmp(sk->pattern, raw_mail)) {
			g_free(raw_mail);
			break;
		}
		g_free(raw_mail);
	}
	if (!uid)
		uid = key->uids;

        if ( uid->validity < GPGME_VALIDITY_FULL ) {
            use_key = use_untrusted(key, uid, sk->proto);
            if (!use_key) {
                debug_print ("** Key untrusted, will not encrypt\n");
                return;
            }
        }
        sk->kset = g_realloc(sk->kset,
                sizeof(gpgme_key_t) * (sk->num_keys + 1));
        gpgme_key_ref(key);
        sk->kset[sk->num_keys] = key;
        sk->num_keys++;
        sk->okay = 1;
	sk->result = KEY_SELECTION_OK;
        gtk_main_quit ();
    }
}


static void 
cancel_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;

    cm_return_if_fail (sk);
    sk->okay = 0;
    sk->result = KEY_SELECTION_CANCEL;
    if (sk->select_ctx)
        gpgme_cancel (sk->select_ctx);
    gtk_main_quit ();
}

static void 
dont_encrypt_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;

    cm_return_if_fail (sk);
    sk->okay = 0;
    sk->result = KEY_SELECTION_DONT;
    if (sk->select_ctx)
        gpgme_cancel (sk->select_ctx);
    gtk_main_quit ();
}

static void
other_btn_cb (GtkWidget *widget, gpointer data)
{
    struct select_keys_s *sk = data;
    char *uid;

    cm_return_if_fail (sk);
    uid = input_dialog ( _("Add key"),
                         _("Enter another user or key ID:"),
                         NULL );
    if (!uid)
        return;
    if (fill_view (sk, uid, sk->proto) != NULL) {
	    gpgme_release(sk->select_ctx);
	    sk->select_ctx = NULL;
    }
    update_progress (sk, 0, sk->pattern);
    g_free (uid);
}


static gboolean
use_untrusted (gpgme_key_t key, gpgme_user_id_t uid, gpgme_protocol_t proto)
{
    AlertValue aval;
    gchar *buf = NULL;
    gchar *title = NULL;
    if (proto != GPGME_PROTOCOL_OpenPGP)
    	return TRUE;

    title = g_strdup_printf(_("Encrypt to %s <%s>"), uid->name, uid->email);
    buf = g_strdup_printf(_("This encryption key is not fully trusted.\n"
	       "If you choose to encrypt the message with this key, you don't\n"
	       "know for sure that it will go to the person you mean it to.\n\n"
	       "Key details: ID %s, primary identity %s &lt;%s&gt;\n\n"
	       "Do you trust this key enough to use it anyway?"), 
	       key->subkeys->keyid, key->uids->name, key->uids->email);
    aval = alertpanel(title, buf,
		      NULL, _("_No"), NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_FIRST);
    g_free(buf);
    g_free(title);
    if (aval == G_ALERTALTERNATE)
	return TRUE;
    else
	return FALSE;
}

#endif /*USE_GPGME*/
