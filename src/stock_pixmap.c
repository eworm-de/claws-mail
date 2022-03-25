/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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
#include <gtk/gtk.h>
#include <string.h>
#include <dirent.h>
#ifdef HAVE_SVG
#include <librsvg/rsvg.h>
#include <math.h>
#endif

#include "defs.h"
#include "stock_pixmap.h"
#include "gtkutils.h"
#include "utils.h"
#include "prefs_common.h"

#include "pixmaps/addr_one.xpm"
#include "pixmaps/addr_two.xpm"
#include "pixmaps/address.xpm"
#include "pixmaps/anonymous.xpm"
#include "pixmaps/book.xpm"
#include "pixmaps/category.xpm"
#include "pixmaps/checkbox_off.xpm"
#include "pixmaps/checkbox_on.xpm"
#include "pixmaps/clip.xpm"
#include "pixmaps/clipkey.xpm"
#include "pixmaps/complete.xpm"
#include "pixmaps/continue.xpm"
#include "pixmaps/deleted.xpm"
#include "pixmaps/error.xpm"
#include "pixmaps/edit_extern.xpm"
#include "pixmaps/forwarded.xpm"
#include "pixmaps/group.xpm"
#include "pixmaps/insert_file.xpm"
#include "pixmaps/interface.xpm"
#include "pixmaps/jpilot.xpm"
#include "pixmaps/key.xpm"
#include "pixmaps/key_gpg_signed.xpm"
#include "pixmaps/ldap.xpm"
#include "pixmaps/linewrap.xpm"
#include "pixmaps/linewrapcurrent.xpm"
#include "pixmaps/mark.xpm"
#include "pixmaps/locked.xpm"
#include "pixmaps/new.xpm"
#include "pixmaps/replied.xpm"
#include "pixmaps/replied_and_forwarded.xpm"
#include "pixmaps/close.xpm"
#include "pixmaps/down_arrow.xpm"
#include "pixmaps/up_arrow.xpm"
#include "pixmaps/exec.xpm"
#include "pixmaps/mail_draft.xpm"
#include "pixmaps/mail_attach.xpm"
#include "pixmaps/mail_compose.xpm"
#include "pixmaps/mail_forward.xpm"
#include "pixmaps/mail_privacy_encrypted.xpm"
#include "pixmaps/mail_privacy_signed.xpm"
#include "pixmaps/mail_receive.xpm"
#include "pixmaps/mail_receive_all.xpm"
#include "pixmaps/mail_reply.xpm"
#include "pixmaps/mail_reply_to_all.xpm"
#include "pixmaps/mail_reply_to_author.xpm"
#include "pixmaps/mail_reply_to_list.xpm"
#include "pixmaps/mail_send.xpm"
#include "pixmaps/mail_send_queue.xpm"
#include "pixmaps/mail_sign.xpm"
#include "pixmaps/open_mail.xpm"
#include "pixmaps/news_compose.xpm"
#include "pixmaps/paste.xpm"
#include "pixmaps/preferences.xpm"
#include "pixmaps/properties.xpm"
#include "pixmaps/queue_close.xpm"
#include "pixmaps/queue_close_hrm.xpm"
#include "pixmaps/queue_open.xpm"
#include "pixmaps/queue_open_hrm.xpm"
#include "pixmaps/claws_mail_icon.xpm"
#include "pixmaps/claws_mail_icon_64.xpm"
#include "pixmaps/claws_mail_compose_logo.xpm"
#ifndef GENERIC_UMPC
#include "pixmaps/claws_mail_logo.xpm"
#else
#include "pixmaps/claws_mail_logo_small.xpm"
#endif
#include "pixmaps/address_book.xpm"
#include "pixmaps/unread.xpm"
#include "pixmaps/read.xpm"
#include "pixmaps/vcard.xpm"
#include "pixmaps/ignorethread.xpm"
#include "pixmaps/online.xpm"
#include "pixmaps/offline.xpm"
#include "pixmaps/notice_warn.xpm"
#include "pixmaps/notice_error.xpm"
#include "pixmaps/notice_note.xpm"
#include "pixmaps/quicksearch.xpm"
#include "pixmaps/clip_gpg_signed.xpm"
#include "pixmaps/gpg_signed.xpm"
#include "pixmaps/go_folders.xpm"
#include "pixmaps/mime_text_plain.xpm"
#include "pixmaps/mime_text_html.xpm"
#include "pixmaps/mime_text_patch.xpm"
#include "pixmaps/mime_application.xpm"
#include "pixmaps/mime_image.xpm"
#include "pixmaps/mime_audio.xpm"
#include "pixmaps/mime_text_enriched.xpm"
#include "pixmaps/mime_unknown.xpm"
#include "pixmaps/mime_pdf.xpm"
#include "pixmaps/mime_ps.xpm"
#include "pixmaps/mime_calendar.xpm"
#include "pixmaps/mime_pgpsig.xpm"
#include "pixmaps/printer.xpm"
#include "pixmaps/printer_btn.xpm"
#include "pixmaps/privacy_signed.xpm"
#include "pixmaps/privacy_passed.xpm"
#include "pixmaps/privacy_failed.xpm"
#include "pixmaps/privacy_unknown.xpm"
#include "pixmaps/privacy_expired.xpm"
#include "pixmaps/privacy_warn.xpm"
#include "pixmaps/privacy_emblem_encrypted.xpm"
#include "pixmaps/privacy_emblem_signed.xpm"
#include "pixmaps/privacy_emblem_passed.xpm"
#include "pixmaps/privacy_emblem_failed.xpm"
#include "pixmaps/privacy_emblem_warn.xpm"
#include "pixmaps/mime_message.xpm"
#include "pixmaps/address_search.xpm"
#include "pixmaps/check_spelling.xpm"
#include "pixmaps/dir_close.xpm"
#include "pixmaps/dir_close_hrm.xpm"
#include "pixmaps/dir_open.xpm"
#include "pixmaps/dir_open_hrm.xpm"
#include "pixmaps/inbox_open.xpm"
#include "pixmaps/inbox_open_hrm.xpm"
#include "pixmaps/inbox_close.xpm"
#include "pixmaps/inbox_close_hrm.xpm"
#include "pixmaps/outbox_open.xpm"
#include "pixmaps/outbox_open_hrm.xpm"
#include "pixmaps/outbox_close.xpm"
#include "pixmaps/outbox_close_hrm.xpm"
#include "pixmaps/trash_open.xpm"
#include "pixmaps/trash_close.xpm"
#include "pixmaps/delete_btn.xpm"
#include "pixmaps/delete_dup_btn.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/trash_btn.xpm"
#include "pixmaps/trash_open_hrm.xpm"
#include "pixmaps/trash_close_hrm.xpm"
#include "pixmaps/drafts_close.xpm"
#include "pixmaps/drafts_open.xpm"
#include "pixmaps/dir_close_mark.xpm"
#include "pixmaps/dir_close_hrm_mark.xpm"
#include "pixmaps/dir_open_mark.xpm"
#include "pixmaps/dir_open_hrm_mark.xpm"
#include "pixmaps/inbox_open_mark.xpm"
#include "pixmaps/inbox_open_hrm_mark.xpm"
#include "pixmaps/inbox_close_mark.xpm"
#include "pixmaps/inbox_close_hrm_mark.xpm"
#include "pixmaps/outbox_open_mark.xpm"
#include "pixmaps/outbox_open_hrm_mark.xpm"
#include "pixmaps/outbox_close_mark.xpm"
#include "pixmaps/outbox_close_hrm_mark.xpm"
#include "pixmaps/trash_open_mark.xpm"
#include "pixmaps/trash_close_mark.xpm"
#include "pixmaps/queue_close_mark.xpm"
#include "pixmaps/queue_close_hrm_mark.xpm"
#include "pixmaps/queue_open_mark.xpm"
#include "pixmaps/queue_open_hrm_mark.xpm"
#include "pixmaps/trash_open_hrm_mark.xpm"
#include "pixmaps/trash_close_hrm_mark.xpm"
#include "pixmaps/drafts_close_mark.xpm"
#include "pixmaps/drafts_open_mark.xpm"
#include "pixmaps/dir_noselect_close.xpm"
#include "pixmaps/dir_noselect_close_mark.xpm"
#include "pixmaps/dir_noselect_open.xpm"
#include "pixmaps/dir_subs_close_mark.xpm"
#include "pixmaps/dir_subs_close.xpm"
#include "pixmaps/dir_subs_open.xpm"
#include "pixmaps/spam.xpm"
#include "pixmaps/spam_btn.xpm"
#include "pixmaps/ham_btn.xpm"
#include "pixmaps/moved.xpm"
#include "pixmaps/copied.xpm"
#include "pixmaps/selection.xpm"
#include "pixmaps/watchthread.xpm"
#include "pixmaps/empty.xpm"
#include "pixmaps/tray_newmail_offline.xpm"
#include "pixmaps/tray_newmail.xpm"
#include "pixmaps/tray_newmarkedmail_offline.xpm"
#include "pixmaps/tray_newmarkedmail.xpm"
#include "pixmaps/tray_nomail_offline.xpm"
#include "pixmaps/tray_nomail.xpm"
#include "pixmaps/tray_unreadmail_offline.xpm"
#include "pixmaps/tray_unreadmail.xpm"
#include "pixmaps/tray_unreadmarkedmail_offline.xpm"
#include "pixmaps/tray_unreadmarkedmail.xpm"
#include "pixmaps/doc_index.xpm"
#include "pixmaps/doc_index_close.xpm"
#include "pixmaps/doc_info.xpm"
#include "pixmaps/first_arrow.xpm"
#include "pixmaps/last_arrow.xpm"
#include "pixmaps/left_arrow.xpm"
#include "pixmaps/right_arrow.xpm"
#include "pixmaps/rotate_left.xpm"
#include "pixmaps/rotate_right.xpm"
#include "pixmaps/zoom_fit.xpm"
#include "pixmaps/zoom_in.xpm"
#include "pixmaps/zoom_out.xpm"
#include "pixmaps/zoom_width.xpm"
#include "pixmaps/mark_ignorethread.xpm"
#include "pixmaps/mark_watchthread.xpm"
#include "pixmaps/mark_mark.xpm"
#include "pixmaps/mark_unmark.xpm"
#include "pixmaps/mark_locked.xpm"
#include "pixmaps/mark_unlocked.xpm"
#include "pixmaps/mark_allread.xpm"
#include "pixmaps/mark_allunread.xpm"
#include "pixmaps/mark_read.xpm"
#include "pixmaps/mark_unread.xpm"

