/* gtkpspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
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
 */
 
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if USE_PSPELL
#include "intl.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <prefs_common.h>
#include <utils.h>

#include <dirent.h>

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gdk/gdkkeysyms.h>

#include "gtkstext.h"

#include "gtkspell.h"

#include <pspell/pspell.h>
/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024

/* number of suggestions to display on each menu. */
#define MENUCOUNT 15

/* 'config' must be defined as a 'PspellConfig *' */
#define RETURN_FALSE_IF_CONFIG_ERROR() \
	if (pspell_config_error_number(config) != 0) { \
		gtkpspellcheckers->error_message = g_strdup(pspell_config_error_message(config)); \
		return FALSE; \
	}

#define CONFIG_REPLACE_RETURN_FALSE_IF_FAIL(option, value) { \
	pspell_config_replace(config, option, value);       \
	RETURN_FALSE_IF_CONFIG_ERROR();                      \
	}

/******************************************************************************/

GtkPspellCheckers *gtkpspellcheckers;

/* Error message storage */
static void 		gtkpspell_checkers_error_message(gchar *message);

/* Callbacks */
static void 		entry_insert_cb			(GtkSText *gtktext, 
							 gchar *newtext, 
						 	 guint len, 
							 guint *ppos, 
					 		 GtkPspell *gtkpspell);
static void 		entry_delete_cb			(GtkSText *gtktext, 
							 gint start, 
							 gint end,
						 	 GtkPspell *gtkpspell);
static gint 		button_press_intercept_cb	(GtkSText *gtktext, 
							 GdkEvent *e, 
							 GtkPspell *gtkpspell);

/* Checker creation */
static GtkPspeller *	gtkpspeller_new			(Dictionary *dict);
static GtkPspeller *	gtkpspeller_real_new		(Dictionary *dict);
static GtkPspeller *	gtkpspeller_delete		(GtkPspeller *gtkpspeller);
static GtkPspeller *	gtkpspeller_real_delete		(GtkPspeller *gtkpspeller);

/* Checker configuration */
static gint 		set_dictionary   		(PspellConfig *config, 
							 Dictionary *dict);
static void 		set_sug_mode_cb     		(GtkMenuItem *w, 
							 GtkPspell *gtkpspell);
static void 		set_real_sug_mode		(GtkPspell *gtkpspell, 
							 const char *themode);

/* Checker actions */
static gboolean 	check_at			(GtkPspell *gtkpspell, 
							 int from_pos);
static gboolean		check_next_prev			(GtkPspell *gtkpspell, 
							 gboolean forward);
static GList*		misspelled_suggest	 	(GtkPspell *gtkpspell, 
							 guchar *word);
static void 		add_word_to_session_cb		(GtkWidget *w, 
							 gpointer data);
static void 		add_word_to_personal_cb		(GtkWidget *w, 
							 gpointer data);
static void 		replace_with_create_dialog_cb	(GtkWidget *w,
							 gpointer data);
static void 		replace_with_supplied_word_cb	(GtkWidget *w, 
							 GtkPspell *gtkpspell);
static void 		replace_word_cb			(GtkWidget *w, 
							 gpointer data); 
static void 		replace_real_word		(GtkPspell *gtkpspell, 
							 gchar *newword);
static void 		toggle_check_while_typing_cb	(GtkWidget *w, 
							 gpointer data);

/* Menu creation */
static void 		popup_menu			(GtkPspell *gtkpspell, 
							 GdkEventButton *eb);
static GtkMenu*		make_sug_menu			(GtkPspell *gtkpspell);
static void 		populate_submenu		(GtkPspell *gtkpspell, 
							 GtkWidget *menu);
static GtkMenu*		make_config_menu		(GtkPspell *gtkpspell);
static void 		set_menu_pos			(GtkMenu *menu, 
							 gint *x, 
							 gint *y, 
							 gpointer data);
/* Other menu callbacks */
static gboolean 	cancel_menu_cb			(GtkMenuShell *w,
							 gpointer data);
static void 		change_dict_cb			(GtkWidget *w, 
							 GtkPspell *gtkpspell);

/* Misc. helper functions */
static void	 	set_point_continue		(GtkPspell *gtkpspell);
static void 		continue_check			(gpointer *gtkpspell);
static gboolean 	iswordsep			(unsigned char c);
static guchar 		get_text_index_whar		(GtkPspell *gtkpspell, 
							 int pos);
static gboolean 	get_word_from_pos		(GtkPspell *gtkpspell, 
							 gint pos, 
							 unsigned char* buf,
							 gint buflen,
							 gint *pstart, 
							 gint *pend);
static void 		allocate_color			(GtkPspell *gtkpspell,
							 gint rgbvalue);
static void 		change_color			(GtkPspell *gtkpspell, 
			 				 gint start, 
							 gint end, 
							 gchar *newtext,
							 GdkColor *color);
static guchar*		convert_to_pspell_encoding 	(const guchar *encoding);
static gint 		compare_dict			(Dictionary *a, 
							 Dictionary *b);
static void 		dictionary_delete		(Dictionary *dict);
static Dictionary *	dictionary_dup			(const Dictionary *dict);
static void 		free_suggestions_list		(GtkPspell *gtkpspell);
static void 		reset_theword_data		(GtkPspell *gtkpspell);
static void 		free_checkers			(gpointer elt, 
							 gpointer data);
static gint 		find_gtkpspeller		(gconstpointer aa, 
							 gconstpointer bb);
static void		gtkpspell_alert_dialog		(gchar *message);
/* gtkspellconfig - only one config per session */
GtkPspellConfig * gtkpspellconfig;

/******************************************************************************/

GtkPspellCheckers * gtkpspell_checkers_new()
{
	GtkPspellCheckers *gtkpspellcheckers;
	
	gtkpspellcheckers = g_new(GtkPspellCheckers, 1);
	
	gtkpspellcheckers->checkers        = NULL;

	gtkpspellcheckers->dictionary_list = NULL;

	gtkpspellcheckers->error_message   = NULL;
	
	return gtkpspellcheckers;
}
	
GtkPspellCheckers *gtkpspell_checkers_delete()
{
	GSList *checkers;
	GSList *dict_list;

	if (gtkpspellcheckers == NULL) return NULL;

	if ((checkers  = gtkpspellcheckers->checkers)) {
		debug_print(_("Pspell: number of running checkers to delete %d\n"),
				g_slist_length(checkers));

		g_slist_foreach(checkers, free_checkers, NULL);
		g_slist_free(checkers);
	}

	if ((dict_list = gtkpspellcheckers->dictionary_list)) {
		debug_print(_("Pspell: number of dictionaries to delete %d\n"),
				g_slist_length(dict_list));

		gtkpspell_free_dictionary_list(dict_list);
		gtkpspellcheckers->dictionary_list = NULL;
	}

	g_free(gtkpspellcheckers->error_message);

	return NULL;
}

static void gtkpspell_checkers_error_message (gchar *message)
{
	gchar *tmp;
	if (gtkpspellcheckers->error_message) {
		tmp = g_strdup_printf("%s\n%s", 
				      gtkpspellcheckers->error_message, message);
		g_free(message);
		g_free(gtkpspellcheckers->error_message);
		gtkpspellcheckers->error_message = tmp;
	} else gtkpspellcheckers->error_message = message;
}

void gtkpspell_checkers_reset_error()
{
	g_return_if_fail(gtkpspellcheckers);
	
	g_free(gtkpspellcheckers->error_message);
	
	gtkpspellcheckers->error_message = NULL;
}

