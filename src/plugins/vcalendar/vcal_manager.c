/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Colin Leroy <colin@colino.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_PTHREAD
#include <pthread.h>
#endif
#include <libical/ical.h>
#include "vcalendar.h"
#include "vcal_folder.h"
#include "vcal_manager.h"
#include "vcal_meeting_gtk.h"
#include "vcal_prefs.h"
#include "xml.h"
#include "xmlprops.h"
#include "prefs.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "alertpanel.h"
#include "account.h"
#include "codeconv.h"
#include <time.h>
#include "folder.h"
#include "quoted-printable.h"
#include "file-utils.h"
#include "utils.h"
#include "defs.h"

#ifdef G_OS_WIN32
#define getuid() 0
#endif

Answer *answer_new(const gchar *attendee, 
			  const gchar *name,
			  icalparameter_partstat ans,
			  icalparameter_cutype cutype)
{
	Answer *answer = g_new0(Answer, 1);
	answer->attendee = g_strdup(attendee);
	answer->name = g_strdup(name);
	if (!answer->name)
		answer->name = g_strdup("");
	if (!answer->attendee)
		answer->attendee = g_strdup("");
	answer->answer = ans;
	answer->cutype = cutype;
	return answer;
}

static void answer_free(Answer *answer)
{
	g_free(answer->attendee);
	g_free(answer->name);
	g_free(answer);
}

static GSList *answer_find(VCalEvent *event, Answer *answer)
{
	GSList *cur = event->answers;
	while (cur && cur->data) {
		Answer *b = (Answer *)cur->data;
		if (!strcasecmp(b->attendee, answer->attendee))
			return cur;
		cur = cur->next;
	}
	return NULL;
}

void vcal_manager_copy_attendees(VCalEvent *src, VCalEvent *dest)
{
	GSList *cur = src->answers;
	while (cur && cur->data) {
		Answer *a = (Answer *)cur->data;
		Answer *b = answer_new(a->attendee, a->name, a->answer, a->cutype);
		dest->answers = g_slist_prepend(dest->answers, b);
		cur = cur->next;
	}
	dest->answers = g_slist_reverse(dest->answers);
}

gchar *vcal_manager_answer_get_text(icalparameter_partstat ans) 
{
	static gchar *replies[5]={
		N_("accepted"),
		N_("tentatively accepted"),
		N_("declined"),
		N_("did not answer"),
		N_("unknown")
	};

	switch (ans) {
	case ICAL_PARTSTAT_ACCEPTED:
		return _(replies[0]);
		break;
	case ICAL_PARTSTAT_DECLINED:
		return _(replies[2]);
		break;
	case ICAL_PARTSTAT_TENTATIVE:
		return _(replies[1]);
		break;
	case ICAL_PARTSTAT_NEEDSACTION:
		return _(replies[3]);
	case ICAL_PARTSTAT_DELEGATED:
	case ICAL_PARTSTAT_COMPLETED:
	case ICAL_PARTSTAT_X:
	case ICAL_PARTSTAT_INPROCESS:
	case ICAL_PARTSTAT_NONE:
  case ICAL_PARTSTAT_FAILED:
		return _(replies[4]);
		break;			
	}
	return NULL;
}

gchar *vcal_manager_cutype_get_text(icalparameter_cutype type) 
{
	static gchar *replies[5]={
		N_("individual"),
		N_("group"),
		N_("resource"),
		N_("room"),
		N_("unknown")
	};

	switch (type) {
	case ICAL_CUTYPE_INDIVIDUAL:
		return _(replies[0]);
		break;
	case ICAL_CUTYPE_GROUP:
		return _(replies[1]);
		break;
	case ICAL_CUTYPE_RESOURCE:
		return _(replies[2]);
		break;
	case ICAL_CUTYPE_ROOM:
		return _(replies[3]);
	default:
		return _(replies[4]);
		break;			
	}
	return NULL;
}

static GSList *answer_remove(VCalEvent *event, Answer *answer)
{
	GSList *cur = answer_find(event, answer);
	if (cur) {
		Answer *b = (Answer *)cur->data;
		event->answers = g_slist_remove(event->answers, b);
		answer_free(b);
	}
	return event->answers;
}

static GSList *answer_add(VCalEvent *event, Answer *answer)
{
	event->answers = g_slist_append(event->answers, answer);
	return event->answers;
}

GSList *vcal_manager_get_answers_emails(VCalEvent *event)
{
	GSList *new = NULL;
	GSList *cur = event->answers;
	while (cur && cur->data) {
		Answer *b = (Answer *)cur->data;
		new = g_slist_prepend(new, b->attendee);
		cur = cur->next;
	}
	new = g_slist_reverse(new);
	return new;	
}

icalparameter_partstat vcal_manager_get_reply_for_attendee(VCalEvent *event, const gchar *att)
{
	Answer *a = answer_new(att, NULL, 0, 0);
	GSList *ans = answer_find(event, a);
	icalparameter_partstat res = 0;
	if (ans) {
		Answer *b = (Answer *)ans->data;
		res = b->answer;
	}
	answer_free(a);
	return res;
}

gchar *vcal_manager_get_cutype_text_for_attendee(VCalEvent *event, const gchar *att)
{
	icalparameter_cutype status = vcal_manager_get_cutype_for_attendee(event, att);
	gchar *res = NULL;
	if (status != 0) 
		res = g_strdup(vcal_manager_cutype_get_text(status));

	return res;
}

icalparameter_cutype vcal_manager_get_cutype_for_attendee(VCalEvent *event, const gchar *att)
{
	Answer *a = answer_new(att, NULL, 0, 0);
	GSList *ans = answer_find(event, a);
	icalparameter_cutype res = 0;
	if (ans) {
		Answer *b = (Answer *)ans->data;
		res = b->cutype;
	}
	answer_free(a);
	return res;
}

gchar *vcal_manager_get_reply_text_for_attendee(VCalEvent *event, const gchar *att)
{
	icalparameter_partstat status = vcal_manager_get_reply_for_attendee(event, att);
	gchar *res = NULL;
	if (status != 0) 
		res = g_strdup(vcal_manager_answer_get_text(status));

	return res;
}

