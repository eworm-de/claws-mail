/* gtkpspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

/*
 * Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 * Adapted for Sylpheed (Claws) (c) 2001 by Hiroyuki Yamamoto & 
 * The Sylpheed Claws Team.
 * Adapted for pspell (c) 2001 Melvin Hadasht
 */
 
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if USE_PSPELL
#include "intl.h"

#include <gtk/gtk.h>
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

#include "gtkxtext.h"

#include "gtkspell.h"

#include <pspell/pspell.h>

/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024

/* number of suggestions to display on each menu. */
#define MENUCOUNT 15

/******************************************************************************/

/* Function called from menus */

static void add_word_to_session		(GtkWidget *w, GtkPspell *d);
static void add_word_to_personal	(GtkWidget *w, GtkPspell *d);
static void set_sug_mode		(GtkWidget *w, GtkPspell *gtkpspell);
static void set_learn_mode		(GtkWidget *w, GtkPspell *gtkpspell);
static void check_all			(GtkWidget *w, GtkPspell *gtkpspell);
static void menu_change_dict		(GtkWidget *w, GtkPspell *gtkpspell);
static void entry_insert_cb		(GtkXText *gtktext, gchar *newtext, 
					 guint len, guint *ppos, 
					 GtkPspell *gtkpspell);
static gint compare_dict		(Dictionary *a, Dictionary *b);


/* gtkspellconfig - only one config per session */
GtkPspellConfig * gtkpspellconfig;

/* TODO: configurable */
static GdkColor highlight = { 0, 255 * 256, 0, 0 };

/******************************************************************************/

/* gtkspell_init() - run the first pspell_config from which every
 * new config is cloned 
 */
GtkPspellConfig * gtkpspell_init()
{
	return new_pspell_config();
}

/* gtkspell_finished() - Finish all. No more spelling. Called when the 
 * program ends 
 */
void gtkpspell_finished(GtkPspellConfig *gtkpspellconfig)
{
	if (gtkpspellconfig) {
		delete_pspell_config(gtkpspellconfig);
		gtkpspellconfig = NULL;
	}
}

/* gtkspell_running - Test if there is a manager running 
 */
int gtkpspell_running(GtkPspell * gtkpspell) 
{
	return (gtkpspell->config!=NULL);
}

/* gtkspell_new() - creates a new session if a gtkpspellconfig exists. The 
 * settings are defaults.  If no path/dict is set afterwards, the default 
 * one is used  
 */
GtkPspell * gtkpspell_new(GtkPspellConfig *gtkpspellconfig)
{
	GtkPspell *gtkpspell;

	if (gtkpspellconfig == NULL) {
		gtkpspellconfig = gtkpspell_init();
		if (gtkpspellconfig == NULL) {
			debug_print(_("Pspell could not be started."));
			prefs_common.enable_pspell=FALSE;
			return NULL;
		}
	}

	gtkpspell               = g_new(GtkPspell ,1);
	gtkpspell->config       = gtkpspellconfig;
	gtkpspell->possible_err = new_pspell_manager(gtkpspell->config);
	gtkpspell->checker      = NULL;

	if (pspell_error_number(gtkpspell->possible_err) != 0) {
		debug_print(_("Pspell error : %s\n"), pspell_error_message(gtkpspell->possible_err));
		delete_pspell_can_have_error( gtkpspell->possible_err );
	}
	else {
		gtkpspell->checker = to_pspell_manager(gtkpspell->possible_err);
	}
 
	gtkpspell->dictionary_list = NULL;
	gtkpspell->path            = NULL;
	gtkpspell->dict            = NULL;
	gtkpspell->mode            = PSPELL_FASTMODE;
	gtkpspell->learn	   = TRUE;
	gtkpspell->gtktext         = NULL;
	return gtkpspell;
}

/* gtkspell_new_with_config() - Creates a new session and set path/dic 
 */
GtkPspell *gtkpspell_new_with_config(GtkPspellConfig *gtkpspellconfig, 
                                     guchar *path, guchar *dict, 
                                     guint mode, guchar *encoding)
{
	GtkPspell *gtkpspell;

	if (gtkpspellconfig == NULL) {
		gtkpspellconfig=gtkpspell_init();
		if (gtkpspellconfig == NULL) {
			debug_print(_("Pspell could not be started."));
			prefs_common.enable_pspell = FALSE;
			return NULL;
		}
	}

	gtkpspell                  = g_new( GtkPspell ,1);
	gtkpspell->path            = NULL;
	gtkpspell->dict            = NULL;
	gtkpspell->dictionary_list = NULL;
	gtkpspell->gtktext         = NULL;
  
	gtkpspell->config	   = pspell_config_clone(gtkpspellconfig);
	gtkpspell->mode		   = PSPELL_FASTMODE;
	gtkpspell->learn	   = TRUE;
	
	if (!set_path_and_dict(gtkpspell, gtkpspell->config, path, dict)) {
		debug_print(_("Pspell could not be configured."));
		gtkpspell = gtkpspell_delete(gtkpspell);
		return gtkpspell;
	}
	
	if (encoding)
		pspell_config_replace(gtkpspell->config,"encoding",encoding);

	gtkpspell->possible_err = new_pspell_manager(gtkpspell->config);
	gtkpspell->checker      = NULL;

	if (pspell_error_number(gtkpspell->possible_err) != 0) {
		debug_print(_("Pspell error : %s\n"), pspell_error_message(gtkpspell->possible_err));
		delete_pspell_can_have_error(gtkpspell->possible_err);
		gtkpspell = gtkpspell_delete(gtkpspell);
	}
	else {
		gtkpspell->checker = to_pspell_manager( gtkpspell->possible_err );
	}
	return gtkpspell;
}

