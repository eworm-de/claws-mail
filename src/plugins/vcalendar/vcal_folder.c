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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <curl/curlver.h>
#include <ctype.h>

#include "account.h"
#include "file-utils.h"
#include "utils.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "folderview.h"
#include "folder_item_prefs.h"
#include "vcalendar.h"
#include "vcal_folder.h"
#include "vcal_prefs.h"
#include "vcal_manager.h"
#include "vcal_meeting_gtk.h"
#include "vcal_interface.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "claws.h"
#include "menu.h"
#include "inputdialog.h"
#include "inc.h"
#include "xml.h"
#include "alertpanel.h"
#include "log.h"
#include "mainwindow.h"
#include "statusbar.h"
#include "msgcache.h"
#include "passwordstore.h"
#include "timing.h"
#include "messageview.h"

#include <gtk/gtk.h>
#include <dirent.h>

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

typedef struct _thread_data {
	const gchar *url;
	gchar *result;
	gchar *error;
	gboolean done;
} thread_data;

typedef struct _IcalFeedData {
	icalcomponent *event;
	gchar *pseudoevent_id;
} IcalFeedData;

typedef struct _VCalFolder VCalFolder;
typedef struct _VCalFolderItem VCalFolderItem;

static Folder *vcal_folder_new(const gchar * name,
				  const gchar * folder);
static void vcal_folder_destroy(Folder * folder);
static void vcal_item_destroy(Folder *folder, FolderItem *_item);
static gchar *vcal_item_get_path(Folder *folder, FolderItem *item);

static gint vcal_scan_tree(Folder * folder);
static FolderItem *vcal_item_new(Folder * folder);
static gint vcal_get_num_list(Folder * folder, FolderItem * item,
				 MsgNumberList ** list,
				 gboolean * old_uids_valid);
static MsgInfo *vcal_get_msginfo(Folder * folder, FolderItem * item,
				    gint num);
static gchar *vcal_fetch_msg(Folder * folder, FolderItem * item,
				gint num);
static gint vcal_add_msg(Folder * folder, FolderItem * _dest,
			    const gchar * file, MsgFlags * flags);
static gint vcal_remove_msg(Folder * folder, FolderItem * _item,
			       gint num);
static FolderItem *vcal_create_folder(Folder * folder,
					 FolderItem * parent,
					 const gchar * name);
static gint vcal_create_tree(Folder *folder);
static gint vcal_remove_folder(Folder *folder, FolderItem *item);
static gboolean vcal_scan_required(Folder *folder, FolderItem *item);
static void vcal_set_mtime(Folder *folder, FolderItem *item);
static void vcal_change_flags(Folder *folder, FolderItem *_item, MsgInfo *msginfo, MsgPermFlags newflags);

static void new_meeting_cb(GtkAction *action, gpointer data);
static void export_cal_cb(GtkAction *action, gpointer data);
static void subscribe_cal_cb(GtkAction *action, gpointer data);
static void check_subs_cb(GtkAction *action, gpointer data);
static void unsubscribe_cal_cb(GtkAction *action, gpointer data);
static void rename_cb(GtkAction *action, gpointer data);
static void set_view_cb(GtkAction *action, GtkRadioAction *current, gpointer data);

static void add_menuitems(GtkUIManager *ui_manager, FolderItem *item);
static void set_sensitivity(GtkUIManager *ui_manager, FolderItem *item);

static void update_subscription(const gchar *uri, gboolean verbose);
static void vcal_folder_set_batch	(Folder		*folder,
					 FolderItem	*item,
					 gboolean	 batch);
static void convert_to_utc(icalcomponent *calendar);

gboolean vcal_subscribe_uri(Folder *folder, const gchar *uri);

FolderClass vcal_class;

static GSList *created_files = NULL;
static GHashTable *hash_uids = NULL;

struct _VCalFolder
{
	Folder folder;
};

struct _VCalFolderItem
{
	FolderItem item;
	gchar *uri;
	gchar *feed;
	icalcomponent *cal;
	GSList *numlist;
	GSList *evtlist;
	gboolean batching;
	gboolean dirty;
	day_win *dw;
	month_win *mw;
	time_t last_fetch;
	int use_cal_view;
};

static char *vcal_popup_labels[] =
{
	N_("_New meeting..."),
	N_("_Export calendar..."),
	N_("_Subscribe to Webcal..."),
	N_("_Unsubscribe..."),
	N_("_Rename..."),
	N_("U_pdate subscriptions"),
	N_("_List view"),
	N_("_Week view"),
	N_("_Month view"),
	NULL
};

static GtkActionEntry vcal_popup_entries[] = 
{
	{"FolderViewPopup/NewMeeting",		NULL, NULL, NULL, NULL, G_CALLBACK(new_meeting_cb) },
	{"FolderViewPopup/ExportCal",		NULL, NULL, NULL, NULL, G_CALLBACK(export_cal_cb) },

	{"FolderViewPopup/SubscribeCal",	NULL, NULL, NULL, NULL, G_CALLBACK(subscribe_cal_cb) },
	{"FolderViewPopup/UnsubscribeCal",	NULL, NULL, NULL, NULL, G_CALLBACK(unsubscribe_cal_cb) },

	{"FolderViewPopup/RenameFolder",	NULL, NULL, NULL, NULL, G_CALLBACK(rename_cb) },

	{"FolderViewPopup/CheckSubs",		NULL, NULL, NULL, NULL, G_CALLBACK(check_subs_cb) },

};			

static GtkRadioActionEntry vcal_popup_radio_entries[] = { /* set_view_cb */
	{"FolderViewPopup/ListView",		NULL, NULL, NULL, NULL, 0 }, 
	{"FolderViewPopup/WeekView",		NULL, NULL, NULL, NULL, 1 }, 
	{"FolderViewPopup/MonthView",		NULL, NULL, NULL, NULL, 2 }, 
};
		
static IcalFeedData *icalfeeddata_new(icalcomponent *evt, gchar *str)
{
	IcalFeedData *data = g_new0(IcalFeedData, 1);
	if (str)
		data->pseudoevent_id = g_strdup(str);
	data->event = evt;
	return data;
}

static void icalfeeddata_free(IcalFeedData *data)
{
	g_free(data->pseudoevent_id);
	if (data->event)
		icalcomponent_free(data->event);
	g_free(data);
}

static void slist_free_icalfeeddata(GSList *list)
{
	while (list) {
		IcalFeedData *data = (IcalFeedData *)list->data;
		icalfeeddata_free(data);
		list = list->next;
	}
}

static void vcal_fill_popup_menu_labels(void)
{
	vcal_popup_entries[0].label = _(vcal_popup_labels[0]);
	vcal_popup_entries[1].label = _(vcal_popup_labels[1]);
	vcal_popup_entries[2].label = _(vcal_popup_labels[2]);
	vcal_popup_entries[3].label = _(vcal_popup_labels[3]);
	vcal_popup_entries[4].label = _(vcal_popup_labels[4]);
	vcal_popup_entries[5].label = _(vcal_popup_labels[5]);
	vcal_popup_radio_entries[0].label = _(vcal_popup_labels[6]);
	vcal_popup_radio_entries[1].label = _(vcal_popup_labels[7]);
	vcal_popup_radio_entries[2].label = _(vcal_popup_labels[8]);
}

static FolderViewPopup vcal_popup =
{
	PLUGIN_NAME,
	"<vCalendar>",
	vcal_popup_entries,
	G_N_ELEMENTS(vcal_popup_entries),
	NULL, 0,
	vcal_popup_radio_entries, 
	G_N_ELEMENTS(vcal_popup_radio_entries), 1, set_view_cb,
	add_menuitems,
	set_sensitivity
};

static void vcal_item_set_xml(Folder *folder, FolderItem *item, XMLTag *tag)
{
	GList *cur;
	folder_item_set_xml(folder, item, tag);
	gboolean found_cal_view_setting = FALSE;

	for (cur = tag->attr; cur != NULL; cur = g_list_next(cur)) {
		XMLAttr *attr = (XMLAttr *) cur->data;

		if (!attr || !attr->name || !attr->value) continue;
		if (!strcmp(attr->name, "uri")) {
			if (((VCalFolderItem *)item)->uri != NULL)
				g_free(((VCalFolderItem *)item)->uri);
			((VCalFolderItem *)item)->uri = g_strdup(attr->value);
		} 
		if (!strcmp(attr->name, "use_cal_view")) {
			found_cal_view_setting = TRUE;
			((VCalFolderItem *)item)->use_cal_view = atoi(attr->value);
		} 
	}
	if (((VCalFolderItem *)item)->uri == NULL) {
		/* give a path to inbox */
		g_free(item->path);
		item->path = g_strdup(".meetings");
	}
	if (!found_cal_view_setting)
		((VCalFolderItem *)item)->use_cal_view = 1; /*week view */
	
}

static XMLTag *vcal_item_get_xml(Folder *folder, FolderItem *item)
{
	XMLTag *tag;

	tag = folder_item_get_xml(folder, item);

	if (((VCalFolderItem *)item)->uri)
		xml_tag_add_attr(tag, xml_attr_new("uri", ((VCalFolderItem *)item)->uri));

	xml_tag_add_attr(tag, xml_attr_new_int("use_cal_view", ((VCalFolderItem *)item)->use_cal_view));

	return tag;
}

static gint vcal_rename_folder(Folder *folder, FolderItem *item,
			     const gchar *name)
{
	if (!name)
		return -1;
	g_free(item->name);
	item->name = g_strdup(name);
	return 0;
}

static void vcal_get_sort_type(Folder *folder, FolderSortKey *sort_key,
			       FolderSortType *sort_type)
{
	if (sort_key)
		*sort_key = SORT_BY_DATE;
}

static void vcal_item_opened(FolderItem *item)
{
	struct tm tmdate;
	time_t t = time(NULL);
#ifndef G_OS_WIN32
	localtime_r(&t, &tmdate);
#else
	if (t < 0)
		t = 1;
	tmdate = *localtime(&t);
#endif
	if (!((VCalFolderItem *)(item))->dw 
	    && ((VCalFolderItem *)(item))->use_cal_view == 1)
		((VCalFolderItem *)(item))->dw = create_day_win(item, tmdate);		
	else if (!((VCalFolderItem *)(item))->mw 
	    && ((VCalFolderItem *)(item))->use_cal_view == 2)
		((VCalFolderItem *)(item))->mw = create_month_win(item, tmdate);
	else if (((VCalFolderItem *)(item))->use_cal_view != 0)
		vcal_folder_refresh_cal(item);
}

