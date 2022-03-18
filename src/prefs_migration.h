/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 the Claws Mail team
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

#ifndef __PREFS_MIGRATION_H__
#define __PREFS_MIGRATION_H__

int prefs_update_config_version_common();
int prefs_update_config_version_accounts();
int prefs_update_config_version_password_store(gint from_version);
int prefs_update_config_version_folderlist(gint from_version);
int prefs_update_config_version_folder_item(FolderItem *item);

#endif /* __PREFS_MIGRATION_H__ */
