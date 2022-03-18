/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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

#ifndef __VCAL_MANAGER_H__
#define __VCAL_MANAGER_H__

#include <glib.h>
#include <libical/ical.h>
#include "prefs_account.h"
#include "folder.h"

#define EVENT_PAST_ID "past-events@vcal"
#define EVENT_TODAY_ID "today-events@vcal"
#define EVENT_TOMORROW_ID "tomorrow-events@vcal"
#define EVENT_THISWEEK_ID "thisweek-events@vcal"
#define EVENT_LATER_ID "later-events@vcal"
typedef struct _VCalEvent VCalEvent;

struct _VCalEvent 
{
	gchar *uid;
	gchar *organizer;
	gchar *orgname;
	gchar *start;
	gchar *end;
	gchar *dtstart;
	gchar *dtend;
	gchar *recur;
	gchar *tzid;
	gchar *location;
	gchar *summary;
	gchar *description;
	GSList *answers;
	enum icalproperty_method method;
	gint sequence;	
	gchar *url;
	enum icalcomponent_kind type;
	time_t postponed;
	gboolean rec_occurrence;
};

typedef struct _Answer Answer;

struct _Answer {
	gchar *attendee;
	gchar *name;
	enum icalparameter_partstat answer;
	enum icalparameter_cutype cutype;
};

typedef enum {
	EVENT_PAST = 0,
	EVENT_TODAY,
	EVENT_TOMORROW,
	EVENT_THISWEEK,
	EVENT_LATER
} EventTime;
VCalEvent *vcal_manager_new_event	(const gchar 	*uid, 
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
					 enum icalproperty_method method,
					 gint		 sequence,
					 enum icalcomponent_kind type);
					 
void vcal_manager_free_event (VCalEvent *event);
void vcal_manager_save_event (VCalEvent *event, gboolean export_after);
void vcal_manager_update_answer (VCalEvent 	*event, 
				 const gchar 	*attendee,
				 const gchar 	*name,
				 enum icalparameter_partstat ans,
				 enum icalparameter_cutype cutype);

VCalEvent *vcal_manager_load_event (const gchar *uid);
gboolean vcal_manager_reply (PrefsAccount 	*account, 
			     VCalEvent 		*event);
gboolean vcal_manager_request (PrefsAccount 	*account, 
			       VCalEvent 	*event);

GSList *vcal_manager_get_answers_emails(VCalEvent *event);
gchar *vcal_manager_get_attendee_name(VCalEvent *event, const gchar *attendee);
gchar *vcal_manager_get_reply_text_for_attendee(VCalEvent *event, const gchar *att);
gchar *vcal_manager_get_cutype_text_for_attendee(VCalEvent *event, const gchar *att);
enum icalparameter_partstat vcal_manager_get_reply_for_attendee(VCalEvent *event, const gchar *att);
enum icalparameter_cutype vcal_manager_get_cutype_for_attendee(VCalEvent *event, const gchar *att);
gchar *vcal_manager_get_event_path(void);
gchar *vcal_manager_get_event_file(const gchar *uid);
gchar *vcal_manager_event_dump(VCalEvent *event, gboolean change_date, gboolean
		change_from, icalcomponent *use_calendar, gboolean modif);
gchar *vcal_manager_icalevent_dump(icalcomponent *event, gchar *orga, icalcomponent *use_calendar);
gchar *vcal_manager_dateevent_dump(const gchar *uid, FolderItem *item);

gchar *vcal_manager_answer_get_text(enum icalparameter_partstat ans);
gchar *vcal_manager_cutype_get_text(enum icalparameter_cutype type);

PrefsAccount *vcal_manager_get_account_from_event(VCalEvent *event);

void vcal_manager_event_print(VCalEvent *event);
EventTime event_to_today(VCalEvent *event, time_t t);
const gchar *event_to_today_str(VCalEvent *event, time_t t);
void vcal_manager_copy_attendees(VCalEvent *src, VCalEvent *dest);
Answer *answer_new(const gchar *attendee, 
			  const gchar *name,
			  enum icalparameter_partstat ans,
			  enum icalparameter_cutype cutype);
#endif
