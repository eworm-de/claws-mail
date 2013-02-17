/* GData plugin for Claws-Mail
 * Copyright (C) 2011 Holger Berndt
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

#include "cm_gdata_contacts.h"
#include "cm_gdata_prefs.h"

#include <gtk/gtk.h>
#include "addr_compl.h"
#include "main.h"
#include "prefs_common.h"
#include "common/log.h"
#include "common/xml.h"

#include <gdata/gdata.h>

#define GDATA_CONTACTS_FILENAME "gdata_cache.xml"

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


CmGDataContactsCache contacts_cache;
gboolean cm_gdata_contacts_query_running = FALSE;
gchar *contacts_group_id = NULL;

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
  if(prefs_file_close(pfile) < 0)
    debug_print("GData plugin error: Failed to write file " GDATA_CONTACTS_FILENAME "\n");

  debug_print("GData plugin error: Wrote cache to file " GDATA_CONTACTS_FILENAME "\n");

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
	/* i18n: First part of "Added X of Y contacts to cache" */
  tmpstr1 = g_strdup_printf(ngettext("Added %d of", "Added %d of", num_contacts_added), num_contacts_added);
	/* i18n: Second part of "Added X of Y contacts to cache" */
  tmpstr2 = g_strdup_printf(ngettext("1 contact to the cache", "%d contacts to the cache", num_contacts), num_contacts);
  log_message(LOG_PROTOCOL, "%s %s\n", tmpstr1, tmpstr2);
	g_free(tmpstr1);
	g_free(tmpstr2);
}

static void query_after_auth(GDataContactsService *service)
{
  GDataContactsQuery *query;

  log_message(LOG_PROTOCOL, _("GData plugin: Starting async contacts query\n"));

  query = gdata_contacts_query_new(NULL);
  gdata_contacts_query_set_group(query, contacts_group_id);
  gdata_query_set_max_results(GDATA_QUERY(query), cm_gdata_config.max_num_results);
  gdata_contacts_service_query_contacts_async(service, GDATA_QUERY(query), NULL, NULL, NULL,
#ifdef HAVE_GDATA_VERSION_0_9_1
  NULL,
#endif
  (GAsyncReadyCallback)cm_gdata_query_contacts_ready, NULL);

  g_object_unref(query);
}

#ifdef HAVE_GDATA_VERSION_0_9_1
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

  query_after_auth(service);
}
#endif

#ifdef HAVE_GDATA_VERSION_0_9
static void query_for_contacts_group_id(GDataClientLoginAuthorizer *authorizer)
{
  GDataContactsService *service;
#ifdef HAVE_GDATA_VERSION_0_9_1

  log_message(LOG_PROTOCOL, _("GData plugin: Starting async groups query\n"));

  service = gdata_contacts_service_new(GDATA_AUTHORIZER(authorizer));
  gdata_contacts_service_query_groups_async(service, NULL, NULL, NULL, NULL, NULL,
      (GAsyncReadyCallback)cm_gdata_query_groups_ready, NULL);
#else
  service = gdata_contacts_service_new(GDATA_AUTHORIZER(authorizer));
  query_after_auth(service);
#endif
  g_object_unref(service);
}

