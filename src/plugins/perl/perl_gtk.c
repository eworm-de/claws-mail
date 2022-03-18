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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include <gtk/gtk.h>

#include "common/utils.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "main.h"
#include "menu.h"

#include "perl_plugin.h"
#include "perl_gtk.h"

static void perl_filter_edit(GtkAction *,gpointer);


static GtkActionEntry mainwindow_tools_perl_edit[] = {{
	"Tools/EditPerlRules",
	NULL, N_("Edit perl filter rules (ext)..."), NULL, NULL, G_CALLBACK(perl_filter_edit)
}};

static gint main_menu_id = 0;


void perl_gtk_init(void)
{
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();

  gtk_action_group_add_actions(mainwin->action_group, mainwindow_tools_perl_edit,
		  1, (gpointer)mainwin);
  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools", "EditPerlRules", 
		    "Tools/EditPerlRules", GTK_UI_MANAGER_MENUITEM,
		    main_menu_id)
}

void perl_gtk_done(void)
{
  MainWindow *mainwin;

  mainwin = mainwindow_get_mainwindow();

  if(mainwin && !claws_is_exiting()) {
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Tools/EditPerlRules", main_menu_id);
    main_menu_id = 0;
  }
}

static void perl_filter_edit(GtkAction *action, gpointer callback_data)
{
  gchar *perlfilter;
  gchar *pp;
  gchar buf[1024];
  gchar **cmdline;

  perlfilter = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PERLFILTER, NULL);
  if (prefs_common_get_ext_editor_cmd() &&
      (pp = strchr(prefs_common_get_ext_editor_cmd(), '%')) &&
      *(pp + 1) == 's' && !strchr(pp + 2, '%')) {
    g_snprintf(buf, sizeof(buf), prefs_common_get_ext_editor_cmd(), perlfilter);
  }
  else {
    if (prefs_common_get_ext_editor_cmd())
      g_warning("Perl plugin: external editor command-line is invalid: `%s'",
		prefs_common_get_ext_editor_cmd());
    g_snprintf(buf, sizeof(buf), "emacs %s", perlfilter);
  }
  g_free(perlfilter);
  cmdline = strsplit_with_quote(buf, " ", 1024);
  execute_detached(cmdline);
  g_strfreev(cmdline);
}