typedef struct _PrivPixmapData	PrivPixmapData;

struct _PrivPixmapData
{
	gchar **data;
	cairo_surface_t *pixmap;
	cairo_pattern_t *mask;
	gchar *file;
	gchar *icon_path;
	GdkPixbuf *pixbuf;
};

typedef struct _StockPixmapData	StockPixmapData;

struct _StockPixmapData
{
	gchar **data;
	cairo_surface_t *pixmap;
	cairo_pattern_t *mask;
	gchar *file;
	gchar *icon_path;
	GdkPixbuf *pixbuf;
};

typedef struct _OverlayData OverlayData;

struct _OverlayData
{
	GdkPixbuf *base_pixbuf;
	GdkPixbuf *overlay_pixbuf;

	guint base_height;
	guint base_width;
	guint overlay_height;
	guint overlay_width;
	OverlayPosition position;
	gint border_x;
	gint border_y;
	gboolean highlight;
};

static void stock_pixmap_find_themes_in_dir(GList **list, const gchar *dirname);

static PrivPixmapData privpixmaps[] =
{
    {claws_mail_icon_xpm              , NULL, NULL, "claws_mail_icon", NULL, NULL},
    {claws_mail_icon_64_xpm           , NULL, NULL, "claws_mail_icon_64", NULL, NULL},
#ifndef GENERIC_UMPC
    {claws_mail_logo_xpm              , NULL, NULL, "claws_mail_logo", NULL, NULL},
#else
    {claws_mail_logo_small_xpm        , NULL, NULL, "claws_mail_logo_small", NULL, NULL},
#endif
    {empty_xpm                        , NULL, NULL, "empty", NULL, NULL}
};

