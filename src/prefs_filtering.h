/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#ifndef __PREFS_FILTERING_H__
#define __PREFS_FILTERING_H__

/*
void prefs_scoring_read_config	(void);
void prefs_scoring_write_config	(void);
*/
void prefs_filtering_open		(FolderItem * item);
void prefs_filtering_rename_path	(const gchar	*old_path,
				 	 const gchar	*new_path);
void prefs_filtering_delete_path	(const gchar	*path);

#endif /* __PREFS_FILTERING_H__ */
