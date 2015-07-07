/* GeoLocation plugin for Claws-Mail
 * Copyright (C) 2009 Holger Berndt
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
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "pixmap_earth.h"

#include "common/plugin.h"
#include "common/version.h"
#include "common/utils.h"
#include "common/hooks.h"
#include "prefs_common.h"
#include "mimeview.h"
#include "procheader.h"
#include "main.h"

#include <libsoup/soup.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include <string.h>


/* For now, make extracting the ip address string from the Received header
 * as simple as possible. This should be made more accurate, some day.
 * Logic:
 * Find four 1-3 digit numbers, separated by dots, encosed in word boundaries,
 * where the word "from" must be ahead, and the word "by" must follow, but must
 * not be between the "from" and the digits.
 * The result will be in backreference 2, the individual numbers in 3 to 6 */
#define EXTRACT_IP_FROM_RECEIVED_HEADER_REGEX "\\bfrom\\b((?!\\bby\\b).)*(\\b(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\b).*\\bby\\b"

#define EXTRACT_LAT_LON_FROM_HOSTIP_RESPONSE_REGEX "<gml:coordinates>(-?[\\d.]+),(-?[\\d.]+)</gml:coordinates>"

#define GEOLOCATION_CONTAINER_NAME "cm_geolocation_container"
#define GEOLOCATION_CONTAINER_DATA_TOGGLEBUTTON "togglebutton"
#define GEOLOCATION_CONTAINER_DATA_LABEL "label"

#define GEOLOCATION_TOGGLE_BUTTON_DATA_CB_ID "toggled_cb_id"
#define GEOLOCATION_TOGGLE_BUTTON_DATA_IP "ip"

#define GEOLOCATION_NOTEBOOK_DATA_MAPWIDGET "mapwidget"

#define GEOLOCATION_MAP_DATA_VIEW "view"
#define GEOLOCATION_MAP_DATA_ORIGIN_MARKER "originmarker"

#define BUFFSIZE 8192

#define HOSTIP_URL "http://api.hostip.info/"

#define COUNTRY_RESOLVER_SUFFIX "zz.countries.nerd.dk"


static gboolean my_messageview_show_hook(gpointer, gpointer);
static guint hook_messageview_show;

static GRegex *ip_from_received_header_regex = NULL;
static GRegex *lat_lon_from_hostip_response_regex = NULL;

static GSList *container_list = NULL;

static GHashTable *iso_country_hash = NULL;

static gchar* get_ip_from_received_header(gchar *received)
{
  GMatchInfo *match_info;
  gchar *ip;
  gchar *n1;

  g_regex_match(ip_from_received_header_regex, received, 0, &match_info);
  ip = g_match_info_fetch(match_info, 2);

  if (ip) {
    /* check for loopback and local subnetworks */
    if(!strcmp(ip, "127.0.0.1")) {
      g_free(ip);
      ip = NULL;
    }
    else {
      n1 = g_match_info_fetch(match_info, 3);
      if(n1) {
        /* 10.x.x.x */
        if(!strcmp(n1, "10")) {
          g_free(ip);
          ip = NULL;
        }
        /* 192.168.x.x */
        else if(!strcmp(n1, "192")) {
          gchar *n2;
          n2 = g_match_info_fetch(match_info, 4);
          if(n2 && !strcmp(n2, "168")) {
            g_free(ip);
            ip = NULL;
          }
        }
        /* 172.16.xx to 172.31.x.x */
        else if(!strcmp(n1, "172")) {
          gchar *n2;
          int i2;
          n2 = g_match_info_fetch(match_info, 4);
          if(n2) {
            i2 = atoi(n2);
            if((i2 >= 16) && (i2 <= 31)) {
              g_free(ip);
              ip = NULL;
            }
          }
        }
      }
    }
  }

  g_match_info_free(match_info);
  return ip;
}

enum {
  H_RECEIVED = 0
};

static HeaderEntry hentry[] = {
    {"Received:", NULL, TRUE},
    {NULL, NULL, FALSE}};

static gchar* get_ip_from_msginfo(MsgInfo *msginfo)
{
  gchar *file;
  GStatBuf ss;
  FILE *fp;
  gchar buf[BUFFSIZE];
  gint hnum;
  gchar *hp;
  gchar *ip;
  gchar *new_ip;

  file = procmsg_get_message_file_path(msginfo);
  g_return_val_if_fail(file, NULL);

  if(g_stat(file, &ss) < 0) {
    FILE_OP_ERROR(file, "stat");
    return NULL;
  }
  if(!S_ISREG(ss.st_mode)) {
    g_warning("mail file is not a regular file\n");
    return NULL;
  }

  if((fp = g_fopen(file, "rb")) == NULL) {
    FILE_OP_ERROR(file, "fopen");
    return NULL;
  }

  ip = NULL;
  new_ip = NULL;
  while((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry)) != -1) {
    switch(hnum) {
    case H_RECEIVED:
      hp = buf + strlen(hentry[hnum].name);
      while(*hp == ' ' || *hp == '\t') hp++;
      new_ip = get_ip_from_received_header(hp);
      if(new_ip) {
        g_free(ip);
        ip = new_ip;
      }
      break;
    default:
      break;
    }
  }

  fclose(fp);

  g_free(file);
  return ip;
}