void vcal_folder_refresh_cal(FolderItem *item)
{
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (item->folder != folder)
		return;
	if (((VCalFolderItem *)(item))->dw)
		refresh_day_win(((VCalFolderItem *)(item))->dw);
	if (((VCalFolderItem *)(item))->mw)
		refresh_month_win(((VCalFolderItem *)(item))->mw);
}

static void vcal_item_closed(FolderItem *item)
{
	if (((VCalFolderItem *)(item))->dw) {
		dw_close_window(((VCalFolderItem *)(item))->dw);
		((VCalFolderItem *)(item))->dw = NULL;
	}
	if (((VCalFolderItem *)(item))->mw) {
		mw_close_window(((VCalFolderItem *)(item))->mw);
		((VCalFolderItem *)(item))->mw = NULL;
	}
}

FolderClass *vcal_folder_get_class()
{
	if (vcal_class.idstr == NULL) {
		debug_print("register class\n");
		vcal_class.type = F_UNKNOWN;
		vcal_class.idstr = PLUGIN_NAME;
		vcal_class.uistr = PLUGIN_NAME;

		/* Folder functions */
		vcal_class.new_folder = vcal_folder_new;
		vcal_class.destroy_folder = vcal_folder_destroy;
		vcal_class.set_xml = folder_set_xml;
		vcal_class.get_xml = folder_get_xml;
		vcal_class.item_set_xml = vcal_item_set_xml;
		vcal_class.item_get_xml = vcal_item_get_xml;
		vcal_class.scan_tree = vcal_scan_tree;
		vcal_class.create_tree = vcal_create_tree;
		vcal_class.get_sort_type = vcal_get_sort_type;

		/* FolderItem functions */
		vcal_class.item_new = vcal_item_new;
		vcal_class.item_destroy = vcal_item_destroy;
		vcal_class.item_get_path = vcal_item_get_path;
		vcal_class.create_folder = vcal_create_folder;
		vcal_class.remove_folder = vcal_remove_folder;
		vcal_class.rename_folder = vcal_rename_folder;
		vcal_class.scan_required = vcal_scan_required;
		vcal_class.set_mtime = vcal_set_mtime;
		vcal_class.get_num_list = vcal_get_num_list;
		vcal_class.set_batch = vcal_folder_set_batch;

		/* Message functions */
		vcal_class.get_msginfo = vcal_get_msginfo;
		vcal_class.fetch_msg = vcal_fetch_msg;
		vcal_class.add_msg = vcal_add_msg;
		vcal_class.copy_msg = NULL;
		vcal_class.remove_msg = vcal_remove_msg;
		vcal_class.change_flags = vcal_change_flags;
		vcal_class.subscribe = vcal_subscribe_uri;

		/* FolderView functions */
		vcal_class.item_opened = vcal_item_opened;
		vcal_class.item_closed = vcal_item_closed;
		debug_print("registered class for real\n");
	}

	return &vcal_class;
}

static void vcal_folder_set_batch	(Folder		*folder,
					 FolderItem	*_item,
					 gboolean	 batch)
{
	VCalFolderItem *item = (VCalFolderItem *)_item;

	g_return_if_fail(item != NULL);
	
	if (item->batching == batch)
		return;
	
	if (batch) {
		item->batching = TRUE;
		debug_print("vcal switching to batch mode\n");
	} else {
		debug_print("vcal switching away from batch mode\n");
		/* ici */
		item->batching = FALSE;
		if (item->dirty)
			vcal_folder_export(folder);
		item->dirty = FALSE;
	}
}

static Folder *vcal_folder_new(const gchar * name,
				  const gchar * path)
{
	VCalFolder *folder;		   
	
	debug_print("vcal_folder_new\n");
	folder = g_new0(VCalFolder, 1);
	FOLDER(folder)->klass = &vcal_class;
	folder_init(FOLDER(folder), name);

	return FOLDER(folder);
}

static void vcal_folder_destroy(Folder *_folder)
{
}

static FolderItem *vcal_item_new(Folder *folder)
{
	VCalFolderItem *item;
	item = g_new0(VCalFolderItem, 1);
	item->use_cal_view = 1;
	return (FolderItem *) item;

}

static void vcal_item_destroy(Folder *folder, FolderItem *_item)
{
	VCalFolderItem *item = (VCalFolderItem *)_item;
	g_return_if_fail(item != NULL);
	g_free(item);
}

static gchar *vcal_item_get_path(Folder *folder, FolderItem *item)
{
	VCalFolderItem *fitem = (VCalFolderItem *)item;
	if (fitem->uri == NULL)
		return g_strdup(vcal_manager_get_event_path());
	else {
		gchar *path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					"vcalendar", G_DIR_SEPARATOR_S,
					item->path, NULL);
		return path;
	}
}

static gint vcal_scan_tree(Folder *folder)
{
	g_return_val_if_fail(folder != NULL, -1);

	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;

	debug_print("scanning tree\n");
	vcal_create_tree(folder);

	return 0;
}

gboolean manual_update = TRUE;

static gint feed_fetch(FolderItem *fitem, MsgNumberList ** list, gboolean *old_uids_valid)
{
	VCalFolderItem *item = (VCalFolderItem *)fitem;
	icalcomponent *evt = NULL;
	icalcomponent_kind type = ICAL_VEVENT_COMPONENT;
	gint num = 1;
	gint past_msg = -1, today_msg = -1, tomorrow_msg = -1, 
		thisweek_msg = -1, later_msg = -1;

	debug_print("fetching\n");

	if (!item->uri) {
		debug_print("no uri!\n");
		return -1;
	}

	update_subscription(item->uri, TRUE);

	*old_uids_valid = FALSE;
	*list = NULL;

	if (item->cal) {
		evt = icalcomponent_get_first_component(
			item->cal, ICAL_VEVENT_COMPONENT);
		if (evt == NULL) {
			evt = icalcomponent_get_first_component(
				item->cal, ICAL_VTODO_COMPONENT);
			if (evt != NULL)
				type = ICAL_VTODO_COMPONENT;
		}
	} else
		debug_print("no cal!\n");

	if (evt == NULL)
		debug_print("no event\n");

	if (item->numlist) {
		g_slist_free(item->numlist);
		item->numlist = NULL;
	}

	if (item->evtlist) {
		slist_free_icalfeeddata(item->evtlist);
		g_slist_free(item->evtlist);
		item->evtlist = NULL;
	}

	while (evt) {
		icalproperty *prop;
		icalproperty *rprop = icalcomponent_get_first_property(evt, ICAL_RRULE_PROPERTY);
		struct icalrecurrencetype recur;
        	struct icaltimetype next;
        	icalrecur_iterator* ritr = NULL;
		EventTime days;

		if (rprop) {
			icalproperty *rprop2;
			recur = icalproperty_get_rrule(rprop);
			rprop2 = icalproperty_new_rrule(recur);
			prop = icalcomponent_get_first_property(evt, ICAL_DTSTART_PROPERTY);
			if (prop) {
        			ritr = icalrecur_iterator_new(recur, icalproperty_get_dtstart(prop));
				next = icalrecur_iterator_next(ritr); /* skip first one */
			}
			
			rprop = rprop2;
		} 

		prop = icalcomponent_get_first_property(evt, ICAL_UID_PROPERTY);
		if (prop) {
			gchar *orig_uid = NULL;
			gchar *uid = g_strdup(icalproperty_get_uid(prop));
			IcalFeedData *data = icalfeeddata_new(
						icalcomponent_new_clone(evt), NULL);
			int i = 0;
			orig_uid = g_strdup(uid);

add_new:			
			item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
			item->evtlist = g_slist_prepend(item->evtlist, data);
			data = NULL;
			debug_print("add %d : %s\n", num, uid);
			g_free(uid);
			num++;
			prop = icalcomponent_get_first_property(evt, ICAL_DTSTART_PROPERTY);
			if (prop) {
				struct icaltimetype itt = icalproperty_get_dtstart(prop);
				days = event_to_today(NULL, icaltime_as_timet(itt));
				if (days == EVENT_PAST && past_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_PAST_ID);
					past_msg = num++;
				} else if (days == EVENT_TODAY && today_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_TODAY_ID);
					today_msg = num++;
				} else if (days == EVENT_TOMORROW && tomorrow_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_TOMORROW_ID);
					tomorrow_msg = num++;
				} else if (days == EVENT_THISWEEK && thisweek_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_THISWEEK_ID);
					thisweek_msg = num++;
				} else if (days == EVENT_LATER && later_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_LATER_ID);
					later_msg = num++;
				}
			} else {
				if (past_msg == -1) {
					item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
					data = icalfeeddata_new(NULL, EVENT_PAST_ID);
					past_msg = num++;
				}
			}
			if (data) {
				item->evtlist = g_slist_prepend(item->evtlist, data);
				data = NULL;
			}
			if (rprop && ritr) {
				struct icaldurationtype ical_dur;
				struct icaltimetype dtstart = icaltime_null_time();
				struct icaltimetype dtend = icaltime_null_time();
				evt = icalcomponent_new_clone(evt);
				prop = icalcomponent_get_first_property(evt, ICAL_RRULE_PROPERTY);
				if (prop) {
					icalcomponent_remove_property(evt, prop);
					icalproperty_free(prop);
				}
				prop = icalcomponent_get_first_property(evt, ICAL_DTSTART_PROPERTY);
				if (prop)
					dtstart = icalproperty_get_dtstart(prop);
				else
					debug_print("event has no DTSTART!\n");
				prop = icalcomponent_get_first_property(evt, ICAL_DTEND_PROPERTY);
				if (prop)
					dtend = icalproperty_get_dtend(prop);
				else
					debug_print("event has no DTEND!\n");
				ical_dur = icaltime_subtract(dtend, dtstart);
				next = icalrecur_iterator_next(ritr);
				if (!icaltime_is_null_time(next) &&
				    !icaltime_is_null_time(dtstart) && i < 100) {
					prop = icalcomponent_get_first_property(evt, ICAL_DTSTART_PROPERTY);
					icalproperty_set_dtstart(prop, next);

					prop = icalcomponent_get_first_property(evt, ICAL_DTEND_PROPERTY);
					if (prop)
						icalproperty_set_dtend(prop, icaltime_add(next, ical_dur));

					prop = icalcomponent_get_first_property(evt, ICAL_UID_PROPERTY);
					uid = g_strdup_printf("%s-%d", orig_uid, i);
					icalproperty_set_uid(prop, uid);
					/* dont free uid, used after (add_new) */
					data = icalfeeddata_new(evt, NULL);
					i++;
					goto add_new;
				} else {
					icalcomponent_free(evt);
					evt = NULL;
				}
			}
			g_free(orig_uid);
		} else {
			debug_print("no uid!\n");
		}
		if (rprop) {
			icalproperty_free(rprop);
		}
		if (ritr) {
			icalrecur_iterator_free(ritr);
			ritr = NULL;
		}
		evt = icalcomponent_get_next_component(
			item->cal, type);
	}
	if (today_msg == -1) {
		IcalFeedData *data = icalfeeddata_new(NULL, EVENT_TODAY_ID);
		item->numlist = g_slist_prepend(item->numlist, GINT_TO_POINTER(num));
		today_msg = num++;
		item->evtlist = g_slist_prepend(item->evtlist, data);
	}
	item->numlist = g_slist_reverse(item->numlist);
	item->evtlist = g_slist_reverse(item->evtlist);
	
	*list = item->numlist ? g_slist_copy(item->numlist) : NULL;
	debug_print("return %d\n", num);
	return num;
}

