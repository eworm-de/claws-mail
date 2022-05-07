/* GData plugin for Claws Mail
 * Copyright (C) 2011 Holger Berndt
 * Copyright (C) 2011-2019 the Claws Mail team
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

#include "cm_gdata_contacts.h"
#include "cm_gdata_prefs.h"

#include "addr_compl.h"
#include "main.h"
#include "passwordstore.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "common/log.h"
#include "common/xml.h"
#include "common/utils.h"
#include "common/passcrypt.h"
#include "gtk/gtkutils.h"

#include <gdata/gdata.h>

#define GDATA_CONTACTS_FILENAME "gdata_cache.xml"

#define GDATA_C1 "EOnuQt4fxED3WrO//iub3KLQMScIxXiT0VBD8RZUeKjkcm1zEBVMboeWDLYyqjJKZaL4oaZ24umWygbj19T2oJR6ZpjbCw=="
#define GDATA_C2 "QYjIgZblg/4RMCnEqNQypcHZba9ePqAN"
#define GDATA_C3 "XHEZEgO06YbWfQWOyYhE/ny5Q10aNOZlkQ=="

#define REFRESH_TIMEOUT_MINUTES 45.0


typedef struct
{
  const gchar *family_name;
  const gchar *given_name;
  const gchar *full_name;
  const gchar *address;
} Contact;

typedef struct
{
  GSList *contacts;
} CmGDataContactsCache;


static CmGDataContactsCache contacts_cache;
static gboolean cm_gdata_contacts_query_running = FALSE;
static gchar *contacts_group_id = NULL;
static GDataOAuth2Authorizer *authorizer = NULL;
static GDataContactsService *service = NULL;
static GTimer *refresh_timer = NULL;


static void protect_fields_against_NULL(Contact *contact)
{
  g_return_if_fail(contact != NULL);

  /* protect fields against being NULL */
  if(contact->full_name == NULL)
    contact->full_name = g_strdup("");
  if(contact->given_name == NULL)
    contact->given_name = g_strdup("");
  if(contact->family_name == NULL)
    contact->family_name = g_strdup("");
}

typedef struct
{
  const gchar *auth_uri;
  GtkWidget *entry;
} AuthCodeQueryButtonData;


static void auth_uri_link_button_clicked_cb(GtkButton *button, gpointer data)
{
  AuthCodeQueryButtonData *auth_code_query_data = data;
  open_uri(auth_code_query_data->auth_uri, prefs_common_get_uri_cmd());
  gtk_widget_grab_focus(auth_code_query_data->entry);
}

static void auth_code_entry_changed_cb(GtkEditable *entry, gpointer data)
{
  gtk_widget_set_sensitive(GTK_WIDGET(data), gtk_entry_get_text_length(GTK_ENTRY(entry)) > 0);
}