GtkPspell *gtkpspell_new(const gchar *dictionary, 
			 const gchar *encoding,
			 gint  misspelled_color,
			 gboolean check_while_typing,
			 GtkSText *gtktext)
{
	Dictionary 	*dict = g_new0(Dictionary, 1);
	GtkPspell 	*gtkpspell;
	GtkPspeller 	*gtkpspeller;

	g_return_val_if_fail(gtktext, NULL);
	
	dict->fullname = g_strdup(dictionary);
	dict->encoding = g_strdup(encoding);
	gtkpspeller    = gtkpspeller_new(dict); 
	dictionary_delete(dict);

	if (!gtkpspeller)
		return NULL;
	
	gtkpspell = g_new(GtkPspell, 1);

	gtkpspell->gtkpspeller	      = gtkpspeller;
	gtkpspell->theword[0]	      = 0x00;
	gtkpspell->start_pos	      = 0;
	gtkpspell->end_pos	      = 0;
	gtkpspell->orig_pos	      = -1;
	gtkpspell->end_check_pos      = -1;
	gtkpspell->misspelled	      = -1;
	gtkpspell->check_while_typing = check_while_typing;
	gtkpspell->continue_check     = NULL;
	gtkpspell->config_menu        = NULL;
	gtkpspell->popup_config_menu  = NULL;
	gtkpspell->sug_menu	      = NULL;
	gtkpspell->replace_entry      = NULL;
	gtkpspell->gtktext	      = gtktext;
	gtkpspell->default_sug_mode   = PSPELL_FASTMODE;
	gtkpspell->max_sug	      = -1;
	gtkpspell->suggestions_list   = NULL;

	allocate_color(gtkpspell, misspelled_color);

	gtk_signal_connect_after(GTK_OBJECT(gtktext), "insert-text",
		                 GTK_SIGNAL_FUNC(entry_insert_cb), gtkpspell);
	gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
		                 GTK_SIGNAL_FUNC(entry_delete_cb), gtkpspell);
	gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
		           GTK_SIGNAL_FUNC(button_press_intercept_cb), gtkpspell);
	
	debug_print("Pspell: created gtkpspell %0x\n", gtkpspell);

	return gtkpspell;
}

void gtkpspell_delete(GtkPspell * gtkpspell) 
{
	GtkSText * gtktext;
	
	gtktext = gtkpspell->gtktext;

        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_insert_cb), gtkpspell);
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_delete_cb), gtkpspell);
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(button_press_intercept_cb), 
				      gtkpspell);

	gtkpspell_uncheck_all(gtkpspell);
	
	gtkpspeller_delete(gtkpspell->gtkpspeller);

	if (gtkpspell->sug_menu)
		gtk_widget_destroy(gtkpspell->sug_menu);
	if (gtkpspell->popup_config_menu)
		gtk_widget_destroy(gtkpspell->popup_config_menu);
	if (gtkpspell->config_menu)
		gtk_widget_destroy(gtkpspell->config_menu);
	if (gtkpspell->suggestions_list)
		free_suggestions_list(gtkpspell);
	
	debug_print("Pspell: deleting gtkpspell %0x\n", gtkpspell);

	g_free(gtkpspell);

	gtkpspell = NULL;
}

static void entry_insert_cb(GtkSText *gtktext, gchar *newtext, 
			    guint len, guint *ppos, 
                            GtkPspell * gtkpspell) 
{
	g_return_if_fail(gtkpspell->gtkpspeller->checker);

	if (!gtkpspell->check_while_typing)
		return;
	
	/* We must insert ourself the character to impose the */
	/* color of the inserted character to be default */
	/* Never mess with set_insertion when frozen */
	gtk_stext_freeze(gtktext);
	gtk_stext_backward_delete(GTK_STEXT(gtktext), len);
	gtk_stext_insert(GTK_STEXT(gtktext), NULL, NULL, NULL, newtext, len);
	*ppos = gtk_stext_get_point(GTK_STEXT(gtktext));
	       
	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (*ppos >= 2) 
			check_at(gtkpspell, *ppos - 2);

		/* did we just split a word? */
		if (*ppos < gtk_stext_get_length(gtktext))
			check_at(gtkpspell, *ppos + 1);
	} else {
		/* check as they type, *except* if they're typing at the end (the most
                 * common case.
                 */
		if (*ppos < gtk_stext_get_length(gtktext) 
		&&  !iswordsep(get_text_index_whar(gtkpspell, *ppos)))
			check_at(gtkpspell, *ppos - 1);
		}
	gtk_stext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), *ppos);
}

static void entry_delete_cb(GtkSText *gtktext, gint start, gint end, 
			    GtkPspell *gtkpspell) 
{
	int origpos;
    
	g_return_if_fail(gtkpspell->gtkpspeller->checker);

	if (!gtkpspell->check_while_typing)
		return;

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	if (start) {
		check_at(gtkpspell, start - 1);
		check_at(gtkpspell, start);
	}

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(gtktext, origpos);
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
	/* this is to *UNDO* the selection, in case they were holding shift
         * while hitting backspace. */
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor,
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu.
 * HACK HACK HACK.
 */
static gint button_press_intercept_cb(GtkSText *gtktext, GdkEvent *e, GtkPspell *gtkpspell) 
{
	GdkEventButton *eb;
	gboolean retval;

	g_return_val_if_fail(gtkpspell->gtkpspeller->checker, FALSE);

	if (e->type != GDK_BUTTON_PRESS) 
		return FALSE;
	eb = (GdkEventButton*) e;

	if (eb->button != 3) 
		return FALSE;

	/* forge the leftclick */
	eb->button = 1;

        gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(button_press_intercept_cb), 
					 gtkpspell);
	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "button-press-event",
				e, &retval);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(button_press_intercept_cb), 
					   gtkpspell);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "button-press-event");
    
	/* now do the menu wackiness */
	popup_menu(gtkpspell, eb);
	gtk_grab_remove(GTK_WIDGET(gtktext));
	return TRUE;
}

/*****************************************************************************/
/* Checker creation */
static GtkPspeller *gtkpspeller_new(Dictionary *dictionary)
{
	GSList 		*exist;
	GtkPspeller	*gtkpspeller = NULL;
	GtkPspeller	*g           = g_new0(GtkPspeller, 1);
	Dictionary	*dict;
	gint		ispell;
		
	g_return_val_if_fail(gtkpspellcheckers, NULL);

	g_return_val_if_fail(dictionary, NULL);

	if (dictionary->fullname == NULL)
		gtkpspell_checkers_error_message(g_strdup(_("No dictionary selected.")));
	
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

	ispell = (strstr2(dict->dictname, "-ispell") != NULL);

	g->dictionary = dict;

	exist = g_slist_find_custom(gtkpspellcheckers->checkers, g, 
				    find_gtkpspeller);
	
	g_free(g);

	if (exist && ispell) {
		gtkpspeller = (GtkPspeller *) exist->data;
		dictionary_delete(dict);

		debug_print(_("Pspell: Using existing ispell checker %0x\n"),
			    (gint) gtkpspeller);
	} else {
		if ((gtkpspeller = gtkpspeller_real_new(dict)) != NULL) {
			gtkpspellcheckers->checkers = g_slist_append(
						gtkpspellcheckers->checkers,
						gtkpspeller);

			debug_print(_("Pspell: Created a new gtkpspeller %0x\n"),
				    (gint) gtkpspeller);
		} else {
			dictionary_delete(dict);

			debug_print(_("Pspell: Could not create spell checker.\n"));
		}
	}

	debug_print(_("Pspell: number of existing checkers %d\n"), 
		    g_slist_length(gtkpspellcheckers->checkers));

	return gtkpspeller;
}

static GtkPspeller *gtkpspeller_real_new(Dictionary *dict)
{
	GtkPspeller		*gtkpspeller;
	PspellConfig		*config;
	PspellCanHaveError 	*ret;
	
	g_return_val_if_fail(gtkpspellcheckers, NULL);
	g_return_val_if_fail(dict, NULL);

	gtkpspeller = g_new(GtkPspeller, 1);
	
	gtkpspeller->dictionary = dict;
	gtkpspeller->sug_mode   = PSPELL_FASTMODE;

	config = new_pspell_config();

	if (!set_dictionary(config, dict))
		return NULL;
	
	ret = new_pspell_manager(config);
	delete_pspell_config(config);

	if (pspell_error_number(ret) != 0) {
		gtkpspellcheckers->error_message = g_strdup(pspell_error_message(ret));
		
		delete_pspell_can_have_error(ret);
		
		return NULL;
	}

	gtkpspeller->checker = to_pspell_manager(ret);
	gtkpspeller->config  = pspell_manager_config(gtkpspeller->checker);
	gtkpspeller->ispell  = (strstr2(dict->fullname, "-ispell") != NULL);

	return gtkpspeller;
}

