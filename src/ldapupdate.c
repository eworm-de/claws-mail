/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2007 Michael Rasmussen and the Claws Mail team
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

/*
 * Functions necessary to access LDAP servers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_LDAP

#include <glib.h>
#include <glib/gi18n.h>
#include <sys/time.h>
#include <string.h>
#include <ldap.h>
#include <lber.h>

#include "ldapupdate.h"
#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "ldapctrl.h"
#include "ldapquery.h"
#include "ldapserver.h"
#include "ldaputil.h"
#include "utils.h"
#include "adbookbase.h"

/**
 * Structure to hold user defined attributes
 * from contacts
 */
typedef struct _AttrKeyValue AttrKeyValue;
struct _AttrKeyValue {
	gchar *key;
	gchar *value;
};

/**
 * Structure to hold contact information.
 * Each addressbook will have 0..N contacts.
 */
typedef struct _EmailKeyValue EmailKeyValue;
struct _EmailKeyValue {
	gchar *mail;
	gchar *alias;
	gchar *remarks;
};

/**
 * Structure to hold information about RDN.
 */
typedef struct _Rdn Rdn;
struct _Rdn {
	gchar *attribute;
	gchar *value;
	gchar *new_dn;
};

/**
 * Retrieve address group item for update.
 * \param group  Group to print.
 * \param array  GHashTAble of item_group, or <i>NULL</i> if none created.
 */
void ldapsvr_retrieve_item_group(ItemGroup *group, GHashTable *array) {
	/* Not implemented in this release */
	g_return_if_fail(group != NULL);
}

/**
 * Create an initial EmailKeyValue structure
 * \return empty structure
 */
EmailKeyValue *emailkeyvalue_create() {
	EmailKeyValue *buf;

	buf = g_new0(EmailKeyValue, 1);
	buf->alias = NULL;
	buf->mail = NULL;
	buf->remarks = NULL;
	return buf;
}

/**
 * Create an initial AttrKeyValue structure
 * \return empty structure
 */
AttrKeyValue *attrkeyvalue_create() {
	AttrKeyValue *buf;

	buf = g_new0(AttrKeyValue, 1);
	buf->key = NULL;
	buf->value = NULL;
	return buf;
}

/**
 * Retrieve E-Mail address object for update.
 * \param item   ItemEmail to update.
 * \return object, or <i>NULL</i> if none created.
 */
EmailKeyValue *ldapsvr_retrieve_item_email(ItemEMail *item) {
	EmailKeyValue *newItem;
	g_return_val_if_fail(item != NULL, NULL);
	newItem = emailkeyvalue_create();		
	newItem->alias = g_strdup(ADDRITEM_NAME(item));
	newItem->mail = g_strdup(item->address);
	newItem->remarks = g_strdup(item->remarks);
	return newItem;
}

/**
 * Retrieve user attribute object for update.
 * \param item   UserAttribute to update.
 * \return object, or <i>NULL</i> if none created.
 */
AttrKeyValue *ldapsvr_retrieve_attribute(UserAttribute *item) {
	AttrKeyValue *newItem;
	g_return_val_if_fail(item != NULL, NULL);
	newItem = attrkeyvalue_create();
	newItem->key = g_strdup(item->name);
	newItem->value = g_strdup(item->value);
	return newItem;
}

/**
 * Retrieve person object for update.
 * \param person ItemPerson to update.
 * \param array GHashTable with user input.
 * \return false if update is not needed, or true if update is needed.
 */
