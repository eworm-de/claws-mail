/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Colin Leroy
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libical/ical.h>
#include <gtk/gtk.h>

#if USE_PTHREAD
#include "pthread.h"
#endif

#include "folder.h"
#include "folder_item_prefs.h"
#include "mimeview.h"
#include "file-utils.h"
#include "utils.h"
#include "vcalendar.h"
#include "vcal_manager.h"
#include "vcal_folder.h"
#include "vcal_meeting_gtk.h"
#include "vcal_interface.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "menu.h"
#include "account.h"
#include "codeconv.h"
#include "xml.h"
#include "xmlprops.h"
#include "alertpanel.h"
#include "vcal_prefs.h"
#include "statusbar.h"
#include "timing.h"
#include "inc.h"

MimeViewerFactory vcal_viewer_factory;

static void create_meeting_from_message_cb_ui(GtkAction *action, gpointer data);

static GdkRGBA uri_color = {
	0, 0, 0, 1
};

struct _VCalViewer
{
	MimeViewer mimeviewer;

	gchar	  *file;
	MimeInfo  *mimeinfo;
	gchar 	  *tmpfile;
	VCalEvent *event;

	GtkWidget *scrolledwin;
	GtkWidget *table;
	GtkWidget *type;
	GtkWidget *who;
	GtkWidget *start;
	GtkWidget *end;
	GtkWidget *location;
	GtkWidget *summary;
	GtkWidget *description;
	gchar	  *url;
	GtkWidget *answer;
	GtkWidget *button;
	GtkWidget *reedit;
	GtkWidget *cancel;
	GtkWidget *uribtn;
	GtkWidget *attendees;
	GtkWidget *unavail_box;
};

static GtkActionEntry vcalendar_main_menu[] = {{
	"Message/CreateMeeting",
	NULL, N_("Create meeting from message..."), NULL, NULL, G_CALLBACK(create_meeting_from_message_cb_ui)
}};

static void create_meeting_from_message_cb_ui(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	SummaryView *summaryview = mainwin->summaryview;
	GSList *msglist = summary_get_selected_msg_list(summaryview);
	FolderItem *item = NULL;
	GSList *cur;
	gchar *msg;
	gint total=0;

	if (summary_is_locked(summaryview) || !msglist) {
		if (msglist)
			g_slist_free(msglist);
		return;
	}
	total = g_slist_length(msglist);
	
	msg = g_strdup_printf(_("You are about to create %d "
				       "meetings, one by one. Do you "
				       "want to continue?"), 
				       total);
	if (total > 9
	&&  alertpanel(_("Warning"), msg, NULL, _("_Cancel"),
		       NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_SECOND)
	    != G_ALERTALTERNATE) {
		g_free(msg);
		return;
	}
	g_free(msg);

	main_window_cursor_wait(summaryview->mainwin);
	gtk_cmclist_freeze(GTK_CMCLIST(summaryview->ctree));
	folder_item_update_freeze();
	inc_lock();

	item = summaryview->folder_item;

	STATUSBAR_PUSH(mainwin, _("Creating meeting..."));

	for (cur = msglist; cur; cur = cur->next) {
		MsgInfo *msginfo = procmsg_msginfo_get_full_info((MsgInfo *)cur->data);
		VCalEvent *event = NULL;
		FILE *fp = NULL;

		if (MSG_IS_ENCRYPTED(msginfo->flags)) {
			fp = procmime_get_first_encrypted_text_content(msginfo);
		} else {
			fp = procmime_get_first_text_content(msginfo);
		}
		
		if (fp) {
			gchar *uid;
			time_t t = time(NULL);
			time_t t2 = t+3600;
			gchar *org = NULL;
			gchar *orgname = NULL;
			gchar *summary = g_strdup(msginfo->subject ? msginfo->subject:_("no subject"));
			gchar *description = file_read_stream_to_str(fp);
			gchar *dtstart = g_strdup(icaltime_as_ical_string(icaltime_from_timet_with_zone(t, FALSE, NULL)));
			gchar *dtend = g_strdup(icaltime_as_ical_string(icaltime_from_timet_with_zone(t2, FALSE, NULL)));
			gchar *recur = NULL;
			gchar *tzid = g_strdup("UTC");
			gchar *url = NULL;
			gint method = ICAL_METHOD_REQUEST;
			gint sequence = 1;
			PrefsAccount *account = NULL;
			
			claws_fclose(fp);

			if (item && item->prefs && item->prefs->enable_default_account)
				account = account_find_from_id(item->prefs->default_account);

		 	if (!account) 
				account = account_get_cur_account();
			
			if (!account)
				goto bail;

			org = g_strdup(account->address);

			uid = prefs_account_generate_msgid(account);
			
			event = vcal_manager_new_event(uid,
					org, NULL, NULL/*location*/, summary, description, 
					dtstart, dtend, recur, tzid, url, method, sequence, 
					ICAL_VTODO_COMPONENT);
			g_free(uid);
			
			/* hack to get default hours */
			g_free(event->dtstart);
			g_free(event->dtend);
			event->dtstart = NULL;
			event->dtend = NULL;

			vcal_meeting_create(event);
			vcal_manager_free_event(event);
			
bail:
			g_free(org);
			g_free(orgname);
			g_free(summary);
			g_free(description);
			g_free(dtstart);
			g_free(dtend);
			g_free(recur);
			g_free(tzid);
			g_free(url);
		}

		procmsg_msginfo_free(&msginfo);
	}

	statusbar_progress_all(0,0,0);
	STATUSBAR_POP(mainwin);
	inc_unlock();
	folder_item_update_thaw();
	gtk_cmclist_thaw(GTK_CMCLIST(summaryview->ctree));
	main_window_cursor_normal(summaryview->mainwin);
	g_slist_free(msglist);
}

