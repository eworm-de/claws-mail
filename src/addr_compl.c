/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 *
 * Copyright (c) 2000-2003 by Alfons Hoogervorst <alfons@proteus.demon.nl>
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
#include "intl.h"
#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkscrolledwindow.h>

#include <string.h>
#include <ctype.h>
#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "addressbook.h"
#include "addr_compl.h"
#include "utils.h"
#include "addritem.h"
#include "addrquery.h"
#include <pthread.h>

/* How it works:
 *
 * The address book is read into memory. We set up an address list
 * containing all address book entries. Next we make the completion
 * list, which contains all the completable strings, and store a
 * reference to the address entry it belongs to.
 * After calling the g_completion_complete(), we get a reference
 * to a valid email address.  
 *
 * Completion is very simplified. We never complete on another searchTerm,
 * i.e. we neglect the next smallest possible searchTerm for the current
 * completion cache. This is simply done so we might break up the
 * addresses a little more (e.g. break up alfons@proteus.demon.nl into
 * something like alfons, proteus, demon, nl; and then completing on
 * any of those words).
 */

typedef struct _CompletionWindow CompletionWindow;
struct _CompletionWindow {
	gint      listCount;
	gchar     *searchTerm;
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *clist;
};

/**
 * Current query ID.
 */
static gint _queryID_ = 0;

/**
 * Completion idle ID.
 */
static gint _completionIdleID_ = 0;

/**
 * Completion window.
 */
static CompletionWindow *_compWindow_ = NULL;

/**
 * Mutex to protect callback from multiple threads.
 */
static pthread_mutex_t _completionMutex_ = PTHREAD_MUTEX_INITIALIZER;

/**
 * Completion queue list.
 */
static GList *_displayQueue_ = NULL;

/**
 * Create a completion window object.
 * \return Initialized completion window.
 */
static CompletionWindow *addrcompl_create_window( void ) {
	CompletionWindow *cw;

	cw = g_new0( CompletionWindow, 1 );
	cw->listCount = 0;
	cw->searchTerm = NULL;
	cw->window = NULL;
	cw->entry = NULL;
	cw->clist = NULL;

	return cw;	
}

/**
 * Destroy completion window.
 * \param cw Window to destroy.
 */
static void addrcompl_destroy_window( CompletionWindow *cw ) {
	/* Remove idler function... or application may not terminate */
	if( _completionIdleID_ != 0 ) {
		gtk_idle_remove( _completionIdleID_ );
		_completionIdleID_ = 0;
	}

	/* Now destroy window */	
	if( cw ) {
		/* Clear references to widgets */
		cw->entry = NULL;
		cw->clist = NULL;

		/* Free objects */
		if( cw->window ) {
			gtk_widget_hide( cw->window );
			gtk_widget_destroy( cw->window );
		}
		cw->window = NULL;
	}
}

/**
 * Free up completion window.
 * \param cw Window to free.
 */
static void addrcompl_free_window( CompletionWindow *cw ) {
	/* printf( "addrcompl_free_window...\n" ); */
	if( cw ) {
		addrcompl_destroy_window( cw );

		g_free( cw->searchTerm );
		cw->searchTerm = NULL;

		/* Clear references */		
		cw->listCount = 0;

		/* Free object */		
		g_free( cw );
	}
	/* printf( "addrcompl_free_window...done\n" ); */
}

/**
 * Retrieve a possible address (or a part) from an entry box. To make life
 * easier, we only look at the last valid address component; address
 * completion only works at the last string component in the entry box.
 *
 * \param entry Address entry field.
 * \param start_pos Address of start position of address.
 * \return Possible address.
 */ 
