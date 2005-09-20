/* gtkaspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin (original code for ispell).
 * Copyright (c) 2002 Melvin Hadasht.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
/*
 * Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 * Adapted for Sylpheed (Claws) (c) 2001-2002 by Hiroyuki Yamamoto & 
 * The Sylpheed Claws Team.
 * Adapted for pspell (c) 2001-2002 Melvin Hadasht
 * Adapted for GNU/aspell (c) 2002 Melvin Hadasht
 */
 
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_ASPELL

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gdk/gdkkeysyms.h>

#include <aspell.h>

#include "utils.h"
#include "codeconv.h"
#include "alertpanel.h"
#include "gtkaspell.h"
#include "gtk/gtkutils.h"

#define ASPELL_FASTMODE       1
#define ASPELL_NORMALMODE     2
#define ASPELL_BADSPELLERMODE 3

#define GTKASPELLWORDSIZE 1024

/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024

/* number of suggestions to display on each menu. */
#define MENUCOUNT 15

/* 'config' must be defined as a 'AspellConfig *' */
#define RETURN_FALSE_IF_CONFIG_ERROR() \
{ \
	if (aspell_config_error_number(config) != 0) { \
		gtkaspellcheckers->error_message = g_strdup(aspell_config_error_message(config)); \
		return FALSE; \
	} \
}

#define CONFIG_REPLACE_RETURN_FALSE_IF_FAIL(option, value) { \
	aspell_config_replace(config, option, value);        \
	RETURN_FALSE_IF_CONFIG_ERROR();                      \
	}

typedef struct _GtkAspellCheckers {
	GSList		*checkers;
	GSList		*dictionary_list;
	gchar		*error_message;
} GtkAspellCheckers;

typedef struct _Dictionary {
	gchar *fullname;
	gchar *dictname;
	gchar *encoding;
} Dictionary;

typedef struct _GtkAspeller {
	Dictionary	*dictionary;
	gint		 sug_mode;
	AspellConfig	*config;
	AspellSpeller	*checker;
} GtkAspeller;

typedef void (*ContCheckFunc) (gpointer *gtkaspell);

struct _GtkAspell
{
	GtkAspeller	*gtkaspeller;
	GtkAspeller	*alternate_speller;
	gchar		*dictionary_path;
	gchar 		 theword[GTKASPELLWORDSIZE];
	gint  		 start_pos;
	gint  		 end_pos;
        gint 		 orig_pos;
	gint		 end_check_pos;
	gboolean	 misspelled;
	gboolean	 check_while_typing;
	gboolean	 use_alternate;

	ContCheckFunc 	 continue_check; 

	GtkWidget	*config_menu;
	GtkWidget	*popup_config_menu;
	GtkWidget	*sug_menu;
	GtkWidget	*replace_entry;
	GtkWidget 	*parent_window;

	gint		 default_sug_mode;
	gint		 max_sug;
	GList		*suggestions_list;

	GtkTextView	*gtktext;
	GdkColor 	 highlight;
	GtkAccelGroup	*accel_group;
};

typedef AspellConfig GtkAspellConfig;

/******************************************************************************/

static GtkAspellCheckers *gtkaspellcheckers;

/* Error message storage */
static void gtkaspell_checkers_error_message	(gchar		*message);

/* Callbacks */
static void entry_insert_cb			(GtkTextBuffer	*textbuf,
						 GtkTextIter	*iter,
						 gchar		*newtext, 
						 gint		len,
					 	 GtkAspell	*gtkaspell);
static void entry_delete_cb			(GtkTextBuffer	*textbuf,
						 GtkTextIter	*startiter,
						 GtkTextIter	*enditer,
						 GtkAspell	*gtkaspell);
static gint button_press_intercept_cb		(GtkTextView	*gtktext,
						 GdkEvent	*e, 
						 GtkAspell	*gtkaspell);

/* Checker creation */
static GtkAspeller* gtkaspeller_new		(Dictionary	*dict);
static GtkAspeller* gtkaspeller_real_new	(Dictionary	*dict);
static GtkAspeller* gtkaspeller_delete		(GtkAspeller	*gtkaspeller);
static GtkAspeller* gtkaspeller_real_delete	(GtkAspeller	*gtkaspeller);

/* Checker configuration */
static gint 		set_dictionary   		(AspellConfig *config, 
							 Dictionary *dict);
static void 		set_sug_mode_cb     		(GtkMenuItem *w, 
							 GtkAspell *gtkaspell);
static void 		set_real_sug_mode		(GtkAspell *gtkaspell, 
							 const char *themode);

/* Checker actions */
static gboolean check_at			(GtkAspell	*gtkaspell, 
						 int		 from_pos);
static gboolean	check_next_prev			(GtkAspell	*gtkaspell, 
						 gboolean	 forward);
static GList* misspelled_suggest	 	(GtkAspell	*gtkaspell, 
						 guchar		*word);
static void add_word_to_session_cb		(GtkWidget	*w, 
						 gpointer	 data);
static void add_word_to_personal_cb		(GtkWidget	*w, 
						 gpointer	 data);
static void replace_with_create_dialog_cb	(GtkWidget	*w,
						 gpointer	 data);
static void replace_with_supplied_word_cb	(GtkWidget	*w, 
						 GtkAspell	*gtkaspell);
static void replace_word_cb			(GtkWidget	*w, 
						 gpointer	data); 
static void replace_real_word			(GtkAspell	*gtkaspell, 
						 gchar		*newword);
static void check_with_alternate_cb		(GtkWidget	*w,
						 gpointer	 data);
static void use_alternate_dict			(GtkAspell	*gtkaspell);
static void toggle_check_while_typing_cb	(GtkWidget	*w, 
						 gpointer	 data);

/* Menu creation */
static void popup_menu				(GtkAspell	*gtkaspell, 
						 GdkEventButton	*eb);
static GtkMenu*	make_sug_menu			(GtkAspell	*gtkaspell);
static void populate_submenu			(GtkAspell	*gtkaspell, 
						 GtkWidget	*menu);
static GtkMenu*	make_config_menu		(GtkAspell	*gtkaspell);
static void set_menu_pos			(GtkMenu	*menu, 
						 gint		*x, 
						 gint		*y, 
						 gboolean	*push_in,
						 gpointer	 data);
/* Other menu callbacks */
static gboolean cancel_menu_cb			(GtkMenuShell	*w,
						 gpointer	 data);
static void change_dict_cb			(GtkWidget	*w, 
						 GtkAspell	*gtkaspell);
static void switch_to_alternate_cb		(GtkWidget	*w, 
						 gpointer	 data);

/* Misc. helper functions */
static void	 	set_point_continue		(GtkAspell *gtkaspell);
static void 		continue_check			(gpointer *gtkaspell);
static gboolean 	iswordsep			(unsigned char c);
static guchar 		get_text_index_whar		(GtkAspell *gtkaspell, 
							 int pos);
static gboolean 	get_word_from_pos		(GtkAspell *gtkaspell, 
							 gint pos, 
							 unsigned char* buf,
							 gint buflen,
							 gint *pstart, 
							 gint *pend);
static void 		allocate_color			(GtkAspell *gtkaspell,
							 gint rgbvalue);
static void 		change_color			(GtkAspell *gtkaspell, 
			 				 gint start, 
							 gint end, 
							 gchar *newtext,
							 GdkColor *color);
static guchar*		convert_to_aspell_encoding 	(const guchar *encoding);
static gint 		compare_dict			(Dictionary *a, 
							 Dictionary *b);
static void 		dictionary_delete		(Dictionary *dict);
static Dictionary *	dictionary_dup			(const Dictionary *dict);
static void 		free_suggestions_list		(GtkAspell *gtkaspell);
static void 		reset_theword_data		(GtkAspell *gtkaspell);
static void 		free_checkers			(gpointer elt, 
							 gpointer data);
static gint 		find_gtkaspeller		(gconstpointer aa, 
							 gconstpointer bb);
/* gtkspellconfig - only one config per session */
GtkAspellConfig * gtkaspellconfig;
static void destroy_menu(GtkWidget *widget, gpointer user_data);	

/******************************************************************************/
static gint get_textview_buffer_charcount(GtkTextView *view);

static gint get_textview_buffer_charcount(GtkTextView *view)
{
	GtkTextBuffer *buffer;

	g_return_val_if_fail(view, 0);

	buffer = gtk_text_view_get_buffer(view);
	g_return_val_if_fail(buffer, 0);

	return gtk_text_buffer_get_char_count(buffer);
}
static gint get_textview_buffer_offset(GtkTextView *view)
{
	GtkTextBuffer * buffer;
	GtkTextMark * mark;
	GtkTextIter iter;

	g_return_val_if_fail(view, 0);

	buffer = gtk_text_view_get_buffer(view);
	g_return_val_if_fail(buffer, 0);

	mark = gtk_text_buffer_get_insert(buffer);
	g_return_val_if_fail(mark, 0);

	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	return gtk_text_iter_get_offset(&iter);
}
static void set_textview_buffer_offset(GtkTextView *view, gint offset)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	g_return_if_fail(view);

	buffer = gtk_text_view_get_buffer(view);
	g_return_if_fail(buffer);

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, offset);
	gtk_text_buffer_place_cursor(buffer, &iter);
}
/******************************************************************************/

void gtkaspell_checkers_init(void)
{
	gtkaspellcheckers 		   = g_new(GtkAspellCheckers, 1);
	gtkaspellcheckers->checkers        = NULL;
	gtkaspellcheckers->dictionary_list = NULL;
	gtkaspellcheckers->error_message   = NULL;
}
	