static gchar *get_tmpfile(VCalViewer *vcalviewer)
{
	gchar *tmpfile = NULL;
	
	if (!vcalviewer->tmpfile) {
		tmpfile = procmime_get_tmp_file_name(vcalviewer->mimeinfo);
		debug_print("creating %s\n", tmpfile);

		if (procmime_get_part(tmpfile, vcalviewer->mimeinfo) < 0) {
			g_warning("can't get mimepart file");	
			g_free(tmpfile);
			return NULL;
		}
		vcalviewer->tmpfile = tmpfile;
	}
	
	return vcalviewer->tmpfile;
}

static GtkWidget *vcal_viewer_get_widget(MimeViewer *_mimeviewer)
{
	VCalViewer *vcalviewer = (VCalViewer *) _mimeviewer;

	debug_print("vcal_viewer_get_widget\n");
	gtk_widget_show_all(vcalviewer->scrolledwin);
	return vcalviewer->scrolledwin;
}

static void vcal_viewer_clear_viewer(MimeViewer *_mimeviewer)
{
	VCalViewer *vcalviewer = (VCalViewer *) _mimeviewer;

	debug_print("vcal_viewer_clear_viewer\n");

	g_free(vcalviewer->file);
	vcalviewer->file = NULL;
	if (vcalviewer->tmpfile) {
		debug_print("unlinking %s\n", vcalviewer->tmpfile);
		g_unlink(vcalviewer->tmpfile);
		g_free(vcalviewer->tmpfile);
		vcalviewer->tmpfile = NULL;
	}
	vcalviewer->mimeinfo = NULL;
}

static VCalEvent *vcalviewer_get_component(const gchar *file, const gchar *charset)
{
	gchar *compstr = NULL;
	VCalEvent *event = NULL;
	FILE *fp;
	GByteArray *array;
	gchar buf[BUFSIZ];
	gint n_read;

	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = claws_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "claws_fopen");
		return NULL;
	}

	array = g_byte_array_new();

	while ((n_read = claws_fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && claws_ferror(fp))
			break;
		g_byte_array_append(array, (guchar *)buf, n_read);
	}

	if (claws_ferror(fp)) {
		FILE_OP_ERROR("file stream", "claws_fread");
		g_byte_array_free(array, TRUE);
		claws_fclose(fp);
		return NULL;
	}

	buf[0] = '\0';
	g_byte_array_append(array, (guchar *)buf, 1);
	compstr = (gchar *)array->data;
	g_byte_array_free(array, FALSE);

	claws_fclose(fp);	

	if (compstr) {
		event = vcal_get_event_from_ical(compstr, charset);
		g_free(compstr);
	}
	
	return event;
}

#define GTK_LABEL_SET_TEXT_TRIMMED(label, text) {		\
	gchar *tmplbl = strretchomp(g_strdup(text));		\
	gtk_label_set_text(label, tmplbl);			\
	g_free(tmplbl);						\
}

static void vcalviewer_answer_set_choices(VCalViewer *vcalviewer, VCalEvent *event, icalproperty_method method);

static void vcalviewer_reset(VCalViewer *vcalviewer) 
{
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->location), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->summary), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->description), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->end), "-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->attendees), "-");
	g_free(vcalviewer->url);
	vcalviewer->url = NULL;
	gtk_widget_hide(vcalviewer->uribtn);
	vcalviewer_answer_set_choices(vcalviewer, NULL, ICAL_METHOD_CANCEL);
}

static void vcalviewer_show_error(VCalViewer *vcalviewer, const gchar *msg)
{
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), msg);
}

static void vcalviewer_hide_error(VCalViewer *vcalviewer)
{
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), "-");
}

static void vcalviewer_show_unavailable(VCalViewer *vcalviewer, gboolean visi)
{
	if (visi)
		gtk_widget_show_all(vcalviewer->unavail_box);
	else
		gtk_widget_hide(vcalviewer->unavail_box);
}

static void vcalviewer_answer_set_choices(VCalViewer *vcalviewer, VCalEvent *event, icalproperty_method method)
{
	int i = 0;
	
	gtk_widget_hide(vcalviewer->reedit);
	gtk_widget_hide(vcalviewer->cancel);
	gtk_widget_hide(vcalviewer->answer);
	gtk_widget_hide(vcalviewer->button);

	for (i = 0; i < 3; i++) 
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(vcalviewer->answer), 0);
	
	vcalviewer_show_unavailable(vcalviewer, FALSE);

	if (method == ICAL_METHOD_REQUEST && event && !event->rec_occurrence) {
		PrefsAccount *account = vcal_manager_get_account_from_event(event);
		
		if (!account)
			account = vcal_manager_get_account_from_event(vcalviewer->event);

		if (!account && event) {
			account = account_get_default();
			vcal_manager_update_answer(event, account->address, 
					account->name,
					ICAL_PARTSTAT_NEEDSACTION, 
					ICAL_CUTYPE_INDIVIDUAL);
		}
		if (account) {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vcalviewer->answer), _("Accept"));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vcalviewer->answer), _("Tentatively accept"));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vcalviewer->answer), _("Decline"));
			gtk_widget_set_sensitive(vcalviewer->answer, TRUE);
			gtk_widget_set_sensitive(vcalviewer->button, TRUE);
			gtk_widget_show(vcalviewer->answer);
			gtk_widget_show(vcalviewer->button);
		} else {
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vcalviewer->answer), "-");
			gtk_widget_set_sensitive(vcalviewer->answer, FALSE);
			gtk_widget_set_sensitive(vcalviewer->button, FALSE);
		}
	} else {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vcalviewer->answer), "-");
		gtk_widget_set_sensitive(vcalviewer->answer, FALSE);
		gtk_widget_set_sensitive(vcalviewer->button, FALSE);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(vcalviewer->answer), 0);
	
	if (event && event->method == ICAL_METHOD_REQUEST) {
		PrefsAccount *account = vcal_manager_get_account_from_event(event);
		gchar *internal_ifb = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					"vcalendar", G_DIR_SEPARATOR_S, 
					"internal.ifb", NULL);
		gchar *myfb = file_read_to_str(internal_ifb);
		g_free(internal_ifb);
		if (account) {
			icalparameter_partstat answer = 
				vcal_manager_get_reply_for_attendee(event, account->address);
				
			if (answer == ICAL_PARTSTAT_ACCEPTED)
				gtk_combo_box_set_active(GTK_COMBO_BOX(vcalviewer->answer), 0);
			if (answer == ICAL_PARTSTAT_TENTATIVE)
				gtk_combo_box_set_active(GTK_COMBO_BOX(vcalviewer->answer), 1);
			if (answer == ICAL_PARTSTAT_DECLINED)
				gtk_combo_box_set_active(GTK_COMBO_BOX(vcalviewer->answer), 2);
			if (event->dtstart && event->dtend && myfb && *myfb 
			&& answer != ICAL_PARTSTAT_ACCEPTED 
			&& answer != ICAL_PARTSTAT_TENTATIVE) {
				if (!attendee_available(NULL, event->dtstart, event->dtend, myfb))
					vcalviewer_show_unavailable(vcalviewer, TRUE);
			}
		}
		g_free(myfb);
	}

	g_free(vcalviewer->url);
	if (event && event->url && *(event->url)) {
		vcalviewer->url = g_strdup(event->url);
		gtk_widget_show(vcalviewer->uribtn);
	} else {
		vcalviewer->url = NULL;
		gtk_widget_hide(vcalviewer->uribtn);
	}
}