static gchar *get_address_from_edit(GtkEntry *entry, gint *start_pos)
{
	const gchar *edit_text;
	gint cur_pos;
	wchar_t *wtext;
	wchar_t *wp;
	wchar_t rfc_mail_sep;
	wchar_t quote;
	wchar_t lt;
	wchar_t gt;
	gboolean in_quote = FALSE;
	gboolean in_bracket = FALSE;
	gchar *str;

	if (mbtowc(&rfc_mail_sep, ",", 1) < 0) return NULL;
	if (mbtowc(&quote, "\"", 1) < 0) return NULL;
	if (mbtowc(&lt, "<", 1) < 0) return NULL;
	if (mbtowc(&gt, ">", 1) < 0) return NULL;

	edit_text = gtk_entry_get_text(entry);
	if (edit_text == NULL) return NULL;

	wtext = strdup_mbstowcs(edit_text);
	g_return_val_if_fail(wtext != NULL, NULL);

	cur_pos = gtk_editable_get_position(GTK_EDITABLE(entry));

	/* scan for a separator. doesn't matter if walk points at null byte. */
	for (wp = wtext + cur_pos; wp > wtext; wp--) {
		if (*wp == quote)
			in_quote ^= TRUE;
		else if (!in_quote) {
			if (!in_bracket && *wp == rfc_mail_sep)
				break;
			else if (*wp == gt)
				in_bracket = TRUE;
			else if (*wp == lt)
				in_bracket = FALSE;
		}
	}

	/* have something valid */
	if (wcslen(wp) == 0) {
		g_free(wtext);
		return NULL;
	}

#define IS_VALID_CHAR(x) \
	(iswalnum(x) || (x) == quote || (x) == lt || ((x) > 0x7f))

	/* now scan back until we hit a valid character */
	for (; *wp && !IS_VALID_CHAR(*wp); wp++)
		;

#undef IS_VALID_CHAR

	if (wcslen(wp) == 0) {
		g_free(wtext);
		return NULL;
	}

	if (start_pos) *start_pos = wp - wtext;

	str = strdup_wcstombs(wp);
	g_free(wtext);

	return str;
}

/**
 * Replace text in entry field with specified address.
 * \param entry     Address entry field.
 * \param newtext   New text.
 * \param start_pos Insertion point in entry field.
 */