void gtkaspell_checkers_quit(void)
{
	GSList *checkers;
	GSList *dict_list;

	if (gtkaspellcheckers == NULL) 
		return;

	if ((checkers  = gtkaspellcheckers->checkers)) {
		debug_print("Aspell: number of running checkers to delete %d\n",
				g_slist_length(checkers));

		g_slist_foreach(checkers, free_checkers, NULL);
		g_slist_free(checkers);
		gtkaspellcheckers->checkers = NULL;
	}

	if ((dict_list = gtkaspellcheckers->dictionary_list)) {
		debug_print("Aspell: number of dictionaries to delete %d\n",
				g_slist_length(dict_list));

		gtkaspell_free_dictionary_list(dict_list);
		gtkaspellcheckers->dictionary_list = NULL;
	}

	g_free(gtkaspellcheckers->error_message);
	gtkaspellcheckers->error_message = NULL;
	return;
}

static void gtkaspell_checkers_error_message (gchar *message)
{
	gchar *tmp;
	if (gtkaspellcheckers->error_message) {
		tmp = g_strdup_printf("%s\n%s", 
				      gtkaspellcheckers->error_message,
				      message);
		g_free(message);
		g_free(gtkaspellcheckers->error_message);
		gtkaspellcheckers->error_message = tmp;
	} else 
		gtkaspellcheckers->error_message = message;
}

const char *gtkaspell_checkers_strerror(void)
{
	g_return_val_if_fail(gtkaspellcheckers, "");
	return gtkaspellcheckers->error_message;
}

void gtkaspell_checkers_reset_error(void)
{
	g_return_if_fail(gtkaspellcheckers);
	
	g_free(gtkaspellcheckers->error_message);
	
	gtkaspellcheckers->error_message = NULL;
}

GtkAspell *gtkaspell_new(const gchar *dictionary_path,
			 const gchar *dictionary, 
			 const gchar *encoding,
			 gint  misspelled_color,
			 gboolean check_while_typing,
			 gboolean use_alternate,
			 GtkTextView *gtktext,
			 GtkWindow *parent_win)
{
	Dictionary 	*dict;
	GtkAspell 	*gtkaspell;
	GtkAspeller 	*gtkaspeller;
	GtkTextBuffer *buffer;

	g_return_val_if_fail(gtktext, NULL);
	buffer = gtk_text_view_get_buffer(gtktext);
	
	dict 	       = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(dictionary);
	dict->encoding = g_strdup(encoding);

	gtkaspeller    = gtkaspeller_new(dict); 
	dictionary_delete(dict);

	if (!gtkaspeller)
		return NULL;
	
	gtkaspell = g_new0(GtkAspell, 1);

	gtkaspell->dictionary_path    = g_strdup(dictionary_path);

	gtkaspell->gtkaspeller	      = gtkaspeller;
	gtkaspell->alternate_speller  = NULL;
	gtkaspell->theword[0]	      = 0x00;
	gtkaspell->start_pos	      = 0;
	gtkaspell->end_pos	      = 0;
	gtkaspell->orig_pos	      = -1;
	gtkaspell->end_check_pos      = -1;
	gtkaspell->misspelled	      = -1;
	gtkaspell->check_while_typing = check_while_typing;
	gtkaspell->continue_check     = NULL;
	gtkaspell->config_menu        = NULL;
	gtkaspell->popup_config_menu  = NULL;
	gtkaspell->sug_menu	      = NULL;
	gtkaspell->replace_entry      = NULL;
	gtkaspell->gtktext	      = gtktext;
	gtkaspell->default_sug_mode   = ASPELL_FASTMODE;
	gtkaspell->max_sug	      = -1;
	gtkaspell->suggestions_list   = NULL;
	gtkaspell->use_alternate      = use_alternate;
	gtkaspell->parent_window      = GTK_WIDGET(parent_win);
	
	allocate_color(gtkaspell, misspelled_color);

	g_signal_connect_after(G_OBJECT(buffer), "insert-text",
			       G_CALLBACK(entry_insert_cb), gtkaspell);
	g_signal_connect_after(G_OBJECT(buffer), "delete-range",
		               G_CALLBACK(entry_delete_cb), gtkaspell);
	g_signal_connect(G_OBJECT(gtktext), "button-press-event",
			 G_CALLBACK(button_press_intercept_cb),
			 gtkaspell);
	
	debug_print("Aspell: created gtkaspell %0x\n", (guint) gtkaspell);

	return gtkaspell;
}

void gtkaspell_delete(GtkAspell *gtkaspell) 
{
	GtkTextView *gtktext = gtkaspell->gtktext;
	
        g_signal_handlers_disconnect_by_func(G_OBJECT(gtktext),
					     G_CALLBACK(entry_insert_cb),
					     gtkaspell);
	g_signal_handlers_disconnect_by_func(G_OBJECT(gtktext),
					     G_CALLBACK(entry_delete_cb),
					     gtkaspell);
	g_signal_handlers_disconnect_by_func(G_OBJECT(gtktext),
					     G_CALLBACK(button_press_intercept_cb),
					     gtkaspell);

	gtkaspell_uncheck_all(gtkaspell);
	
	gtkaspeller_delete(gtkaspell->gtkaspeller);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller)
		gtkaspeller_delete(gtkaspell->alternate_speller);

	if (gtkaspell->sug_menu)
		gtk_widget_destroy(gtkaspell->sug_menu);

	if (gtkaspell->popup_config_menu)
		gtk_widget_destroy(gtkaspell->popup_config_menu);

	if (gtkaspell->config_menu)
		gtk_widget_destroy(gtkaspell->config_menu);

	if (gtkaspell->suggestions_list)
		free_suggestions_list(gtkaspell);

	g_free((gchar *)gtkaspell->dictionary_path);
	gtkaspell->dictionary_path = NULL;

	debug_print("Aspell: deleting gtkaspell %0x\n", (guint) gtkaspell);

	g_free(gtkaspell);

	gtkaspell = NULL;
}

static void entry_insert_cb(GtkTextBuffer *textbuf,
			    GtkTextIter *iter,
			    gchar *newtext,
			    gint len,
			    GtkAspell *gtkaspell)
{
	guint pos;

	g_return_if_fail(gtkaspell->gtkaspeller->checker);

	if (!gtkaspell->check_while_typing)
		return;

	pos = gtk_text_iter_get_offset(iter);
	
	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (pos >= 2)
			check_at(gtkaspell, pos - 2);

		/* did we just split a word? */
		if (pos < gtk_text_buffer_get_char_count(textbuf))
			check_at(gtkaspell, pos + 1);
	} else {
		/* check as they type, *except* if they're typing at the end (the most
                 * common case).
                 */
		if (pos < gtk_text_buffer_get_char_count(textbuf) &&
		    !iswordsep(get_text_index_whar(gtkaspell, pos))) {
			check_at(gtkaspell, pos - 1);
		}
	}
}

