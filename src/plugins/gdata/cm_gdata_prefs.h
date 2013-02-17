/* GData plugin for Claws-Mail
 * Copyright (C) 2011 Holger Berndt
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

#ifndef CM_GDATA_PREFS_H_
#define CM_GDATA_PREFS_H_

#include "prefs_gtk.h"

typedef struct {
  char *username;
  char *password;
  int max_num_results;
  int max_cache_age;
} CmGDataPrefs;

extern CmGDataPrefs cm_gdata_config;
extern PrefParam    cm_gdata_param[];

void cm_gdata_prefs_init(void);
void cm_gdata_prefs_done(void);

#endif /* CM_GDATA_PREFS_H_ */