static void replace_address_in_edit(GtkEntry *entry, const gchar *newtext,
			     gint start_pos)
{
	if (!newtext) return;
	gtk_editable_delete_text(GTK_EDITABLE(entry), start_pos, -1);
	gtk_editable_insert_text(GTK_EDITABLE(entry), newtext, strlen(newtext),
				 &start_pos);
	gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

/**
 * Resize window to accommodate maximum number of address entries.
 * \param cw Completion window.
 */
static void addrcompl_resize_window( CompletionWindow *cw ) {
	GtkRequisition r;
	gint x, y, width, height, depth;

	/* Get current geometry of window */
	gdk_window_get_geometry( cw->window->window, &x, &y, &width, &height, &depth );

	gtk_widget_size_request( cw->clist, &r );
	gtk_widget_set_usize( cw->window, width, r.height );
	gtk_widget_show_all( cw->window );
	gtk_widget_size_request( cw->clist, &r );

	/* Adjust window height to available screen space */
	if( ( y + r.height ) > gdk_screen_height() ) {
		gtk_window_set_policy( GTK_WINDOW( cw->window ), TRUE, FALSE, FALSE );
		gtk_widget_set_usize( cw->window, width, gdk_screen_height() - y );
	}
}

/**
 * Add an address the completion window address list.
 * \param cw Completion window.
 * \param address Address to add.
 */
static void addrcompl_add_entry( CompletionWindow *cw, gchar *address ) {
	gchar *text[] = { NULL, NULL };

	/* printf( "\t\tAdding :%s\n", address ); */
	text[0] = address;
	gtk_clist_append( GTK_CLIST(cw->clist), text );
	cw->listCount++;

	/* Resize window */
	addrcompl_resize_window( cw );
	gtk_grab_add( cw->window );

	if( cw->listCount == 2 ) {
		/* Move off first row */
		gtk_clist_select_row( GTK_CLIST(cw->clist), 1, 0);
	}
}

/**
 * Completion idle function. This function is called by the main (UI) thread
 * during UI idle time while an address search is in progress. Items from the
 * display queue are processed and appended to the address list.
 *
 * \param data Target completion window to receive email addresses.
 * \return <i>TRUE</i> to ensure that idle event do not get ignored.
 */
static gboolean addrcompl_idle( gpointer data ) {
	GList *node;
	gchar *address;
	CompletionWindow *cw;

	/* Process all entries in display queue */
	pthread_mutex_lock( & _completionMutex_ );
	if( _displayQueue_ ) {
		cw = data;
		node = _displayQueue_;
		while( node ) {
			ItemEMail *email = node->data;
			/* printf( "email/address ::%s::\n", email->address ); */
			address = addritem_format_email( email );
			/* printf( "address ::: %s :::\n", address ); */
			addrcompl_add_entry( cw, address );
			g_free( address );
			node = g_list_next( node );
		}
		g_list_free( _displayQueue_ );
		_displayQueue_ = NULL;
	}
	pthread_mutex_unlock( & _completionMutex_ );

	return TRUE;
}

/**
 * Callback entry point. The background thread (if any) appends the address
 * list to the display queue.
 * \param queryID    Query ID of search request.
 * \param listEMail  List of zero of more email objects that met search
 *                   criteria.
 * \param target     Target object to received data.
 */
static gint addrcompl_callback(
	gint queryID, GList *listEMail, gpointer target )
{
	GList *node;
	gchar *address;

	pthread_mutex_lock( & _completionMutex_ );
	if( queryID == _queryID_ ) {
		/* Append contents to end of display queue */
		node = listEMail;
		while( node ) {
			ItemEMail *email = node->data;
			if( target ) {
				/*
				printf( "\temail/address ::%s::\n", email->address );
				*/
				_displayQueue_ = g_list_append( _displayQueue_, email );
			}
			node = g_list_next( node );
		}
	}
	pthread_mutex_unlock( & _completionMutex_ );
}

/**
 * Start the search.
 */
static void addrcompl_start_search( void ) {
	gchar *searchTerm;

	searchTerm = g_strdup( _compWindow_->searchTerm );

	/* Clear out display queue */
	pthread_mutex_lock( & _completionMutex_ );

	g_list_free( _displayQueue_ );
	_displayQueue_ = NULL;

	pthread_mutex_unlock( & _completionMutex_ );

	/* Setup the search */
	_queryID_ = addressbook_setup_search(
		searchTerm, _compWindow_, addrcompl_callback );
	g_free( searchTerm );

	/* Sit back and wait until something happens */
	_completionIdleID_ =
		gtk_idle_add( ( GtkFunction ) addrcompl_idle, _compWindow_ );
	addressbook_start_search( _queryID_ );
}

/*
 * address completion entry ui. the ui (completion list was inspired by galeon's
 * auto completion list). remaining things powered by sylpheed's completion engine.
 */

#define ENTRY_DATA_TAB_HOOK	"tab_hook"	/* used to lookup entry */

static gboolean address_completion_entry_key_pressed	(GtkEntry    *entry,
							 GdkEventKey *ev,
							 gpointer     data);
static gboolean address_completion_complete_address_in_entry
							(GtkEntry    *entry);

static void address_completion_create_completion_window	(GtkEntry    *entry);

static void completion_window_select_row(GtkCList	 *clist,
					 gint		  row,
					 gint		  col,
					 GdkEvent	 *event,
					 CompletionWindow *compWin );

static gboolean completion_window_button_press
					(GtkWidget	 *widget,
					 GdkEventButton  *event,
					 CompletionWindow *compWin );

static gboolean completion_window_key_press
					(GtkWidget	 *widget,
					 GdkEventKey	 *event,
					 CompletionWindow *compWin );


/**
 * Advance selection to previous/next item in list.
 * \param clist   List to process.
 * \param forward Set to <i>TRUE</i> to select next or <i>FALSE</i> for
 *                previous entry.
 */
static void completion_window_advance_selection(GtkCList *clist, gboolean forward)
{
	int row;

	g_return_if_fail(clist != NULL);

	if( clist->selection ) {
		row = GPOINTER_TO_INT(clist->selection->data);
		row = forward ? ( row + 1 ) : ( row - 1 );
		gtk_clist_freeze(clist);
		gtk_clist_select_row(clist, row, 0);
		gtk_clist_thaw(clist);
	}
}

/**
 * Apply the current selection in the list to the entry field. Focus is also
 * moved to the next widget so that Tab key works correctly.
 * \param clist List to process.
 * \param entry Address entry field.
 */
static void completion_window_apply_selection(GtkCList *clist, GtkEntry *entry)
{
	gchar *address = NULL, *text = NULL;
	gint   cursor_pos, row;
	GtkWidget *parent;

	g_return_if_fail(clist != NULL);
	g_return_if_fail(entry != NULL);
	g_return_if_fail(clist->selection != NULL);

	/* First remove the idler */
	if( _completionIdleID_ != 0 ) {
		gtk_idle_remove( _completionIdleID_ );
		_completionIdleID_ = 0;
	}

	/* Process selected item */
	row = GPOINTER_TO_INT(clist->selection->data);

	address = get_address_from_edit(entry, &cursor_pos);
	g_free(address);
	gtk_clist_get_text(clist, row, 0, &text);
	replace_address_in_edit(entry, text, cursor_pos);

	/* Move focus to next widget */
	parent = GTK_WIDGET(entry)->parent;
	if( parent ) {
		gtk_container_focus( GTK_CONTAINER(parent), GTK_DIR_TAB_FORWARD );
	}

}

/**
 * Listener that watches for tab or other keystroke in address entry field.
 * \param entry Address entry field.
 * \param ev    Event object.
 * \param data  User data.
 * \return <i>TRUE</i>.
 */
static gboolean address_completion_entry_key_pressed(GtkEntry    *entry,
						     GdkEventKey *ev,
						     gpointer     data)
{
	if (ev->keyval == GDK_Tab) {
		if (address_completion_complete_address_in_entry(entry)) {
			/* route a void character to the default handler */
			/* this is a dirty hack; we're actually changing a key
			 * reported by the system. */
			ev->keyval = GDK_AudibleBell_Enable;
			ev->state &= ~GDK_SHIFT_MASK;
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry),
						     "key_press_event");

			/* printf( "entry_key_pressed::create window\n" ); */
			address_completion_create_completion_window( entry );
			/* printf( "entry_key_pressed::create window...done\n" ); */
			addrcompl_start_search();
		}
	}
	return TRUE;
}