gchar *vcal_manager_get_attendee_name(VCalEvent *event, const gchar *att)
{
	Answer *a = answer_new(att, NULL, 0, 0);
	GSList *ans = answer_find(event, a);
	gchar *res = NULL;
	if (ans) {
		Answer *b = (Answer *)ans->data;
		if (b->name)
			res = g_strdup(b->name);
	}
	answer_free(a);
	return res;
}

void vcal_manager_event_print(VCalEvent *event)
{
	GSList *list = event->answers;
	printf( "event->uid\t\t%s\n"
		"event->organizer\t\t%s\n"
		"event->start\t\t%s\n"
		"event->end\t\t%s\n"
		"event->location\t\t%s\n"
		"event->summary\t\t%s\n"
		"event->description\t%s\n"
		"event->url\t%s\n"
		"event->dtstart\t\t%s\n"
		"event->dtend\t\t%s\n"
		"event->recur\t\t%s\n"
		"event->tzid\t\t%s\n"
		"event->method\t\t%d\n"
		"event->sequence\t\t%d\n",
		event->uid,
		event->organizer,
		event->start,
		event->end,
		event->location,
		event->summary,
		event->description,
		event->url,
		event->dtstart,
		event->dtend,
		event->recur,
		event->tzid,
		event->method,
		event->sequence);
	while (list && list->data) {
		Answer *a = (Answer *)list->data;
		printf(" ans: %s %s, %s\n", a->name, a->attendee, vcal_manager_answer_get_text(a->answer));
		list = list->next;
	}
	
}

static gchar *write_headers(PrefsAccount 	*account, 
			    VCalEvent 		*event,
			    gboolean		 short_headers,
			    gboolean 		 is_reply, 
			    gboolean 		 is_pseudo_event);

gchar *vcal_manager_event_dump(VCalEvent *event, gboolean is_reply, gboolean is_pseudo_event,
				icalcomponent *use_calendar, gboolean modif)
{
	gchar *organizer = g_strdup_printf("MAILTO:%s", event->organizer);
	PrefsAccount *account = vcal_manager_get_account_from_event(event);
	gchar *attendee  = NULL;
	gchar *body, *headers;
	gchar *tmpfile = NULL;
	icalcomponent *calendar, *ievent, *timezone, *tzc;
	icalproperty *attprop;
	icalproperty *orgprop;
	icalparameter_partstat status = ICAL_PARTSTAT_NEEDSACTION;
	gchar *sanitized_uid = g_strdup(event->uid);
	
	subst_for_filename(sanitized_uid);

	tmpfile = g_strdup_printf("%s%cevt-%d-%s", get_tmp_dir(),
				      G_DIR_SEPARATOR, getuid(), sanitized_uid);
	g_free(sanitized_uid);

	if (!account) {
		g_free(organizer);
		g_free(tmpfile);
		debug_print("no account found\n");
		return NULL;
	}

	attendee = g_strdup_printf("MAILTO:%s", account->address);
	
	if (vcal_manager_get_reply_for_attendee(event, account->address) != 0)
		status = vcal_manager_get_reply_for_attendee(event, account->address);
	
	tzset();
	
	if (use_calendar != NULL) {
		calendar = use_calendar;
		g_free(tmpfile);
		tmpfile = NULL;
	} else
		calendar = 
        		icalcomponent_vanew(
        		    ICAL_VCALENDAR_COMPONENT,
	        	    icalproperty_new_version("2.0"),
        		    icalproperty_new_prodid(
                		 "-//Claws Mail//NONSGML Claws Mail Calendar//EN"),
			    icalproperty_new_calscale("GREGORIAN"),
			    icalproperty_new_method(is_reply ? ICAL_METHOD_REPLY:event->method),
        		    (void*)0
	        	    ); 	

	if (!calendar) {
		g_warning("can't generate calendar");
		g_free(organizer);
		g_free(tmpfile);
		g_free(attendee);
		return NULL;
	}

	orgprop = icalproperty_new_organizer(organizer);
	
	timezone = icalcomponent_new(ICAL_VTIMEZONE_COMPONENT);
	
	icalcomponent_add_property(timezone,
		icalproperty_new_tzid("UTC")); /* free */
	
	tzc = icalcomponent_new(ICAL_XSTANDARD_COMPONENT);
	icalcomponent_add_property(tzc,
		icalproperty_new_dtstart(
			icaltime_from_string("19700101T000000")));
	icalcomponent_add_property(tzc,
		icalproperty_new_tzoffsetfrom(0.0));
	icalcomponent_add_property(tzc,
		icalproperty_new_tzoffsetto(0.0));
	icalcomponent_add_property(tzc,
		icalproperty_new_tzname("Greenwich meridian time"));

        icalcomponent_add_component(timezone, tzc);

	ievent = 
	    icalcomponent_vanew(
                ICAL_VEVENT_COMPONENT, (void*)0);

	if (!ievent) {
		g_warning("can't generate event");
		g_free(organizer);
		g_free(tmpfile);
		g_free(attendee);
		return NULL;
	}
	
	icalcomponent_add_property(ievent,
                icalproperty_new_uid(event->uid));
	icalcomponent_add_property(ievent,
		icalproperty_vanew_dtstamp(icaltime_from_timet_with_zone(time(NULL), TRUE, NULL), (void*)0));
	icalcomponent_add_property(ievent,
		icalproperty_vanew_dtstart((icaltime_from_string(event->dtstart)), (void*)0));
	icalcomponent_add_property(ievent,
		icalproperty_vanew_dtend((icaltime_from_string(event->dtend)), (void*)0));
	if (event->recur && *(event->recur)) {
		icalcomponent_add_property(ievent,
			icalproperty_vanew_rrule((icalrecurrencetype_from_string(event->recur)), (void*)0));
	}
	icalcomponent_add_property(ievent,
		icalproperty_new_description(event->description));
	icalcomponent_add_property(ievent,
		icalproperty_new_summary(event->summary));
	icalcomponent_add_property(ievent,
		icalproperty_new_sequence(modif && !is_reply ? event->sequence + 1 : event->sequence));
	icalcomponent_add_property(ievent,
		icalproperty_new_class(ICAL_CLASS_PUBLIC));
	icalcomponent_add_property(ievent,
		icalproperty_new_transp(ICAL_TRANSP_OPAQUE));
	if (event->location && *event->location)
		icalcomponent_add_property(ievent,
			icalproperty_new_location(event->location));
	else
		icalcomponent_add_property(ievent,
			icalproperty_new_location(""));
	icalcomponent_add_property(ievent,
		icalproperty_new_status(ICAL_STATUS_CONFIRMED));
	icalcomponent_add_property(ievent,
		icalproperty_vanew_created(icaltime_from_timet_with_zone(time(NULL), TRUE, NULL), (void*)0));
	icalcomponent_add_property(ievent,
		icalproperty_vanew_lastmodified(icaltime_from_timet_with_zone(time(NULL), TRUE, NULL), (void*)0));
	icalcomponent_add_property(ievent,		
                orgprop);

	if (!icalcomponent_get_first_component(calendar, ICAL_VTIMEZONE_COMPONENT))
	        icalcomponent_add_component(calendar, timezone);
	else 
		icalcomponent_free(timezone);

        icalcomponent_add_component(calendar, ievent);

	if (event->url && *(event->url)) {
		attprop = icalproperty_new_url(event->url);
        	icalcomponent_add_property(ievent, attprop);
	}

	if (is_reply) {
		/* dump only this attendee */
		attprop =
	                icalproperty_vanew_attendee(
                	    attendee,
                	    icalparameter_new_role(
                        	ICAL_ROLE_REQPARTICIPANT),
                	    icalparameter_new_rsvp(ICAL_RSVP_TRUE),
			    icalparameter_new_partstat(status),
                	    (void*)0
                	    );
        	icalcomponent_add_property(ievent, attprop);
	} else {
		/* dump all attendees */
		GSList *cur = event->answers;
		while (cur && cur->data) {
			Answer *a = (Answer *)cur->data;

			if (a->cutype == 0)
				a->cutype = ICAL_CUTYPE_INDIVIDUAL;
			if (a->answer == 0)
				a->answer = ICAL_PARTSTAT_NEEDSACTION;

			attprop =
	                	icalproperty_vanew_attendee(
                		    a->attendee,
                		    icalparameter_new_role(
                        		ICAL_ROLE_REQPARTICIPANT),
                		    icalparameter_new_rsvp(ICAL_RSVP_TRUE),
                		    icalparameter_new_cutype(a->cutype),
				    icalparameter_new_partstat(a->answer),
                		    (void*)0
                		    );

			icalcomponent_add_property(ievent, attprop);
			cur = cur->next;
		}
	}

	if (use_calendar) {
		g_free(organizer);
		g_free(tmpfile);
		g_free(attendee);
		return NULL;
	}

	headers = write_headers(account, event, is_pseudo_event, is_reply, is_pseudo_event);

	if (!headers) {
		g_warning("can't get headers");
		g_free(organizer);
		g_free(tmpfile);
		g_free(attendee);
		return NULL;
	}

	body = g_strdup_printf("%s"
			       "\n"
			       "%s", headers, icalcomponent_as_ical_string(calendar));
	
	if (str_write_to_file(body, tmpfile, FALSE) < 0) {
		g_free(tmpfile);
		tmpfile = NULL;
	}

	chmod(tmpfile, S_IRUSR|S_IWUSR);

	g_free(body);
	g_free(headers);
	icalcomponent_free(calendar);
	g_free(attendee);
	g_free(organizer);
	
	tzset();

	return tmpfile;	
}