/* Returns the authorization code as newly allocated string, or NULL */
gchar* ask_user_for_auth_code(const gchar *auth_uri)
{
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *link_button;
  GtkWidget *label;
  GtkWidget *entry;
  gchar *str;
  gchar *retval = NULL;
  MainWindow *mainwin;
  gint dlg_res;
  GtkWidget *btn_ok;
  AuthCodeQueryButtonData *auth_code_query_data;

  mainwin = mainwindow_get_mainwindow();
  dialog = gtk_message_dialog_new_with_markup(mainwin ? GTK_WINDOW(mainwin->window) : NULL,
      GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_NONE,
      "<span weight=\"bold\" size=\"larger\">%s</span>", _("GData plugin: Authorization required"));
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
      _("You need to authorize Claws Mail to access your Google contact list to use the GData plugin."
      "\n\nVisit Google's authorization page by pressing the button below. After you "
      "confirmed the authorization, you will get an authorization code. Enter that code "
      "in the field below to grant Claws Mail access to your Google contact list."));
  gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  btn_ok = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_OK"), GTK_RESPONSE_OK);
  gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

  gtk_widget_set_sensitive(btn_ok, FALSE);

  table = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(table), 8);
  gtk_grid_set_column_spacing(GTK_GRID(table), 8);

  str = g_strconcat("<b>", _("Step 1:"), "</b>", NULL);
  label = gtk_label_new(str);
  g_free(str);
  gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
  gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

  link_button = gtk_button_new_with_label(_("Click here to open the Google authorization page in a browser"));
  auth_code_query_data = g_new0(AuthCodeQueryButtonData,1);
  gtk_grid_attach(GTK_GRID(table), link_button, 1, 0, 1, 1);

  str = g_strconcat("<b>", _("Step 2:"), "</b>", NULL);
  label = gtk_label_new(str);
  g_free(str);
  gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
  gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);

  gtk_grid_attach(GTK_GRID(table),  gtk_label_new(_("Enter code:")), 1, 1, 1, 1);

  entry = gtk_entry_new();
  g_signal_connect(G_OBJECT(entry), "changed", (GCallback)auth_code_entry_changed_cb, (gpointer)btn_ok);
  gtk_grid_attach(GTK_GRID(table), entry, 2, 1, 1, 1);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_widget_set_halign(entry, GTK_ALIGN_FILL);

  auth_code_query_data->auth_uri = auth_uri;
  auth_code_query_data->entry = entry;
  g_signal_connect(G_OBJECT(link_button), "clicked", (GCallback)auth_uri_link_button_clicked_cb, (gpointer)auth_code_query_data);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog))), vbox, FALSE, FALSE, 0);

  gtk_widget_show_all(dialog);

  dlg_res = gtk_dialog_run(GTK_DIALOG(dialog));
  switch(dlg_res)
  {
  case GTK_RESPONSE_DELETE_EVENT:
  case GTK_RESPONSE_CANCEL:
    break;
  case GTK_RESPONSE_OK:
    retval = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    break;
  }

  g_free(auth_code_query_data);
  gtk_widget_destroy(dialog);

  return retval;
}


static void write_cache_to_file(void)
{
  gchar *path;
  PrefFile *pfile;
  XMLTag *tag;
  XMLNode *xmlnode;
  GNode *rootnode;
  GNode *contactsnode;
  GSList *walk;

  path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, GDATA_CONTACTS_FILENAME, NULL);
  pfile = prefs_write_open(path);
  g_free(path);
  if(pfile == NULL) {
    debug_print("GData plugin error: Cannot open file " GDATA_CONTACTS_FILENAME " for writing\n");
    return;
  }

  /* XML declarations */
  xml_file_put_xml_decl(pfile->fp);

  /* Build up XML tree */

  /* root node */
  tag = xml_tag_new("gdata");
  xmlnode = xml_node_new(tag, NULL);
  rootnode = g_node_new(xmlnode);

  /* contacts node */
  tag = xml_tag_new("contacts");
  xmlnode = xml_node_new(tag, NULL);
  contactsnode = g_node_new(xmlnode);
  g_node_append(rootnode, contactsnode);

  /* walk contacts cache */
  for(walk = contacts_cache.contacts; walk; walk = walk->next)
  {
    GNode *contactnode;
    Contact *contact = walk->data;
    tag = xml_tag_new("contact");
    xml_tag_add_attr(tag, xml_attr_new("family_name",contact->family_name));
    xml_tag_add_attr(tag, xml_attr_new("given_name",contact->given_name));
    xml_tag_add_attr(tag, xml_attr_new("full_name",contact->full_name));
    xml_tag_add_attr(tag, xml_attr_new("address",contact->address));
    xmlnode = xml_node_new(tag, NULL);
    contactnode = g_node_new(xmlnode);
    g_node_append(contactsnode, contactnode);
  }

  /* Actual writing and cleanup */
  xml_write_tree(rootnode, pfile->fp);
  if (prefs_file_close(pfile) < 0)
    debug_print("GData plugin error: Failed to write file " GDATA_CONTACTS_FILENAME "\n");
  else
    debug_print("GData plugin: Wrote cache to file " GDATA_CONTACTS_FILENAME "\n");

  /* Free XML tree */
  xml_free_tree(rootnode);
}