gboolean ldapsvr_retrieve_item_person(ItemPerson *person, GHashTable *array) {
	GList *node, *attr;

	g_return_val_if_fail(person != NULL, FALSE);
	switch (person->status) {
		case NONE: return FALSE;
		case ADD_ENTRY: g_hash_table_insert(array, "status", "new"); break;
		case UPDATE_ENTRY: g_hash_table_insert(array, "status", "update"); break;
		case DELETE_ENTRY: g_hash_table_insert(array, "status", "delete"); break;
		default: g_critical(_("ldapsvr_retrieve_item_person->Unknown status: %d"), person->status);
	}
	g_hash_table_insert(array, "uid", ADDRITEM_ID(person));
	g_hash_table_insert(array, "cn", ADDRITEM_NAME(person));
	g_hash_table_insert(array, "givenName", person->firstName);
	g_hash_table_insert(array, "sn", person->lastName);
	g_hash_table_insert(array, "nickName", person->nickName);
	g_hash_table_insert(array, "dn", person->externalID);
	node = person->listEMail;
	attr = NULL;
	while (node) {
		EmailKeyValue *newEmail = ldapsvr_retrieve_item_email(node->data);
		if (newEmail)
			attr = g_list_append(attr, newEmail);
		node = g_list_next(node);
	}
	g_hash_table_insert(array, "mail", attr);
/* Not implemented in this release.
	node = person->listAttrib;
	attr = NULL;
	while (node) {
		AttrKeyValue *newAttr = ldapsvr_retrieve_attribute(node->data)
		if (newAttr)
			attr = g_list_append(attr, newAttr);
		node = g_list_next(node);
	}
	g_hash_table_insert(array, "attribute", attr);
*/
	return TRUE;
}

/**
 * Print contents of contacts hashtable for debug.
 * This function must be called with g_hash_table_foreach.
 * \param key Key to process.
 * \param data Data to process.
 * \param fd Output stream.
 */
void ldapsvr_print_contacts_hashtable(gpointer key, gpointer data, gpointer fd) {
	gchar *keyName = (gchar *) key;
	GList *node;

	if (g_ascii_strcasecmp("mail", keyName) == 0) {
		node = (GList *) data;
		while (node) {
			EmailKeyValue *item = node->data;
			if (debug_get_mode()) {
				debug_print("\t\talias = %s\n", item->alias);
				debug_print("\t\tmail = %s\n", item->mail);
				debug_print("\t\tremarks = %s\n", item->remarks);
			}
			else if (fd) {
				FILE *stream = (FILE *) fd;
				fprintf(stream, "\t\talias = %s\n", item->alias);
				fprintf(stream, "\t\tmail = %s\n", item->mail);
				fprintf(stream, "\t\tremarks = %s\n", item->remarks);
			}
			node = g_list_next(node);
		}
	}
	else if (g_ascii_strcasecmp("attribute", keyName) == 0) {
		node = (GList *) data;
		while (node) {
			AttrKeyValue *item = node->data;
			if (debug_get_mode()) {
				debug_print("\t\t%s = %s\n", item->key, item->value);
			}
			else if (fd) {
				FILE *stream = (FILE *) fd;
				fprintf(stream, "\t\t%s = %s\n", item->key, item->value);
			}
			node = g_list_next(node);
		}
	}
	else {
		if (debug_get_mode())
			debug_print("\t\t%s = %s\n", keyName, (gchar *) data);
		else if (fd) {
			FILE *stream = (FILE *) fd;
			fprintf(stream, "\t\t%s = %s\n", keyName, (gchar *) data);
		}
	}
}

/**
 * Free list of changed contacts
 *
 * \param list List of GHashTable
 */
void ldapsvr_free_hashtable(GList *list) {
	while (list) {
		g_hash_table_destroy(list->data);
		list = g_list_next(list);
	}
	g_list_free(list);
}

/**
 * Get person object from cache
 *
 * \param server Resource to LDAP
 * \param uid PersonID in cache
 * \return person object, or <i>NULL</i> if fail
 */
ItemPerson *ldapsvr_get_contact(LdapServer *server, gchar *uid) {
	AddrItemObject *aio;
	g_return_val_if_fail(server != NULL || uid != NULL, NULL);
	aio = addrcache_get_object(server->addressCache, uid);
	if (aio) {
		if(aio->type == ITEMTYPE_PERSON) {
			return (ItemPerson *) aio;
		}
	}
	return NULL;
}