static void entry_delete_cb(GtkTextBuffer *textbuf,
			    GtkTextIter *startiter,
			    GtkTextIter *enditer,
			    GtkAspell *gtkaspell)
{
	int origpos;
	gint start, end;
    
	g_return_if_fail(gtkaspell->gtkaspeller->checker);

	if (!gtkaspell->check_while_typing)
		return;

	start = gtk_text_iter_get_offset(startiter);
	end = gtk_text_iter_get_offset(enditer);
	origpos = get_textview_buffer_offset(gtkaspell->gtktext);
	if (start) {
		check_at(gtkaspell, start - 1);
		check_at(gtkaspell, start);
	}

	set_textview_buffer_offset(gtkaspell->gtktext, origpos);
	/* this is to *UNDO* the selection, in case they were holding shift
         * while hitting backspace. */
	/* needed with textview ??? */
	/* gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos); */
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor,
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu.
 * HACK HACK HACK.
 */
static gint button_press_intercept_cb(GtkTextView *gtktext,
				      GdkEvent *e, 
				      GtkAspell *gtkaspell)
{
	GdkEventButton *eb;
	gboolean retval;

	g_return_val_if_fail(gtkaspell->gtkaspeller->checker, FALSE);

	if (e->type != GDK_BUTTON_PRESS) 
		return FALSE;
	eb = (GdkEventButton*) e;

	if (eb->button != 3) 
		return FALSE;

        g_signal_handlers_block_by_func(G_OBJECT(gtktext),
					G_CALLBACK(button_press_intercept_cb), 
					gtkaspell);
	g_signal_emit_by_name(G_OBJECT(gtktext), "button-release-event",
			      e, &retval);

	/* forge the leftclick */
	eb->button = 1;
	
	g_signal_emit_by_name(G_OBJECT(gtktext), "button-press-event",
			      e, &retval);
	g_signal_emit_by_name(G_OBJECT(gtktext), "button-release-event",
			      e, &retval);
	g_signal_handlers_unblock_by_func(G_OBJECT(gtktext),
					  G_CALLBACK(button_press_intercept_cb), 
					   gtkaspell);
	g_signal_stop_emission_by_name(G_OBJECT(gtktext), "button-press-event");
    
	/* now do the menu wackiness */
	popup_menu(gtkaspell, eb);
	gtk_grab_remove(GTK_WIDGET(gtktext));
	return FALSE;
}

/* Checker creation */
static GtkAspeller *gtkaspeller_new(Dictionary *dictionary)
{
	GSList 		*exist;
	GtkAspeller	*gtkaspeller = NULL;
	GtkAspeller	*tmp;
	Dictionary	*dict;

	g_return_val_if_fail(gtkaspellcheckers, NULL);

	g_return_val_if_fail(dictionary, NULL);

	if (dictionary->fullname == NULL)
		gtkaspell_checkers_error_message(
				g_strdup(_("No dictionary selected.")));
	
	g_return_val_if_fail(dictionary->fullname, NULL);
	
	if (dictionary->dictname == NULL) {
		gchar *tmp;

		tmp = strrchr(dictionary->fullname, G_DIR_SEPARATOR);

		if (tmp == NULL)
			dictionary->dictname = dictionary->fullname;
		else
			dictionary->dictname = tmp + 1;
	}

	dict = dictionary_dup(dictionary);

	tmp = g_new0(GtkAspeller, 1);
	tmp->dictionary = dict;

	exist = g_slist_find_custom(gtkaspellcheckers->checkers, tmp, 
				    find_gtkaspeller);
	
	g_free(tmp);

	if ((gtkaspeller = gtkaspeller_real_new(dict)) != NULL) {
		gtkaspellcheckers->checkers = g_slist_append(
				gtkaspellcheckers->checkers,
				gtkaspeller);

		debug_print("Aspell: Created a new gtkaspeller %0x\n",
				(gint) gtkaspeller);
	} else {
		dictionary_delete(dict);

		debug_print("Aspell: Could not create spell checker.\n");
	}

	debug_print("Aspell: number of existing checkers %d\n", 
			g_slist_length(gtkaspellcheckers->checkers));

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_real_new(Dictionary *dict)
{
	GtkAspeller		*gtkaspeller;
	AspellConfig		*config;
	AspellCanHaveError 	*ret;
	
	g_return_val_if_fail(gtkaspellcheckers, NULL);
	g_return_val_if_fail(dict, NULL);

	gtkaspeller = g_new(GtkAspeller, 1);
	
	gtkaspeller->dictionary = dict;
	gtkaspeller->sug_mode   = ASPELL_FASTMODE;

	config = new_aspell_config();

	if (!set_dictionary(config, dict))
		return NULL;
	
	ret = new_aspell_speller(config);
	delete_aspell_config(config);

	if (aspell_error_number(ret) != 0) {
		gtkaspellcheckers->error_message
			= g_strdup(aspell_error_message(ret));
		
		delete_aspell_can_have_error(ret);
		
		return NULL;
	}

	gtkaspeller->checker = to_aspell_speller(ret);
	gtkaspeller->config  = aspell_speller_config(gtkaspeller->checker);

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_delete(GtkAspeller *gtkaspeller)
{
	g_return_val_if_fail(gtkaspellcheckers, NULL);
	
	gtkaspellcheckers->checkers = 
		g_slist_remove(gtkaspellcheckers->checkers, 
				gtkaspeller);

	debug_print("Aspell: Deleting gtkaspeller %0x.\n", 
			(gint) gtkaspeller);

	gtkaspeller_real_delete(gtkaspeller);

	debug_print("Aspell: number of existing checkers %d\n", 
			g_slist_length(gtkaspellcheckers->checkers));

	return gtkaspeller;
}

static GtkAspeller *gtkaspeller_real_delete(GtkAspeller *gtkaspeller)
{
	g_return_val_if_fail(gtkaspeller,          NULL);
	g_return_val_if_fail(gtkaspeller->checker, NULL);

	aspell_speller_save_all_word_lists(gtkaspeller->checker);

	delete_aspell_speller(gtkaspeller->checker);

	dictionary_delete(gtkaspeller->dictionary);

	debug_print("Aspell: gtkaspeller %0x deleted.\n", 
		    (gint) gtkaspeller);

	g_free(gtkaspeller);

	return NULL;
}

/*****************************************************************************/
/* Checker configuration */

static gboolean set_dictionary(AspellConfig *config, Dictionary *dict)
{
	gchar *language = NULL;
	gchar *jargon = NULL;
	gchar *size   = NULL;
	gchar  buf[BUFSIZE];
	
	g_return_val_if_fail(config, FALSE);
	g_return_val_if_fail(dict,   FALSE);

	strncpy(buf, dict->fullname, BUFSIZE-1);
	buf[BUFSIZE-1] = 0x00;

	buf[dict->dictname - dict->fullname] = 0x00;

	CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("dict-dir", buf);
	debug_print("Aspell: looking for dictionaries in path %s.\n", buf);

	strncpy(buf, dict->dictname, BUFSIZE-1);
	language = buf;
	
	if ((size = strrchr(buf, '-')) && isdigit((int) size[1]))
		*size++ = 0x00;
	else
		size = NULL;
				
	if ((jargon = strchr(language, '-')) != NULL) 
		*jargon++ = 0x00;
	
	if (size != NULL && jargon == size)
		jargon = NULL;

	debug_print("Aspell: language: %s, jargon: %s, size: %s\n",
		    language, jargon ? jargon : "",
                    size ? size : "");
	
	if (language)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("lang", language);
	if (jargon)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("jargon", jargon);
	if (size)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("size", size);
	if (dict->encoding) {
		gchar *aspell_enc;
	
		aspell_enc = convert_to_aspell_encoding (dict->encoding);
		aspell_config_replace(config, "encoding",
				      (const char *) aspell_enc);
		g_free(aspell_enc);

		RETURN_FALSE_IF_CONFIG_ERROR();
	}
	
	return TRUE;
}

guchar *gtkaspell_get_dict(GtkAspell *gtkaspell)
{

	g_return_val_if_fail(gtkaspell->gtkaspeller->config,     NULL);
	g_return_val_if_fail(gtkaspell->gtkaspeller->dictionary, NULL);
 	
	return g_strdup(gtkaspell->gtkaspeller->dictionary->dictname);
}
  
guchar *gtkaspell_get_path(GtkAspell *gtkaspell)
{
	guchar *path;
	Dictionary *dict;

	g_return_val_if_fail(gtkaspell->gtkaspeller->config, NULL);
	g_return_val_if_fail(gtkaspell->gtkaspeller->dictionary, NULL);

	dict = gtkaspell->gtkaspeller->dictionary;
	path = g_strndup(dict->fullname, dict->dictname - dict->fullname);

	return path;
}

/* set_sug_mode_cb() - Menu callback: Set the suggestion mode */
static void set_sug_mode_cb(GtkMenuItem *w, GtkAspell *gtkaspell)
{
	char *themode;
	
	themode = (char *) gtk_label_get_text(GTK_LABEL(GTK_BIN(w)->child));
	themode = g_strdup(themode);
	
	set_real_sug_mode(gtkaspell, themode);
	g_free(themode);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static void set_real_sug_mode(GtkAspell *gtkaspell, const char *themode)
{
	gint result;
	gint mode = ASPELL_FASTMODE;
	g_return_if_fail(gtkaspell);
	g_return_if_fail(gtkaspell->gtkaspeller);
	g_return_if_fail(themode);

	if (!strcmp(themode,_("Normal Mode")))
		mode = ASPELL_NORMALMODE;
	else if (!strcmp( themode,_("Bad Spellers Mode")))
		mode = ASPELL_BADSPELLERMODE;

	result = gtkaspell_set_sug_mode(gtkaspell, mode);

	if(!result) {
		debug_print("Aspell: error while changing suggestion mode:%s\n",
			    gtkaspellcheckers->error_message);
		gtkaspell_checkers_reset_error();
	}
}
  
/* gtkaspell_set_sug_mode() - Set the suggestion mode */
gboolean gtkaspell_set_sug_mode(GtkAspell *gtkaspell, gint themode)
{
	AspellConfig *config;

	g_return_val_if_fail(gtkaspell, FALSE);
	g_return_val_if_fail(gtkaspell->gtkaspeller, FALSE);
	g_return_val_if_fail(gtkaspell->gtkaspeller->config, FALSE);

	debug_print("Aspell: setting sug mode of gtkaspeller %0x to %d\n",
			(guint) gtkaspell->gtkaspeller, themode);

	config = gtkaspell->gtkaspeller->config;

	switch (themode) {
		case ASPELL_FASTMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "fast");
			break;
		case ASPELL_NORMALMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "normal");
			break;
		case ASPELL_BADSPELLERMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", 
							    "bad-spellers");
			break;
		default: 
			gtkaspellcheckers->error_message = 
				g_strdup(_("Unknown suggestion mode."));
			return FALSE;
		}

	gtkaspell->gtkaspeller->sug_mode = themode;
	gtkaspell->default_sug_mode      = themode;

	return TRUE;
}

/* misspelled_suggest() - Create a suggestion list for  word  */
static GList *misspelled_suggest(GtkAspell *gtkaspell, guchar *word) 
{
	const guchar          *newword;
	GList                 *list = NULL;
	const AspellWordList  *suggestions;
	AspellStringEnumeration *elements;

	g_return_val_if_fail(word, NULL);

	if (!aspell_speller_check(gtkaspell->gtkaspeller->checker, word, -1)) {
		free_suggestions_list(gtkaspell);

		suggestions = aspell_speller_suggest(
				gtkaspell->gtkaspeller->checker,
						     (const char *)word, -1);
		elements    = aspell_word_list_elements(suggestions);
		list        = g_list_append(list, g_strdup(word)); 
		
		while ((newword = aspell_string_enumeration_next(elements)) != NULL)
			list = g_list_append(list, g_strdup(newword));

		gtkaspell->max_sug          = g_list_length(list) - 1;
		gtkaspell->suggestions_list = list;

		return list;
	}

	free_suggestions_list(gtkaspell);

	return NULL;
}

/* misspelled_test() - Just test if word is correctly spelled */  
static int misspelled_test(GtkAspell *gtkaspell, unsigned char *word) 
{
	return aspell_speller_check(gtkaspell->gtkaspeller->checker, word, -1)
				    ? 0 : 1;
}


static gboolean iswordsep(unsigned char c) 
{
	return (isspace(c) || ispunct(c)) && c != '\'';
}

static guchar get_text_index_whar(GtkAspell *gtkaspell, int pos) 
{
	GtkTextView *view = gtkaspell->gtktext;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
	GtkTextIter start, end;
	const gchar *utf8chars;
	guchar a = '\0';

	gtk_text_buffer_get_iter_at_offset(buffer, &start, pos);
	gtk_text_buffer_get_iter_at_offset(buffer, &end, pos+1);

	utf8chars = gtk_text_iter_get_text(&start, &end);
	if (is_ascii_str(utf8chars)) {
		a = utf8chars ? utf8chars[0] : '\0' ;
	} else {
		gchar *tr = conv_iconv_strdup(utf8chars, CS_UTF_8, 
				gtkaspell->gtkaspeller->dictionary->encoding);
		if (tr) {
			a = tr[0];
			g_free(tr);
		}
	}

	return a;
}

/* get_word_from_pos () - return the word pointed to. */
/* Handles correctly the quotes. */
static gboolean get_word_from_pos(GtkAspell *gtkaspell, gint pos, 
                                  unsigned char* buf, gint buflen,
                                  gint *pstart, gint *pend) 
{

	/* TODO : when correcting a word into quotes, change the color of */
	/* the quotes too, as may be they were highlighted before. To do  */
	/* this, we can use two others pointers that points to the whole    */
	/* word including quotes. */

	gint start;
	gint end;
		  
	guchar c;
	GtkTextView *gtktext;
	
	gtktext = gtkaspell->gtktext;
	if (iswordsep(get_text_index_whar(gtkaspell, pos))) 
		return FALSE;
	
	/* The apostrophe character is somtimes used for quotes 
	 * So include it in the word only if it is not surrounded 
	 * by other characters. 
	 */
	 
	for (start = pos; start >= 0; --start) {
		c = get_text_index_whar(gtkaspell, start);
		if (c == '\'') {
			if (start > 0) {
				if (isspace(get_text_index_whar(gtkaspell,
								 start - 1))
				||  ispunct(get_text_index_whar(gtkaspell,
								 start - 1))
				||  isdigit(get_text_index_whar(gtkaspell,
								 start - 1))) {
					/* start_quote = TRUE; */
					break;
				}
			}
			else {
				/* start_quote = TRUE; */
				break;
			}
		}
		else if (isspace(c) || ispunct(c) || isdigit(c))
				break;
	}

	start++;

	for (end = pos; end < get_textview_buffer_charcount(gtktext); end++) {
		c = get_text_index_whar(gtkaspell, end); 
		if (c == '\'') {
			if (end < get_textview_buffer_charcount(gtktext)) {
				if (isspace(get_text_index_whar(gtkaspell,
								 end + 1))
				||  ispunct(get_text_index_whar(gtkaspell,
								 end + 1))
				||  isdigit(get_text_index_whar(gtkaspell,
								 end + 1))) {
					/* end_quote = TRUE; */
					break;
				}
			}
			else {
				/* end_quote = TRUE; */
				break;
			}
		}
		else if (isspace(c) || ispunct(c) || isdigit(c))
				break;
	}
						
	if (pstart) 
		*pstart = start;
	if (pend) 
		*pend = end;

	if (buf) {
		if (end - start < buflen) {
			GtkTextIter iterstart, iterend;
			gchar *tmp, *conv;
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(gtktext);
			gtk_text_buffer_get_iter_at_offset(buffer, &iterstart, start);
			gtk_text_buffer_get_iter_at_offset(buffer, &iterend, end);
			tmp = gtk_text_buffer_get_text(buffer, &iterstart, &iterend, FALSE);
			conv = conv_iconv_strdup(tmp, CS_UTF_8, 
				gtkaspell->gtkaspeller->dictionary->encoding);
			g_free(tmp);
			strncpy(buf, conv, buflen-1);
			buf[buflen]='\0';
			g_free(conv);
		} else
			return FALSE;
	}

	return TRUE;
}

static gboolean check_at(GtkAspell *gtkaspell, gint from_pos) 
{
	gint	      start, end;
	unsigned char buf[GTKASPELLWORDSIZE];
	GtkTextView     *gtktext;

	g_return_val_if_fail(from_pos >= 0, FALSE);
    
	gtktext = gtkaspell->gtktext;

	if (!get_word_from_pos(gtkaspell, from_pos, buf, sizeof(buf), 
			       &start, &end))
		return FALSE;
	if (misspelled_test(gtkaspell, buf)) {
		strncpy(gtkaspell->theword, buf, GTKASPELLWORDSIZE - 1);
		gtkaspell->theword[GTKASPELLWORDSIZE - 1] = 0;
		gtkaspell->start_pos  = start;
		gtkaspell->end_pos    = end;
		free_suggestions_list(gtkaspell);

		change_color(gtkaspell, start, end, buf, &(gtkaspell->highlight));
		return TRUE;
	} else {
		change_color(gtkaspell, start, end, buf, NULL);
		return FALSE;
	}
}

static gboolean check_next_prev(GtkAspell *gtkaspell, gboolean forward)
{
	gint pos;
	gint minpos;
	gint maxpos;
	gint direc = -1;
	gboolean misspelled;
	
	minpos = 0;
	maxpos = gtkaspell->end_check_pos;

	if (forward) {
		minpos = -1;
		direc = 1;
		maxpos--;
	} 

	pos = get_textview_buffer_offset(gtkaspell->gtktext);
	gtkaspell->orig_pos = pos;
	while (iswordsep(get_text_index_whar(gtkaspell, pos)) &&
	       pos > minpos && pos <= maxpos)
		pos += direc;
	while (!(misspelled = check_at(gtkaspell, pos)) &&
	       pos > minpos && pos <= maxpos) {

		while (!iswordsep(get_text_index_whar(gtkaspell, pos)) &&
		       pos > minpos && pos <= maxpos)
			pos += direc;

		while (iswordsep(get_text_index_whar(gtkaspell, pos)) &&
		       pos > minpos && pos <= maxpos)
			pos += direc;
	}
	if (misspelled) {
		GtkMenu *menu = NULL;
		misspelled_suggest(gtkaspell, gtkaspell->theword);

		if (forward)
			gtkaspell->orig_pos = gtkaspell->end_pos;

		set_textview_buffer_offset(gtkaspell->gtktext,
				gtkaspell->end_pos);
		/* scroll line to window center */
		gtk_text_view_scroll_to_mark(gtkaspell->gtktext,
			gtk_text_buffer_get_insert(
				gtk_text_view_get_buffer(gtkaspell->gtktext)),
			0.0, TRUE, 0.0,	0.5);
		/* let textview recalculate coordinates (set_menu_pos) */
		while (gtk_events_pending ())
			gtk_main_iteration ();

		menu = make_sug_menu(gtkaspell);
		gtk_menu_popup(menu, NULL, NULL,
				set_menu_pos, gtkaspell, 0, GDK_CURRENT_TIME);
		g_signal_connect(G_OBJECT(menu), "deactivate",
					 G_CALLBACK(destroy_menu), 
					 gtkaspell);

	} else {
		reset_theword_data(gtkaspell);

		alertpanel_notice(_("No misspelled word found."));
		set_textview_buffer_offset(gtkaspell->gtktext,
					  gtkaspell->orig_pos);
	}

	return misspelled;
}

void gtkaspell_check_backwards(GtkAspell *gtkaspell)
{
	gtkaspell->continue_check = NULL;
	gtkaspell->end_check_pos =
		get_textview_buffer_charcount(gtkaspell->gtktext);
	check_next_prev(gtkaspell, FALSE);
}

void gtkaspell_check_forwards_go(GtkAspell *gtkaspell)
{

	gtkaspell->continue_check = NULL;
	gtkaspell->end_check_pos =
		get_textview_buffer_charcount(gtkaspell->gtktext);
	check_next_prev(gtkaspell, TRUE);
}

void gtkaspell_check_all(GtkAspell *gtkaspell)
{	
	GtkTextView *gtktext;
	gint start, end;
	GtkTextBuffer *buffer;
	GtkTextIter startiter, enditer;

	g_return_if_fail(gtkaspell);
	g_return_if_fail(gtkaspell->gtktext);

	gtktext = gtkaspell->gtktext;
	buffer = gtk_text_view_get_buffer(gtktext);
	gtk_text_buffer_get_selection_bounds(buffer, &startiter, &enditer);
	start = gtk_text_iter_get_offset(&startiter);
	end = gtk_text_iter_get_offset(&enditer);

	if (start == end) {
		start = 0;
		end = gtk_text_buffer_get_char_count(buffer);
	} else if (start > end) {
		gint tmp;

		tmp   = start;
		start = end;
		end   = tmp;
	}

	set_textview_buffer_offset(gtktext, start);

	gtkaspell->continue_check = continue_check;
	gtkaspell->end_check_pos  = end;

	gtkaspell->misspelled = check_next_prev(gtkaspell, TRUE);
}	

static void continue_check(gpointer *data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	gint pos = get_textview_buffer_offset(gtkaspell->gtktext);
	if (pos < gtkaspell->end_check_pos && gtkaspell->misspelled)
		gtkaspell->misspelled = check_next_prev(gtkaspell, TRUE);
	else
		gtkaspell->continue_check = NULL;
}

void gtkaspell_highlight_all(GtkAspell *gtkaspell) 
{
	guint     origpos;
	guint     pos = 0;
	guint     len;
	GtkTextView *gtktext;

	g_return_if_fail(gtkaspell->gtkaspeller->checker);	

	gtktext = gtkaspell->gtktext;

	len = get_textview_buffer_charcount(gtktext);

	origpos = get_textview_buffer_offset(gtktext);

	while (pos < len) {
		while (pos < len &&
		       iswordsep(get_text_index_whar(gtkaspell, pos)))
			pos++;
		while (pos < len &&
		       !iswordsep(get_text_index_whar(gtkaspell, pos)))
			pos++;
		if (pos > 0)
			check_at(gtkaspell, pos - 1);
	}
	set_textview_buffer_offset(gtktext, origpos);
}

static void replace_with_supplied_word_cb(GtkWidget *w, GtkAspell *gtkaspell) 
{
	unsigned char *newword;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();

	newword = gtk_editable_get_chars(GTK_EDITABLE(gtkaspell->replace_entry),
					 0, -1);

	if (strcmp(newword, gtkaspell->theword)) {
		replace_real_word(gtkaspell, newword);

		if ((e->type == GDK_KEY_PRESS &&
		    ((GdkEventKey *) e)->state & GDK_CONTROL_MASK)) {
			aspell_speller_store_replacement(
					gtkaspell->gtkaspeller->checker,
					 gtkaspell->theword, -1,
					 newword, -1);
		}
		gtkaspell->replace_entry = NULL;
	}

	g_free(newword);

	if (w && GTK_IS_DIALOG(w)) {
		gtk_widget_destroy(w);
	}

	set_point_continue(gtkaspell);
}


static void replace_word_cb(GtkWidget *w, gpointer data)
{
	unsigned char *newword;
	GtkAspell *gtkaspell = (GtkAspell *) data;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();

	newword = (unsigned char *) gtk_label_get_text(GTK_LABEL(GTK_BIN(w)->child));
	newword = g_strdup(newword);

	replace_real_word(gtkaspell, newword);

	if ((e->type == GDK_KEY_PRESS && 
	    ((GdkEventKey *) e)->state & GDK_CONTROL_MASK) ||
	    (e->type == GDK_BUTTON_RELEASE && 
	     ((GdkEventButton *) e)->state & GDK_CONTROL_MASK)) {
		aspell_speller_store_replacement(
				gtkaspell->gtkaspeller->checker,
						 gtkaspell->theword, -1, 
						 newword, -1);
	}

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));

	set_point_continue(gtkaspell);
	g_free(newword);
}

