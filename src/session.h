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

#ifndef __SESSION_H__
#define __SESSION_H__

#include <glib.h>
#include <time.h>

#include "socket.h"

typedef struct _Session	Session;

#define SESSION(obj)	((Session *)obj)

typedef enum {
	SESSION_IMAP,
	SESSION_NEWS,
	SESSION_SMTP
} SessionType;

typedef enum {
	SESSION_READY,
	SESSION_SEND,
	SESSION_RECV
} SessionPhase;

struct _Session
{
	SessionType type;

	SockInfo *sock;
	gchar *server;

	gboolean connected;
	SessionPhase phase;

	time_t last_access_time;

	gpointer data;

	void (*destroy)		(Session	*session);
};

void session_destroy	(Session	*session);

#endif /* __SESSION_H__ */