/**
 * Connect to LDAP server.
 * \param  ctl Control object to process.
 * \return LDAP Resource to LDAP.
 */
LDAP *ldapsvr_connect(LdapControl *ctl) {
	LDAP *ld = NULL;
	gint rc;
	gint version;
	gchar *uri = NULL;

	g_return_val_if_fail(ctl != NULL, NULL);

	uri = g_strdup_printf("ldap%s://%s:%d",
				ctl->enableSSL?"s":"",
				ctl->hostName, ctl->port);
	ldap_initialize(&ld, uri);
	g_free(uri);

	if (ld == NULL)
		return NULL;

	debug_print("connected to LDAP host %s on port %d\n", ctl->hostName, ctl->port);

#ifdef USE_LDAP_TLS
	/* Handle TLS */
	version = LDAP_VERSION3;
	rc = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (rc == LDAP_OPT_SUCCESS) {
		ctl->version = LDAP_VERSION3;
	}

	if (ctl->version == LDAP_VERSION3) {
		if (ctl->enableTLS && !ctl->enableSSL) {
			rc = ldap_start_tls_s(ld, NULL, NULL);
			
			if (rc != LDAP_SUCCESS) {
				fprintf(stderr, "LDAP Error(tls): ldap_simple_bind_s: %s\n",
					ldap_err2string(rc));
				return NULL;
			}
		}
	}
#endif

	/* Bind to the server, if required */
	if (ctl->bindDN) {
		if (* ctl->bindDN != '\0') {
			rc = claws_ldap_simple_bind_s(ld, ctl->bindDN, ctl->bindPass);
			if (rc != LDAP_SUCCESS) {
				fprintf(stderr, "bindDN: %s, bindPass: %s\n", ctl->bindDN, ctl->bindPass);
				fprintf(stderr, "LDAP Error(bind): ldap_simple_bind_s: %s\n",
					ldap_err2string(rc));
				return NULL;
			}
		}
	}
	return ld;
}

/**
 * Disconnect to LDAP server.
 * \param ld Resource to LDAP.
 */
void ldapsvr_disconnect(LDAP *ld) {
	/* Disconnect */
	g_return_if_fail(ld != NULL);
	ldap_unbind_ext(ld, NULL, NULL);
}

/**
 * Create an initial Rdn structure
 *
 * \return empty structure
 */
Rdn *rdn_create() {
	Rdn *buf;

	buf = g_new0(Rdn, 1);
	buf->attribute = NULL;
	buf->value = NULL;
	buf->new_dn = NULL;
	return buf;
}

/**
 * update Rdn structure
 *
 * \param rdn Rdn structure to update
 * \param head Uniq part of dn
 * \param tail Common part of dn
 */
void update_rdn(Rdn *rdn, gchar *head, gchar *tail) {
	rdn->value = g_strdup(head);
	rdn->new_dn = g_strdup_printf("mail=%s%s", head, tail);
}

/**
 * Deside if dn needs to be changed
 *
 * \param hash GHashTable with user input.
 * \param dn dn for current object
 * \return Rdn structure
 */