static GtkPspeller *gtkpspeller_delete(GtkPspeller *gtkpspeller)
{
	g_return_val_if_fail(gtkpspellcheckers, NULL);
	
	if (gtkpspeller->ispell) 
		debug_print(_("Pspell: Won't remove existing ispell checker %0x.\n"), 
			    (gint) gtkpspeller);
	else {
		gtkpspellcheckers->checkers = g_slist_remove(gtkpspellcheckers->checkers, 
						gtkpspeller);

		debug_print(_("Pspell: Deleting gtkpspeller %0x.\n"), 
			    (gint) gtkpspeller);
		
		gtkpspeller_real_delete(gtkpspeller);
	}

	debug_print(_("Pspell: number of existing checkers %d\n"), 
		    g_slist_length(gtkpspellcheckers->checkers));

	return gtkpspeller;
}

static GtkPspeller *gtkpspeller_real_delete(GtkPspeller *gtkpspeller)
{
	g_return_val_if_fail(gtkpspeller,          NULL);
	g_return_val_if_fail(gtkpspeller->checker, NULL);

	pspell_manager_save_all_word_lists(gtkpspeller->checker);

	delete_pspell_manager(gtkpspeller->checker);

	dictionary_delete(gtkpspeller->dictionary);

	g_free(gtkpspeller);

	return NULL;
}

/*****************************************************************************/
/* Checker configuration */

static gboolean set_dictionary(PspellConfig *config, Dictionary *dict)
{
	gchar *language = NULL;
	gchar *spelling = NULL;
	gchar *jargon   = NULL;
	gchar *module   = NULL;
	gchar *end      = NULL;
	gchar buf[BUFSIZE];
	
	g_return_val_if_fail(config, FALSE);
	g_return_val_if_fail(dict,   FALSE);

	strncpy(buf, dict->fullname, BUFSIZE-1);
	buf[BUFSIZE-1] = 0x00;

	buf[dict->dictname - dict->fullname] = 0x00;

	CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("rem-all-word-list-path", "");
	debug_print(_("Pspell: removed all paths.\n"));

	CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("add-word-list-path", buf);
	debug_print(_("Pspell: added path %s.\n"), buf);

	strncpy(buf, dict->dictname, BUFSIZE-1);
	language = buf;
	
	if ((module = strrchr(buf, '-')) != NULL) {
		module++;
	}
	if ((spelling = strchr(language, '-')) != NULL) 
		*spelling++ = 0x00;
	if (spelling != module) {
		if ((end = strchr(spelling, '-')) != NULL) {
			*end++ = 0x00;
			jargon = end;
			if (jargon != module)
				if ((end = strchr(jargon, '-')) != NULL)
					*end = 0x00;
				else
					jargon = NULL;
			else
				jargon = NULL;
		}
		else
			spelling = NULL;
	}
	else
		spelling = NULL;

	debug_print(_("Pspell: Language: %s, spelling: %s, jargon: %s, module: %s\n"),
		    language, spelling, jargon, module);
	
	if (language)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("language-tag", language);
	
	if (spelling)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("spelling",     spelling);
	
	if (jargon)
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("jargon",       jargon);
	
	if (module) {
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("rem-all-module-search-order", "");
		CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("add-module-search-order", module);
	}
	
	if (dict->encoding) {
		gchar *pspell_enc;
	
		pspell_enc = convert_to_pspell_encoding (dict->encoding);
		pspell_config_replace(config, "encoding", (const char *) pspell_enc);
		g_free(pspell_enc);

		RETURN_FALSE_IF_CONFIG_ERROR();
	}
	
	return TRUE;
}

guchar *gtkpspell_get_dict(GtkPspell *gtkpspell)
{

	g_return_val_if_fail(gtkpspell->gtkpspeller->config,     NULL);
	g_return_val_if_fail(gtkpspell->gtkpspeller->dictionary, NULL);
 	
	return g_strdup(gtkpspell->gtkpspeller->dictionary->dictname);
}
  
guchar *gtkpspell_get_path(GtkPspell *gtkpspell)
{
	guchar *path;
	Dictionary *dict;

	g_return_val_if_fail(gtkpspell->gtkpspeller->config, NULL);
	g_return_val_if_fail(gtkpspell->gtkpspeller->dictionary, NULL);

	dict = gtkpspell->gtkpspeller->dictionary;
	path = g_strndup(dict->fullname, dict->dictname - dict->fullname);

	return path;
}

/* set_sug_mode_cb() - Menu callback : Set the suggestion mode */
static void set_sug_mode_cb(GtkMenuItem *w, GtkPspell *gtkpspell)
{
	char *themode;
	
	if (gtkpspell->gtkpspeller->ispell) return;

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar **) &themode);
	
	set_real_sug_mode(gtkpspell, themode);

	if (gtkpspell->config_menu)
		populate_submenu(gtkpspell, gtkpspell->config_menu);
}
static void set_real_sug_mode(GtkPspell *gtkpspell, const char *themode)
{
	gint result;
	gint mode = PSPELL_FASTMODE;
	g_return_if_fail(gtkpspell);
	g_return_if_fail(gtkpspell->gtkpspeller);
	g_return_if_fail(themode);

	if (!strcmp(themode,_("Normal Mode")))
		mode = PSPELL_NORMALMODE;
	else if (!strcmp( themode,_("Bad Spellers Mode")))
		mode = PSPELL_BADSPELLERMODE;

	result = gtkpspell_set_sug_mode(gtkpspell, mode);

	if(!result) {
		debug_print(_("Pspell: error while changing suggestion mode:%s\n"),
			    gtkpspellcheckers->error_message);
		gtkpspell_checkers_reset_error();
	}
}
  
/* gtkpspell_set_sug_mode() - Set the suggestion mode */
gboolean gtkpspell_set_sug_mode(GtkPspell *gtkpspell, gint themode)
{
	PspellConfig *config;

	g_return_val_if_fail(gtkpspell, FALSE);
	g_return_val_if_fail(gtkpspell->gtkpspeller, FALSE);
	g_return_val_if_fail(gtkpspell->gtkpspeller->config, FALSE);

	debug_print("Pspell: setting sug mode of gtkpspeller %0x to %d\n",
			gtkpspell->gtkpspeller, themode);
	config = gtkpspell->gtkpspeller->config;
	switch (themode) {
		case PSPELL_FASTMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "fast");
			break;
		case PSPELL_NORMALMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", "normal");
			break;
		case PSPELL_BADSPELLERMODE: 
			CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("sug-mode", 
							    "bad-spellers");
			break;
		default: 
			gtkpspellcheckers->error_message = 
				g_strdup(_("Unknown suggestion mode."));
			return FALSE;
		}

	gtkpspell->gtkpspeller->sug_mode = themode;
	gtkpspell->default_sug_mode      = themode;

	return TRUE;
}

/* misspelled_suggest() - Create a suggestion list for  word  */
static GList *misspelled_suggest(GtkPspell *gtkpspell, guchar *word) 
{
	const guchar          *newword;
	GList                 *list = NULL;
	const PspellWordList  *suggestions;
	PspellStringEmulation *elements;

	g_return_val_if_fail(word, NULL);

	if (!pspell_manager_check(gtkpspell->gtkpspeller->checker, word, -1)) {
		free_suggestions_list(gtkpspell);

		suggestions = pspell_manager_suggest(gtkpspell->gtkpspeller->checker, 
						     (const char *)word, -1);
		elements    = pspell_word_list_elements(suggestions);
		list        = g_list_append(list, g_strdup(word)); 
		
		while ((newword = pspell_string_emulation_next(elements)) != NULL)
			list = g_list_append(list, g_strdup(newword));

		gtkpspell->max_sug          = g_list_length(list) - 1;
		gtkpspell->suggestions_list = list;

		return list;
	}

	free_suggestions_list(gtkpspell);

	return NULL;
}

/* misspelled_test() - Just test if word is correctly spelled */  
static int misspelled_test(GtkPspell *gtkpspell, unsigned char *word) 
{
	return pspell_manager_check(gtkpspell->gtkpspeller->checker, word, -1) ? 0 : 1; 
}