/* gtkspell_delete() - Finishes a session 
 */
GtkPspell *gtkpspell_delete( GtkPspell *gtkpspell )
{
	if ( gtkpspell->checker ) {
		/* First save all word lists */
		pspell_manager_save_all_word_lists(gtkpspell->checker);
		delete_pspell_manager(gtkpspell->checker);
		gtkpspell->checker      = NULL;
		gtkpspell->possible_err = NULL; /* checker is a cast from possible_err */
	}

	if (gtkpspell->dictionary_list)
		gtkpspell_free_dictionary_list(gtkpspell->dictionary_list);

	g_free(gtkpspell->path);
	g_free(gtkpspell->dict);
	gtkpspell->path = NULL;
	gtkpspell->dict = NULL;

	g_free(gtkpspell);
	return NULL;
}
  
int set_path_and_dict(GtkPspell *gtkpspell, PspellConfig *config,
		      guchar *path, guchar * dict)
{
	guchar *module   = NULL;
	guchar *language = NULL;
	guchar *spelling = NULL;
	guchar *jargon   = NULL;
	guchar  buf[BUFSIZE];
	guchar *end;
	guchar *temppath;
	guchar *tempdict;

	/* Change nothing if any of path/dict/config is NULL */
	g_return_val_if_fail(path, 0);
	g_return_val_if_fail(dict, 0);
	g_return_val_if_fail(config, 0);
	
	/* This is done, so we can free gtkpspell->path, even if it was
	 * given as an argument of the function */
	temppath = g_strdup(path);
	g_free(gtkpspell->path);
  
  	/* pspell dict name format :                         */
	/*   <lang>[[-<spelling>[-<jargon>]]-<module>.pwli   */
	/* Strip off path                                    */
	
	if (!strrchr(dict,G_DIR_SEPARATOR)) {
		/* plain dict name */
		strncpy(buf,dict,BUFSIZE-1);
	}
	else { 
		/* strip path */
		strncpy(buf, strrchr(dict, G_DIR_SEPARATOR)+1, BUFSIZE-1);
	}
	
	g_free(gtkpspell->dict);

	/* Ensure no buffers overflows if the dict is to long */
	buf[BUFSIZE-1] = 0x00;

	language = buf;
	if ((module = strrchr(buf, '-')) != NULL) {
		module++;
		if ((end = strrchr(module, '.')) != NULL)
			end[0] = 0x00;
	}

	/* In tempdict, we have only the dict name, without path nor
	   extension. Useful in the popup menus */

	tempdict = g_strdup(buf);

	/* Probably I am too paranoied... */
	if (!(language[0] != 0x00 && language[1] != 0x00))
		language = NULL;
	else {
		spelling = strchr(language, '-');
		if (spelling != NULL) {
			spelling[0] = 0x00;
			spelling++;
		}
		if (spelling != module) {
			if ((end = strchr(spelling, '-')) != NULL) {
				end[0] = 0x00;
				jargon = end + 1;
				if (jargon != module)
					if ((end = strchr(jargon, '-')) != NULL)
						end[0] = 0x00;
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
	}

	debug_print(_("Language : %s\nSpelling: %s\nJargon: %s\nModule: %s\n"),
		    language, spelling, jargon, module);
	
	if (language) 
		pspell_config_replace(config, "language-tag", language);
	if (spelling) 
		pspell_config_replace(config, "spelling", spelling);
	if (jargon)
		pspell_config_replace(config, "jargon", jargon);
	if (module)
		pspell_config_replace(config, "module", module);
	if (temppath)
		pspell_config_replace(config, "word-list-path", temppath);

	switch(gtkpspell->mode) {
	case PSPELL_FASTMODE: 
		pspell_config_replace(config, "sug_mode", "fast");
		break;
	case PSPELL_NORMALMODE: 
		pspell_config_replace(config, "sug_mode", "normal");
		break;
	case PSPELL_BADSPELLERMODE: 
		pspell_config_replace(config, "sug_mode", "bad-spellers");
		break;
	}
  
	gtkpspell->path = g_strdup(temppath);
	gtkpspell->dict = g_strdup(tempdict);
	g_free(temppath);
	g_free(tempdict);

	return TRUE;
}


/* gtkpspell_set_path_and_dict() - Set path and dict. The session is 
 * resetted. 
 * FALSE on error, TRUE on success */
int gtkpspell_set_path_and_dict(GtkPspell * gtkpspell, guchar * path, 
				guchar * dict)
{
	PspellConfig * config2;

	/* It seems changing an already running config is not the way to go
         */

	config2 = pspell_config_clone(gtkpspell->config);

	if (gtkpspell->checker) {
		pspell_manager_save_all_word_lists(gtkpspell->checker);
		delete_pspell_manager(gtkpspell->checker);
	}
	
	gtkpspell->checker      = NULL;
	gtkpspell->possible_err = NULL;

	if (set_path_and_dict(gtkpspell,config2,path,dict) == 0) {
		debug_print(_("Pspell set_path_and_dict error."));
		return FALSE;
	}
  
	gtkpspell->possible_err = new_pspell_manager(config2);

	delete_pspell_config(config2);
	config2 = NULL;

	if (pspell_error_number(gtkpspell->possible_err) != 0) {
		debug_print(_("Pspell path & dict. error %s\n"),
			    pspell_error_message(gtkpspell->possible_err));
		delete_pspell_can_have_error(gtkpspell->possible_err);
		gtkpspell->possible_err = NULL;
		return FALSE;
	}

	gtkpspell->checker=to_pspell_manager(gtkpspell->possible_err);

	return TRUE;
}
  
/* gtkpspell_get_dict() - What dict are we using ? language-spelling-jargon-module format */
/* Actually, this function is not used and hence not tested. */
/* Returns an allocated string */  
guchar *gtkpspell_get_dict(GtkPspell *gtkpspell)
{
	guchar *dict;
	guchar *language;
	guchar *spelling;
	guchar *jargon;
	guint   len;

	g_return_val_if_fail(gtkpspell->config, NULL);
  
	language = g_strdup(pspell_config_retrieve(gtkpspell->config, "language"));
	spelling = g_strdup(pspell_config_retrieve(gtkpspell->config, "spelling"));
	jargon   = g_strdup(pspell_config_retrieve(gtkpspell->config, "jargon"  ));
	len      = strlen(language) + strlen(spelling) + strlen(jargon);

	if (len < BUFSIZE) {
		dict = g_new(char,len + 4);
		strcpy(dict, language);
		if (spelling) {
			strcat(dict, "-");
			strcat(dict, spelling);
			if (jargon) {
				strcat(dict, "-");
				strcat(dict,jargon);
			}
		}
	}
	g_free(language);
	g_free(spelling);
	g_free(jargon);
  
	return dict;
}
  
/* gtkpspell_get_path() - Return the dict path as an allocated string */
/* Not used = not tested */
guchar *gtkpspell_get_path(GtkPspell *gtkpspell)
{
	guchar * path;


	g_return_val_if_fail(gtkpspell->config, NULL);

	path = g_strdup(pspell_config_retrieve(gtkpspell->config,"word-list-path"));

	return path;
}

/* menu_change_dict() - Menu callback : change dict */
static void menu_change_dict(GtkWidget *w, GtkPspell *gtkpspell)
{
	guchar *thedict,
	       *thelabel;
  
	/* Dict is simply the menu label */

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar **) &thelabel);
	thedict = g_strdup(thelabel);

	/* Set path, dict, (and sug_mode ?) */
	gtkpspell_set_path_and_dict(gtkpspell, gtkpspell->path, thedict);
	g_free(thedict);
}