static void replace_real_word(GtkAspell *gtkaspell, gchar *newword)
{
	int		oldlen, newlen, wordlen;
	gint		origpos;
	gint		pos;
	GtkTextView	*gtktext;
	GtkTextBuffer	*textbuf;
	GtkTextIter	startiter, enditer;
    
	if (!newword) return;

	gtktext = gtkaspell->gtktext;
	textbuf = gtk_text_view_get_buffer(gtktext);

	origpos = gtkaspell->orig_pos;
	pos     = origpos;
	oldlen  = gtkaspell->end_pos - gtkaspell->start_pos;
	wordlen = strlen(gtkaspell->theword);

	newlen = strlen(newword); /* FIXME: multybyte characters? */

	g_signal_handlers_block_by_func(G_OBJECT(gtktext),
					 G_CALLBACK(entry_insert_cb),
					 gtkaspell);
	g_signal_handlers_block_by_func(G_OBJECT(gtktext),
					 G_CALLBACK(entry_delete_cb),
					 gtkaspell);

	gtk_text_buffer_get_iter_at_offset(textbuf, &startiter,
					   gtkaspell->start_pos);
	gtk_text_buffer_get_iter_at_offset(textbuf, &enditer,
					   gtkaspell->end_pos);
	g_signal_emit_by_name(G_OBJECT(textbuf), "delete-range",
			      &startiter, &enditer, gtkaspell);
	g_signal_emit_by_name(G_OBJECT(textbuf), "insert-text",
			      &startiter, newword, newlen, gtkaspell);

	g_signal_handlers_unblock_by_func(G_OBJECT(gtktext),
					   G_CALLBACK(entry_insert_cb),
					   gtkaspell);
	g_signal_handlers_unblock_by_func(G_OBJECT(gtktext),
					   G_CALLBACK(entry_delete_cb),
					   gtkaspell);

	/* Put the point and the position where we clicked with the mouse
	 * It seems to be a hack, as I must thaw,freeze,thaw the widget
	 * to let it update correctly the word insertion and then the
	 * point & position position. If not, SEGV after the first replacement
	 * If the new word ends before point, put the point at its end.
	 */

	if (origpos - gtkaspell->start_pos < oldlen &&
	    origpos - gtkaspell->start_pos >= 0) {
		/* Original point was in the word.
		 * Let it there unless point is going to be outside of the word
		 */
		if (origpos - gtkaspell->start_pos >= newlen) {
			pos = gtkaspell->start_pos + newlen;
		}
	}
	else if (origpos >= gtkaspell->end_pos) {
		/* move the position according to the change of length */
		pos = origpos + newlen - oldlen;
	}

	gtkaspell->end_pos = gtkaspell->start_pos + strlen(newword); /* FIXME: multibyte characters? */

	if (get_textview_buffer_charcount(gtktext) < pos)
		pos = get_textview_buffer_charcount(gtktext);
	gtkaspell->orig_pos = pos;

	set_textview_buffer_offset(gtktext, gtkaspell->orig_pos);
}