static StockPixmapData pixmaps[] =
{
    {addr_one_xpm                     , NULL, NULL, "addr_one", NULL, NULL},
    {addr_two_xpm                     , NULL, NULL, "addr_two", NULL, NULL},
    {address_xpm                      , NULL, NULL, "address", NULL, NULL},
    {address_book_xpm                 , NULL, NULL, "address_book", NULL, NULL},
    {address_search_xpm               , NULL, NULL, "address_search", NULL, NULL},
    {anonymous_xpm                    , NULL, NULL, "anonymous", NULL, NULL},
    {book_xpm                         , NULL, NULL, "book", NULL, NULL},
    {category_xpm                     , NULL, NULL, "category", NULL, NULL},
    {checkbox_off_xpm                 , NULL, NULL, "checkbox_off", NULL, NULL},
    {checkbox_on_xpm                  , NULL, NULL, "checkbox_on", NULL, NULL},
    {check_spelling_xpm               , NULL, NULL, "check_spelling", NULL, NULL},
    {clip_xpm                         , NULL, NULL, "clip", NULL, NULL},
    {clipkey_xpm                      , NULL, NULL, "clipkey", NULL, NULL},
    {clip_gpg_signed_xpm              , NULL, NULL, "clip_gpg_signed", NULL, NULL},
    {close_xpm                        , NULL, NULL, "close", NULL, NULL},
    {complete_xpm                     , NULL, NULL, "complete", NULL, NULL},
    {continue_xpm                     , NULL, NULL, "continue", NULL, NULL},
    {deleted_xpm                      , NULL, NULL, "deleted", NULL, NULL},
    {dir_close_xpm                    , NULL, NULL, "dir_close", NULL, NULL},
    {dir_close_hrm_xpm                , NULL, NULL, "dir_close_hrm", NULL, NULL},
    {dir_open_xpm                     , NULL, NULL, "dir_open", NULL, NULL},
    {dir_open_hrm_xpm                 , NULL, NULL, "dir_open_hrm", NULL, NULL},
    {dir_close_mark_xpm               , NULL, NULL, "dir_close_mark", NULL, NULL},
    {dir_close_hrm_mark_xpm           , NULL, NULL, "dir_close_hrm_mark", NULL, NULL},
    {dir_open_mark_xpm                , NULL, NULL, "dir_open_mark", NULL, NULL},
    {dir_open_hrm_mark_xpm            , NULL, NULL, "dir_open_hrm_mark", NULL, NULL},
    {down_arrow_xpm                   , NULL, NULL, "down_arrow", NULL, NULL},
    {up_arrow_xpm                     , NULL, NULL, "up_arrow", NULL, NULL},
    {edit_extern_xpm                  , NULL, NULL, "edit_extern", NULL, NULL},
    {error_xpm                        , NULL, NULL, "error", NULL, NULL},
    {exec_xpm                         , NULL, NULL, "exec", NULL, NULL},
    {forwarded_xpm                    , NULL, NULL, "forwarded", NULL, NULL},
    {group_xpm                        , NULL, NULL, "group", NULL, NULL},
    {ignorethread_xpm                 , NULL, NULL, "ignorethread", NULL, NULL},
    {inbox_close_xpm                  , NULL, NULL, "inbox_close", NULL, NULL},
    {inbox_close_hrm_xpm              , NULL, NULL, "inbox_close_hrm", NULL, NULL},
    {inbox_open_xpm                   , NULL, NULL, "inbox_open", NULL, NULL},
    {inbox_open_hrm_xpm               , NULL, NULL, "inbox_open_hrm", NULL, NULL},
    {inbox_close_mark_xpm             , NULL, NULL, "inbox_close_mark", NULL, NULL},
    {inbox_close_hrm_mark_xpm         , NULL, NULL, "inbox_close_hrm_mark", NULL, NULL},
    {inbox_open_mark_xpm              , NULL, NULL, "inbox_open_mark", NULL, NULL},
    {inbox_open_hrm_mark_xpm          , NULL, NULL, "inbox_open_hrm_mark", NULL, NULL},
    {insert_file_xpm                  , NULL, NULL, "insert_file", NULL, NULL},
    {interface_xpm                    , NULL, NULL, "interface", NULL, NULL},
    {jpilot_xpm                       , NULL, NULL, "jpilot", NULL, NULL},
    {key_xpm                          , NULL, NULL, "key", NULL, NULL},
    {key_gpg_signed_xpm               , NULL, NULL, "key_gpg_signed", NULL, NULL},
    {ldap_xpm                         , NULL, NULL, "ldap", NULL, NULL},
    {linewrapcurrent_xpm              , NULL, NULL, "linewrapcurrent", NULL, NULL},
    {linewrap_xpm                     , NULL, NULL, "linewrap", NULL, NULL},
    {locked_xpm                       , NULL, NULL, "locked", NULL, NULL},
    {mail_draft_xpm                   , NULL, NULL, "mail_draft", NULL, NULL},
    {mail_attach_xpm                  , NULL, NULL, "mail_attach", NULL, NULL},
    {mail_compose_xpm                 , NULL, NULL, "mail_compose", NULL, NULL},
    {mail_forward_xpm                 , NULL, NULL, "mail_forward", NULL, NULL},
    {mail_privacy_encrypted_xpm       , NULL, NULL, "mail_privacy_encrypted", NULL, NULL},
    {mail_privacy_signed_xpm          , NULL, NULL, "mail_privacy_signed", NULL, NULL},
    {mail_receive_xpm                 , NULL, NULL, "mail_receive", NULL, NULL},
    {mail_receive_all_xpm             , NULL, NULL, "mail_receive_all", NULL, NULL},
    {mail_reply_xpm                   , NULL, NULL, "mail_reply", NULL, NULL},
    {mail_reply_to_all_xpm            , NULL, NULL, "mail_reply_to_all", NULL, NULL},
    {mail_reply_to_author_xpm         , NULL, NULL, "mail_reply_to_author", NULL, NULL},
    {mail_reply_to_list_xpm           , NULL, NULL, "mail_reply_to_list", NULL, NULL},
    {mail_send_xpm                    , NULL, NULL, "mail_send", NULL, NULL},
    {mail_send_queue_xpm              , NULL, NULL, "mail_send_queue", NULL, NULL},
    {mail_sign_xpm                    , NULL, NULL, "mail_sign", NULL, NULL},
    {open_mail_xpm                    , NULL, NULL, "open_mail", NULL, NULL},
    {mark_xpm                         , NULL, NULL, "mark", NULL, NULL},
    {new_xpm                          , NULL, NULL, "new", NULL, NULL},
    {news_compose_xpm                 , NULL, NULL, "news_compose", NULL, NULL},
    {outbox_close_xpm                 , NULL, NULL, "outbox_close", NULL, NULL},
    {outbox_close_hrm_xpm             , NULL, NULL, "outbox_close_hrm", NULL, NULL},
    {outbox_open_xpm                  , NULL, NULL, "outbox_open", NULL, NULL},
    {outbox_open_hrm_xpm              , NULL, NULL, "outbox_open_hrm", NULL, NULL},
    {outbox_close_mark_xpm            , NULL, NULL, "outbox_close_mark", NULL, NULL},
    {outbox_close_hrm_mark_xpm        , NULL, NULL, "outbox_close_hrm_mark", NULL, NULL},
    {outbox_open_mark_xpm             , NULL, NULL, "outbox_open_mark", NULL, NULL},
    {outbox_open_hrm_mark_xpm         , NULL, NULL, "outbox_open_hrm_mark", NULL, NULL},
    {replied_xpm                      , NULL, NULL, "replied", NULL, NULL},
    {replied_and_forwarded_xpm        , NULL, NULL, "replied_and_forwarded", NULL, NULL},
    {paste_xpm                        , NULL, NULL, "paste", NULL, NULL},
    {preferences_xpm                  , NULL, NULL, "preferences", NULL, NULL},
    {properties_xpm                   , NULL, NULL, "properties", NULL, NULL},
    {queue_close_xpm                  , NULL, NULL, "queue_close", NULL, NULL},
    {queue_close_hrm_xpm              , NULL, NULL, "queue_close_hrm", NULL, NULL},
    {queue_open_xpm                   , NULL, NULL, "queue_open", NULL, NULL},
    {queue_open_hrm_xpm               , NULL, NULL, "queue_open_hrm", NULL, NULL},
    {trash_open_xpm                   , NULL, NULL, "trash_open", NULL, NULL},
    {trash_open_hrm_xpm               , NULL, NULL, "trash_open_hrm", NULL, NULL},
    {trash_close_xpm                  , NULL, NULL, "trash_close", NULL, NULL},
    {trash_close_hrm_xpm              , NULL, NULL, "trash_close_hrm", NULL, NULL},
    {queue_close_mark_xpm             , NULL, NULL, "queue_close_mark", NULL, NULL},
    {queue_close_hrm_mark_xpm         , NULL, NULL, "queue_close_hrm_mark", NULL, NULL},
    {queue_open_mark_xpm              , NULL, NULL, "queue_open_mark", NULL, NULL},
    {queue_open_hrm_mark_xpm          , NULL, NULL, "queue_open_hrm_mark", NULL, NULL},
    {trash_open_mark_xpm              , NULL, NULL, "trash_open_mark", NULL, NULL},
    {trash_open_hrm_mark_xpm          , NULL, NULL, "trash_open_hrm_mark", NULL, NULL},
    {trash_close_mark_xpm             , NULL, NULL, "trash_close_mark", NULL, NULL},
    {trash_close_hrm_mark_xpm         , NULL, NULL, "trash_close_hrm_mark", NULL, NULL},
    {unread_xpm                       , NULL, NULL, "unread", NULL, NULL},
    {vcard_xpm                        , NULL, NULL, "vcard", NULL, NULL},
    {online_xpm                       , NULL, NULL, "online", NULL, NULL},
    {offline_xpm                      , NULL, NULL, "offline", NULL, NULL},
    {notice_warn_xpm                  , NULL, NULL, "notice_warn",  NULL, NULL},
    {notice_error_xpm                 , NULL, NULL, "notice_error",  NULL, NULL},
    {notice_note_xpm                  , NULL, NULL, "notice_note",  NULL, NULL},
    {quicksearch_xpm                  , NULL, NULL, "quicksearch",  NULL, NULL},
    {gpg_signed_xpm                   , NULL, NULL, "gpg_signed", NULL, NULL},
    {go_folders_xpm                   , NULL, NULL, "go_folders", NULL, NULL},
    {drafts_close_xpm                 , NULL, NULL, "drafts_close", NULL, NULL},
    {drafts_open_xpm                  , NULL, NULL, "drafts_open", NULL, NULL},
    {drafts_close_mark_xpm            , NULL, NULL, "drafts_close_mark", NULL, NULL},
    {drafts_open_mark_xpm             , NULL, NULL, "drafts_open_mark", NULL, NULL},
    {mime_text_plain_xpm              , NULL, NULL, "mime_text_plain", NULL, NULL},
    {mime_text_html_xpm               , NULL, NULL, "mime_text_html", NULL, NULL},
    {mime_text_patch_xpm              , NULL, NULL, "mime_text_patch", NULL, NULL},
    {mime_application_xpm             , NULL, NULL, "mime_application", NULL, NULL},
    {mime_image_xpm                   , NULL, NULL, "mime_image", NULL, NULL},
    {mime_audio_xpm                   , NULL, NULL, "mime_audio", NULL, NULL},
    {mime_text_enriched_xpm           , NULL, NULL, "mime_text_enriched", NULL, NULL},
    {mime_unknown_xpm                 , NULL, NULL, "mime_unknown", NULL, NULL},
    {mime_pdf_xpm                     , NULL, NULL, "mime_pdf", NULL, NULL},
    {mime_ps_xpm                      , NULL, NULL, "mime_ps", NULL, NULL},
    {mime_calendar_xpm                , NULL, NULL, "mime_calendar", NULL, NULL},
    {mime_pgpsig_xpm                  , NULL, NULL, "mime_pgpsig", NULL, NULL},
    {printer_btn_xpm                  , NULL, NULL, "printer_btn", NULL, NULL},
    {printer_xpm                      , NULL, NULL, "printer", NULL, NULL},
    {privacy_signed_xpm               , NULL, NULL, "privacy_signed", NULL, NULL},
    {privacy_passed_xpm               , NULL, NULL, "privacy_passed", NULL, NULL},
    {privacy_failed_xpm               , NULL, NULL, "privacy_failed", NULL, NULL},
    {privacy_unknown_xpm              , NULL, NULL, "privacy_unknown", NULL, NULL},
    {privacy_expired_xpm              , NULL, NULL, "privacy_expired", NULL, NULL},
    {privacy_warn_xpm                 , NULL, NULL, "privacy_warn", NULL, NULL},
    {privacy_emblem_encrypted_xpm     , NULL, NULL, "privacy_emblem_encrypted", NULL, NULL},
    {privacy_emblem_signed_xpm        , NULL, NULL, "privacy_emblem_signed", NULL, NULL},
    {privacy_emblem_passed_xpm        , NULL, NULL, "privacy_emblem_passed", NULL, NULL},
    {privacy_emblem_failed_xpm        , NULL, NULL, "privacy_emblem_failed", NULL, NULL},
    {privacy_emblem_warn_xpm          , NULL, NULL, "privacy_emblem_warn", NULL, NULL},
    {mime_message_xpm                 , NULL, NULL, "mime_message", NULL, NULL},
    {read_xpm                         , NULL, NULL, "read", NULL, NULL},
    {delete_btn_xpm                   , NULL, NULL, "delete_btn", NULL, NULL},
    {delete_dup_btn_xpm               , NULL, NULL, "delete_dup_btn", NULL, NULL},
    {cancel_xpm                       , NULL, NULL, "cancel", NULL, NULL},
    {trash_btn_xpm                    , NULL, NULL, "trash_btn", NULL, NULL},
    {claws_mail_compose_logo_xpm      , NULL, NULL, "claws_mail_compose_logo", NULL, NULL},
    {dir_noselect_close_xpm           , NULL, NULL, "dir_noselect_close", NULL, NULL},
    {dir_noselect_close_mark_xpm      , NULL, NULL, "dir_noselect_close_mark", NULL, NULL},
    {dir_noselect_open_xpm            , NULL, NULL, "dir_noselect_open", NULL, NULL},
    {dir_subs_close_mark_xpm          , NULL, NULL, "dir_subs_close_mark", NULL, NULL},
    {dir_subs_close_xpm               , NULL, NULL, "dir_subs_close", NULL, NULL},
    {dir_subs_open_xpm                , NULL, NULL, "dir_subs_open", NULL, NULL},
    {spam_xpm                         , NULL, NULL, "spam", NULL, NULL},
    {spam_btn_xpm                     , NULL, NULL, "spam_btn", NULL, NULL},
    {ham_btn_xpm                      , NULL, NULL, "ham_btn", NULL, NULL},
    {moved_xpm                        , NULL, NULL, "moved", NULL, NULL},
    {copied_xpm                       , NULL, NULL, "copied", NULL, NULL},
    {selection_xpm                    , NULL, NULL, "selection", NULL, NULL},
    {watchthread_xpm                  , NULL, NULL, "watchthread", NULL, NULL},
    {tray_newmail_offline_xpm         , NULL, NULL, "tray_newmail_offline", NULL, NULL},
    {tray_newmail_xpm                 , NULL, NULL, "tray_newmail", NULL, NULL},
    {tray_newmarkedmail_offline_xpm   , NULL, NULL, "tray_newmarkedmail_offline", NULL, NULL},
    {tray_newmarkedmail_xpm           , NULL, NULL, "tray_newmarkedmail", NULL, NULL},
    {tray_nomail_offline_xpm          , NULL, NULL, "tray_nomail_offline", NULL, NULL},
    {tray_nomail_xpm                  , NULL, NULL, "tray_nomail", NULL, NULL},
    {tray_unreadmail_offline_xpm      , NULL, NULL, "tray_unreadmail_offline", NULL, NULL},
    {tray_unreadmail_xpm              , NULL, NULL, "tray_unreadmail", NULL, NULL},
    {tray_unreadmarkedmail_offline_xpm, NULL, NULL, "tray_unreadmarkedmail_offline", NULL, NULL},
    {tray_unreadmarkedmail_xpm        , NULL, NULL, "tray_unreadmarkedmail", NULL, NULL},
    {doc_index_xpm                    , NULL, NULL, "doc_index", NULL, NULL},
    {doc_index_close_xpm              , NULL, NULL, "doc_index_close", NULL, NULL},
    {doc_info_xpm                     , NULL, NULL, "doc_info", NULL, NULL},
    {first_arrow_xpm                  , NULL, NULL, "first_arrow", NULL, NULL},
    {last_arrow_xpm                   , NULL, NULL, "last_arrow", NULL, NULL},
    {left_arrow_xpm                   , NULL, NULL, "left_arrow", NULL, NULL},
    {right_arrow_xpm                  , NULL, NULL, "right_arrow", NULL, NULL},
    {rotate_left_xpm                  , NULL, NULL, "rotate_left", NULL, NULL},
    {rotate_right_xpm                 , NULL, NULL, "rotate_right", NULL, NULL},
    {zoom_fit_xpm                     , NULL, NULL, "zoom_fit", NULL, NULL},
    {zoom_in_xpm                      , NULL, NULL, "zoom_in", NULL, NULL},
    {zoom_out_xpm                     , NULL, NULL, "zoom_out", NULL, NULL},
    {zoom_width_xpm                   , NULL, NULL, "zoom_width", NULL, NULL},
    {mark_ignorethread_xpm            , NULL, NULL, "mark_ignorethread", NULL, NULL},
    {mark_watchthread_xpm             , NULL, NULL, "mark_watchthread", NULL, NULL},
    {mark_mark_xpm                    , NULL, NULL, "mark_mark", NULL, NULL},
    {mark_unmark_xpm                  , NULL, NULL, "mark_unmark", NULL, NULL},
    {mark_locked_xpm                  , NULL, NULL, "mark_locked", NULL, NULL},
    {mark_unlocked_xpm                , NULL, NULL, "mark_unlocked", NULL, NULL},
    {mark_allread_xpm                 , NULL, NULL, "mark_allread", NULL, NULL},
    {mark_allunread_xpm               , NULL, NULL, "mark_allunread", NULL, NULL},
    {mark_read_xpm                    , NULL, NULL, "mark_read", NULL, NULL},
    {mark_unread_xpm                  , NULL, NULL, "mark_unread", NULL, NULL},
    {empty_xpm                        , NULL, NULL, "empty", NULL, NULL}
};