static FolderItem *vcalendar_get_current_item(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderItem *item = NULL;
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	
	if (mainwin) {
		item = mainwin->summaryview->folder_item;
		if (item == NULL)
			return NULL;
		if (item->folder == folder)
			return item;
		else 
			return folder->inbox;
	} else {
		return NULL;
	}
}

void vcalviewer_display_event (VCalViewer *vcalviewer, VCalEvent *event)
{
	GSList *list = NULL;
	gchar *attendees = NULL;
	gboolean mine = FALSE;
	gchar *label = NULL;
	gboolean save_evt = FALSE;
	FolderItem *item = vcalendar_get_current_item();
	if (!event)
		return;
	if (!vcalviewer)
		return;

/* general */
	if (event->type == ICAL_VTODO_COMPONENT) {

		label = g_strjoin(" ", _("You have a Todo item."),
				_("Details follow:"), NULL);
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);

	} else if (event->method == ICAL_METHOD_REQUEST) {

		if (account_find_from_address(event->organizer, FALSE)) {
			label = g_strjoin(" ", _("You have created a meeting."),
					_("Details follow:"), NULL);
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
			mine = TRUE;
		} else {
			label = g_strjoin(" ", _("You have been invited to a meeting."),
						_("Details follow:"), NULL);
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
		}

	} else if (event->method == ICAL_METHOD_CANCEL) {

		label = g_strjoin(" ",
				_("A meeting to which you had been invited has been cancelled."),
				_("Details follow:"), NULL);
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
		save_evt = TRUE;
		vcalendar_refresh_folder_contents(item);
	} else if (event->method == ICAL_METHOD_REPLY) {
		/* don't change label */
	} else {

		label = g_strjoin(" ", _("You have been forwarded an appointment."),
				_("Details follow:"), NULL);
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
	}

	g_free(label);

/* organizer */
	if (event->orgname && *(event->orgname)
	&&  event->organizer && *(event->organizer)) {
		gchar *addr = g_strconcat(event->orgname, " <", event->organizer, ">", NULL);
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), addr);
		g_free(addr);
	} else if (event->organizer && *(event->organizer)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), event->organizer);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), "-");
	}

/* location */
	if (event->location && *(event->location)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->location), event->location);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->location), "-");
	}

/* summary */
	if (event->summary && *(event->summary)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->summary), event->summary);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->summary), "-");
	}

/* description */
	if (event->description && *(event->description)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->description), event->description);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->description), "-");
	}

/* url */
	g_free(vcalviewer->url);
	if (event->url && *(event->url)) {
		vcalviewer->url = g_strdup(event->url);
		gtk_widget_show(vcalviewer->uribtn);
	} else {
		vcalviewer->url = NULL;
		gtk_widget_hide(vcalviewer->uribtn);
	}
	
/* start */
	if (event->start && *(event->start)) {
		if (event->recur && *(event->recur)) {
			gchar *tmp = g_strdup_printf("%s <span weight=\"bold\">%s</span>",
							_("(this event recurs)"), event->start);
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), tmp);
			gtk_label_set_use_markup(GTK_LABEL(vcalviewer->start), TRUE);
			g_free(tmp);
		} else if (event->rec_occurrence) {
			gchar *tmp = g_strdup_printf("%s <span weight=\"bold\">%s</span>",
							_("(this event is part of a recurring event)"), event->start);
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), tmp);
			gtk_label_set_use_markup(GTK_LABEL(vcalviewer->start), TRUE);
			g_free(tmp);
		} else {
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), event->start);
		}
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), "-");
	}

/* end */
	if (event->end && *(event->end)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->end), event->end);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->end), "-");
	}
	