/**
 * Initialize search term for address completion.
 * \param entry Address entry field.
 */
static gboolean address_completion_complete_address_in_entry( GtkEntry *entry )
{
	gint cursor_pos;
	gchar *searchTerm;

	g_return_val_if_fail(entry != NULL, FALSE);

	if (!GTK_WIDGET_HAS_FOCUS(entry)) return FALSE;

	/* get an address component from the cursor */
	searchTerm = get_address_from_edit( entry, &cursor_pos );
	if (! searchTerm ) return FALSE;

	/* Clear any existing search */
	if( _compWindow_->searchTerm ) {
		g_free( _compWindow_->searchTerm );
	}

	/* Replace with new search term */
	_compWindow_->searchTerm = searchTerm;

	return TRUE;
}

/**
 * Create new address completion window for specified entry.
 * \param entry_ Entry widget to associate with window.
 */
static void address_completion_create_completion_window( GtkEntry *entry_ )
{
	gint x, y, height, width, depth;
	GtkWidget *scroll, *clist;
	GtkRequisition r;
	GtkWidget *window;
	GtkWidget *entry = GTK_WIDGET(entry_);
	gchar *searchTerm;

	/* Create new window and list */
	window = gtk_window_new(GTK_WINDOW_POPUP);
	clist  = gtk_clist_new(1);

	/* Destroy any existing window */
	addrcompl_destroy_window( _compWindow_ );

	/* Create new object */
	_compWindow_->window = window;
	_compWindow_->entry = entry;
	_compWindow_->clist = clist;
	_compWindow_->listCount = 0;

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(window), scroll);
	gtk_container_add(GTK_CONTAINER(scroll), clist);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);

	/* Use entry widget to create initial window */
	gdk_window_get_geometry(entry->window, &x, &y, &width, &height, &depth);
	gdk_window_get_deskrelative_origin (entry->window, &x, &y);
	y += height;
	gtk_widget_set_uposition(window, x, y);

	/* Resize window to fit initial (empty) address list */
	gtk_widget_size_request( clist, &r );
	gtk_widget_set_usize( window, width, r.height );
	gtk_widget_show_all( window );
	gtk_widget_size_request( clist, &r );

	/* Setup handlers */
	gtk_signal_connect(GTK_OBJECT(clist), "select_row",
			   GTK_SIGNAL_FUNC(completion_window_select_row),
			   _compWindow_ );
	gtk_signal_connect(GTK_OBJECT(window),
			   "button-press-event",
			   GTK_SIGNAL_FUNC(completion_window_button_press),
			   _compWindow_ );
	gtk_signal_connect(GTK_OBJECT(window),
			   "key-press-event",
			   GTK_SIGNAL_FUNC(completion_window_key_press),
			   _compWindow_ );
	gdk_pointer_grab(window->window, TRUE,
			 GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK,
			 NULL, NULL, GDK_CURRENT_TIME);
	gtk_grab_add( window );

	/* this gets rid of the irritating focus rectangle that doesn't
	 * follow the selection */
	GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);

	/* Add first entry into address list */
	searchTerm = g_strdup( _compWindow_->searchTerm );
	addrcompl_add_entry( _compWindow_, searchTerm );
	gtk_clist_select_row(GTK_CLIST(clist), 0, 0);

	g_free( searchTerm );
}

