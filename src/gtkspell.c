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
static void 		set_sug_mode_cb     		(GtkWidget *w, 
							 GtkPspell *gtkpspell);
static void 		set_real_sug_mode		(GtkPspell *gtkpspell, 
							 const char *themode);

/* Checker actions */
static gboolean 	check_at			(GtkPspell *gtkpspell, 
							 int from_pos);
static void		check_next_prev			(GtkPspell *gtkpspell, 
							 gboolean forward);

static GList*		misspelled_suggest	 	(GtkPspell *gtkpspell, 
							 guchar *word);
static void 		add_word_to_session		(GtkWidget *w, 
							 GtkPspell *d);
static void 		add_word_to_personal		(GtkWidget *w, 
							 GtkPspell *d);
static void 		replace_word			(GtkWidget *w, 
							 GtkPspell *gtkpspell); 
static void 		replace_real_word		(GtkPspell *gtkpspell, 
							 gchar *newword);
static void 		check_all_cb			(GtkWidget *w, 
							 GtkPspell *gtkpspell);

/* Menu creation */
static void 		popup_menu			(GtkPspell *gtkpspell, 
							 GdkEventButton *eb);
static GtkMenu*		make_sug_menu			(GtkPspell *gtkpspell);
static GtkMenu*		make_config_menu		(GtkPspell *gtkpspell);
static void 		set_menu_pos			(GtkMenu *menu, 
							 gint *x, 
							 gint *y, 
							 gpointer data);
/* Other menu callbacks */
static gboolean 	deactivate_menu_cb		(GtkWidget *w, 
							 gpointer *data);
static void 		change_dict_cb			(GtkWidget *w, 
							 GtkPspell *gtkpspell);

/* Misc. helper functions */
static gboolean 	iswordsep			(unsigned char c);
static guchar 		get_text_index_whar		(GtkPspell *gtkpspell, 
							 int pos);
static gboolean 	get_word_from_pos		(GtkPspell *gtkpspell, 
							 int pos, 
							 unsigned char* buf,
							 int *pstart, int *pend);
static void 		allocate_color			(GtkPspell *gtkpspell);
static void 		change_color			(GtkPspell *gtkpspell, 
			 				 int start, 
							 int end, 
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

void gtkpspell_checkers_reset()
{
	g_return_if_fail(gtkpspellcheckers);
	
	g_free(gtkpspellcheckers->error_message);
	
	gtkpspellcheckers->error_message = NULL;
}

GtkPspell *gtkpspell_new(const gchar *dictionary, 
			 const gchar *encoding, 
			 GtkSText *gtktext)
{
	Dictionary 	*dict = g_new0(Dictionary, 1);
	GtkPspell 	*gtkpspell;
	GtkPspeller 	*gtkpspeller;

	g_return_val_if_fail(gtktext, NULL);
	
	dict->fullname = g_strdup(dictionary);
	dict->encoding = g_strdup(encoding);
	gtkpspeller = gtkpspeller_new(dict); 
	dictionary_delete(dict);

	if (!gtkpspeller)
		return NULL;
	
	gtkpspell = g_new(GtkPspell, 1);

	gtkpspell->gtkpspeller = gtkpspeller;
	gtkpspell->theword[0]  = 0x00;
	gtkpspell->start_pos   = 0;
	gtkpspell->end_pos     = 0;
	gtkpspell->orig_pos    = -1;

	gtkpspell->gtktext     = gtktext;

	gtkpspell->default_sug_mode   = PSPELL_FASTMODE;
	gtkpspell->max_sug            = -1;
	gtkpspell->suggestions_list   = NULL;

	gtk_signal_connect_after(GTK_OBJECT(gtktext), "insert-text",
		                 GTK_SIGNAL_FUNC(entry_insert_cb), gtkpspell);

	gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
		                 GTK_SIGNAL_FUNC(entry_delete_cb), gtkpspell);
	
	gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
		           GTK_SIGNAL_FUNC(button_press_intercept_cb), gtkpspell);
	
	allocate_color(gtkpspell);
	
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

	g_free(gtkpspell);
	gtkpspell = NULL;
}

