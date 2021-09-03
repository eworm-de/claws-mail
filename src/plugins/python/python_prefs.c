/* Python plugin for Claws Mail
 * Copyright (C) 2009-2018 Holger Berndt and The Claws Mail Team
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "common/version.h"
#include "defs.h"
#include "claws.h"
#include "plugin.h"
#include "file-utils.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "python_prefs.h"

#define PREFS_BLOCK_NAME "Python"

PythonConfig python_config;

static PrefParam prefs[] = {
        {"console_win_width", "-1", &python_config.console_win_width,
		P_INT, NULL, NULL, NULL},
        {"console_win_height", "-1", &python_config.console_win_height,
		P_INT, NULL, NULL, NULL},
        {0,0,0,0,0,0,0}
};

void python_prefs_init(void)
{
	gchar *rcpath;

	prefs_set_default(prefs);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(prefs, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);
}

void python_prefs_done(void)
{
	PrefFile *pref_file;
	gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					  COMMON_RC, NULL);
	pref_file = prefs_write_open(rc_file_path);
	g_free(rc_file_path);

	if (!(pref_file) || (prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0))
		return;
	
	if (prefs_write_param(prefs, pref_file->fp) < 0) {
		g_warning("failed to write Python plugin configuration");
		prefs_file_close_revert(pref_file);
		return;
	}

	if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rc_file_path, "fprintf");
		prefs_file_close_revert(pref_file);
	} else
		prefs_file_close(pref_file);
}