/* attendees */
	attendees = g_strdup("");
	for (list = vcal_manager_get_answers_emails(event); 
	     list && list->data; list = list->next) {
	     	gchar *attendee = (gchar *)list->data;
		gchar *name = vcal_manager_get_attendee_name(event, attendee);
		gchar *ename = g_markup_printf_escaped("%s", name?name:"");
		gchar *eatt = g_markup_printf_escaped("%s", attendee);
		icalparameter_partstat acode = vcal_manager_get_reply_for_attendee(event, attendee);
		gchar *answer = vcal_manager_get_reply_text_for_attendee(event, attendee);
		gchar *type = vcal_manager_get_cutype_text_for_attendee(event, attendee);
		gchar *tmp = NULL;
		gint e_len, n_len;
		tmp = g_strdup_printf(
				"%s%s&lt;%s&gt; (%s, <span%s>%s</span>)", 
				(ename && *(ename))?ename:"",
				(ename && *(ename))?" ":"",
				(eatt && *(eatt))?eatt:"", 
				(type && *(type))?type:"", 
				(acode != ICAL_PARTSTAT_ACCEPTED ? " foreground=\"red\"":""),
				(answer && *(answer))?answer:"");
		e_len = strlen(attendees);
		n_len = strlen(tmp);
		if (e_len) {
			attendees = g_realloc(attendees, e_len+n_len+2);
			*(attendees+e_len) = '\n';
			strcpy(attendees+e_len+1, tmp);
		} else {
			attendees = g_realloc(attendees, e_len+n_len+1);
			strcpy(attendees, tmp);
		}
		g_free(tmp);
		g_free(answer);
		g_free(type);
		g_free(name);
		g_free(ename);
		g_free(eatt);
	}
	
	if (attendees && *(attendees)) {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->attendees), attendees);
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->attendees), "-");
	}
	g_free(attendees);
	gtk_label_set_use_markup(GTK_LABEL(vcalviewer->attendees), TRUE);
	
/* buttons */
	if (!mine)
		if (event->type != ICAL_VTODO_COMPONENT)
			vcalviewer_answer_set_choices(vcalviewer, event, event->method);
		else
			vcalviewer_answer_set_choices(vcalviewer, event, ICAL_METHOD_PUBLISH);
	else {
		vcalviewer_answer_set_choices(vcalviewer, event, ICAL_METHOD_REPLY);
		gtk_widget_show(vcalviewer->reedit);
		gtk_widget_show(vcalviewer->cancel);
	}

/* save if cancelled */
	if (save_evt)
		vcal_manager_save_event(event, TRUE);
}

gchar *vcalviewer_get_uid_from_mimeinfo(MimeInfo *mimeinfo)
{
	gchar *compstr = NULL;
	gchar *res = NULL;
	VCalEvent *event = NULL;

	compstr = procmime_get_part_as_string(mimeinfo, TRUE);
	
	event = vcal_get_event_from_ical(compstr, NULL);
	if (event)
		res = g_strdup(event->uid);
	
	vcal_manager_free_event(event);

	debug_print("got uid: %s\n", res);
	return res;

}

static void vcalviewer_get_request_values(VCalViewer *vcalviewer, MimeInfo *mimeinfo, gboolean is_todo) 
{
	VCalEvent *saved_event = NULL;
	const gchar *saveme =  procmime_mimeinfo_get_parameter(mimeinfo, "vcalsave");
	
	if (!vcalviewer->event)
		return;

	/* see if we have it registered and more recent */
	saved_event = vcal_manager_load_event(vcalviewer->event->uid);
	if (saved_event && saved_event->sequence >= vcalviewer->event->sequence) {
		saved_event->method = vcalviewer->event->method;
		vcalviewer_display_event(vcalviewer, saved_event);
		vcal_manager_free_event(saved_event);
		return;
	} else if (saved_event) {
		vcal_manager_free_event(saved_event);
	}

	/* load it and register it */

	if (!saveme || strcmp(saveme,"no"))
		vcal_manager_save_event(vcalviewer->event, FALSE);

	vcalviewer_display_event(vcalviewer, vcalviewer->event);
}

static void vcalviewer_get_reply_values(VCalViewer *vcalviewer, MimeInfo *mimeinfo) 
{
	VCalEvent *saved_event = NULL;
	gchar *attendee = NULL, *label = NULL;
	Answer *answer = NULL;

	if (!vcalviewer->event)
		return;

	if (!vcalviewer->event->answers || g_slist_length(vcalviewer->event->answers) > 1) {
		g_warning("strange, no answers or more than one");
	} 
	
	if (vcalviewer->event->answers) {
		answer = (Answer *)vcalviewer->event->answers->data;
		attendee = g_strdup(answer->attendee);
	}
		

	if (!answer) {
		label = g_strjoin(" ",
			_("You have received an answer to an unknown meeting proposal."),
			_("Details follow:"), NULL);
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
	} else {
		label = g_strdup_printf(_("You have received an answer to a meeting proposal.\n"
			"%s has %s the invitation whose details follow:"),
			attendee, vcal_manager_answer_get_text(answer->answer));
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->type), label);
		g_free(attendee);
	}
	g_free(label);

	saved_event = vcal_manager_load_event(vcalviewer->event->uid);
	if (saved_event && answer) {
		vcal_manager_update_answer(saved_event, 
			answer->attendee, answer->name, answer->answer, answer->cutype);
		vcal_manager_save_event(saved_event, TRUE);
		saved_event->method = vcalviewer->event->method;
		vcalviewer_display_event(vcalviewer, saved_event);
		vcal_manager_free_event(saved_event);
		return;
	} else if (saved_event) {
		vcal_manager_free_event(saved_event);
	}

	if (vcalviewer->event->organizer) {
		if (vcalviewer->event->orgname) {
			gchar *addr = g_strconcat(vcalviewer->event->orgname, " <", 
					vcalviewer->event->organizer, ">", NULL);
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), addr);
			g_free(addr);
		} else {
			GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who),
				vcalviewer->event->organizer);
		}
	} else {
		GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->who), "-");
	}
	
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->location),
		vcalviewer->event->location?vcalviewer->event->location:"-");

	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->summary), 
		vcalviewer->event->summary?vcalviewer->event->summary:"-");

	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->description), 
		vcalviewer->event->description?vcalviewer->event->description:"-");

	g_free(vcalviewer->url);
	if (vcalviewer->event->url) {
		vcalviewer->url = g_strdup(vcalviewer->event->url);
		gtk_widget_show(vcalviewer->uribtn);
	} else {
		vcalviewer->url = NULL;
		gtk_widget_hide(vcalviewer->uribtn);
	}

	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->start), 
		vcalviewer->event->start?vcalviewer->event->start:"-");
	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->end), 
		vcalviewer->event->end?vcalviewer->event->end:"-");

	GTK_LABEL_SET_TEXT_TRIMMED(GTK_LABEL(vcalviewer->attendees), "-"); 

	vcalviewer_answer_set_choices(vcalviewer, NULL, ICAL_METHOD_REPLY);
}