static int add_gdata_contact_to_cache(GDataContactsContact *contact)
{
  GList *walk;
  int retval;

  retval = 0;
  for(walk = gdata_contacts_contact_get_email_addresses(contact); walk; walk = walk->next) {
    const gchar *email_address;
    GDataGDEmailAddress *address = GDATA_GD_EMAIL_ADDRESS(walk->data);

    email_address = gdata_gd_email_address_get_address(address);
    if(email_address && (*email_address != '\0')) {
      GDataGDName *name;
      Contact *cached_contact;

      name = gdata_contacts_contact_get_name(contact);

      cached_contact = g_new0(Contact, 1);
      cached_contact->full_name = g_strdup(gdata_gd_name_get_full_name(name));
      cached_contact->given_name = g_strdup(gdata_gd_name_get_given_name(name));
      cached_contact->family_name = g_strdup(gdata_gd_name_get_family_name(name));
      cached_contact->address = g_strdup(email_address);

      protect_fields_against_NULL(cached_contact);

      contacts_cache.contacts = g_slist_prepend(contacts_cache.contacts, cached_contact);

      debug_print("GData plugin: Added %s <%s>\n", cached_contact->full_name, cached_contact->address);
      retval = 1;
    }
  }
  if(retval == 0)
  {
    debug_print("GData plugin: Skipped received contact \"%s\" because it doesn't have an email address\n",
        gdata_gd_name_get_full_name(gdata_contacts_contact_get_name(contact)));
  }
  return retval;
}

static void free_contact(Contact *contact)
{
  g_free((gpointer)contact->full_name);
  g_free((gpointer)contact->family_name);
  g_free((gpointer)contact->given_name);
  g_free((gpointer)contact->address);
  g_free(contact);
}

static void clear_contacts_cache(void)
{
  GSList *walk;
  for(walk = contacts_cache.contacts; walk; walk = walk->next)
    free_contact(walk->data);
  g_slist_free(contacts_cache.contacts);
  contacts_cache.contacts = NULL;
}

static void cm_gdata_query_contacts_ready(GDataContactsService *service, GAsyncResult *res, gpointer data)
{
  GDataFeed *feed;
  GList *walk;
  GError *error = NULL;
  guint num_contacts = 0;
  guint num_contacts_added = 0;
  gchar *tmpstr1, *tmpstr2;

  feed = gdata_service_query_finish(GDATA_SERVICE(service), res, &error);
  cm_gdata_contacts_query_running = FALSE;
  if(error)
  {
    g_object_unref(feed);
    log_error(LOG_PROTOCOL, _("GData plugin: Error querying for contacts: %s\n"), error->message);
    g_error_free(error);
    return;
  }

  /* clear cache */
  clear_contacts_cache();

  /* Iterate through the returned contacts and fill the cache */
  for(walk = gdata_feed_get_entries(feed); walk; walk = walk->next) {
    num_contacts_added += add_gdata_contact_to_cache(GDATA_CONTACTS_CONTACT(walk->data));
    num_contacts++;
  }
  g_object_unref(feed);
  contacts_cache.contacts = g_slist_reverse(contacts_cache.contacts);
  /* TRANSLATORS: First part of "Added X of Y contacts to cache" */
  tmpstr1 = g_strdup_printf(ngettext("Added %d of", "Added %d of", num_contacts_added), num_contacts_added);
  /* TRANSLATORS: Second part of "Added X of Y contacts to cache" */
  tmpstr2 = g_strdup_printf(ngettext("1 contact to the cache", "%d contacts to the cache", num_contacts), num_contacts);
  log_message(LOG_PROTOCOL, "%s %s\n", tmpstr1, tmpstr2);
  g_free(tmpstr1);
  g_free(tmpstr2);
}

static void query_contacts(GDataContactsService *service)
{
  GDataContactsQuery *query;

  log_message(LOG_PROTOCOL, _("GData plugin: Starting async contacts query\n"));

  query = gdata_contacts_query_new(NULL);
  gdata_contacts_query_set_group(query, contacts_group_id);
  gdata_query_set_max_results(GDATA_QUERY(query), cm_gdata_config.max_num_results);
  gdata_contacts_service_query_contacts_async(service, GDATA_QUERY(query), NULL, NULL, NULL,
  NULL, (GAsyncReadyCallback)cm_gdata_query_contacts_ready, NULL);

  g_object_unref(query);
}