#define VCAL_FOLDER_ADD_EVENT(event) \
{ \
 \
	*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
	debug_print("add %d %s\n", n_msg, event->uid); \
	n_msg++; \
	days = event_to_today(event, 0); \
 \
	if (days == EVENT_PAST && past_msg == -1) { \
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
		past_msg = n_msg++; \
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(past_msg), g_strdup(EVENT_PAST_ID)); \
	} else if (days == EVENT_TODAY && today_msg == -1) { \
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
		today_msg = n_msg++; \
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(today_msg), g_strdup(EVENT_TODAY_ID)); \
	} else if (days == EVENT_TOMORROW && tomorrow_msg == -1) { \
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
		tomorrow_msg = n_msg++; \
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(tomorrow_msg), g_strdup(EVENT_TOMORROW_ID)); \
	} else if (days == EVENT_THISWEEK && thisweek_msg == -1) { \
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
		thisweek_msg = n_msg++; \
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(thisweek_msg), g_strdup(EVENT_THISWEEK_ID)); \
	} else if (days == EVENT_LATER && later_msg == -1) { \
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); \
		later_msg = n_msg++; \
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(later_msg), g_strdup(EVENT_LATER_ID)); \
	} \
}

GSList *vcal_get_events_list(FolderItem *item)
{
	GDir *dp;
	const gchar *d;
	GSList *events = NULL;
	GError *error = NULL;

	if (item != item->folder->inbox) {
		GSList *subs = vcal_folder_get_webcal_events_for_folder(item);
		GSList *cur = NULL;
		for (cur = subs; cur; cur = cur->next) {
			/* Don't free that, it's done when subscriptions are
			 * fetched */
			icalcomponent *ical = (icalcomponent *)cur->data;
			VCalEvent *event = vcal_get_event_from_ical(
				icalcomponent_as_ical_string(ical), NULL);
			events = g_slist_prepend(events, event);
		}
		g_slist_free(subs);
		return events;
	}

	dp = g_dir_open(vcal_manager_get_event_path(), 0, &error);
	
	if (!dp) {
		debug_print("couldn't open dir '%s': %s (%d)\n",
				vcal_manager_get_event_path(), error->message, error->code);
		g_error_free(error);
		return 0;
	}

	while ((d = g_dir_read_name(dp)) != NULL) {
		VCalEvent *event = NULL;
		if (d[0] == '.' || strstr(d, ".bak")
		||  !strcmp(d, "internal.ics")
		||  !strcmp(d, "internal.ifb")
		||  !strcmp(d, "multisync")) 
			continue;

		event = vcal_manager_load_event(d);
		if (!event)
			continue;
		if (event->rec_occurrence) {
			vcal_manager_free_event(event);
			claws_unlink(d);
			continue;
		}

		if (event && event->method != ICAL_METHOD_CANCEL) {
			PrefsAccount *account = vcal_manager_get_account_from_event(event);
			enum icalparameter_partstat status =
				account ? vcal_manager_get_reply_for_attendee(event, account->address): ICAL_PARTSTAT_NEEDSACTION;
			if (status == ICAL_PARTSTAT_ACCEPTED
			||  status == ICAL_PARTSTAT_TENTATIVE) {
				events = g_slist_prepend(events, event);
			} else {
				vcal_manager_free_event(event);
				continue;
			}
			if ((status == ICAL_PARTSTAT_ACCEPTED
			     || status == ICAL_PARTSTAT_TENTATIVE) 
			    && event->recur && *(event->recur)) {
        			struct icalrecurrencetype recur;
        			struct icaltimetype dtstart;
        			struct icaltimetype next;
        			icalrecur_iterator* ritr;
				time_t duration = (time_t) NULL;
				struct icaldurationtype ical_dur;
				int i = 0;

				debug_print("dumping recurring events from main event %s\n", d);
        			recur = icalrecurrencetype_from_string(event->recur);
				dtstart = icaltime_from_string(event->dtstart);

				duration = icaltime_as_timet(icaltime_from_string(event->dtend))
							    - icaltime_as_timet(icaltime_from_string(event->dtstart));

				ical_dur = icaldurationtype_from_int(duration);

        			ritr = icalrecur_iterator_new(recur, dtstart);

				next = icalrecur_iterator_next(ritr); /* skip first one */
				if (!icaltime_is_null_time(next))
					next = icalrecur_iterator_next(ritr);
				debug_print("next time is %snull\n", icaltime_is_null_time(next)?"":"not ");
        			while (!icaltime_is_null_time(next) && i < 100) {
					const gchar *new_start = NULL, *new_end = NULL;
					VCalEvent *nevent = NULL;
					gchar *uid = g_strdup_printf("%s-%d", event->uid, i);
					new_start = icaltime_as_ical_string(next);
					new_end = icaltime_as_ical_string(
							icaltime_add(next, ical_dur));
					debug_print("adding with start/end %s:%s\n", new_start, new_end);
					nevent = vcal_manager_new_event(uid, event->organizer, event->orgname, 
								event->location, event->summary, event->description, 
								new_start, new_end, NULL, 
								event->tzid, event->url, event->method, 
								event->sequence, event->type);
					g_free(uid);
					vcal_manager_copy_attendees(event, nevent);
					nevent->rec_occurrence = TRUE;
					vcal_manager_save_event(nevent, FALSE);
					account = vcal_manager_get_account_from_event(event);
					status =
						account ? vcal_manager_get_reply_for_attendee(event, account->address): ICAL_PARTSTAT_NEEDSACTION;
					if (status == ICAL_PARTSTAT_ACCEPTED
					||  status == ICAL_PARTSTAT_TENTATIVE) {
						events = g_slist_prepend(events, nevent);
					} else {
						vcal_manager_free_event(nevent);
					}
					next = icalrecur_iterator_next(ritr);
					debug_print("next time is %snull\n", icaltime_is_null_time(next)?"":"not ");
					i++;
				}
				icalrecur_iterator_free(ritr);
			}
		} else {
			vcal_manager_free_event(event);
		}
	}
	g_dir_close(dp);
	return g_slist_reverse(events);
}

static gint vcal_get_num_list(Folder *folder, FolderItem *item,
				 MsgNumberList ** list, gboolean *old_uids_valid)
{
	int n_msg = 1;
	gint past_msg = -1, today_msg = -1, tomorrow_msg = -1, 
		thisweek_msg = -1, later_msg = -1;
	GSList *events = NULL, *cur;
	START_TIMING("");
	g_return_val_if_fail (*list == NULL, 0); /* we expect a NULL list */

	debug_print(" num for %s\n", ((VCalFolderItem *)item)->uri ? ((VCalFolderItem *)item)->uri:"(null)");
	
	*old_uids_valid = FALSE;
	
	if (((VCalFolderItem *)item)->uri) 
		return feed_fetch(item, list, old_uids_valid);
	
	events = vcal_get_events_list(item);
	
	debug_print("get_num_list\n");

	if (hash_uids != NULL)
		g_hash_table_destroy(hash_uids);
		
	hash_uids = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					  NULL, g_free);
	
	for (cur = events; cur; cur = cur->next) {
		VCalEvent *event = (VCalEvent *)cur->data;

		if (!event)
			continue;
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(n_msg), g_strdup(event->uid));
		
		if (event->rec_occurrence) {
			vcal_manager_free_event(event);
			continue;
		}

		if (event->method != ICAL_METHOD_CANCEL) {
			EventTime days;
			VCAL_FOLDER_ADD_EVENT(event);
		}
		if (event)
			vcal_manager_free_event(event);


	}

	if (today_msg == -1) { 
		*list = g_slist_prepend(*list, GINT_TO_POINTER(n_msg)); 
		today_msg = n_msg++; 
		g_hash_table_insert(hash_uids, GINT_TO_POINTER(today_msg), g_strdup(EVENT_TODAY_ID)); 
	}

	g_slist_free(events);
	vcal_folder_export(folder);

	vcal_set_mtime(folder, item);
	
	*list = g_slist_reverse(*list);
	END_TIMING();
	return g_slist_length(*list);
}

static MsgInfo *vcal_parse_msg(const gchar *file, FolderItem *item, int num)
{
	MsgInfo *msginfo = NULL;
	MsgFlags flags;

	debug_print("parse_msg\n");
	
	flags.perm_flags = 0;
	flags.tmp_flags = 0;
	msginfo = procheader_parse_file(file, flags, TRUE, TRUE);
	
	msginfo->msgnum = num;
	msginfo->folder = item;
	return msginfo;
}

