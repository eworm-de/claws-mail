/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Address item data.
 */

#ifndef __ADDRESSITEM_H__
#define __ADDRESSITEM_H__

#define ADDRESS_OBJECT(obj)		((AddressObject *)obj)
#define ADDRESS_OBJECT_TYPE(obj)	(ADDRESS_OBJECT(obj)->type)
#define ADDRESS_ITEM(obj)		((AddressItem *)obj)

#define ADDRESS_ITEM_CAT_UNKNOWN -1

typedef struct _AddressObject	AddressObject;
typedef struct _AddressItem	AddressItem;

typedef enum
{
	ADDR_ITEM,
	ADDR_GROUP,
	ADDR_FOLDER,
	ADDR_VCARD,
	ADDR_JPILOT,
	ADDR_CATEGORY,
	ADDR_LDAP
} AddressObjectType;

struct _AddressObject
{
	AddressObjectType type;
};

struct _AddressItem
{
	AddressObject obj;

	gchar *name;
	gchar *address;
	gchar *remarks;
	gchar *externalID;
	gint  categoryID;
};

#endif /* __ADDRESSITEM_H__ */
