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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkutils.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "statusbar.h"
#include "alertpanel.h"
#include "clamd-plugin.h"
#include "file-utils.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

/* needs to be generic */
static const gchar* config_dirs[] = { 
	"/etc", 
	"/usr/local/etc",
	"/etc/clamav",
	"/usr/local/etc/clamav",
	NULL };

static const gchar* clamd_tokens[] = {
	"LocalSocket",
	"TCPSocket",
	"TCPAddr",
	NULL };

static Clamd_Socket* Socket = NULL;
static Config* config = NULL;

/**
 *  clamd commands used
 *  prefixing with either z or n is recommended
 *  z <=> null terminated command
 *  n <=> newline terminated command
 */
static const gchar ping[] = "nPING\n";
static const gchar version[] = "nVERSION\n";
static const gchar scan[] = "nSCAN";
static const gchar contscan[] = "nCONTSCAN";
static const gchar instream[10] = "zINSTREAM\0";

void clamd_create_config_automatic(const gchar* path) {
	FILE* conf;
	char buf[1024];
	gchar* key = NULL;

	/*debug_set_mode(TRUE);*/
	/*debug_print("%s : %s\n", folder, path);*/
	if (! path) {
		g_warning("missing path");
		return;
	}
	if (config && config->ConfigType == AUTOMATIC &&
			config->automatic.folder &&
			strcmp(config->automatic.folder, path) == 0) {
		debug_print("%s : %s - Identical. No need to read again\n",
			config->automatic.folder, path);
		return;
	}
	if (config)
		clamd_config_free(config);
	config = clamd_config_new();
	
	config->ConfigType = AUTOMATIC;
	config->automatic.folder = g_strdup(path);
	debug_print("Opening %s to parse config file\n", path);
	conf = claws_fopen(path, "r");
	if (!conf) {
		/*g_error("%s: Unable to open", path);*/
		alertpanel_error(_("%s: Unable to open\nclamd will be disabled"), path);
		return;
	}
	while (claws_fgets(buf, sizeof(buf), conf)) {
		g_strstrip(buf);
		if (buf[0] == '#')
			continue;
		const gchar** tokens = clamd_tokens;
		while (*tokens) {
			const gchar* token = *tokens++;
			if ((key = g_strstr_len(buf, strlen(buf), token)) != NULL) {
				gchar* value = NULL;

				gchar* tmp = &(*(key + strlen(token)));
				tmp = g_strchug(tmp);
				gchar* end = index(tmp, '#');
				if (end)
					value = g_strndup(tmp, end - tmp);
				else
					value = g_strdup(g_strchomp(tmp));
				if (strcmp(clamd_tokens[0], token) == 0) {
					/* UNIX socket */
					Socket = (Clamd_Socket *) malloc(sizeof(Clamd_Socket));
					if (Socket) {
						Socket->socket.host = NULL;
						Socket->socket.port = -1;
						Socket->type = UNIX_SOCKET;
						Socket->socket.path = g_strdup(value);
						g_free(value);
						claws_fclose(conf);
						debug_print("clamctl: %s\n", Socket->socket.path);
						return;
					}
				}
				else if (strcmp(clamd_tokens[1], token) == 0) {
					/* INET socket */
					if (! Socket) {
						Socket = (Clamd_Socket *) malloc(sizeof(Clamd_Socket));
						if (Socket) {
							Socket->socket.path = NULL;
							Socket->socket.port = -1;
							Socket->type = INET_SOCKET;
							Socket->socket.port = atoi(value);
							Socket->socket.host = g_strdup("localhost");
							debug_print("clamctl: %s:%d\n", 
								Socket->socket.host, Socket->socket.port);
						}
					}
					else {
						Socket->type = INET_SOCKET;
						Socket->socket.port = atoi(value);
						if (! Socket->socket.host)
							Socket->socket.host = g_strdup("localhost");
						debug_print("clamctl: %s:%d\n", 
							Socket->socket.host, Socket->socket.port);
					}
					/* We must continue since TCPAddr could also be configured */
				}
				else if (strcmp(clamd_tokens[2], token) == 0) {
					if (! Socket) {
						Socket = (Clamd_Socket *) malloc(sizeof(Clamd_Socket));
						if (Socket) {
							Socket->socket.path = NULL;
							Socket->socket.port = 3310; /* default port */
							Socket->type = INET_SOCKET;
							Socket->socket.host = g_strdup(value);
							debug_print("clamctl: %s:%d\n", 
								Socket->socket.host, Socket->socket.port);
						}
					}
					else {
						Socket->type = INET_SOCKET;
						if (Socket->socket.host)
							g_free(Socket->socket.host);
						Socket->socket.host = g_strdup(value);
						if (Socket->socket.port == -1)
							Socket->socket.port = 3310;
						debug_print("clamctl: %s:%d\n", 
							Socket->socket.host, Socket->socket.port);
					}
					/* We must continue since TCPSocket could also be configured */
				}
				g_free(value);
			}
		}
	}
	claws_fclose(conf);
	if (! (Socket && (Socket->socket.port || Socket->socket.path))) {
		/*g_error("%s: Not able to find required information", path);*/
		alertpanel_error(_("%s: Not able to find required information\nclamd will be disabled"), path);
	}
	/*debug_set_mode(FALSE);*/
}