static MsgInfo *vcal_get_msginfo(Folder * folder,
				    FolderItem * item, gint num)
{
	MsgInfo *msginfo = NULL;
	gchar *file;

	debug_print("get_msginfo\n");
	
	g_return_val_if_fail(item != NULL, NULL);
	g_return_val_if_fail(num > 0, NULL);

	file = vcal_fetch_msg(folder, item, num);

	if (!file) {
		return NULL;
	}

	msginfo = vcal_parse_msg(file, item, num);

	if (msginfo) {
		msginfo->flags.perm_flags = 0;
		msginfo->flags.tmp_flags = 0;

		vcal_change_flags(NULL, NULL, msginfo, 0);

		debug_print("  adding %d\n", num);
	}
	g_unlink(file);
	g_free(file);

	debug_print("  got msginfo %p\n", msginfo);

	return msginfo;
}

static gchar *feed_fetch_item(FolderItem * fitem, gint num)
{
	gchar *filename = NULL;
	VCalFolderItem *item = (VCalFolderItem *)fitem;
	GSList *ncur, *ecur;
	int i = 1;
	IcalFeedData *data = NULL;

	if (!item->numlist) {
		folder_item_scan_full(fitem, FALSE);
	}	
	if (!item->numlist) {
		debug_print("numlist null\n");
		return NULL;
	}

	ncur = item->numlist;
	ecur = item->evtlist;
	
	while (i < num) {
		if (!ncur || !ecur) {
			debug_print("list short end (%d to %d) %d,%d\n", i, num, ncur!=NULL, ecur!=NULL);
			return NULL;
		}
		ncur = ncur->next;
		ecur = ecur->next;
		i++;
	}
	
	data = (IcalFeedData *)ecur->data;
	
	if (!data) {
		return NULL;
	}

	if (data->event)
		filename = vcal_manager_icalevent_dump(data->event, fitem->name, NULL);
	else if (data->pseudoevent_id) {
		filename = vcal_manager_dateevent_dump(data->pseudoevent_id, fitem);
		created_files = g_slist_prepend(created_files, g_strdup(filename));
	} else {
		debug_print("no event\n");
		return NULL;
	}

	debug_print("feed item dump to %s\n", filename);
	return filename;
}

static gchar *vcal_fetch_msg(Folder * folder, FolderItem * item,
				gint num)
{
	gchar *filename = NULL;
	const gchar *uid = NULL;

	debug_print(" fetch for %s %d\n", (((VCalFolderItem *)item)->uri ? ((VCalFolderItem *)item)->uri:"(null)"), num);
	if (((VCalFolderItem *)item)->uri) 
		return feed_fetch_item(item, num);

	if (!uid) {
		if (!hash_uids)
			folder_item_scan_full(item, FALSE);
		uid = g_hash_table_lookup(hash_uids, GINT_TO_POINTER(num));
	}
	if (uid && 
	    (!strcmp(uid, EVENT_PAST_ID) ||
	     !strcmp(uid, EVENT_TODAY_ID) ||
	     !strcmp(uid, EVENT_TOMORROW_ID) ||
	     !strcmp(uid, EVENT_THISWEEK_ID) ||
	     !strcmp(uid, EVENT_LATER_ID))) {
		filename = vcal_manager_dateevent_dump(uid, item);
	} else if (uid) {
		VCalEvent *event = NULL;
		event = vcal_manager_load_event(uid);
		if (event)
			filename = vcal_manager_event_dump(event, FALSE, TRUE, NULL, FALSE);

		if (filename) {
			created_files = g_slist_prepend(created_files, g_strdup(filename));
		}

		vcal_manager_free_event(event);
	} 
		
	return filename;
}

static gint vcal_add_msg(Folder *folder, FolderItem *_dest, const gchar *file, MsgFlags *flags)
{
	gchar *contents = file_read_to_str(file);
	if (contents) {
		vcal_add_event(contents);		
	}
	g_free(contents);
	return 0;
}

static void vcal_remove_event (Folder *folder, MsgInfo *msginfo);

static gint vcal_remove_msg(Folder *folder, FolderItem *_item, gint num)
{
	MsgInfo *msginfo = folder_item_get_msginfo(_item, num);

	if (!msginfo)
		return 0;

	if (_item == folder->inbox)
		vcal_remove_event(folder, msginfo);

	procmsg_msginfo_free(&msginfo);
	return 0;
}

static FolderItem *vcal_create_folder(Folder * folder,
					 FolderItem * parent,
					 const gchar * name)
{
	gchar *path = NULL;
	FolderItem *newitem = NULL;
	debug_print("creating new vcal folder\n");

	path = g_strconcat((parent->path != NULL) ? parent->path : "", ".", name, NULL);
	newitem = folder_item_new(folder, name, path);
	folder_item_append(parent, newitem);
	g_free(path);

	return newitem;
}

static gint vcal_create_tree(Folder *folder)
{
	FolderItem *rootitem, *inboxitem;
	GNode *rootnode, *inboxnode;

	if (!folder->node) {
		rootitem = folder_item_new(folder, folder->name, NULL);
		rootitem->folder = folder;
		rootnode = g_node_new(rootitem);
		folder->node = rootnode;
		rootitem->node = rootnode;
	} else {
		rootitem = FOLDER_ITEM(folder->node->data);
		rootnode = folder->node;
	}

	/* Add inbox folder */
	if (!folder->inbox) {
		inboxitem = folder_item_new(folder, _("Meetings"), ".meetings");
		inboxitem->folder = folder;
		inboxitem->stype = F_INBOX;
		inboxnode = g_node_new(inboxitem);
		inboxitem->node = inboxnode;
		folder->inbox = inboxitem;
		g_node_append(rootnode, inboxnode);	
	} else {
		g_free(folder->inbox->path);
		folder->inbox->path = g_strdup(".meetings");
	}

	debug_print("created new vcal tree\n");
	return 0;
}

static gint vcal_remove_folder(Folder *folder, FolderItem *fitem)
{
	VCalFolderItem *item = (VCalFolderItem *)fitem;
	if (!item->uri)
		return -1;
	else {
		if (item->feed)
			g_free(item->feed);
		if (item->uri)
			g_free(item->uri);
		item->feed = NULL;
		item->uri = NULL;
		folder_item_remove(fitem);
		return 0;
	}
}

static gboolean vcal_scan_required(Folder *folder, FolderItem *item)
{
	GStatBuf s;
	VCalFolderItem *vitem = (VCalFolderItem *)item;

	g_return_val_if_fail(item != NULL, FALSE);

	if (vitem->uri) {
		return TRUE;
	} else if (g_stat(vcal_manager_get_event_path(), &s) < 0) {
		return TRUE;
	} else if ((s.st_mtime > item->mtime) &&
		(s.st_mtime - 3600 != item->mtime)) {
		return TRUE;
	}
	return FALSE;
}

static gint vcal_folder_lock_count = 0;

static void vcal_set_mtime(Folder *folder, FolderItem *item)
{
	GStatBuf s;
	gchar *path = folder_item_get_path(item);

	if (folder->inbox != item)
		return;

	g_return_if_fail(path != NULL);

	if (g_stat(path, &s) < 0) {
		FILE_OP_ERROR(path, "stat");
		g_free(path);
		return;
	}

	item->mtime = s.st_mtime;
	debug_print("VCAL: forced mtime of %s to %"CM_TIME_FORMAT"\n",
			item->name?item->name:"(null)", item->mtime);
	g_free(path);
}

void vcal_folder_export(Folder *folder)
{	
	FolderItem *item = folder?folder->inbox:NULL;
	gboolean need_scan = folder?vcal_scan_required(folder, item):TRUE;
	gchar *export_pass = NULL;
	gchar *export_freebusy_pass = NULL;

	if (vcal_folder_lock_count) /* blocked */
		return;
	vcal_folder_lock_count++;
	
	export_pass = vcal_passwd_get("export");
	export_freebusy_pass = vcal_passwd_get("export_freebusy");

	if (vcal_meeting_export_calendar(vcalprefs.export_path, 
			vcalprefs.export_user, 
			export_pass,
			TRUE)) {
		debug_print("exporting calendar\n");
		if (vcalprefs.export_enable &&
		    vcalprefs.export_command &&
		    strlen(vcalprefs.export_command))
			execute_command_line(
				vcalprefs.export_command, TRUE, NULL);
	}
	if (export_pass != NULL) {
		memset(export_pass, 0, strlen(export_pass));
	}
	g_free(export_pass);
	if (vcal_meeting_export_freebusy(vcalprefs.export_freebusy_path,
			vcalprefs.export_freebusy_user,
			export_freebusy_pass)) {
		debug_print("exporting freebusy\n");
		if (vcalprefs.export_freebusy_enable &&
		    vcalprefs.export_freebusy_command &&
		    strlen(vcalprefs.export_freebusy_command))
			execute_command_line(
				vcalprefs.export_freebusy_command, TRUE, NULL);
	}
	if (export_freebusy_pass != NULL) {
		memset(export_freebusy_pass, 0, strlen(export_freebusy_pass));
	}
	g_free(export_freebusy_pass);
	vcal_folder_lock_count--;
	if (!need_scan && folder) {
		vcal_set_mtime(folder, folder->inbox);
	}
}

static void vcal_remove_event (Folder *folder, MsgInfo *msginfo)
{
	const gchar *uid = msginfo->msgid;
	VCalFolderItem *item = (VCalFolderItem *)msginfo->folder;

	if (uid) {
		gchar *file = vcal_manager_get_event_file(uid);
		g_unlink(file);
		g_free(file);
	}
	
	if (!item || !item->batching)
		vcal_folder_export(folder);
	else if (item) {
		item->dirty = TRUE;
	}
}

static void vcal_change_flags(Folder *folder, FolderItem *_item, MsgInfo *msginfo, MsgPermFlags newflags)
{
	EventTime date;

	if (newflags & MSG_DELETED) {
		/* delete the stuff */
		msginfo->flags.perm_flags |= MSG_DELETED;
		vcal_remove_event(folder, msginfo);
		return;
	}

	/* accept the rest */
	msginfo->flags.perm_flags = newflags;

	/* but not color */
	msginfo->flags.perm_flags &= ~MSG_CLABEL_FLAG_MASK;
	
	date = event_to_today(NULL, msginfo->date_t);
	switch(date) {
	case EVENT_PAST:
		break;
	case EVENT_TODAY:
		msginfo->flags.perm_flags |= MSG_COLORLABEL_TO_FLAGS(2); /* Red */
		break;
	case EVENT_TOMORROW:
		break;
	case EVENT_THISWEEK:
		break;
	case EVENT_LATER:
		break;
	}
	if (msginfo->msgid) {
		if (!strcmp(msginfo->msgid, EVENT_TODAY_ID) ||
		    !strcmp(msginfo->msgid, EVENT_TOMORROW_ID))
		msginfo->flags.perm_flags |= MSG_MARKED;
	}
}

