/* GData plugin for Claws Mail
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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#ifdef G_OS_UNIX
# include <libintl.h>
#endif

#include "common/plugin.h"
#include "common/version.h"
#include "common/utils.h"
#include "common/hooks.h"
#include "common/defs.h"
#include "common/prefs.h"
#include "main.h"
#include "mainwindow.h"
#include "addr_compl.h"
#include "passwordstore.h"

#include "cm_gdata_contacts.h"
#include "cm_gdata_prefs.h"

static gulong hook_address_completion= 0;
static gulong hook_offline_switch = 0;
static gulong timer_query_contacts = 0;

static gboolean my_address_completion_build_list_hook(gpointer source, gpointer data)
{
  cm_gdata_add_contacts(source);
  return FALSE;
}

static gboolean my_offline_switch_hook(gpointer source, gpointer data)
{
  cm_gdata_update_contacts_cache();
  return FALSE;
}

static void cm_gdata_save_config(void)
{
  PrefFile *pfile;
  gchar *rcpath;

  debug_print("Saving GData plugin configuration...\n");

  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  pfile = prefs_write_open(rcpath);
  g_free(rcpath);
  if (!pfile || (prefs_set_block_label(pfile, "GDataPlugin") < 0))
    return;

  if (prefs_write_param(cm_gdata_param, pfile->fp) < 0) {
    debug_print("failed!\n");
    g_warning("GData plugin: failed to write plugin configuration to file");
    prefs_file_close_revert(pfile);
    return;
  }
  if (fprintf(pfile->fp, "\n") < 0) {
    FILE_OP_ERROR(rcpath, "fprintf");
    prefs_file_close_revert(pfile);
  }
  else
    prefs_file_close(pfile);
  debug_print("done.\n");
}

void cm_gdata_update_contacts_update_timer(void)
{
  if(timer_query_contacts != 0)
    g_source_remove(timer_query_contacts);
  timer_query_contacts = g_timeout_add_seconds(cm_gdata_config.max_cache_age, (GSourceFunc)cm_gdata_update_contacts_cache, NULL);
}

gint plugin_init(gchar **error)
{
  gchar *rcpath;

  /* Version check */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,13,2,39),
			   VERSION_NUMERIC, _("GData"), error))
    return -1;

  hook_address_completion = hooks_register_hook(ADDDRESS_COMPLETION_BUILD_ADDRESS_LIST_HOOKLIST,
      my_address_completion_build_list_hook, NULL);
  if(hook_address_completion == 0) {
    *error = g_strdup(_("Failed to register address completion hook in the GData plugin"));
    return -1;
  }

  hook_offline_switch = hooks_register_hook(OFFLINE_SWITCH_HOOKLIST, my_offline_switch_hook, NULL);
  if(hook_offline_switch == 0) {
    hooks_unregister_hook(ADDDRESS_COMPLETION_BUILD_ADDRESS_LIST_HOOKLIST, hook_address_completion);
    *error = g_strdup(_("Failed to register offline switch hook in the GData plugin"));
    return -1;
  }

  /* Configuration */
  prefs_set_default(cm_gdata_param);
  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  prefs_read_config(cm_gdata_param, "GDataPlugin", rcpath, NULL);
  g_free(rcpath);

	/* If the refresh token is still stored in config, save it to
	 * password store. */
	if(cm_gdata_config.oauth2_refresh_token != NULL) {
		passwd_store_set(PWS_PLUGIN, "GData", GDATA_TOKEN_PWD_STRING,
				cm_gdata_config.oauth2_refresh_token, TRUE);
		passwd_store_write_config();
	}

  cm_gdata_prefs_init();

  debug_print("GData plugin loaded\n");

  /* contacts cache */
  cm_gdata_load_contacts_cache_from_file();
  cm_gdata_update_contacts_update_timer();
  cm_gdata_update_contacts_cache();

  return 0;
}

gboolean plugin_done(void)
{
  if(!claws_is_exiting()) {
    hooks_unregister_hook(ADDDRESS_COMPLETION_BUILD_ADDRESS_LIST_HOOKLIST, hook_address_completion);
    hooks_unregister_hook(OFFLINE_SWITCH_HOOKLIST, hook_offline_switch);
    g_source_remove(timer_query_contacts);
  }
  cm_gdata_prefs_done();
  cm_gdata_contacts_done();

  cm_gdata_save_config();

  debug_print("GData plugin unloaded\n");

  /* returning FALSE because dependant libraries may not be unload-safe. */
  return FALSE;
}

const gchar *plugin_name(void)
{
  return _("GData");
}

const gchar *plugin_desc(void)
{
  return _("This plugin provides access to the GData protocol "
	   "for Claws Mail.\n\n"
      "The GData protocol is an interface to Google services.\n"
      "Currently, the only implemented functionality is to include "
      "Google Contacts into the Tab-address completion.\n"
     "\nFeedback to <berndth@gmx.de> is welcome.");
}

const gchar *plugin_type(void)
{
  return "GTK3";
}

const gchar *plugin_licence(void)
{
  return "GPL3+";
}

const gchar *plugin_version(void)
{
  return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
  static struct PluginFeature features[] =
    { {PLUGIN_UTILITY, N_("GData integration")},
      {PLUGIN_NOTHING, NULL}};
  return features;
}
