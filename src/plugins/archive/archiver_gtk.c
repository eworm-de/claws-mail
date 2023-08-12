/* vim: set textwidth=80 tabstop=4 */

/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 Michael Rasmussen and the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ii*
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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "gtk/gtkutils.h"
#include "common/claws.h"
#include "common/version.h"
#include "common/md5.h"
#include "plugin.h"
#include "mainwindow.h"
#include "file-utils.h"
#include "utils.h"
#include "prefs.h"
#include "folder.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "libarchive_archive.h"
#include "archiver.h"
#include "archiver_prefs.h"
#include "alertpanel.h"

#include <archive.h>

typedef struct _progress_widget progress_widget;
struct _progress_widget {
	GtkWidget*	progress_dialog;
	GtkWidget*	frame;
	GtkWidget*	vbox1;
	GtkWidget*	hbox1;
	GtkWidget*	add_label;
	GtkWidget*	file_label;
	GtkWidget*	progress;
	guint		position;
};

typedef enum {
    A_FILE_OK           = 1 << 0,
    A_FILE_EXISTS       = 1 << 1,
    A_FILE_IS_LINK      = 1 << 2,
    A_FILE_IS_DIR       = 1 << 3,
    A_FILE_NO_WRITE     = 1 << 4,
    A_FILE_UNKNOWN      = 1 << 5
} AFileTest;

static progress_widget* progress = NULL;

static progress_widget* init_progress() {
	progress_widget* ptr = malloc(sizeof(*ptr));

	debug_print("creating progress struct\n");
	ptr->progress_dialog = NULL;
	ptr->frame = NULL;
	ptr->vbox1 = NULL;
	ptr->hbox1 = NULL;
	ptr->add_label = NULL;
	ptr->file_label = NULL;
	ptr->progress = NULL;
	ptr->position = 0;

	return ptr;
}

static void progress_dialog_cb(GtkWidget* widget, gint action, gpointer data) {
	struct ArchivePage* page = (struct ArchivePage *) data;

	debug_print("Cancel operation\n");
	stop_archiving();
	page->cancelled = TRUE;
	archive_free_file_list(page->md5, page->rename);
	gtk_widget_destroy(widget);
}

static void create_progress_dialog(struct ArchivePage* page) {
	gchar* title = g_strdup_printf("%s %s", _("Archiving"), page->path);
	MainWindow* mainwin = mainwindow_get_mainwindow();
	GtkWidget *content_area;

	progress->position = 0;
	progress->progress_dialog = gtk_dialog_new_with_buttons (
				title,
				GTK_WINDOW(mainwin->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				_("_Cancel"),
				GTK_RESPONSE_CANCEL,
				NULL);

	g_signal_connect (
				progress->progress_dialog,
				"response",
				G_CALLBACK(progress_dialog_cb),
				page);

	progress->frame = gtk_frame_new(_("Press Cancel button to stop archiving"));
	gtk_frame_set_shadow_type(GTK_FRAME(progress->frame),
					GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(progress->frame), 4);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(progress->progress_dialog));
	gtk_container_add(GTK_CONTAINER(content_area), progress->frame);

	progress->vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width (GTK_CONTAINER (progress->vbox1), 4);
	gtk_container_add(GTK_CONTAINER(progress->frame), progress->vbox1);
	
	progress->hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_set_border_width(GTK_CONTAINER(progress->hbox1), 8);
	gtk_box_pack_start(GTK_BOX(progress->vbox1),
					progress->hbox1, FALSE, FALSE, 0);

	progress->add_label = gtk_label_new(_("Archiving:"));
	gtk_box_pack_start(GTK_BOX(progress->hbox1),
					progress->add_label, FALSE, FALSE, 0);

	progress->file_label = gtk_label_new("");
	gtk_label_set_ellipsize(GTK_LABEL(progress->file_label),
					PANGO_ELLIPSIZE_START);
	gtk_box_pack_start(GTK_BOX(progress->hbox1),
					progress->file_label, TRUE, TRUE, 0);

	progress->hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_set_border_width(GTK_CONTAINER(progress->hbox1), 8);
	gtk_box_pack_start(GTK_BOX(progress->vbox1),
					progress->hbox1, FALSE, FALSE, 0);

	progress->progress = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(progress->hbox1), 
					progress->progress, TRUE, TRUE, 0);
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(progress->progress), 0.25);

	gtk_window_set_default_size(GTK_WINDOW(progress->progress_dialog), 400, 80);
	gtk_widget_show_all(progress->progress_dialog);
}

static struct ArchivePage* init_archive_page() {
	struct ArchivePage* page = malloc(sizeof(struct ArchivePage));

	debug_print("creating ArchivePage\n");
	page->path = NULL;
	page->name = NULL;
	page->file = NULL;
	page->folder = NULL;
	page->response = FALSE;
	page->force_overwrite = FALSE;
	page->compress_methods = NULL;
	page->archive_formats = NULL;
	page->recursive = NULL;
	page->cancelled = FALSE;
	page->md5 = FALSE;
	page->md5sum = NULL;
	page->rename = FALSE;
	page->rename_files = NULL;
        page->isoDate = NULL;
        page->unlink_files = NULL;
        page->unlink = FALSE;

	return page;
}