void vcal_folder_gtk_init(void)
{
	vcal_fill_popup_menu_labels();

	folderview_register_popup(&vcal_popup);
}

void vcal_folder_gtk_done(void)
{
	GSList *cur = created_files;
	while (cur) {
		gchar *file = (gchar *)cur->data;
		cur = cur->next;
		if (!file)			
			continue;
		debug_print("removing %s\n", file);
		if (g_unlink(file) < 0)
                        FILE_OP_ERROR(file, "g_unlink");
		g_free(file);
	}
	g_slist_free(created_files);
	folderview_unregister_popup(&vcal_popup);
}

static void add_menuitems(GtkUIManager *ui_manager, FolderItem *item)
{
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewMeeting", "FolderViewPopup/NewMeeting", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "ExportCal", "FolderViewPopup/ExportCal", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVcal1", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SubscribeCal", "FolderViewPopup/SubscribeCal", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "UnsubscribeCal", "FolderViewPopup/UnsubscribeCal", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVcal2", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RenameFolder", "FolderViewPopup/RenameFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVcal3", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "CheckSubs", "FolderViewPopup/CheckSubs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVcal4", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "ListView", "FolderViewPopup/ListView", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "WeekView", "FolderViewPopup/WeekView", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "MonthView", "FolderViewPopup/MonthView", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVcal5", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
}

static gboolean setting_sensitivity = FALSE;
static void set_sensitivity(GtkUIManager *ui_manager, FolderItem *fitem)
{
	VCalFolderItem *item = (VCalFolderItem *)fitem;

#define SET_SENS(name, sens) \
	cm_menu_set_sensitive_full(ui_manager, "Popup/"name, sens)

	setting_sensitivity = TRUE;

	cm_toggle_menu_set_active_full(ui_manager, "Popup/FolderViewPopup/ListView", (item->use_cal_view == 0));
	cm_toggle_menu_set_active_full(ui_manager, "Popup/FolderViewPopup/WeekView", (item->use_cal_view == 1));
	cm_toggle_menu_set_active_full(ui_manager, "Popup/FolderViewPopup/MonthView", (item->use_cal_view == 2));
	SET_SENS("FolderViewPopup/NewMeeting",   item->uri == NULL);
	SET_SENS("FolderViewPopup/ExportCal", TRUE);
	SET_SENS("FolderViewPopup/SubscribeCal", item->uri == NULL);
	SET_SENS("FolderViewPopup/UnsubscribeCal", item->uri != NULL);
	SET_SENS("FolderViewPopup/RenameFolder", folder_item_parent(fitem) != NULL);
	SET_SENS("FolderViewPopup/CheckSubs", TRUE);
	SET_SENS("FolderViewPopup/ListView", folder_item_parent(fitem) != NULL);
	SET_SENS("FolderViewPopup/WeekView", folder_item_parent(fitem) != NULL);
	SET_SENS("FolderViewPopup/MonthView", folder_item_parent(fitem) != NULL);
	setting_sensitivity = FALSE;
#undef SET_SENS
}

static void new_meeting_cb(GtkAction *action, gpointer data)
{
	debug_print("new_meeting_cb\n");
	vcal_meeting_create(NULL);
}

GSList * vcal_folder_get_waiting_events(void)
{
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	return vcal_get_events_list(folder->inbox);
}

typedef struct _get_webcal_data {
	GSList *list;
	FolderItem *item;
} GetWebcalData;

static gboolean get_webcal_events_func(GNode *node, gpointer user_data)
{
	FolderItem *item = node->data;
	GetWebcalData *data = user_data;
	gboolean dummy = FALSE;
	GSList *list = NULL, *cur = NULL;

	if (data->item && data->item != item)
		return FALSE;

	feed_fetch(item, &list, &dummy);

	g_slist_free(list);

	for (cur = ((VCalFolderItem *)item)->evtlist; cur; cur = cur->next) {
		IcalFeedData *fdata = (IcalFeedData *)cur->data;
		if (fdata->event)
			data->list = g_slist_prepend(data->list, fdata->event);
	}
	return FALSE;
}

GSList * vcal_folder_get_webcal_events(void)
{
	GetWebcalData *data = g_new0(GetWebcalData, 1);
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	GSList *list = NULL;
	data->item = NULL;
	g_node_traverse(folder->node, G_PRE_ORDER,
			G_TRAVERSE_ALL, -1, get_webcal_events_func, data);

	list = data->list;
	g_free(data);

	return g_slist_reverse(list);
}

static gboolean vcal_free_data_func(GNode *node, gpointer user_data)
{
	VCalFolderItem *item = node->data;

	if (item->cal) {
		icalcomponent_free(item->cal);
		item->cal = NULL;
	}
	if (item->numlist) {
		g_slist_free(item->numlist);
		item->numlist = NULL;
	}

	if (item->evtlist) {
		slist_free_icalfeeddata(item->evtlist);
		g_slist_free(item->evtlist);
		item->evtlist = NULL;
	}

	return FALSE;
}

void vcal_folder_free_data(void)
{
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());

	g_node_traverse(folder->node, G_PRE_ORDER,
			G_TRAVERSE_ALL, -1, vcal_free_data_func, NULL);
}

GSList * vcal_folder_get_webcal_events_for_folder(FolderItem *item)
{
	GetWebcalData *data = g_new0(GetWebcalData, 1);
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	GSList *list = NULL;
	data->item = item;
	g_node_traverse(folder->node, G_PRE_ORDER,
			G_TRAVERSE_ALL, -1, get_webcal_events_func, data);

	list = data->list;
	g_free(data);

	return g_slist_reverse(list);
}

gchar* get_item_event_list_for_date(FolderItem *item, EventTime date)
{
	GSList *strs = NULL;
	GSList *cur;
	gchar *result = NULL;
	gchar *datestr = NULL;

	if (((VCalFolderItem *)item)->uri) {
		for (cur = ((VCalFolderItem *)item)->evtlist; cur; cur = cur->next) {
			IcalFeedData *fdata = (IcalFeedData *)cur->data;
			icalproperty *prop;
			struct icaltimetype itt;
			gchar *summary = NULL;
			EventTime days;
			if (!fdata->event)
				continue;
			prop = icalcomponent_get_first_property((icalcomponent *)fdata->event, ICAL_DTSTART_PROPERTY);
			
			if (!prop)
				continue;
			itt = icalproperty_get_dtstart(prop);
			days = event_to_today(NULL, icaltime_as_timet(itt));
			if (days != date)
				continue;
			prop = icalcomponent_get_first_property((icalcomponent *)fdata->event, ICAL_SUMMARY_PROPERTY);
			summary = g_strdup(icalproperty_get_summary(prop));
			if (summary) {
				if (!g_utf8_validate(summary, -1, NULL))
					summary = conv_codeset_strdup(summary, 
						conv_get_locale_charset_str(), CS_UTF_8);
			} else
				summary = g_strdup(_("[no summary]"));

			strs = g_slist_prepend(strs, summary);
		}
	} else {
		GSList *evtlist = vcal_folder_get_waiting_events();
		for (cur = evtlist; cur; cur = cur->next) {
			VCalEvent *event = (VCalEvent *)cur->data;
			EventTime days;
			days = event_to_today(event, 0);
			gchar *summary = NULL;
			if (days == date) {
				summary = g_strdup(event->summary);
				strs = g_slist_prepend(strs, summary);
			}
			vcal_manager_free_event(event);
		}
	}
	
	switch(date) {
	case EVENT_PAST:
		datestr=_("in the past");
		break;
	case EVENT_TODAY:
		datestr=_("today");
		break;
	case EVENT_TOMORROW:
		datestr=_("tomorrow");
		break;
	case EVENT_THISWEEK:
		datestr=_("this week");
		break;
	case EVENT_LATER:
		datestr=_("later");
		break;
	}
	
	result = g_strdup_printf(_("\nThese are the events planned %s:\n"),
			datestr?datestr:"never");
	
	strs = g_slist_reverse(strs);
	for (cur = strs; cur; cur = cur->next) {
		int e_len = strlen(result);
		int n_len = strlen((gchar *)cur->data);
		if (e_len) {
			result = g_realloc(result, e_len+n_len+4);
			*(result+e_len) = '\n';
			strcpy(result+e_len+1, "- ");
			strcpy(result+e_len+3, (gchar *)cur->data);
		} else {
			result = g_realloc(result, e_len+n_len+3);
			strcpy(result+e_len, "- ");
			strcpy(result+e_len+2, (gchar *)cur->data);
		}
	}
	slist_free_strings_full(strs);
	return result;
}

static void export_cal_cb(GtkAction *action, gpointer data)
{
	vcal_meeting_export_calendar(NULL, NULL, NULL, FALSE);
}

struct CBuf {
	gchar *str;
};

static size_t curl_recv(void *buf, size_t size, size_t nmemb, void *stream)
{
	struct CBuf *buffer = (struct CBuf *)stream;
	gchar *tmp = NULL;
	gchar *tmpbuf = g_malloc0(size*nmemb + 1);

	g_return_val_if_fail(tmpbuf != NULL, 0);

	memcpy(tmpbuf, buf, size*nmemb);

	if (buffer->str) {
		/* If the buffer already has contents, append the new data. */
		tmp = g_strconcat(buffer->str, tmpbuf, NULL);
		g_free(tmpbuf);
		g_free(buffer->str);
		buffer->str = tmp;
	} else {
		buffer->str = tmpbuf;
	}

	return size*nmemb;
}

