/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */
/* gtkpspell - a spell-checking addon for GtkText
 * Copyright (c) 2001-2002 Melvin Hadasht
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
 *  Adapted for pspell (c) 2001-2002 Melvin Hadasht
 *
 */

#ifndef __gtkpspell_h__
#define __gtkpspell_h__

#include "gtkstext.h"

#include <gtk/gtkoptionmenu.h>
#include <pspell/pspell.h>

#define PSPELL_FASTMODE       1
#define PSPELL_NORMALMODE     2
#define PSPELL_BADSPELLERMODE 3

typedef struct _GtkPspellCheckers {
	GSList		*checkers;
	GSList		*dictionary_list;
	gchar		*error_message;
} GtkPspellCheckers;

typedef struct _Dictionary {
	gchar		*fullname;
	gchar		*dictname; /* dictname points into fullname */
	gchar		*encoding;
} Dictionary;

typedef struct _GtkPspeller {
	Dictionary *	dictionary;
	gint        	sug_mode;
	gint        	ispell;
	PspellConfig  *	config;
	PspellManager *	checker;
} GtkPspeller;

typedef void (*ContCheckFunc)	(gpointer *gtkpspell);

#define GTKPSPELLWORDSIZE 1024
typedef struct _GtkPspell {
	GtkPspeller	*gtkpspeller;
	GtkPspeller	*alternate_speller;
	gchar 		 theword[GTKPSPELLWORDSIZE];
	gint  		 start_pos;
	gint  		 end_pos;
        gint 		 orig_pos;
	gint		 end_check_pos;
	gboolean	 misspelled;
	gboolean	 check_while_typing;
	gboolean	 use_alternate;

	ContCheckFunc 	 continue_check; 
#if 0
	Dictionary	*dict1;
	Dictionary	*dict2;

	GtkWidget	*gui;
	gpointer	*compose;
#endif
	GtkWidget	*config_menu;
	GtkWidget	*popup_config_menu;
	GtkWidget	*sug_menu;
	GtkWidget	*replace_entry;

	gint		 default_sug_mode;
	gint		 max_sug;
	GList		*suggestions_list;

	GtkSText	*gtktext;
	GdkColor 	 highlight;
} GtkPspell;

typedef PspellConfig GtkPspellConfig;

extern GtkPspellCheckers *gtkpspellcheckers;

GtkPspellCheckers*	gtkpspell_checkers_new		();

GtkPspellCheckers*	gtkpspell_checkers_delete	();

void 			gtkpspell_checkers_reset_error	();

GtkPspell*		gtkpspell_new			(const gchar *dictionary, 
							 const gchar *encoding,
							 gint  misspelled_color,
							 gboolean check_while_typing,  
							 gboolean use_alternate,  
							 GtkSText *gtktext);

void 			gtkpspell_delete		(GtkPspell *gtkpspell); 

guchar*			gtkpspell_get_dict		(GtkPspell *gtkpspell);

guchar*			gtkpspell_get_path		(GtkPspell *gtkpspell);

gboolean 		gtkpspell_set_sug_mode		(GtkPspell *gtkpspell, 
							 gint  themode);

GSList*			gtkpspell_get_dictionary_list	(const char *pspell_path,
							 gint refresh);

void 			gtkpspell_free_dictionary_list	(GSList *list);

void 			gtkpspell_check_forwards_go	(GtkPspell *gtkpspell);
void 			gtkpspell_check_backwards	(GtkPspell *gtkpspell);

void 			gtkpspell_check_all		(GtkPspell *gtkpspell);
void 			gtkpspell_uncheck_all		(GtkPspell *gtkpspell);
void 			gtkpspell_highlight_all		(GtkPspell *gtkpspell);

void 			gtkpspell_populate_submenu	(GtkPspell *gtkpspell, 
							 GtkWidget *menuitem);

GtkWidget*		gtkpspell_dictionary_option_menu_new
							(const gchar *pspell_path);
gchar*			gtkpspell_get_dictionary_menu_active_item
							(GtkWidget *menu);

GtkWidget*		gtkpspell_sugmode_option_menu_new
							(gint sugmode);

void 			gtkpspell_sugmode_option_menu_set
							(GtkOptionMenu *optmenu, 
							 gint sugmode);

gint 			gtkpspell_get_sugmode_from_option_menu	
							(GtkOptionMenu *optmenu);

#endif /* __gtkpspell_h__ */