static void get_rfc822_date_from_time_t(gchar *buf, gint len, time_t t)
{
#ifndef G_OS_WIN32
	struct tm *lt;
	gchar day[4], mon[4];
	gint dd, hh, mm, ss, yyyy;
	gchar buft1[512];
	struct tm buft2;

	lt = localtime_r(&t, &buft2);
	if (sscanf(asctime_r(lt, buft1), "%3s %3s %d %d:%d:%d %d\n",
	       day, mon, &dd, &hh, &mm, &ss, &yyyy) != 7)
		g_warning("failed reading date/time");
	g_snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d %s",
		   day, dd, mon, yyyy, hh, mm, ss, tzoffset(&t));
#else
	GDateTime *dt = g_date_time_new_from_unix_local(t);
	if (dt == NULL) {
		g_warning("failed getting date/time");
		g_snprintf(buf, len, "(NULL)");
		return;
	}

	gchar *ret = g_date_time_format(dt, "%a, %e %b %Y %T %z");
	g_date_time_unref(dt);

	if (ret == NULL) {
		g_warning("failed formatting date/time");
		g_snprintf(buf, len, "(NULL)");
		return;
	}

	g_snprintf(buf, len, ret);
	g_free(ret);
#endif
}

static gchar *write_headers_date(const gchar *uid)
{
	gchar subject[512];
	gchar *t_subject;
	gchar date[RFC822_DATE_BUFFSIZE];
	time_t t;
	struct tm lt;

	memset(subject, 0, sizeof(subject));
	memset(date, 0, sizeof(date));
	
	if (!strcmp(uid, EVENT_PAST_ID)) {
		t = 1;
		t_subject = _("Past");
	} else if (!strcmp(uid, EVENT_TODAY_ID)) {
		t = time(NULL);
		t_subject = _("Today");
	}  else if (!strcmp(uid, EVENT_TOMORROW_ID)) {
		t = time(NULL) + 86400;
		t_subject = _("Tomorrow");
	}  else if (!strcmp(uid, EVENT_THISWEEK_ID)) {
		t = time(NULL) + (86400*2);
		t_subject = _("This week");
	}  else if (!strcmp(uid, EVENT_LATER_ID)) {
		t = time(NULL) + (86400*7);
		t_subject = _("Later");
	}  else {
		g_warning("unknown spec date");
		return NULL;
	} 
	
#ifndef G_OS_WIN32
	struct tm buft;
	lt = *localtime_r(&t, &buft);
#else
	if (t < 0)
		t = 1;
	lt = *localtime(&t);
#endif
	lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
	t = mktime(&lt);
	get_rfc822_date_from_time_t(date, sizeof(date), t);
	conv_encode_header(subject, 511, t_subject, strlen("Subject: "), FALSE);
				
	return g_strdup_printf("From: -\n"
				"To: -\n"
				"Subject: %s\n"
				"Date: %s\n"
				"MIME-Version: 1.0\n"
				"Content-Type: text/plain; charset=\"UTF-8\";\n"
				"Content-Transfer-Encoding: quoted-printable\n"
				"Message-ID: <%s>\n",
				subject,
				date,
				uid);
}