static void dispose_archive_page(struct ArchivePage* page) {
	debug_print("freeing ArchivePage\n");
	if (page->path)
		g_free(page->path);
	page->path = NULL;
	if (page->name)
		g_free(page->name);
	page->name = NULL;
	g_free(page);
}

static gboolean valid_file_name(gchar* file) {
	int i;

	for (i = 0; INVALID_UNIX_CHARS[i] != '\0'; i++) {
		if (g_utf8_strchr(file,	g_utf8_strlen(file, -1), INVALID_UNIX_CHARS[i]))
			return FALSE;
	}
	return TRUE;
}

static COMPRESS_METHOD get_compress_method(GSList* btn) {
	const gchar* name;

	while (btn) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn->data))) {
			name = gtk_widget_get_name(GTK_WIDGET(btn->data));
			if (strcmp("GZIP", name) == 0) {
				debug_print("GZIP compression enabled\n");
				return GZIP;
			}
			else if (strcmp("BZIP", name) == 0) {
				debug_print("BZIP2 compression enabled\n");
				return BZIP2;
			}
            else if (strcmp("COMPRESS", name) == 0) {
				debug_print("COMPRESS compression enabled\n");
				return COMPRESS;
			}
#if ARCHIVE_VERSION_NUMBER >= 2006990
			else if (strcmp("LZMA", name) == 0) {
				debug_print("LZMA compression enabled\n");
				return LZMA;
			}
			else if (strcmp("XZ", name) == 0) {
				debug_print("XZ compression enabled\n");
				return XZ;
			}
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
			else if (strcmp("LZIP", name) == 0) {
				debug_print("LZIP compression enabled\n");
				return LZIP;
			}
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
			else if (strcmp("LRZIP", name) == 0) {
				debug_print("LRZIP compression enabled\n");
				return LRZIP;
			}
			else if (strcmp("LZOP", name) == 0) {
				debug_print("LZOP compression enabled\n");
				return LZOP;
			}
			else if (strcmp("GRZIP", name) == 0) {
				debug_print("GRZIP compression enabled\n");
				return GRZIP;
			}
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
			else if (strcmp("LZ4", name) == 0) {
				debug_print("LZ4 compression enabled\n");
				return LZ4;
			}
#endif
			else if (strcmp("NONE", name) == 0) {
				debug_print("Compression disabled\n");
				return NO_COMPRESS;
			}
		}
		btn = g_slist_next(btn);
	}
	return NO_COMPRESS;
}

static ARCHIVE_FORMAT get_archive_format(GSList* btn) {
	const gchar* name;

	while (btn) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn->data))) {
			name = gtk_widget_get_name(GTK_WIDGET(btn->data));
			if (strcmp("TAR", name) == 0) {
				debug_print("TAR archive enabled\n");
				return TAR;
			}
			else if (strcmp("SHAR", name) == 0) {
				debug_print("SHAR archive enabled\n");
				return SHAR;
			}
			else if (strcmp("PAX", name) == 0) {
				debug_print("PAX archive enabled\n");
				return PAX;
			}
			else if (strcmp("CPIO", name) == 0) {
				debug_print("CPIO archive enabled\n");
				return CPIO;
			}
		}
		btn = g_slist_next(btn);
	}
	return NO_FORMAT;
}

