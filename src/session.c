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

#include "session.h"
#include "imap.h"
#include "news.h"
#include "smtp.h"

void session_destroy(Session *session)
{
	g_return_if_fail(session != NULL);

	switch (session->type) {
	case SESSION_IMAP:
		imap_session_destroy(IMAP_SESSION(session));
		break;
	case SESSION_NEWS:
		news_session_destroy(NNTP_SESSION(session));
		break;
	case SESSION_SMTP:
		smtp_session_destroy(SMTP_SESSION(session));
		break;
	default:
		break;
	}

	g_free(session->server);
	g_free(session);
}