static void cm_gdata_auth_ready(GDataClientLoginAuthorizer *authorizer, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;

  if(gdata_client_login_authorizer_authenticate_finish(authorizer, res, &error) == FALSE)
  {
    log_error(LOG_PROTOCOL, _("GData plugin: Authentication error: %s\n"), error->message);
    g_error_free(error);
    cm_gdata_contacts_query_running = FALSE;
    return;
  }

  log_message(LOG_PROTOCOL, _("GData plugin: Authenticated\n"));

  if(!contacts_group_id)
  {
    query_for_contacts_group_id(authorizer);
  }
  else {
    GDataContactsService *service;
    service = gdata_contacts_service_new(GDATA_AUTHORIZER(authorizer));
    query_after_auth(service);
    g_object_unref(service);
  }
}
#else
static void cm_gdata_auth_ready(GDataContactsService *service, GAsyncResult *res, gpointer data)
{
  GError *error = NULL;

  if(!gdata_service_authenticate_finish(GDATA_SERVICE(service), res, &error))
  {
    log_error(LOG_PROTOCOL, _("GData plugin: Authentication error: %s\n"), error->message);
    g_error_free(error);
    cm_gdata_contacts_query_running = FALSE;
    return;
  }

  log_message(LOG_PROTOCOL, _("GData plugin: Authenticated\n"));

  query_after_auth(service);
}
#endif
static void query()
{

#ifdef HAVE_GDATA_VERSION_0_9
  GDataClientLoginAuthorizer *authorizer;
#else
  GDataContactsService *service;
#endif

  if(cm_gdata_contacts_query_running)
  {
    debug_print("GData plugin: Network query already in progress");
    return;
  }

  log_message(LOG_PROTOCOL, _("GData plugin: Starting async authentication\n"));

#ifdef HAVE_GDATA_VERSION_0_9
  authorizer = gdata_client_login_authorizer_new(CM_GDATA_CLIENT_ID, GDATA_TYPE_CONTACTS_SERVICE);
  gdata_client_login_authorizer_authenticate_async(authorizer, cm_gdata_config.username, cm_gdata_config.password, NULL, (GAsyncReadyCallback)cm_gdata_auth_ready, NULL);
  cm_gdata_contacts_query_running = TRUE;
#else
  service = gdata_contacts_service_new(CM_GDATA_CLIENT_ID);
  cm_gdata_contacts_query_running = TRUE;
  gdata_service_authenticate_async(GDATA_SERVICE(service), cm_gdata_config.username, cm_gdata_config.password, NULL,
      (GAsyncReadyCallback)cm_gdata_auth_ready, NULL);
#endif


#ifdef HAVE_GDATA_VERSION_0_9
  g_object_unref(authorizer);
#else
  g_object_unref(service);
#endif

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
  if(prefs_common.work_offline)
  {
    debug_print("GData plugin: Offline mode\n");
  }
  else if(!cm_gdata_config.username || *(cm_gdata_config.username) == '\0' || !cm_gdata_config.password)
  {
    /* noop if no credentials are given */
    debug_print("GData plugin: Empty username or password\n");
  }
  else
  {
    debug_print("GData plugin: Querying contacts");
    query();
  }
  return TRUE;
}

void cm_gdata_contacts_done(void)
{
  g_free(contacts_group_id);
  contacts_group_id = NULL;

  write_cache_to_file();
  if(contacts_cache.contacts && !claws_is_exiting())
    clear_contacts_cache();
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
  if(strcmp2(xmlnode->tag->tag, "gdata") != 0) {
    g_warning("wrong gdata cache file\n");
    xml_free_tree(rootnode);
    return;
  }

  for(childnode = rootnode->children; childnode; childnode = childnode->next) {
    GList *attributes;
    xmlnode = childnode->data;

    if(strcmp2(xmlnode->tag->tag, "contacts") != 0)
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
          if(!strcmp2(attr->name, "full_name"))
            cached_contact->full_name = g_strdup(attr->value);
          else if(!strcmp2(attr->name, "given_name"))
            cached_contact->given_name = g_strdup(attr->value);
          else if(!strcmp2(attr->name, "family_name"))
            cached_contact->family_name = g_strdup(attr->value);
          else if(!strcmp2(attr->name, "address"))
            cached_contact->address = g_strdup(attr->value);
        }
      }
      contacts_cache.contacts = g_slist_prepend(contacts_cache.contacts, cached_contact);
      debug_print("Read contact from cache: %s\n", cached_contact->full_name);
    }
  }

  /* Free XML tree */
  xml_free_tree(rootnode);

  contacts_cache.contacts = g_slist_reverse(contacts_cache.contacts);
}
