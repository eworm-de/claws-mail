/* Perl plugin -- Perl Support for Claws Mail
 *
 * Copyright (C) 2004-2007 Holger Berndt
 *
 * Sylpheed and Claws Mail are GTK based, lightweight, and fast e-mail clients
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail Team
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
 */

#ifndef SC_PERL_PLUGIN_H
#define SC_PERL_PLUGIN_H SC_PERL_PLUGIN_H

/* the name of the filtering Perl script file */
#define PERLFILTER "perl_filter"

typedef struct {
  gchar *address;
  gchar *bookname;
} PerlPluginEmailEntry;

typedef struct {
  gchar *address;
  gchar *value;
  gchar *bookname;
} PerlPluginAttributeEntry;

typedef struct {
  GSList *g_slist;
  time_t mtime;
} PerlPluginTimedSList;

typedef struct {
  gint filter_log_verbosity;
} PerlPluginConfig;

gint execute_detached(gchar**);

#endif /* include guard */