void *url_read_thread(void *data)
{
	thread_data *td = (thread_data *)data;
	CURLcode res;
	CURL *curl_ctx = NULL;
	long response_code;
	struct CBuf buffer = { NULL };
	gchar *t_url = (gchar *)td->url;

	while (*t_url == ' ')
		t_url++;
	if (strchr(t_url, ' '))
		*(strchr(t_url, ' ')) = '\0';

#ifdef USE_PTHREAD
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
	
	curl_ctx = curl_easy_init();
	
	curl_easy_setopt(curl_ctx, CURLOPT_URL, t_url);
	curl_easy_setopt(curl_ctx, CURLOPT_WRITEFUNCTION, curl_recv);
	curl_easy_setopt(curl_ctx, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl_ctx, CURLOPT_TIMEOUT, prefs_common_get_prefs()->io_timeout_secs);
	curl_easy_setopt(curl_ctx, CURLOPT_NOSIGNAL, 1);
#ifdef G_OS_WIN32
	curl_easy_setopt(curl_ctx, CURLOPT_CAINFO, claws_ssl_get_cert_file());
#endif
#if LIBCURL_VERSION_NUM >= 0x070a00
	if(vcalprefs.ssl_verify_peer == FALSE) {
		curl_easy_setopt(curl_ctx, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_ctx, CURLOPT_SSL_VERIFYHOST, 0);
	}
#endif
	curl_easy_setopt(curl_ctx, CURLOPT_USERAGENT, 
		"Claws Mail vCalendar plugin "
		"(" PLUGINS_URI ")");
	curl_easy_setopt(curl_ctx, CURLOPT_FOLLOWLOCATION, 1);
	res = curl_easy_perform(curl_ctx);
	
	if (res != 0) {
		debug_print("res %d %s\n", res, curl_easy_strerror(res));
		td->error = g_strdup(curl_easy_strerror(res));
		
		if(res == CURLE_OPERATION_TIMEOUTED)
			log_error(LOG_PROTOCOL, _("Timeout (%d seconds) connecting to %s\n"),
				prefs_common_get_prefs()->io_timeout_secs, t_url);
	}

	curl_easy_getinfo(curl_ctx, CURLINFO_RESPONSE_CODE, &response_code);
	if( response_code >= 400 && response_code < 500 ) {
		debug_print("VCalendar: got %ld\n", response_code);
		switch(response_code) {
			case 401: 
				td->error = g_strdup(_("401 (Authorisation required)"));
				break;
			case 403:
				td->error = g_strdup(_("403 (Unauthorised)"));
				break;
			case 404:
				td->error = g_strdup(_("404 (Not found)"));
				break;
			default:
				td->error = g_strdup_printf(_("Error %ld"), response_code);
				break;
		}
	}
	curl_easy_cleanup(curl_ctx);
	if (buffer.str) {
		td->result = g_strdup(buffer.str);
		g_free(buffer.str);
	}

	td->done = TRUE; /* let the caller thread join() */
	return GINT_TO_POINTER(0);
}

gchar *vcal_curl_read(const char *url, const gchar *label, gboolean verbose, 
	void (*callback)(const gchar *url, gchar *data, gboolean verbose, gchar *error))
{
	gchar *result;
	thread_data *td;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif
	void *res;
	gchar *error = NULL;
	result = NULL;
	td = g_new0(thread_data, 1);
	res = NULL;

	td->url  = url;
	td->result  = NULL;
	td->done = FALSE;

	STATUSBAR_PUSH(mainwindow_get_mainwindow(), label);

#ifdef USE_PTHREAD
	if (pthread_create(&pt, NULL, url_read_thread, td) != 0) {
		url_read_thread(td);	
	}
	while (!td->done)  {
 		claws_do_idle();
	}
 
	pthread_join(pt, &res);
#else
	url_read_thread(td);
#endif
	
	result = td->result;
	error = td->error;
	g_free(td);
	
	STATUSBAR_POP(mainwindow_get_mainwindow());

	if (callback) {
		callback(url, result, verbose, error);
		return NULL;
	} else {
		if (error)
			g_free(error);
		return result;
	}
}

gboolean vcal_curl_put(gchar *url, FILE *fp, gint filesize, const gchar *user, const gchar *pass)
{
	gboolean res = TRUE;
	CURL *curl_ctx = curl_easy_init();
	long response_code = 0;
	gchar *t_url = url;
	gchar *userpwd = NULL;

	struct curl_slist * headers = curl_slist_append(NULL, 
		"Content-Type: text/calendar; charset=\"utf-8\"" );

	while (*t_url == ' ')
		t_url++;
	if (strchr(t_url, ' '))
		*(strchr(t_url, ' ')) = '\0';

	if (user && pass && *user && *pass) {
		userpwd = g_strdup_printf("%s:%s",user,pass);
		curl_easy_setopt(curl_ctx, CURLOPT_USERPWD, userpwd);
	}
	curl_easy_setopt(curl_ctx, CURLOPT_URL, t_url);
	curl_easy_setopt(curl_ctx, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl_ctx, CURLOPT_READFUNCTION, NULL);
	curl_easy_setopt(curl_ctx, CURLOPT_READDATA, fp);
	curl_easy_setopt(curl_ctx, CURLOPT_HTTPHEADER, headers);
#ifdef G_OS_WIN32
	curl_easy_setopt(curl_ctx, CURLOPT_CAINFO, claws_ssl_get_cert_file());
#endif
#if LIBCURL_VERSION_NUM >= 0x070a00
	if(vcalprefs.ssl_verify_peer == FALSE) {
		curl_easy_setopt(curl_ctx, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl_ctx, CURLOPT_SSL_VERIFYHOST, 0);
	}
#endif
	curl_easy_setopt(curl_ctx, CURLOPT_USERAGENT, 
		"Claws Mail vCalendar plugin "
		"(" PLUGINS_URI ")");
	curl_easy_setopt(curl_ctx, CURLOPT_INFILESIZE, filesize);
	res = curl_easy_perform(curl_ctx);
	g_free(userpwd);

	if (res != 0) {
		debug_print("res %d %s\n", res, curl_easy_strerror(res));
	} else {
		res = TRUE;
	}

	curl_easy_getinfo(curl_ctx, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code < 200 || response_code >= 300) {
		g_warning("can't export calendar, got code %ld", response_code);
		res = FALSE;
	}
	curl_easy_cleanup(curl_ctx);
	curl_slist_free_all(headers);
	return res;
}

static gboolean folder_item_find_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;
	gpointer *d = data;
	const gchar *uri = d[0];

	if (!uri || !((VCalFolderItem *)item)->uri
	||  strcmp(uri, ((VCalFolderItem *)item)->uri))
		return FALSE;

	d[1] = item;

	return TRUE;
}

static FolderItem *get_folder_item_for_uri(const gchar *uri)
{
	Folder *root = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	gpointer d[2];
	
	if (root == NULL)
		return NULL;
	
	d[0] = (gpointer)uri;
	d[1] = NULL;
	g_node_traverse(root->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			folder_item_find_func, d);
	return d[1];
}

static gchar *feed_get_title(const gchar *str)
{
	gchar *title = NULL;
	if (strstr(str, "X-WR-CALNAME:")) {
		title = g_strdup(strstr(str, "X-WR-CALNAME:")+strlen("X-WR-CALNAME:"));
	} else if (strstr(str, "X-WR-CALDESC:")) {
		title = g_strdup(strstr(str, "X-WR-CALDESC:")+strlen("X-WR-CALDESC:"));
	}
	return strcrlftrunc(title);
}

static void update_subscription_finish(const gchar *uri, gchar *feed, gboolean verbose, gchar *error)
{
	Folder *root = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	FolderItem *item = NULL;
	icalcomponent *cal = NULL;
	
	if (root == NULL) {
		g_warning("can't get root folder");
		g_free(feed);
		if (error)
			g_free(error);
		return;
	}

	if (feed == NULL) {
		gchar *err_msg = _("Could not retrieve the Webcal URL:\n%s:\n\n%s");

		if (verbose && manual_update) {
			gchar *tmp = g_strdup(uri);
			if (strlen(uri) > 61) {
				tmp[55]='[';
				tmp[56]='.';
				tmp[57]='.';
				tmp[58]='.';
				tmp[59]=']';
				tmp[60]='\0';
			} 
			alertpanel_error(err_msg, tmp, error ? error:_("Unknown error"));
			g_free(tmp);
		} else  {
			gchar *msg = g_strdup_printf("%s\n", err_msg);
			log_error(LOG_PROTOCOL, msg, uri, error ? error:_("Unknown error"));
			g_free(msg);
		}
		main_window_cursor_normal(mainwindow_get_mainwindow());
		g_free(feed);
		if (error)
			g_free(error);
		return;
	}

	gchar *tmp = feed;
	while (*tmp && isspace((unsigned char)*tmp))
		tmp++;

	if (strncmp(tmp, "BEGIN:VCALENDAR", strlen("BEGIN:VCALENDAR"))) {
		gchar *err_msg = _("This URL does not look like a Webcal URL:\n%s\n%s");

		if (verbose && manual_update) {
			alertpanel_error(err_msg, uri, error ? error:_("Unknown error"));
		} else  {
			gchar *msg = g_strdup_printf("%s\n", err_msg);
			log_error(LOG_PROTOCOL, msg, uri, error ? error:_("Unknown error"));
			g_free(msg);
		}
		g_free(feed);
		main_window_cursor_normal(mainwindow_get_mainwindow());
		if (error)
			g_free(error);
		return;
	}
	
	if (error)
		g_free(error);
	item = get_folder_item_for_uri(uri);
	if (item == NULL) {
		gchar *title = feed_get_title(feed);
		if (title == NULL) {
			if (strstr(uri, "://"))
				title = g_path_get_basename(strstr(uri,"://")+3);
			else
				title = g_strdup(uri);
			subst_for_filename(title);
		}
		item = folder_create_folder(root->node->data, title);
		if (!item) {
			if (verbose && manual_update) {
				alertpanel_error(_("Could not create directory %s"),
					title);
			} else  {
				log_error(LOG_PROTOCOL, _("Could not create directory %s"),
					title);
			}
			g_free(feed);
			g_free(title);
			main_window_cursor_normal(mainwindow_get_mainwindow());
			return;
		}
		debug_print("item done %s\n", title);
		((VCalFolderItem *)item)->uri = g_strdup(uri);
		((VCalFolderItem *)item)->feed = feed;
		g_free(title);
	} else {
		if (((VCalFolderItem *)item)->feed)
			g_free(((VCalFolderItem *)item)->feed);

		((VCalFolderItem *)item)->feed = feed;
		/* if title differs, update it */
	}
	cal = icalparser_parse_string(feed);

	convert_to_utc(cal);
	
	if (((VCalFolderItem *)item)->cal)
		icalcomponent_free(((VCalFolderItem *)item)->cal);

	((VCalFolderItem *)item)->cal = cal;
	
	main_window_cursor_normal(mainwindow_get_mainwindow());
	((VCalFolderItem *)item)->last_fetch = time(NULL);
}

