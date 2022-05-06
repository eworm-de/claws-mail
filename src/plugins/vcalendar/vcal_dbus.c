/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2011 Colin Leroy <colin@colino.net> and 
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libical/ical.h>
#include <gtk/gtk.h>

#include "utils.h"
#include "vcal_manager.h"
#include "vcal_folder.h"

static guint dbus_own_id;

static void add_event_to_builder_if_match(VCalEvent *event, GVariantBuilder *array,
					  time_t start, time_t end)
{
	time_t evt_start = icaltime_as_timet((icaltime_from_string(event->dtstart)));
	time_t evt_end = icaltime_as_timet((icaltime_from_string(event->dtend)));
	if ((evt_start >= start && evt_start <= end)
	 || (evt_end >= start && evt_end <= end)) {
		g_variant_builder_open(array, 
			G_VARIANT_TYPE("(sssbxxa{sv})"));
			g_variant_builder_add(array, "s", event->uid);
			g_variant_builder_add(array, "s", event->summary);
			g_variant_builder_add(array, "s", event->description);
			g_variant_builder_add(array, "b", FALSE);
			g_variant_builder_add(array, "x", (gint64)evt_start);
			g_variant_builder_add(array, "x", (gint64)evt_end);
			g_variant_builder_open(array, G_VARIANT_TYPE("a{sv}"));
				/* We don't put stuff there. */
			g_variant_builder_close(array);
		g_variant_builder_close(array);
	 }
}

static void handle_method_call (GDBusConnection       *connection,
			        const gchar           *sender,
			        const gchar           *object_path,
			        const gchar           *interface_name,
			        const gchar           *method_name,
			        GVariant              *parameters,
			        GDBusMethodInvocation *invocation,
			        gpointer               user_data)
{
	time_t start, end;
	gboolean refresh;
	GSList *list, *cur;
	int i;
	GVariantBuilder *array = g_variant_builder_new(
			G_VARIANT_TYPE("(a(sssbxxa{sv}))"));
	GVariant *value;

	if (g_strcmp0(method_name, "GetEvents") != 0) {
		debug_print("Unknown method %s\n", method_name);
	}
	
	g_variant_get(parameters, "(xxb)", &start, &end, &refresh);

	
	g_variant_builder_open(array, G_VARIANT_TYPE("a(sssbxxa{sv})"));

	/* First our own calendar */
	list = vcal_folder_get_waiting_events();
	for (cur = list, i = 0; cur; cur = cur->next, i++) {
		VCalEvent *event = (VCalEvent *)cur->data;

		add_event_to_builder_if_match(event, array, start, end);

		g_free(event);
	}
	g_slist_free(list);

	/* Then subs. */
	list = vcal_folder_get_webcal_events();
	for (cur = list; cur; cur = cur->next) {
		/* Don't free that, it's done when subscriptions are
		 * fetched */
		icalcomponent *ical = (icalcomponent *)cur->data;
		if (ical != NULL) {
			VCalEvent *event = vcal_get_event_from_ical(
				icalcomponent_as_ical_string(ical), NULL);
			if (event != NULL) {
				add_event_to_builder_if_match(
					event, array, start, end);
				g_free(event);
			}
		}
	}
	g_slist_free(list);

	g_variant_builder_close(array);
	
	value = g_variant_builder_end(array);
	g_variant_builder_unref(array);

	g_dbus_method_invocation_return_value (invocation, value);
	g_variant_unref(value);
}


static GDBusInterfaceVTable* interface_vtable = NULL;

static GDBusNodeInfo *introspection_data = NULL;
static GDBusInterfaceInfo *interface_info = NULL;

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnome.Shell.CalendarServer'>"
  "    <method name='GetEvents'>"
  "      <arg type='x' name='greeting' direction='in'/>"
  "      <arg type='x' name='greeting' direction='in'/>"
  "      <arg type='b' name='greeting' direction='in'/>"
  "      <arg type='a(sssbxxa{sv})' name='events' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static void name_acquired (GDBusConnection *connection,
			   const gchar     *name,
			   gpointer	    user_data)
{
	debug_print("Acquired DBUS name %s\n", name);
}

static void name_lost (GDBusConnection *connection,
		       const gchar     *name,
		       gpointer		user_data)
{
	debug_print("Lost DBUS name %s\n", name);
}

static void bus_acquired(GDBusConnection *connection,
			 const gchar     *name,
			 gpointer         user_data)
{
	GError *err = NULL;

	cm_return_if_fail(interface_vtable);

	g_dbus_connection_register_object(connection,
		"/org/gnome/Shell/CalendarServer",
		introspection_data->interfaces[0],
		(const GDBusInterfaceVTable *)interface_vtable, NULL, NULL, &err);
	if (err != NULL)
		debug_print("Error: %s\n", err->message);
}

void connect_dbus(void)
{
	debug_print("connect_dbus() invoked\n");

	interface_vtable = g_malloc0(sizeof(GDBusInterfaceVTable));
	cm_return_if_fail(interface_vtable);
	interface_vtable->method_call = (GDBusInterfaceMethodCallFunc)handle_method_call;

	introspection_data = g_dbus_node_info_new_for_xml(
				introspection_xml, NULL);
	if (introspection_data == NULL) {
		debug_print("Couldn't figure out XML.\n");
		return;
	}

	interface_info = g_dbus_node_info_lookup_interface(
				introspection_data,
				"org.gnome.Shell.CalendarServer");
	dbus_own_id = g_bus_own_name(G_BUS_TYPE_SESSION,
			"org.gnome.Shell.CalendarServer",
			G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT
			| G_BUS_NAME_OWNER_FLAGS_REPLACE,
			bus_acquired,
			name_acquired,
			name_lost,
			NULL, NULL);
}

void disconnect_dbus(void)
{
	debug_print("disconnect_dbus() invoked\n");
	g_bus_unown_name(dbus_own_id);

	g_free(interface_vtable);
	interface_vtable = NULL;
}