static void create_md5sum(const gchar* file, const gchar* md5_file) {
	int fd;
	gchar* text = NULL;
	gchar* md5sum = malloc(33);

	debug_print("Creating md5sum file: %s\n", md5_file);
	if (md5_hex_digest_file(md5sum, (const unsigned char *) file) == -1) {
		free(md5sum);
		return;
	}
	debug_print("md5sum: %s\n", md5sum);
	if ((fd = 
		open(md5_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
		FILE_OP_ERROR(md5_file, "create");
		free(md5sum);
		return;
	}
	text = g_strrstr_len(file, strlen(file), "/");
	if (text) {
		text++;
		text = g_strdup_printf("%s  %s\n", md5sum, text);
	}
	else
		text = g_strdup_printf("%s  %s\n", md5sum, file);
	g_free(md5sum);
	debug_print("md5sum: %s\n", text);
	if (write(fd, text, strlen(text)) < 0)
		FILE_OP_ERROR(md5_file, "write");
	close(fd);
	g_free(text);
}

static gchar* descriptive_file_name(
		struct ArchivePage* page, const gchar* file, MsgInfo *msginfo) {
	gchar* new_file = NULL;
	gchar *name, *p, *to, *from, *date, *subject;

	debug_print("renaming file\n");
	p = g_strrstr_len(file, strlen(file), "/");
	p = g_strndup(file, p - file);
	if (!p)
		return NULL;
	if (msginfo->to) {
		to = g_strdup(msginfo->to);
		extract_address(to);
	}
	else
		to = g_strdup("");
	if (msginfo->from) {
		from = g_strdup(msginfo->from);
		extract_address(from);
	}
	else
		from = g_strdup("");
	if (msginfo->date) {
		date = g_strdup(msginfo->date);
		subst_for_shellsafe_filename(date);
		/* if not on windows we need to subst some more */
		subst_chars(date, ":", '_');
	}
	else
		date = g_strdup("");
	if (msginfo->subject) {
		subject = g_strdup(msginfo->subject);
		subst_for_shellsafe_filename(subject);
		/* if not on windows we need to subst some more */
		subst_chars(subject, ":", '_');
	}
	else
		subject = g_strdup("");
	name = g_strdup_printf("%s_%s@%s@%s", date, from, to, subject);
	/* ensure file name is not larger than 96 chars (max file name size
	 * is 100 chars but reserve for .md5)
	 */
	if (strlen(name) > 96)
		name[96] = 0;
	
	new_file = g_strconcat(p, "/", name, NULL);

	g_free(name);
	g_free(p);
	g_free(to);
	g_free(from);
	g_free(date);
	g_free(subject);

	debug_print("New_file: %s\n", new_file);
	if (link(file, new_file) != 0) {
		if (errno != EEXIST) {
			FILE_OP_ERROR(new_file, "link");
			g_free(new_file);
			new_file = g_strdup(file);
			page->rename = FALSE;
		}
	}

	return new_file;
}

static void walk_folder(struct ArchivePage* page, FolderItem* item,
				gboolean recursive) {
	FolderItem* child;
	GSList *msglist;
	GSList *cur;
	MsgInfo *msginfo;
	GNode* node;
	int count;
	gchar* md5_file = NULL;
	gchar* text = NULL;
	gchar* file = NULL;
        MsgTrash* msg_trash = NULL;
        const gchar* date = NULL;

	if (recursive && ! page->cancelled) {
		debug_print("Scanning recursive\n");
		node = item->node->children;
		while(node && ! page->cancelled) {
			debug_print("Number of nodes: %d\n", g_node_n_children(node));
			if (node->data) {
				child = FOLDER_ITEM(node->data);
				debug_print("new node: %d messages\n", child->total_msgs);
				walk_folder(page, child, recursive);
			}
			node = node->next;
		}
	}
	if (! page->cancelled) {
                date = gtk_entry_get_text(GTK_ENTRY(page->isoDate));
                debug_print("cut-off date: %s\n", date);
		count = 0;
		page->files += item->total_msgs;
		msglist = folder_item_get_msg_list(item);
                msg_trash = new_msg_trash(item);
		for (cur = msglist; cur && ! page->cancelled; cur = cur->next) {
			msginfo = (MsgInfo *) cur->data;
			debug_print("%s_%s_%s_%s\n",
				msginfo->date, msginfo->to, msginfo->from, msginfo->subject);
			file = folder_item_fetch_msg(item, msginfo->msgnum);
                        if (date && strlen(date) > 0 && !before_date(msginfo->date_t, date)) {
                            page->files--;
                            continue;
                        }
			page->total_size += msginfo->size;
			/*debug_print("Processing: %s\n", file);*/
			if (file) {
                                if (page->unlink) {
                                    archive_add_msg_mark(msg_trash, msginfo);
                                }
				if (page->rename) {
					file = descriptive_file_name(page, file, msginfo);
					if (!file) {
                                                /* Could not create a descriptive name */
                                            file = folder_item_fetch_msg(item, msginfo->msgnum);
                                        }
				}
				if (page->md5) {
					md5_file = g_strdup_printf("%s.md5", file);
					create_md5sum(file, md5_file);
					archive_add_file(md5_file);
					g_free(md5_file);
				}
				archive_add_file(file);
				if (page->rename)
					g_free(file);
			}
			if (count % 350 == 0) {
				debug_print("pulse progressbar\n");
				text = g_strdup_printf(
							"Scanning %s: %d files", item->name, count);
				gtk_progress_bar_set_text(
						GTK_PROGRESS_BAR(progress->progress), text);
				g_free(text);
				gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress->progress));
				GTK_EVENTS_FLUSH();
			}
			count++;
		}
		procmsg_msg_list_free(msglist);
	}
}