static void vcalviewer_get_event(VCalViewer *vcalviewer, MimeInfo *mimeinfo) 
{
	gchar *tmpfile = get_tmpfile(vcalviewer);
	const gchar *charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	if (!charset)
		charset = CS_WINDOWS_1252;
	
	if (!strcasecmp(charset, CS_ISO_8859_1))
		charset = CS_WINDOWS_1252;
	
	if (vcalviewer->event) {
		vcal_manager_free_event(vcalviewer->event);
		vcalviewer->event = NULL;
	}
	
	if (!tmpfile) {
		vcalviewer_reset(vcalviewer);
		vcalviewer_show_error(vcalviewer, _("Error - could not get the calendar MIME part."));
		return;
	}

	vcalviewer->event = vcalviewer_get_component(tmpfile, charset);
	if (!vcalviewer->event) {
		vcalviewer_reset(vcalviewer);
		vcalviewer_show_error(vcalviewer, _("Error - no calendar part found."));
		return;
	}
	
	if (vcalviewer->event->type == ICAL_VTODO_COMPONENT) {
		vcalviewer_get_request_values(vcalviewer, mimeinfo, TRUE);
	} else if (vcalviewer->event->method == ICAL_METHOD_REQUEST ||
		   vcalviewer->event->method == ICAL_METHOD_PUBLISH ||
		   vcalviewer->event->method == ICAL_METHOD_CANCEL) {
		vcalviewer_get_request_values(vcalviewer, mimeinfo, FALSE);
	} else if (vcalviewer->event->method == ICAL_METHOD_REPLY) {
		vcalviewer_get_reply_values(vcalviewer, mimeinfo);
	} else {
		vcalviewer_reset(vcalviewer);
		vcalviewer_show_error(vcalviewer, _("Error - Unknown calendar component type."));
	}
}

static VCalViewer *s_vcalviewer = NULL;

