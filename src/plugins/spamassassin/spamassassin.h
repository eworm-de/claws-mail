/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifndef SPAMASSASSIN_H
#define SPAMASSASSIN_H 1

#include <glib.h>

#ifdef WIN32
#define extern

typedef struct {
	gboolean spamassassin_enable;
	gchar *spamassassin_hostname;
	int spamassassin_port;
	int spamassassin_max_size;
	gboolean spamassassin_receive_spam;
	gchar *spamassassin_save_folder;
} _spamassassin_cfg;
_spamassassin_cfg spamassassin_cfg;

#define FROM_SPAMCFG_STRUCT(cfg) \
{ \
	spamassassin_enable	  = cfg->spamassassin_enable		; \
	spamassassin_hostname	  = cfg->spamassassin_hostname		; \
	spamassassin_port	  = cfg->spamassassin_port		; \
	spamassassin_max_size	  = cfg->spamassassin_max_size		; \
	spamassassin_receive_spam = cfg->spamassassin_receive_spam	; \
	spamassassin_save_folder  = cfg->spamassassin_save_folder	; \
}

#define TO_SPAMCFG_STRUCT(cfg) \
{ \
	cfg->spamassassin_enable	= spamassassin_enable		; \
	cfg->spamassassin_hostname	= spamassassin_hostname		; \
	cfg->spamassassin_port		= spamassassin_port		; \
	cfg->spamassassin_max_size	= spamassassin_max_size		; \
	cfg->spamassassin_receive_spam	= spamassassin_receive_spam	; \
	cfg->spamassassin_save_folder	= spamassassin_save_folder	; \
}
void spamassassin_getconf(_spamassassin_cfg * const spamassassin_cfg);
void spamassassin_setconf(const _spamassassin_cfg * const spamassassin_cfg);
#endif /* WIN32 */

extern gboolean spamassassin_enable;
extern gchar *spamassassin_hostname;
extern int spamassassin_port;
extern int spamassassin_max_size;
extern gboolean spamassassin_receive_spam;
extern gchar *spamassassin_save_folder;

void spamassassin_save_config();

#endif
