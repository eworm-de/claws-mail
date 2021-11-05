/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2007 Holger Berndt
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

#ifndef NOTIFICATION_POPUP_H
#define NOTIFICATION_POPUP_H NOTIFICATION_POPUP_H

#ifdef HAVE_CONFIG_H
#  include "claws-features.h"
#endif

#ifdef NOTIFICATION_POPUP

#include "procmsg.h"

#define POPUP_SPECIFIC_FOLDER_ID_STR "popup"

void notification_popup_msg(MsgInfo*);

#endif /* NOTIFICATION_POPUP */

#endif /* NOTIFICATION_POPUP_H */