static void cm_gdata_query_groups_ready(GDataContactsService *service, GAsyncResult *res, gpointer data)
{
  GDataFeed *feed;
  GList *walk;
  GError *error = NULL;

  feed = gdata_service_query_finish(GDATA_SERVICE(service), res, &error);
  if(error)
  {
    g_object_unref(feed);
    log_error(LOG_PROTOCOL, _("GData plugin: Error querying for groups: %s\n"), error->message);
    g_error_free(error);
    return;
  }

  /* Iterate through the returned groups and search for Contacts group id */
  for(walk = gdata_feed_get_entries(feed); walk; walk = walk->next) {
    const gchar *system_group_id;
    GDataContactsGroup *group = GDATA_CONTACTS_GROUP(walk->data);

    system_group_id = gdata_contacts_group_get_system_group_id(group);
    if(system_group_id && !strcmp(system_group_id, GDATA_CONTACTS_GROUP_CONTACTS)) {
      gchar *pos;
      const gchar *id;

      id = gdata_entry_get_id(GDATA_ENTRY(group));

      /* possibly replace projection "full" by "base" */
      pos = g_strrstr(id, "/full/");
      if(pos) {
        GString *str = g_string_new("\0");
        int off = pos-id;

        g_string_append_len(str, id, off);
        g_string_append(str, "/base/");
        g_string_append(str, id+off+strlen("/full/"));
        g_string_append_c(str, '\0');
        contacts_group_id = str->str;
        g_string_free(str, FALSE);
      }
      else
        contacts_group_id = g_strdup(id);
      break;
    }
  }
  g_object_unref(feed);

  log_message(LOG_PROTOCOL, _("GData plugin: Groups received\n"));

  query_contacts(service);
}

static void query_for_contacts_group_id(GDataAuthorizer *authorizer)
{
  log_message(LOG_PROTOCOL, _("GData plugin: Starting async groups query\n"));

  gdata_contacts_service_query_groups_async(service, NULL, NULL, NULL, NULL, NULL,
      (GAsyncReadyCallback)cm_gdata_query_groups_ready, NULL);
}


static void query_after_auth()
{
  if(!contacts_group_id)
    query_for_contacts_group_id(GDATA_AUTHORIZER(authorizer));
  else
    query_contacts(service);
}


static void cm_gdata_auth_ready(GDataOAuth2Authorizer *auth, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;

  if(gdata_oauth2_authorizer_request_authorization_finish(auth, res, &error) == FALSE)
  {
    /* Notify the user of all errors except cancellation errors */
    if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      log_error(LOG_PROTOCOL, _("GData plugin: Authorization error: %s\n"), error->message);
    }
    g_error_free(error);
    cm_gdata_contacts_query_running = FALSE;
    return;
  }

  log_message(LOG_PROTOCOL, _("GData plugin: Authorization successful\n"));

  query_after_auth();
}

static void cm_gdata_interactive_auth()
{
  static gboolean interactive_auth_running = FALSE;

  gchar *auth_uri;

  auth_uri = gdata_oauth2_authorizer_build_authentication_uri(authorizer, cm_gdata_config.username, FALSE);
  g_return_if_fail(auth_uri);

  if(!interactive_auth_running)
  {
    gchar *auth_code;

    interactive_auth_running = TRUE;

    log_message(LOG_PROTOCOL, _("GData plugin: Starting interactive authorization\n"));

    auth_code = ask_user_for_auth_code(auth_uri);

    if(auth_code)
    {
      cm_gdata_contacts_query_running = TRUE;
      log_message(LOG_PROTOCOL, _("GData plugin: Got authorization code, requesting authorization\n"));
      gdata_oauth2_authorizer_request_authorization_async(authorizer, auth_code, NULL, (GAsyncReadyCallback)cm_gdata_auth_ready, NULL);
      memset(auth_code, 0, strlen(auth_code));
      g_free(auth_code);
    }
    else
    {
      log_warning(LOG_PROTOCOL, _("GData plugin: No authorization code received, authorization request cancelled\n"));
    }
    interactive_auth_running = FALSE;
  }
  else
  {
    log_message(LOG_PROTOCOL, _("GData plugin: Interactive authorization still running, no additional session started\n"));
  }

  g_free(auth_uri);
}