/* set_sug_mode() - Menu callback : Set the suggestion mode */
static void set_sug_mode(GtkWidget *w, GtkPspell *gtkpspell)
{
	unsigned char *themode;

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar **) &themode);

	if (!strcmp(themode, _("Fast Mode"))) {
		gtkpspell_set_sug_mode(gtkpspell, "fast");
		gtkpspell->mode = PSPELL_FASTMODE;
	}
	if (!strcmp(themode,_("Normal Mode"))) {
		gtkpspell_set_sug_mode(gtkpspell, "normal");
		gtkpspell->mode = PSPELL_NORMALMODE;
	}
	if (!strcmp( themode,_("Bad Spellers Mode"))) {
		gtkpspell_set_sug_mode(gtkpspell, "bad-spellers");
		gtkpspell->mode = PSPELL_BADSPELLERMODE;
	}
}
  
/* gtkpspell_set_sug_mode() - Set the suggestion mode */
/* Actually, the session is resetted and everything is reset. */
/* We take the used path/dict pair and create a new config with them */
int gtkpspell_set_sug_mode(GtkPspell *gtkpspell, gchar *themode)
{
	PspellConfig *config2;
	guchar       *path;
	guchar       *dict;

	pspell_manager_save_all_word_lists(gtkpspell->checker);
	delete_pspell_manager(gtkpspell->checker);

	gtkpspell->checker = NULL;

	config2 = pspell_config_clone(gtkpspell->config);

	if (!set_path_and_dict(gtkpspell, config2, gtkpspell->path, gtkpspell->dict)) {
		debug_print(_("Pspell set_sug_mod could not reset path & dict\n"));
		return FALSE;
	}

	pspell_config_replace(config2, "sug-mode", themode);

	gtkpspell->possible_err = new_pspell_manager(config2);
	delete_pspell_config(config2);
	config2 = NULL;

	if (pspell_error_number(gtkpspell->possible_err) != 0) {
		debug_print(_("Pspell set sug-mode error %s\n"),
			    pspell_error_message(gtkpspell->possible_err));
		delete_pspell_can_have_error(gtkpspell->possible_err);
		gtkpspell->possible_err = NULL;
		return FALSE;
	}
	gtkpspell->checker = to_pspell_manager(gtkpspell->possible_err);
	return TRUE;
}

