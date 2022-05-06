/* Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014 Charles Lehner and the Claws Mail team
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

#ifndef __ADDRMERGE_H__
#define __ADDRMERGE_H__

#include <gtk/gtk.h>

#include "addritem.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addrselect.h"

struct AddrMergePage {
   GtkCMCTree*     clist;
   AddressObject*	   pobj;
   AddressBookFile*    abf;
   AddressDataSource*  ds;
   AddrSelectList*     addressSelect;
   ItemPerson*     target;
   ItemPerson*     nameTarget;
   gchar*		   picture;
   GList*		   emails;
   GList*		   persons;
   gboolean 	   pickPicture;
   gboolean 	   pickName;
   GtkWidget*	   dialog;
   GtkWidget*	   iconView;
   GtkWidget*	   namesList;
};

void addrmerge_merge   ( GtkCMCTree *clist,
			 AddressObject *pobj,
			 AddressDataSource *ds,
			 AddrSelectList *list );

#endif /*__ADDRMERGE_H__*/
