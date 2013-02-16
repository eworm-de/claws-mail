/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2008 Michael Rasmussen and the Claws Mail Team
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>

#include "gtkutils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"
#include "alertpanel.h"
#include "utils.h"
#include "filesel.h"

#include "archiver_prefs.h"

#define PREFS_BLOCK_NAME "Archiver"

ArchiverPrefs archiver_prefs;

struct ArchiverPrefsPage {
        PrefsPage page;
        GtkWidget *save_folder;
	gint compression;
	GtkWidget *zip_radiobtn;
	GtkWidget *bzip_radiobtn;
#if NEW_ARCHIVE_API
        GtkWidget *compress_radiobtn;
#endif
	GtkWidget *none_radiobtn;
	GtkWidget *tar_radiobtn;
	GtkWidget *shar_radiobtn;
	GtkWidget *cpio_radiobtn;
	GtkWidget *pax_radiobtn;
	GtkWidget *recursive_chkbtn;
	GtkWidget *md5sum_chkbtn;
	GtkWidget *rename_chkbtn;
        GtkWidget *unlink_chkbtn;
};

struct ArchiverPrefsPage archiver_prefs_page;

static void create_archiver_prefs_page			(PrefsPage *page,
				      			 GtkWindow *window,
				      			 gpointer   data);
static void destroy_archiver_prefs_page			(PrefsPage *page);
static void save_archiver_prefs				(PrefsPage *page);

static PrefParam param[] = {
	{"save_folder", NULL, &archiver_prefs.save_folder, P_STRING, NULL, NULL, NULL},
	{"compression", "0", &archiver_prefs.compression, P_ENUM, NULL, NULL, NULL},
	{"format", "0", &archiver_prefs.format, P_ENUM, NULL, NULL, NULL},
	{"recursive", "TRUE", &archiver_prefs.recursive, P_BOOL, NULL, NULL, NULL},
	{"md5sum",  "FALSE", &archiver_prefs.md5sum, P_BOOL, NULL, NULL, NULL},
	{"rename", "FALSE", &archiver_prefs.rename, P_BOOL, NULL, NULL, NULL},
	{"unlink", "FALSE", &archiver_prefs.unlink, P_BOOL, NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

void archiver_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = _("Mail Archiver");
	path[2] = NULL;

        prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
        prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);
        
        archiver_prefs_page.page.path = path;
        archiver_prefs_page.page.create_widget = create_archiver_prefs_page;
        archiver_prefs_page.page.destroy_widget = destroy_archiver_prefs_page;
        archiver_prefs_page.page.save_page = save_archiver_prefs;
	archiver_prefs_page.page.weight = 30.0;
        
        prefs_gtk_register_page((PrefsPage *) &archiver_prefs_page);
}

void archiver_prefs_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &archiver_prefs_page);
}

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	struct ArchiverPrefsPage *page = (struct ArchiverPrefsPage *) data;
	gchar *startdir = NULL;
	gchar *dirname;
	gchar *tmp;
	
	if (archiver_prefs.save_folder && *archiver_prefs.save_folder)
		startdir = g_strconcat(archiver_prefs.save_folder,
				       G_DIR_SEPARATOR_S, NULL);
	else
		startdir = g_strdup(get_home_dir());

	dirname = filesel_select_file_save_folder(_("Select destination folder"), startdir);
	if (!dirname) {
		g_free(startdir);
		return;
	}
	if (!is_dir_exist(dirname)) {
		alertpanel_error(_("'%s' is not a directory."),dirname);
		g_free(dirname);
		g_free(startdir);
		return;
	}
	if (dirname[strlen(dirname)-1] == G_DIR_SEPARATOR)
		dirname[strlen(dirname)-1] = '\0';
	g_free(startdir);

	tmp =  g_filename_to_utf8(dirname,-1, NULL, NULL, NULL);
	gtk_entry_set_text(GTK_ENTRY(page->save_folder), tmp);

	g_free(dirname);
	g_free(tmp);
}