/* Supported theme extensions */
static const char *extension[] = {
	".png",
	".xpm",
#ifdef HAVE_SVG
	".svg",
#endif
	NULL
};

/* return current supported extensions */
const char **stock_pixmap_theme_extensions(void)
{
	return extension;
}

/* return newly constructed GtkPixmap from GdkPixmap */
GtkWidget *priv_pixmap_widget(PrivPixmap icon)
{
	GdkPixbuf *pixbuf;

	cm_return_val_if_fail(icon < N_PRIV_PIXMAPS, NULL);

	if (priv_pixbuf_gdk(icon, &pixbuf) != -1)
		return gtk_image_new_from_pixbuf(pixbuf);

	return NULL;
}

GtkWidget *stock_pixmap_widget(StockPixmap icon)
{
	GdkPixbuf *pixbuf;

	cm_return_val_if_fail(icon < N_STOCK_PIXMAPS, NULL);

	if (stock_pixbuf_gdk(icon, &pixbuf) != -1)
		return gtk_image_new_from_pixbuf(pixbuf);

	return NULL;
}

#ifdef HAVE_SVG
/*
 * Renders a SVG into a Cairo context at the given dimensions keeping
 * the aspect ratio.
 *
 * Adapted from https://developer.gnome.org/rsvg/2.40/RsvgHandle.html
 * #rsvg-handle-set-size-callback
 */