/* inspired by GeoClue */
static gboolean get_lat_lon_from_ip(const gchar *ip, double *lat, double *lon)
{
  gchar *url;
  GMatchInfo *match_info;
  gchar *slat;
  gchar *slon;
  SoupSession *session;
  SoupMessage *msg;

  /* fetch data from hostip.info */
  url = g_strdup_printf("%s?ip=%s", HOSTIP_URL, ip);
  session = soup_session_async_new();
  msg = soup_message_new(SOUP_METHOD_GET, url);
  g_free(url);

  soup_session_send_message(session, msg);
  g_object_unref(session);
  if(!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code) || !msg->response_body->data)
    return FALSE;

  /* extract longitude and latitude from */
  g_regex_match(lat_lon_from_hostip_response_regex, msg->response_body->data, 0, &match_info);
  g_object_unref(msg);
  slon = g_match_info_fetch(match_info, 1);
  slat = g_match_info_fetch(match_info, 2);
  g_match_info_free(match_info);

  if(!slat || !slon)
    return FALSE;

  *lat = g_ascii_strtod(slat, NULL);
  *lon = g_ascii_strtod(slon, NULL);

  return TRUE;
}

/* try to look up country */
static const gchar* get_country_from_ip(const gchar *ip)
{
  gchar **tok;
  gchar *uri;
  SoupAddress *addr;
  const gchar *val = NULL;

  tok = g_strsplit_set(ip, ".", 4);
  if(!(tok[0] && tok[1] && tok[2] && tok[3] && !tok[4])) {
    g_strfreev(tok);
    debug_print("GeoLocation plugin: Error parsing ip address '%s'\n", ip);
    return NULL;
  }
  uri = g_strjoin(".", tok[3], tok[2], tok[1], tok[0], COUNTRY_RESOLVER_SUFFIX, NULL);
  g_strfreev(tok);

  addr = soup_address_new(uri, SOUP_ADDRESS_ANY_PORT);
  g_free(uri);

  if(soup_address_resolve_sync(addr, NULL) == SOUP_STATUS_OK) {
    const gchar *physical = soup_address_get_physical(addr);
    val = g_hash_table_lookup(iso_country_hash, physical);
  }

  g_object_unref(addr);
  return val;
}

static GtkWidget* create_map_widget()
{
  GtkWidget *map;
  ChamplainLayer *layer;
  ChamplainView *view;
  ClutterActor *origin_marker;
  ClutterColor orange = { 0xf3, 0x94, 0x07, 0xbb };

  /* create map widget */
  map = gtk_champlain_embed_new();
  view = gtk_champlain_embed_get_view(GTK_CHAMPLAIN_EMBED(map));
  g_object_set(G_OBJECT(view), "scroll-mode", CHAMPLAIN_SCROLL_MODE_KINETIC, "zoom-level", 8, NULL);

  /* mail origin marker */
  layer = champlain_layer_new();
  origin_marker = champlain_marker_new_with_text("mail origin", "Serif 14", NULL, NULL);
  champlain_marker_set_color(CHAMPLAIN_MARKER(origin_marker), &orange);
  clutter_container_add(CLUTTER_CONTAINER(layer), origin_marker, NULL);
  champlain_view_add_layer(view, layer);

  clutter_actor_show(CLUTTER_ACTOR(view));
  clutter_actor_show(CLUTTER_ACTOR(layer));
  clutter_actor_show(origin_marker);

  g_object_set_data(G_OBJECT(map), GEOLOCATION_MAP_DATA_VIEW, view);
  g_object_set_data(G_OBJECT(map), GEOLOCATION_MAP_DATA_ORIGIN_MARKER, origin_marker);

  return map;
}

static int get_notebook_map_page_num(GtkWidget *notebook)
{
  GtkWidget *wid;
  int page_num = 0;

  wid = g_object_get_data(G_OBJECT(notebook), GEOLOCATION_NOTEBOOK_DATA_MAPWIDGET);

  if(!wid) {
    /* create page */
    GtkWidget *map;
    map = create_map_widget();
    page_num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), map, NULL);
    gtk_widget_show_all(map);
    if(page_num == -1)
      page_num = 0;
    else
      g_object_set_data(G_OBJECT(notebook), GEOLOCATION_NOTEBOOK_DATA_MAPWIDGET, map);
  }
  else {
    /* find page */
    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), wid);
    if(page_num == -1)
      page_num = 0;
  }

  return page_num;
}