static void entry_insert_cb(GtkSText *gtktext, gchar *newtext, 
			    guint len, guint *ppos, 
                            GtkPspell * gtkpspell) 
{
	g_return_if_fail(gtkpspell->gtkpspeller->checker);
			
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
	GSList 		*checkers, *exist;
	GtkPspeller	*gtkpspeller = NULL;
	GtkPspeller	*g           = g_new(GtkPspeller, 1);
	Dictionary	*dict;
	gint		ispell;
	gboolean	free_dict = TRUE;
		
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

	checkers = gtkpspellcheckers->checkers;

	exist = g_slist_find_custom(checkers, g, find_gtkpspeller);
	
	g_free(g);

	if (exist)
		gtkpspeller = (GtkPspeller *) exist->data;

	if (exist && ispell)
		debug_print(_("Pspell: Using existing ispell checker %0x\n"),
			    (gint) gtkpspeller);
	else {
		if ((gtkpspeller = gtkpspeller_real_new(dict)) != NULL) {
			checkers = g_slist_append(checkers, gtkpspeller);
			
			debug_print(_("Pspell: Created a new gtkpspeller %0x\n"),
				    (gint) gtkpspeller);
			
			free_dict = FALSE;
		} else 
			debug_print(_("Pspell: Could not create spell checker.\n"));
	}

	debug_print(_("Pspell: number of existing checkers %d\n"), 
		    g_slist_length(checkers));

	if (free_dict)
		dictionary_delete(dict);

	gtkpspellcheckers->checkers = checkers;

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
	GSList *checkers;

	g_return_val_if_fail(gtkpspellcheckers, NULL);
	
	checkers = gtkpspellcheckers->checkers;
	
	if (gtkpspeller->ispell) 
		debug_print(_("Pspell: Won't remove existing ispell checker %0x.\n"), 
			    (gint) gtkpspeller);
	else {
		checkers = g_slist_remove(checkers, gtkpspeller);

		gtkpspellcheckers->checkers = checkers;
		
		debug_print(_("Pspell: Deleting gtkpspeller %0x.\n"), 
			    (gint) gtkpspeller);
		
		gtkpspeller_real_delete(gtkpspeller);
	}

	debug_print(_("Pspell: number of existing checkers %d\n"), 
		    g_slist_length(checkers));

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
	debug_print(_("Pspell: removing all paths.\n"));

	CONFIG_REPLACE_RETURN_FALSE_IF_FAIL("add-word-list-path", buf);
	debug_print(_("Pspell: adding path (%s).\n"), buf);

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
static void set_sug_mode_cb(GtkWidget *w, GtkPspell *gtkpspell)
{
	unsigned char *themode;
	
	if (gtkpspell->gtkpspeller->ispell) return;

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar **) &themode);
	
	set_real_sug_mode(gtkpspell, themode);

}
static void set_real_sug_mode(GtkPspell *gtkpspell, const char *themode)
{
	gint result;
	g_return_if_fail(gtkpspell);
	g_return_if_fail(gtkpspell->gtkpspeller);
	g_return_if_fail(themode);

	if (!strcmp(themode, _("Fast Mode"))) {
		result = gtkpspell_set_sug_mode(gtkpspell, PSPELL_FASTMODE);
	} else if (!strcmp(themode,_("Normal Mode"))) {
		result = gtkpspell_set_sug_mode(gtkpspell, PSPELL_NORMALMODE);
	} else if (!strcmp( themode,_("Bad Spellers Mode"))) {
		result = gtkpspell_set_sug_mode(gtkpspell, PSPELL_BADSPELLERMODE);
	}

	if(!result) {
		debug_print(_("Pspell: error while changing suggestion mode:%s\n"),
			    gtkpspellcheckers->error_message);
		gtkpspell_checkers_reset();
	}
}
  
