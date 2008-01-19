/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2007 the Claws Mail Team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <clamav.h>

#ifdef CL_DB_STDOPT
#define NEW_CLAMAV 1
#else
#undef NEW_CLAMAV
#endif

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "inc.h"
#include "mimeview.h"
#include "folder.h"
#include "prefs.h"
#include "prefs_gtk.h"

#include "clamav_plugin.h"

#define PLUGIN_NAME (_("Clam AntiVirus"))

static guint hook_id;
static MessageCallback message_callback;

static ClamAvConfig config;

static PrefParam param[] = {
	{"clamav_enable", "FALSE", &config.clamav_enable, P_BOOL,
	 NULL, NULL, NULL},
	{"clamav_enable_arc", "FALSE", &config.clamav_enable_arc, P_BOOL,
	 NULL, NULL, NULL},
	{"clamav_max_size", "1", &config.clamav_max_size, P_USHORT,
	 NULL, NULL, NULL},
	{"clamav_recv_infected", "TRUE", &config.clamav_recv_infected, P_BOOL,
	 NULL, NULL, NULL},
	{"clamav_save_folder", NULL, &config.clamav_save_folder, P_STRING,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

#ifdef NEW_CLAMAV
static struct cl_engine *engine;
#else
static struct cl_node *cl_database;
#endif

struct scan_parameters {
	gboolean is_infected;

	struct cl_limits limits;
#ifdef NEW_CLAMAV
	struct cl_engine *engine;
#else
	struct cl_node *root;
#endif
	gboolean scan_archive;
};

static gboolean scan_func(GNode *node, gpointer data)
{
	struct scan_parameters *params = (struct scan_parameters *) data;
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	gchar *outfile;
	int ret;
	unsigned long int size;
	const char *virname;

	outfile = procmime_get_tmp_file_name(mimeinfo);
	if (procmime_get_part(outfile, mimeinfo) < 0)
		g_warning("Can't get the part of multipart message.");
	else {
		debug_print("Scanning %s\n", outfile);
#ifdef NEW_CLAMAV
    		if ((ret = cl_scanfile(outfile, &virname, &size, params->engine, 
				      &params->limits, params->scan_archive)) == CL_VIRUS) {
#else
    		if ((ret = cl_scanfile(outfile, &virname, &size, params->root, 
				      &params->limits, params->scan_archive)) == CL_VIRUS) {
#endif
				params->is_infected = TRUE;
			debug_print("Detected %s virus.\n", virname); 
    		} else {
			debug_print("No virus detected.\n");
			if (ret != CL_CLEAN)
				debug_print("Error: %s\n", cl_strerror(ret));
    		}

		g_unlink(outfile);
	}

	return params->is_infected;
}

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	MimeInfo *mimeinfo;

	struct scan_parameters params;

	if (!config.clamav_enable)
		return FALSE;

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return FALSE;

	debug_print("Scanning message %d for viruses\n", msginfo->msgnum);
	if (message_callback != NULL)
		message_callback(_("ClamAV: scanning message..."));

	params.is_infected = FALSE;
#ifdef NEW_CLAMAV
	params.engine = engine;
#else
	params.root = cl_database;
#endif

    	params.limits.maxfiles = 1000; /* max files */
    	params.limits.maxfilesize = config.clamav_max_size * 1048576; /* maximum archived file size */
    	params.limits.maxreclevel = 8; /* maximum recursion level */
	params.limits.maxratio = 200; /* maximal compression ratio */

	if (config.clamav_enable_arc)
		params.scan_archive = TRUE;

	g_node_traverse(mimeinfo->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, scan_func, &params);

	if (params.is_infected) {
		if (config.clamav_recv_infected) {
			FolderItem *clamav_save_folder;

			if ((!config.clamav_save_folder) ||
			    (config.clamav_save_folder[0] == '\0') ||
			    ((clamav_save_folder = folder_find_item_from_identifier(config.clamav_save_folder)) == NULL))
				    clamav_save_folder = folder_get_default_trash();

			procmsg_msginfo_unset_flags(msginfo, ~0, 0);
			msginfo->filter_op = IS_MOVE;
			msginfo->to_filter_folder = clamav_save_folder;
		} else {
			folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		}
	}
	
	procmime_mimeinfo_free_all(mimeinfo);
	
	return params.is_infected;
}

ClamAvConfig *clamav_get_config(void)
{
	return &config;
}

void clamav_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving ClamAV Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "ClamAV") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write ClamAV configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

void clamav_set_message_callback(MessageCallback callback)
{
	message_callback = callback;
}

#ifdef NEW_CLAMAV
int cl_build(struct cl_engine *engine);
void cl_free(struct cl_engine *engine);
#else
int cl_build(struct cl_node *root);
void cl_free(struct cl_node *root);
#endif

gint plugin_init(gchar **error)
{
	gchar *rcpath;
	int ret;
#ifdef NEW_CLAMAV
	unsigned int sigs = 0;
#else
	unsigned int no;
#endif

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup(_("Failed to register mail filtering hook"));
		return -1;
	}

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "ClamAV", rcpath, NULL);
	g_free(rcpath);

	clamav_gtk_init();

#ifdef NEW_CLAMAV
    	if ((ret = cl_load(cl_retdbdir(), &engine, &sigs, CL_DB_STDOPT))) {
		debug_print("cl_load: %s\n", cl_strerror(ret));
		*error = g_strdup_printf("cl_load: %s\n", cl_strerror(ret));
		return -1;
    	}
    	debug_print("Database loaded (containing in total %d signatures)\n", sigs);
#else
    	if ((ret = cl_loaddbdir(cl_retdbdir(), &cl_database, &no))) {
		debug_print("cl_loaddbdir: %s\n", cl_strerror(ret));
		*error = g_strdup_printf("cl_loaddbdir: %s\n", cl_strerror(ret));
		return -1;
    	}
    	debug_print("Database loaded (containing in total %d signatures)\n", no);
#endif


#ifdef NEW_CLAMAV
    	if((ret = cl_build(engine))) {
#else
    	if((ret = cl_build(cl_database))) {
#endif
		debug_print("Database initialization error: %s\n", cl_strerror(ret));
		*error = g_strdup_printf("Database initialization error: %s\n", cl_strerror(ret));
		return -1;
    	}

	debug_print("ClamAV plugin loaded\n");

	return 0;
	
}

gboolean plugin_done(void)
{
	hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	g_free(config.clamav_save_folder);
#ifdef NEW_CLAMAV
	cl_free(engine);
#else
	cl_free(cl_database);
#endif
	clamav_gtk_done();

	debug_print("ClamAV plugin unloaded\n");
	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

const gchar *plugin_desc(void)
{
	return _("This plugin uses Clam AntiVirus to scan all messages that are "
	       "received from an IMAP, LOCAL or POP account.\n"
	       "\n"
	       "When a message attachment is found to contain a virus it can be "
	       "deleted or saved in a specially designated folder.\n"
	       "\n"
	       "Options can be found in /Configuration/Preferences/Plugins/Clam AntiVirus");
}

const gchar *plugin_type(void)
{
	return "GTK2";
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
		{ {PLUGIN_FILTERING, N_("Virus detection")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