static void geolocation_button_toggled_cb(GtkToggleButton *button, gpointer data)
{
  if(gtk_toggle_button_get_active(button)) {
    gchar *ip;
    double lat, lon;
    gchar *text = NULL;
    GtkWidget *container, *label;

    ip = g_object_get_data(G_OBJECT(button), GEOLOCATION_TOGGLE_BUTTON_DATA_IP);
    container = gtk_widget_get_parent(GTK_WIDGET(button));
    g_return_if_fail(container);
    label = g_object_get_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_LABEL);
    g_return_if_fail(label);

    if(ip && get_lat_lon_from_ip(ip, &lat, &lon)) {
      GtkWidget *notebook;
      GtkWidget *map;
      MessageView *messageview;
      ClutterActor *view;
      ClutterActor *origin_marker;
      gint page_num;

      /* TRANSLATORS: The two numbers are latitude and longitude coordinates */
      text = g_strdup_printf(_("Found location: (%.2f,%.2f)"), lat, lon);
      gtk_label_set_text(GTK_LABEL(label), text);
      debug_print("%s\n", text);

      /* switch to map widget, and center on found location */
      messageview = data;
      notebook = messageview->mimeview->mime_notebook;
      page_num = get_notebook_map_page_num(notebook);
      map = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_num);
      view = g_object_get_data(G_OBJECT(map), GEOLOCATION_MAP_DATA_VIEW);
      origin_marker = g_object_get_data(G_OBJECT(map), GEOLOCATION_MAP_DATA_ORIGIN_MARKER);
      champlain_view_center_on(CHAMPLAIN_VIEW(view), lat, lon);
      champlain_base_marker_set_position(CHAMPLAIN_BASE_MARKER(origin_marker), lat, lon);
      gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);
    }
    else {
      const char *country;

      if(ip && ((country = get_country_from_ip(ip)) != NULL)) {
        /* TRANSLATORS: The country name is appended to the string */
        text = g_strconcat(_("Alleged country of origin: "), country, NULL);
      }
      else {
        /* TRANSLATORS: The IP address is appended to the string */
        text = g_strconcat(_("Could not resolve location of IP address "), ip, NULL);
      }
      gtk_label_set_text(GTK_LABEL(label), text);
      debug_print("%s\n", text);
      gtk_toggle_button_set_active(button, FALSE);
    }
    g_free(text);
  }
  else {
    /* swich to first notebook page */
    MessageView *messageview;
    messageview = data;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(messageview->mimeview->mime_notebook), 0);
  }
}

static void container_finalize_cb(gpointer data, GObject *where_the_object_was)
{
  container_list = g_slist_remove(container_list, where_the_object_was);
}

static GtkWidget* get_geolocation_container_from_messageview(MessageView *messageview)
{
  GtkWidget *container = NULL;
  GtkWidget *vbox;
  GList *children, *walk;
  GtkWidget *button;
  gulong *cb_id;

  g_return_val_if_fail(messageview && messageview->mimeview && messageview->mimeview->paned, NULL);

  vbox = gtk_paned_get_child2(GTK_PANED(messageview->mimeview->paned));

  /* check if there is already a geolocation container */
  children = gtk_container_get_children(GTK_CONTAINER(vbox));
  for(walk = children; walk; walk = walk->next) {
    GtkWidget *child = walk->data;
    if(!strcmp(gtk_widget_get_name(child), GEOLOCATION_CONTAINER_NAME)) {
      container = child;
      break;
    }
  }
  g_list_free(children);

  /* if not, create one */
  if(!container) {
    GtkWidget *label;
    GtkWidget *image;
    GdkPixbuf *pixbuf, *scaled_pixbuf;

    container = gtk_hbox_new(FALSE, 0);
    gtk_widget_set_name(container, GEOLOCATION_CONTAINER_NAME);

    /* button */
    pixbuf = gdk_pixbuf_new_from_inline(-1, pixmap_earth, FALSE, NULL);
    scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);
    image = gtk_image_new_from_pixbuf(scaled_pixbuf);
    gtk_widget_show(image);
    g_object_unref(scaled_pixbuf);
    button = gtk_toggle_button_new();
    gtk_widget_show(button);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_box_pack_start(GTK_BOX(container), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_TOGGLEBUTTON, button);

    /* label */
    label = gtk_label_new("");
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(container), label, FALSE, FALSE, 2);
    g_object_set_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_LABEL, label);

    gtk_box_pack_start(GTK_BOX(vbox), container, FALSE, FALSE, 0);

    /* add new container to list */
    container_list = g_slist_prepend(container_list, container);
    g_object_weak_ref(G_OBJECT(container), container_finalize_cb, NULL);
  }
  /* if it existed, changed the button callback data */
  else {
    /* disconnect old signal handler from button */
    gulong *id;
    button = g_object_get_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_TOGGLEBUTTON);
    g_return_val_if_fail(button, NULL);
    id = g_object_get_data(G_OBJECT(button), GEOLOCATION_TOGGLE_BUTTON_DATA_CB_ID);
    g_signal_handler_disconnect(button, *id);
  }

  /* connect new signal handler to button, and store its id */
  cb_id = g_new(gulong, 1);
  *cb_id = g_signal_connect(button, "toggled", G_CALLBACK(geolocation_button_toggled_cb), messageview);
  g_object_set_data_full(G_OBJECT(button), GEOLOCATION_TOGGLE_BUTTON_DATA_CB_ID, cb_id, g_free);

  return container;
}