static void vcal_viewer_show_mimepart(MimeViewer *_mimeviewer, const gchar *file, MimeInfo *mimeinfo)
{
	VCalViewer *vcalviewer = (VCalViewer *) _mimeviewer;
	GtkAllocation allocation;

	START_TIMING("");

	s_vcalviewer = vcalviewer;

	if (mimeinfo == NULL) {
		vcal_viewer_clear_viewer(_mimeviewer);
		END_TIMING();
		return;
	}
	debug_print("vcal_viewer_show_mimepart : %s\n", file);

	vcal_viewer_clear_viewer(_mimeviewer);
	gtk_widget_show_all(vcalviewer->scrolledwin);
	g_free(vcalviewer->file);
	vcalviewer->file = g_strdup(file);
	vcalviewer->mimeinfo = mimeinfo;
	vcalviewer_hide_error(vcalviewer);
	vcalviewer_get_event(vcalviewer, mimeinfo);
	GTK_EVENTS_FLUSH();
	gtk_widget_get_allocation(vcalviewer->scrolledwin, &allocation);
	gtk_widget_set_size_request(vcalviewer->description, 
		allocation.width - 200, -1);
	gtk_label_set_line_wrap(GTK_LABEL(vcalviewer->location), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(vcalviewer->summary), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(vcalviewer->description), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(vcalviewer->attendees), FALSE);
	
	if (prefs_common_get_prefs()->textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
						(prefs_common_get_prefs()->textfont);
		if (font_desc) {
			gtk_widget_override_font(
				vcalviewer->description, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	END_TIMING();	
}

void vcalviewer_reload(FolderItem *item)
{
	if (s_vcalviewer) {
		MainWindow *mainwin = mainwindow_get_mainwindow();
		Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());

		folder_item_scan(item);
		if (mainwin && mainwin->summaryview->folder_item) {
			FolderItem *cur = mainwin->summaryview->folder_item;
			if (cur->folder == folder)
				folder_item_scan(cur);
		}
		if (mainwin && mainwin->summaryview->folder_item == item) {
			debug_print("reload: %p, %p\n", (MimeViewer *)s_vcalviewer, s_vcalviewer->mimeinfo);
			summary_redisplay_msg(mainwin->summaryview);
		}
	}
}

static void vcal_viewer_destroy_viewer(MimeViewer *_mimeviewer)
{
	VCalViewer *vcalviewer = (VCalViewer *) _mimeviewer;

	debug_print("vcal_viewer_destroy_viewer\n");

	if (s_vcalviewer == vcalviewer)
		s_vcalviewer = NULL;
	vcal_viewer_clear_viewer(_mimeviewer);
	g_free(vcalviewer);
}

static gboolean vcalviewer_reedit_cb(GtkButton *widget, gpointer data)
{
	VCalViewer *vcalviewer = (VCalViewer *)data;
	gchar * uid = vcalviewer_get_uid_from_mimeinfo(vcalviewer->mimeinfo);
	VCalEvent *event = NULL;
	
	s_vcalviewer = vcalviewer;
	/* see if we have it registered and more recent */
	event = vcal_manager_load_event(uid);
	vcal_meeting_create(event);
	vcal_manager_free_event(event);
	return TRUE;
}

static gboolean vcalviewer_uribtn_cb(GtkButton *widget, gpointer data)
{
	VCalViewer *vcalviewer = (VCalViewer *)data;

	if (vcalviewer->url)
		open_uri(vcalviewer->url, prefs_common_get_uri_cmd());

	return TRUE;
}

void vcalendar_refresh_folder_contents(FolderItem *item)
{
	g_return_if_fail(item != NULL);

	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (folder && item->folder == folder) {
		MainWindow *mainwin = mainwindow_get_mainwindow();
		folder_item_scan(item);
		if (mainwin->summaryview->folder_item == item) {
			summary_show(mainwin->summaryview, item, FALSE);
		}
	}
}

static void send_cancel_notify_toggled_cb(GtkToggleButton *button, gboolean *data)
{
	*data = gtk_toggle_button_get_active(button);
}

void vcalendar_cancel_meeting(FolderItem *item, const gchar *uid)
{
	VCalEvent *event = NULL;
	VCalMeeting *meet = NULL;
	gchar *file = NULL;
	gint val = 0;
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	gboolean redisp = FALSE;
	GtkWidget *send_notify_chkbtn = gtk_check_button_new_with_label(_("Send a notification to the attendees"));
	gboolean send_notify = TRUE;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(send_notify_chkbtn), TRUE);
	gtk_widget_show(send_notify_chkbtn);
	g_signal_connect(G_OBJECT(send_notify_chkbtn), "toggled", 
			 G_CALLBACK(send_cancel_notify_toggled_cb),
			 &send_notify);

	val = alertpanel_full(_("Cancel meeting"),
			      _("Are you sure you want to cancel this meeting?"),
			      NULL, _("_No"), NULL, _("_Yes"), NULL, NULL,
			      ALERTFOCUS_FIRST, FALSE, send_notify_chkbtn, ALERT_WARNING);

	if (val != G_ALERTALTERNATE)
		return;

	event = vcal_manager_load_event(uid);
	if (!event)
		return;
	event->method = ICAL_METHOD_CANCEL;
	
	if (folder) {
		MainWindow *mainwin = mainwindow_get_mainwindow();
		if (mainwin->summaryview->folder_item == item) {
			redisp = TRUE;
			summary_show(mainwin->summaryview, NULL, FALSE);
		}
	}
	
	if (send_notify) {
		meet = vcal_meeting_create_hidden(event);
		if (!vcal_meeting_send(meet)) {
			event->method = ICAL_METHOD_REQUEST;
			vcal_manager_save_event(event, TRUE);
			vcal_manager_free_event(event);
			if (folder)
				folder_item_scan(item);

			if (folder && redisp) {
				MainWindow *mainwin = mainwindow_get_mainwindow();
				summary_show(mainwin->summaryview, item, FALSE);
			}
			return;
		}
	}

	vcal_manager_save_event(event, TRUE);
	
	file = vcal_manager_get_event_file(event->uid);
	g_unlink(file);
	vcal_manager_free_event(event);
	g_free(file);
	if (folder)
		folder_item_scan(item);
	if (folder && redisp) {
		MainWindow *mainwin = mainwindow_get_mainwindow();
		summary_show(mainwin->summaryview, item, FALSE);
	}

	return;
}

static gboolean vcalviewer_cancel_cb(GtkButton *widget, gpointer data)
{
	VCalViewer *vcalviewer = (VCalViewer *)data;
	FolderItem *item = vcalendar_get_current_item();
	gchar * uid = vcalviewer_get_uid_from_mimeinfo(vcalviewer->mimeinfo);
	vcalendar_cancel_meeting(item, uid);
	return TRUE;
}

static gboolean vcalviewer_action_cb(GtkButton *widget, gpointer data)
{
        VCalViewer *vcalviewer = (VCalViewer *)data;
	gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(vcalviewer->answer));
	icalparameter_partstat reply[3] = {ICAL_PARTSTAT_ACCEPTED, ICAL_PARTSTAT_TENTATIVE, ICAL_PARTSTAT_DECLINED};
	PrefsAccount *account = NULL;
	VCalEvent *saved_event = NULL, *event = NULL;
	debug_print("index chosen %d\n", index);
	
	if (index < 0 || index > 2) {
		return TRUE;
	}
	
	s_vcalviewer = vcalviewer;
	
	if (!vcalviewer->event) {
		g_warning("can't get event");
		return TRUE;
	}

/* see if we have it registered and more recent */
	event = vcalviewer->event;
	saved_event = vcal_manager_load_event(vcalviewer->event->uid);
	if (saved_event && saved_event->sequence >= vcalviewer->event->sequence) {
		saved_event->method = vcalviewer->event->method;
		event = saved_event;
	} else if (saved_event) {
		vcal_manager_free_event(saved_event);
		saved_event = NULL;
	}
	account = vcal_manager_get_account_from_event(event);
	
	if (!account) {
		AlertValue val = alertpanel_full(_("No account found"), 
						_("You have no account matching any attendee.\n"
						  "Do you want to reply anyway?"),
						NULL, _("_Cancel"), NULL, _("Reply anyway"), NULL, NULL,
						ALERTFOCUS_SECOND, FALSE, NULL, ALERT_QUESTION);
		if (val == G_ALERTALTERNATE) {		
			account = account_get_default();
			vcal_manager_update_answer(event, account->address, 
					account->name,
					ICAL_PARTSTAT_NEEDSACTION, 
					ICAL_CUTYPE_INDIVIDUAL);
		} else {
			if (saved_event)
				vcal_manager_free_event(saved_event);
			return TRUE;
		}
	}
	
	vcal_manager_update_answer(event, account->address, account->name, reply[index], 0);
	
	if (event->organizer && *(event->organizer) && 
	    !vcal_manager_reply(account, event)) {
		g_warning("couldn't send reply");
	} else {
		debug_print("no organizer, not sending answer\n");
	}
	
	vcal_manager_save_event(event, TRUE);

	vcalviewer_display_event(vcalviewer, event);
	if (saved_event)
		vcal_manager_free_event(saved_event);
        return TRUE;
};