void clamd_create_config_manual(const gchar* host, int port) {
	if (! host || port < 1) {
		g_warning("missing host or port < 1");
		return;
	}
	if (config && config->ConfigType == MANUAL &&
			config->manual.host && config->manual.port == port &&
			strcmp(config->manual.host, host) == 0) {
		debug_print("%s : %s and %d : %d - Identical. No need to read again\n",
			config->manual.host, host, config->manual.port, port);
		return;
	}

	if (config)
		clamd_config_free(config);
	config = clamd_config_new();
	
	config->ConfigType = MANUAL;
	config->manual.host = g_strdup(host);
	config->manual.port = port;
	/* INET socket */
	Socket = (Clamd_Socket *) malloc(sizeof(Clamd_Socket));
	if (Socket) {
		Socket->type = INET_SOCKET;
		Socket->socket.port = port;
		Socket->socket.host = g_strdup(host);
	}
	else {
		/*g_error("%s: Not able to find required information", path);*/
		alertpanel_error(_("Could not create socket"));
	}
}

gboolean clamd_find_socket() {
	const gchar** config_dir = config_dirs;
	gchar *clamd_conf = NULL;
	
	while (*config_dir) {
		clamd_conf = g_strdup_printf("%s/clamd.conf", *config_dir++);
		debug_print("Looking for %s\n", clamd_conf);
		if (g_file_test(clamd_conf, G_FILE_TEST_EXISTS))
			break;
		g_free(clamd_conf);
		clamd_conf = NULL;
	}
	if (! clamd_conf)
		return FALSE;

	debug_print("Using %s to find configuration\n", clamd_conf);
	clamd_create_config_automatic(clamd_conf);
	g_free(clamd_conf);

	return TRUE;
}

Config* clamd_get_config() {
	return config;
}

Clamd_Socket* clamd_get_socket() {
	return Socket;
}