/**
 * Respond to select row event in clist object. selection sends completed
 * address to entry. Note: event is NULL if selected by anything else than a
 * mouse button.
 * \param widget   Window object.
 * \param event    Event.
 * \param compWind Reference to completion window.
 */
static void completion_window_select_row(GtkCList *clist, gint row, gint col,
					 GdkEvent *event,
					 CompletionWindow *compWin )
{
	GtkEntry *entry;

	g_return_if_fail(compWin != NULL);

	entry = GTK_ENTRY(compWin->entry);
	g_return_if_fail(entry != NULL);

	/* Don't update unless user actually selects ! */
	if (!event || event->type != GDK_BUTTON_RELEASE)
		return;

	/* User selected address by releasing the mouse in drop-down list*/
	completion_window_apply_selection( clist, entry );

	addrcompl_destroy_window( _compWindow_ );
}

/**
 * Respond to button press in completion window. Check if mouse click is
 * anywhere outside the completion window. In that case the completion
 * window is destroyed, and the original searchTerm is restored.
 *
 * \param widget   Window object.
 * \param event    Event.
 * \param compWin  Reference to completion window.
 */
static gboolean completion_window_button_press(GtkWidget *widget,
					       GdkEventButton *event,
					       CompletionWindow *compWin )

{
	GtkWidget *event_widget, *entry;
	gchar *searchTerm;
	gint cursor_pos;
	gboolean restore = TRUE;

	g_return_val_if_fail(compWin != NULL, FALSE);

	entry = compWin->entry;
	g_return_val_if_fail(entry != NULL, FALSE);

	/* Test where mouse was clicked */
	event_widget = gtk_get_event_widget((GdkEvent *)event);
	if (event_widget != widget) {
		while (event_widget) {
			if (event_widget == widget)
				return FALSE;
			else if (event_widget == entry) {
				restore = FALSE;
				break;
			}
			event_widget = event_widget->parent;
		}
	}

	if (restore) {
		/* Clicked outside of completion window - restore */
		searchTerm = _compWindow_->searchTerm;
		g_free(get_address_from_edit(GTK_ENTRY(entry), &cursor_pos));
		replace_address_in_edit(GTK_ENTRY(entry), searchTerm, cursor_pos);
	}

	addrcompl_destroy_window( _compWindow_ );

	return TRUE;
}

