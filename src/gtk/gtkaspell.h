/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */
/* gtkaspell - a spell-checking addon for GtkText
 * Copyright (c) 2001-2002 Melvin Hadasht
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Adapted by the Claws Mail Team.
 */

/*
 *  Adapted for pspell (c) 2001-2002 Melvin Hadasht
 *  Adapted for GNU/aspell (c) 2002 Melvin Hadasht
 *
 */

#ifndef __GTKASPELL_H__
#define __GTKASPELL_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_ASPELL

#include <gtk/gtkoptionmenu.h>

typedef struct _GtkAspell GtkAspell; /* Defined in gtkaspell.c */

void		gtkaspell_checkers_init		(void);

void		gtkaspell_checkers_quit		(void);

const char * 	gtkaspell_checkers_strerror	(void);

void 		gtkaspell_checkers_reset_error	(void);

GtkAspell*	gtkaspell_new			(const gchar *dictionary_path,
						 const gchar *dictionary, 
						 const gchar *alt_dictionary, 
						 const gchar *encoding,
						 gint  misspelled_color,
						 gboolean check_while_typing,
						 gboolean recheck_when_changing_dict,
						 gboolean use_alternate,  
						 gboolean use_both_dicts,  
						 GtkTextView *gtktext,
						 GtkWindow *parent_win,
						 void (*spell_menu_cb)(void *data),
						 void *data);

void 		gtkaspell_delete		(GtkAspell *gtkaspell); 


gboolean	gtkaspell_change_dict		(GtkAspell *gtkaspell,
    						 const gchar* dictionary,
							 gboolean always_set_alt_dict);

gboolean	gtkaspell_change_alt_dict	(GtkAspell *gtkaspell,
    						 const gchar* alt_dictionary);


gboolean 	gtkaspell_set_sug_mode		(GtkAspell *gtkaspell, 
						 gint  themode);

void 		gtkaspell_check_forwards_go	(GtkAspell *gtkaspell);
void 		gtkaspell_check_backwards	(GtkAspell *gtkaspell);

void 		gtkaspell_check_all		(GtkAspell *gtkaspell);
void 		gtkaspell_highlight_all		(GtkAspell *gtkaspell);

GtkWidget*	gtkaspell_dictionary_combo_new	(const gchar *aspell_path,
						 const gboolean refresh);

GtkTreeModel*	gtkaspell_dictionary_store_new	(const gchar *aspell_path);
GtkTreeModel*	gtkaspell_dictionary_store_new_with_refresh
							(const gchar *aspell_path,
							 gboolean     refresh);

gchar*		gtkaspell_get_dictionary_menu_active_item
							(GtkComboBox *combo);
gint		gtkaspell_set_dictionary_menu_active_item
							(GtkComboBox *combo, 
							 const gchar *dictionary);

GtkWidget*	gtkaspell_sugmode_combo_new	(gint sugmode);

GSList*		gtkaspell_make_config_menu		(GtkAspell	*gtkaspell);

gchar *gtkaspell_get_default_dictionary(GtkAspell *gtkaspell);

#endif /* USE_ASPELL */
#endif /* __GTKASPELL_H__ */