Rdn *ldapsvr_modify_dn(GHashTable *hash, gchar *dn) {
	Rdn *rdn;
	gchar *pos, *compare;
	gchar *rest;
	gchar *val;
	g_return_val_if_fail(hash != NULL || dn != NULL, NULL);
	
	pos = g_strstr_len(dn, strlen(dn), "=");
	compare = g_strndup(dn, pos - dn);

	if (!pos)
		return NULL;
	pos++;
	rest = g_strstr_len(pos, strlen(pos), ",");
	val = g_strndup(pos, rest - pos);
	if (val == NULL) {
		if (compare)
			g_free(compare);
		return NULL;
	}
	rdn = rdn_create();
	rdn->value = g_strdup(val);
	rdn->attribute = g_strdup(compare);
	g_free(val);
	if (strcmp("mail", rdn->attribute) == 0) {
		GList *list = g_hash_table_lookup(hash, rdn->attribute);
		while (list) {
			EmailKeyValue *item = list->data;
			compare = g_strdup((gchar *) item->mail);
			if (strcmp(compare, rdn->value) == 0) {
				update_rdn(rdn, compare, rest);
				g_free(compare);
				return rdn;
			}
			list = g_list_next(list);
		}
		/* if compare and rdn->attribute are equal then last email removed/empty  */
		if (strcmp(compare, rdn->attribute) != 0) {
	 		/* RDN changed. Find new */
			update_rdn(rdn, compare, rest);
			g_free(compare);
			return rdn;
		}
		else {
			/* We cannot remove dn */
			g_free(compare);
			return NULL;
		}
	}
	else {
		compare = g_hash_table_lookup(hash, rdn->attribute);
		/* if compare and rdn->attribute are equal then dn removed/empty */
		if (strcmp(compare, rdn->attribute) != 0) {
			update_rdn(rdn, compare, rest);
			return rdn;
		}
		else {
			/* We cannot remove dn */
			return NULL;
		}
	}
	return NULL;
}

/**
 * This macro is borrowed from the Balsa project
 * Creates a LDAPMod structure
 *
 * \param mods Empty LDAPMod structure
 * \param modarr Array with values to insert
 * \param op Operation to perform on LDAP
 * \param attr Attribute to insert
 * \param strv Empty array which is NULL terminated
 * \param val Value for attribute
 */
#define SETMOD(mods,modarr,op,attr,strv,val) \
   do { (mods) = &(modarr); (modarr).mod_type=attr; (modarr).mod_op=op;\
        (strv)[0]=(val); (modarr).mod_values=strv; \
      } while(0)

/**
 * Creates a LDAPMod structure
 *
 * \param mods Empty LDAPMod structure
 * \param modarr Array with values to insert
 * \param op Operation to perform on LDAP
 * \param attr Attribute to insert
 * \param strv Array with values to insert. Must be NULL terminated
 */
#define SETMODS(mods,modarr,op,attr,strv) \
   do { (mods) = &(modarr); (modarr).mod_type=attr; \
	   	(modarr).mod_op=op; (modarr).mod_values=strv; \
      } while(0)
#define MODSIZE 10

/**
 * Clean up, close LDAP connection, and refresh cache
 *
 * \param ld Resource to LDAP
 * \param server AddressBook resource
 * \param contact GHashTable with current changed object
 */
void clean_up(LDAP *ld, LdapServer *server, GHashTable *contact) {
	ItemPerson *person = 
		ldapsvr_get_contact(server, g_hash_table_lookup(contact , "uid"));
	if (person) {
		gchar *displayName;
		person->status = NONE;
		displayName = g_hash_table_lookup(contact, "displayName");
		if (displayName)
			person->nickName = g_strdup(displayName);
	}
	if (ld)
		ldapsvr_disconnect(ld);
	ldapsvr_force_refresh(server);
/*	ldapsvr_free_all_query(server);*/
}

/**
 * Get cn attribute from dn
 *
 * \param dn Distinguesh Name for current object
 * \return AttrKeyValue, or <i>NULL</i> if none created
 */
AttrKeyValue *get_cn(gchar *dn) {
	AttrKeyValue *cn;
	gchar *start;
	gchar *end;
	gchar *item;
	gchar **key_value;
	g_return_val_if_fail(dn != NULL, NULL);
	
	cn = attrkeyvalue_create();
	start = g_strstr_len(dn, strlen(dn), "cn");
	if (start == NULL)
		return NULL;
	end = g_strstr_len(start, strlen(start), ",");
	item = g_strndup(start, end - start);
	if (item == NULL)
		return NULL;
	key_value = g_strsplit(item, "=", 2);
	cn->key = g_strdup(key_value[0]);
	cn->value = g_strdup(key_value[1]);
	g_strfreev(key_value);
	g_free(item);
	return cn;
}