static void create_archiver_prefs_page(PrefsPage * _page,
				       GtkWindow *window,
                                       gpointer data)
{
	struct ArchiverPrefsPage *page = (struct ArchiverPrefsPage *) _page;
        GtkWidget *vbox1, *vbox2;
	GtkWidget *hbox1;
	GtkWidget *save_folder_label;
  	GtkWidget *save_folder;
  	GtkWidget *save_folder_select;
	GtkWidget *frame;
	GSList    *compression_group = NULL;
	GtkWidget *zip_radiobtn;
	GtkWidget *bzip_radiobtn;
#if NEW_ARCHIVE_API
        GtkWidget *compress_radiobtn;
#endif
	GtkWidget *none_radiobtn;
	GSList    *format_group = NULL;
	GtkWidget *tar_radiobtn;
	GtkWidget *shar_radiobtn;
	GtkWidget *cpio_radiobtn;
	GtkWidget *pax_radiobtn;
	GtkWidget *recursive_chkbtn;
	GtkWidget *md5sum_chkbtn;
	GtkWidget *rename_chkbtn;
        GtkWidget *unlink_chkbtn;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

  	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

 	save_folder_label = gtk_label_new(_("Default save folder"));
	gtk_widget_show (save_folder_label);
	gtk_box_pack_start (GTK_BOX (hbox1), save_folder_label, FALSE, FALSE, 0);

  	save_folder = gtk_entry_new ();
	gtk_widget_show (save_folder);
	gtk_box_pack_start (GTK_BOX (hbox1), save_folder, TRUE, TRUE, 0);

	save_folder_select = gtkut_get_browse_directory_btn(_("_Select"));
	gtk_widget_show (save_folder_select);
  	gtk_box_pack_start (GTK_BOX (hbox1), save_folder_select, FALSE, FALSE, 0);
	CLAWS_SET_TIP(save_folder_select,
			     _("Click this button to select the default location for saving archives"));

	g_signal_connect(G_OBJECT(save_folder_select), "clicked", 
			 G_CALLBACK(foldersel_cb), page);

	if (archiver_prefs.save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder),
				   archiver_prefs.save_folder);

	PACK_FRAME (vbox1, frame, _("Default compression"));

	hbox1 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox1);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	zip_radiobtn = gtk_radio_button_new_with_label(compression_group, "ZIP");
	compression_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(zip_radiobtn));
	gtk_widget_show(zip_radiobtn);
 	gtk_box_pack_start(GTK_BOX (hbox1), zip_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(zip_radiobtn,
			_("Choose this option to use ZIP compression by default"));

	bzip_radiobtn = gtk_radio_button_new_with_label(compression_group, "BZIP2");
	compression_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(bzip_radiobtn));
	gtk_widget_show(bzip_radiobtn);
	gtk_box_pack_start(GTK_BOX (hbox1), bzip_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(bzip_radiobtn,
			_("Choose this option to use BZIP2 compression by default"));

#if NEW_ARCHIVE_API
	compress_radiobtn = gtk_radio_button_new_with_label(compression_group, "COMPRESS");
	compression_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(compress_radiobtn));
	gtk_widget_show(compress_radiobtn);
	gtk_box_pack_start(GTK_BOX (hbox1), compress_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(compress_radiobtn,
			_("Choose this option to use COMPRESS compression by default"));
#endif

        none_radiobtn = gtk_radio_button_new_with_label(compression_group, _("None"));
	compression_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(none_radiobtn));
	gtk_widget_show(none_radiobtn);
	gtk_box_pack_start(GTK_BOX (hbox1), none_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(none_radiobtn,
			_("Choose this option to disable compression by default"));

	switch (archiver_prefs.compression) {
	case COMPRESSION_ZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(zip_radiobtn), TRUE);
		break;
	case COMPRESSION_BZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bzip_radiobtn), TRUE);
		break;
#if NEW_ARCHIVE_API
        case COMPRESSION_COMPRESS:       
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compress_radiobtn), TRUE);
		break;