/* gtkpspell_set_sug_mode() - Set the suggestion mode */
gboolean gtkpspell_set_sug_mode(GtkPspell *gtkpspell, gint themode)
{
	PspellConfig *config;
	g_return_val_if_fail(gtkpspell, FALSE);
	g_return_val_if_fail(gtkpspell->gtkpspeller, FALSE);
	g_return_val_if_fail(gtkpspell->gtkpspeller->config, FALSE);

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
static gboolean get_word_from_pos(GtkPspell *gtkpspell, int pos, 
                                  unsigned char* buf,
                                  int *pstart, int *pend) 
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
						
	if (buf) {
		for (pos = start; pos < end; pos++) 
			buf[pos - start] = get_text_index_whar(gtkpspell, pos);
		buf[pos - start] = 0;
	}

	if (pstart) 
		*pstart = start;
	if (pend) 
		*pend = end;

	return TRUE;
}

static gboolean check_at(GtkPspell *gtkpspell, int from_pos) 
{
	int	      start, end;
	unsigned char buf[BUFSIZE];
	GtkSText     *gtktext;

	g_return_val_if_fail(from_pos >= 0, FALSE);
    
	gtktext = gtkpspell->gtktext;

	if (!get_word_from_pos(gtkpspell, from_pos, buf, &start, &end))
		return FALSE;


	if (misspelled_test(gtkpspell, buf)) {
		strncpy(gtkpspell->theword, buf, GTKPSPELLWORDSIZE - 1);
		gtkpspell->theword[GTKPSPELLWORDSIZE - 1] = 0;
		gtkpspell->start_pos  = start;
		gtkpspell->end_pos    = end;
		gtkpspell->newword[0] = 0;
		free_suggestions_list(gtkpspell);

		change_color(gtkpspell, start, end, &(gtkpspell->highlight));
		
		return TRUE;
	} else {
		change_color(gtkpspell, start, end, NULL);
		return FALSE;
	}
}

static void check_all_cb(GtkWidget *w, GtkPspell *gtkpspell)
{
	gtkpspell_check_all(gtkpspell);
}

void gtkpspell_check_all(GtkPspell *gtkpspell) 
{
	guint     origpos;
	guint     pos = 0;
	guint     len;
	float     adj_value;
	GtkSText *gtktext;

	g_return_if_fail(gtkpspell->gtkpspeller->checker);	

	gtktext = gtkpspell->gtktext;

	len = gtk_stext_get_length(gtktext);

	adj_value = gtktext->vadj->value;
	gtk_stext_freeze(gtktext);
	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	gtk_editable_set_position(GTK_EDITABLE(gtktext),0);
	while (pos < len) {
		while (pos < len && iswordsep(get_text_index_whar(gtkpspell, pos)))
			pos++;
		while (pos < len && !iswordsep(get_text_index_whar(gtkpspell, pos)))
			pos++;
		if (pos > 0)
			check_at(gtkpspell, pos - 1);
	}
	gtk_stext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
}

static void check_next_prev(GtkPspell *gtkpspell, gboolean forward)
{
	gint pos;
	gint minpos;
	gint maxpos;
	gint direc = -1;
	gboolean misspelled;
	GdkEvent *event= (GdkEvent *) gtk_get_current_event();
	
	minpos = 0;
	maxpos = gtk_stext_get_length(gtkpspell->gtktext);

	if (forward) {
		minpos = -1;
		direc = -direc;
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
				set_menu_pos, gtkpspell, 0, gdk_event_get_time(event));
	} else {
		reset_theword_data(gtkpspell);

		gtk_stext_set_point(GTK_STEXT(gtkpspell->gtktext), gtkpspell->orig_pos);
		gtk_editable_set_position(GTK_EDITABLE(gtkpspell->gtktext), gtkpspell->orig_pos);
	}
}

void gtkpspell_check_backwards(GtkPspell *gtkpspell)
{
	check_next_prev(gtkpspell, FALSE);
}

void gtkpspell_check_forwards_go(GtkPspell *gtkpspell)
{
	check_next_prev(gtkpspell, TRUE);
}