static void update_subscription(const gchar *uri, gboolean verbose)
{
	FolderItem *item = get_folder_item_for_uri(uri);
	gchar *label;

	if (prefs_common_get_prefs()->work_offline) {
		if (!verbose || 
		!inc_offline_should_override(TRUE,
		   _("Claws Mail needs network access in order "
		     "to update the Webcal feed.")))
			return;
	}
	if (item) {
		if (time(NULL) - ((VCalFolderItem *)(item))->last_fetch < 60 && 
		    ((VCalFolderItem *)(item))->cal)
			return;
	}
	main_window_cursor_wait(mainwindow_get_mainwindow());

	label = g_strdup_printf(_("Fetching calendar for %s..."), 
			item && item->name ? item->name : _("new subscription"));
	vcal_curl_read(uri, label, verbose, update_subscription_finish);
	g_free(label);
}

static void check_subs_cb(GtkAction *action, gpointer data)
{
	Folder *root = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());

	if (prefs_common_get_prefs()->work_offline && 
	    !inc_offline_should_override(TRUE,
		     _("Claws Mail needs network access in order "
		     "to update the subscription.")))
		return;

	folderview_check_new(root);
}

static void subscribe_cal_cb(GtkAction *action, gpointer data)
{
	gchar *uri = NULL;
	gchar *tmp = NULL;
	gchar *clip_text = NULL, *str = NULL;

    clip_text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));

    if (clip_text) {
        str = clip_text;
#if GLIB_CHECK_VERSION(2,66,0)
        GError *error = NULL;
        GUri *uri = NULL;

        /* skip any leading white-space */
        while (str && *str && g_ascii_isspace(*str))
            str++;
        uri = g_uri_parse(str, G_URI_FLAGS_PARSE_RELAXED, &error);
        if (error) {
            g_warning("could not parse clipboard text for URI: '%s'", error->message);
            g_error_free(error);
        }
        if (uri) {
            gchar* newstr = g_uri_to_string(uri);

            debug_print("URI: '%s' -> '%s'\n", str, newstr ? newstr : "N/A");
            if (newstr)
                g_free(newstr);
            g_uri_unref(uri);
        } else {
#else
        if (!is_uri_string(str)) {
#endif
            /* if no URL, ignore clipboard text */
            str = NULL;
        }
    }

	tmp = input_dialog(_("Subscribe to Webcal"), _("Enter the Webcal URL:"), str ? str : "");

	if (clip_text)
		g_free(clip_text);

	if (tmp == NULL)
		return;
	
	if (!strncmp(tmp, "http", 4)) {
		uri = tmp;
	} else if (!strncmp(tmp, "file://", 7)) {
		uri = tmp;
	} else if (!strncmp(tmp, "webcal", 6)) {
		uri = g_strconcat("http", tmp+6, NULL);
		g_free(tmp);
	} else {
		alertpanel_error(_("Could not parse the URL."));
		g_free(tmp);
		return;
	}
	debug_print("uri %s\n", uri);
	
	update_subscription(uri, TRUE);	
	folder_write_list();
	g_free(uri);
}

static void unsubscribe_cal_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item, *opened;
	gchar *message;
	AlertValue avalue;
	gchar *old_id;

	if (!folderview->selected) return;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);
	opened = folderview_get_opened_item(folderview);

	message = g_strdup_printf
		(_("Do you really want to unsubscribe?"));
	avalue = alertpanel_full(_("Delete subscription"), message,
		 		 NULL, _("_Cancel"), "edit-delete", _("_Delete"),
				 NULL, NULL, ALERTFOCUS_FIRST,
				 FALSE, NULL, ALERT_WARNING);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

	old_id = folder_item_get_identifier(item);

	vcal_item_closed(item);

	if (item == opened ||
			folder_is_child_of(item, opened)) {
		summary_clear_all(folderview->summaryview);
		folderview_close_opened(folderview, TRUE);
	}

	if (item->folder->klass->remove_folder(item->folder, item) < 0) {
		folder_item_scan(item);
		alertpanel_error(_("Can't remove the folder '%s'."), item->name);
		g_free(old_id);
		return;
	}

	folder_write_list();

	prefs_filtering_delete_path(old_id);
	g_free(old_id);
}

gboolean vcal_subscribe_uri(Folder *folder, const gchar *uri)
{
	gchar *tmp = NULL;
	if (folder->klass != vcal_folder_get_class())
		return FALSE;

	if (uri == NULL)
		return FALSE;

	if (!strncmp(uri, "webcal", 6)) {
		tmp = g_strconcat("http", uri+6, NULL);
	} else {
		return FALSE;
	}
	debug_print("uri %s\n", tmp);
	
	update_subscription(tmp, FALSE);
	folder_write_list();
	return TRUE;
}

static void rename_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for '%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message, name);
	g_free(message);
	g_free(name);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	if (folder_find_child_item_by_name(folder_item_parent(item), new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder '%s' already exists."), name);
		g_free(name);
		return;
	}

	if (folder_item_rename(item, new_folder) < 0) {
		alertpanel_error(_("The folder could not be renamed.\n"
				   "The new folder name is not allowed."));
		return;
	}

	folder_item_prefs_save_config_recursive(item);
	folder_write_list();
}

static void set_view_cb(GtkAction *gaction, GtkRadioAction *current, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	gint action = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	FolderItem *item = NULL, *oitem = NULL;

	if (!folderview->selected) return;
	if (setting_sensitivity) return;

	oitem = folderview_get_opened_item(folderview);
	item = folderview_get_selected_item(folderview);

	if (!item)
		return;

	if (((VCalFolderItem *)(item))->use_cal_view == action)
		return;
	debug_print("set view %d\n", action);
	if (oitem && item == oitem && oitem->folder->klass == vcal_folder_get_class())
		oitem->folder->klass->item_closed(oitem);
	((VCalFolderItem *)(item))->use_cal_view = action;
	if (((VCalFolderItem *)(item))->use_cal_view) {
		if (oitem && item == oitem && oitem->folder->klass == vcal_folder_get_class())
			oitem->folder->klass->item_opened(oitem);
	}
}

gchar *vcal_get_event_as_ical_str(VCalEvent *event)
{
	gchar *ical;
	icalcomponent *calendar = icalcomponent_vanew(
            ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
            icalproperty_new_prodid(
                 "-//Claws Mail//NONSGML Claws Mail Calendar//EN"),
	    icalproperty_new_calscale("GREGORIAN"),
            (void*)0);
	vcal_manager_event_dump(event, FALSE, FALSE, calendar, FALSE);
	ical = g_strdup(icalcomponent_as_ical_string(calendar));
	icalcomponent_free(calendar);
	
	return ical;
}

static gchar *get_name_from_property(icalproperty *p)
{
	gchar *tmp = NULL;
	
	if (p && icalproperty_get_parameter_as_string(p, "CN") != NULL)
		tmp = g_strdup(icalproperty_get_parameter_as_string(p, "CN"));

	return tmp;
}

static gchar *get_email_from_property(icalproperty *p)
{
	gchar *tmp = NULL;
	gchar *email = NULL;
	
	if (p)
		tmp = g_strdup(icalproperty_get_organizer(p));

	if (!tmp) 
		return NULL;

	if (!strncasecmp(tmp, "MAILTO:", strlen("MAILTO:")))
		email = g_strdup(tmp+strlen("MAILTO:"));
	else
		email = g_strdup(tmp);
	g_free(tmp);
	
	return email;
}

static void convert_to_utc(icalcomponent *calendar)
{
	icalcomponent *event;
	icaltimezone *tz, *tzutc = icaltimezone_get_utc_timezone();
	icalproperty *prop;
	icalparameter *tzid;

	cm_return_if_fail(calendar != NULL);

	for (
			event = icalcomponent_get_first_component(calendar,
				ICAL_VEVENT_COMPONENT);
			event != NULL;
			event = icalcomponent_get_next_component(calendar,
				ICAL_VEVENT_COMPONENT)) {

		/* DTSTART */
		if ((prop = icalcomponent_get_first_property(event, ICAL_DTSTART_PROPERTY)) != NULL
				&& (tzid = icalproperty_get_first_parameter(prop, ICAL_TZID_PARAMETER)) != NULL) {
			/* Event has its DTSTART with a timezone specification, let's convert
			 * to UTC and remove the TZID parameter. */

			tz = icalcomponent_get_timezone(calendar, icalparameter_get_iana_value(tzid));
			if (tz != NULL) {
				debug_print("Converting DTSTART to UTC.\n");
				icaltimetype t = icalproperty_get_dtstart(prop);
				icaltimezone_convert_time(&t, tz, tzutc);
				icalproperty_set_dtstart(prop, t);
				icalproperty_remove_parameter_by_ref(prop, tzid);
			}
		}

		/* DTEND */
		if ((prop = icalcomponent_get_first_property(event, ICAL_DTEND_PROPERTY)) != NULL
				&& (tzid = icalproperty_get_first_parameter(prop, ICAL_TZID_PARAMETER)) != NULL) {
			/* Event has its DTEND with a timezone specification, let's convert
			 * to UTC and remove the TZID parameter. */

			tz = icalcomponent_get_timezone(calendar, icalparameter_get_iana_value(tzid));
			if (tz != NULL) {
				debug_print("Converting DTEND to UTC.\n");
				icaltimetype t = icalproperty_get_dtend(prop);
				icaltimezone_convert_time(&t, tz, tzutc);
				icalproperty_set_dtend(prop, t);
				icalproperty_remove_parameter_by_ref(prop, tzid);
			}
		}
	}
}