gchar *vcal_manager_dateevent_dump(const gchar *uid, FolderItem *item)
{
	gchar *sanitized_uid = NULL;
	gchar *headers = NULL;
	gchar *lines, *body, *tmpfile;
	EventTime date;
	sanitized_uid = g_strdup(uid);
	subst_for_filename(sanitized_uid);
	
	tmpfile = g_strdup_printf("%s%cevt-%d-%s", get_tmp_dir(),
				   G_DIR_SEPARATOR, getuid(), sanitized_uid);
	g_free(sanitized_uid);

	headers = write_headers_date(uid);

	if (!headers) {
		g_warning("can't get headers");
		g_free(tmpfile);
		return NULL;
	}

	if (!strcmp(uid, EVENT_PAST_ID))
		date = EVENT_PAST;
	else if (!strcmp(uid, EVENT_TODAY_ID))
		date = EVENT_TODAY;
	else if (!strcmp(uid, EVENT_TOMORROW_ID))
		date = EVENT_TOMORROW;
	else if (!strcmp(uid, EVENT_THISWEEK_ID))
		date = EVENT_THISWEEK;
	else if (!strcmp(uid, EVENT_LATER_ID))
		date = EVENT_LATER;
	else
		date = EVENT_PAST;

	lines = get_item_event_list_for_date(item, date);
	body = g_strdup_printf("%s"
			       "\n"
			       "%s", headers, lines);
	g_free(lines);
	if (str_write_to_file(body, tmpfile, FALSE) < 0) {
		g_free(tmpfile);
		tmpfile = NULL;
	} else
		chmod(tmpfile, S_IRUSR|S_IWUSR);

	g_free(body);
	g_free(headers);
	
	return tmpfile;	
}

static gchar *write_headers_ical(PrefsAccount 	*account, 
			    icalcomponent	*ievent,
			    gchar		*orga);

gchar *vcal_manager_icalevent_dump(icalcomponent *event, gchar *orga, icalcomponent *use_calendar)
{
	PrefsAccount *account = account_get_cur_account();
	gchar *body, *headers, *qpbody;
	gchar **lines = NULL;
	gchar *tmpfile = NULL;
	icalcomponent *calendar;
	icalproperty *prop;
	icalcomponent *ievent = NULL;
	int i = 0;

	ievent = icalcomponent_new_clone(event);

	prop = icalcomponent_get_first_property(ievent, ICAL_UID_PROPERTY);
	if (prop) {
		gchar *sanitized_uid = g_strdup(icalproperty_get_uid(prop));

		subst_for_filename(sanitized_uid);

		tmpfile = g_strdup_printf("%s%cevt-%d-%s", get_tmp_dir(),
					      G_DIR_SEPARATOR, getuid(), sanitized_uid);
		g_free(sanitized_uid);
		icalproperty_free(prop);
	} else {
		tmpfile = g_strdup_printf("%s%cevt-%d-%p", get_tmp_dir(),
				      G_DIR_SEPARATOR, getuid(), ievent);
	}

	if (!account) {
		g_free(tmpfile);
		icalcomponent_free(ievent);
		return NULL;
	}

	tzset();
	
	if (use_calendar != NULL) {
		calendar = use_calendar;
		g_free(tmpfile);
		tmpfile = NULL;
	} else
		calendar = 
        		icalcomponent_vanew(
        		    ICAL_VCALENDAR_COMPONENT,
	        	    icalproperty_new_version("2.0"),
        		    icalproperty_new_prodid(
                		 "-//Claws Mail//NONSGML Claws Mail Calendar//EN"),
			    icalproperty_new_calscale("GREGORIAN"),
			    icalproperty_new_method(ICAL_METHOD_PUBLISH),
        		    (void*)0
	        	    ); 	

	if (!calendar) {
		g_warning("can't generate calendar");
		g_free(tmpfile);
		icalcomponent_free(ievent);
		return NULL;
	}
	
        icalcomponent_add_component(calendar, ievent);

	if (use_calendar)
		return NULL;

	headers = write_headers_ical(account, ievent, orga);

	if (!headers) {
		g_warning("can't get headers");
		g_free(tmpfile);
		icalcomponent_free(calendar);
		return NULL;
	}

	lines = g_strsplit(icalcomponent_as_ical_string(calendar), "\n", 0);
	qpbody = g_strdup("");
	
	/* encode to quoted-printable */
	while (lines[i]) {
		gint e_len = strlen(qpbody), n_len = 0;
		gchar *outline = conv_codeset_strdup(lines[i], CS_UTF_8, conv_get_outgoing_charset_str());
		gchar *qpoutline = g_malloc(strlen(outline)*8 + 1);
		
		qp_encode_line(qpoutline, (guchar *)outline);
		n_len = strlen(qpoutline);
		
		qpbody = g_realloc(qpbody, e_len + n_len + 1);
		strcpy(qpbody+e_len, qpoutline);
		*(qpbody+n_len+e_len) = '\0';
		
		g_free(outline);
		g_free(qpoutline);
		i++;
	}
	
	body = g_strdup_printf("%s"
			       "\n"
			       "%s", headers, qpbody);
	
	if (str_write_to_file(body, tmpfile, FALSE) < 0) {
		g_free(tmpfile);
		tmpfile = NULL;
	} else
		chmod(tmpfile, S_IRUSR|S_IWUSR);

	g_strfreev(lines);
	g_free(body);
	g_free(qpbody);
	g_free(headers);
	icalcomponent_free(calendar);
	
	return tmpfile;	
}