static void cm_gdata_refresh_ready(GDataOAuth2Authorizer *auth, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;
  gboolean start_interactive_auth = FALSE;

  if(gdata_authorizer_refresh_authorization_finish(GDATA_AUTHORIZER(auth), res, &error) == FALSE)
  {
    /* Notify the user of all errors except cancellation errors */
    if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      log_error(LOG_PROTOCOL, _("GData plugin: Authorization refresh error: %s\n"), error->message);

      if(mainwindow_get_mainwindow())
      {
        mainwindow_show_error();
      }
    }

    /* Only start an interactive auth session in case of authorization issues, but not
     * for e.g. sporadic network issues or other non-authorization-related problems. */
    start_interactive_auth =
        g_error_matches(error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED) ||
        g_error_matches(error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_FORBIDDEN);

    g_error_free(error);

    if(start_interactive_auth)
    {
      cm_gdata_interactive_auth();
    }

    return;
  }

  log_message(LOG_PROTOCOL, _("GData plugin: Authorization refresh successful\n"));

  g_timer_start(refresh_timer);

  query_after_auth();
}


/* returns allocated string which must be freed */
static guchar* decode(const gchar *in)
{
  guchar *tmp;
  gsize len;

  tmp = g_base64_decode(in, &len);
  passcrypt_decrypt(tmp, len);
  return tmp;
}


static void query()
{
  gchar *token;
  int elapsed_time_min;

  if(cm_gdata_contacts_query_running)
  {
    debug_print("GData plugin: Network query already in progress\n");
    return;
  }

  if(!authorizer)
  {
    gchar *c1 = decode(GDATA_C1);
    gchar *c2 = decode(GDATA_C2);
    gchar *c3 = decode(GDATA_C3);

    authorizer = gdata_oauth2_authorizer_new(c1, c2, c3, GDATA_TYPE_CONTACTS_SERVICE);

    g_free(c1);
    g_free(c2);
    g_free(c3);
  }
  g_return_if_fail(authorizer);

  if(!service)
  {
    service = gdata_contacts_service_new(GDATA_AUTHORIZER(authorizer));
  }
  g_return_if_fail(service);

  if(!refresh_timer)
  {
    refresh_timer = g_timer_new();
  }
  g_return_if_fail(refresh_timer);

  elapsed_time_min = (int)((g_timer_elapsed(refresh_timer, NULL)/60.0)+0.5);
  if(elapsed_time_min > REFRESH_TIMEOUT_MINUTES)
  {
    log_message(LOG_PROTOCOL, _("GData plugin: Elapsed time since last refresh: %d minutes, refreshing now\n"), elapsed_time_min);
    gdata_authorizer_refresh_authorization_async(GDATA_AUTHORIZER(authorizer), NULL, (GAsyncReadyCallback)cm_gdata_refresh_ready, NULL);
  }
  else if(!gdata_service_is_authorized(GDATA_SERVICE(service)))
  {
    /* Try to restore from saved refresh token.*/
    if((token = passwd_store_get(PWS_PLUGIN, "GData", GDATA_TOKEN_PWD_STRING)) != NULL)
    {
      log_message(LOG_PROTOCOL, _("GData plugin: Trying to refresh authorization\n"));
      gdata_oauth2_authorizer_set_refresh_token(authorizer, token);
      memset(token, 0, strlen(token));
      g_free(token);
      gdata_authorizer_refresh_authorization_async(GDATA_AUTHORIZER(authorizer), NULL, (GAsyncReadyCallback)cm_gdata_refresh_ready, NULL);
    }
    else
    {
      cm_gdata_interactive_auth();
    }
  }
  else
  {
    query_after_auth();
  }
}


static void add_contacts_to_list(GList **address_list, GSList *contacts)
{
  GSList *walk;

  for(walk = contacts; walk; walk = walk->next)
  {
    address_entry *ae;
    Contact *contact = walk->data;

    ae = g_new0(address_entry, 1);
    ae->name = g_strdup(contact->full_name);
    ae->address = g_strdup(contact->address);
    ae->grp_emails = NULL;

    *address_list = g_list_prepend(*address_list, ae);
    addr_compl_add_address1(ae->address, ae);

    if(contact->given_name && *(contact->given_name) != '\0')
      addr_compl_add_address1(contact->given_name, ae);

    if(contact->family_name && *(contact->family_name) != '\0')
      addr_compl_add_address1(contact->family_name, ae);
  }
}