static void replace_word(GtkWidget *w, GtkPspell *gtkpspell) 
{
	unsigned char *newword;
	GdkEvent *e= (GdkEvent *) gtk_get_current_event();

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar**) &newword);
	replace_real_word(gtkpspell, newword);
	if ((e->type == GDK_KEY_PRESS && ((GdkEventKey *) e)->state & GDK_MOD1_MASK) ||
	    (e->type == GDK_BUTTON_RELEASE && ((GdkEventButton *) e)->state & GDK_MOD1_MASK)) {
		pspell_manager_store_replacement(gtkpspell->gtkpspeller->checker, 
						 gtkpspell->theword, -1, 
						 newword, -1);
	}
	gtkpspell->newword[0] = 0x00;
	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));
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

	strncpy(gtkpspell->newword, newword, GTKPSPELLWORDSIZE - 1);
	gtkpspell->newword[GTKPSPELLWORDSIZE-1] = 0x00;
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
    
	if (origpos - gtkpspell->start_pos < oldlen && origpos - gtkpspell->start_pos >= 0) {
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

static void add_word_to_session(GtkWidget *w, GtkPspell *gtkpspell)
{
	guint     pos;
	GtkSText *gtktext;
    
	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));

	pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    
	pspell_manager_add_to_session(gtkpspell->gtkpspeller->checker,gtkpspell->theword, strlen(gtkpspell->theword));

	check_at(gtkpspell, gtkpspell->start_pos);

	gtk_stext_thaw(gtkpspell->gtktext);
	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));
}

/* add_word_to_personal() - add word to personal dict. */
static void add_word_to_personal(GtkWidget *w, GtkPspell *gtkpspell)
{
	GtkSText *gtktext;
    
	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(GTK_STEXT(gtktext));
    
	pspell_manager_add_to_personal(gtkpspell->gtkpspeller->checker,gtkpspell->theword, strlen(gtkpspell->theword));
    
	check_at(gtkpspell, gtkpspell->start_pos);

	gtk_stext_thaw(gtkpspell->gtktext);
	gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->parent));
}

void gtkpspell_uncheck_all(GtkPspell * gtkpspell) 
{
	int origpos;
	unsigned char *text;
	float adj_value;
	GtkSText *gtktext;
	
	gtktext=gtkpspell->gtktext;

	adj_value = gtktext->vadj->value;
	gtk_stext_freeze(gtktext);
	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);
	gtk_stext_set_point(gtktext, 0);
	gtk_stext_forward_delete(gtktext, gtk_stext_get_length(gtktext));
	gtk_stext_insert(gtktext, NULL, NULL, NULL, text, strlen(text));
	gtk_stext_thaw(gtktext);

	g_free(text);

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);
}

