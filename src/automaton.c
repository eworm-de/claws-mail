/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkmain.h>

#include "automaton.h"

Automaton *automaton_create(gint num)
{
	Automaton *atm;

	g_return_val_if_fail(num > 0, NULL);

	atm = g_new0(Automaton, 1);
	atm->max = num - 1;
	atm->state = g_new0(AtmState, num);

	return atm;
}

void automaton_destroy(Automaton *atm)
{
	if (!atm) return;

	g_free(atm->state);
	g_free(atm);
}

void automaton_input_cb(gpointer data, gint dummy_source,
			GdkInputCondition condition)
{
	Automaton *atm = (Automaton *)data;
	SockInfo *sock;
	gint next;

	/* We get out sockinfo from the atm context and not from the
	 * passed file descriptor because we can't map that one back
	 * to the sockinfo */
	sock = atm->help_sock;
	g_assert(sock->sock == dummy_source);

	if (atm->timeout_tag > 0) {
		gtk_timeout_remove(atm->timeout_tag);
		atm->timeout_tag = 0;
		atm->elapsed = 0;
	}
	gdk_input_remove(atm->tag);
	atm->tag = 0;

	if (atm->ui_func)
		atm->ui_func(atm->data, atm->num);
	next = atm->state[atm->num].handler(sock, atm->data);

	if (atm->terminated)
		return;

	if (next >= 0 && next <= atm->max && next != atm->num) {
		atm->num = next;
		atm->tag = sock_gdk_input_add(sock,
					 atm->state[atm->num].condition,       
					 automaton_input_cb,
					 data);
	} else {
		atm->terminate(sock, data);
	}
}