/* Accept this word for this session */
static void add_word_to_session_cb(GtkWidget *w, gpointer data)
{
	guint     pos;
	GtkTextView *gtktext;
   	GtkAspell *gtkaspell = (GtkAspell *) data; 
	gtktext = gtkaspell->gtktext;

	pos = get_textview_buffer_offset(gtktext);

	aspell_speller_add_to_session(gtkaspell->gtkaspeller->checker,
				      gtkaspell->theword,
				      strlen(gtkaspell->theword));

	check_at(gtkaspell, gtkaspell->start_pos);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	set_point_continue(gtkaspell);
}

/* add_word_to_personal_cb() - add word to personal dict. */
static void add_word_to_personal_cb(GtkWidget *w, gpointer data)
{
   	GtkAspell *gtkaspell = (GtkAspell *) data; 

	aspell_speller_add_to_personal(gtkaspell->gtkaspeller->checker,
				       gtkaspell->theword,
				       strlen(gtkaspell->theword));

	check_at(gtkaspell, gtkaspell->start_pos);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));
	set_point_continue(gtkaspell);
}

static void check_with_alternate_cb(GtkWidget *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	gint misspelled;

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	use_alternate_dict(gtkaspell);
	misspelled = check_at(gtkaspell, gtkaspell->start_pos);

	if (!gtkaspell->continue_check) {

		gtkaspell->misspelled = misspelled;

		if (gtkaspell->misspelled) {
			GtkMenu *menu;
			misspelled_suggest(gtkaspell, gtkaspell->theword);

			set_textview_buffer_offset(gtkaspell->gtktext,
					    gtkaspell->end_pos);

			menu = make_sug_menu(gtkaspell);
			gtk_menu_popup(menu, NULL, NULL,
				       set_menu_pos, gtkaspell, 0,
				       GDK_CURRENT_TIME);
			g_signal_connect(G_OBJECT(menu), "deactivate",
					 G_CALLBACK(destroy_menu), 
					 gtkaspell);
			return;
		}
	} else
		gtkaspell->orig_pos = gtkaspell->start_pos;

	set_point_continue(gtkaspell);
}
	
static gboolean replace_key_pressed(GtkWidget *widget,
				   GdkEventKey *event,
				   GtkAspell *gtkaspell)
{
	if (event && event->keyval == GDK_Escape) {
		gtk_widget_destroy(widget);
		return TRUE;
	} else if (event && event->keyval == GDK_Return) {
		replace_with_supplied_word_cb(widget, gtkaspell);
		return TRUE;
	}
	return FALSE;
}
	
static void replace_with_create_dialog_cb(GtkWidget *w, gpointer data)
{
	static PangoFontDescription *font_desc;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *w_hbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *entry;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	GtkWidget *confirm_area;
	GtkWidget *icon;
	gchar *thelabel;
	gint xx, yy;
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gdk_window_get_origin((GTK_WIDGET(w)->parent)->window, &xx, &yy);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	dialog = gtk_dialog_new();

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_title(GTK_WINDOW(dialog),_("Replace unknown word"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_widget_set_uposition(dialog, xx, yy);

	g_signal_connect_swapped(G_OBJECT(dialog), "destroy",
				 G_CALLBACK(gtk_widget_destroy), 
				 G_OBJECT(dialog));

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    FALSE, FALSE, 0);

	thelabel = g_strdup_printf(_("<span weight=\"bold\" "
					"size=\"larger\">Replace \"%s\" with: </span>"), 
				   gtkaspell->theword);
	/* for title label */
	w_hbox = gtk_hbox_new(FALSE, 0);
	
	icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION,
        				GTK_ICON_SIZE_DIALOG); 
	gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);
	
	label = gtk_label_new(thelabel);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(label->style->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_modify_font(label, font_desc);
	g_free(thelabel);
	
	entry = gtk_entry_new();
	gtkaspell->replace_entry = entry;
	gtk_entry_set_text(GTK_ENTRY(entry), gtkaspell->theword);
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	g_signal_connect(G_OBJECT(dialog),
			"key_press_event",
		       	G_CALLBACK(replace_key_pressed), gtkaspell);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

	label = gtk_label_new(_("Holding down Control key while pressing "
				"Enter\nwill learn from mistake.\n"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(TRUE, 0);

	gtkut_stock_button_set_create(&confirm_area,
				      &ok_button, GTK_STOCK_OK,
				      &cancel_button, GTK_STOCK_CANCEL,
				      NULL, NULL);

	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area),
			 confirm_area, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(confirm_area), 5);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(replace_with_supplied_word_cb), 
			 gtkaspell);
	g_signal_connect_swapped(G_OBJECT(ok_button), "clicked",
				   G_CALLBACK(gtk_widget_destroy), 
				   G_OBJECT(dialog));

	g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked",
				 G_CALLBACK(gtk_widget_destroy), 
				 G_OBJECT(dialog));

	gtk_widget_grab_focus(entry);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	gtk_widget_show_all(dialog);
}