static AFileTest file_is_writeable(struct ArchivePage* page) {
    int fd;

    if (g_file_test(page->name, G_FILE_TEST_EXISTS) &&
				! page->force_overwrite)
        return A_FILE_EXISTS;
    if (g_file_test(page->name, G_FILE_TEST_IS_SYMLINK))
        return A_FILE_IS_LINK;
    if (g_file_test(page->name, G_FILE_TEST_IS_DIR))
        return A_FILE_IS_DIR;
    if ((fd = open(page->name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1) {
        switch (errno) {
            case EACCES: return A_FILE_NO_WRITE;
            case EEXIST: return A_FILE_OK;
            default:     return A_FILE_UNKNOWN;
        }
    }
    else {
        close(fd);
        claws_unlink(page->name);
    }
    return A_FILE_OK;
}

static gboolean archiver_save_files(struct ArchivePage* page) {
	FolderItem* item;
	COMPRESS_METHOD method;
	ARCHIVE_FORMAT format;
	gboolean recursive;
	guint orig_file;
	GSList* list = NULL;
	const gchar* res = NULL;
	AFileTest perm;
	gchar* msg = NULL;
	gboolean folder_not_set;
	gboolean file_not_set;

	/* check if folder to archive and target archive filename are set */
	folder_not_set = (*gtk_entry_get_text(GTK_ENTRY(page->folder)) == '\0');
	file_not_set = (*gtk_entry_get_text(GTK_ENTRY(page->file)) == '\0');
	if (folder_not_set || file_not_set) {
		alertpanel_error(_("Some uninitialized data prevents from starting\n"
					"the archiving process:\n"
					"%s%s"),
			folder_not_set ? _("\n- the folder to archive is not set") : "",
			file_not_set ? _("\n- the name for archive is not set") : "");
		return FALSE;
	}
	/* sync page struct info with folder and archive name edit widgets */
	if (page->path) {
		g_free(page->path);
		page->path = NULL;
	}
	page->path = g_strdup(gtk_entry_get_text(GTK_ENTRY(page->folder)));
	g_strstrip(page->path);
	debug_print("page->path: %s\n", page->path);

	if (page->name) {
		g_free(page->name);
		page->name = NULL;
	}
	page->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(page->file)));
	g_strstrip(page->name);
	debug_print("page->name: %s\n", page->name);

	if ((perm = file_is_writeable(page)) != A_FILE_OK) {
		switch (perm) {
			case A_FILE_EXISTS:
				msg = g_strdup_printf(_("%s: Exists. Continue anyway?"), page->name);
				break;
			case A_FILE_IS_LINK:
				msg = g_strdup_printf(_("%s: Is a link. Cannot continue"), page->name);
				break;
			 case A_FILE_IS_DIR:
				 msg = g_strdup_printf(_("%s: Is a directory. Cannot continue"), page->name);
				break;
			case A_FILE_NO_WRITE:
				 msg = g_strdup_printf(_("%s: Missing permissions. Cannot continue"), page->name);
				break;
			case A_FILE_UNKNOWN:
				msg = g_strdup_printf(_("%s: Unknown error. Cannot continue"), page->name);
				break;
			default:
				break;
		}
		if (perm == A_FILE_EXISTS) {
			AlertValue aval;

			aval = alertpanel_full(_("Creating archive"), msg,
				NULL, _("_Cancel"), NULL, _("_OK"), NULL, NULL,
				ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING);
			g_free(msg);
			if (aval != G_ALERTALTERNATE)
				return FALSE;
		} else {
			alertpanel_error("%s", msg);
			g_free(msg);
			return FALSE;
		}
	}
	if (! valid_file_name(page->name)) {
		alertpanel_error(_("Not a valid file name:\n%s."), page->name);
		return FALSE;
	}
	item = folder_find_item_from_identifier(page->path);
	if (! item) {
		alertpanel_error(_("Not a valid Claws Mail folder:\n%s."), page->name);
		return FALSE;
	}
	page->files = 0;
	page->total_size = 0;
	page->rename = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->rename_files));
	recursive = gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(page->recursive));
	page->md5 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->md5sum));
	page->unlink = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->unlink_files));
	create_progress_dialog(page);
	walk_folder(page, item, recursive);
	if (page->cancelled)
		return FALSE;
	list = archive_get_file_list();
	orig_file = (page->md5) ? page->files * 2 : page->files;
	debug_print("md5: %d, orig: %d, md5: %d\n", 
					page->md5, page->files, orig_file);
	if (orig_file != g_slist_length(list)) {
		AlertValue aval;

		msg = g_strdup_printf(
				_("Adding files in folder failed\n"
				  "Files in folder: %d\n"
				  "Files in list:   %d\n"
				  "\nContinue anyway?"),
				orig_file, g_slist_length(list));
		aval = alertpanel_full(_("Creating archive"), msg,
			NULL, _("_Cancel"), NULL, _("_OK"), NULL, NULL,
			ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING);
		g_free(msg);
		if (aval != G_ALERTALTERNATE) {
			archive_free_file_list(page->md5, page->rename);
			return FALSE;
		}
	}
	method = get_compress_method(page->compress_methods);
	format = get_archive_format(page->archive_formats);
	if ((res = archive_create(page->name, list, method, format)) != NULL) {
		alertpanel_error(_("Archive creation error:\n%s"), res);
		archive_free_file_list(page->md5, page->rename);
		return FALSE;
	}
	if (page->unlink) {
		archive_free_archived_files();
	}
	return TRUE;
}

static void entry_change_cb(GtkWidget* widget, gpointer data) {
	const gchar* name = gtk_widget_get_name(widget);
	struct ArchivePage* page = (struct ArchivePage *) data;

	if (strcmp("folder", name) == 0) {
		page->path = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
		debug_print("page->folder = %s\n", page->path);
	}
	else if (strcmp("file", name) == 0) {
		page->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
		page->force_overwrite = FALSE;
		debug_print("page->name = %s\n", page->name);
	}
}