void render_scaled_proportionally(RsvgHandle *handle, cairo_t *cr, int width, int height)
{
	RsvgDimensionData dimensions;
	double x_factor, y_factor;
	double scale_factor;

	rsvg_handle_get_dimensions(handle, &dimensions);

	x_factor = (double) width / dimensions.width;
	y_factor = (double) height / dimensions.height;

	scale_factor = MIN(x_factor, y_factor);

	cairo_scale(cr, scale_factor, scale_factor);

	rsvg_handle_render_cairo(handle, cr);
}

/*
 * Generates a new Pixbuf from a Cairo context of the given dimensions.
 *
 * Adapted from https://gist.github.com/bert/985903
 */
GdkPixbuf *pixbuf_from_cairo(cairo_t *cr, gboolean alpha, int width, int height)
{
	gint p_stride, /* Pixbuf stride value */
	     p_n_channels, /* RGB -> 3, RGBA -> 4 */
	     s_stride; /* Surface stride value */
	guchar *p_pixels, /* Pixbuf's pixel data */
	       *s_pixels; /* Surface's pixel data */
	cairo_surface_t *surface; /* Temporary image surface */
	GdkPixbuf *pixbuf; /* Returned pixbuf */

	/* Create pixbuf */
	pixbuf = gdk_pixbuf_new
			(GDK_COLORSPACE_RGB, alpha, 8, width, height);
	if (pixbuf == NULL) {
		g_warning("failed to create a new %d x %d pixbuf", width, height);
		return NULL;
	}
	/* Obtain surface from where pixel values will be copied */
	surface = cairo_get_target(cr);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		g_warning("invalid cairo surface for copying");
		return NULL;
	}
	/* Inspect pixbuf */
	g_object_get(G_OBJECT(pixbuf),
		"rowstride", &p_stride,
		"n-channels", &p_n_channels,
		"pixels", &p_pixels,
		NULL);
	/* and surface */
	s_stride = cairo_image_surface_get_stride(surface);
	s_pixels = cairo_image_surface_get_data(surface);

	/* Copy pixel data from surface to pixbuf */
	while (height--) {
		gint i;
		guchar *p_iter = p_pixels, *s_iter = s_pixels;
		for (i = 0; i < width; i++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			/* Pixbuf: RGB(A) - Surface: BGRA */
			gdouble alpha_factor = (gdouble)0xff / s_iter[3];
			p_iter[0] = (guchar)( s_iter[2] * alpha_factor + .5 );
			p_iter[1] = (guchar)( s_iter[1] * alpha_factor + .5 );
			p_iter[2] = (guchar)( s_iter[0] * alpha_factor + .5 );
			if (p_n_channels == 4)
				 p_iter[3] = s_iter[3];
#elif G_BYTE_ORDER == G_BIG_ENDIAN
			/* Pixbuf: RGB(A) - Surface: ARGB */
			gdouble alpha_factor = (gdouble)0xff / s_iter[0];
			p_iter[0] = (guchar)( s_iter[1] * alpha_factor + .5 );
			p_iter[1] = (guchar)( s_iter[2] * alpha_factor + .5 );
			p_iter[2] = (guchar)( s_iter[3] * alpha_factor + .5 );
			if (p_n_channels == 4)
				p_iter[3] = s_iter[0];
#else /* PDP endianness */
			/* Pixbuf: RGB(A) - Surface: RABG */
			gdouble alpha_factor = (gdouble)0xff / s_iter[1];
			p_iter[0] = (guchar)( s_iter[0] * alpha_factor + .5 );
			p_iter[1] = (guchar)( s_iter[3] * alpha_factor + .5 );
			p_iter[2] = (guchar)( s_iter[2] * alpha_factor + .5 );
			if (p_n_channels == 4)
				p_iter[3] = s_iter[1];
#endif
			s_iter += 4;
			p_iter += p_n_channels;
		}
		s_pixels += s_stride;
		p_pixels += p_stride;
	}
	/* Destroy context */
	cairo_destroy(cr);

	return pixbuf;
}