#define TABLE_ADD_LINE(label_text, widget) { 				\
	gchar *tmpstr = g_strdup_printf("<span weight=\"bold\">%s</span>",\
				label_text);				\
	GtkWidget *label = gtk_label_new(tmpstr);		 	\
	g_free(tmpstr);							\
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);		\
	gtk_label_set_xalign (GTK_LABEL(label), 1.0);			\
	gtk_grid_attach(GTK_GRID(vcalviewer->table), label, 0, i, 1, 1);\
	gtk_widget_set_hexpand(label, TRUE);				\
	gtk_widget_set_halign(label, GTK_ALIGN_FILL);			\
	gtk_grid_attach(GTK_GRID(vcalviewer->table), widget,		\
			1, i, 1, 1);					\
	gtk_widget_set_hexpand(widget, TRUE);				\
	gtk_widget_set_halign(widget, GTK_ALIGN_FILL);			\
	if (GTK_IS_LABEL(widget)) {					\
		gtk_label_set_use_markup(GTK_LABEL (widget), TRUE);	\
		gtk_label_set_xalign(GTK_LABEL(widget), 0.0);		\
		gtk_label_set_yalign(GTK_LABEL(widget), 0.0);		\
		gtk_label_set_line_wrap_mode(GTK_LABEL(widget),		\
					     PANGO_WRAP_CHAR); 		\
		gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);	\
	}								\
	i++;								\
}

static gchar *vcal_viewer_get_selection(MimeViewer *_viewer)
{
	VCalViewer *viewer = (VCalViewer *)_viewer;
	if (viewer->summary == NULL)
		return NULL;
	if (gtk_label_get_text(GTK_LABEL(viewer->description))
	&&  *(gtk_label_get_text(GTK_LABEL(viewer->description))) > 2) {
		gint start, end;
		if (gtk_label_get_selection_bounds(GTK_LABEL(viewer->description), 
						   &start, &end)) {
			gchar *tmp = g_strdup(gtk_label_get_text(
						GTK_LABEL(viewer->description))+start);
			tmp[end-start] = '\0';
			return tmp;
		} else {
			return g_strdup(gtk_label_get_text(GTK_LABEL(viewer->description)));
		}
	}
	else if (gtk_label_get_text(GTK_LABEL(viewer->summary))
	&&  *(gtk_label_get_text(GTK_LABEL(viewer->summary))) > 2)
		return g_strdup(gtk_label_get_text(GTK_LABEL(viewer->summary)));
	else 
		return NULL;
}


static gboolean vcal_viewer_scroll_page(MimeViewer *_viewer, gboolean up)
{
	VCalViewer *viewer = (VCalViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(viewer->scrolledwin));
	
	return gtkutils_scroll_page(viewer->table, vadj, up);
}

static void vcal_viewer_scroll_one_line(MimeViewer *_viewer, gboolean up)
{
	VCalViewer *viewer = (VCalViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(viewer->scrolledwin));
	
	gtkutils_scroll_one_line(viewer->table, vadj, up);
}

MimeViewer *vcal_viewer_create(void)
{
	VCalViewer *vcalviewer;
	int i = 0;
	GtkWidget *hbox = NULL, *vbox = NULL;
	GtkWidget *warning_img;
	GtkWidget *warning_label;
	
	debug_print("Creating vcal view...\n");
	vcalviewer = g_new0(VCalViewer, 1);
	vcalviewer->mimeviewer.factory = &vcal_viewer_factory;

	vcalviewer->mimeviewer.get_widget = vcal_viewer_get_widget;
	vcalviewer->mimeviewer.show_mimepart = vcal_viewer_show_mimepart;
	vcalviewer->mimeviewer.clear_viewer = vcal_viewer_clear_viewer;
	vcalviewer->mimeviewer.destroy_viewer = vcal_viewer_destroy_viewer;
	vcalviewer->mimeviewer.get_selection = vcal_viewer_get_selection;
	vcalviewer->mimeviewer.scroll_page = vcal_viewer_scroll_page;
	vcalviewer->mimeviewer.scroll_one_line = vcal_viewer_scroll_one_line;

	vcalviewer->table = gtk_grid_new();
	vcalviewer->type = gtk_label_new("meeting");
	vcalviewer->who = gtk_label_new("who");
	vcalviewer->start = gtk_label_new("start");
	vcalviewer->end = gtk_label_new("end");
	vcalviewer->location = gtk_label_new("location");
	vcalviewer->summary = gtk_label_new("summary");
	vcalviewer->description = gtk_label_new("description");
	vcalviewer->attendees = gtk_label_new("attendees");

	vcalviewer->answer = gtk_combo_box_text_new();
	vcalviewer->url = NULL;
	vcalviewer->button = gtk_button_new_with_label(_("Answer"));
	vcalviewer->reedit = gtk_button_new_with_label(_("Edit meeting..."));
	vcalviewer->cancel = gtk_button_new_with_label(_("Cancel meeting..."));
	vcalviewer->uribtn = gtk_button_new_with_label(_("Launch website"));
	vcalviewer->unavail_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	warning_img = gtk_image_new_from_icon_name
                        ("dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR);
	warning_label = gtk_label_new(_("You are already busy at this time."));

	gtk_box_pack_start(GTK_BOX(vcalviewer->unavail_box), warning_img, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vcalviewer->unavail_box), warning_label, FALSE, FALSE, 0);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(hbox), vcalviewer->answer, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vcalviewer->button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vcalviewer->reedit, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vcalviewer->cancel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vcalviewer->uribtn, FALSE, FALSE, 0);
	
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vcalviewer->unavail_box, FALSE, FALSE, 0);

	vcalviewer_answer_set_choices(vcalviewer, NULL, ICAL_METHOD_REQUEST);

	gtk_label_set_selectable(GTK_LABEL(vcalviewer->type), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->who), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->start), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->end), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->location), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->summary), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->description), TRUE);
	gtk_label_set_selectable(GTK_LABEL(vcalviewer->attendees), TRUE);

	g_signal_connect(G_OBJECT(vcalviewer->button), "clicked",
			 G_CALLBACK(vcalviewer_action_cb), vcalviewer);

	g_signal_connect(G_OBJECT(vcalviewer->reedit), "clicked",
			 G_CALLBACK(vcalviewer_reedit_cb), vcalviewer);

	g_signal_connect(G_OBJECT(vcalviewer->cancel), "clicked",
			 G_CALLBACK(vcalviewer_cancel_cb), vcalviewer);

	g_signal_connect(G_OBJECT(vcalviewer->uribtn), "clicked",
			 G_CALLBACK(vcalviewer_uribtn_cb), vcalviewer);

	TABLE_ADD_LINE(_("Event:"), vcalviewer->type);
	TABLE_ADD_LINE(_("Organizer:"), vcalviewer->who);
	TABLE_ADD_LINE(_("Location:"), vcalviewer->location);
	TABLE_ADD_LINE(_("Summary:"), vcalviewer->summary);
	TABLE_ADD_LINE(_("Starting:"), vcalviewer->start);
	TABLE_ADD_LINE(_("Ending:"), vcalviewer->end);
	TABLE_ADD_LINE(_("Description:"), vcalviewer->description);
	TABLE_ADD_LINE(_("Attendees:"), vcalviewer->attendees);
	gtk_label_set_line_wrap(GTK_LABEL(vcalviewer->attendees), FALSE);
	TABLE_ADD_LINE(_("Action:"), vbox);
	
	vcalviewer->scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(GTK_WIDGET(vcalviewer->scrolledwin), "vcalendar_viewer");
	gtk_container_add(GTK_CONTAINER(vcalviewer->scrolledwin),
			vcalviewer->table);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(vcalviewer->scrolledwin),
				       GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	
	gtk_widget_show_all(vcalviewer->scrolledwin);
	return (MimeViewer *) vcalviewer;
}