static int create_socket() {
	struct sockaddr_un addr_u;
	struct sockaddr_in addr_i;
	struct hostent *hp;

	int new_sock = -1;

	/*debug_set_mode(TRUE);*/
	if (! Socket) {
		return -1;
	}
	memset(&addr_u, 0, sizeof(addr_u));
	memset(&addr_i, 0, sizeof(addr_i));
	debug_print("socket->type: %d\n", Socket->type);
	switch (Socket->type) {
		case UNIX_SOCKET:
			debug_print("socket path: %s\n", Socket->socket.path);
			new_sock = socket(PF_UNIX, SOCK_STREAM, 0);
			if (new_sock < 0) {
				perror("create socket");
				return new_sock;
			}
			debug_print("socket file (create): %d\n", new_sock);
			addr_u.sun_family = AF_UNIX;
			if (strlen(Socket->socket.path) > UNIX_PATH_MAX) {
				g_error("socket path longer than %d-char: %s",
					UNIX_PATH_MAX, Socket->socket.path);
				new_sock = -2;
			} else {
				memcpy(addr_u.sun_path, Socket->socket.path, 
						strlen(Socket->socket.path));
				if (connect(new_sock, (struct sockaddr *) &addr_u, sizeof(addr_u)) < 0) {
					perror("connect socket");
					close(new_sock);
					new_sock = -2;
				}
			}
			debug_print("socket file (connect): %d\n", new_sock);
			break;
		case INET_SOCKET:
			addr_i.sin_family = AF_INET;
			addr_i.sin_port = htons(Socket->socket.port);
			hp = gethostbyname(Socket->socket.host);
			if (!hp) {
				g_error("fail to get host by: %s", Socket->socket.host);
				return new_sock;
			}
			debug_print("IP socket host: %s:%d\n",
					Socket->socket.host, Socket->socket.port);
			memcpy((void *)&addr_i.sin_addr, (void *)hp->h_addr, hp->h_length);
			new_sock = socket(PF_INET, SOCK_STREAM, 0);
			if (new_sock < 0) {
				perror("create socket");
				return new_sock;
			}
			debug_print("IP socket (create): %d\n", new_sock);
			if (connect(new_sock, (struct sockaddr *)&addr_i, sizeof(addr_i)) < 0) {
				perror("connect socket");
				close(new_sock);
				new_sock = -2;
			}
			debug_print("IP socket (connect): %d\n", new_sock);
			break;
	}

	return new_sock;
}

static void copy_socket(Clamd_Socket* sock) {
	Socket = (Clamd_Socket *) malloc(sizeof(Clamd_Socket));
	Socket->type = sock->type;
	if (Socket->type == UNIX_SOCKET) {
		Socket->socket.path = g_strdup(sock->socket.path);
		Socket->socket.host = NULL;
	}
	else {
		Socket->socket.path = NULL;
		Socket->socket.host = g_strdup(sock->socket.host);
		Socket->socket.port = sock->socket.port;
	}
}

Clamd_Stat clamd_init(Clamd_Socket* config) {
	gchar buf[BUFSIZ];
	int n_read;
	gboolean connect = FALSE;
	int sock;

	/*debug_set_mode(TRUE);*/
	if (config != NULL && Socket != NULL)
		return NO_SOCKET;
	if (config) {
		debug_print("socket: %s\n", config->socket.path);
		copy_socket(config);
	}
	sock = create_socket();
	if (sock < 0) {
		debug_print("no connection\n");
		return NO_CONNECTION;
	}
	if (write(sock, ping, strlen(ping)) == -1) {
		debug_print("write error %d\n", errno);
		close(sock);
		return NO_CONNECTION;
	}
	memset(buf, '\0', sizeof(buf));
	while ((n_read = read(sock, buf, BUFSIZ - 1)) > 0) {
		buf[n_read] = '\0';
		if (buf[n_read - 1] == '\n')
			buf[n_read - 1] = '\0';
		debug_print("Ping result: %s\n", buf);
		if (strcmp("PONG", buf) == 0)
			connect = TRUE;
	}
	close(sock);
	sock = create_socket();
	if (sock < 0) {
	    debug_print("no connection\n");
	    return NO_CONNECTION;
	}
	if (write(sock, version, strlen(version)) == -1) {
	    debug_print("write error %d\n", errno);
	    close(sock);
	    return NO_CONNECTION;
	}
	memset(buf, '\0', sizeof(buf));
	while ((n_read = read(sock, buf, BUFSIZ - 1)) > 0) {
		buf[n_read] = '\0';
		if (buf[n_read - 1] == '\n')
			buf[n_read - 1] = '\0';
	    debug_print("Version: %s\n", buf);
	}
	close(sock);
	/*debug_set_mode(FALSE);*/
	return (connect) ? OK : NO_CONNECTION;
}