/* set_learn_mode() - menu callback to toggle learn mode */
static void set_learn_mode (GtkWidget *w, GtkPspell *gtkpspell)
{
	gtkpspell->learn = gtkpspell->learn == FALSE ;
}
  
/* misspelled_suggest() - Create a suggestion list for  word  */
static GList *misspelled_suggest(GtkPspell *gtkpspell, guchar *word) 
{
	const guchar          *newword;
	GList                 *list = NULL;
	int                    count;
	const PspellWordList  *suggestions;
	PspellStringEmulation *elements;

	g_return_val_if_fail(word, NULL);

	if (!pspell_manager_check(gtkpspell->checker, word, -1)) {
		suggestions = pspell_manager_suggest(gtkpspell->checker, (const char *)word, -1);
		elements    = pspell_word_list_elements(suggestions);
		/* First one must be the misspelled (why this ?) */
		list        = g_list_append(list, g_strdup(word)); 
		
		while ((newword = pspell_string_emulation_next(elements)) != NULL)
			list = g_list_append(list, g_strdup(newword));
		return list;
	}
	return NULL;
}

/* misspelled_test() - Just test if word is correctly spelled */  
static int misspelled_test(GtkPspell *gtkpspell, unsigned char *word) 
{
	return pspell_manager_check(gtkpspell->checker, word, -1) ? 0 : 1; 
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
	/* so, we can use two others pointers that points to the whole    */
	/* word including quotes. */

	gint      start, 
	          end;
	guchar    c;
	GtkXText *gtktext;
	
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

	for (end = pos; end < gtk_xtext_get_length(gtktext); end++) {
		c = get_text_index_whar(gtkpspell, end); 
		if (c == '\'') {
			if (end < gtk_xtext_get_length(gtktext)) {
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

static gboolean get_curword(GtkPspell *gtkpspell, unsigned char* buf,
                            int *pstart, int *pend) 
{
	int pos = gtk_editable_get_position(GTK_EDITABLE(gtkpspell->gtktext));
	return get_word_from_pos(gtkpspell, pos, buf, pstart, pend);
}

static void change_color(GtkPspell * gtkpspell, 
			 int start, int end, 
                         GdkColor *color) 
{
	char	 *newtext;
	GtkXText *gtktext;

	g_return_if_fail(start < end);
    
	gtktext = gtkpspell->gtktext;
    
	gtk_xtext_freeze(gtktext);
	newtext = gtk_editable_get_chars(GTK_EDITABLE(gtktext), start, end);
	if (newtext) {
		gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
						 GTK_SIGNAL_FUNC(entry_insert_cb), 
						 gtkpspell);
		gtk_xtext_set_point(gtktext, start);
		gtk_xtext_forward_delete(gtktext, end - start);

		gtk_xtext_insert(gtktext, NULL, color, NULL, newtext, end - start);
		gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
						   GTK_SIGNAL_FUNC(entry_insert_cb), 
						   gtkpspell);
	}
	gtk_xtext_thaw(gtktext);
}

static gboolean check_at(GtkPspell *gtkpspell, int from_pos) 
{
	int	      start, end;
	unsigned char buf[BUFSIZE];
	GtkXText     *gtktext;

	g_return_val_if_fail(from_pos >= 0, FALSE);
    
	gtktext = gtkpspell->gtktext;

	if (!get_word_from_pos(gtkpspell, from_pos, buf, &start, &end))
		return FALSE;

	strncpy(gtkpspell->theword, buf, BUFSIZE - 1);
	gtkpspell->theword[BUFSIZE - 1] = 0;

	if (misspelled_test(gtkpspell, buf)) {
		if (highlight.pixel == 0) {
			/* add an entry for the highlight in the color map. */
			GdkColormap *gc = gtk_widget_get_colormap(GTK_WIDGET(gtktext));
			gdk_colormap_alloc_color(gc, &highlight, FALSE, TRUE);
		}
		change_color(gtkpspell, start, end, &highlight);
		return TRUE;
	} else {
		change_color(gtkpspell, start, end,
			     &(GTK_WIDGET(gtktext)->style->fg[0]));
		return FALSE;
	}
}


static void check_all(GtkWidget *w, GtkPspell *gtkpspell)
{
	gtkpspell_check_all(gtkpspell);
}


void gtkpspell_check_all(GtkPspell *gtkpspell) 
{
	guint     origpos;
	guint     pos = 0;
	guint     len;
	float     adj_value;
	GtkXText *gtktext;

	
	if (!gtkpspell_running(gtkpspell)) return ;
	gtktext = gtkpspell->gtktext;

	len = gtk_xtext_get_length(gtktext);

	adj_value = gtktext->vadj->value;
	gtk_xtext_freeze(gtktext);
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
	gtk_xtext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
}

static void entry_insert_cb(GtkXText *gtktext, gchar *newtext, 
			    guint len, guint *ppos, 
                            GtkPspell * gtkpspell) 
{
	guint origpos;
	if (!gtkpspell_running(gtkpspell)) 
		return;

	/* We must insert ourself the character to impose the */
	/* color of the inserted character to be default */
	/* Never mess with set_insertion when frozen */
	gtk_xtext_freeze(gtktext);
	gtk_xtext_backward_delete(GTK_XTEXT(gtktext), len);
	gtk_xtext_insert(GTK_XTEXT(gtktext), NULL,
                        &(GTK_WIDGET(gtktext)->style->fg[0]), NULL, newtext, len);
	*ppos = gtk_xtext_get_point(GTK_XTEXT(gtktext));
	       
	if (iswordsep(newtext[0])) {
		/* did we just end a word? */
		if (*ppos >= 2) 
			check_at(gtkpspell, *ppos - 2);

		/* did we just split a word? */
		if (*ppos < gtk_xtext_get_length(gtktext))
			check_at(gtkpspell, *ppos + 1);
	} else {
		/* check as they type, *except* if they're typing at the end (the most
                 * common case.
                 */
		if (*ppos < gtk_xtext_get_length(gtktext) 
		&&  !iswordsep(get_text_index_whar(gtkpspell, *ppos)))
			check_at(gtkpspell, *ppos - 1);
		}
	gtk_xtext_thaw(gtktext);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), *ppos);
}