/**
 * Respond to key press in completion window.
 * \param widget   Window object.
 * \param event    Event.
 * \param compWind Reference to completion window.
 */
static gboolean completion_window_key_press(GtkWidget *widget,
					    GdkEventKey *event,
					    CompletionWindow *compWin )
{
	GdkEventKey tmp_event;
	GtkWidget *entry;
	gchar *searchTerm;
	gint cursor_pos;
	GtkWidget *clist;

	g_return_val_if_fail(compWin != NULL, FALSE);

	entry = compWin->entry;
	clist = compWin->clist;
	g_return_val_if_fail(entry != NULL, FALSE);

	/* allow keyboard navigation in the alternatives clist */
	if (event->keyval == GDK_Up || event->keyval == GDK_Down ||
	    event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down) {
		completion_window_advance_selection
			(GTK_CLIST(clist),
			 event->keyval == GDK_Down ||
			 event->keyval == GDK_Page_Down ? TRUE : FALSE);
		return FALSE;
	}		

	/* also make tab / shift tab go to next previous completion entry. we're
	 * changing the key value */
	if (event->keyval == GDK_Tab || event->keyval == GDK_ISO_Left_Tab) {
		event->keyval = (event->state & GDK_SHIFT_MASK)
			? GDK_Up : GDK_Down;
		/* need to reset shift state if going up */
		if (event->state & GDK_SHIFT_MASK)
			event->state &= ~GDK_SHIFT_MASK;
		completion_window_advance_selection(GTK_CLIST(clist), 
			event->keyval == GDK_Down ? TRUE : FALSE);
		return FALSE;
	}

	/* look for presses that accept the selection */
	if (event->keyval == GDK_Return || event->keyval == GDK_space) {
		/* User selected address with a key press */

		/* Display selected address in entry field */		
		completion_window_apply_selection(
			GTK_CLIST(clist), GTK_ENTRY(entry) );

		/* Discard the window */
		addrcompl_destroy_window( _compWindow_ );
		return FALSE;
	}

	/* key state keys should never be handled */
	if (event->keyval == GDK_Shift_L
		 || event->keyval == GDK_Shift_R
		 || event->keyval == GDK_Control_L
		 || event->keyval == GDK_Control_R
		 || event->keyval == GDK_Caps_Lock
		 || event->keyval == GDK_Shift_Lock
		 || event->keyval == GDK_Meta_L
		 || event->keyval == GDK_Meta_R
		 || event->keyval == GDK_Alt_L
		 || event->keyval == GDK_Alt_R) {
		return FALSE;
	}

	/* some other key, let's restore the searchTerm (orignal text) */
	searchTerm = _compWindow_->searchTerm;
	g_free(get_address_from_edit(GTK_ENTRY(entry), &cursor_pos));
	replace_address_in_edit(GTK_ENTRY(entry), searchTerm, cursor_pos);

	/* make sure anything we typed comes in the edit box */
	tmp_event.type       = event->type;
	tmp_event.window     = entry->window;
	tmp_event.send_event = TRUE;
	tmp_event.time       = event->time;
	tmp_event.state      = event->state;
	tmp_event.keyval     = event->keyval;
	tmp_event.length     = event->length;
	tmp_event.string     = event->string;
	gtk_widget_event(entry, (GdkEvent *)&tmp_event);

	/* and close the completion window */
	addrcompl_destroy_window( _compWindow_ );

	return TRUE;
}