static Clamd_Stat clamd_stream_scan(int sock,
		const gchar* path, gchar** res, ssize_t size) {
	int fd;
	ssize_t count;
	gchar buf[BUFSIZ];
	int n_read;
	int32_t chunk;
	
	debug_print("Scanning: %s\n", path);

	memset(buf, '\0', sizeof(buf));

	if (! res || size < 1) {
		return SCAN_ERROR;
	}
	if (! *res)
		*res = g_new(gchar, size);
	memset(*res, '\0', size);
	
	if (! g_file_test(path, G_FILE_TEST_EXISTS)) {
		*res = g_strconcat("ERROR -> ", path, _(": File does not exist"), NULL);
		debug_print("res: %s\n", *res);
		return SCAN_ERROR;
	}

#ifdef _LARGE_FILES
	fd = open(path, O_RDONLY, O_LARGEFILE);
#else
	fd = open(path, O_RDONLY);
#endif

	if (fd < 0) {
		/*g_error("%s: Unable to open", path);*/
		*res = g_strconcat("ERROR -> ", path, _(": Unable to open"), NULL);
		return SCAN_ERROR;
	}
	
	debug_print("command: %s\n", instream);
	if (write(sock, instream, strlen(instream) + 1) == -1) {
		close(fd);
		return NO_CONNECTION;
	}

	while ((count = read(fd, (void *) buf, BUFSIZ - 1)) > 0) {
		buf[count] = '\0';
		if (buf[count - 1] == '\n')
			buf[count - 1] = '\0';
		
		debug_print("chunk size: %ld\n", count);
		chunk = htonl(count);
		if (write(sock, &chunk, 4) == -1) {
			close(fd);
			*res = g_strconcat("ERROR -> ", _("Socket write error"), NULL);
			return SCAN_ERROR;
		}
		if (write(sock, buf, count) == -1) {
			close(fd);
			*res = g_strconcat("ERROR -> ", _("Socket write error"), NULL);
			return SCAN_ERROR;
		}
		memset(buf, '\0', BUFSIZ - 1);
	}
	if (count == -1) {
		close(fd);
		*res = g_strconcat("ERROR -> ", path, _("%s: Error reading"), NULL);
		return SCAN_ERROR;
	}
	close(fd);
	
	chunk = htonl(0);
	if (write(sock, &chunk, 4) == -1) {
		*res = g_strconcat("ERROR -> ", _("Socket write error"), NULL);
		return SCAN_ERROR;
	}
	
	debug_print("reading from socket\n");
	n_read = read(sock, *res, size);
	if (n_read < 0) {
		*res = g_strconcat("ERROR -> ", _("Socket read error"), NULL);
		return SCAN_ERROR;
	}
	(*res)[n_read] = '\0';
	debug_print("received: %s\n", *res);
	return OK;
}

Clamd_Stat clamd_verify_email(const gchar* path, response* result) {
	gchar buf[BUFSIZ];
	int n_read;
	gchar* command;
	Clamd_Stat stat;
	int sock;

	/*debug_set_mode(TRUE);*/
	if (!result) {
		return SCAN_ERROR;
	}
	sock = create_socket();
	if (sock < 0) {
		debug_print("no connection (socket create)\n");
		return NO_CONNECTION;
	}
	memset(buf, '\0', sizeof(buf));
	if (Socket->type == INET_SOCKET) {
		gchar* tmp = g_new0(gchar, BUFSIZ);
		stat = clamd_stream_scan(sock, path, &tmp, BUFSIZ);
		if (stat != OK) {
			close(sock);
			result->msg = g_strdup(tmp);
			g_free(tmp);
			debug_print("result: %s\n", result->msg);
			return stat;
		}
		debug_print("copy to buf: %s\n", tmp);
		memcpy(&buf, tmp, BUFSIZ);
		g_free(tmp);
		debug_print("response: %s\n", buf);
	}
	else {
		command = g_strconcat(scan, " ", path, "\n", NULL);
		debug_print("command: %s\n", command);
		if (write(sock, command, strlen(command)) == -1) {
			debug_print("no connection (socket write)\n");
			g_free(command);
			return NO_CONNECTION;
		}
		g_free(command);
		memset(buf, '\0', sizeof(buf));
		/* shouldn't we read only once here? we're checking the last response line anyway */
		while ((n_read = read(sock, buf, BUFSIZ - 1)) > 0) {
			buf[n_read] = '\0';
			if (buf[n_read - 1] == '\n')
				buf[n_read - 1] = '\0';
			debug_print("response: %s\n", buf);
		}
		if (n_read == 0) {
			buf[n_read] = '\0';
			debug_print("response: %s\n", buf);
		} else {
			/* in case read() fails */
			debug_print("read error %d\n", errno);
			result->msg = NULL;
			close(sock);
			return NO_CONNECTION;
		}
	}
	if (strstr(buf, "ERROR")) {
		stat = SCAN_ERROR;
		result->msg = g_strdup(buf);
	}		
	else if (strstr(buf, "FOUND")) {
		stat = VIRUS;
		result->msg = g_strdup(buf);
	}		
	else {
		stat = OK;
		result->msg = NULL;
	}
	close(sock);
	/*debug_set_mode(FALSE);*/

	return stat;
}