/*
 * Renders a SVG file into a pixbuf with the dimensions of the
 * given pixmap data (optionally with alpha channel).
 */
GdkPixbuf *pixbuf_from_svg_like_icon(char *filename, GError **error, StockPixmapData *icondata, gboolean alpha)
{
	int width, height;
	cairo_surface_t *surface;
	cairo_t *context;
	RsvgHandle *handle;

	cm_return_val_if_fail(filename != NULL, NULL);
	cm_return_val_if_fail(icondata != NULL, NULL);

	/* load SVG file */
	handle = rsvg_handle_new_from_file(filename, error);
	if (handle == NULL) {
		g_warning("failed loading SVG '%s': %s (%d)", filename,
				(*error)->message, (*error)->code);
		return NULL;
	}

	/* scale dimensions */
	if (prefs_common.enable_pixmap_scaling) {
		/* default is pixmap icon size */
		if (sscanf((icondata->data)[0], "%d %d ", &width, &height) != 2) {
			g_warning("failed reading icondata width and height");
			return NULL;
		}
		/* which can be modified by some factor */
		if (prefs_common.pixmap_scaling_ppi > 0) {
			gdouble factor = (gdouble) prefs_common.pixmap_scaling_ppi / MIN_PPI;
			width = (int) floor(factor * width);
			height = (int) floor(factor * height);
		}
	} else { /* render using SVG size */
		RsvgDimensionData dimension;

		rsvg_handle_get_dimensions (handle, &dimension);
		width = dimension.width;
		height = dimension.height;
	}

	/* create drawing context */
	surface = cairo_image_surface_create(
			alpha? CAIRO_FORMAT_ARGB32: CAIRO_FORMAT_RGB24,
			width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		g_warning("failed to create a cairo surface: %s",
				cairo_status_to_string(cairo_surface_status(surface)));
		g_object_unref(handle);
		return NULL;
	}
	context = cairo_create(surface);
	cairo_surface_destroy(surface);
	if (cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		g_warning("failed to create a cairo context: %s",
				cairo_status_to_string(cairo_status(context)));
		cairo_destroy(context);
		return NULL;
	}
	/* render SVG */
	render_scaled_proportionally(handle, context, width, height);
	/* build result and destroy context */
	return pixbuf_from_cairo(context, alpha, width, height);
}
#endif

/*!
 *\brief
 */
gint priv_pixbuf_gdk(PrivPixmap icon, GdkPixbuf **pixbuf)
{
	PrivPixmapData *pix_d;

	if (pixbuf)
		*pixbuf = NULL;

	cm_return_val_if_fail(icon < N_PRIV_PIXMAPS, -1);

	pix_d = &privpixmaps[icon];

	if (!pix_d->pixbuf)
		pix_d->pixbuf = gdk_pixbuf_new_from_xpm_data((const gchar **) pix_d->data);

	cm_return_val_if_fail(pix_d->pixbuf != NULL, -1);

	if (pixbuf)
		*pixbuf = pix_d->pixbuf;

	/* pixbuf should have one ref outstanding */

	return 0;
}

