/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */
/* gtkspell - a spell-checking addon for GtkText
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

#ifndef __gtkspell_h__
#define __gtkspell_h__

#include "gtkxtext.h"

typedef struct _Dictionary {
	gchar *name;
	gchar *path;
} Dictionary;

int gtkspell_start(unsigned char *path, char * args[]);

void gtkspell_attach(GtkXText *text_ccc);

void gtkspell_detach(GtkXText *gtktext);

void gtkspell_check_all(GtkXText *gtktext);

void gtkspell_uncheck_all(GtkXText *gtktext);

GSList *gtkspell_get_dictionary_list(const char *ispell_path);

void gtkspell_free_dictionary_list(GSList *list);

GtkWidget *gtkspell_dictionary_option_menu_new(const gchar *ispell_path);
gchar *gtkspell_get_dictionary_menu_active_item(GtkWidget *menu);

#endif /* __gtkspell_h__ */