void cm_gdata_add_contacts(GList **address_list)
{
  add_contacts_to_list(address_list, contacts_cache.contacts);
}

gboolean cm_gdata_update_contacts_cache(void)
{
  if(prefs_common_get_prefs()->work_offline)
  {
    debug_print("GData plugin: Offline mode\n");
  }
  else
  {
    debug_print("GData plugin: Querying contacts\n");
    query();
  }
  return TRUE;
}

void cm_gdata_contacts_done(void)
{
  gchar *pass;

  g_free(contacts_group_id);
  contacts_group_id = NULL;

  write_cache_to_file();
  if(contacts_cache.contacts && !claws_is_exiting())
    clear_contacts_cache();

  if(authorizer)
  {
    /* store refresh token */
    pass = gdata_oauth2_authorizer_dup_refresh_token(authorizer);
    passwd_store_set(PWS_PLUGIN, "GData", GDATA_TOKEN_PWD_STRING, pass, FALSE);
    if (pass != NULL) {
      memset(pass, 0, strlen(pass));
      g_free(pass);
    }
    passwd_store_write_config();

    g_object_unref(G_OBJECT(authorizer));
    authorizer = NULL;
  }

  if(service)
  {
    g_object_unref(G_OBJECT(service));
    service = NULL;
  }

  if(refresh_timer)
  {
    g_timer_destroy(refresh_timer);
    refresh_timer = NULL;
  }
}

void cm_gdata_load_contacts_cache_from_file(void)
{
  gchar *path;
  GNode *rootnode, *childnode, *contactnode;
  XMLNode *xmlnode;

  path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, GDATA_CONTACTS_FILENAME, NULL);
  if(!is_file_exist(path)) {
    g_free(path);
    return;
  }

  /* no merging; make sure the cache is empty (this should be a noop, but just to be safe...) */
  clear_contacts_cache();

  rootnode = xml_parse_file(path);
  g_free(path);
  if(!rootnode)
    return;
  xmlnode = rootnode->data;

  /* Check that root entry is "gdata" */
  if(g_strcmp0(xmlnode->tag->tag, "gdata") != 0) {
    g_warning("wrong gdata cache file");
    xml_free_tree(rootnode);
    return;
  }

  for(childnode = rootnode->children; childnode; childnode = childnode->next) {
    GList *attributes;
    xmlnode = childnode->data;

    if(g_strcmp0(xmlnode->tag->tag, "contacts") != 0)
      continue;

    for(contactnode = childnode->children; contactnode; contactnode = contactnode->next)
    {
      Contact *cached_contact;

      xmlnode = contactnode->data;

      cached_contact = g_new0(Contact, 1);
      /* Attributes of the branch nodes */
      for(attributes = xmlnode->tag->attr; attributes; attributes = attributes->next) {
        XMLAttr *attr = attributes->data;

        if(attr && attr->name && attr->value) {
          if(!g_strcmp0(attr->name, "full_name"))
            cached_contact->full_name = g_strdup(attr->value);
          else if(!g_strcmp0(attr->name, "given_name"))
            cached_contact->given_name = g_strdup(attr->value);
          else if(!g_strcmp0(attr->name, "family_name"))
            cached_contact->family_name = g_strdup(attr->value);
          else if(!g_strcmp0(attr->name, "address"))
            cached_contact->address = g_strdup(attr->value);
        }
      }

      if(cached_contact->address)
      {
        protect_fields_against_NULL(cached_contact);

        contacts_cache.contacts = g_slist_prepend(contacts_cache.contacts, cached_contact);
        debug_print("Read contact from cache: %s\n", cached_contact->full_name);
      }
      else {
        debug_print("Ignored contact without email address: %s\n", cached_contact->full_name ? cached_contact->full_name : "(null)");
        /* Not added to list: return allocated memory */
        if (cached_contact->full_name)
            g_free(cached_contact->full_name);
        if (cached_contact->given_name)
            g_free(cached_contact->given_name);
        if (cached_contact->family_name)
            g_free(cached_contact->family_name);
        if (cached_contact->address)
            g_free(cached_contact->address);
        g_free(cached_contact);
      }
    }
  }

  /* Free XML tree */
  xml_free_tree(rootnode);

  contacts_cache.contacts = g_slist_reverse(contacts_cache.contacts);
}