/**
 * Get ou or o attribute from dn
 *
 * \param dn Distinguesh Name for current object
 * \return AttrKeyValue, or <i>NULL</i> if none created
 */
AttrKeyValue *get_ou(gchar *dn) {
	AttrKeyValue *ou;
	gchar *start;
	gchar *end;
	gchar *item;
	gchar **key_value;
	
	g_return_val_if_fail(dn != NULL, NULL);
	ou = attrkeyvalue_create();
	start = g_strstr_len(dn, strlen(dn), ",o=");
	if (start == NULL)
		start = g_strstr_len(dn, strlen(dn), ",ou=");
	if (start == NULL)
		return NULL;
	start++;
	end = g_strstr_len(start, strlen(start), ",");
	item = g_strndup(start, end - start);
	if (item == NULL)
		return NULL;
	key_value = g_strsplit(item, "=", 2);
	ou->key = g_strdup(key_value[0]);
	ou->value = g_strdup(key_value[1]);
	g_strfreev(key_value);
	g_free(item);
	return ou;
}

/**
 * Print the contents of a LDAPMod structure for debuging purposes
 *
 * \param mods LDAPMod structure
 */
void ldapsvr_print_ldapmod(LDAPMod *mods[]) {
	gchar *mod_op;
	int i;

	fprintf( stderr, "Type\n");
	for (i = 0; NULL != mods[i]; i++) {
		LDAPMod *mod = (LDAPMod *) mods[i];
		gchar **vals;
		switch (mod->mod_op) {
			case LDAP_MOD_ADD: mod_op = g_strdup("ADD"); break;
			case LDAP_MOD_REPLACE: mod_op = g_strdup("MODIFY"); break;
			case LDAP_MOD_DELETE: mod_op = g_strdup("DELETE"); break;
			default: mod_op = g_strdup("UNKNOWN");
		}
		fprintf( stderr, "Operation: %s\tType:%s\nValues:\n", mod_op, mod->mod_type);
		vals = mod->mod_vals.modv_strvals;
		while (*vals) {
			fprintf( stderr, "\t%s\n", *vals++);
		}
	}
}

/**
 * Add new contact to LDAP
 *
 * \param server AddressBook resource
 * \param contact GHashTable with object to add
 */