static gboolean iswordsep(unsigned char c) 
{
	return !isalpha(c) && c != '\'';
}

static guchar get_text_index_whar(GtkPspell *gtkpspell, int pos) 
{
	guchar a;
	gchar *text;
	
	text = gtk_editable_get_chars(GTK_EDITABLE(gtkpspell->gtktext), pos, pos + 1);
	if (text == NULL) 
		return 0;
	a = (guchar) *text;
	g_free(text);
	return a;
}

/* get_word_from_pos () - return the word pointed to. */
/* Handles correctly the quotes. */
static gboolean get_word_from_pos(GtkPspell *gtkpspell, gint pos, 
                                  unsigned char* buf, gint buflen,
                                  gint *pstart, gint *pend) 
{

	/* TODO : when correcting a word into quotes, change the color of */
	/* the quotes too, as may be they were highlighted before. To do  */
	/* this, we can use two others pointers that points to the whole    */
	/* word including quotes. */

	gint      start, 
	          end;
	guchar    c;
	GtkSText *gtktext;
	
	gtktext = gtkpspell->gtktext;
	if (iswordsep(get_text_index_whar(gtkpspell, pos))) 
		return FALSE;
	
	/* The apostrophe character is somtimes used for quotes 
	 * So include it in the word only if it is not surrounded 
	 * by other characters. 
	 */
	 
	for (start = pos; start >= 0; --start) {
		c = get_text_index_whar(gtkpspell, start);
		if (c == '\'') {
			if (start > 0) {
				if (!isalpha(get_text_index_whar(gtkpspell, start - 1))) {
					/* start_quote = TRUE; */
					break;
				}
			}
			else {
				/* start_quote = TRUE; */
				break;
			}
		}
		else
			if (!isalpha(c))
				break;
	}
	start++;

	for (end = pos; end < gtk_stext_get_length(gtktext); end++) {
		c = get_text_index_whar(gtkpspell, end); 
		if (c == '\'') {
			if (end < gtk_stext_get_length(gtktext)) {
				if (!isalpha(get_text_index_whar(gtkpspell, end + 1))) {
					/* end_quote = TRUE; */
					break;
				}
			}
			else {
				/* end_quote = TRUE; */
				break;
			}
		}
		else
			if(!isalpha(c))
				break;
	}
						
	if (pstart) 
		*pstart = start;
	if (pend) 
		*pend = end;

	if (buf) {
		if (end - start < buflen) {
			for (pos = start; pos < end; pos++) 
				buf[pos - start] = get_text_index_whar(gtkpspell, pos);
			buf[pos - start] = 0;
		} else return FALSE;
	}

	return TRUE;
}

static gboolean check_at(GtkPspell *gtkpspell, gint from_pos) 
{
	gint	      start, end;
	unsigned char buf[GTKPSPELLWORDSIZE];
	GtkSText     *gtktext;

	g_return_val_if_fail(from_pos >= 0, FALSE);
    
	gtktext = gtkpspell->gtktext;

	if (!get_word_from_pos(gtkpspell, from_pos, buf, sizeof(buf), 
			       &start, &end))
		return FALSE;

	if (misspelled_test(gtkpspell, buf)) {
		strncpy(gtkpspell->theword, buf, GTKPSPELLWORDSIZE - 1);
		gtkpspell->theword[GTKPSPELLWORDSIZE - 1] = 0;
		gtkpspell->start_pos  = start;
		gtkpspell->end_pos    = end;
		free_suggestions_list(gtkpspell);

		change_color(gtkpspell, start, end, buf, &(gtkpspell->highlight));
		return TRUE;
	} else {
		change_color(gtkpspell, start, end, buf, NULL);
		return FALSE;
	}
}

static gboolean check_next_prev(GtkPspell *gtkpspell, gboolean forward)
{
	gint pos;
	gint minpos;
	gint maxpos;
	gint direc = -1;
	gboolean misspelled;
	
	minpos = 0;
	maxpos = gtkpspell->end_check_pos;

	if (forward) {
		minpos = -1;
		direc = 1;
		maxpos--;
	} 

	pos = gtk_editable_get_position(GTK_EDITABLE(gtkpspell->gtktext));
	gtkpspell->orig_pos = pos;
	while (iswordsep(get_text_index_whar(gtkpspell, pos)) && pos > minpos && pos <= maxpos) 
		pos += direc;
	while (!(misspelled = check_at(gtkpspell, pos)) && pos > minpos && pos <= maxpos)
	{
		while (!iswordsep(get_text_index_whar(gtkpspell, pos)) && pos > minpos && pos <= maxpos)
			pos += direc;

		while (iswordsep(get_text_index_whar(gtkpspell, pos)) && pos > minpos && pos <= maxpos) 
			pos += direc;
	}
	if (misspelled) {
		misspelled_suggest(gtkpspell, gtkpspell->theword);
		if (forward)
			gtkpspell->orig_pos = gtkpspell->end_pos;
		gtk_stext_set_point(GTK_STEXT(gtkpspell->gtktext),
				gtkpspell->end_pos);
		gtk_editable_set_position(GTK_EDITABLE(gtkpspell->gtktext),
				gtkpspell->end_pos);
		gtk_menu_popup(make_sug_menu(gtkpspell), NULL, NULL, 
				set_menu_pos, gtkpspell, 0, GDK_CURRENT_TIME);
	} else {
		reset_theword_data(gtkpspell);

		gtkpspell_alert_dialog(_("No misspelled word found."));
		gtk_stext_set_point(GTK_STEXT(gtkpspell->gtktext), gtkpspell->orig_pos);
		gtk_editable_set_position(GTK_EDITABLE(gtkpspell->gtktext), gtkpspell->orig_pos);
	}
	return misspelled;
}

void gtkpspell_check_backwards(GtkPspell *gtkpspell)
{
	gtkpspell->continue_check = NULL;
	gtkpspell->end_check_pos = gtk_stext_get_length(GTK_STEXT(gtkpspell->gtktext));
	check_next_prev(gtkpspell, FALSE);
}

void gtkpspell_check_forwards_go(GtkPspell *gtkpspell)
{

	gtkpspell->continue_check = NULL;
	gtkpspell->end_check_pos = gtk_stext_get_length(GTK_STEXT(gtkpspell->gtktext));
	check_next_prev(gtkpspell, TRUE);
}

void gtkpspell_check_all(GtkPspell *gtkpspell)
{	
	GtkWidget *gtktext;
	
	gint start, end;

	g_return_if_fail(gtkpspell);
	g_return_if_fail(gtkpspell->gtktext);

	gtktext = (GtkWidget *) gtkpspell->gtktext;

	start = 0;	
	end   = gtk_stext_get_length(GTK_STEXT(gtktext));

	if (GTK_EDITABLE(gtktext)->has_selection) {
		start = GTK_EDITABLE(gtktext)->selection_start_pos;
		end   = GTK_EDITABLE(gtktext)->selection_end_pos;
	}

	if (start > end) {
		gint tmp;
		tmp   = start;
		start = end;
		end   = tmp;
	}
		
	
	gtk_editable_set_position(GTK_EDITABLE(gtktext), start);
	gtk_stext_set_point(GTK_STEXT(gtktext), start);

	gtkpspell->continue_check = continue_check;
	gtkpspell->end_check_pos = end;

	gtkpspell->misspelled = check_next_prev(gtkpspell, TRUE);

}	

static void continue_check(gpointer *data)
{
	GtkPspell *gtkpspell = (GtkPspell *) data;
	gint pos = gtk_editable_get_position(GTK_EDITABLE(gtkpspell->gtktext));
	
	if (pos < gtkpspell->end_check_pos && gtkpspell->misspelled)
		gtkpspell->misspelled = check_next_prev(gtkpspell, TRUE);
	else
		gtkpspell->continue_check = NULL;
		
}