void gtkaspell_uncheck_all(GtkAspell * gtkaspell) 
{
	GtkTextView *gtktext;
	GtkTextBuffer *buffer;
	GtkTextIter startiter, enditer;
	
	gtktext = gtkaspell->gtktext;

	buffer = gtk_text_view_get_buffer(gtktext);
	gtk_text_buffer_get_iter_at_offset(buffer, &startiter, 0);
	gtk_text_buffer_get_iter_at_offset(buffer, &enditer,
				   get_textview_buffer_charcount(gtktext)-1);
	gtk_text_buffer_remove_tag_by_name(buffer, "misspelled",
					   &startiter, &enditer);
}

static void toggle_check_while_typing_cb(GtkWidget *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gtkaspell->check_while_typing = gtkaspell->check_while_typing == FALSE;

	if (!gtkaspell->check_while_typing)
		gtkaspell_uncheck_all(gtkaspell);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static GSList *create_empty_dictionary_list(void)
{
	GSList *list = NULL;
	Dictionary *dict;

	dict = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(_("None"));
	dict->dictname = dict->fullname;
	dict->encoding = NULL;

	return g_slist_append(list, dict);
}

/* gtkaspell_get_dictionary_list() - returns list of dictionary names */
GSList *gtkaspell_get_dictionary_list(const gchar *aspell_path, gint refresh)
{
	GSList *list;
	Dictionary *dict;
	AspellConfig *config;
	AspellDictInfoList *dlist;
	AspellDictInfoEnumeration *dels;
	const AspellDictInfo *entry;

	if (!gtkaspellcheckers)
		gtkaspell_checkers_init();

	if (gtkaspellcheckers->dictionary_list && !refresh)
		return gtkaspellcheckers->dictionary_list;
	else
		gtkaspell_free_dictionary_list(
				gtkaspellcheckers->dictionary_list);
	list = NULL;

	config = new_aspell_config();

	aspell_config_replace(config, "dict-dir", aspell_path);
	if (aspell_config_error_number(config) != 0) {
		gtkaspellcheckers->error_message = g_strdup(
				aspell_config_error_message(config));
		gtkaspellcheckers->dictionary_list =
			create_empty_dictionary_list();

		return gtkaspellcheckers->dictionary_list; 
	}

	dlist = get_aspell_dict_info_list(config);
	delete_aspell_config(config);

	debug_print("Aspell: checking for dictionaries in %s\n", aspell_path);
	dels = aspell_dict_info_list_elements(dlist);
	while ( (entry = aspell_dict_info_enumeration_next(dels)) != 0) 
	{
		dict = g_new0(Dictionary, 1);
		dict->fullname = g_strdup_printf("%s%s", aspell_path, 
				entry->name);
		dict->dictname = dict->fullname + strlen(aspell_path);
		dict->encoding = g_strdup(entry->code);
		
		if (g_slist_find_custom(list, dict, 
				(GCompareFunc) compare_dict) != NULL) {
			dictionary_delete(dict);
			continue;	
		}
		
		debug_print("Aspell: found dictionary %s %s %s\n", dict->fullname,
				dict->dictname, dict->encoding);
		list = g_slist_insert_sorted(list, dict,
				(GCompareFunc) compare_dict);
	}

	delete_aspell_dict_info_enumeration(dels);
	
        if(list==NULL){
		
		debug_print("Aspell: error when searching for dictionaries: "
			      "No dictionary found.\n");
		list = create_empty_dictionary_list();
	}

	gtkaspellcheckers->dictionary_list = list;

	return list;
}

void gtkaspell_free_dictionary_list(GSList *list)
{
	Dictionary *dict;
	GSList *walk;
	for (walk = list; walk != NULL; walk = g_slist_next(walk))
		if (walk->data) {
			dict = (Dictionary *) walk->data;
			dictionary_delete(dict);
		}				
	g_slist_free(list);
}

GtkWidget *gtkaspell_dictionary_option_menu_new(const gchar *aspell_path)
{
	GSList *dict_list, *tmp;
	GtkWidget *item;
	GtkWidget *menu;
	Dictionary *dict;

	dict_list = gtkaspell_get_dictionary_list(aspell_path, TRUE);
	g_return_val_if_fail(dict_list, NULL);

	menu = gtk_menu_new();
	
	for (tmp = dict_list; tmp != NULL; tmp = g_slist_next(tmp)) {
		dict = (Dictionary *) tmp->data;
		item = gtk_menu_item_new_with_label(dict->dictname);
		g_object_set_data(G_OBJECT(item), "dict_name",
				  dict->fullname); 
					 
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	return menu;
}

gchar *gtkaspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	gchar *dict_fullname;
	gchar *label;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);

	menuitem = gtk_menu_get_active(GTK_MENU(menu));
        dict_fullname = (gchar *) g_object_get_data(G_OBJECT(menuitem), 
						    "dict_name");
        g_return_val_if_fail(dict_fullname, NULL);

	label = g_strdup(dict_fullname);

        return label;
  
}

gint gtkaspell_set_dictionary_menu_active_item(GtkWidget *menu,
					       const gchar *dictionary)
{
	GList *cur;
	gint n;

	g_return_val_if_fail(menu != NULL, 0);
	g_return_val_if_fail(dictionary != NULL, 0);
	g_return_val_if_fail(GTK_IS_OPTION_MENU(menu), 0);

	n = 0;
	for (cur = GTK_MENU_SHELL(gtk_option_menu_get_menu(
					GTK_OPTION_MENU(menu)))->children;
	     cur != NULL; cur = cur->next) {
		GtkWidget *menuitem;
		gchar *dict_name;

		menuitem = GTK_WIDGET(cur->data);
		dict_name = g_object_get_data(G_OBJECT(menuitem), 
					      "dict_name");
		if ((dict_name != NULL) && !strcmp2(dict_name, dictionary)) {
			gtk_option_menu_set_history(GTK_OPTION_MENU(menu), n);

			return 1;
		}
		n++;
	}

	return 0;
}

GtkWidget *gtkaspell_sugmode_option_menu_new(gint sugmode)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	item = gtk_menu_item_new_with_label(_("Fast Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	g_object_set_data(G_OBJECT(item), "sugmode",
			  GINT_TO_POINTER(ASPELL_FASTMODE));

	item = gtk_menu_item_new_with_label(_("Normal Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	g_object_set_data(G_OBJECT(item), "sugmode",
			  GINT_TO_POINTER(ASPELL_NORMALMODE));
	
	item = gtk_menu_item_new_with_label(_("Bad Spellers Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	g_object_set_data(G_OBJECT(item), "sugmode",
			  GINT_TO_POINTER(ASPELL_BADSPELLERMODE));

	return menu;
}
	
void gtkaspell_sugmode_option_menu_set(GtkOptionMenu *optmenu, gint sugmode)
{
	g_return_if_fail(GTK_IS_OPTION_MENU(optmenu));

	g_return_if_fail(sugmode == ASPELL_FASTMODE ||
			 sugmode == ASPELL_NORMALMODE ||
			 sugmode == ASPELL_BADSPELLERMODE);

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), sugmode - 1);
}

gint gtkaspell_get_sugmode_from_option_menu(GtkOptionMenu *optmenu)
{
	gint sugmode;
	GtkWidget *item;
	
	g_return_val_if_fail(GTK_IS_OPTION_MENU(optmenu), -1);

	item = gtk_menu_get_active(GTK_MENU(gtk_option_menu_get_menu(optmenu)));
	
	sugmode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item),
						    "sugmode"));

	return sugmode;
}

static void use_alternate_dict(GtkAspell *gtkaspell)
{
	GtkAspeller *tmp;

	tmp = gtkaspell->gtkaspeller;
	gtkaspell->gtkaspeller = gtkaspell->alternate_speller;
	gtkaspell->alternate_speller = tmp;

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);
}

static void destroy_menu(GtkWidget *widget,
			     gpointer user_data) {

	GtkAspell *gtkaspell = (GtkAspell *)user_data;

	if (gtkaspell->accel_group) {
		gtk_window_remove_accel_group(GTK_WINDOW(gtkaspell->parent_window), 
				gtkaspell->accel_group);
		gtkaspell->accel_group = NULL;
	}
}

