/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */
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
 * Adapted by the Sylpheed Claws Team.
 */

/*
 *  Adapted for pspell (c) 2001 Melvin Hadasht
 *
 */

 /* first a pspell config must be created (at the beginning of the app)
 with 
 GtkPspellConfig gtkpspellconfig = gtkpspell_init();
 then for each document use this to create a manager
 GtkPspell = gtkpspell_new(GtkPspellConfig *gtkpspellconfig);
 Now, path and dict can be set
 gtkpspell_set_path_and_dict(GtkPspell *gtkpspell, guchar * path, guchar * dict);
 then attach this to a gtktext widget :
 gtkpspell_attach(GtkPspell *gtkpspell, GtkXText *gtkxtext);
 and we can also detach :
 gtkpspell_deattach(GtkPspell *gtkpspell, GtkXText *gtkxtext);
 When finished, GtkPspell can be deleted with
 gtkpspell_delete(GtkPspell * gtkpspell);
 At the end of the app, GtkPspellConfig should be deleted with :
 gtkpspell_finished(GtkPspellConfig * gtkpspellconfig);
*/ 

#ifndef __gtkpspell_h__
#define __gtkpspell_h__

#include "gtkxtext.h"
#include <pspell/pspell.h>

#define PSPELL_FASTMODE       1
#define PSPELL_NORMALMODE     2
#define PSPELL_BADSPELLERMODE 3

typedef struct _Dictionary {
	gchar *name;
} Dictionary;

typedef struct _GtkPspell 
{
	PspellConfig * config;
	PspellCanHaveError * possible_err ;
	PspellManager * checker;
	gchar theword[1111];
	gchar *path;
	gchar *dict;
	guint mode;
	guint learn;
        guint orig_pos;
	     
	GSList * dictionary_list;
	GtkXText * gtktext;

} GtkPspell;

typedef PspellConfig GtkPspellConfig;

/* Create a new gtkspell instance to manage one text widget */

/* These one create and delete a pspell config */
GtkPspellConfig *gtkpspell_init();
void gtkpspell_finished(GtkPspellConfig * gtkpspellconfig);

/* These ones create and delete a manager*/
GtkPspell *gtkpspell_new(GtkPspellConfig * config);

GtkPspell *gtkpspell_new_with_config(GtkPspellConfig *gtkpspellconfig, 
                                     guchar *path, 
                                     guchar *dict, 
                                     guint mode, 
                                     guchar *encoding);
GtkPspell *gtkpspell_delete(GtkPspell *gtkpspell);

int gtkpspell_set_path_and_dict(GtkPspell *gtkpspell, guchar * path,
                                guchar * dict);
guchar *gtkpspell_get_dict(GtkPspell *gtkpspell);

guchar *gtkpspell_get_path(GtkPspell *gtkpspell);

/* This sets suggestion mode "fast" "normal" "bad-spellers" */
/* and resets the dict & path (which should be set first)  */
/* return 0 on failure and -1 on success */
int gtkpspell_set_sug_mode(GtkPspell * gtkpspell, gchar * themode);

void gtkpspell_attach(GtkPspell *gtkpspell, GtkXText *text_ccc);

void gtkpspell_detach(GtkPspell *gtkpspell);

void gtkpspell_check_all(GtkPspell *gtkpspell);

void gtkpspell_uncheck_all(GtkPspell *gtkpspell);

GSList *gtkpspell_get_dictionary_list(const char *pspell_path);

void gtkpspell_free_dictionary_list(GSList *list);

GtkWidget *gtkpspell_dictionary_option_menu_new(const gchar *pspell_path);
gchar *gtkpspell_get_dictionary_menu_active_item(GtkWidget *menu);

extern GtkPspellConfig * gtkpspellconfig;

#endif /* __gtkpspell_h__ */