VCalEvent * vcal_manager_new_event	(const gchar 	*uid, 
					 const gchar	*organizer,
					 const gchar	*orgname,
					 const gchar	*location,
					 const gchar	*summary,
					 const gchar	*description,
					 const gchar	*dtstart,
					 const gchar	*dtend,
					 const gchar	*recur,
					 const gchar	*tzid,
					 const gchar	*url,
					 icalproperty_method method,
					 gint 		 sequence,
					 icalcomponent_kind type)
{
	VCalEvent *event = g_new0(VCalEvent, 1);

	event->uid 		= g_strdup(uid?uid:"");
	event->organizer 	= g_strdup(organizer?organizer:"");
	event->orgname	 	= g_strdup(orgname?orgname:"");

	if (dtend && *(dtend)) {
		time_t tmp = icaltime_as_timet((icaltime_from_string(dtend)));
		GDateTime *dt = g_date_time_new_from_unix_local(tmp);
		event->end = g_date_time_format(dt, "%a, %e %b %Y %H:%M:%S %Z");
		g_date_time_unref(dt);
	}
	
	if (dtstart && *(dtstart)) {
		time_t tmp = icaltime_as_timet((icaltime_from_string(dtstart)));
		GDateTime *dt = g_date_time_new_from_unix_local(tmp);
		event->start = g_date_time_format(dt, "%a, %e %b %Y %H:%M:%S %Z");
		g_date_time_unref(dt);
	}
	event->dtstart		= g_strdup(dtstart?dtstart:"");
	event->dtend		= g_strdup(dtend?dtend:"");
	event->recur		= g_strdup(recur?recur:"");
	event->location 	= g_strdup(location?location:"");
	event->summary		= g_strdup(summary?summary:"");
	event->description	= g_strdup(description?description:"");
	event->url		= g_strdup(url?url:"");
	event->tzid		= g_strdup(tzid?tzid:"");
	event->method		= method;
	event->sequence		= sequence;
	event->type		= type;
	event->rec_occurrence		= FALSE;
	while (strchr(event->summary, '\n'))
		*(strchr(event->summary, '\n')) = ' ';

	return event;
}
					 
void vcal_manager_free_event (VCalEvent *event)
{
	GSList *cur;
	if (!event)
		return;

	g_free(event->uid);
	g_free(event->organizer);
	g_free(event->orgname);
	g_free(event->start);
	g_free(event->end);
	g_free(event->location);
	g_free(event->summary);
	g_free(event->dtstart);
	g_free(event->dtend);
	g_free(event->recur);
	g_free(event->tzid);
	g_free(event->description);
	g_free(event->url);
	for (cur = event->answers; cur; cur = cur->next) {
		answer_free((Answer *)cur->data);
	}
	g_slist_free(event->answers);
	g_free(event);
}

gchar *vcal_manager_get_event_path(void)
{
	static gchar *event_path = NULL;
	if (!event_path)
		event_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					"vcalendar", NULL);
	
	return event_path;
}

gchar *vcal_manager_get_event_file(const gchar *uid)
{
	gchar *tmp = g_strdup(uid);
	gchar *res = NULL;

	subst_for_filename(tmp);
	res = g_strconcat(vcal_manager_get_event_path(), G_DIR_SEPARATOR_S,
			   tmp, NULL);
	g_free(tmp);
	return res;
}

PrefsAccount *vcal_manager_get_account_from_event(VCalEvent *event)
{
	GSList *list = vcal_manager_get_answers_emails(event);
	GSList *cur = list;
	
	/* find an attendee corresponding to one of our accounts */
	while (cur && cur->data) {
		gchar *email = (gchar *)cur->data;
		if (account_find_from_address(email, FALSE)) {
			g_slist_free(list);
			return account_find_from_address(email, FALSE);
		}
		cur = cur->next;
	}
	g_slist_free(list);
	return NULL;
}

void vcal_manager_save_event (VCalEvent *event, gboolean export_after)
{
	XMLTag *tag = NULL;
	XMLNode *xmlnode = NULL;
	GNode *rootnode = NULL;
	PrefFile *pfile;
	gchar *path = NULL;
	GSList *answers = event->answers;
	gchar *tmp = NULL;
	gint tmp_method = event->method;
	
	tag = xml_tag_new("event");
	xml_tag_add_attr(tag, xml_attr_new("organizer", event->organizer));
	xml_tag_add_attr(tag, xml_attr_new("orgname", event->orgname));
	xml_tag_add_attr(tag, xml_attr_new("location", event->location));
	xml_tag_add_attr(tag, xml_attr_new("summary", event->summary));
	xml_tag_add_attr(tag, xml_attr_new("description", event->description));
	xml_tag_add_attr(tag, xml_attr_new("url", event->url));
	xml_tag_add_attr(tag, xml_attr_new("dtstart", event->dtstart));
	xml_tag_add_attr(tag, xml_attr_new("dtend", event->dtend));
	xml_tag_add_attr(tag, xml_attr_new("recur", event->recur));
	xml_tag_add_attr(tag, xml_attr_new("tzid", event->tzid));
	
	/* updating answers saves events, don't save them with reply type */
	if (tmp_method == ICAL_METHOD_REPLY)
		tmp_method = ICAL_METHOD_REQUEST;
	
	tmp = g_strdup_printf("%d", tmp_method);
	xml_tag_add_attr(tag, xml_attr_new("method", tmp));
	g_free(tmp);

	tmp = g_strdup_printf("%d", event->sequence);
	xml_tag_add_attr(tag, xml_attr_new("sequence", tmp));
	g_free(tmp);
	
	tmp = g_strdup_printf("%d", event->type);
	xml_tag_add_attr(tag, xml_attr_new("type", tmp));
	g_free(tmp);
	
	tmp = g_strdup_printf("%"CM_TIME_FORMAT, event->postponed);
	xml_tag_add_attr(tag, xml_attr_new("postponed", tmp));
	g_free(tmp);
	
	tmp = g_strdup_printf("%d", event->rec_occurrence);
	xml_tag_add_attr(tag, xml_attr_new("rec_occurrence", tmp));
	g_free(tmp);
	
	xmlnode = xml_node_new(tag, NULL);
	rootnode = g_node_new(xmlnode);
	
	while (answers && answers->data) {
		XMLNode *ansxmlnode = NULL;
		GNode *ansnode = NULL;
		XMLTag *anstag = xml_tag_new("answer");
		Answer *a = (Answer *)answers->data;
		xml_tag_add_attr(anstag, xml_attr_new("attendee", a->attendee));
		xml_tag_add_attr(anstag, xml_attr_new("name", a->name?a->name:""));
		tmp = g_strdup_printf("%d", a->answer);
		xml_tag_add_attr(anstag, xml_attr_new("answer", tmp));
		g_free(tmp);
		tmp = g_strdup_printf("%d", a->cutype);
		xml_tag_add_attr(anstag, xml_attr_new("cutype", tmp));
		g_free(tmp);
		ansxmlnode = xml_node_new(anstag, NULL);
		ansnode = g_node_new(ansxmlnode);
		g_node_append(rootnode, ansnode);
		answers = answers->next;
	}
	
	path = vcal_manager_get_event_file(event->uid);
					
	if ((pfile = prefs_write_open(path)) == NULL) {
		gchar *dir_path = vcal_manager_get_event_path();
		if (!is_dir_exist(dir_path) && make_dir(vcal_manager_get_event_path()) != 0) {
			g_free(dir_path);
			g_free(path);
			return;
		}
		g_free(dir_path);
		if ((pfile = prefs_write_open(path)) == NULL) {
			g_free(path);
			return;
		}
	}
	
	g_free(path);
	xml_file_put_xml_decl(pfile->fp);
	xml_write_tree(rootnode, pfile->fp);
	xml_free_tree(rootnode);

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write event");
		return;
	}
 
 	if (export_after)
		vcal_folder_export(NULL);
}