static void entry_delete_cb(GtkXText *gtktext, gint start, gint end, 
			    GtkPspell *gtkpspell) 
{
	int origpos;
    
	if (!gtkpspell_running(gtkpspell)) 
		return;

	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	if (start)
		check_at(gtkpspell, start - 1);
	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
	/* this is to *UNDO* the selection, in case they were holding shift
         * while hitting backspace. */
}

static void replace_word(GtkWidget *w, GtkPspell *gtkpspell) 
{
	int		start, end,oldlen, newlen;
	guint		origpos;
	unsigned char  *newword;
	unsigned char   buf[BUFSIZE];
	guint		pos;
	GtkXText       *gtktext;
    
	gtktext = gtkpspell->gtktext;

	gtk_xtext_freeze(GTK_XTEXT(gtktext));
	origpos = gtkpspell->orig_pos;
	pos     = origpos;

	gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar**) &newword);
	newlen = strlen(newword);

	get_curword(gtkpspell, buf, &start, &end);
	oldlen = end - start;

	gtk_xtext_set_point(GTK_XTEXT(gtktext), end);
	gtk_xtext_backward_delete(GTK_XTEXT(gtktext), end - start);
	gtk_xtext_insert(GTK_XTEXT(gtktext), NULL, NULL, NULL, newword, strlen(newword));
    
	if (end-start > 0 && gtkpspell->learn) { 
		/* Just be sure the buffer ends somewhere... */
		buf[end-start] = 0; 
		/* Learn from common misspellings */
		pspell_manager_store_replacement(gtkpspell->checker, buf, 
						 end-start, newword, 
						 strlen(newword));
	}

	/* Put the point and the position where we clicked with the mouse */
	/* It seems to be a hack, as I must thaw,freeze,thaw the widget   */
	/* to let it update correctly the word insertion and then the     */
	/* point & position position. If not, SEGV after the first replacement */
	/* If the new word ends before point, put the point at its end*/
    
	if (origpos-start <= oldlen && origpos-start >= 0) {
		/* Original point was in the word. */
		/* Put the insertion point in the same location */
		/* with respect to the new length */
		/* If the original position is still within the word, */
		/* then keep the original position. If not, move to the */
		/* end of the word */
		if (origpos-start > newlen)
			pos = start + newlen;
	}
	else if (origpos > end) {
		/* move the position according to the change of length */
		pos = origpos + newlen - oldlen;
	}
	gtk_xtext_thaw(GTK_XTEXT(gtktext));
	gtk_xtext_freeze(GTK_XTEXT(gtktext));
	if (GTK_XTEXT(gtktext)->text_len < pos)
		pos = gtk_xtext_get_length(GTK_XTEXT(gtktext));
	gtkpspell->orig_pos = pos;
	gtk_editable_set_position(GTK_EDITABLE(gtktext), gtkpspell->orig_pos);
	gtk_xtext_set_point(GTK_XTEXT(gtktext), 
			    gtk_editable_get_position(GTK_EDITABLE(gtktext)));
	gtk_xtext_thaw(GTK_XTEXT(gtktext));
}