static GSList *create_empty_dictionary_list(void)
{
	GSList *list = NULL;
	Dictionary *dict;

	dict = g_new0(Dictionary, 1);/*printf("N %08x            dictionary\n", dict);*/
	dict->fullname = g_strdup(_("None"));/*printf("N %08x            fullname\n", dict->fullname);*/
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
	prevdir = g_get_current_dir();
	if (chdir(dict_path) <0) {
		debug_print(_("Error when searching for dictionaries:\n%s\n"),
					  g_strerror(errno));
		g_free(prevdir);
		g_free(dict_path);
		gtkpspellcheckers->dictionary_list = create_empty_dictionary_list();
		return gtkpspellcheckers->dictionary_list; 
	}

	debug_print(_("Checking for dictionaries in %s\n"), dict_path);

	if (NULL != (dir = opendir("."))) {
		while (NULL != (ent = readdir(dir))) {
			/* search for pwli */
			if ((NULL != (tmp = strstr2(ent->d_name, ".pwli"))) && (tmp[5] == 0x00)) {
				g_snprintf(tmpname, BUFSIZE, "%s%s", G_DIR_SEPARATOR_S, ent->d_name);
				tmpname[MIN(tmp - ent->d_name + 1, BUFSIZE-1)] = 0x00;
				dict = g_new0(Dictionary, 1);/*printf("N %08x            dictionary\n", dict);*/
				dict->fullname = g_strdup_printf("%s%s", dict_path, tmpname);/*printf("N %08x            fullname\n", dict->fullname);*/
				dict->dictname = strrchr(dict->fullname, G_DIR_SEPARATOR) + 1;
				dict->encoding = NULL;
				debug_print(_("Found dictionary %s %s\n"), dict->fullname, dict->dictname);
				list = g_slist_insert_sorted(list, dict, (GCompareFunc) compare_dict);
			}
		}			
		closedir(dir);
	}
	else {
		debug_print(_("Error when searching for dictionaries.\nNo dictionary found.\n(%s)"), 
					  g_strerror(errno));
		list = create_empty_dictionary_list();
	}
        if(list==NULL){
		
		debug_print(_("Error when searching for dictionaries.\nNo dictionary found."));
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
		if (dict->dictname)
			gtk_object_set_data(GTK_OBJECT(item), "dict_name",
					 dict); 
					 
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	return menu;
}

gchar *gtkpspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	Dictionary *result;
	gchar *label;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
        result = (Dictionary *) gtk_object_get_data(GTK_OBJECT(menuitem), "dict_name");
        g_return_val_if_fail(result->fullname, NULL);
	label = g_strdup(result->fullname);
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
					       eb->button, eb->time);
				
				return;
			}
		} else {
			gtk_editable_set_position(GTK_EDITABLE(gtktext), 
						  gtkpspell->orig_pos);
			gtk_stext_set_point(gtktext, gtkpspell->orig_pos);
		}
	}

	gtk_menu_popup(make_config_menu(gtkpspell),NULL,NULL,NULL,NULL,
		       eb->button,eb->time);
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
	
	gtk_signal_connect(GTK_OBJECT(menu), "deactivate",
		GTK_SIGNAL_FUNC(deactivate_menu_cb), gtkpspell);

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
			   GTK_SIGNAL_FUNC(add_word_to_session), 
			   gtkpspell);
	gtk_accel_group_add(accel, GDK_space, GDK_MOD1_MASK, 
			    GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE, 
			    GTK_OBJECT(item), "activate");

	item = gtk_menu_item_new_with_label(_("Add to personal dictionary"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_personal), 
			   gtkpspell);
	gtk_accel_group_add(accel, GDK_Return, GDK_MOD1_MASK, 
			    GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE, 
			    GTK_OBJECT(item), "activate");
	
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
					   GTK_SIGNAL_FUNC(replace_word), gtkpspell);
			if (count <= MENUCOUNT) {
				gtk_accel_group_add(accel, 'a' + count, 0, 
					GTK_ACCEL_LOCKED | GTK_ACCEL_VISIBLE, 
					GTK_OBJECT(item), "activate");
				gtk_accel_group_add(accel, 'a' + count, 
					GDK_MOD1_MASK, GTK_ACCEL_LOCKED, 
					GTK_OBJECT(item), "activate");
				}


			count++;
		} while ((l = l->next) != NULL);
	}
	
	gtk_accel_group_attach(accel, GTK_OBJECT(menu));
	
	return GTK_MENU(menu);
}

static GtkMenu *make_config_menu(GtkPspell *gtkpspell)
{
	GtkWidget *menu, *item, *submenu;
	gint ispell = gtkpspell->gtkpspeller->ispell;
  
	menu = gtk_menu_new();

        item = gtk_menu_item_new_with_label(_("Spell check all"));
        gtk_signal_connect(GTK_OBJECT(item),"activate",
			   GTK_SIGNAL_FUNC(check_all_cb), 
			   gtkpspell);
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);


        item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu),item);

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
			gtk_object_set_data(GTK_OBJECT(item), "dict_name", dict); 
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

        return GTK_MENU(menu);
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

static gboolean deactivate_menu_cb(GtkWidget *w, gpointer *data)
{
	GtkPspell *gtkpspell = (GtkPspell *) data;
	GtkSText *gtktext;
	gtktext = gtkpspell->gtktext;

	gtk_stext_freeze(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext),gtkpspell->orig_pos);
	gtk_stext_set_point(gtktext, gtkpspell->orig_pos);
	gtk_stext_thaw(gtktext);

	return FALSE;
}