static void show_result(struct ArchivePage* page) {
	
	enum {
		STRING1,
		STRING2,
		N_COLUMNS
	};

	GStatBuf st;
	GtkListStore* list;
	GtkTreeIter iter;
	GtkTreeView* view;
	GtkTreeViewColumn* header;
	GtkCellRenderer* renderer;
	GtkWidget* dialog;
	GtkWidget* content_area;
	gchar* msg = NULL;
	gchar* method = NULL;
	gchar* format = NULL;

	MainWindow* mainwin = mainwindow_get_mainwindow();

	switch (get_compress_method(page->compress_methods)) {
		case GZIP:
			method = g_strdup("GZIP");
			break;
		case BZIP2:
			method = g_strdup("BZIP2");
			break;
        case COMPRESS:
			method = g_strdup("Compress");
			break;
#if ARCHIVE_VERSION_NUMBER >= 2006990
		case LZMA:
			method = g_strdup("LZMA");
			break;
		case XZ:
			method = g_strdup("XZ");
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
		case LZIP:
			method = g_strdup("LZIP");
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
		case LRZIP:
			method = g_strdup("LRZIP");
			break;
		case LZOP:
			method = g_strdup("LZOP");
			break;
		case GRZIP:
			method = g_strdup("GRZIP");
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
		case LZ4:
			method = g_strdup("LZ4");
			break;
#endif
		case NO_COMPRESS:
			method = g_strdup("No Compression");
			break;
	}
	
	switch (get_archive_format(page->archive_formats)) {
		case TAR:
			format = g_strdup("TAR");
			break;
		case SHAR:
			format = g_strdup("SHAR");
			break;
		case PAX:
			format = g_strdup("PAX");
			break;
		case CPIO:
			format = g_strdup("CPIO");
			break;
		case NO_FORMAT:
			format = g_strdup("NO FORMAT");
	}

	if (g_stat(page->name, &st) == -1) {
		alertpanel_error("Could not get size of archive file '%s'.", page->name);
		return;
	}
	dialog = gtk_dialog_new_with_buttons(
			_("Archive result"),
			GTK_WINDOW(mainwin->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("_OK"),
			GTK_RESPONSE_NONE,
			NULL);
	g_signal_connect_swapped(
				dialog,
				"response",
				G_CALLBACK(gtk_widget_destroy),
				dialog);

	list = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	
	view = g_object_new(
				GTK_TYPE_TREE_VIEW,
				"model", list,
				"rules-hint", FALSE,
				"headers-clickable", FALSE,
				"reorderable", FALSE,
				"enable-search", FALSE,
				NULL);

	renderer = gtk_cell_renderer_text_new();

	header = gtk_tree_view_column_new_with_attributes(
				_("Attributes"), renderer, "text", STRING1, NULL);
	gtk_tree_view_append_column(view, header);

	header = gtk_tree_view_column_new_with_attributes(
				_("Values"), renderer, "text", STRING2, NULL);
	gtk_tree_view_append_column(view, header);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_add(
				GTK_CONTAINER(content_area), GTK_WIDGET(view));

	gtk_list_store_append(list, &iter);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Archive"),
				STRING2, page->name, -1);

	gtk_list_store_append(list, &iter);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Archive format"),
				STRING2, format, -1);
	g_free(format);

	gtk_list_store_append(list, &iter);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Compression method"),
				STRING2, method, -1);
	g_free(method);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%d", (page->md5) ? page->files * 2 : page->files);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Number of files"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%d byte(s)", (guint) st.st_size);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Archive Size"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%d byte(s)", page->total_size);
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Folder Size"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%d%%", 
					(guint)((st.st_size * 100) / page->total_size));
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Compression level"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%s", (page->md5) ? _("Yes") : _("No"));
	gtk_list_store_set(
				list, &iter,
				STRING1, _("MD5 checksum"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%s", (page->rename) ? _("Yes") : _("No"));
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Descriptive names"),
				STRING2, msg, -1);
	g_free(msg);

	gtk_list_store_append(list, &iter);
	msg = g_strdup_printf("%s", (page->unlink) ? _("Yes") : _("No"));
	gtk_list_store_set(
				list, &iter,
				STRING1, _("Delete selected files"),
				STRING2, msg, -1);
	g_free(msg);
        
	msg = g_strdup(gtk_entry_get_text(GTK_ENTRY(page->isoDate)));
	if (msg) {
	gtk_list_store_append(list, &iter);
	gtk_list_store_set(
			list, &iter,
			STRING1, _("Select mails before"),
			STRING2, msg, -1);
	}
	g_free(msg);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 320, 260);

	gtk_widget_show_all(dialog);
}

static void archiver_dialog_cb(GtkWidget* widget, gint action, gpointer data) {
	struct ArchivePage* page = (struct ArchivePage *) data;
	gboolean result = FALSE;

	switch (action) {
		case GTK_RESPONSE_ACCEPT:
			debug_print("User chose OK\n");
			page->response = TRUE;
			break;
		default:
			debug_print("User chose CANCEL\n");
			page->response = FALSE;
			archiver_gtk_done(page, widget);
			return;
	}
	debug_print("Settings:\naction: %d\n", page->response);
	if (page->response) {
		debug_print("Settings:\nfolder: %s\nname: %s\n",
				(page->path) ? page->path : "(null)",
				(page->name) ? page->name : "(null)");
		result = archiver_save_files(page);
		debug_print("Result->archiver_save_files: %d\n", result);
		if (progress->progress_dialog && 
						GTK_IS_WIDGET(progress->progress_dialog))
			gtk_widget_destroy(progress->progress_dialog);		
		if (result && ! page->cancelled) {
			show_result(page);
			archive_free_file_list(page->md5, page->rename);
			archiver_gtk_done(page, widget);
			return;
		}
		if (page->cancelled) {
			archiver_gtk_done(page, widget);
			archiver_gtk_show();
		}
	}
}

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	struct ArchivePage* page = (struct ArchivePage *) data;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL, FALSE,
			_("Select folder to archive"));
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(page->folder), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(page->folder),
					item_id, strlen(item_id), &newpos);
		page->path = g_strdup(item_id);
		g_free(item_id);
	}
	debug_print("Folder to archive: %s\n", 
				gtk_entry_get_text(GTK_ENTRY(page->folder)));
}