static void popup_menu(GtkAspell *gtkaspell, GdkEventButton *eb) 
{
	GtkTextView * gtktext;
	GtkMenu *menu = NULL;
	
	gtktext = gtkaspell->gtktext;

	gtkaspell->orig_pos = get_textview_buffer_offset(gtktext);

	if (!(eb->state & GDK_SHIFT_MASK)) {
		if (check_at(gtkaspell, gtkaspell->orig_pos)) {

			set_textview_buffer_offset(gtktext, gtkaspell->orig_pos);

			if (misspelled_suggest(gtkaspell, gtkaspell->theword)) {
				menu = make_sug_menu(gtkaspell);
				gtk_menu_popup(menu,
					       NULL, NULL, NULL, NULL,
					       eb->button, eb->time);

				g_signal_connect(G_OBJECT(menu), "deactivate",
							 G_CALLBACK(destroy_menu), 
							 gtkaspell);
				return;
			}
		} else
			set_textview_buffer_offset(gtktext, gtkaspell->orig_pos);
	}
	menu = make_config_menu(gtkaspell);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
		       eb->button, eb->time);

	g_signal_connect(G_OBJECT(menu), "deactivate",
				 G_CALLBACK(destroy_menu), 
				 gtkaspell);
}

static gboolean aspell_key_pressed(GtkWidget *widget,
				   GdkEventKey *event,
				   GtkAspell *gtkaspell)
{
	if (event && (isascii(event->keyval) || event->keyval == GDK_Return)) {
		gtk_accel_groups_activate(
				G_OBJECT(gtkaspell->parent_window),
				event->keyval, event->state);
	} else if (event && event->keyval == GDK_Escape) {
		destroy_menu(NULL, gtkaspell);
	}
	return FALSE;
}

/* make_sug_menu() - Add menus to accept this word for this session 
 * and to add it to personal dictionary 
 */
static GtkMenu *make_sug_menu(GtkAspell *gtkaspell) 
{
	GtkWidget 	*menu, *item;
	unsigned char	*caption;
	GtkTextView 	*gtktext;
	GtkAccelGroup 	*accel;
	GList 		*l = gtkaspell->suggestions_list;
	gchar		*utf8buf;

	gtktext = gtkaspell->gtktext;

	accel = gtk_accel_group_new();
	menu = gtk_menu_new(); 

	if (gtkaspell->sug_menu)
		gtk_widget_destroy(gtkaspell->sug_menu);

	if (gtkaspell->accel_group) {
		gtk_window_remove_accel_group(GTK_WINDOW(gtkaspell->parent_window), 
				gtkaspell->accel_group);
		gtkaspell->accel_group = NULL;
	}

	gtkaspell->sug_menu = menu;	

	g_signal_connect(G_OBJECT(menu), "cancel",
			 G_CALLBACK(cancel_menu_cb), gtkaspell);

	utf8buf  = conv_codeset_strdup((unsigned char*)l->data,
				conv_get_locale_charset_str(),
				CS_UTF_8);
	caption = g_strdup_printf(_("\"%s\" unknown in %s"), 
				  utf8buf, 
				  gtkaspell->gtkaspeller->dictionary->dictname);
	item = gtk_menu_item_new_with_label(caption);
	g_free(utf8buf);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_misc_set_alignment(GTK_MISC(GTK_BIN(item)->child), 0.5, 0.5);
	g_free(caption);

	item = gtk_menu_item_new();
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new_with_label(_("Accept in this session"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(add_word_to_session_cb), 
			 gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_space,
				   GDK_CONTROL_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

	item = gtk_menu_item_new_with_label(_("Add to personal dictionary"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(add_word_to_personal_cb), 
			 gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_Return,
				   GDK_CONTROL_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

        item = gtk_menu_item_new_with_label(_("Replace with..."));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(replace_with_create_dialog_cb), 
			 gtkaspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_R, 0,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_R, 
				   GDK_CONTROL_MASK,
				   GTK_ACCEL_LOCKED);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller) {
		caption = g_strdup_printf(_("Check with %s"), 
			gtkaspell->alternate_speller->dictionary->dictname);
		item = gtk_menu_item_new_with_label(caption);
		g_free(caption);
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(check_with_alternate_cb),
				 gtkaspell);
		gtk_widget_add_accelerator(item, "activate", accel, GDK_X, 0,
					   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);
		gtk_widget_add_accelerator(item, "activate", accel, GDK_X, 
					   GDK_CONTROL_MASK,
					   GTK_ACCEL_LOCKED);
	}

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	l = l->next;
        if (l == NULL) {
		item = gtk_menu_item_new_with_label(_("(no suggestions)"));
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
        } else {
		GtkWidget *curmenu = menu;
		gint count = 0;
		
		do {
			if (count == MENUCOUNT) {
				count -= MENUCOUNT;

				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_widget_show(item);
				gtk_menu_append(GTK_MENU(curmenu), item);

				curmenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
							  curmenu);
			}

			utf8buf  = conv_codeset_strdup((unsigned char*)l->data,
							conv_get_locale_charset_str(),
							CS_UTF_8);
			item = gtk_menu_item_new_with_label(utf8buf);
			g_free(utf8buf);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(replace_word_cb),
					 gtkaspell);

			if (curmenu == menu && count < MENUCOUNT) {
				gtk_widget_add_accelerator(item, "activate",
							   accel,
							   GDK_A + count, 0,
							   GTK_ACCEL_LOCKED | 
							   GTK_ACCEL_VISIBLE);
				gtk_widget_add_accelerator(item, "activate", 
							   accel,
							   GDK_A + count, 
							   GDK_CONTROL_MASK,
							   GTK_ACCEL_LOCKED);
				}

			count++;

		} while ((l = l->next) != NULL);
	}

	gtk_window_add_accel_group
		(GTK_WINDOW(gtkaspell->parent_window),
		 accel);
	gtkaspell->accel_group = accel;

	g_signal_connect(G_OBJECT(menu),
			"key_press_event",
		       	G_CALLBACK(aspell_key_pressed), gtkaspell);
	
	return GTK_MENU(menu);
}

static void populate_submenu(GtkAspell *gtkaspell, GtkWidget *menu)
{
	GtkWidget *item, *submenu;
	gchar *dictname;
	GtkAspeller *gtkaspeller = gtkaspell->gtkaspeller;

	if (GTK_MENU_SHELL(menu)->children) {
		GList *amenu, *alist;
		for (amenu = (GTK_MENU_SHELL(menu)->children); amenu; ) {
			alist = amenu->next;
			gtk_widget_destroy(GTK_WIDGET(amenu->data));
			amenu = alist;
		}
	}
	
	dictname = g_strdup_printf(_("Dictionary: %s"),
				   gtkaspeller->dictionary->dictname);
	item = gtk_menu_item_new_with_label(dictname);
	gtk_misc_set_alignment(GTK_MISC(GTK_BIN(item)->child), 0.5, 0.5);
	g_free(dictname);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
		
	if (gtkaspell->use_alternate && gtkaspell->alternate_speller) {
		dictname = g_strdup_printf(_("Use alternate (%s)"), 
				gtkaspell->alternate_speller->dictionary->dictname);
		item = gtk_menu_item_new_with_label(dictname);
		g_free(dictname);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(switch_to_alternate_cb),
				 gtkaspell);
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu), item);
	}

      	item = gtk_check_menu_item_new_with_label(_("Fast Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_FASTMODE) {
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(item),FALSE);
	} else
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(set_sug_mode_cb),
				 gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_check_menu_item_new_with_label(_("Normal Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_NORMALMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	} else
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(set_sug_mode_cb),
				 gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu),item);

	item = gtk_check_menu_item_new_with_label(_("Bad Spellers Mode"));
	if (gtkaspell->gtkaspeller->sug_mode == ASPELL_BADSPELLERMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	} else
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(set_sug_mode_cb),
				 gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_check_menu_item_new_with_label(_("Check while typing"));
	if (gtkaspell->check_while_typing)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	else	
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(toggle_check_while_typing_cb),
			 gtkaspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	submenu = gtk_menu_new();
        item = gtk_menu_item_new_with_label(_("Change dictionary"));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	/* Dict list */
        if (gtkaspellcheckers->dictionary_list == NULL)
		gtkaspell_get_dictionary_list(gtkaspell->dictionary_path, FALSE);
        {
		GtkWidget * curmenu = submenu;
		int count = 0;
		Dictionary *dict;
		GSList *tmp;
		tmp = gtkaspellcheckers->dictionary_list;
		
		for (tmp = gtkaspellcheckers->dictionary_list; tmp != NULL; 
				tmp = g_slist_next(tmp)) {
			if (count == MENUCOUNT) {
				GtkWidget *newmenu;
				
				newmenu = gtk_menu_new();
				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), 
							  newmenu);
				
				gtk_menu_append(GTK_MENU(curmenu), item);
				gtk_widget_show(item);
				curmenu = newmenu;
				count = 0;
			}
			dict = (Dictionary *) tmp->data;
			item = gtk_check_menu_item_new_with_label(dict->dictname);
			g_object_set_data(G_OBJECT(item), "dict_name",
					  dict->fullname); 
			if (strcmp2(dict->fullname,
			    gtkaspell->gtkaspeller->dictionary->fullname))
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
			else {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(item),
							 FALSE);
			}
			g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(change_dict_cb),
					 gtkaspell);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			
			count++;
		}
        }  
}

static GtkMenu *make_config_menu(GtkAspell *gtkaspell)
{
	if (!gtkaspell->popup_config_menu)
		gtkaspell->popup_config_menu = gtk_menu_new();

	debug_print("Aspell: creating/using popup_config_menu %0x\n", 
			(guint) gtkaspell->popup_config_menu);
	populate_submenu(gtkaspell, gtkaspell->popup_config_menu);

        return GTK_MENU(gtkaspell->popup_config_menu);
}