/*
 * ============================================================================
 * Publically accessible functions.
 * ============================================================================
 */
/**
 * Setup completion object.
 */
void addrcompl_initialize( void ) {
	/* printf( "addrcompl_initialize...\n" ); */
	if( ! _compWindow_ ) {
		_compWindow_ = addrcompl_create_window();
	}
	_queryID_ = 0;
	_completionIdleID_ = 0;
	/* printf( "addrcompl_initialize...done\n" ); */
}

/**
 * Teardown completion object.
 */
void addrcompl_teardown( void ) {
	/* printf( "addrcompl_teardown...\n" ); */
	addrcompl_free_window( _compWindow_ );
	_compWindow_ = NULL;
	if( _displayQueue_ ) {
		g_list_free( _displayQueue_ );
	}
	_displayQueue_ = NULL;
	_completionIdleID_ = 0;
	/* printf( "addrcompl_teardown...done\n" ); */
}

/**
 * Start address completion operation.
 */
gint start_address_completion(void)
{
	if( ! _compWindow_ ) {
		addrcompl_initialize();
	}
	addressbook_read_all();
	return 0;
}

/**
 * Terminate addess completion.
 */
gint end_address_completion(void)
{
	return 0;
}

/**
 * Start address completion. Should be called when creating the main window
 * containing address completion entries. This originally cleared the cache.
 * Function no longer required?
 */
void address_completion_start(GtkWidget *mainwindow)
{
	start_address_completion();
}

/**
 * Need unique data to make unregistering signal handler possible for the auto
 * completed entry.
 */
#define COMPLETION_UNIQUE_DATA (GINT_TO_POINTER(0xfeefaa))

/**
 * Register specified entry widget for address completion.
 * \param entry Address entry field.
 */
void address_completion_register_entry(GtkEntry *entry)
{
	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));

	/* add hooked property */
	gtk_object_set_data(GTK_OBJECT(entry), ENTRY_DATA_TAB_HOOK, entry);

	/* add keypress event */
	gtk_signal_connect_full(GTK_OBJECT(entry), "key_press_event",
				GTK_SIGNAL_FUNC(address_completion_entry_key_pressed),
				NULL,
				COMPLETION_UNIQUE_DATA,
				NULL,
				0,
				0); /* magic */
}

/**
 * Unregister specified entry widget from address completion operations.
 * \param entry Address entry field.
 */
void address_completion_unregister_entry(GtkEntry *entry)
{
	GtkObject *entry_obj;

	g_return_if_fail(entry != NULL);
	g_return_if_fail(GTK_IS_ENTRY(entry));

	entry_obj = gtk_object_get_data(GTK_OBJECT(entry), ENTRY_DATA_TAB_HOOK);
	g_return_if_fail(entry_obj);
	g_return_if_fail(entry_obj == GTK_OBJECT(entry));

	/* has the hooked property? */
	gtk_object_set_data(GTK_OBJECT(entry), ENTRY_DATA_TAB_HOOK, NULL);

	/* remove the hook */
	gtk_signal_disconnect_by_func(GTK_OBJECT(entry), 
		GTK_SIGNAL_FUNC(address_completion_entry_key_pressed),
		COMPLETION_UNIQUE_DATA);
}

/**
 * End address completion. Should be called when main window with address
 * completion entries terminates. NOTE: this function assumes that it is
 * called upon destruction of the window.
 */
void address_completion_end(GtkWidget *mainwindow)
{
	/* if address_completion_end() is really called on closing the window,
	 * we don't need to unregister the set_focus_cb */
	end_address_completion();
}

/*
 * End of Source.
 */