void gtkpspell_highlight_all(GtkPspell *gtkpspell) 
{
	guint     origpos;
	guint     pos = 0;
	guint     len;
	GtkSText *gtktext;
	gfloat    adj_value;

	g_return_if_fail(gtkpspell->gtkpspeller->checker);	

	gtktext = gtkpspell->gtktext;

	adj_value = gtktext->vadj->value;

	len = gtk_stext_get_length(gtktext);

	gtk_stext_freeze(gtktext);

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

/*	gtk_editable_set_position(GTK_EDITABLE(gtktext), 0);*/

	while (pos < len) {
		while (pos < len && 
		       iswordsep(get_text_index_whar(gtkpspell, pos)))
			pos++;
		while (pos < len &&
		       !iswordsep(get_text_index_whar(gtkpspell, pos)))
			pos++;
		if (pos > 0)
			check_at(gtkpspell, pos - 1);
	}
	gtk_stext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(GTK_STEXT(gtktext), origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);
	/*gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);*/
}

static void replace_with_supplied_word_cb(GtkWidget *w, GtkPspell *gtkpspell) 
{
	unsigned char *newword;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();
	
	newword = gtk_editable_get_chars(GTK_EDITABLE(gtkpspell->replace_entry), 0, -1);
	
	if (strcmp(newword, gtkpspell->theword)) {
		replace_real_word(gtkpspell, newword);

		if ((e->type == GDK_KEY_PRESS && 
		    ((GdkEventKey *) e)->state & GDK_MOD1_MASK)) {
			pspell_manager_store_replacement(gtkpspell->gtkpspeller->checker, 
							 gtkpspell->theword, -1, 
							 newword, -1);
		}
		gtkpspell->replace_entry = NULL;
	}

	g_free(newword);

	set_point_continue(gtkpspell);
}


static void replace_word_cb(GtkWidget *w, gpointer data)
{
	unsigned char *newword;
	GtkPspell *gtkpspell = (GtkPspell *) data;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();


	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar**) &newword);

	replace_real_word(gtkpspell, newword);

	if ((e->type == GDK_KEY_PRESS && 
	    ((GdkEventKey *) e)->state & GDK_MOD1_MASK) ||
	    (e->type == GDK_BUTTON_RELEASE && 
	     ((GdkEventButton *) e)->state & GDK_MOD1_MASK)) {
		pspell_manager_store_replacement(gtkpspell->gtkpspeller->checker, 
						 gtkpspell->theword, -1, 
						 newword, -1);
	}

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));

	set_point_continue(gtkpspell);
}

static void replace_real_word(GtkPspell *gtkpspell, gchar *newword)
{
	int		oldlen, newlen, wordlen;
	gint		origpos;
	gint		pos;
	gint 		start = gtkpspell->start_pos;
	GtkSText       *gtktext;
    
	if (!newword) return;

	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));
	origpos = gtkpspell->orig_pos;
	pos     = origpos;
	oldlen  = gtkpspell->end_pos - gtkpspell->start_pos;
	wordlen = strlen(gtkpspell->theword);

	newlen = strlen(newword); /* FIXME: multybyte characters? */

	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(entry_insert_cb), 
					 gtkpspell);
	gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
					 GTK_SIGNAL_FUNC(entry_delete_cb), 
					 gtkpspell);

	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "delete-text", 
				gtkpspell->start_pos, gtkpspell->end_pos);
	gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "insert-text", 
				newword, newlen, &start);
	
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(entry_insert_cb), 
					   gtkpspell);
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
					   GTK_SIGNAL_FUNC(entry_delete_cb), 
					   gtkpspell);
	
	/* Put the point and the position where we clicked with the mouse */
	/* It seems to be a hack, as I must thaw,freeze,thaw the widget   */
	/* to let it update correctly the word insertion and then the     */
	/* point & position position. If not, SEGV after the first replacement */
	/* If the new word ends before point, put the point at its end*/
    
	if (origpos - gtkpspell->start_pos < oldlen && 
	    origpos - gtkpspell->start_pos >= 0) {
		/* Original point was in the word. */
		/* Let it there unless point is going to be outside of the word */
		if (origpos - gtkpspell->start_pos >= newlen) {
			pos = gtkpspell->start_pos + newlen;
		}
	}
	else if (origpos >= gtkpspell->end_pos) {
		/* move the position according to the change of length */
		pos = origpos + newlen - oldlen;
	}
	
	gtkpspell->end_pos = gtkpspell->start_pos + strlen(newword); /* FIXME: multibyte characters? */
	
	gtk_stext_thaw(GTK_STEXT(gtktext));
	gtk_stext_freeze(GTK_STEXT(gtktext));

	if (GTK_STEXT(gtktext)->text_len < pos)
		pos = gtk_stext_get_length(GTK_STEXT(gtktext));

	gtkpspell->orig_pos = pos;

	gtk_editable_set_position(GTK_EDITABLE(gtktext), gtkpspell->orig_pos);
	gtk_stext_set_point(GTK_STEXT(gtktext), 
			    gtk_editable_get_position(GTK_EDITABLE(gtktext)));

	gtk_stext_thaw(GTK_STEXT(gtktext));
}

/* Accept this word for this session */
static void add_word_to_session_cb(GtkWidget *w, gpointer data)
{
	guint     pos;
	GtkSText *gtktext;
   	GtkPspell *gtkpspell = (GtkPspell *) data; 
	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));

	pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    
	pspell_manager_add_to_session(gtkpspell->gtkpspeller->checker,
				      gtkpspell->theword, 
				      strlen(gtkpspell->theword));

	check_at(gtkpspell, gtkpspell->start_pos);

	gtk_stext_thaw(gtkpspell->gtktext);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	set_point_continue(gtkpspell);
}

/* add_word_to_personal_cb() - add word to personal dict. */
static void add_word_to_personal_cb(GtkWidget *w, gpointer data)
{
	GtkSText *gtktext;
   	GtkPspell *gtkpspell = (GtkPspell *) data; 
	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));
    
	pspell_manager_add_to_personal(gtkpspell->gtkpspeller->checker,
				       gtkpspell->theword,
				       strlen(gtkpspell->theword));
    
	check_at(gtkpspell, gtkpspell->start_pos);

	gtk_stext_thaw(gtkpspell->gtktext);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));
	set_point_continue(gtkpspell);
}

static void deactivate_sug_menu_cb(GtkObject *w, gpointer data)
{
	GtkMenuShell *menu_shell = GTK_MENU_SHELL(data);
	debug_print("Destroying %0x\n", w);
	gtk_menu_shell_deactivate(menu_shell);
}

static void replace_with_create_dialog_cb(GtkWidget *w, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	gchar *thelabel;
	gint xx, yy;
	GtkPspell *gtkpspell = (GtkPspell *) data;

	gdk_window_get_origin((GTK_WIDGET(w)->parent)->window, &xx, &yy);

	gtk_menu_shell_deactivate(GTK_MENU_SHELL(GTK_WIDGET(w)->parent));

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_title(GTK_WINDOW(dialog),_("Replace unknown word"));
	gtk_widget_set_uposition(dialog, xx, yy);

	gtk_signal_connect_object(GTK_OBJECT(dialog), "destroy",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				  GTK_OBJECT(dialog));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

	thelabel = g_strdup_printf(_("Replace \"%s\" with: "), 
				   gtkpspell->theword);
	label = gtk_label_new(thelabel);
	g_free(thelabel);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtkpspell->replace_entry = entry;
	gtk_entry_set_text(GTK_ENTRY(entry), gtkpspell->theword);
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(replace_with_supplied_word_cb), 
			   gtkpspell);
	gtk_signal_connect_object(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
			   GTK_OBJECT(dialog));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, 
			   TRUE, 0);
	if (!gtkpspell->gtkpspeller->ispell) {
		label = gtk_label_new(_("Holding down MOD1 key while pressing Enter\nwill learn from mistake.\n"));
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
		gtk_misc_set_padding(GTK_MISC(label), 8, 0);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, 
				   TRUE, TRUE, 0);
	}

	hbox = gtk_hbox_new(TRUE, 0);

	ok_button = gtk_button_new_with_label(_("OK"));
	gtk_box_pack_start(GTK_BOX(hbox), ok_button, TRUE, TRUE, 8);
	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(replace_with_supplied_word_cb), 
			   gtkpspell);
	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				  GTK_OBJECT(dialog));

	cancel_button = gtk_button_new_with_label(_("Cancel"));
	gtk_box_pack_start(GTK_BOX(hbox), cancel_button, TRUE, TRUE, 8);
	gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				  GTK_OBJECT(dialog));

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), hbox);

	gtk_widget_grab_focus(entry);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	gtk_widget_show_all(dialog);
}