static gboolean my_messageview_show_hook(gpointer source, gpointer data)
{
  MessageView *messageview;
  GtkWidget *container;
  GtkWidget *button;
  gchar *ip;

  messageview = source;

  g_return_val_if_fail(messageview, FALSE);

  container = get_geolocation_container_from_messageview(messageview);
  g_return_val_if_fail(container, FALSE);

  /* make sure toggle button is turned off */
  button = g_object_get_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_TOGGLEBUTTON);
  g_return_val_if_fail(button, FALSE);
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
  }

  /* do nothing except hiding the button if claws mail is in offline mode */
  if(prefs_common_get_prefs()->work_offline) {
    gtk_widget_hide(container);
    return FALSE;
  }

  /* try to get ip address from message */
  if(!messageview->msginfo) {
    debug_print("Warning: Messageview does not have a MsgInfo\n");
    return FALSE;
  }
  ip = get_ip_from_msginfo(messageview->msginfo);
  if(!ip) {
    debug_print("Could not get IP address from message\n");
    gtk_widget_hide(container);
  }
  else {
    GtkWidget *label;
    debug_print("Found sender ip address: %s\n", ip);
    label = g_object_get_data(G_OBJECT(container), GEOLOCATION_CONTAINER_DATA_LABEL);
    if(label) {
      gchar *text;
      text = g_strconcat(_("Try to locate sender"), NULL);
      gtk_label_set_text(GTK_LABEL(label), text);
      g_free(text);
    }
    /* button takes ownership of ip string */
    g_object_set_data_full(G_OBJECT(button), GEOLOCATION_TOGGLE_BUTTON_DATA_IP, ip, g_free);
    gtk_widget_show(container);
  }

  return FALSE;
}