gint stock_pixbuf_gdk(StockPixmap icon, GdkPixbuf **pixbuf)
{
	StockPixmapData *pix_d;
	int i = 0;
	gboolean theme_changed = FALSE;

	if (pixbuf)
		*pixbuf = NULL;

	cm_return_val_if_fail(icon < N_STOCK_PIXMAPS, -1);

	pix_d = &pixmaps[icon];

	theme_changed = (g_strcmp0(pix_d->icon_path, prefs_common.pixmap_theme_path) != 0);
	if (!pix_d->pixbuf || theme_changed) {
		GdkPixbuf *pix = NULL;

		if (theme_changed && pix_d->pixmap) {
			g_object_unref(pix_d->pixmap);
			pix_d->pixmap = NULL;
		}

		if (strcmp(prefs_common.pixmap_theme_path, DEFAULT_PIXMAP_THEME) != 0) {
			if (is_dir_exist(prefs_common.pixmap_theme_path)) {
				char *icon_file_name;
try_next_extension:
				icon_file_name = g_strconcat(prefs_common.pixmap_theme_path,
							     G_DIR_SEPARATOR_S,
							     pix_d->file,
							     extension[i],
							     NULL);
				if (is_file_exist(icon_file_name)) {
					GError *err = NULL;
#ifdef HAVE_SVG
					if (!strncmp(extension[i], ".svg", 4)) {
						pix = pixbuf_from_svg_like_icon(icon_file_name, &err, pix_d,
								prefs_common.enable_alpha_svg);
					} else {
						pix = gdk_pixbuf_new_from_file(icon_file_name, &err);
					}
#else
					pix = gdk_pixbuf_new_from_file(icon_file_name, &err);
#endif
					if (err) g_error_free(err);
				}
				if (pix) {
					g_free(pix_d->icon_path);
					pix_d->icon_path = g_strdup(prefs_common.pixmap_theme_path);
				}
				g_free(icon_file_name);
				if (!pix) {
					i++;
					if (extension[i])
						goto try_next_extension;
				}
			} else {
				/* even the path does not exist (deleted between two sessions), so
				set the preferences to the internal theme */
				prefs_common.pixmap_theme_path = g_strdup(DEFAULT_PIXMAP_THEME);
			}
		}
		pix_d->pixbuf = pix;
	}

	if (!pix_d->pixbuf) {
		pix_d->pixbuf = gdk_pixbuf_new_from_xpm_data((const gchar **) pix_d->data);
		if (pix_d->pixbuf) {
			g_free(pix_d->icon_path);
			pix_d->icon_path = g_strdup(DEFAULT_PIXMAP_THEME);
		}
	}

	cm_return_val_if_fail(pix_d->pixbuf != NULL, -1);

	if (pixbuf)
		*pixbuf = pix_d->pixbuf;

	/* pixbuf should have one ref outstanding */

	return 0;
}

static void stock_pixmap_find_themes_in_dir(GList **list, const gchar *dirname)
{
	const gchar *entry;
	gchar *fullentry;
	GDir *dp;
	GError *error = NULL;

	if ((dp = g_dir_open(dirname, 0, &error)) == NULL) {
		debug_print("skipping theme scan, dir %s could not be opened: %s (%d)\n",
				dirname ? dirname : "(null)", error->message, error->code);
		g_error_free(error);
		return;
	}

	while ((entry = g_dir_read_name(dp)) != NULL) {
		fullentry = g_strconcat(dirname, G_DIR_SEPARATOR_S, entry, NULL);

		if (strcmp(entry, ".") != 0 && strcmp(entry, "..") != 0 && is_dir_exist(fullentry)) {
			gchar *filetoexist;
			gboolean found = FALSE;
			int i;
			int j;
			for (i = 0; i < N_STOCK_PIXMAPS && !found; i++) {
				for (j = 0; extension[j] && !found; j++) {
					filetoexist = g_strconcat(fullentry, G_DIR_SEPARATOR_S, pixmaps[i].file, extension[j], NULL);
					if (is_file_exist(filetoexist)) {
						*list = g_list_append(*list, fullentry);
						found = TRUE;
					}
					g_free(filetoexist);
				}
			}
			if (i == N_STOCK_PIXMAPS)
				g_free(fullentry);
		} else
			g_free(fullentry);
	}
	g_dir_close(dp);
}

gchar *stock_pixmap_get_system_theme_dir_for_theme(const gchar *theme)
{
	if (theme && *theme) {
#ifndef G_OS_WIN32
		return g_strconcat(PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S,
	        	   PIXMAP_THEME_DIR, G_DIR_SEPARATOR_S, theme, NULL);
#else
		return g_strconcat(w32_get_themes_dir(), G_DIR_SEPARATOR_S, theme, NULL);
#endif
	} else {
#ifndef G_OS_WIN32
		return g_strconcat(PACKAGE_DATA_DIR, G_DIR_SEPARATOR_S,
	        	   PIXMAP_THEME_DIR, NULL);
#else
		return g_strdup(w32_get_themes_dir());
#endif
	}
}

GList *stock_pixmap_themes_list_new(void)
{
	gchar *defaulttheme;
	gchar *userthemes;
	gchar *systemthemes;
	GList *list = NULL;

	defaulttheme = g_strdup(DEFAULT_PIXMAP_THEME);
	userthemes   = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   PIXMAP_THEME_DIR, NULL);
	systemthemes = stock_pixmap_get_system_theme_dir_for_theme(NULL);

	list = g_list_append(list, defaulttheme);
	stock_pixmap_find_themes_in_dir(&list, userthemes);
	stock_pixmap_find_themes_in_dir(&list, systemthemes);

	g_free(userthemes);
	g_free(systemthemes);
	return list;
}

void stock_pixmap_themes_list_free(GList *list)
{
	GList *ptr;

	if (list == NULL)
		return;

	for (ptr = g_list_first(list); ptr != NULL; ptr = g_list_next(ptr))
		g_free(ptr->data);
	g_list_free(list);
}

gchar *stock_pixmap_get_name (StockPixmap icon)
{
	if (icon >= N_STOCK_PIXMAPS)
		return NULL;

	return pixmaps[icon].file;

}