void gtkpspell_uncheck_all(GtkPspell * gtkpspell) 
{
	gint 	  origpos;
	gchar	 *text;
	gfloat 	  adj_value;
	GtkSText *gtktext;
	
	gtktext = gtkpspell->gtktext;

	adj_value = gtktext->vadj->value;

	gtk_stext_freeze(gtktext);

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);

	gtk_stext_set_point(gtktext, 0);
	gtk_stext_forward_delete(gtktext, gtk_stext_get_length(gtktext));
	gtk_stext_insert(gtktext, NULL, NULL, NULL, text, strlen(text));

	gtk_stext_thaw(gtktext);

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_stext_set_point(gtktext, origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);

	g_free(text);

}

static void toggle_check_while_typing_cb(GtkWidget *w, gpointer data)
{
	GtkPspell *gtkpspell = (GtkPspell *) data;

	gtkpspell->check_while_typing = gtkpspell->check_while_typing == FALSE;
	if (!gtkpspell->check_while_typing)
		gtkpspell_uncheck_all(gtkpspell);

	if (gtkpspell->config_menu)
		populate_submenu(gtkpspell, gtkpspell->config_menu);
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

/* gtkpspell_get_dictionary_list() - returns list of dictionary names */
GSList *gtkpspell_get_dictionary_list(const gchar *pspell_path, gint refresh)
{
	GSList *list;
	gchar *dict_path, *tmp, *prevdir;
	gchar tmpname[BUFSIZE];
	Dictionary *dict;
	DIR *dir;
	struct dirent *ent;

	if (!gtkpspellcheckers)
		gtkpspellcheckers = gtkpspell_checkers_new();

	if (gtkpspellcheckers->dictionary_list && !refresh)
		return gtkpspellcheckers->dictionary_list;
	else
		gtkpspell_free_dictionary_list(gtkpspellcheckers->dictionary_list);

	list = NULL;

#ifdef USE_THREADS
#warning TODO: no directory change
#endif
	dict_path = g_strdup(pspell_path);
	prevdir   = g_get_current_dir();
	if (chdir(dict_path) <0) {
		debug_print(_("Pspell: error when searching for dictionaries:\n%s\n"),
			    g_strerror(errno));
		g_free(prevdir);
		g_free(dict_path);
		gtkpspellcheckers->dictionary_list = create_empty_dictionary_list();
		return gtkpspellcheckers->dictionary_list; 
	}

	debug_print(_("Pspell: checking for dictionaries in %s\n"), dict_path);

	if (NULL != (dir = opendir("."))) {
		while (NULL != (ent = readdir(dir))) {
			/* search for pwli */
			if ((NULL != (tmp = strstr2(ent->d_name, ".pwli"))) && 
			    (tmp[5] == 0x00)) {
				g_snprintf(tmpname, BUFSIZE, "%s%s", 
					   G_DIR_SEPARATOR_S, ent->d_name);
				tmpname[MIN(tmp - ent->d_name + 1, BUFSIZE-1)] = 0x00;
				dict = g_new0(Dictionary, 1);
				dict->fullname = g_strdup_printf("%s%s", 
								 dict_path, 
								 tmpname);
				dict->dictname = strrchr(dict->fullname, 
							 G_DIR_SEPARATOR) + 1;
				dict->encoding = NULL;
				debug_print(_("Pspell: found dictionary %s %s\n"),
					    dict->fullname, dict->dictname);
				list = g_slist_insert_sorted(list, dict,
						(GCompareFunc) compare_dict);
			}
		}			
		closedir(dir);
	}
	else {
		debug_print(_("Pspell: error when searching for dictionaries.\nNo dictionary found.\n(%s)"), 
			    g_strerror(errno));
		list = create_empty_dictionary_list();
	}
        if(list==NULL){
		
		debug_print(_("Pspell: error when searching for dictionaries.\nNo dictionary found."));
		list = create_empty_dictionary_list();
	}
	chdir(prevdir);
	g_free(dict_path);
	g_free(prevdir);
	gtkpspellcheckers->dictionary_list = list;
	return list;
}

void gtkpspell_free_dictionary_list(GSList *list)
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

#if 0
static void dictionary_option_menu_item_data_destroy(gpointer data)
{
	Dictionary *dict = (Dictionary *) data;

	if (dict)
		dictionary_delete(dict);
}
#endif

GtkWidget *gtkpspell_dictionary_option_menu_new(const gchar *pspell_path)
{
	GSList *dict_list, *tmp;
	GtkWidget *item;
	GtkWidget *menu;
	Dictionary *dict;

	dict_list = gtkpspell_get_dictionary_list(pspell_path, TRUE);
	g_return_val_if_fail(dict_list, NULL);

	menu = gtk_menu_new();
	
	for (tmp = dict_list; tmp != NULL; tmp = g_slist_next(tmp)) {
		dict = (Dictionary *) tmp->data;
		item = gtk_menu_item_new_with_label(dict->dictname);
		gtk_object_set_data(GTK_OBJECT(item), "dict_name",
				    dict->fullname); 
					 
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	return menu;
}

gchar *gtkpspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	gchar *dict_fullname;
	gchar *label;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
        dict_fullname = (gchar *) gtk_object_get_data(GTK_OBJECT(menuitem), "dict_name");
        g_return_val_if_fail(dict_fullname, NULL);
	label = g_strdup(dict_fullname);
        return label;
  
}

GtkWidget *gtkpspell_sugmode_option_menu_new(gint sugmode)
{
	GtkWidget *menu;
	GtkWidget *item;


	menu = gtk_menu_new();
	gtk_widget_show(menu);

	item = gtk_menu_item_new_with_label(_("Fast Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(PSPELL_FASTMODE));

	item = gtk_menu_item_new_with_label(_("Normal Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(PSPELL_NORMALMODE));
	
	item = gtk_menu_item_new_with_label(_("Bad Spellers Mode"));
        gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "sugmode", GINT_TO_POINTER(PSPELL_BADSPELLERMODE));

	return menu;
}
	
void gtkpspell_sugmode_option_menu_set(GtkOptionMenu *optmenu, gint sugmode)
{
	g_return_if_fail(GTK_IS_OPTION_MENU(optmenu));

	g_return_if_fail(sugmode == PSPELL_FASTMODE ||
			 sugmode == PSPELL_NORMALMODE ||
			 sugmode == PSPELL_BADSPELLERMODE);

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), sugmode - 1);
}

gint gtkpspell_get_sugmode_from_option_menu(GtkOptionMenu *optmenu)
{
	gint sugmode;
	GtkWidget *item;
	
	g_return_val_if_fail(GTK_IS_OPTION_MENU(optmenu), -1);

	item = gtk_menu_get_active(GTK_MENU(gtk_option_menu_get_menu(optmenu)));
	
	sugmode = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item), "sugmode"));

	return sugmode;
	
}
#if 0 /* Experimenal */
void gtkpspell_set_primary_dict(GtkPspell *gtkpspell, gchar *dict, gchar *encoding)
{
	g_return_if_fail(gtkpspell);
	if (gtkpspell->dict1)
		dictionary_delete(gtkpspell->dict1);
	gtkpspell->dict1 = g_new(Dictionary, 1);
	gtkpspell->dict1->fullname = g_strdup(dict);
	gtkpspell->dict1->encoding = g_strdup(encoding);
}

void gtkpspell_set_secondary_dict(GtkPspell *gtkpspell, gchar *dict, gchar *encoding)
{
	g_return_if_fail(gtkpspell);
	if (gtkpspell->dict2)
		dictionary_delete(gtkpspell->dict2);
	gtkpspell->dict2 = g_new(Dictionary, 1);
	gtkpspell->dict2->fullname = g_strdup(dict);
	gtkpspell->dict2->encoding = g_strdup(encoding);
}

gboolean gtkpspell_use_dict(GtkPspell *gtkpspell, Dictionary *dict)
{
	return TRUE;
}

