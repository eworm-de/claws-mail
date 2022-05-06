/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2015 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef SIEVE_EDITOR_H
#define SIEVE_EDITOR_H

#include "undo.h"

typedef struct SieveEditorPage SieveEditorPage;

struct SieveEditorPage
{
	GtkWidget*	window;
	GtkWidget*	status_text;
	GtkWidget*	status_icon;
	GtkWidget*	text;
	GtkUIManager *ui_manager;
	UndoMain	*undostruct;
	struct SieveSession *session;
	gchar *script_name;
	gboolean	first_line;
	gboolean	modified;
	gboolean	closing;
	gboolean	is_new;

	/* callback for failure to load the script */
	sieve_session_cb_fn on_load_error;
	gpointer on_load_error_data;
};

void sieve_editors_close();
SieveEditorPage *sieve_editor_new(SieveSession *session, gchar *script_name);
SieveEditorPage *sieve_editor_get(SieveSession *session, gchar *script_name);
void sieve_editor_load(SieveEditorPage *page,
		sieve_session_cb_fn on_load_error, gpointer load_error_data);
void sieve_editor_append_text(SieveEditorPage *page, gchar *text, gint len);
void sieve_editor_close(SieveEditorPage *page);
void sieve_editor_show(SieveEditorPage *page);
void sieve_editor_present(SieveEditorPage *page);

#endif /* SIEVE_EDITOR_H */