static void filesel_cb(GtkWidget *widget, gpointer data)
{
	GtkFileChooserNative *dialog;
	gchar* file;
	gint newpos = 0;
	struct ArchivePage* page = (struct ArchivePage *) data;

	dialog = gtk_file_chooser_native_new(
		_("Select file name for archive [suffix should reflect archive like .tgz]"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			_("_Apply"),
			_("_Cancel"));

	if (archiver_prefs.save_folder)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
						    archiver_prefs.save_folder);
	else
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
						    get_home_dir());
	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (file) {
			gtk_editable_delete_text(GTK_EDITABLE(page->file), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(page->file),
						file, strlen(file), &newpos);
			page->name = g_strdup(file);
			g_free(file);
			page->force_overwrite = TRUE;
		}
	}
	g_object_unref(dialog);
	debug_print("Name for archive: %s\n",
				gtk_entry_get_text(GTK_ENTRY(page->file)));
}

void set_progress_file_label(const gchar* file) {
	debug_print("IsLabel: %s, Update label: %s\n", 
				GTK_IS_WIDGET(progress->file_label)? "Yes" : "No", file);
	if (GTK_IS_WIDGET(progress->file_label))
		gtk_label_set_text(GTK_LABEL(progress->file_label), file);
}

void set_progress_print_all(guint fraction, guint total, guint step) {
	gchar* text_count;

	if (GTK_IS_WIDGET(progress->progress)) {
		if ((fraction - progress->position) % step == 0) {
			debug_print("frac: %d, total: %d, step: %d, prog->pos: %d\n",
					fraction, total, step, progress->position);
			gtk_progress_bar_set_fraction(
					GTK_PROGRESS_BAR(progress->progress), 
					(total == 0) ? 0 : (gfloat)fraction / (gfloat)total);
			text_count = g_strdup_printf(_("%ld of %ld"), 
					(long) fraction, (long) total);
			gtk_progress_bar_set_text(
					GTK_PROGRESS_BAR(progress->progress), text_count);
			g_free(text_count);
			progress->position = fraction;
			GTK_EVENTS_FLUSH();
		}
	}
}