void ldapsvr_add_contact(LdapServer *server, GHashTable *contact) {
	gchar *email = NULL, *param = NULL;
	LDAP *ld = NULL;
	LDAPMod *mods[MODSIZE];
	LDAPMod modarr[7];
	gint cnt = 0;
	char *cn[] = {NULL, NULL};
	char *displayName[] = {NULL, NULL};
	char *givenName[] = {NULL, NULL};
	char **mail = NULL;
	char *sn[] = {NULL, NULL};
	char *org[] = {NULL, NULL};
	char *obj[] = {/*"top",*/ "person", "organizationalPerson", "inetOrgPerson", NULL}; 
	int rc=0;
	GList *node;
	AttrKeyValue *ou, *commonName;
	ItemPerson *person;
	gchar *base_dn;
	GList *mailList;

	g_return_if_fail(server != NULL || contact != NULL);
	node = g_hash_table_lookup(contact , "mail");
	if (node) {
		EmailKeyValue *newEmail = node->data;
		email = g_strdup(newEmail->mail);
	}
	if (email == NULL) {
		server->retVal = LDAPRC_NODN;
		return;
	}
	base_dn = g_strdup_printf("mail=%s,%s", email, server->control->baseDN);
	g_free(email);
	person = 
		ldapsvr_get_contact(server, g_hash_table_lookup(contact , "uid"));
	person->externalID = g_strdup(base_dn);
	debug_print("dn: %s\n", base_dn);
	ld = ldapsvr_connect(server->control);
	if (ld == NULL) {
		clean_up(ld, server, contact);
		debug_print("no ldap found\n");
		return;
	}
	SETMODS(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "objectClass", obj);
	cnt++;
	ou = get_ou(base_dn);
	if (ou != NULL) {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, ou->key, org, ou->value);
		cnt++;
	}
	
	commonName = get_cn(base_dn);
	if (commonName == NULL) {
		param = g_hash_table_lookup(contact , "cn");
		if (param) {
			SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "cn", cn, param);
		}
		else {
			clean_up(ld, server, contact);
			debug_print("no CN found\n");
			return;
		}
	}
	else {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, commonName->key, cn, commonName->value);
		cnt++;
		param = g_hash_table_lookup(contact , "cn");
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "displayName", displayName, param);
		g_hash_table_insert(contact, "displayName", param);
	}
	cnt++;
	param = g_hash_table_lookup(contact , "givenName");
	if (param) {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "givenName", givenName, param);
		cnt++;
	}
	mailList = g_hash_table_lookup(contact , "mail");
	if (mailList) {
		char **tmp;
		mail = g_malloc(sizeof(*mail));
		tmp = g_malloc(sizeof(*tmp));
		mail = tmp;
		while (mailList) {
			EmailKeyValue *item = mailList->data;
			*tmp++ = g_strdup((gchar *) item->mail);
			mailList = g_list_next(mailList);
		}
		*tmp = NULL;
		SETMODS(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "mail", mail);
		cnt++;
	}
	param = g_hash_table_lookup(contact, "sn");
	if (param == NULL)
		param = g_strdup(N_("Some SN"));
	SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_ADD, "sn", sn, param);
	cnt++;
	mods[cnt] = NULL;
	if (debug_get_mode()) {
		ldapsvr_print_ldapmod(mods);
	}
	server->retVal = LDAPRC_SUCCESS;
	rc = ldap_add_ext_s(ld, base_dn, mods, NULL, NULL);
	if (rc) {
		switch (rc) {
			case LDAP_ALREADY_EXISTS: 
				server->retVal = LDAPRC_ALREADY_EXIST;
				break;
			default:
				fprintf(stderr, "ldap_modify for dn=%s\" failed[0x%x]: %s\n",base_dn, rc, ldap_err2string(rc));
				if (rc == 0x8)
					server->retVal = LDAPRC_STRONG_AUTH;
				else
					server->retVal = LDAPRC_NAMING_VIOLATION;
		}
	}
	g_free(base_dn);
	clean_up(ld, server, contact);
}

/**
 * Update contact to LDAP
 *
 * \param server AddressBook resource
 * \param contact GHashTable with object to update
 */
