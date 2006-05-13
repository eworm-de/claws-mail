/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <ctype.h>
#include <time.h>

#include "mbox.h"
#include "procmsg.h"
#include "folder.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "utils.h"
#include "filtering.h"
#include "alertpanel.h"

#define MSGBUFSIZE	8192

#define FPUTS_TO_TMP_ABORT_IF_FAIL(s) \
{ \
	lines++; \
	if (fputs(s, tmp_fp) == EOF) { \
		g_warning("can't write to temporary file\n"); \
		fclose(tmp_fp); \
		fclose(mbox_fp); \
		g_unlink(tmp_file); \
		g_free(tmp_file); \
		return -1; \
	} \
}

gint proc_mbox(FolderItem *dest, const gchar *mbox, gboolean apply_filter)
/* return values: -1 error, >=0 number of msgs added */
{
	FILE *mbox_fp;
	gchar buf[MSGBUFSIZE];
	gchar *tmp_file;
	gint msgs = 0;
	gint lines;
	MsgInfo *msginfo;
	gboolean more;
	GSList *to_filter = NULL, *cur;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print("Getting messages from %s into %s...\n", mbox, dest->path);

	if ((mbox_fp = g_fopen(mbox, "rb")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		alertpanel_error(_("Could not open mbox file:\n%s\n"), mbox);
		return -1;
	}

	/* ignore empty lines on the head */
	do {
		if (fgets(buf, sizeof(buf), mbox_fp) == NULL) {
			g_warning("can't read mbox file.\n");
			fclose(mbox_fp);
			return -1;
		}
	} while (buf[0] == '\n' || buf[0] == '\r');

	if (strncmp(buf, "From ", 5) != 0) {
		g_warning("invalid mbox format: %s\n", mbox);
		fclose(mbox_fp);
		return -1;
	}

	tmp_file = get_tmp_file();

	folder_item_update_freeze();
	
	do {
		FILE *tmp_fp;
		FolderItem *dropfolder;
		gint empty_lines;
		gint msgnum;

		if ((tmp_fp = g_fopen(tmp_file, "wb")) == NULL) {
			FILE_OP_ERROR(tmp_file, "fopen");
			g_warning("can't open temporary file\n");
			fclose(mbox_fp);
			g_free(tmp_file);
			return -1;
		}
		if (change_file_mode_rw(tmp_fp, tmp_file) < 0) {
			FILE_OP_ERROR(tmp_file, "chmod");
		}

		empty_lines = 0;
		lines = 0;
		more = FALSE;

		/* process all lines from mboxrc file */
		while (fgets(buf, sizeof(buf), mbox_fp) != NULL) {
			int offset;

			/* eof not reached, expect more lines */
			more = TRUE;

			/* eat empty lines */
			if (buf[0] == '\n' || buf[0] == '\r') {
				empty_lines++;
				continue;
			}

			/* From separator or quoted From */
			offset = 0;
			/* detect leading '>' char(s) */
			while ((buf[offset] == '>')) {
				offset++;
			}
			if (!strncmp(buf+offset, "From ", 5)) {
				/* From separator: */
				if (offset == 0) {
					/* expect next mbox item */
					break;
				}

				/* quoted From: */
				/* flush any eaten empty line */
				if (empty_lines > 0) {
					while (empty_lines-- > 0) {
						FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
				}
					empty_lines = 0;
				}
				/* store the unquoted line */
				FPUTS_TO_TMP_ABORT_IF_FAIL(buf + 1);
				continue;
			}

			/* other line */
			/* flush any eaten empty line */
			if (empty_lines > 0) {			
				while (empty_lines-- > 0) {
					FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
			}
				empty_lines = 0;
			}
			/* store the line itself */
					FPUTS_TO_TMP_ABORT_IF_FAIL(buf);
		}
		/* end of mbox item or end of mbox */

		/* flush any eaten empty line (but the last one) */
		if (empty_lines > 0) {
			while (--empty_lines > 0) {
				FPUTS_TO_TMP_ABORT_IF_FAIL("\n");
			}
		}

		/* more emails to expect? */
		more = !feof(mbox_fp);

		/* warn if email part is empty (it's the minimum check 
		   we can do */
		if (lines == 0) {
			g_warning("malformed mbox: %s: message %d is empty\n", mbox, msgs);
			fclose(tmp_fp);
			fclose(mbox_fp);
			g_unlink(tmp_file);
			return -1;
		}

		if (fclose(tmp_fp) == EOF) {
			FILE_OP_ERROR(tmp_file, "fclose");
			g_warning("can't write to temporary file\n");
			fclose(mbox_fp);
			g_unlink(tmp_file);
			g_free(tmp_file);
			return -1;
		}

		dropfolder = folder_get_default_processing();
			
		if ((msgnum = folder_item_add_msg(dropfolder, tmp_file, NULL, TRUE)) < 0) {
			fclose(mbox_fp);
			g_unlink(tmp_file);
			g_free(tmp_file);
			return -1;
		}

		msginfo = folder_item_get_msginfo(dropfolder, msgnum);
                if (!apply_filter || !procmsg_msginfo_filter(msginfo)) {
			folder_item_move_msg(dest, msginfo);
			procmsg_msginfo_free(msginfo);
		} else
			to_filter = g_slist_append(to_filter, msginfo);

		msgs++;
	} while (more);

	filtering_move_and_copy_msgs(to_filter);
	for (cur = to_filter; cur; cur = g_slist_next(cur)) {
		MsgInfo *info = (MsgInfo *)cur->data;
		procmsg_msginfo_free(info);
	}

	folder_item_update_thaw();
	
	g_free(tmp_file);
	fclose(mbox_fp);
	debug_print("%d messages found.\n", msgs);

	return msgs;
}

gint lock_mbox(const gchar *base, LockType type)
{
#ifdef G_OS_UNIX
	gint retval = 0;

	if (type == LOCK_FILE) {
		gchar *lockfile, *locklink;
		gint retry = 0;
		FILE *lockfp;

		lockfile = g_strdup_printf("%s.%d", base, getpid());
		if ((lockfp = g_fopen(lockfile, "wb")) == NULL) {
			FILE_OP_ERROR(lockfile, "fopen");
			g_warning("can't create lock file %s\n", lockfile);
			g_warning("use 'flock' instead of 'file' if possible.\n");
			g_free(lockfile);
			return -1;
		}

		fprintf(lockfp, "%d\n", getpid());
		fclose(lockfp);

		locklink = g_strconcat(base, ".lock", NULL);
		while (link(lockfile, locklink) < 0) {
			FILE_OP_ERROR(lockfile, "link");
			if (retry >= 5) {
				g_warning("can't create %s\n", lockfile);
				g_unlink(lockfile);
				g_free(lockfile);
				return -1;
			}
			if (retry == 0)
				g_warning("mailbox is owned by another"
					    " process, waiting...\n");
			retry++;
			sleep(5);
		}
		g_unlink(lockfile);
		g_free(lockfile);
	} else if (type == LOCK_FLOCK) {
		gint lockfd;

#if HAVE_FLOCK
		if ((lockfd = open(base, O_RDONLY)) < 0) {
#else
		if ((lockfd = open(base, O_RDWR)) < 0) {
#endif
			FILE_OP_ERROR(base, "open");
			return -1;
		}
#if HAVE_FLOCK
		if (flock(lockfd, LOCK_EX|LOCK_NB) < 0) {
			perror("flock");
#else
#if HAVE_LOCKF
		if (lockf(lockfd, F_TLOCK, 0) < 0) {
			perror("lockf");
#else
		{
#endif
#endif /* HAVE_FLOCK */
			g_warning("can't lock %s\n", base);
			if (close(lockfd) < 0)
				perror("close");
			return -1;
		}
		retval = lockfd;
	} else {
		g_warning("invalid lock type\n");
		return -1;
	}

	return retval;
#else
	return -1;
#endif /* G_OS_UNIX */
}

gint unlock_mbox(const gchar *base, gint fd, LockType type)
{
	if (type == LOCK_FILE) {
		gchar *lockfile;

		lockfile = g_strconcat(base, ".lock", NULL);
		if (g_unlink(lockfile) < 0) {
			FILE_OP_ERROR(lockfile, "unlink");
			g_free(lockfile);
			return -1;
		}
		g_free(lockfile);

		return 0;
	} else if (type == LOCK_FLOCK) {
#if HAVE_FLOCK
		if (flock(fd, LOCK_UN) < 0) {
			perror("flock");
#else
#if HAVE_LOCKF
		if (lockf(fd, F_ULOCK, 0) < 0) {
			perror("lockf");
#else
		{
#endif
#endif /* HAVE_FLOCK */
			g_warning("can't unlock %s\n", base);
			if (close(fd) < 0)
				perror("close");
			return -1;
		}

		if (close(fd) < 0) {
			perror("close");
			return -1;
		}

		return 0;
	}

	g_warning("invalid lock type\n");
	return -1;
}

gint copy_mbox(const gchar *src, const gchar *dest)
{
	return copy_file(src, dest, TRUE);
}

void empty_mbox(const gchar *mbox)
{
	FILE *fp;

	if ((fp = g_fopen(mbox, "wb")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		g_warning("can't truncate mailbox to zero.\n");
		return;
	}
	fclose(fp);
}

gint export_list_to_mbox(GSList *mlist, const gchar *mbox)
/* return values: -2 skipped, -1 error, 0 OK */
{
	GSList *cur;
	MsgInfo *msginfo;
	FILE *msg_fp;
	FILE *mbox_fp;
	gchar buf[BUFFSIZE];

	if (g_file_test(mbox, G_FILE_TEST_EXISTS) == TRUE) {
		if (alertpanel_full(_("Overwrite mbox file"),
							_("This file already exists. Do you want to overwrite it?"),
							_("Overwrite"), GTK_STOCK_CANCEL, NULL, FALSE,
							NULL, ALERT_WARNING, G_ALERTALTERNATE)
			== G_ALERTALTERNATE) {
		return -2;
	}
	}

	if ((mbox_fp = g_fopen(mbox, "wb")) == NULL) {
		FILE_OP_ERROR(mbox, "fopen");
		alertpanel_error(_("Could not create mbox file:\n%s\n"), mbox);
		return -1;
	}

	for (cur = mlist; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		int len;

		msg_fp = procmsg_open_message(msginfo);
		if (!msg_fp) {
			procmsg_msginfo_free(msginfo);
			continue;
		}

		strncpy2(buf,
			 msginfo->from ? msginfo->from :
			 cur_account && cur_account->address ?
			 cur_account->address : "unknown",
			 sizeof(buf));
		extract_address(buf);

		fprintf(mbox_fp, "From %s %s",
			buf, ctime(&msginfo->date_t));

		buf[0] = '\0';
		
		/* write email to mboxrc */
		while (fgets(buf, sizeof(buf), msg_fp) != NULL) {
			/* quote any From, >From, >>From, etc., according to mbox format specs */
			int offset;

			offset = 0;
			/* detect leading '>' char(s) */
			while ((buf[offset] == '>')) {
				offset++;
			}
			if (!strncmp(buf+offset, "From ", 5))
				fputc('>', mbox_fp);
			fputs(buf, mbox_fp);
		}

		/* force last line to end w/ a newline */
		len = strlen(buf);
		if (len > 0) {
			len--;
			if ((buf[len] != '\n') && (buf[len] != '\r'))
				fputc('\n', mbox_fp);
		}

		/* add a trailing empty line */
		fputc('\n', mbox_fp);

		fclose(msg_fp);
		procmsg_msginfo_free(msginfo);
	}
	
	fclose(mbox_fp);

	return 0;
}

/* read all messages in SRC, and store them into one MBOX file. */
/* return values: -2 skipped, -1 error, 0 OK */
gint export_to_mbox(FolderItem *src, const gchar *mbox)
{
	GSList *mlist;
	gint ret;
	
	g_return_val_if_fail(src != NULL, -1);
	g_return_val_if_fail(src->folder != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	debug_print("Exporting messages from %s into %s...\n",
		    src->path, mbox);

	mlist = folder_item_get_msg_list(src);

	ret = export_list_to_mbox(mlist, mbox);

	procmsg_msg_list_free(mlist);

	return ret;
}