#define ISO_IN(k,v) g_hash_table_insert(iso_country_hash, (k), (v))
static void fill_iso_country_hash(void)
{
  iso_country_hash = g_hash_table_new(g_str_hash, g_str_equal);
  ISO_IN("127.0.0.20", _("Andorra"));
  ISO_IN("127.0.3.16", _("United Arab Emirates"));
  ISO_IN("127.0.0.4", _("Afghanistan"));
  ISO_IN("127.0.0.28", _("Antigua And Barbuda"));
  ISO_IN("127.0.2.148", _("Anguilla"));
  ISO_IN("127.0.0.8", _("Albania"));
  ISO_IN("127.0.0.51", _("Armenia"));
  ISO_IN("127.0.2.18", _("Netherlands Antilles"));
  ISO_IN("127.0.0.24", _("Angola"));
  ISO_IN("127.0.0.10", _("Antarctica"));
  ISO_IN("127.0.0.32", _("Argentina"));
  ISO_IN("127.0.0.16", _("American Samoa"));
  ISO_IN("127.0.0.40", _("Austria"));
  ISO_IN("127.0.0.36", _("Australia"));
  ISO_IN("127.0.2.21", _("Aruba"));
  ISO_IN("127.0.0.31", _("Azerbaijan"));
  ISO_IN("127.0.0.70", _("Bosnia And Herzegovina"));
  ISO_IN("127.0.0.52", _("Barbados"));
  ISO_IN("127.0.0.50", _("Bangladesh"));
  ISO_IN("127.0.0.56", _("Belgium"));
  ISO_IN("127.0.3.86", _("Burkina Faso"));
  ISO_IN("127.0.0.100", _("Bulgaria"));
  ISO_IN("127.0.0.48", _("Bahrain"));
  ISO_IN("127.0.0.108", _("Burundi"));
  ISO_IN("127.0.0.204", _("Benin"));
  ISO_IN("127.0.0.60", _("Bermuda"));
  ISO_IN("127.0.0.96", _("Brunei Darussalam"));
  ISO_IN("127.0.0.68", _("Bolivia"));
  ISO_IN("127.0.0.76", _("Brazil"));
  ISO_IN("127.0.0.44", _("Bahamas"));
  ISO_IN("127.0.0.64", _("Bhutan"));
  ISO_IN("127.0.0.74", _("Bouvet Island"));
  ISO_IN("127.0.0.72", _("Botswana"));
  ISO_IN("127.0.0.112", _("Belarus"));
  ISO_IN("127.0.0.84", _("Belize"));
  ISO_IN("127.0.0.124", _("Canada"));
  ISO_IN("127.0.0.166", _("Cocos (Keeling) Islands"));
  ISO_IN("127.0.0.140", _("Central African Republic"));
  ISO_IN("127.0.0.178", _("Congo"));
  ISO_IN("127.0.2.244", _("Switzerland"));
  ISO_IN("127.0.1.128", _("Cote D'Ivoire"));
  ISO_IN("127.0.0.184", _("Cook Islands"));
  ISO_IN("127.0.0.152", _("Chile"));
  ISO_IN("127.0.0.120", _("Cameroon"));
  ISO_IN("127.0.0.156", _("China"));
  ISO_IN("127.0.0.170", _("Colombia"));
  ISO_IN("127.0.0.188", _("Costa Rica"));
  ISO_IN("127.0.0.192", _("Cuba"));
  ISO_IN("127.0.0.132", _("Cape Verde"));
  ISO_IN("127.0.0.162", _("Christmas Island"));
  ISO_IN("127.0.0.196", _("Cyprus"));
  ISO_IN("127.0.0.203", _("Czech Republic"));
  ISO_IN("127.0.1.20", _("Germany"));
  ISO_IN("127.0.1.6", _("Djibouti"));
  ISO_IN("127.0.0.208", _("Denmark"));
  ISO_IN("127.0.0.212", _("Dominica"));
  ISO_IN("127.0.0.214", _("Dominican Republic"));
  ISO_IN("127.0.0.12", _("Algeria"));
  ISO_IN("127.0.0.218", _("Ecuador"));
  ISO_IN("127.0.0.233", _("Estonia"));
  ISO_IN("127.0.3.50", _("Egypt"));
  ISO_IN("127.0.2.220", _("Western Sahara"));
  ISO_IN("127.0.0.232", _("Eritrea"));
  ISO_IN("127.0.2.212", _("Spain"));
  ISO_IN("127.0.0.231", _("Ethiopia"));
  ISO_IN("127.0.0.246", _("Finland"));
  ISO_IN("127.0.0.242", _("Fiji"));
  ISO_IN("127.0.0.238", _("Falkland Islands (Malvinas)"));
  ISO_IN("127.0.2.71", _("Micronesia, Federated States Of"));
  ISO_IN("127.0.0.234", _("Faroe Islands"));
  ISO_IN("127.0.0.250", _("France"));
  ISO_IN("127.0.0.249", _("France, Metropolitan"));
  ISO_IN("127.0.1.10", _("Gabon"));
  ISO_IN("127.0.3.58", _("United Kingdom"));
  ISO_IN("127.0.1.52", _("Grenada"));
  ISO_IN("127.0.1.12", _("Georgia"));
  ISO_IN("127.0.0.254", _("French Guiana"));
  ISO_IN("127.0.1.32", _("Ghana"));
  ISO_IN("127.0.1.36", _("Gibraltar"));
  ISO_IN("127.0.1.48", _("Greenland"));
  ISO_IN("127.0.1.14", _("Gambia"));
  ISO_IN("127.0.1.68", _("Guinea"));
  ISO_IN("127.0.1.56", _("Guadeloupe"));
  ISO_IN("127.0.0.226", _("Equatorial Guinea"));
  ISO_IN("127.0.1.44", _("Greece"));
  ISO_IN("127.0.0.239", _("South Georgia And The South Sandwich Islands"));
  ISO_IN("127.0.1.64", _("Guatemala"));
  ISO_IN("127.0.1.60", _("Guam"));
  ISO_IN("127.0.2.112", _("Guinea-Bissau"));
  ISO_IN("127.0.1.72", _("Guyana"));
  ISO_IN("127.0.1.88", _("Hong Kong"));
  ISO_IN("127.0.1.78", _("Heard Island And Mcdonald Islands"));
  ISO_IN("127.0.1.84", _("Honduras"));
  ISO_IN("127.0.0.191", _("Croatia"));
  ISO_IN("127.0.1.76", _("Haiti"));
  ISO_IN("127.0.1.92", _("Hungary"));
  ISO_IN("127.0.1.104", _("Indonesia"));
  ISO_IN("127.0.1.116", _("Ireland"));
  ISO_IN("127.0.1.120", _("Israel"));
  ISO_IN("127.0.1.100", _("India"));
  ISO_IN("127.0.0.86", _("British Indian Ocean Territory"));
  ISO_IN("127.0.1.112", _("Iraq"));
  ISO_IN("127.0.1.108", _("Iran, Islamic Republic Of"));
  ISO_IN("127.0.1.96", _("Iceland"));
  ISO_IN("127.0.1.124", _("Italy"));
  ISO_IN("127.0.1.132", _("Jamaica"));
  ISO_IN("127.0.1.144", _("Jordan"));
  ISO_IN("127.0.1.136", _("Japan"));
  ISO_IN("127.0.1.148", _("Kenya"));
  ISO_IN("127.0.1.161", _("Kyrgyzstan"));
  ISO_IN("127.0.0.116", _("Cambodia"));
  ISO_IN("127.0.1.40", _("Kiribati"));
  ISO_IN("127.0.0.174", _("Comoros"));
  ISO_IN("127.0.2.147", _("Saint Kitts And Nevis"));
  ISO_IN("127.0.1.152", _("Korea, Democratic People'S Republic Of"));
  ISO_IN("127.0.1.154", _("Korea, Republic Of"));
  ISO_IN("127.0.1.158", _("Kuwait"));
  ISO_IN("127.0.0.136", _("Cayman Islands"));
  ISO_IN("127.0.1.142", _("Kazakhstan"));
  ISO_IN("127.0.1.162", _("Lao People'S Democratic Republic"));
  ISO_IN("127.0.1.166", _("Lebanon"));
  ISO_IN("127.0.2.150", _("Saint Lucia"));
  ISO_IN("127.0.1.182", _("Liechtenstein"));
  ISO_IN("127.0.0.144", _("Sri Lanka"));
  ISO_IN("127.0.1.174", _("Liberia"));
  ISO_IN("127.0.1.170", _("Lesotho"));
  ISO_IN("127.0.1.184", _("Lithuania"));
  ISO_IN("127.0.1.186", _("Luxembourg"));
  ISO_IN("127.0.1.172", _("Latvia"));
  ISO_IN("127.0.1.178", _("Libyan Arab Jamahiriya"));
  ISO_IN("127.0.1.248", _("Morocco"));
  ISO_IN("127.0.1.236", _("Monaco"));
  ISO_IN("127.0.1.242", _("Moldova, Republic Of"));
  ISO_IN("127.0.1.194", _("Madagascar"));
  ISO_IN("127.0.2.72", _("Marshall Islands"));
  ISO_IN("127.0.3.39", _("Macedonia, The Former Yugoslav Republic Of"));
  ISO_IN("127.0.1.210", _("Mali"));
  ISO_IN("127.0.0.104", _("Myanmar"));
  ISO_IN("127.0.1.240", _("Mongolia"));
  ISO_IN("127.0.1.190", _("Macao"));
  ISO_IN("127.0.2.68", _("Northern Mariana Islands"));
  ISO_IN("127.0.1.218", _("Martinique"));
  ISO_IN("127.0.1.222", _("Mauritania"));
  ISO_IN("127.0.1.244", _("Montserrat"));
  ISO_IN("127.0.1.214", _("Malta"));
  ISO_IN("127.0.1.224", _("Mauritius"));
  ISO_IN("127.0.1.206", _("Maldives"));
  ISO_IN("127.0.1.198", _("Malawi"));
  ISO_IN("127.0.1.228", _("Mexico"));
  ISO_IN("127.0.1.202", _("Malaysia"));
  ISO_IN("127.0.1.252", _("Mozambique"));
  ISO_IN("127.0.2.4", _("Namibia"));
  ISO_IN("127.0.2.28", _("New Caledonia"));
  ISO_IN("127.0.2.50", _("Niger"));
  ISO_IN("127.0.2.62", _("Norfolk Island"));
  ISO_IN("127.0.2.54", _("Nigeria"));
  ISO_IN("127.0.2.46", _("Nicaragua"));
  ISO_IN("127.0.2.16", _("Netherlands"));
  ISO_IN("127.0.2.66", _("Norway"));
  ISO_IN("127.0.2.12", _("Nepal"));
  ISO_IN("127.0.2.8", _("Nauru"));
  ISO_IN("127.0.2.58", _("Niue"));
  ISO_IN("127.0.2.42", _("New Zealand"));
  ISO_IN("127.0.2.0", _("Oman"));
  ISO_IN("127.0.2.79", _("Panama"));
  ISO_IN("127.0.2.92", _("Peru"));
  ISO_IN("127.0.1.2", _("French Polynesia"));
  ISO_IN("127.0.2.86", _("Papua New Guinea"));
  ISO_IN("127.0.2.96", _("Philippines"));
  ISO_IN("127.0.2.74", _("Pakistan"));
  ISO_IN("127.0.2.104", _("Poland"));
  ISO_IN("127.0.2.154", _("Saint Pierre And Miquelon"));
  ISO_IN("127.0.2.100", _("Pitcairn"));
  ISO_IN("127.0.2.118", _("Puerto Rico"));
  ISO_IN("127.0.2.108", _("Portugal"));
  ISO_IN("127.0.2.73", _("Palau"));
  ISO_IN("127.0.2.88", _("Paraguay"));
  ISO_IN("127.0.2.122", _("Qatar"));
  ISO_IN("127.0.2.126", _("Reunion"));
  ISO_IN("127.0.2.130", _("Romania"));
  ISO_IN("127.0.2.131", _("Russian Federation"));
  ISO_IN("127.0.2.134", _("Rwanda"));
  ISO_IN("127.0.2.170", _("Saudi Arabia"));
  ISO_IN("127.0.0.90", _("Solomon Islands"));
  ISO_IN("127.0.2.178", _("Seychelles"));
  ISO_IN("127.0.2.224", _("Sudan"));
  ISO_IN("127.0.2.240", _("Sweden"));
  ISO_IN("127.0.2.190", _("Singapore"));
  ISO_IN("127.0.2.142", _("Saint Helena"));
  ISO_IN("127.0.2.193", _("Slovenia"));
  ISO_IN("127.0.2.232", _("Svalbard And Jan Mayen"));
  ISO_IN("127.0.2.191", _("Slovakia"));
  ISO_IN("127.0.2.182", _("Sierra Leone"));
  ISO_IN("127.0.2.162", _("San Marino"));
  ISO_IN("127.0.2.174", _("Senegal"));
  ISO_IN("127.0.2.194", _("Somalia"));
  ISO_IN("127.0.2.228", _("Suriname"));
  ISO_IN("127.0.2.166", _("Sao Tome And Principe"));
  ISO_IN("127.0.0.222", _("El Salvador"));
  ISO_IN("127.0.2.248", _("Syrian Arab Republic"));
  ISO_IN("127.0.2.236", _("Swaziland"));
  ISO_IN("127.0.3.28", _("Turks And Caicos Islands"));
  ISO_IN("127.0.0.148", _("Chad"));
  ISO_IN("127.0.1.4", _("French Southern Territories"));
  ISO_IN("127.0.3.0", _("Togo"));
  ISO_IN("127.0.2.252", _("Thailand"));
  ISO_IN("127.0.2.250", _("Tajikistan"));
  ISO_IN("127.0.3.4", _("Tokelau"));
  ISO_IN("127.0.3.27", _("Turkmenistan"));
  ISO_IN("127.0.3.20", _("Tunisia"));
  ISO_IN("127.0.3.8", _("Tonga"));
  ISO_IN("127.0.2.114", _("East Timor"));
  ISO_IN("127.0.3.24", _("Turkey"));
  ISO_IN("127.0.3.12", _("Trinidad And Tobago"));
  ISO_IN("127.0.3.30", _("Tuvalu"));
  ISO_IN("127.0.0.158", _("Taiwan, Province Of China"));
  ISO_IN("127.0.3.66", _("Tanzania, United Republic Of"));
  ISO_IN("127.0.3.36", _("Ukraine"));
  ISO_IN("127.0.3.32", _("Uganda"));
  ISO_IN("127.0.3.58", _("United Kingdom"));
  ISO_IN("127.0.2.69", _("United States Minor Outlying Islands"));
  ISO_IN("127.0.3.72", _("United States"));
  ISO_IN("127.0.3.90", _("Uruguay"));
  ISO_IN("127.0.3.92", _("Uzbekistan"));
  ISO_IN("127.0.1.80", _("Holy See (Vatican City State)"));
  ISO_IN("127.0.2.158", _("Saint Vincent And The Grenadines"));
  ISO_IN("127.0.3.94", _("Venezuela"));
  ISO_IN("127.0.0.92", _("Virgin Islands, British"));
  ISO_IN("127.0.3.82", _("Virgin Islands, U.S."));
  ISO_IN("127.0.2.192", _("Viet Nam"));
  ISO_IN("127.0.2.36", _("Vanuatu"));
  ISO_IN("127.0.3.108", _("Wallis And Futuna"));
  ISO_IN("127.0.3.114", _("Samoa"));
  ISO_IN("127.0.3.119", _("Yemen"));
  ISO_IN("127.0.0.175", _("Mayotte"));
  ISO_IN("127.0.3.123", _("Serbia And Montenegro"));
  ISO_IN("127.0.2.198", _("South Africa"));
  ISO_IN("127.0.3.126", _("Zambia"));
  ISO_IN("127.0.0.180", _("Democratic Republic Of The Congo"));
  ISO_IN("127.0.2.204", _("Zimbabwe"));
}
#undef ISO_IN