/* Accept this word for this session */

static void add_word_to_session(GtkWidget *w, GtkPspell *gtkpspell)
{
	guint     pos;
	GtkXText *gtkxtext;
    
	gtkxtext = gtkpspell->gtktext;
	gtk_xtext_freeze(GTK_XTEXT(gtkxtext));
	pos = gtk_editable_get_position(GTK_EDITABLE(gtkxtext));
    
	pspell_manager_add_to_session(gtkpspell->checker,gtkpspell->theword, strlen(gtkpspell->theword));

	check_at(gtkpspell,gtk_editable_get_position(GTK_EDITABLE(gtkxtext)));

	gtk_xtext_thaw(GTK_XTEXT(gtkxtext));
	gtk_xtext_freeze(GTK_XTEXT(gtkxtext));
	gtk_editable_set_position(GTK_EDITABLE(gtkxtext),pos);
	gtk_xtext_set_point(GTK_XTEXT(gtkxtext), gtk_editable_get_position(GTK_EDITABLE(gtkxtext)));
	gtk_xtext_thaw(GTK_XTEXT(gtkxtext));
}

/* add_word_to_personal() - add word to personal dict. */

static void add_word_to_personal(GtkWidget *w, GtkPspell *gtkpspell)
{
	guint     pos;
	GtkXText *gtkxtext;
    
	gtkxtext = gtkpspell->gtktext;

	gtk_xtext_freeze(GTK_XTEXT(gtkxtext));
	pos = gtk_editable_get_position(GTK_EDITABLE(gtkxtext));
    
	pspell_manager_add_to_personal(gtkpspell->checker,gtkpspell->theword,strlen(gtkpspell->theword));
    
	check_at(gtkpspell,gtk_editable_get_position(GTK_EDITABLE(gtkxtext)));
	gtk_xtext_thaw(GTK_XTEXT(gtkxtext));
	gtk_xtext_freeze(GTK_XTEXT(gtkxtext));
	gtk_editable_set_position(GTK_EDITABLE(gtkxtext),pos);
	gtk_xtext_set_point(GTK_XTEXT(gtkxtext), gtk_editable_get_position(GTK_EDITABLE(gtkxtext)));
	gtk_xtext_thaw(GTK_XTEXT(gtkxtext));
}

       
static GtkMenu *make_menu_config(GtkPspell *gtkpspell)
{
	GtkWidget *menu, *item, *submenu;

  
	menu = gtk_menu_new();

        item = gtk_menu_item_new_with_label(_("Spell check all"));
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
        gtk_signal_connect(GTK_OBJECT(item),"activate",
			   GTK_SIGNAL_FUNC(check_all), 
			   gtkpspell);


        item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu),item);

      	submenu = gtk_menu_new();
        item = gtk_menu_item_new_with_label(_("Change dictionary"));
        gtk_widget_show(item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),submenu);
        gtk_menu_append(GTK_MENU(menu), item);

	/* Dict list */
        if (gtkpspell->dictionary_list==NULL)
		gtkpspell->dictionary_list = gtkpspell_get_dictionary_list(gtkpspell->path);
       
        {
		GtkWidget * curmenu = submenu;
		int count = 0;
		Dictionary *dict;
		GSList *tmp;
		
		tmp = gtkpspell->dictionary_list;
		for (tmp = gtkpspell->dictionary_list; tmp != NULL; tmp =g_slist_next(tmp)) {
			dict = (Dictionary *) tmp->data;
			item = gtk_check_menu_item_new_with_label(dict->name);
			if (strcmp2(dict->name, gtkpspell->dict))
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
			else {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
			}
			gtk_menu_append(GTK_MENU(curmenu), item);
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(menu_change_dict),
					   gtkpspell);
			gtk_widget_show(item);
			count++;
			if (count == MENUCOUNT) {
				GtkWidget *newmenu;
				newmenu = gtk_menu_new();
				item = gtk_menu_item_new_with_label(_("More..."));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), newmenu);
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
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
        if (gtkpspell->mode == PSPELL_FASTMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item),FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),TRUE);
	}
        else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode),
				   gtkpspell);

        item = gtk_check_menu_item_new_with_label(_("Normal Mode"));
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu),item);
        if (gtkpspell->mode == PSPELL_NORMALMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	}
        else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode),
				   gtkpspell);
        
        item = gtk_check_menu_item_new_with_label(_("Bad Spellers Mode"));
	gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);
        if (gtkpspell->mode==PSPELL_BADSPELLERMODE) {
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
	}
        else
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(set_sug_mode),
				   gtkpspell);
        item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

	item = gtk_check_menu_item_new_with_label(_("Learn from mistakes"));
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), 
				       gtkpspell->learn);
	gtk_signal_connect(GTK_OBJECT(item), "activate", 
			   GTK_SIGNAL_FUNC(set_learn_mode), gtkpspell);
			
	
        return GTK_MENU(menu);
}
  