#define GET_PROP(comp,prop,kind) {						\
	prop = NULL;								\
	if (!(prop = icalcomponent_get_first_property(comp, kind))) {		\
		prop = inner							\
			? icalcomponent_get_first_property(inner, kind)		\
			: NULL;							\
	}									\
}

#define GET_PROP_LIST(comp,list,kind) {						\
	list = NULL;								\
	if (!(prop = icalcomponent_get_first_property(comp, kind))) {		\
		prop = inner							\
			? icalcomponent_get_first_property(inner, kind)		\
			: NULL;							\
		if (prop) do {							\
			list = g_slist_prepend(list, prop);			\
		} while ((prop = icalcomponent_get_next_property(inner, kind)));\
	}else do {								\
		list = g_slist_prepend(list, prop);				\
	} while ((prop = icalcomponent_get_next_property(comp, kind)));		\
}

#define TO_UTF8(string) {							\
	if (string && !g_utf8_validate(string, -1, NULL)) {			\
		gchar *tmp = conv_codeset_strdup(string, 			\
				charset ? charset:conv_get_locale_charset_str(),\
				CS_UTF_8);					\
		g_free(string);							\
		string = tmp;							\
	}									\
}

VCalEvent *vcal_get_event_from_ical(const gchar *ical, const gchar *charset)
{
	VCalEvent *event = NULL;
	gchar *int_ical = g_strdup(ical);
	icalcomponent *comp = icalcomponent_new_from_string(int_ical);
	icalcomponent *inner = NULL;
	icalproperty *prop = NULL;
	GSList *list = NULL, *cur = NULL;
	gchar *uid = NULL;
	gchar *location = NULL;
	gchar *summary = NULL;
	gchar *dtstart = NULL;
	gchar *dtend = NULL;
	gchar *org_email = NULL, *org_name = NULL;
	gchar *description = NULL;
	gchar *url = NULL;
	gchar *tzid = NULL;
	gchar *recur = NULL;
	int sequence = 0;
	enum icalproperty_method method = ICAL_METHOD_REQUEST;
	enum icalcomponent_kind type = ICAL_VEVENT_COMPONENT;
	GSList *attendees = NULL;
	
	if (comp == NULL) {
		g_free(int_ical);
		return NULL;
	}

	if ((inner = icalcomponent_get_inner(comp)) != NULL)
	    type = icalcomponent_isa(inner);

	GET_PROP(comp, prop, ICAL_UID_PROPERTY);
	if (prop) {
		uid = g_strdup(icalproperty_get_uid(prop));
		TO_UTF8(uid);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_LOCATION_PROPERTY);
	if (prop) {
		location = g_strdup(icalproperty_get_location(prop));
		TO_UTF8(location);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_SUMMARY_PROPERTY);
	if (prop) {
		summary = g_strdup(icalproperty_get_summary(prop));
		TO_UTF8(summary);
		icalproperty_free(prop);
	}

	convert_to_utc(comp);

	GET_PROP(comp, prop, ICAL_DTSTART_PROPERTY);
	if (prop) {
		dtstart = g_strdup(icaltime_as_ical_string(icalproperty_get_dtstart(prop)));
		TO_UTF8(dtstart);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_DTEND_PROPERTY);
	if (prop) {
		dtend = g_strdup(icaltime_as_ical_string(icalproperty_get_dtend(prop)));
		TO_UTF8(dtend);
		icalproperty_free(prop);
	} else {
		GET_PROP(comp, prop, ICAL_DURATION_PROPERTY);
		if (prop) {
			struct icaldurationtype duration = icalproperty_get_duration(prop);
			struct icaltimetype itt;
			icalproperty_free(prop);
			GET_PROP(comp, prop, ICAL_DTSTART_PROPERTY);
			if (prop) {
				itt = icalproperty_get_dtstart(prop);
				icalproperty_free(prop);
				dtend = g_strdup(icaltime_as_ical_string(icaltime_add(itt,duration)));
				TO_UTF8(dtend);
			}
		}
	}
	GET_PROP(comp, prop, ICAL_SEQUENCE_PROPERTY);
	if (prop) {
		sequence = icalproperty_get_sequence(prop);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_METHOD_PROPERTY);
	if (prop) {
		method = icalproperty_get_method(prop);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_ORGANIZER_PROPERTY);
	if (prop) {
		org_email = get_email_from_property(prop);
		TO_UTF8(org_email);
		org_name = get_name_from_property(prop);
		TO_UTF8(org_name);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_DESCRIPTION_PROPERTY);
	if (prop) {
		description = g_strdup(icalproperty_get_description(prop));
		TO_UTF8(description);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_URL_PROPERTY);
	if (prop) {
		url = g_strdup(icalproperty_get_url(prop));
		TO_UTF8(url);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_TZID_PROPERTY);
	if (prop) {
		tzid = g_strdup(icalproperty_get_tzid(prop));
		TO_UTF8(tzid);
		icalproperty_free(prop);
	}
	GET_PROP(comp, prop, ICAL_RRULE_PROPERTY);
	if (prop) {
		struct icalrecurrencetype rrule = icalproperty_get_rrule(prop);
		recur = g_strdup(icalrecurrencetype_as_string(&rrule));
		TO_UTF8(recur);
		icalproperty_free(prop);
	}
	GET_PROP_LIST(comp, list, ICAL_ATTENDEE_PROPERTY);
	for (cur = list; cur; cur = cur->next) {
		enum icalparameter_partstat partstat = 0;
		enum icalparameter_cutype cutype = 0;
		icalparameter *param = NULL;
		gchar *email = NULL;
		gchar *name = NULL;
		Answer *answer = NULL;

		prop = (icalproperty *)(cur->data);

		email = get_email_from_property(prop);
		TO_UTF8(email);
		name = get_name_from_property(prop);
		TO_UTF8(name);

		param = icalproperty_get_first_parameter(prop, ICAL_PARTSTAT_PARAMETER);
		if (param)
			partstat = icalparameter_get_partstat(param);

		param = icalproperty_get_first_parameter(prop, ICAL_CUTYPE_PARAMETER);
		if (param)
			cutype= icalparameter_get_cutype(param);
		
		if (!partstat)
			partstat = ICAL_PARTSTAT_NEEDSACTION;
		if (!cutype)
			cutype = ICAL_CUTYPE_INDIVIDUAL;
		answer = answer_new(email, name, partstat, cutype);
		attendees = g_slist_prepend(attendees, answer);
		g_free(email);
		g_free(name);
		icalproperty_free(prop);
	}
	g_slist_free(list);
	
	event = vcal_manager_new_event	(uid, org_email, org_name,
					 location, summary, description,
					 dtstart, dtend, recur,
					 tzid, url,
					 method, sequence, type);
	event->answers = attendees;
	g_free(uid);
	g_free(location);
	g_free(summary);
	g_free(dtstart);
	g_free(dtend);
	g_free(org_email);
	g_free(org_name);
	g_free(description);
	g_free(url);
	g_free(tzid);
	g_free(recur);
	g_free(int_ical);
	icalcomponent_free(comp);
	return event;
}

gboolean vcal_event_exists(const gchar *id)
{
	MsgInfo *info = NULL;
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (!folder)
		return FALSE;

	info = folder_item_get_msginfo_by_msgid(folder->inbox, id);
	if (info != NULL) {
		procmsg_msginfo_free(&info);
		return TRUE;
	}
	return FALSE;
}

void vcal_foreach_event(gboolean (*cb_func)(const gchar *vevent))
{
	GSList *list = vcal_folder_get_waiting_events();
	GSList *cur = NULL;
	if (!cb_func)
		return;
	debug_print("calling cb_func...\n");
	for (cur = list; cur; cur = cur->next) {
		VCalEvent *event = (VCalEvent *)cur->data;
		gchar *tmp = vcal_get_event_as_ical_str(event);
		if (tmp) {
			debug_print(" ...for event %s\n", event->uid);
			cb_func(tmp);
		}
		vcal_manager_free_event(event);
		g_free(tmp);
	}
}

/* please call vcalendar_refresh_folder_contents() after one or more 
 * calls to this function */
gboolean vcal_delete_event(const gchar *id)
{
	MsgInfo *info = NULL;
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (!folder)
		return FALSE;

	info = folder_item_get_msginfo_by_msgid(folder->inbox, id);
	if (info != NULL) {
		debug_print("removing event %s\n", id);
		vcal_remove_event(folder, info);
		procmsg_msginfo_free(&info);
		folder_item_scan(folder->inbox);
		return TRUE;
	}
	debug_print("not removing unexisting event %s\n", id);
	return FALSE;
}

/* please call vcalendar_refresh_folder_contents() after one or more 
 * calls to this function */
gchar* vcal_add_event(const gchar *vevent)
{
	VCalEvent *event = vcal_get_event_from_ical(vevent, NULL);
	gchar *retVal = NULL;
	Folder *folder = folder_find_from_name (PLUGIN_NAME, vcal_folder_get_class());
	if (!folder) {
		vcal_manager_free_event(event);
		return NULL;
	}

	if (event) {
		if (vcal_event_exists(event->uid)) {
			debug_print("event %s already exists\n", event->uid);
			vcal_manager_free_event(event);
			return retVal;
		}
		debug_print("adding event %s\n", event->uid);
		if (!account_find_from_address(event->organizer, FALSE) &&
		    !vcal_manager_get_account_from_event(event)) {
			PrefsAccount *account = account_get_default();
			vcal_manager_update_answer(event, account->address, 
					account->name,
					ICAL_PARTSTAT_ACCEPTED, 
					ICAL_CUTYPE_INDIVIDUAL);
			debug_print("can't find our accounts in event, adding default\n");
		}
		vcal_manager_save_event(event, TRUE);
		folder_item_scan(folder->inbox);
		retVal = vcal_get_event_as_ical_str(event);
		vcal_manager_free_event(event);
	}

	return retVal;
}

/* please call vcalendar_refresh_folder_contents() after one or more 
 * calls to this function */
gchar* vcal_update_event(const gchar *vevent)
{
	VCalEvent *event = vcal_get_event_from_ical(vevent, NULL);
	gboolean r = FALSE;
	if (event) {
		r = vcal_delete_event(event->uid);
		vcal_manager_free_event(event);
		if (r)
			return vcal_add_event(vevent);
	}
	return NULL;
}
