/* vim: set textwidth=80 tabstop=4: */

/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2017 Michael Rasmussen and the Claws Mail Team
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
 * 
 */

#ifndef __CLAMD_PLUGIN_H__
#define __CLAMD_PLUGIN_H__

#include <glib.h>

typedef enum _Type Type;
enum _Type { UNIX_SOCKET, INET_SOCKET };

typedef enum _Clamd_Stat Clamd_Stat;
enum _Clamd_Stat { OK, VIRUS, NO_SOCKET, NO_CONNECTION, SCAN_ERROR };

typedef struct _Clamd_Socket Clamd_Socket;
struct _Clamd_Socket {
	Type type;
	union {
		struct {
			gchar*	path;
		};
		struct {
			gchar*	host;
			int		port;
		};
	} socket;
};

typedef struct {
	enum { AUTOMATIC, MANUAL } ConfigType;
	union {
		struct {
			gchar*	folder;
		} automatic;
		struct {
			gchar*	host;
			int		port;
		} manual;
	};
} Config;
	
typedef struct _response response;
struct _response {
	gchar* msg;
};

void clamd_create_config_automatic(const gchar* path);

void clamd_create_config_manual(const gchar* host, int port);

gchar* int2char(int i);

gchar* long2char(long l);

/**
 * Function which looks for clamd.conf the default places
 * and configures the plugin according to the information
 * found.
 * @return <b>TRUE</b> if clamd.conf found which means all
 * information need to make a connection has been found.
 * <b>FALSE</b> otherwise.
 */
gboolean clamd_find_socket();

/**
 * Function to get current configuration
 * @return the current configuration for clamd or <b>NULL</b>
 */
Config* clamd_get_config();

/**
 * Function to retrieve virus name from msg
 * @param msg Message returned from clamd
 * @return virus name or <b>NULL</b> if no virus name found
 */
gchar* clamd_get_virus_name(gchar* msg);

/**
 * Function to initialize the connection to clamd.
 * @param config A pointer to a struct _Clamd_Socket having
 * the required information. If clamd_find_socket returned
 * TRUE config should be <b>NULL</b> because all the needed
 * information is already present @see clamd_find_socket.
 * @return Clamd_Stat. @see _Clamd_Stat.
 */
Clamd_Stat clamd_init(Clamd_Socket* config);

/**
 * Function returning the current socket information.
 * @return reference to the current Clamd_Socket. @see _Clamd_Socket.
 */
Clamd_Socket* clamd_get_socket();

/**
 * Function which is checks a specific email for known viruses
 * @param path Absolut path to email to check.
 * @param msg String to which result of scan will be copied. Will be
 * <b>NULL</b> if no virus was found.
 * @return Clamd_Stat. @see _Clamd_Stat.
 */
Clamd_Stat clamd_verify_email(const gchar* path, response* result);

/**
 * Function which is checks files in a specific directory for
 * known viruses. Dont stop when a virus is found but keeps going
 * @param path Absolut path to directory to check.
 * @return list of list with virus or <b>NULL</b>.
 */
GSList* clamd_verify_dir(const gchar* path);

/**
 * Function to free all memory assigned to a GSList
 * @param list The GSList to free
 */
void clamd_free_gslist(GSList* list);

/**
 * Function which frees all memory assigned to clamd_plugin
 */
void clamd_free();

Config* clamd_config_new();

void clamd_config_free(Config* c);

#endif