static VCalEvent *event_get_from_xml (const gchar *uid, GNode *node)
{
	XMLNode *xmlnode;
	GList *list;
	gchar *org = NULL, *location = NULL, *summary = NULL, *orgname = NULL;
	gchar *dtstart = NULL, *dtend = NULL, *tzid = NULL;
	gchar *description = NULL, *url = NULL, *recur = NULL;
	VCalEvent *event = NULL;
	icalproperty_method method = ICAL_METHOD_REQUEST;
	icalcomponent_kind type = ICAL_VEVENT_COMPONENT;
	gint sequence = 0, rec_occurrence = 0;
	time_t postponed = (time_t)0;
	
	g_return_val_if_fail(node->data != NULL, NULL);

	xmlnode = node->data;
	if (g_strcmp0(xmlnode->tag->tag, "event") != 0) {
		g_warning("tag name != \"event\"");
		return NULL;
	}
	
	list = xmlnode->tag->attr;
	for (; list != NULL; list = list->next) {
		XMLAttr *attr = list->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "organizer"))
			org = g_strdup(attr->value);
		if (!strcmp(attr->name, "orgname"))
			orgname = g_strdup(attr->value);
		if (!strcmp(attr->name, "location"))
			location = g_strdup(attr->value);
		if (!strcmp(attr->name, "summary"))
			summary = g_strdup(attr->value);
		if (!strcmp(attr->name, "description"))
			description = g_strdup(attr->value);
		if (!strcmp(attr->name, "url"))
			url = g_strdup(attr->value);
		if (!strcmp(attr->name, "dtstart"))
			dtstart = g_strdup(attr->value);
		if (!strcmp(attr->name, "dtend"))
			dtend = g_strdup(attr->value);
		if (!strcmp(attr->name, "recur"))
			recur = g_strdup(attr->value);
		if (!strcmp(attr->name, "tzid"))
			tzid = g_strdup(attr->value);
		if (!strcmp(attr->name, "type"))
			type = atoi(attr->value);
		if (!strcmp(attr->name, "method"))
			method = atoi(attr->value);
		if (!strcmp(attr->name, "sequence"))
			sequence = atoi(attr->value);
		if (!strcmp(attr->name, "postponed"))
			postponed = atoi(attr->value);
		if (!strcmp(attr->name, "rec_occurrence"))
			rec_occurrence = atoi(attr->value);
	}

	event = vcal_manager_new_event(uid, org, orgname, location, summary, description, 
					dtstart, dtend, recur, tzid, url, method, 
					sequence, type);

	event->postponed = postponed;
	event->rec_occurrence = rec_occurrence;

	g_free(org); 
	g_free(orgname); 
	g_free(location);
	g_free(summary); 
	g_free(description); 
	g_free(url); 
	g_free(dtstart); 
	g_free(dtend);
	g_free(recur);
	g_free(tzid);

	node = node->children;
	while (node != NULL) {
		gchar *attendee = NULL;
		gchar *name = NULL;
		icalparameter_partstat answer = ICAL_PARTSTAT_NEEDSACTION;
		icalparameter_cutype cutype   = ICAL_CUTYPE_INDIVIDUAL;
		
		xmlnode = node->data;
		if (g_strcmp0(xmlnode->tag->tag, "answer") != 0) {
			g_warning("tag name != \"answer\"");
			return event;
		}
		list = xmlnode->tag->attr;
		for (; list != NULL; list = list->next) {
			XMLAttr *attr = list->data;

			if (!attr || !attr->name || !attr->value) continue;
			if (!strcmp(attr->name, "attendee"))
				attendee = g_strdup(attr->value);
			if (!strcmp(attr->name, "name"))
				name = g_strdup(attr->value);
			if (!strcmp(attr->name, "answer"))
				answer = atoi(attr->value);
			if (!strcmp(attr->name, "cutype"))
				cutype = atoi(attr->value);
		}

		event->answers = g_slist_prepend(event->answers, answer_new(attendee, name, answer, cutype));
		g_free(attendee);
		g_free(name);
		node = node->next;
	}
	event->answers = g_slist_reverse(event->answers);
	
	return event;
}

VCalEvent *vcal_manager_load_event (const gchar *uid)
{
	GNode *node;
	gchar *path = NULL;
	VCalEvent *event = NULL;

	path = vcal_manager_get_event_file(uid);
	
	if (!is_file_exist(path)) {
		g_free(path);
		return NULL;
	}
	
	node = xml_parse_file(path);
	
	g_free(path);
	
	if (!node) {
		g_warning("no node");
		return NULL;
	}
	
	event = event_get_from_xml(uid, node);
	/* vcal_manager_event_print(event); */
	xml_free_tree(node);

	if (!event)
		return NULL;

	while (strchr(event->summary, '\n'))
		*(strchr(event->summary, '\n')) = ' ';

	return event;
	
}

void vcal_manager_update_answer (VCalEvent 	*event, 
				 const gchar 	*attendee,
				 const gchar	*name,
				 icalparameter_partstat ans,
				 icalparameter_cutype cutype)
{
	Answer *answer = NULL;
	GSList *existing = NULL;
	Answer *existing_a = NULL;
	
	if (!ans)
		return;
		
	answer = answer_new(attendee, name, ans, cutype);
	existing = answer_find(event, answer);
		
	if (existing) {
		existing_a = (Answer *)existing->data;
	
		if (!answer->name && existing_a->name)
			answer->name = g_strdup(existing_a->name);
		if (!answer->cutype && existing_a->cutype)
			answer->cutype = existing_a->cutype;

		answer_remove(event, answer);
	}

	answer_add(event, answer);
	
	vcal_manager_save_event(event, FALSE);
}