gboolean gtkpspell_use_dictionary(GtkPspell *gtkpspell, gchar *dictpath, gchar *encoding)
{
	Dictionary *dict;
	gboolean retval;
	
	g_return_val_if_fail(gtkpspell, FALSE);
	g_return_val_if_fail(dict, FALSE);
	g_return_val_if_fail(encoding, FALSE);

	dict = g_new(Dictionary, 1);
	
	dict->fullname = g_strdup(dictpath);
	dict->encoding = NULL; /* To be continued */

	retval = gtkpspell_use_dict(gtkpspell, dict);

	dictionary_delete(dict);
	
	return retval;
	
}

gboolean gtkpspell_use_primary(GtkPspell *gtkpspell)
{
	g_return_val_if_fail(gtkpspell, FALSE);

	return gtkpspell_use_dict(gtkpspell, gtkpspell->dict1);
}

gboolean gtkpspell_use_secondary(GtkPspell *gtkpspell)
{
	g_return_val_if_fail(gtkpspell, FALSE);

	return gtkpspell_use_dict(gtkpspell, gtkpspell->dict2);
}
#endif

/*****************************************************************************/

static void popup_menu(GtkPspell *gtkpspell, GdkEventButton *eb) 
{
	GtkSText * gtktext;
	
	gtktext = gtkpspell->gtktext;
	gtkpspell->orig_pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	if (!(eb->state & GDK_SHIFT_MASK)) {
		if (check_at(gtkpspell, gtkpspell->orig_pos)) {

			gtk_editable_set_position(GTK_EDITABLE(gtktext), 
						  gtkpspell->orig_pos);
			gtk_stext_set_point(gtktext, gtkpspell->orig_pos);

			if (misspelled_suggest(gtkpspell, gtkpspell->theword)) {
				gtk_menu_popup(make_sug_menu(gtkpspell), 
					       NULL, NULL, NULL, NULL,
					       eb->button, GDK_CURRENT_TIME);
				
				return;
			}
		} else {
			gtk_editable_set_position(GTK_EDITABLE(gtktext), 
						  gtkpspell->orig_pos);
			gtk_stext_set_point(gtktext, gtkpspell->orig_pos);
		}
	}

	gtk_menu_popup(make_config_menu(gtkpspell),NULL,NULL,NULL,NULL,
		       eb->button, GDK_CURRENT_TIME);
}

/* make_sug_menu() - Add menus to accept this word for this session and to add it to 
 * personal dictionary */
static GtkMenu *make_sug_menu(GtkPspell *gtkpspell) 
{
	GtkWidget 	*menu, *item;
	unsigned char	*caption;
	GtkSText 	*gtktext;
	GList 		*l = gtkpspell->suggestions_list;
	GtkAccelGroup 	*accel;
	

	gtktext = gtkpspell->gtktext;

	accel = gtk_accel_group_new();
	menu = gtk_menu_new(); 

	if (gtkpspell->sug_menu)
		gtk_widget_destroy(gtkpspell->sug_menu);

	gtkpspell->sug_menu = menu;	

	gtk_signal_connect(GTK_OBJECT(menu), "cancel",
		GTK_SIGNAL_FUNC(cancel_menu_cb), gtkpspell);

	caption = g_strdup_printf(_("Unknown word: \"%s\""), 
				  (unsigned char*) l->data);
	item = gtk_menu_item_new_with_label(caption);
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
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_session_cb), 
			   gtkpspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_space, GDK_MOD1_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

	item = gtk_menu_item_new_with_label(_("Add to personal dictionary"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_personal_cb), 
			   gtkpspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_Return, GDK_MOD1_MASK,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

        item = gtk_menu_item_new_with_label(_("Replace with..."));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(replace_with_create_dialog_cb), 
			   gtkpspell);
	gtk_widget_add_accelerator(item, "activate", accel, GDK_R, 0,
				   GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE);

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
			if (l->data == NULL && l->next != NULL) {
				count = 0;
				curmenu = gtk_menu_new();
				item = gtk_menu_item_new_with_label(_("Others..."));
				gtk_widget_show(item);
				gtk_menu_append(GTK_MENU(curmenu), item);
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);

				l = l->next;
			} else if (count > MENUCOUNT) {
				count -= MENUCOUNT;

				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_widget_show(item);
				gtk_menu_append(GTK_MENU(curmenu), item);

				curmenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
			}

			item = gtk_menu_item_new_with_label((unsigned char*)l->data);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(replace_word_cb), gtkpspell);

			if (curmenu == menu && count < MENUCOUNT) {
				gtk_widget_add_accelerator(item, "activate",
							   accel,
							   GDK_A + count, 0,
							   GTK_ACCEL_LOCKED | 
							   GTK_ACCEL_VISIBLE);
				gtk_widget_add_accelerator(item, "activate", 
							   accel,
							   GDK_A + count, 
							   GDK_MOD1_MASK,
							   GTK_ACCEL_LOCKED);
				}

			count++;

		} while ((l = l->next) != NULL);
	}

	gtk_accel_group_attach(accel, GTK_OBJECT(menu));
	gtk_accel_group_unref(accel);
	
	return GTK_MENU(menu);
}

static void populate_submenu(GtkPspell *gtkpspell, GtkWidget *menu)
{
	GtkWidget *item, *submenu;
	GtkPspeller *gtkpspeller = gtkpspell->gtkpspeller;
	gchar *dictname;
	gint ispell = gtkpspeller->ispell;

	if (GTK_MENU_SHELL(menu)->children) {
		GList *amenu, *alist;
		for (amenu = (GTK_MENU_SHELL(menu)->children); amenu; ) {
			alist = amenu->next;
			gtk_widget_destroy(GTK_WIDGET(amenu->data));
			amenu = alist;
		}
	}
	
	dictname = g_strdup_printf(_("Dictionary: %s"), gtkpspeller->dictionary->dictname);
	item = gtk_menu_item_new_with_label(dictname);
	gtk_misc_set_alignment(GTK_MISC(GTK_BIN(item)->child), 0.5, 0.5);
	g_free(dictname);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
		
      	item = gtk_check_menu_item_new_with_label(_("Fast Mode"));
	if (ispell || gtkpspell->gtkpspeller->sug_mode == PSPELL_FASTMODE)
		gtk_widget_set_sensitive(GTK_WIDGET(item),FALSE);
	if (!ispell && gtkpspell->gtkpspeller->sug_mode == PSPELL_FASTMODE)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
	else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkpspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_check_menu_item_new_with_label(_("Normal Mode"));
	if (ispell || gtkpspell->gtkpspeller->sug_mode == PSPELL_NORMALMODE)
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
	if (!ispell && gtkpspell->gtkpspeller->sug_mode == PSPELL_NORMALMODE) 
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkpspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu),item);

	item = gtk_check_menu_item_new_with_label(_("Bad Spellers Mode"));
	if (ispell || gtkpspell->gtkpspeller->sug_mode == PSPELL_BADSPELLERMODE)
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
	if (!ispell && gtkpspell->gtkpspeller->sug_mode == PSPELL_BADSPELLERMODE)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode_cb),
				   gtkpspell);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
	
	item = gtk_check_menu_item_new_with_label(_("Check while typing"));
	if (gtkpspell->check_while_typing)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	else	
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
	gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(toggle_check_while_typing_cb),
			   gtkpspell);
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
        if (gtkpspellcheckers->dictionary_list == NULL)
		gtkpspell_get_dictionary_list(prefs_common.pspell_path, FALSE);
        {
		GtkWidget * curmenu = submenu;
		int count = 0;
		Dictionary *dict;
		GSList *tmp;
		tmp = gtkpspellcheckers->dictionary_list;
		
		for (tmp = gtkpspellcheckers->dictionary_list; tmp != NULL; 
				tmp = g_slist_next(tmp)) {
			dict = (Dictionary *) tmp->data;
			item = gtk_check_menu_item_new_with_label(dict->dictname);
			gtk_object_set_data(GTK_OBJECT(item), "dict_name", dict->fullname); 
			if (strcmp2(dict->fullname, gtkpspell->gtkpspeller->dictionary->fullname))
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
			else {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
			}
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(change_dict_cb),
					   gtkpspell);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			
			count++;
			
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
		}
        }  

        
	

}