GSList* clamd_verify_dir(const gchar* path) {
	gchar buf[BUFSIZ];
	int n_read;
	gchar* command;
	GSList *list = NULL;
	int sock;

	if (Socket->type == INET_SOCKET)
		return list;

	sock = create_socket();
	if (sock < 0) {
		debug_print("No socket\n");
		return list;
	}
	command = g_strconcat(contscan, path, "\n", NULL);
	debug_print("command: %s\n", command);
	if (write(sock, command, strlen(command)) == -1) {
		debug_print("write error %d\n", errno);
		close(sock);
		g_free(command);
		return list;
	}
	g_free(command);
	memset(buf, '\0', sizeof(buf));
	while ((n_read = read(sock, buf, BUFSIZ - 1)) > 0) {
        buf[n_read] = 0;
		gchar** tmp = g_strsplit(buf, "\n", 0);
		gchar** head = tmp;
		while (*tmp) {
			gchar* file = *tmp++;
			debug_print("%s\n", file);
			if (strstr(file, "ERROR")) {
				g_warning("%s", file);
				/* dont report files with errors */
			}
			else if (strstr(file, "FOUND")) {
				list = g_slist_append(list, g_strdup(file));
			}
		}
		g_strfreev(head);
	}
	close(sock);
	return list;
}

void clamd_free_gslist(GSList* list) {
	GSList* tmp = list;
	while(tmp) {
		g_free(tmp->data);
		tmp = g_slist_next(tmp);
	}
	g_slist_free(list);
}

gchar* clamd_get_virus_name(gchar* msg) {
	gchar *head, *tail, *name;

	tail = g_strrstr_len(msg, strlen(msg), "FOUND");
	if (! tail)
		return NULL;
	head = g_strstr_len(msg, strlen(msg), ":");
	++head;
	name = g_strndup(head, tail - head);
	g_strstrip(name);
	return name;
}

void clamd_free() {
	if (Socket) {
		switch (Socket->type) {
		    case UNIX_SOCKET:
			if (Socket->socket.path) {
			    g_free(Socket->socket.path);
			    Socket->socket.path = NULL;
			}
			break;
		    case INET_SOCKET:
			if (Socket->socket.host) {
			    g_free(Socket->socket.host);
			    Socket->socket.host = NULL;
			}
			break;
		}
		g_free(Socket);
		Socket = NULL;
	}
	if (config) {
	    clamd_config_free(config);
	    config = NULL;
	}
}

Config* clamd_config_new() {
	return g_new0(Config, 1);
}

void clamd_config_free(Config* c) {
	if (c->ConfigType == AUTOMATIC) {
		g_free(c->automatic.folder);
		c->automatic.folder = NULL;
	}
	else {
		g_free(c->manual.host);
		c->manual.host = NULL;
	}
	g_free(c);
}

gchar* int2char(int i) {
	gchar* s = g_new0(gchar, 5);

	sprintf(s, "%d", i);
	
	return s;
}

gchar* long2char(long l) {
	gchar* s = g_new0(gchar, 5);

	debug_print("l: %ld\n", l);
	sprintf(s, "%ld", l);
	debug_print("s: %s\n", s);
	
	return s;
}