#endif
	case COMPRESSION_NONE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(none_radiobtn), TRUE);
		break;
	}

	PACK_FRAME (vbox1, frame, _("Default format"));

	hbox1 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox1);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	tar_radiobtn = gtk_radio_button_new_with_label(format_group, "TAR");
	format_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(tar_radiobtn));
	gtk_widget_show(tar_radiobtn);
 	gtk_box_pack_start(GTK_BOX (hbox1), tar_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(tar_radiobtn,
			_("Choose this option to use the TAR format by default"));

	shar_radiobtn = gtk_radio_button_new_with_label(format_group, "SHAR");
	format_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(shar_radiobtn));
	gtk_widget_show(shar_radiobtn);
 	gtk_box_pack_start(GTK_BOX (hbox1), shar_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(shar_radiobtn,
			_("Choose this option to use the SHAR format by default"));

	cpio_radiobtn = gtk_radio_button_new_with_label(format_group, "CPIO");
	format_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(cpio_radiobtn));
	gtk_widget_show(cpio_radiobtn);
 	gtk_box_pack_start(GTK_BOX (hbox1), cpio_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(cpio_radiobtn,
			_("Choose this option to use the CPIO format by default"));

	pax_radiobtn = gtk_radio_button_new_with_label(format_group, "PAX");
	format_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(pax_radiobtn));
	gtk_widget_show(pax_radiobtn);
 	gtk_box_pack_start(GTK_BOX (hbox1), pax_radiobtn, FALSE, FALSE, 0);
	CLAWS_SET_TIP(pax_radiobtn,
			_("Choose this option to use the PAX format by default"));

	switch (archiver_prefs.format) {
	case FORMAT_TAR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tar_radiobtn), TRUE);
		break;
	case FORMAT_SHAR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shar_radiobtn), TRUE);
		break;
	case FORMAT_CPIO:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cpio_radiobtn), TRUE);
		break;
	case FORMAT_PAX:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pax_radiobtn), TRUE);
		break;
	}

	PACK_FRAME (vbox1, frame, _("Default miscellaneous options"));

	hbox1 = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox1);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	PACK_CHECK_BUTTON(hbox1, recursive_chkbtn, _("Recursive"));
	CLAWS_SET_TIP(recursive_chkbtn,
		_("Choose this option to include subfolders in the archives by default"));
	PACK_CHECK_BUTTON(hbox1, md5sum_chkbtn, _("MD5sum"));
	CLAWS_SET_TIP(md5sum_chkbtn,
		_("Choose this option to add MD5 checksums for each file in the archives by default.\n"
		  "Be aware though, that this dramatically increases the time it\n"
		  "will take to create the archives"));

	PACK_CHECK_BUTTON(hbox1, rename_chkbtn, _("Rename"));
	CLAWS_SET_TIP(rename_chkbtn,
		_("Choose this option to use descriptive names for each file in the archive.\n"
		  "The naming scheme: date_from@to@subject.\n"
		  "Names will be truncated to max 96 characters"));

	PACK_CHECK_BUTTON(hbox1, unlink_chkbtn, _("Delete"));
	CLAWS_SET_TIP(unlink_chkbtn,
		_("Choose this option to delete mails after archiving"));

	if (archiver_prefs.recursive)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recursive_chkbtn), TRUE);
	if (archiver_prefs.md5sum)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(md5sum_chkbtn), TRUE);
	if (archiver_prefs.rename)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rename_chkbtn), TRUE);
	if (archiver_prefs.unlink)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(unlink_chkbtn), TRUE);


	page->save_folder = save_folder;
	page->zip_radiobtn = zip_radiobtn;
	page->bzip_radiobtn = bzip_radiobtn;
#if NEW_ARCHIVE_API
        page->compress_radiobtn = compress_radiobtn;
#endif
	page->none_radiobtn = none_radiobtn;
	page->tar_radiobtn = tar_radiobtn;
	page->shar_radiobtn = shar_radiobtn;
	page->cpio_radiobtn = cpio_radiobtn;
	page->pax_radiobtn = pax_radiobtn;
	page->recursive_chkbtn = recursive_chkbtn;
	page->md5sum_chkbtn = md5sum_chkbtn;
	page->rename_chkbtn = rename_chkbtn;
        page->unlink_chkbtn = unlink_chkbtn;

	page->page.widget = vbox1;
}

static void destroy_archiver_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}

static void save_archiver_prefs(PrefsPage * _page)
{
	struct ArchiverPrefsPage *page = (struct ArchiverPrefsPage *) _page;
        PrefFile *pref_file;
        gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
                                          COMMON_RC, NULL);

	archiver_prefs.save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->zip_radiobtn)))
		archiver_prefs.compression = COMPRESSION_ZIP;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->bzip_radiobtn)))
		archiver_prefs.compression = COMPRESSION_BZIP;
#if NEW_ARCHIVE_API
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->compress_radiobtn)))
		archiver_prefs.compression = COMPRESSION_COMPRESS;
#endif
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->none_radiobtn)))
		archiver_prefs.compression = COMPRESSION_NONE;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->tar_radiobtn)))
		archiver_prefs.format = FORMAT_TAR;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->shar_radiobtn)))
		archiver_prefs.format = FORMAT_SHAR;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->cpio_radiobtn)))
		archiver_prefs.format = FORMAT_CPIO;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->pax_radiobtn)))
		archiver_prefs.format = FORMAT_PAX;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->recursive_chkbtn)))
		archiver_prefs.recursive = TRUE;
	else
		archiver_prefs.recursive = FALSE;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->md5sum_chkbtn)))
		archiver_prefs.md5sum = TRUE;
	else
		archiver_prefs.md5sum = FALSE;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->rename_chkbtn)))
		archiver_prefs.rename = TRUE;
	else
		archiver_prefs.rename = FALSE;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->unlink_chkbtn)))
		archiver_prefs.unlink = TRUE;
	else
		archiver_prefs.unlink = FALSE;


        pref_file = prefs_write_open(rc_file_path);
        g_free(rc_file_path);
        
        if (!(pref_file) ||
	    (prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0))
          return;
        
        if (prefs_write_param(param, pref_file->fp) < 0) {
          g_warning("failed to write Archiver Plugin configuration\n");
          prefs_file_close_revert(pref_file);
          return;
        }
        if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rc_file_path, "fprintf");
		prefs_file_close_revert(pref_file);
	} else
	        prefs_file_close(pref_file);

}
