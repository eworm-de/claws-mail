/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */
/* gtkaspell - a spell-checking addon for GtkText
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
 *  Adapted for GNU/aspell (c) 2002 Melvin Hadasht
 *
 */

#ifndef __GTKASPELL_H__
#define __GTKASPELL_H__

#include <gtk/gtkoptionmenu.h>
#include <aspell.h>

#include "gtkstext.h"

#define ASPELL_FASTMODE       1
#define ASPELL_NORMALMODE     2
#define ASPELL_BADSPELLERMODE 3

#define GTKASPELLWORDSIZE 1024

typedef struct _GtkAspellCheckers
{
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

typedef struct _GtkAspell
{
	GtkAspeller	*gtkaspeller;
	GtkAspeller	*alternate_speller;
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

	gint		 default_sug_mode;
	gint		 max_sug;
	GList		*suggestions_list;

	GtkSText	*gtktext;
	GdkColor 	 highlight;
} GtkAspell;

typedef AspellConfig GtkAspellConfig;

extern GtkAspellCheckers *gtkaspellcheckers;

GtkAspellCheckers*	gtkaspell_checkers_new		();

GtkAspellCheckers*	gtkaspell_checkers_delete	();

void 			gtkaspell_checkers_reset_error	();

GtkAspell*		gtkaspell_new			(const gchar *dictionary, 
							 const gchar *encoding,
							 gint  misspelled_color,
							 gboolean check_while_typing,  
							 gboolean use_alternate,  
							 GtkSText *gtktext);

void 			gtkaspell_delete		(GtkAspell *gtkaspell); 

guchar*			gtkaspell_get_dict		(GtkAspell *gtkaspell);

guchar*			gtkaspell_get_path		(GtkAspell *gtkaspell);

gboolean 		gtkaspell_set_sug_mode		(GtkAspell *gtkaspell, 
							 gint  themode);

GSList*			gtkaspell_get_dictionary_list	(const char *aspell_path,
							 gint refresh);

void 			gtkaspell_free_dictionary_list	(GSList *list);

void 			gtkaspell_check_forwards_go	(GtkAspell *gtkaspell);
void 			gtkaspell_check_backwards	(GtkAspell *gtkaspell);

void 			gtkaspell_check_all		(GtkAspell *gtkaspell);
void 			gtkaspell_uncheck_all		(GtkAspell *gtkaspell);
void 			gtkaspell_highlight_all		(GtkAspell *gtkaspell);

void 			gtkaspell_populate_submenu	(GtkAspell *gtkaspell, 
							 GtkWidget *menuitem);

GtkWidget*		gtkaspell_dictionary_option_menu_new
							(const gchar *aspell_path);
gchar*			gtkaspell_get_dictionary_menu_active_item
							(GtkWidget *menu);

GtkWidget*		gtkaspell_sugmode_option_menu_new
							(gint sugmode);

void 			gtkaspell_sugmode_option_menu_set
							(GtkOptionMenu *optmenu, 
							 gint sugmode);

gint 			gtkaspell_get_sugmode_from_option_menu	
							(GtkOptionMenu *optmenu);

#endif /* __GTKASPELL_H__ */