static gchar *write_headers(PrefsAccount 	*account, 
			    VCalEvent 		*event,
			    gboolean		 short_headers,
			    gboolean 		 is_reply, 
			    gboolean 		 is_pseudo_display)
{
	gchar *subject = NULL;
	gchar date[RFC822_DATE_BUFFSIZE];
	gchar *save_folder = NULL;
	gchar *result = NULL;
	gchar *queue_headers = NULL;
	gchar *method_str = NULL;
	gchar *attendees = NULL;
	icalparameter_partstat status;
	gchar *prefix = NULL;
	gchar enc_subject[512], enc_from[512], *from = NULL;
	gchar *msgid;
	gchar *calmsgid = NULL;

	cm_return_val_if_fail(account != NULL, NULL);

	memset(date, 0, sizeof(date));
	
	if (is_pseudo_display) {
		struct icaltimetype itt = (icaltime_from_string(event->dtstart));
		time_t t = icaltime_as_timet(itt);
		get_rfc822_date_from_time_t(date, sizeof(date), t);
	} else {
		get_rfc822_date(date, sizeof(date));
	}
	
	if (account_get_special_folder(account, F_OUTBOX)) {
		save_folder = folder_item_get_identifier(account_get_special_folder
				  (account, F_OUTBOX));
	}
	
	if (!is_reply) {
		GSList *cur = event->answers;
		while (cur && cur->data) {
			gchar *tmp = NULL;
			Answer *a = (Answer *)cur->data;
			
			if (strcasecmp(a->attendee, event->organizer)) {
				if (attendees) {
					tmp = g_strdup_printf("%s>,\n <%s", attendees, a->attendee);
					g_free(attendees);
					attendees = tmp;
				} else {
					attendees = g_strdup_printf("%s", a->attendee);
				}
			}
			cur = cur->next;
		}
	}
	
	if (!short_headers) {
		queue_headers = g_strdup_printf("S:%s\n"
				"SSV:%s\n"
				"R:<%s>\n"
				"MAID:%d\n"
				"%s%s%s"
				"X-Claws-End-Special-Headers: 1\n",
				account->address,
				account->smtp_server,
				is_reply ? event->organizer:attendees,
				account->account_id,
				save_folder?"SCF:":"",
				save_folder?save_folder:"",
				save_folder?"\n":"");
	} else {
		queue_headers = g_strdup("");
	}
	
	prefix = "";
	if (is_reply) {
		method_str = "REPLY";
		status = vcal_manager_get_reply_for_attendee(event, account->address);
		if (status == ICAL_PARTSTAT_ACCEPTED)
			prefix = _("Accepted: ");
		else if (status == ICAL_PARTSTAT_DECLINED)
			prefix = _("Declined: ");
		else if (status == ICAL_PARTSTAT_TENTATIVE)
			prefix = _("Tentatively Accepted: ");
		else 
			prefix = "Re: "; 
	} else if (event->method == ICAL_METHOD_PUBLISH) {
		method_str = "PUBLISH";
	} else if (event->method == ICAL_METHOD_CANCEL) {
		method_str = "CANCEL";
	} else {
		method_str = "REQUEST";
	}
	
	subject = g_strdup_printf("%s%s", prefix, event->summary);

	conv_encode_header_full(enc_subject, sizeof(enc_subject), subject, strlen("Subject: "), 
			FALSE, conv_get_outgoing_charset_str());
	from = is_reply?account->name:(event->orgname?event->orgname:"");
	conv_encode_header_full(enc_from, sizeof(enc_from), from, strlen("From: "), 
			TRUE, conv_get_outgoing_charset_str());

	if (is_pseudo_display && event->uid) {
		calmsgid = g_strdup_printf("Message-ID: <%s>\n",event->uid);
	} else {
		calmsgid = g_strdup("");
	}

	msgid = prefs_account_generate_msgid(account);

	result = g_strdup_printf("%s"
				"From: %s <%s>\n"
				"To: <%s>\n"
				"Subject: %s\n"
				"Date: %s\n"
				"MIME-Version: 1.0\n"
				"Content-Type: text/calendar; method=%s; charset=\"%s\"\n"
				"Content-Transfer-Encoding: 8bit\n"
				"%s"
				"%s: <%s>\n",
				queue_headers,
				enc_from,
				is_reply ? account->address:event->organizer,
				is_reply ? event->organizer:(attendees?attendees:event->organizer),
				enc_subject,
				date,
				method_str,
				CS_UTF_8,
				calmsgid,
				is_pseudo_display?
					"In-Reply-To":"Message-ID",
				is_pseudo_display?
					event_to_today_str(event, 0):msgid);
	g_free(calmsgid);
	g_free(subject);
	g_free(save_folder);
	g_free(queue_headers);
	g_free(attendees);
	g_free(msgid);
	return result;			
                                                                               

}