/* change_dict_cb() - Menu callback : change dict */
static void change_dict_cb(GtkWidget *w, GtkPspell *gtkpspell)
{
	Dictionary 	*dict, *dict2;       
	GtkPspeller 	*gtkpspeller;
	gint 		 sug_mode;
  
	/* Dict is simply the menu label */

        dict = (Dictionary *) gtk_object_get_data(GTK_OBJECT(w), "dict_name");
	
	if (!strcmp2(dict->fullname, _("None")))
			return;

	sug_mode  = gtkpspell->default_sug_mode;

	dict2 = dictionary_dup(dict);
	if (dict2->encoding)
		g_free(dict2->encoding);
	dict2->encoding = g_strdup(gtkpspell->gtkpspeller->dictionary->encoding);
	
	gtkpspeller = gtkpspeller_new(dict2);

	if (!gtkpspeller) {
		gchar *message;
		message = g_strdup_printf(_("The spell checker could not change dictionary.\n%s\n"), 
					  gtkpspellcheckers->error_message);

		gtkpspell_alert_dialog(message); 
		g_free(message);
	} else {
		gtkpspeller_delete(gtkpspell->gtkpspeller);
		gtkpspell->gtkpspeller = gtkpspeller;
		gtkpspell_set_sug_mode(gtkpspell, sug_mode);
	}
	
	dictionary_delete(dict2);
}

/******************************************************************************/
/* Misc. helper functions */

static void allocate_color(GtkPspell *gtkpspell)
{
	GdkColormap *gc;
	GdkColor *color = &(gtkpspell->highlight);
	gint rgbvalue 	= prefs_common.misspelled_col;

	gc = gtk_widget_get_colormap(GTK_WIDGET(gtkpspell->gtktext));

	if (gtkpspell->highlight.pixel)
		gdk_colormap_free_colors(gc, &(gtkpspell->highlight), 1);

	/* Shameless copy from Sylpheed gtkutils.c */
	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);

	gdk_colormap_alloc_color(gc, &(gtkpspell->highlight), FALSE, TRUE);
}

static void change_color(GtkPspell * gtkpspell, 
			 int start, int end, 
                         GdkColor *color) 
{
	char	 *newtext;
	GtkSText *gtktext;

	g_return_if_fail(start < end);
    
	gtktext = gtkpspell->gtktext;
    
	gtk_stext_freeze(gtktext);
	newtext = gtk_editable_get_chars(GTK_EDITABLE(gtktext), start, end);
	if (newtext) {
		gtk_stext_set_point(gtktext, start);
		gtk_stext_forward_delete(gtktext, end - start);

		gtk_stext_insert(gtktext, NULL, color, NULL, newtext, end - start);
		g_free(newtext);
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
	gtkpspell->newword[0]    =  0;

	free_suggestions_list(gtkpspell);
}

static void free_checkers(gpointer elt, gpointer data)
{
	GtkPspeller *gtkpspeller = elt;

	g_return_if_fail(gtkpspeller);

	gtkpspeller_delete(gtkpspeller);
}

static gint find_gtkpspeller(gconstpointer aa, gconstpointer bb)
{
	Dictionary *a = ((GtkPspeller *) aa)->dictionary;
	Dictionary *b = ((GtkPspeller *) bb)->dictionary;

	if (!a || !b)
		return 1;

	if (strcmp(a->fullname, b->fullname) == 0
	    && a->encoding && b->encoding)
		return strcmp(a->encoding, b->encoding);

	return 1;
}

static void gtkpspell_alert_dialog(gchar *message)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *ok_button;

	dialog = gtk_dialog_new();
	label  = gtk_label_new(message);
	ok_button = gtk_button_new_with_label(_("OK"));

	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog),GTK_WIN_POS_MOUSE);
	GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
	gtk_signal_connect_object(GTK_OBJECT(ok_button), "clicked",
				   GTK_SIGNAL_FUNC(gtk_widget_destroy), 
				   GTK_OBJECT(dialog));

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), ok_button);
			
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 8);
	gtk_widget_grab_default(ok_button);
	gtk_widget_grab_focus(ok_button);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_widget_show_all(dialog);
}
#endif