static GtkMenu *make_config_menu(GtkPspell *gtkpspell)
{
	if (!gtkpspell->popup_config_menu)
		gtkpspell->popup_config_menu = gtk_menu_new();

	debug_print("Pspell: creating/using popup_config_menu %0x\n", 
			gtkpspell->popup_config_menu);
	populate_submenu(gtkpspell, gtkpspell->popup_config_menu);

        return GTK_MENU(gtkpspell->popup_config_menu);
}

void gtkpspell_populate_submenu(GtkPspell *gtkpspell, GtkWidget *menuitem)
{
	GtkWidget *menu;

	menu = GTK_WIDGET(GTK_MENU_ITEM(menuitem)->submenu);
	
	debug_print("Pspell: using config menu %0x\n", 
			gtkpspell->popup_config_menu);
	populate_submenu(gtkpspell, menu);
	
	gtkpspell->config_menu = menu;
	
}

static void set_menu_pos(GtkMenu *menu, gint *x, gint *y, gpointer data)
{
	GtkPspell 	*gtkpspell = (GtkPspell *) data;
	gint 		 xx = 0, yy = 0;
	gint 		 sx,     sy;
	gint 		 wx,     wy;
	GtkSText 	*text = GTK_STEXT(gtkpspell->gtktext);
	GtkRequisition 	 r;

	gdk_window_get_origin(GTK_WIDGET(gtkpspell->gtktext)->window, &xx, &yy);
	
	sx = gdk_screen_width();
	sy = gdk_screen_height();
	
	gtk_widget_get_child_requisition(GTK_WIDGET(menu), &r);
	
	wx =  r.width;
	wy =  r.height;
	
	*x = gtkpspell->gtktext->cursor_pos_x + xx +
	     gdk_char_width(GTK_WIDGET(text)->style->font, ' ');
	*y = gtkpspell->gtktext->cursor_pos_y + yy;

	if (*x + wx > sx)
		*x = sx - wx;
	if (*y + wy > sy)
		*y = *y - wy - 
		     gdk_string_height((GTK_WIDGET(gtkpspell->gtktext))->style->font, 
				       gtkpspell->theword);

}

/*************************************************************************/
/* Menu call backs */

static gboolean cancel_menu_cb(GtkMenuShell *w, gpointer data)
{
	GtkPspell *gtkpspell = (GtkPspell *) data;
	gtkpspell->continue_check = NULL;
	set_point_continue(gtkpspell);
	return FALSE;
	
}
/* change_dict_cb() - Menu callback : change dict */
static void change_dict_cb(GtkWidget *w, GtkPspell *gtkpspell)
{
	Dictionary 	*dict;       
	gchar		*fullname;
	GtkPspeller 	*gtkpspeller;
	gint 		 sug_mode;
  
        fullname = (gchar *) gtk_object_get_data(GTK_OBJECT(w), "dict_name");
	
	if (!strcmp2(fullname, _("None")))
			return;

	sug_mode  = gtkpspell->default_sug_mode;

	dict = g_new0(Dictionary, 1);
	dict->fullname = g_strdup(fullname);
	dict->encoding = g_strdup(gtkpspell->gtkpspeller->dictionary->encoding);
	
	gtkpspeller = gtkpspeller_new(dict);

	if (!gtkpspeller) {
		gchar *message;
		message = g_strdup_printf(_("The spell checker could not change dictionary.\n%s"), 
					  gtkpspellcheckers->error_message);

		gtkpspell_alert_dialog(message); 
		g_free(message);
	} else {
		gtkpspeller_delete(gtkpspell->gtkpspeller);
		gtkpspell->gtkpspeller = gtkpspeller;
		gtkpspell_set_sug_mode(gtkpspell, sug_mode);
	}
	
	dictionary_delete(dict);

	if (gtkpspell->config_menu)
		populate_submenu(gtkpspell, gtkpspell->config_menu);

}

/******************************************************************************/
/* Misc. helper functions */

static void set_point_continue(GtkPspell *gtkpspell)
{
	GtkSText  *gtktext;

	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext),gtkpspell->orig_pos);
	gtk_stext_set_point(gtktext, gtkpspell->orig_pos);
	gtk_stext_thaw(gtktext);

	if (gtkpspell->continue_check)
		gtkpspell->continue_check((gpointer *) gtkpspell);
}

static void allocate_color(GtkPspell *gtkpspell, gint rgbvalue)
{
	GdkColormap *gc;
	GdkColor *color = &(gtkpspell->highlight);

	gc = gtk_widget_get_colormap(GTK_WIDGET(gtkpspell->gtktext));

	if (gtkpspell->highlight.pixel)
		gdk_colormap_free_colors(gc, &(gtkpspell->highlight), 1);

	/* Shameless copy from Sylpheed's gtkutils.c */
	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);

	gdk_colormap_alloc_color(gc, &(gtkpspell->highlight), FALSE, TRUE);
}

static void change_color(GtkPspell * gtkpspell, 
			 gint start, gint end,
			 gchar *newtext,
                         GdkColor *color) 
{
	GtkSText *gtktext;

	g_return_if_fail(start < end);
    
	gtktext = gtkpspell->gtktext;
    
	gtk_stext_freeze(gtktext);
	if (newtext) {
		gtk_stext_set_point(gtktext, start);
		gtk_stext_forward_delete(gtktext, end - start);
		gtk_stext_insert(gtktext, NULL, color, NULL, newtext, end - start);
	}
	gtk_stext_thaw(gtktext);
}

/* convert_to_pspell_encoding () - converts ISO-8859-* strings to iso8859-* 
 * as needed by pspell. Returns an allocated string.
 */

static guchar *convert_to_pspell_encoding (const guchar *encoding)
{
	guchar * pspell_encoding;

	if (strstr2(encoding, "ISO-8859-")) {
		pspell_encoding = g_strdup_printf("iso8859%s", encoding+8);
	}
	else
		if (!strcmp2(encoding, "US-ASCII"))
			pspell_encoding = g_strdup("iso8859-1");
		else
			pspell_encoding = g_strdup(encoding);

	return pspell_encoding;
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

static void free_suggestions_list(GtkPspell *gtkpspell)
{
	GList *list;

	for (list = gtkpspell->suggestions_list; list != NULL; list = list->next)
		g_free(list->data);
	g_list_free(list);
	
	gtkpspell->max_sug          = -1;
	gtkpspell->suggestions_list = NULL;
 
}

static void reset_theword_data(GtkPspell *gtkpspell)
{
	gtkpspell->start_pos     =  0;
	gtkpspell->end_pos       =  0;
	gtkpspell->theword[0]    =  0;
	gtkpspell->max_sug       = -1;

	free_suggestions_list(gtkpspell);
}

static void free_checkers(gpointer elt, gpointer data)
{
	GtkPspeller *gtkpspeller = elt;

	g_return_if_fail(gtkpspeller);

	gtkpspeller_real_delete(gtkpspeller);
}

static gint find_gtkpspeller(gconstpointer aa, gconstpointer bb)
{
	Dictionary *a = ((GtkPspeller *) aa)->dictionary;
	Dictionary *b = ((GtkPspeller *) bb)->dictionary;

	if (a && b && a->fullname && b->fullname  &&
	    strcmp(a->fullname, b->fullname) == 0 &&
	    a->encoding && b->encoding)
		return strcmp(a->encoding, b->encoding);

	return 1;
}

static void gtkpspell_alert_dialog(gchar *message)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *ok_button;

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_MOUSE);
	gtk_signal_connect_object(GTK_OBJECT(dialog), "destroy",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				   GTK_OBJECT(dialog));

	label  = gtk_label_new(message);
	gtk_misc_set_padding(GTK_MISC(label), 8, 8);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	
	hbox = gtk_hbox_new(FALSE, 0);

	ok_button = gtk_button_new_with_label(_("OK"));
	GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbox), ok_button, TRUE, TRUE, 8);	

	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				   GTK_OBJECT(dialog));
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), hbox);
			
	gtk_widget_grab_default(ok_button);
	gtk_widget_grab_focus(ok_button);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	gtk_widget_show_all(dialog);
}
#endif
