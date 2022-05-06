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

#ifndef SIEVE_MANAGER_H
#define SIEVE_MANAGER_H

#include "managesieve.h"

typedef struct SieveManagerPage SieveManagerPage;

struct SieveManagerPage
{
	GtkWidget*	window;
	GtkWidget*	accounts_menu;
	GtkWidget*	status_text;
	GtkWidget*	filters_list;
	GtkWidget*	vbox_buttons;
	SieveSession	*active_session;
	gboolean	got_list;
};

void sieve_managers_done(void);
void sieve_manager_show(void);
void sieve_manager_done(SieveManagerPage *page);
void sieve_manager_script_created(SieveSession *session,
		const gchar *filter_name);

#endif /* SIEVE_MANAGER_H */