void ldapsvr_update_contact(LdapServer *server, GHashTable *contact) {
	LDAP *ld = NULL;
	LDAPMod *mods[MODSIZE];
	LDAPMod modarr[4];
	gint cnt = 0;
	gchar *param, *dn;
	Rdn *NoRemove = NULL;
	char *cn[] = {NULL, NULL};
	char *givenName[] = {NULL, NULL};
	char **mail = NULL;
	char *sn[] = {NULL, NULL};
	GList *mailList;

	g_return_if_fail(server != NULL || contact != NULL);
	ld = ldapsvr_connect(server->control);
	if (ld == NULL) {
		clean_up(ld, server, contact);
		return;
	}
	dn = g_hash_table_lookup(contact, "dn");
	if (dn == NULL) {
		clean_up(ld, server, contact);
		return;
	}
	NoRemove = ldapsvr_modify_dn(contact, dn);
	if (NoRemove) {
		/* We are trying to change RDN */
		gchar *newRdn = g_strdup_printf("%s=%s", NoRemove->attribute, NoRemove->value);
		int rc = ldap_modrdn2_s(ld, dn, newRdn, 1);
		if(rc != LDAP_SUCCESS) {
			if (rc ==  LDAP_ALREADY_EXISTS) {
				/* We are messing with a contact with more than one listed email
				 * address and the email we are changing is not the one used for dn
				 */
				/* It needs to be able to handle renaming errors to an already defined
				 * dn. For now we just refuse the update. It will be caught later on as
				 * a LDAPRC_NAMING_VIOLATION error.
				 */
			}
			else {
				fprintf(stderr, "Current dn: %s\n", dn);
				fprintf(stderr, "new dn: %s\n", newRdn);
				fprintf(stderr, "LDAP Error(ldap_modrdn2_s) failed[0x%x]: %s\n", rc, ldap_err2string(rc));
				g_free(newRdn);
				return;
			}
		}
		else {
			g_free(newRdn);
			dn = g_strdup(NoRemove->new_dn);
		}
	}
	else {
		server->retVal = LDAPRC_NODN;
		clean_up(ld, server, contact);
		return;
	}
	param = g_hash_table_lookup(contact , "cn");
	if (param && (strcmp(param, NoRemove->value) != 0 && strcmp("cn", NoRemove->attribute) != 0)) {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_REPLACE, "displayName", cn, param);
		cnt++;
		g_hash_table_insert(contact, "displayName", param);
	}
	param = g_hash_table_lookup(contact , "givenName");
	if (param && (strcmp(param, NoRemove->value) != 0 && strcmp("givenName", NoRemove->attribute) != 0)) {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_REPLACE, "givenName", givenName, param);
		cnt++;
	}
	mailList = g_hash_table_lookup(contact , "mail");
	if (mailList) {
		if (!(strcmp("mail", NoRemove->attribute) == 0 && g_list_length(mailList) == 1)) {
			char **tmp;
			mail = g_malloc(sizeof(*mail));
			tmp = g_malloc(sizeof(*tmp));
			mail = tmp;
			while (mailList) {
				EmailKeyValue *item = mailList->data;
				*tmp++ = g_strdup((gchar *) item->mail);
				mailList = g_list_next(mailList);
			}
			*tmp = NULL;
			SETMODS(mods[cnt], modarr[cnt], LDAP_MOD_REPLACE, "mail", mail);
			cnt++;
		}
	}
	param = g_hash_table_lookup(contact , "sn");
	if (param && (strcmp(param, NoRemove->value) != 0 && strcmp("sn", NoRemove->attribute) != 0)) {
		SETMOD(mods[cnt], modarr[cnt], LDAP_MOD_REPLACE, "sn", sn, param);
		cnt++;
	}
	debug_print("newDN: %s\n", dn);
	if (NoRemove)
		g_free(NoRemove);
	server->retVal = LDAPRC_SUCCESS;
	if (cnt > 0) {
		int rc;
		mods[cnt] = NULL;
		rc = ldap_modify_ext_s(ld, dn, mods, NULL, NULL);
		if (rc) {
			fprintf(stderr, "ldap_modify for dn=%s\" failed[0x%x]: %s\n",
                    dn, rc, ldap_err2string(rc));
			server->retVal = LDAPRC_NAMING_VIOLATION;
		}
		if (mail)
			g_free(mail);
	}
	clean_up(ld, server, contact);
}

/**
 * Delete contact from LDAP
 *
 * \param server AddressBook resource
 * \param contact GHashTable with object to delete
 */