PrivPixmap priv_pixmap_get_icon (gchar *file)
{
	gint i;

	for (i = 0; i < N_PRIV_PIXMAPS; i++) {
		if (strcmp (pixmaps[i].file, file) == 0)
			return i;
	}
	return -1;
}

StockPixmap stock_pixmap_get_icon (gchar *file)
{
	gint i;

	for (i = 0; i < N_STOCK_PIXMAPS; i++) {
		if (strcmp (pixmaps[i].file, file) == 0)
			return i;
	}
	return -1;
}

static gboolean do_pix_draw(GtkWidget *widget, cairo_t *cr,
			    OverlayData *data)
{
	GdkWindow *drawable = gtk_widget_get_window(widget);
	gint left = 0;
	gint top = 0;

	cm_return_val_if_fail(data->base_pixbuf != NULL, FALSE);

	if (data->highlight) {
		MainWindow *mw = NULL;

		mw = mainwindow_get_mainwindow();
		if (mw != NULL && mw->menubar != NULL) {
			cairo_t *cr;
			GdkRGBA color;

			GtkStyleContext *context = gtk_widget_get_style_context (mw->menubar);
			gtk_style_context_lookup_color (context, "selected_bg_color", &color);
			cairo_region_t * creg = cairo_region_create();
			GdkDrawingContext * dctx  = gdk_window_begin_draw_frame (drawable, creg);
			cr = gdk_drawing_context_get_cairo_context (dctx);
			gdk_cairo_set_source_rgba (cr, &color);
			cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
			cairo_set_line_width(cr, 1.);
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
			cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
			cairo_rectangle(cr, data->border_x-2, data->border_y-2,
			    data->base_width+3, data->base_height+3);
			cairo_stroke(cr);
			gdk_window_end_draw_frame (drawable, dctx);
			cairo_region_destroy (creg);
		}
	}

	gdk_cairo_set_source_pixbuf(cr, data->base_pixbuf, data->border_x, data->border_y);
	cairo_paint(cr);

	if (data->position != OVERLAY_NONE) {

		switch (data->position) {
			case OVERLAY_TOP_LEFT:
			case OVERLAY_MID_LEFT:
			case OVERLAY_BOTTOM_LEFT:
				left = 0;
				break;

			case OVERLAY_TOP_CENTER:
			case OVERLAY_MID_CENTER:
			case OVERLAY_BOTTOM_CENTER:
				left = (data->base_width + data->border_x * 2 - data->overlay_width)/2;
				break;

			case OVERLAY_TOP_RIGHT:
			case OVERLAY_MID_RIGHT:
			case OVERLAY_BOTTOM_RIGHT:
				left = data->base_width + data->border_x * 2 - data->overlay_width;
				break;

			default:
				break;
		}
		switch (data->position) {
			case OVERLAY_TOP_LEFT:
			case OVERLAY_TOP_CENTER:
			case OVERLAY_TOP_RIGHT:
				top = 0;
				break;

			case OVERLAY_MID_LEFT:
			case OVERLAY_MID_CENTER:
			case OVERLAY_MID_RIGHT:
				top = (data->base_height + data->border_y * 2 - data->overlay_height)/2;
				break;

			case OVERLAY_BOTTOM_LEFT:
			case OVERLAY_BOTTOM_CENTER:
			case OVERLAY_BOTTOM_RIGHT:
				top = data->base_height + data->border_y * 2 - data->overlay_height;
				break;

			default:
				break;
		}
	}

	if (data->position != OVERLAY_NONE) {
     cm_return_val_if_fail(data->overlay_pixbuf != NULL, FALSE);
     gdk_cairo_set_source_pixbuf(cr, data->overlay_pixbuf, left, top);
     cairo_paint(cr);
	}

	return TRUE;
}

static gboolean pixmap_with_overlay_expose_event_cb(GtkWidget *widget, cairo_t *cr,
						    OverlayData *data)
{
	return do_pix_draw(widget, cr, data);
}

static void pixmap_with_overlay_destroy_cb(GtkWidget *object, OverlayData *data)
{
	g_object_unref(data->base_pixbuf);
	if (data->position != OVERLAY_NONE) {
		g_object_unref(data->overlay_pixbuf);
	}
	g_free(data);
}

/**
 * \brief Get a widget showing one icon with another overlaid on top of it.
 *
 * The base icon is always centralised, the other icon can be positioned.
 * The overlay icon is ignored if pos=OVERLAY_NONE is used
 *
 * \param window   top-level window widget
 * \param icon	   the base icon
 * \param overlay  the icon to overlay
 * \param pos      how to align the overlay widget, or OVERLAY_NONE for no overlay
 * \param border_x size of the border around the base icon (left and right)
 * \param border_y size of the border around the base icon (top and bottom)
 */
GtkWidget *stock_pixmap_widget_with_overlay(StockPixmap icon,
					    StockPixmap overlay, OverlayPosition pos,
					    gint border_x, gint border_y)
{
	GdkPixbuf *stock_pixbuf = NULL;
	GtkWidget *widget = NULL;
	GtkWidget *stock_wid = NULL;
	gint height, width;
	OverlayData *data = NULL;

	data = g_new0(OverlayData, 1);

	stock_wid = stock_pixmap_widget(icon);
	g_object_ref_sink(stock_wid);

	stock_pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(stock_wid));
	g_object_ref(stock_pixbuf);

	height = gdk_pixbuf_get_height(stock_pixbuf);
	width = gdk_pixbuf_get_width(stock_pixbuf);

	data->base_pixbuf = stock_pixbuf;
	data->base_height = height;
	data->base_width  = width;
	g_object_unref(stock_wid);

	if (pos != OVERLAY_NONE) {
		stock_wid = stock_pixmap_widget(overlay);
		g_object_ref_sink(stock_wid);

		stock_pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(stock_wid));
		g_object_ref(stock_pixbuf);
		data->overlay_pixbuf = stock_pixbuf;
		data->overlay_height = height;
		data->overlay_width  = width;

		g_object_unref(stock_wid);
	}

	data->position = pos;
	data->border_x = border_x;
	data->border_y = border_y;
	data->highlight = FALSE;

	widget = gtk_drawing_area_new();
	gtk_widget_set_size_request(widget, data->base_width + border_x * 2,
			      data->base_height + border_y * 2);
	g_signal_connect(G_OBJECT(widget), "draw",
			 G_CALLBACK(pixmap_with_overlay_expose_event_cb), data);
	g_signal_connect(G_OBJECT(widget), "destroy",
			 G_CALLBACK(pixmap_with_overlay_destroy_cb), data);
	g_object_set_data(G_OBJECT(widget), "highlight", &(data->highlight));
	return widget;
}