void archiver_gtk_show() {
	GtkWidget* dialog;
	GtkWidget* frame;
	GtkWidget* vbox1;
	GtkWidget* hbox1;
	GtkWidget* folder_label;
	GtkWidget* folder_select;
	GtkWidget* file_label;
	GtkWidget* file_select;
	GtkWidget* gzip_radio_btn;
	GtkWidget* bzip_radio_btn;
    GtkWidget* compress_radio_btn;
#if ARCHIVE_VERSION_NUMBER >= 2006990
	GtkWidget* lzma_radio_btn;
	GtkWidget* xz_radio_btn;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
	GtkWidget* lzip_radio_btn;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
	GtkWidget* lrzip_radio_btn;
	GtkWidget* lzop_radio_btn;
	GtkWidget* grzip_radio_btn;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
	GtkWidget* lz4_radio_btn;
#endif
	GtkWidget* no_radio_btn;
	GtkWidget* shar_radio_btn;
	GtkWidget* pax_radio_btn;
	GtkWidget* cpio_radio_btn;
	GtkWidget* tar_radio_btn;
	GtkWidget* content_area;
	struct ArchivePage* page;
	MainWindow* mainwin = mainwindow_get_mainwindow();

	/*debug_set_mode(TRUE);*/
	progress = init_progress();

	page = init_archive_page();

	dialog = gtk_dialog_new_with_buttons (
				_("Create Archive"),
				GTK_WINDOW(mainwin->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				_("_Cancel"),
				GTK_RESPONSE_CANCEL,
				_("_OK"),
				GTK_RESPONSE_ACCEPT,
				NULL);

	g_signal_connect (
				dialog,
				"response",
				G_CALLBACK(archiver_dialog_cb),
				page);

	frame = gtk_frame_new(_("Enter Archiver arguments"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_add(GTK_CONTAINER(content_area), frame);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox1);
	
	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

	folder_label = gtk_label_new(_("Folder to archive"));
	gtk_box_pack_start(GTK_BOX(hbox1), folder_label, FALSE, FALSE, 0);
	
	page->folder = gtk_entry_new();
	gtk_widget_set_name(page->folder, "folder");
	gtk_box_pack_start(GTK_BOX(hbox1), page->folder, TRUE, TRUE, 0);
	CLAWS_SET_TIP(page->folder,
			_("Folder which is the root of the archive"));

	folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_box_pack_start(GTK_BOX(hbox1), folder_select, FALSE, FALSE, 0);
	CLAWS_SET_TIP(folder_select,
			_("Click this button to select a folder which is to be root of the archive"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

	file_label = gtk_label_new(_("Name for archive"));
	gtk_box_pack_start(GTK_BOX(hbox1), file_label, FALSE, FALSE, 0);
	
	page->file = gtk_entry_new();
	gtk_widget_set_name(page->file, "file");
	gtk_box_pack_start(GTK_BOX(hbox1), page->file, TRUE, TRUE, 0);
	CLAWS_SET_TIP(page->file, _("Archive location and name"));

	file_select = gtkut_get_browse_directory_btn(_("_Select"));
	gtk_box_pack_start(GTK_BOX(hbox1), file_select, FALSE, FALSE, 0);
	CLAWS_SET_TIP(file_select,
			_("Click this button to select a name and location for the archive"));

	frame = gtk_frame_new(_("Choose compression"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	gzip_radio_btn = gtk_radio_button_new_with_mnemonic(NULL, "G_ZIP");
	gtk_widget_set_name(gzip_radio_btn, "GZIP");
	gtk_box_pack_start(GTK_BOX(hbox1), gzip_radio_btn, FALSE, FALSE, 0);
	archiver_set_tooltip(gzip_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "GZIP"));

	bzip_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "BZIP_2");
	gtk_widget_set_name(bzip_radio_btn, "BZIP");
	gtk_box_pack_start(GTK_BOX(hbox1), bzip_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(bzip_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "BZIP2"));

	compress_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "Com_press");
	gtk_widget_set_name(compress_radio_btn, "COMPRESS");
	gtk_box_pack_start(GTK_BOX(hbox1), compress_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(compress_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "COMPRESS"));

#if ARCHIVE_VERSION_NUMBER >= 2006990
	lzma_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "_LZMA");
	gtk_widget_set_name(lzma_radio_btn, "LZMA");
	gtk_box_pack_start(GTK_BOX(hbox1), lzma_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(lzma_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "LZMA"));

	xz_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "_XZ");
	gtk_widget_set_name(xz_radio_btn, "XZ");
	gtk_box_pack_start(GTK_BOX(hbox1), xz_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(xz_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "XZ"));
#endif

#if ARCHIVE_VERSION_NUMBER >= 3000000
	lzip_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "_LZIP");
	gtk_widget_set_name(lzip_radio_btn, "LZIP");
	gtk_box_pack_start(GTK_BOX(hbox1), lzip_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(lzip_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "LZIP"));
#endif

#if ARCHIVE_VERSION_NUMBER >= 3001000
	lrzip_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "L_RZIP");
	gtk_widget_set_name(lrzip_radio_btn, "LRZIP");
	gtk_box_pack_start(GTK_BOX(hbox1), lrzip_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(lrzip_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "LRZIP"));

	lzop_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "LZ_OP");
	gtk_widget_set_name(lzop_radio_btn, "LZOP");
	gtk_box_pack_start(GTK_BOX(hbox1), lzop_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(lzop_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "LZOP"));

	grzip_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "_GRZIP");
	gtk_widget_set_name(grzip_radio_btn, "GRZIP");
	gtk_box_pack_start(GTK_BOX(hbox1), grzip_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(grzip_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "GRZIP"));
#endif

#if ARCHIVE_VERSION_NUMBER >= 3001900
	lz4_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), "LZ_4");
	gtk_widget_set_name(lz4_radio_btn, "LZ4");
	gtk_box_pack_start(GTK_BOX(hbox1), lz4_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(lz4_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "LZ4"));
#endif

	no_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(gzip_radio_btn), _("_None"));
	gtk_widget_set_name(no_radio_btn, "NONE");
	gtk_box_pack_start(GTK_BOX(hbox1), no_radio_btn, FALSE, FALSE, 0);
        archiver_set_tooltip(no_radio_btn, g_strdup_printf(_("Choose this option to use %s compression for the archive"), "NO"));

	page->compress_methods = 
			gtk_radio_button_get_group(GTK_RADIO_BUTTON(gzip_radio_btn));

	switch (archiver_prefs.compression) {
	case COMPRESSION_GZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gzip_radio_btn), TRUE);
		break;
	case COMPRESSION_BZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bzip_radio_btn), TRUE);
		break;
	case COMPRESSION_COMPRESS:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compress_radio_btn), TRUE);
		break;
#if ARCHIVE_VERSION_NUMBER >= 2006990
	case COMPRESSION_LZMA:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lzma_radio_btn), TRUE);
		break;
	case COMPRESSION_XZ:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xz_radio_btn), TRUE);
		break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
	case COMPRESSION_LZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lzip_radio_btn), TRUE);
		break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
	case COMPRESSION_LRZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lrzip_radio_btn), TRUE);
		break;
	case COMPRESSION_LZOP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lzop_radio_btn), TRUE);
		break;
	case COMPRESSION_GRZIP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grzip_radio_btn), TRUE);
		break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
	case COMPRESSION_LZ4:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lz4_radio_btn), TRUE);
		break;