gint plugin_init(gchar **error)
{
  GError *err = NULL;

  /* Version check */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,7,1,55),
			   VERSION_NUMERIC, _("GeoLocation"), error))
    return -1;

  if(gtk_clutter_init(0, NULL) != CLUTTER_INIT_SUCCESS)
  {
    *error = g_strdup_printf(_("Could not initialize clutter"));
    return -1;
  }

  ip_from_received_header_regex = g_regex_new(EXTRACT_IP_FROM_RECEIVED_HEADER_REGEX, 0, 0, &err);
  if(err) {
    *error = g_strdup_printf(_("Could not create regular expression: %s\n"), err->message);
    g_error_free(err);
    return -1;
  }

  lat_lon_from_hostip_response_regex = g_regex_new(EXTRACT_LAT_LON_FROM_HOSTIP_RESPONSE_REGEX, 0, 0, &err);
  if(err) {
    *error = g_strdup_printf(_("Could not create regular expression: %s\n"), err->message);
    g_error_free(err);
    g_regex_unref(ip_from_received_header_regex);
    return -1;
  }

  hook_messageview_show = hooks_register_hook(MESSAGE_VIEW_SHOW_DONE_HOOKLIST, my_messageview_show_hook, NULL);
  if(hook_messageview_show == (guint) -1) {
    *error = g_strdup(_("Failed to register messageview_show hook in the GeoLocation plugin"));
    g_regex_unref(ip_from_received_header_regex);
    g_regex_unref(lat_lon_from_hostip_response_regex);
    return -1;
  }

  fill_iso_country_hash();

  debug_print("GeoLocation plugin loaded\n");

  return 0;
}