void ldapsvr_delete_contact(LdapServer *server, GHashTable *contact) {
	LDAP *ld = NULL;
	gchar *dn;
	int rc;

	g_return_if_fail(server != NULL || contact != NULL);
	ld = ldapsvr_connect(server->control);
	if (ld == NULL) {
		clean_up(ld, server, contact);
		return;
	}
	dn = g_hash_table_lookup(contact, "dn");
	if (dn == NULL) {
		clean_up(ld, server, contact);
		return;
	}
	rc = ldap_delete_ext_s(ld, dn, NULL, NULL);
	if (rc) {
		fprintf(stderr, "ldap_modify for dn=%s\" failed[0x%x]: %s\n",
				dn, rc, ldap_err2string(rc));
		server->retVal = LDAPRC_NODN;
	}
	clean_up(ld, server, contact);
}

/**
 * Update any changes to the server.
 *
 * \param server AddressBook resource.
 * \param person ItemPerson holding user input.
 */
void ldapsvr_update_book(LdapServer *server, ItemPerson *item) {
	GList *node = NULL;
	GHashTable *contact = NULL;
	GList *contacts = NULL, *head = NULL;

	g_return_if_fail(server != NULL);
	debug_print("updating ldap addressbook\n");

	contact = g_hash_table_new(g_str_hash, g_str_equal);
	if (item) {
		gboolean result = ldapsvr_retrieve_item_person(item, contact);
		debug_print("Found contact to update: %s\n", result? "Yes" : "No");
		if (result) {
			if (debug_get_mode()) {
				addritem_print_item_person(item, stdout);
			}
			contacts = g_list_append(contacts, contact);
		}
	}
	else {
		ItemFolder *folder = server->addressCache->rootFolder;
		node = folder->listFolder;
		if (node)
			node = g_list_copy(node);
		node = g_list_prepend(node, server->addressCache->rootFolder);
		if (node) {
			while (node) {
				AddrItemObject *aio = node->data;
				if (aio) {
					if (aio->type == ITEMTYPE_FOLDER) {
						ItemFolder *folder = (ItemFolder *) aio;
						GList *persons = folder->listPerson;
						while (persons) {
							AddrItemObject *aio = persons->data;
							if (aio) {
								if (aio->type == ITEMTYPE_PERSON) {
									ItemPerson *item = (ItemPerson *) aio;
									gboolean result = ldapsvr_retrieve_item_person(item, contact);
									debug_print("Found contact to update: %s\n", result? "Yes" : "No");
									if (result) {
										if (debug_get_mode()) {
											gchar *uid = g_hash_table_lookup(contact, "uid");
											item = ldapsvr_get_contact(server, uid);
											addritem_print_item_person(item, stdout);
										}
										contacts = g_list_append(contacts, contact);
									}
								}
							}
							persons = g_list_next(persons);
						}
					}
				}
				else {
					fprintf(stderr, "\t\tpid : ???\n");
				}
				node = g_list_next(node);
			}
			g_list_free(node);
		}
	}
	head = contacts;
	if (debug_get_mode()) {
		if (contacts)
			debug_print("Contacts which must be updated in LDAP:\n");
		while (contacts) {
			debug_print("\tContact:\n");
			g_hash_table_foreach(contacts->data, 
				ldapsvr_print_contacts_hashtable, stderr);
			contacts = g_list_next(contacts);
		}
	}
	if (contacts == NULL)
		contacts = head;
	while (contacts) {
		gchar *status;
		contact = (GHashTable *) contacts->data;
		status = (gchar *) g_hash_table_lookup(contact, "status");
		if (status == NULL)
			status = g_strdup("NULL");
		if (g_ascii_strcasecmp(status, "new") == 0) {
			ldapsvr_add_contact(server, contact);
		}
		else if (g_ascii_strcasecmp(status, "update") == 0) {
			ldapsvr_update_contact(server, contact);
		}
		else if (g_ascii_strcasecmp(status, "delete") == 0) {
			ldapsvr_delete_contact(server, contact);
		}
		else
			g_critical(_("ldapsvr_update_book->Unknown status: %s\n"), status);
		contacts = g_list_next(contacts);
	}
	ldapsvr_free_hashtable(head);
}

#endif	/* USE_LDAP */

/*
 * End of Source.
 */