static gchar *write_headers_ical(PrefsAccount 	*account, 
			    icalcomponent	*ievent,
			    gchar 		*orga)
{
	gchar subject[512];
	gchar date[RFC822_DATE_BUFFSIZE];
	gchar *result = NULL;
	gchar *method_str = NULL;
	gchar *summary = NULL;
	gchar *organizer = NULL;
	gchar *orgname = NULL;
	icalproperty *prop = NULL;
	gchar *calmsgid = NULL;

	time_t t = (time_t)0;

	memset(subject, 0, sizeof(subject));
	memset(date, 0, sizeof(date));
	
	prop = icalcomponent_get_first_property(ievent, ICAL_SUMMARY_PROPERTY);
	summary = g_strdup(icalproperty_get_summary(prop));
	icalproperty_free(prop);
	if (!summary)
		summary = g_strdup(_("[no summary]"));
	
	while (strchr(summary, '\n'))
		*(strchr(summary, '\n')) = ' ';

	prop = icalcomponent_get_first_property(ievent, ICAL_ORGANIZER_PROPERTY);
	if (prop) {
		organizer = g_strdup(icalproperty_get_organizer(prop));
		if (icalproperty_get_parameter_as_string(prop, "CN") != NULL)
			orgname = g_strdup(icalproperty_get_parameter_as_string(prop, "CN"));

		icalproperty_free(prop);
	} else {
		organizer = orga? g_strdup(orga):g_strdup("");
	}

	prop = icalcomponent_get_first_property(ievent, ICAL_DTSTART_PROPERTY);
	if (prop) {
		t = icaltime_as_timet(icalproperty_get_dtstart(prop));
		get_rfc822_date_from_time_t(date, sizeof(date), t);
	} else {
		get_rfc822_date(date, sizeof(date));
	}

	conv_encode_header(subject, 511, summary, strlen("Subject: "), FALSE);
			
	method_str = "PUBLISH";

	prop = icalcomponent_get_first_property(ievent, ICAL_UID_PROPERTY);
	if (prop) {
		calmsgid = g_strdup_printf("Message-ID: <%s>\n",icalproperty_get_uid(prop));
		icalproperty_free(prop);
	} else {
		calmsgid = g_strdup("");
	}
	

	result = g_strdup_printf("From: %s <%s>\n"
				"To: <%s>\n"
				"Subject: %s%s\n"
				"Date: %s\n"
				"MIME-Version: 1.0\n"
				"Content-Type: text/calendar; method=%s; charset=\"%s\"; vcalsave=\"no\"\n"
				"Content-Transfer-Encoding: quoted-printable\n"
				"%s"
				"In-Reply-To: <%s>\n",
				orgname?orgname:"",
				!strncmp(organizer, "MAILTO:", 7) ? organizer+7 : organizer,
				account->address,
				"",
				subject,
				date,
				method_str,
				conv_get_outgoing_charset_str(),
				calmsgid,
				event_to_today_str(NULL, t));
	
	g_free(calmsgid);
	g_free(orgname);
	g_free(organizer);
	g_free(summary);

	return result;			
                                                                               

}

static gboolean vcal_manager_send (PrefsAccount 	*account, 
				     VCalEvent 		*event,
				     gboolean		 is_reply)
{
	gchar *tmpfile = NULL;
	gint msgnum;
	FolderItem *folderitem;
	gchar *msgpath = NULL;
	Folder *folder = NULL;
	
	tmpfile = vcal_manager_event_dump(event, is_reply, FALSE, NULL, TRUE);

	if (!tmpfile)
		return FALSE;

	folderitem = account_get_special_folder(account, F_QUEUE);
	if (!folderitem) {
		g_warning("can't find queue folder for %s", account->address);
		g_unlink(tmpfile);
		g_free(tmpfile);
		return FALSE;
	}
	folder_item_scan(folderitem);
	
	if ((msgnum = folder_item_add_msg(folderitem, tmpfile, NULL, TRUE)) < 0) {
		g_warning("can't queue the message");
		g_unlink(tmpfile);
		g_free(tmpfile);
		return FALSE;
	}

	msgpath = folder_item_fetch_msg(folderitem, msgnum);
	
	if (!prefs_common_get_prefs()->work_offline) {
		gchar *err = NULL;
		gboolean queued_removed = FALSE;
		gint val = procmsg_send_message_queue_with_lock(msgpath, &err, folderitem, msgnum, &queued_removed);
		if (val == 0) {
			if (!queued_removed)
				folder_item_remove_msg(folderitem, msgnum);
			folder_item_scan(folderitem);
		} else if (err) {
			alertpanel_error_log("%s", err);
			g_free(err);
		}
	}
	g_unlink(tmpfile);
	g_free(tmpfile);
	g_free(msgpath);

	folder = folder_find_from_name ("vCalendar", vcal_folder_get_class());
	if (folder) {
		folder_item_scan(folder->inbox);
		vcalviewer_reload(folder->inbox);
	} else
		g_warning("couldn't find vCalendar folder class");
	return TRUE;
}

gboolean vcal_manager_reply (PrefsAccount 	*account, 
			     VCalEvent 		*event)
{
	return vcal_manager_send(account, event, TRUE);
}

gboolean vcal_manager_request (PrefsAccount 	*account, 
			       VCalEvent 	*event)
{
	return vcal_manager_send(account, event, FALSE);
}

EventTime event_to_today(VCalEvent *event, time_t t)
{
	struct tm evtstart, today;
	time_t evtstart_t, today_t;
	struct icaltimetype itt;

	tzset();
	
	today_t = time(NULL);
	if (event) {
		itt = icaltime_from_string(event->dtstart);
		evtstart_t = icaltime_as_timet(itt);
	} else {
		evtstart_t = t;
	}
	
#ifndef G_OS_WIN32
	struct tm buft;
	today = *localtime_r(&today_t, &buft);
	localtime_r(&evtstart_t, &evtstart);
#else
	if (today_t < 0)
		today_t = 1;
	if (evtstart_t < 0)
		evtstart_t = 1;
	today = *localtime(&today_t);
	evtstart = *localtime(&evtstart_t);
#endif
	
	if (today.tm_year == evtstart.tm_year) {
		int days = evtstart.tm_yday - today.tm_yday;
		if (days < 0) {
			return EVENT_PAST;
		} else if (days == 0) {
			return EVENT_TODAY;
		} else if (days == 1) {
			return EVENT_TOMORROW;
		} else if (days > 1 && days < 7) {
			return EVENT_THISWEEK;
		} else {
			return EVENT_LATER;
		}
	} else if (today.tm_year > evtstart.tm_year) {
		return EVENT_PAST;
	} else if (today.tm_year == evtstart.tm_year - 1) {
		int days = ((365 - today.tm_yday) + evtstart.tm_yday);
		if (days == 0) {
			return EVENT_TODAY;
		} else if (days == 1) {
			return EVENT_TOMORROW;
		} else if (days > 1 && days < 7) {
			return EVENT_THISWEEK;
		} else {
			return EVENT_LATER;
		}
	} else 
		return EVENT_LATER;
}

const gchar *event_to_today_str(VCalEvent *event, time_t t)
{
	EventTime days = event_to_today(event, t);
	switch(days) {
	case EVENT_PAST:
		return EVENT_PAST_ID;
	case EVENT_TODAY:
		return EVENT_TODAY_ID;
	case EVENT_TOMORROW:
		return EVENT_TOMORROW_ID;
	case EVENT_THISWEEK:
		return EVENT_THISWEEK_ID;
	case EVENT_LATER:
		return EVENT_LATER_ID;
	}
	return NULL;
}
