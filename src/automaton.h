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

#ifndef __AUTOMATON_H__
#define __AUTOMATON_H__

#include <glib.h>

#include "socket.h"

typedef struct _AtmState	AtmState;
typedef struct _Automaton	Automaton;

typedef gint	(*AtmHandler)	(SockInfo	*source,
				 gpointer	 data);
typedef void	(*AtmUIFunc)	(gpointer	 data,
				 gint		 state);

struct _AtmState
{
	GdkInputCondition condition;
	gint (*handler)	(SockInfo	*source,
			 gpointer	 data);
};

struct _Automaton
{
	gint max;
	gint num;
	gint tag;
	guint timeout_tag;
	guint elapsed;
	gboolean terminated;

	gpointer data;
	AtmState *state;

	gint (*terminate)	(SockInfo	*source,
				 gpointer	 data);

	AtmUIFunc ui_func;

	SockInfo *help_sock;
};

Automaton *automaton_create	(gint			 num);
void automaton_destroy		(Automaton		*atm);
void automaton_input_cb		(gpointer		 data,
				 gint			 dummy_source,
				 GdkInputCondition	 condition);

#endif /* __AUTOMATON_H__ */
