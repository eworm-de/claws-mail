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
						 const gchar *encoding,
						 gint  misspelled_color,
						 gboolean check_while_typing,
						 gboolean recheck_when_changing_dict,
						 gboolean use_alternate,  
						 GtkTextView *gtktext,
						 GtkWindow *parent_win,
						 void (*spell_menu_cb)(void *data),
						 void *data);

void 		gtkaspell_delete		(GtkAspell *gtkaspell); 

guchar*		gtkaspell_get_dict		(GtkAspell *gtkaspell);

gboolean	gtkaspell_change_dict		(GtkAspell *gtkaspell,
    						 const gchar* dictionary);

guchar*		gtkaspell_get_path		(GtkAspell *gtkaspell);

gboolean 	gtkaspell_set_sug_mode		(GtkAspell *gtkaspell, 
						 gint  themode);

GSList*		gtkaspell_get_dictionary_list	(const char *aspell_path,
						 gint refresh);

void 		gtkaspell_free_dictionary_list	(GSList *list);

void 		gtkaspell_check_forwards_go	(GtkAspell *gtkaspell);
void 		gtkaspell_check_backwards	(GtkAspell *gtkaspell);

void 		gtkaspell_check_all		(GtkAspell *gtkaspell);
void 		gtkaspell_uncheck_all		(GtkAspell *gtkaspell);
void 		gtkaspell_highlight_all		(GtkAspell *gtkaspell);

GtkWidget*	gtkaspell_dictionary_option_menu_new	(const gchar *aspell_path);

gchar*		gtkaspell_get_dictionary_menu_active_item
							(GtkWidget *menu);
gint		gtkaspell_set_dictionary_menu_active_item
							(GtkWidget *menu, 
							 const gchar *dictionary);

GtkWidget*	gtkaspell_sugmode_option_menu_new	(gint sugmode);

void 		gtkaspell_sugmode_option_menu_set	(GtkOptionMenu *optmenu,
							 gint sugmode);

gint 		gtkaspell_get_sugmode_from_option_menu	(GtkOptionMenu *optmenu);
GSList*		gtkaspell_make_config_menu		(GtkAspell	*gtkaspell);

#endif /* USE_ASPELL */
#endif /* __GTKASPELL_H__ */