void gtkaspell_populate_submenu(GtkAspell *gtkaspell, GtkWidget *menuitem)
{
	GtkWidget *menu;

	menu = GTK_WIDGET(GTK_MENU_ITEM(menuitem)->submenu);
	
	debug_print("Aspell: using config menu %0x\n", 
			(guint) gtkaspell->popup_config_menu);
	populate_submenu(gtkaspell, menu);
	
	gtkaspell->config_menu = menu;
	
}

static void set_menu_pos(GtkMenu *menu, gint *x, gint *y, 
			 gboolean *push_in, gpointer data)
{
	GtkAspell 	*gtkaspell = (GtkAspell *) data;
	gint 		 xx = 0, yy = 0;
	gint 		 sx,     sy;
	gint 		 wx,     wy;
	GtkTextView 	*text = GTK_TEXT_VIEW(gtkaspell->gtktext);
	GtkTextBuffer	*textbuf;
	GtkTextIter	 iter;
	GdkRectangle	 rect;
	GtkRequisition 	 r;

	textbuf = gtk_text_view_get_buffer(gtkaspell->gtktext);
	gtk_text_buffer_get_iter_at_mark(textbuf, &iter,
					 gtk_text_buffer_get_insert(textbuf));
	gtk_text_view_get_iter_location(gtkaspell->gtktext, &iter, &rect);
	gtk_text_view_buffer_to_window_coords(text, GTK_TEXT_WINDOW_TEXT,
					      rect.x, rect.y, 
					      &rect.x, &rect.y);

	gdk_window_get_origin(GTK_WIDGET(gtkaspell->gtktext)->window, &xx, &yy);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &r);

	wx =  r.width;
	wy =  r.height;

	*x = rect.x + xx +
	     gdk_char_width(gtk_style_get_font(GTK_WIDGET(text)->style), ' ');

	*y = rect.y + rect.height + yy;

	if (*x + wx > sx)
		*x = sx - wx;
	if (*y + wy > sy)
		*y = *y - wy -
		     gdk_string_height(gtk_style_get_font(
						GTK_WIDGET(text)->style),
				       gtkaspell->theword);
}

/* Menu call backs */

static gboolean cancel_menu_cb(GtkMenuShell *w, gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;

	gtkaspell->continue_check = NULL;
	set_point_continue(gtkaspell);

	return FALSE;
}

gboolean gtkaspell_change_dict(GtkAspell *gtkaspell, const gchar *dictionary)
{
	Dictionary 	*dict;       
	GtkAspeller 	*gtkaspeller;
	gint 		 sug_mode;

	g_return_val_if_fail(gtkaspell, FALSE);
	g_return_val_if_fail(dictionary, FALSE);
  
	sug_mode  = gtkaspell->default_sug_mode;

	dict = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(dictionary);
	dict->encoding = g_strdup(gtkaspell->gtkaspeller->dictionary->encoding);

	if (gtkaspell->use_alternate && gtkaspell->alternate_speller &&
	    dict == gtkaspell->alternate_speller->dictionary) {
		use_alternate_dict(gtkaspell);
		dictionary_delete(dict);
		gtkaspell->alternate_speller->dictionary = NULL;
		return TRUE;
	}
	
	gtkaspeller = gtkaspeller_new(dict);

	if (!gtkaspeller) {
		gchar *message;
		message = g_strdup_printf(_("The spell checker could not change dictionary.\n%s"), 
					  gtkaspellcheckers->error_message);

		alertpanel_warning(message); 
		g_free(message);
	} else {
		if (gtkaspell->use_alternate) {
			if (gtkaspell->alternate_speller)
				gtkaspeller_delete(gtkaspell->alternate_speller);
			gtkaspell->alternate_speller = gtkaspell->gtkaspeller;
		} else
			gtkaspeller_delete(gtkaspell->gtkaspeller);

		gtkaspell->gtkaspeller = gtkaspeller;
		gtkaspell_set_sug_mode(gtkaspell, sug_mode);
	}
	
	dictionary_delete(dict);

	if (gtkaspell->config_menu)
		populate_submenu(gtkaspell, gtkaspell->config_menu);

	return TRUE;	
}

/* change_dict_cb() - Menu callback : change dict */
static void change_dict_cb(GtkWidget *w, GtkAspell *gtkaspell)
{
	gchar		*fullname;
  
        fullname = (gchar *) g_object_get_data(G_OBJECT(w), "dict_name");
	
	if (!strcmp2(fullname, _("None")))
		return;

	gtkaspell_change_dict(gtkaspell, fullname);
}

static void switch_to_alternate_cb(GtkWidget *w,
				   gpointer data)
{
	GtkAspell *gtkaspell = (GtkAspell *) data;
	use_alternate_dict(gtkaspell);
}

/* Misc. helper functions */

static void set_point_continue(GtkAspell *gtkaspell)
{
	GtkTextView  *gtktext;

	gtktext = gtkaspell->gtktext;

	set_textview_buffer_offset(gtktext, gtkaspell->orig_pos);

	if (gtkaspell->continue_check)
		gtkaspell->continue_check((gpointer *) gtkaspell);
}

static void allocate_color(GtkAspell *gtkaspell, gint rgbvalue)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(gtkaspell->gtktext);
	GdkColor *color = &(gtkaspell->highlight);

	/* Shameless copy from Sylpheed's gtkutils.c */
	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0)
			* 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0)
			* 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0)
			* 65535.0);

	if (rgbvalue != 0)
		gtk_text_buffer_create_tag(buffer, "misspelled",
				   "foreground-gdk", color, NULL);
	else
		gtk_text_buffer_create_tag(buffer, "misspelled",
				   "underline", PANGO_UNDERLINE_ERROR, NULL);

}

static void change_color(GtkAspell * gtkaspell, 
			 gint start, gint end,
			 gchar *newtext,
                         GdkColor *color) 
{
	GtkTextView *gtktext;
	GtkTextBuffer *buffer;
	GtkTextIter startiter, enditer;

	if (start > end)
		return;
    
	gtktext = gtkaspell->gtktext;
    
	buffer = gtk_text_view_get_buffer(gtktext);
	gtk_text_buffer_get_iter_at_offset(buffer, &startiter, start);
	gtk_text_buffer_get_iter_at_offset(buffer, &enditer, end);
	if (color)
		gtk_text_buffer_apply_tag_by_name(buffer, "misspelled",
						  &startiter, &enditer);
	else
		gtk_text_buffer_remove_tag_by_name(buffer, "misspelled",
						   &startiter, &enditer);
}

/* convert_to_aspell_encoding () - converts ISO-8859-* strings to iso8859-* 
 * as needed by aspell. Returns an allocated string.
 */

static guchar *convert_to_aspell_encoding (const guchar *encoding)
{
	guchar * aspell_encoding;

	if (strstr2(encoding, "ISO-8859-")) {
		aspell_encoding = g_strdup_printf("iso8859%s", encoding+8);
	}
	else {
		if (!strcmp2(encoding, "US-ASCII"))
			aspell_encoding = g_strdup("iso8859-1");
		else
			aspell_encoding = g_strdup(encoding);
	}

	return aspell_encoding;
}

/* compare_dict () - compare 2 dict names */
static gint compare_dict(Dictionary *a, Dictionary *b)
{
	guint   aparts = 0,  bparts = 0;
	guint  	i;

	for (i=0; i < strlen(a->dictname); i++)
		if (a->dictname[i] == '-')
			aparts++;
	for (i=0; i < strlen(b->dictname); i++)
		if (b->dictname[i] == '-')
			bparts++;

	if (aparts != bparts) 
		return (aparts < bparts) ? -1 : +1;
	else {
		gint compare;
		compare = strcmp2(a->dictname, b->dictname);
		if (!compare)
			compare = strcmp2(a->fullname, b->fullname);
		return compare;
	}
}

static void dictionary_delete(Dictionary *dict)
{
	g_free(dict->fullname);
	g_free(dict->encoding);
	g_free(dict);
}

static Dictionary *dictionary_dup(const Dictionary *dict)
{
	Dictionary *dict2;

	dict2 = g_new(Dictionary, 1); 

	dict2->fullname = g_strdup(dict->fullname);
	dict2->dictname = dict->dictname - dict->fullname + dict2->fullname;
	dict2->encoding = g_strdup(dict->encoding);

	return dict2;
}

static void free_suggestions_list(GtkAspell *gtkaspell)
{
	GList *list;

	for (list = gtkaspell->suggestions_list; list != NULL;
	     list = list->next)
		g_free(list->data);

	g_list_free(gtkaspell->suggestions_list);
	
	gtkaspell->max_sug          = -1;
	gtkaspell->suggestions_list = NULL;
}

static void reset_theword_data(GtkAspell *gtkaspell)
{
	gtkaspell->start_pos     =  0;
	gtkaspell->end_pos       =  0;
	gtkaspell->theword[0]    =  0;
	gtkaspell->max_sug       = -1;

	free_suggestions_list(gtkaspell);
}

static void free_checkers(gpointer elt, gpointer data)
{
	GtkAspeller *gtkaspeller = elt;

	g_return_if_fail(gtkaspeller);

	gtkaspeller_real_delete(gtkaspeller);
}

static gint find_gtkaspeller(gconstpointer aa, gconstpointer bb)
{
	Dictionary *a = ((GtkAspeller *) aa)->dictionary;
	Dictionary *b = ((GtkAspeller *) bb)->dictionary;

	if (a && b && a->fullname && b->fullname  &&
	    strcmp(a->fullname, b->fullname) == 0 &&
	    a->encoding && b->encoding)
		return strcmp(a->encoding, b->encoding);

	return 1;
}
#endif