#endif
	case COMPRESSION_NONE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(no_radio_btn), TRUE);
		break;
	}

	frame = gtk_frame_new(_("Choose format"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	tar_radio_btn = gtk_radio_button_new_with_mnemonic(NULL, "_TAR");
	gtk_widget_set_name(tar_radio_btn, "TAR");
	gtk_box_pack_start(GTK_BOX(hbox1), tar_radio_btn, FALSE, FALSE, 0);
	archiver_set_tooltip(tar_radio_btn, g_strdup_printf(_("Choose this to use %s as format for the archive"), "TAR"));

	shar_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(tar_radio_btn), "S_HAR");
	gtk_widget_set_name(shar_radio_btn, "SHAR");
	gtk_box_pack_start(GTK_BOX(hbox1), shar_radio_btn, FALSE, FALSE, 0);
	archiver_set_tooltip(shar_radio_btn, g_strdup_printf(_("Choose this to use %s as format for the archive"), "SHAR"));

	cpio_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(tar_radio_btn), "CP_IO");
	gtk_widget_set_name(cpio_radio_btn, "CPIO");
	gtk_box_pack_start(GTK_BOX(hbox1), cpio_radio_btn, FALSE, FALSE, 0);
	archiver_set_tooltip(cpio_radio_btn, g_strdup_printf(_("Choose this to use %s as format for the archive"), "CPIO"));

	pax_radio_btn = gtk_radio_button_new_with_mnemonic_from_widget(
					GTK_RADIO_BUTTON(tar_radio_btn), "PA_X");
	gtk_widget_set_name(pax_radio_btn, "PAX");
	gtk_box_pack_start(GTK_BOX(hbox1), pax_radio_btn, FALSE, FALSE, 0);
	archiver_set_tooltip(pax_radio_btn, g_strdup_printf(_("Choose this to use %s as format for the archive"), "PAX"));

	page->archive_formats = 
			gtk_radio_button_get_group(GTK_RADIO_BUTTON(tar_radio_btn));

	switch (archiver_prefs.format) {
	case FORMAT_TAR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tar_radio_btn), TRUE);
		break;
	case FORMAT_SHAR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(shar_radio_btn), TRUE);
		break;
	case FORMAT_CPIO:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cpio_radio_btn), TRUE);
		break;
	case FORMAT_PAX:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pax_radio_btn), TRUE);
		break;
	}

	frame = gtk_frame_new(_("Miscellaneous options"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	page->recursive = gtk_check_button_new_with_mnemonic(_("_Recursive"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->recursive), archiver_prefs.recursive);
	gtk_box_pack_start(GTK_BOX(hbox1), page->recursive, FALSE, FALSE, 0);
	CLAWS_SET_TIP(page->recursive,
		_("Choose this option to include subfolders in the archive"));
	
	page->md5sum = gtk_check_button_new_with_mnemonic(_("_MD5sum"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->md5sum), archiver_prefs.md5sum);
	gtk_box_pack_start(GTK_BOX(hbox1), page->md5sum, FALSE, FALSE, 0);
	CLAWS_SET_TIP(page->md5sum,
		_("Choose this option to add MD5 checksums for each file in the archive.\n"
		  "Be aware though, that this dramatically increases the time it\n"
		  "will take to create the archive"));

	page->rename_files = gtk_check_button_new_with_mnemonic(_("R_ename"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->rename_files), archiver_prefs.rename);
	gtk_box_pack_start(GTK_BOX(hbox1), page->rename_files, FALSE, FALSE, 0);
	CLAWS_SET_TIP(page->rename_files,
		_("Choose this option to use descriptive names for each file in the archive.\n"
		  "The naming scheme: date_from@to@subject.\n"
		  "Names will be truncated to max 96 characters"));

        page->unlink_files = gtk_check_button_new_with_mnemonic(_("_Delete"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->unlink_files), archiver_prefs.unlink);
	gtk_box_pack_start(GTK_BOX(hbox1), page->unlink_files, FALSE, FALSE, 0);
	CLAWS_SET_TIP(page->unlink_files,
		_("Choose this option to delete mails after archiving\n"
                  "At this point only handles IMAP4, Local mbox and POP3"));


	frame = gtk_frame_new(_("Selection options"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox1), 4);
	gtk_container_add(GTK_CONTAINER(frame), hbox1);

	file_label = gtk_label_new(_("Select mails before"));
	gtk_box_pack_start(GTK_BOX(hbox1), file_label, FALSE, FALSE, 0);
	
	page->isoDate = gtk_entry_new();
	gtk_widget_set_name(page->isoDate, "isoDate");
	gtk_box_pack_start(GTK_BOX(hbox1), page->isoDate, TRUE, TRUE, 0);
	CLAWS_SET_TIP(page->isoDate, 
                _("Select emails before a certain date\n"
                  "Date must comply to ISO-8601 [YYYY-MM-DD]"));

        g_signal_connect(G_OBJECT(folder_select), "clicked", 
			 G_CALLBACK(foldersel_cb), page);
	g_signal_connect(G_OBJECT(file_select), "clicked",
			 G_CALLBACK(filesel_cb), page);
	g_signal_connect(G_OBJECT(page->folder), "activate",
			 G_CALLBACK(entry_change_cb), page);
	g_signal_connect(G_OBJECT(page->file), "activate",
			 G_CALLBACK(entry_change_cb), page);

	gtk_widget_show_all(dialog);
}

void archiver_gtk_done(struct ArchivePage* page, GtkWidget* widget) {
	dispose_archive_page(page);
	free(progress);
	gtk_widget_destroy(widget);
	/*debug_set_mode(FALSE);*/
}