gboolean plugin_done(void)
{

  hooks_unregister_hook(MESSAGE_VIEW_SHOW_DONE_HOOKLIST, hook_messageview_show);

  if(!claws_is_exiting()) {
    GSList *copy, *walk;

    copy = g_slist_copy(container_list);
    for(walk = copy; walk; walk = walk->next) {
      GtkWidget *wid = walk->data;
      GtkWidget *button = g_object_get_data(G_OBJECT(wid), GEOLOCATION_CONTAINER_DATA_TOGGLEBUTTON);
      if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
      gtk_widget_destroy(GTK_WIDGET(wid));
    }
    g_slist_free(copy);
    if(container_list) {
      g_slist_free(container_list);
      container_list = NULL;
    }

    if(ip_from_received_header_regex) {
      g_regex_unref(ip_from_received_header_regex);
      ip_from_received_header_regex = NULL;
    }

    if(lat_lon_from_hostip_response_regex) {
      g_regex_unref(lat_lon_from_hostip_response_regex);
      lat_lon_from_hostip_response_regex = NULL;
    }

    if(iso_country_hash) {
      g_hash_table_destroy(iso_country_hash);
      iso_country_hash = NULL;
    }
  }

  debug_print("GeoLocation plugin unloaded\n");

  /* returning FALSE because dependant libraries may not be unload-safe. */
  return FALSE;
}

const gchar *plugin_name(void)
{
  return _("GeoLocation");
}

const gchar *plugin_desc(void)
{
  return _("This plugin provides GeoLocation functionality "
	   "for Claws Mail.\n\n"
	   "Warning: It is technically impossible to derive the geographic "
	   "location of senders from their E-Mails with any amount of certainty. "
	   "The results presented by "
	   "this plugin are only rough estimates. In particular, mailing list managers "
	   "often strip sender information from the mails, so mails from mailing lists "
	   "may be assigned to the location of the mailing list server instead of the "
	   "mail sender.\n"
	   "When in doubt, don't trust the results of this plugin, and don't rely on this "
	   "information to divorce your spouse.\n"
           "\nFeedback to <berndth@gmx.de> is welcome "
	   "(but only if it's not about marital quarrels).");
}

const gchar *plugin_type(void)
{
  return "GTK2";
}

const gchar *plugin_licence(void)
{
  return "GPL3+";
}

const gchar *plugin_version(void)
{
  return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
  static struct PluginFeature features[] =
    { {PLUGIN_UTILITY, N_("GeoLocation integration")},
      {PLUGIN_NOTHING, NULL}};
  return features;
}