/* make_menu() - Add menus to accept this word for this session and to add it to 
 * personal dictionary */
static GtkMenu *make_menu(GList *l, GtkPspell *gtkpspell) 
{
	GtkWidget *menu, *item;
	unsigned char *caption;
	GtkXText * gtktext;
	
	gtktext = gtkpspell->gtktext;

	menu = gtk_menu_new(); 
        caption = g_strdup_printf(_("Accept `%s' for this session"), (unsigned char*)l->data);
	item = gtk_menu_item_new_with_label(caption);
	g_free(caption);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_session), 
			   gtkpspell);

       	caption = g_strdup_printf(_("Add `%s' to personal dictionary"), (char*)l->data);
	item = gtk_menu_item_new_with_label(caption);
	g_free(caption);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu), item);

        gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(add_word_to_personal), 
			   gtkpspell);
         
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
		int count = 0;
		
		do {
			if (l->data == NULL && l->next != NULL) {
				count = 0;
				curmenu = gtk_menu_new();
				item = gtk_menu_item_new_with_label(_("Others..."));
				gtk_widget_show(item);
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
				gtk_menu_append(GTK_MENU(curmenu), item);
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
			gtk_signal_connect(GTK_OBJECT(item), "activate",
					   GTK_SIGNAL_FUNC(replace_word), gtkpspell);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(curmenu), item);
			count++;
		} while ((l = l->next) != NULL);
	}
	return GTK_MENU(menu);
}

static void popup_menu(GtkPspell *gtkpspell, GdkEventButton *eb) 
{
	unsigned char buf[BUFSIZE];
	GList *list, *l;
	GtkXText * gtktext;
	
	gtktext = gtkpspell->gtktext;
	gtkpspell->orig_pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

	if (!(eb->state & GDK_SHIFT_MASK))
		if (get_curword(gtkpspell, buf, NULL, NULL)) {
			if (buf != NULL) {
				strncpy(gtkpspell->theword, buf, BUFSIZE - 1);
				gtkpspell->theword[BUFSIZE - 1] = 0;
				list = misspelled_suggest(gtkpspell, buf);
				if (list != NULL) {
					gtk_menu_popup(make_menu(list, gtkpspell), NULL, NULL, NULL, NULL,
						       eb->button, eb->time);
					for (l = list; l != NULL; l = l->next)
						g_free(l->data);
					g_list_free(list);
					return;
				}
			}
		}
	gtk_menu_popup(make_menu_config(gtkpspell),NULL,NULL,NULL,NULL,
	eb->button,eb->time);
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor,
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu.
 * HACK HACK HACK.
 */
static gint button_press_intercept_cb(GtkXText *gtktext, GdkEvent *e, GtkPspell *gtkpspell) 
{
	GdkEventButton *eb;
	gboolean retval;

	if (!gtkpspell_running(gtkpspell)) 
		return FALSE;

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

void gtkpspell_uncheck_all(GtkPspell * gtkpspell) 
{
	int origpos;
	unsigned char *text;
	float adj_value;
	GtkXText *gtktext;
	
	gtktext=gtkpspell->gtktext;

	adj_value = gtktext->vadj->value;
	gtk_xtext_freeze(gtktext);
	origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
	text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);
	gtk_xtext_set_point(gtktext, 0);
	gtk_xtext_forward_delete(gtktext, gtk_xtext_get_length(gtktext));
	gtk_xtext_insert(gtktext, NULL, NULL, NULL, text, strlen(text));
	gtk_xtext_thaw(gtktext);

	gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
	gtk_adjustment_set_value(gtktext->vadj, adj_value);
}

void gtkpspell_attach(GtkPspell *gtkpspell, GtkXText *gtktext) 
{
	gtkpspell->gtktext=gtktext;
	gtk_signal_connect_after(GTK_OBJECT(gtktext), "insert-text",
		           GTK_SIGNAL_FUNC(entry_insert_cb), gtkpspell);
	gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
		                 GTK_SIGNAL_FUNC(entry_delete_cb), gtkpspell);
	gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
		           GTK_SIGNAL_FUNC(button_press_intercept_cb), gtkpspell);

}