static gchar *content_types[] =
	{"text/calendar", NULL};
	
MimeViewerFactory vcal_viewer_factory =
{
	content_types,
	0,
	vcal_viewer_create,
};

static gint alert_timeout_tag = 0;
static gint scan_timeout_tag = 0;

static gboolean vcal_webcal_check(gpointer data)
{
	Folder *root = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());

	if (prefs_common_get_prefs()->work_offline)
		return TRUE;
	
	manual_update = FALSE;
	folderview_check_new(root);
	manual_update = TRUE;
	return TRUE;
}

static guint context_menu_id = 0;
static guint main_menu_id = 0;

void vcalendar_init(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	Folder *folder = NULL;
	gchar *directory = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				"vcalendar", NULL);
	START_TIMING("");
	if (!is_dir_exist(directory) && make_dir (directory) != 0) {
		g_free(directory);
		return;
	}

	g_free(directory);

	vcal_prefs_init();

	mimeview_register_viewer_factory(&vcal_viewer_factory);
	folder_register_class(vcal_folder_get_class());

	folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (!folder) {
		START_TIMING("creating folder");
		folder = folder_new(vcal_folder_get_class(), PLUGIN_NAME, NULL);
		folder->klass->create_tree(folder);
		folder_add(folder);
		folder_scan_tree(folder, TRUE);
		END_TIMING();
	}

	if (!folder->inbox) {
		folder->klass->create_tree(folder);
		folder_scan_tree(folder, TRUE);
	}
	if (folder->klass->scan_required(folder, folder->inbox)) {
		START_TIMING("scanning folder");
		folder_item_scan(folder->inbox);
		END_TIMING();
	}
	
	vcal_folder_gtk_init();

	alert_timeout_tag = g_timeout_add(60*1000, 
				(GSourceFunc)vcal_meeting_alert_check, 
				(gpointer)NULL);
	scan_timeout_tag = g_timeout_add(3600*1000, 
				(GSourceFunc)vcal_webcal_check, 
				(gpointer)NULL);
	if (prefs_common_get_prefs()->enable_color) {
		uri_color = prefs_common_get_prefs()->color[COL_URI];
	}

	gtk_action_group_add_actions(mainwin->action_group, vcalendar_main_menu,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Message", "CreateMeeting", 
			  "Message/CreateMeeting", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menus/SummaryViewPopup", "CreateMeeting", 
			  "Message/CreateMeeting", GTK_UI_MANAGER_MENUITEM,
			  context_menu_id)
	END_TIMING();
}

void vcalendar_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	FolderItem *fitem = NULL;

	icalmemory_free_ring();

	vcal_folder_free_data();

	if (mainwin == NULL)
		return;

	folderview = mainwin->folderview;
	fitem = folderview->summaryview->folder_item;

	if (fitem && 
	    fitem->folder->klass == vcal_folder_get_class()) {
		folderview_unselect(folderview);
		summary_clear_all(folderview->summaryview);
		if (fitem->folder->klass->item_closed)
			fitem->folder->klass->item_closed(fitem);
	}

	mimeview_unregister_viewer_factory(&vcal_viewer_factory);
	folder_unregister_class(vcal_folder_get_class());
	vcal_folder_gtk_done();
	vcal_prefs_done();
	g_source_remove(alert_timeout_tag);
	alert_timeout_tag = 0;
	g_source_remove(scan_timeout_tag);
	scan_timeout_tag = 0;

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Message/CreateMeeting", main_menu_id);
	main_menu_id = 0;
	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Message/CreateMeeting", context_menu_id);
	context_menu_id = 0;
}
