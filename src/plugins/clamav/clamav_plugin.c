/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <clamav.h>

#include "common/plugin.h"
#include "common/utils.h"
#include "hooks.h"
#include "inc.h"
#include "mimeview.h"
#include "folder.h"
#include "prefs.h"
#include "prefs_gtk.h"

#include "clamav_plugin.h"

static gint hook_id;

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

#define IS_FIRST_PART_TEXT(info) \
	((info->mime_type == MIME_TEXT || info->mime_type == MIME_TEXT_HTML || \
	  info->mime_type == MIME_TEXT_ENRICHED) || \
	 (info->mime_type == MIME_MULTIPART && info->content_type && \
	  !strcasecmp(info->content_type, "multipart/alternative") && \
	  (info->children && \
	   (info->children->mime_type == MIME_TEXT || \
	    info->children->mime_type == MIME_TEXT_HTML || \
	    info->children->mime_type == MIME_TEXT_ENRICHED))))

static gboolean mail_filtering_hook(gpointer source, gpointer data)
{
	MailFilteringData *mail_filtering_data = (MailFilteringData *) source;
	MsgInfo *msginfo = mail_filtering_data->msginfo;
	gboolean is_infected = FALSE;
	FILE  *fp;
	MimeInfo *mimeinfo;
	MimeInfo *child;
	gchar *infile;
	gchar *outfile;
	gint scan_archive = 0;

	int ret, no;
	unsigned long int size;
	long double kb;
	char *virname;
	struct cl_node *root = NULL;
	struct cl_limits limits;

	if (!config.clamav_enable)
		return FALSE;

	fp = procmsg_open_message(msginfo);
	if (fp == NULL) {
		debug_print("failed to open message file");
	} 
	
	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return FALSE;

	child = mimeinfo->children;
	if (!child || IS_FIRST_PART_TEXT(mimeinfo)) {
		procmime_mimeinfo_free_all(mimeinfo);
		return FALSE;
	}

	debug_print("Scanning message %d for viruses\n", msginfo->msgnum);

	if (IS_FIRST_PART_TEXT(child))
		child = child->next;

	infile = procmsg_get_message_file_path(msginfo);


    	if((ret = cl_loaddbdir(cl_retdbdir(), &root, &no))) {
		debug_print("cl_loaddbdir: %s\n", cl_perror(ret));
		exit(2);
    	}

    	debug_print("Loaded %d viruses.\n", no);

    	cl_buildtrie(root);

    	limits.maxfiles = 1000; /* max files */
    	limits.maxfilesize = config.clamav_max_size * 1048576; /* maximum archived file size */
    	limits.maxreclevel = 8; /* maximum recursion level */

	if (config.clamav_enable_arc)
		scan_archive = TRUE;

	while (child != NULL) {
		if (child->children || child->mime_type == MIME_MULTIPART) {
			child = procmime_mimeinfo_next(child);
			continue;
		}
		if(child->parent && child->parent->parent
		&& !strcasecmp(child->parent->parent->content_type, "multipart/signed")
		&& child->mime_type == MIME_TEXT) {
			/* this is the main text part of a signed message */
			child = procmime_mimeinfo_next(child);
			continue;
		}
		outfile = procmime_get_tmp_file_name(child);
		if (procmime_get_part(outfile, infile, child) < 0)
			g_warning("Can't get the part of multipart message.");
		else {
			debug_print("Scanning %s\n", outfile);
    	 		if ((ret = cl_scanfile(outfile, &virname, &size, root, 
					      &limits, scan_archive)) == CL_VIRUS) {
				is_infected = TRUE;
				debug_print("Detected %s virus.\n", virname); 
    			} else {
				debug_print("No virus detected.\n");
				if (ret != CL_CLEAN)
	    				debug_print("Error: %s\n", cl_perror(ret));
    			}

    			kb = size * (CL_COUNT_PRECISION / 1024);
    			debug_print("Data scanned: %2.2Lf Kb\n", kb);
    
			unlink(outfile);

			if (is_infected) break;
			child = child->next;
		}
	}

	if (is_infected) {
		debug_print("message part(s) infected with %s\n", virname);
		if (config.clamav_recv_infected) {
			FolderItem *clamav_save_folder;

			if ((!config.clamav_save_folder) ||
			    (config.clamav_save_folder[0] == '\0') ||
			    ((clamav_save_folder = folder_find_item_from_identifier(config.clamav_save_folder)) == NULL))
				    clamav_save_folder = folder_get_default_trash();

			procmsg_msginfo_unset_flags(msginfo, ~0, 0);
			folder_item_move_msg(clamav_save_folder, msginfo);
		} else {
			folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		}
	}
	
    	cl_freetrie(root);
	g_free(infile);
	procmime_mimeinfo_free_all(mimeinfo);
	
	return is_infected;
}

#undef IS_FIRST_PART_TEXT

ClamAvConfig *clamav_get_config()
{
	return &config;
}

void clamav_save_config()
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
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

gint plugin_init(gchar **error)
{
	hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST, mail_filtering_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup("Failed to register mail filtering hook");
		return -1;
	}

	prefs_set_default(param);
	prefs_read_config(param, "ClamAV", COMMON_RC);

	debug_print("ClamAV plugin loaded\n");

	return 0;
	
}

void plugin_done()
{
	hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, hook_id);
	g_free(config.clamav_save_folder);
	
	debug_print("ClamAV plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return "Clam AntiVirus";
}

const gchar *plugin_desc(void)
{
	return "This plugin uses Clam AntiVirus to scan all message attachments "
	       "that are received from a POP account.\n"
	       "\n"
	       "When a message attachment is found to contain a virus it can be "
	       "deleted or saved in a specially designated folder.\n"
	       "\n"
	       "This plugin only contains the actual function for scanning "
	       "and deleting or moving the message. You probably want to load "
	       "the Gtk+ User Interface plugin too, otherwise you will have to "
	       "manually write the plugin configuration.\n";
}

const gchar *plugin_type(void)
{
	return "Common";
}