void gtkpspell_detach(GtkPspell * gtkpspell) 
{
	GtkXText * gtktext;
	
	gtktext =gtkpspell->gtktext;
/*    if (prefs_common.auto_makepspell) { */
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_insert_cb), gtkpspell);
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_delete_cb), gtkpspell);
/*    }; */
	gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(button_press_intercept_cb), gtkpspell);

	gtkpspell_uncheck_all(gtkpspell);
}

/*** Sylpheed (Claws) ***/

static GSList *create_empty_dictionary_list(void)
{
	GSList *list = NULL;
	Dictionary *dict;
	
	dict = g_new0(Dictionary, 1);
	dict->name = g_strdup(_("None"));
	return g_slist_append(list, dict);
}

/* gtkpspell_get_dictionary_list() - returns list of dictionary names */
GSList *gtkpspell_get_dictionary_list(const gchar *pspell_path)
{
	GSList *list;
	gchar *dict_path, *tmp, *prevdir;
	GSList *walk;
	Dictionary *dict;
	DIR *dir;
	struct dirent *ent;

	list = NULL;

#ifdef USE_THREADS
#warning TODO: no directory change
#endif
	dict_path=g_strdup(pspell_path);
	prevdir = g_get_current_dir();
	if (chdir(dict_path) <0) {
		FILE_OP_ERROR(dict_path, "chdir");
		g_free(prevdir);
		g_free(dict_path);
		return create_empty_dictionary_list();
	}

	debug_print(_("Checking for dictionaries in %s\n"), dict_path);

	if (NULL != (dir = opendir("."))) {
		while (NULL != (ent = readdir(dir))) {
			/* search for pwli */
			if (NULL != (tmp = strstr(ent->d_name, ".pwli"))) {
				dict = g_new0(Dictionary, 1);
				dict->name = g_strndup(ent->d_name, tmp - ent->d_name);
				debug_print(_("Found dictionary %s\n"), dict->name);
				list = g_slist_insert_sorted(list, dict, (GCompareFunc) compare_dict);
			}
		}			
		closedir(dir);
	}
	else {
		FILE_OP_ERROR(dict_path, "opendir");
		debug_print(_("No dictionary found\n"));
		list = create_empty_dictionary_list();
	}
        if(list==NULL){
          debug_print(_("No dictionary found"));
          list = create_empty_dictionary_list();
        }
	chdir(prevdir);
	g_free(dict_path);
	g_free(prevdir);
	return list;
}

/* compare_dict () - compare 2 dict names */

static gint compare_dict(Dictionary *a, Dictionary *b)
{
	guchar *alanguage, *blanguage,
	       *aspelling, *bspelling,
	       *ajargon  , *bjargon  ,
	       *amodule  , *bmodule  ;
	guint aparts = 0, bparts = 0, i;

	for (i=0; i < strlen(a->name) ; i++)
		if (a->name[i]=='-')
			aparts++;
	for (i=0; i < strlen(b->name) ; i++)
		if (b->name[i]=='-')
			bparts++;

	if (aparts != bparts) 
		return (aparts < bparts) ? -1 : +1 ;
	else 
		return strcmp2(a->name, b->name);
}
void gtkpspell_free_dictionary_list(GSList *list)
{
	Dictionary *dict;
	GSList *walk;
	for (walk = list; walk != NULL; walk = g_slist_next(walk))
		if (walk->data) {
			dict = (Dictionary *) walk->data;
			if (dict->name)
				g_free(dict->name);
			g_free(dict);
		}				
	g_slist_free(list);
}

static void dictionary_option_menu_item_data_destroy(gpointer data)
{
	gchar *str = (gchar *) data;

	if (str)
		g_free(str);
}

GtkWidget *gtkpspell_dictionary_option_menu_new(const gchar *pspell_path)
{
	GSList *dict_list, *tmp;
	GtkWidget *item;
	GtkWidget *menu;
	Dictionary *dict;

	dict_list = gtkpspell_get_dictionary_list(pspell_path);
	g_return_val_if_fail(dict_list, NULL);

	menu = gtk_menu_new();
	
	for (tmp = dict_list; tmp != NULL; tmp = g_slist_next(tmp)) {
		dict = (Dictionary *) tmp->data;
		item = gtk_menu_item_new_with_label(dict->name);
		if (dict->name)
			gtk_object_set_data_full(GTK_OBJECT(item), "dict_name",
					 g_strdup(dict->name), 
					 dictionary_option_menu_item_data_destroy);
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	gtkpspell_free_dictionary_list(dict_list);

	return menu;
}



gchar *gtkpspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	gchar *result;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
        result = gtk_object_get_data(GTK_OBJECT(menuitem), "dict_name");
        g_return_val_if_fail(result, NULL);
        return g_strdup(result);
  
}
#endif
