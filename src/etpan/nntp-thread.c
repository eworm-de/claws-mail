/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2022 the Claws Mail team and DINH Viet Hoa
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
#include "claws-features.h"
#endif

#ifdef HAVE_LIBETPAN

#include <glib.h>
#include <glib/gi18n.h>
#include "nntp-thread.h"
#include "news.h"
#include <sys/types.h>
#include <sys/stat.h>
#if (defined(__DragonFly__) || defined (__NetBSD__) || defined (__FreeBSD__) || defined (__OpenBSD__) || defined (__CYGWIN__))
#include <sys/socket.h>
#endif
#include <fcntl.h>
#ifndef G_OS_WIN32
#include <sys/mman.h>
#include <sys/wait.h>
#endif
#include <gtk/gtk.h>
#include <log.h>
#include "etpan-thread-manager.h"
#include "etpan-ssl.h"
#include "utils.h"
#include "mainwindow.h"
#include "ssl_certificate.h"
#include "socket.h"
#include "remotefolder.h"
#include "main.h"
#include "account.h"
#include "statusbar.h"

#define DISABLE_LOG_DURING_LOGIN

#define NNTP_BATCH_SIZE 5000

static struct etpan_thread_manager * thread_manager = NULL;
static chash * nntp_hash = NULL;
static chash * session_hash = NULL;
static guint thread_manager_signal = 0;
static GIOChannel * io_channel = NULL;

static int do_newsnntp_socket_connect(newsnntp * imap, const char * server,
			       gushort port, ProxyInfo * proxy_info)
{
	SockInfo * sock;
	mailstream * stream;

	if (!proxy_info)
		return newsnntp_socket_connect(imap, server, port);

	if (port == 0)
		port = 119;

	sock = sock_connect(proxy_info->proxy_host, proxy_info->proxy_port);

	if (sock == NULL)
		return NEWSNNTP_ERROR_CONNECTION_REFUSED;

	if (proxy_connect(sock, server, port, proxy_info) < 0) {
		sock_close(sock, TRUE);
		return NEWSNNTP_ERROR_CONNECTION_REFUSED;
	}

	stream = mailstream_socket_open_timeout(sock->sock,
			imap->nntp_timeout);
	if (stream == NULL) {
		sock_close(sock, TRUE);
		return NEWSNNTP_ERROR_MEMORY;
	}

	/* Libetpan now has the socket fd, and we're not interested in
	 * rest of the SockInfo struct. Let's free it, while not touching
	 * the socket itself. */
	sock_close(sock, FALSE);

	return newsnntp_connect(imap, stream);
}

#ifdef USE_GNUTLS
static int do_newsnntp_ssl_connect_with_callback(newsnntp * imap, const char * server,
	gushort port,
	void (* callback)(struct mailstream_ssl_context * ssl_context, void * data),
	void * data,
	ProxyInfo *proxy_info)
{
	SockInfo * sock;
	mailstream * stream;

	if (!proxy_info)
		return newsnntp_ssl_connect_with_callback(imap, server,
				port, callback, data);

	if (port == 0)
		port = 563;

	sock = sock_connect(proxy_info->proxy_host, proxy_info->proxy_port);

	if (sock == NULL)
		return NEWSNNTP_ERROR_CONNECTION_REFUSED;

	if (proxy_connect(sock, server, port, proxy_info) < 0) {
		sock_close(sock, TRUE);
		return NEWSNNTP_ERROR_CONNECTION_REFUSED;
	}

	stream = mailstream_ssl_open_with_callback_timeout(sock->sock,
			imap->nntp_timeout, callback, data);
	if (stream == NULL) {
		sock_close(sock, TRUE);
		return NEWSNNTP_ERROR_SSL;
	}

	/* Libetpan now has the socket fd, and we're not interested in
	 * rest of the SockInfo struct. Let's free it, while not touching
	 * the socket itself. */
	sock_close(sock, FALSE);

	return newsnntp_connect(imap, stream);
}
#endif

static void nntp_logger(int direction, const char * str, size_t size) 
{
	gchar *buf;
	gchar **lines;
	int i = 0;

	if (size > 256) {
		log_print(LOG_PROTOCOL, "NNTP%c [data - %"G_GSIZE_FORMAT" bytes]\n", direction?'>':'<', size);
		return;
	}
	buf = malloc(size+1);
	memset(buf, 0, size+1);
	strncpy(buf, str, size);
	buf[size] = '\0';

	if (!strncmp(buf, "<<<<<<<", 7) 
	||  !strncmp(buf, ">>>>>>>", 7)) {
		free(buf);
		return;
	}
	while (strstr(buf, "\r"))
		*strstr(buf, "\r") = ' ';
	while (strlen(buf) > 0 && buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\0';

	lines = g_strsplit(buf, "\n", -1);

	while (lines[i] && *lines[i]) {
		log_print(LOG_PROTOCOL, "NNTP%c %s\n", direction?'>':'<', lines[i]);
		i++;
	}
	g_strfreev(lines);
	free(buf);
}

static void delete_nntp(Folder *folder, newsnntp *nntp)
{
	chashdatum key;

	key.data = &folder;
	key.len = sizeof(folder);
	chash_delete(session_hash, &key, NULL);
	
	key.data = &nntp;
	key.len = sizeof(nntp);
	if (nntp && nntp->nntp_stream) {
		/* we don't want libetpan to logout */
		mailstream_close(nntp->nntp_stream);
		nntp->nntp_stream = NULL;
	}
	debug_print("removing newsnntp %p\n", nntp);
	newsnntp_free(nntp);	
}

static gboolean thread_manager_event(GIOChannel * source,
    GIOCondition condition,
    gpointer data)
{
#ifdef G_OS_WIN32
	gsize bytes_read;
	gchar ch;
	
	if (condition & G_IO_IN)
		g_io_channel_read_chars(source, &ch, 1, &bytes_read, NULL);
#endif
	etpan_thread_manager_loop(thread_manager);
	
	return TRUE;
}

#define ETPAN_DEFAULT_NETWORK_TIMEOUT 60
extern gboolean etpan_skip_ssl_cert_check;

void nntp_main_init(gboolean skip_ssl_cert_check)
{
	int fd_thread_manager;
	
	etpan_skip_ssl_cert_check = skip_ssl_cert_check;
	
	nntp_hash = chash_new(CHASH_COPYKEY, CHASH_DEFAULTSIZE);
	session_hash = chash_new(CHASH_COPYKEY, CHASH_DEFAULTSIZE);
	
	thread_manager = etpan_thread_manager_new();
	
	fd_thread_manager = etpan_thread_manager_get_fd(thread_manager);
	
#ifndef G_OS_WIN32
	io_channel = g_io_channel_unix_new(fd_thread_manager);
#else
	io_channel = g_io_channel_win32_new_fd(fd_thread_manager);
#endif
	
	thread_manager_signal = g_io_add_watch_full(io_channel, 0, G_IO_IN,
						    thread_manager_event,
						    (gpointer) NULL,
						    NULL);
}

void nntp_main_done(gboolean have_connectivity)
{
	nntp_disconnect_all(have_connectivity);
	etpan_thread_manager_stop(thread_manager);
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	return;
#endif
	etpan_thread_manager_join(thread_manager);
	
	g_source_remove(thread_manager_signal);
	g_io_channel_unref(io_channel);
	
	etpan_thread_manager_free(thread_manager);
	
	chash_free(session_hash);
	chash_free(nntp_hash);
}

void nntp_init(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	
	thread = etpan_thread_manager_get_thread(thread_manager);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = thread;
	value.len = 0;
	
	chash_set(nntp_hash, &key, &value, NULL);
}

void nntp_done(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	int r;
	
	key.data = &folder;
	key.len = sizeof(folder);
	
	r = chash_get(nntp_hash, &key, &value);
	if (r < 0)
		return;
	
	thread = value.data;
	
	etpan_thread_unbind(thread);
	
	chash_delete(nntp_hash, &key, NULL);
	
	debug_print("remove thread\n");
}

static struct etpan_thread * get_thread(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	int r;

	key.data = &folder;
	key.len = sizeof(folder);

	r = chash_get(nntp_hash, &key, &value);
	if (r < 0)
		return NULL;

	thread = value.data;

	return thread;
}

static newsnntp * get_nntp(Folder * folder)
{
	newsnntp * nntp;
	chashdatum key;
	chashdatum value;
	int r;
	
	key.data = &folder;
	key.len = sizeof(folder);
	
	r = chash_get(session_hash, &key, &value);
	if (r < 0)
		return NULL;
	
	nntp = value.data;
	debug_print("found nntp %p\n", nntp);
	return nntp;
}


static void generic_cb(int cancelled, void * result, void * callback_data)
{
	struct etpan_thread_op * op;
	
	op = (struct etpan_thread_op *) callback_data;

	debug_print("generic_cb\n");
	op->finished = 1;
}

static void threaded_run(Folder * folder, void * param, void * result,
			 void (* func)(struct etpan_thread_op * ))
{
	struct etpan_thread_op * op;
	struct etpan_thread * thread;
	void (*previous_stream_logger)(int direction,
		const char * str, size_t size);

	nntp_folder_ref(folder);

	op = etpan_thread_op_new();
	
	op->nntp = get_nntp(folder);
	op->param = param;
	op->result = result;

	op->run = func;
	op->callback = generic_cb;
	op->callback_data = op;
	
	previous_stream_logger = mailstream_logger;
	mailstream_logger = nntp_logger;

	thread = get_thread(folder);
	etpan_thread_op_schedule(thread, op);
	
	while (!op->finished) {
		gtk_main_iteration();
	}
	
	mailstream_logger = previous_stream_logger;

	etpan_thread_op_free(op);

	nntp_folder_unref(folder);
}


/* connect */

struct connect_param {
	newsnntp * nntp;
	PrefsAccount *account;
	const char * server;
	int port;
	ProxyInfo * proxy_info;
};

struct connect_result {
	int error;
};

#define CHECK_NNTP() {						\
	if (!param->nntp) {					\
		result->error = NEWSNNTP_ERROR_BAD_STATE;	\
		return;						\
	}							\
}

static void connect_run(struct etpan_thread_op * op)
{
	int r;
	struct connect_param * param;
	struct connect_result * result;
	
	param = op->param;
	result = op->result;
	
	CHECK_NNTP();

	r = do_newsnntp_socket_connect(param->nntp,
				    param->server, param->port,
				    param->proxy_info);
	
	result->error = r;
}


int nntp_threaded_connect(Folder * folder, const char * server, int port, ProxyInfo *proxy_info)
{
	struct connect_param param;
	struct connect_result result;
	chashdatum key;
	chashdatum value;
	newsnntp * nntp, * oldnntp;
	
	oldnntp = get_nntp(folder);

	nntp = newsnntp_new(0, NULL);
	
	if (oldnntp) {
		debug_print("deleting old nntp %p\n", oldnntp);
		delete_nntp(folder, oldnntp);
	}
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = nntp;
	value.len = 0;
	chash_set(session_hash, &key, &value, NULL);
	
	param.nntp = nntp;
	param.server = server;
	param.port = port;
	param.proxy_info = proxy_info;

	refresh_resolvers();
	threaded_run(folder, &param, &result, connect_run);
	
	debug_print("connect ok %i with nntp %p\n", result.error, nntp);
	
	return result.error;
}
#ifdef USE_GNUTLS
static void connect_ssl_run(struct etpan_thread_op * op)
{
	int r;
	struct connect_param * param;
	struct connect_result * result;
	
	param = op->param;
	result = op->result;
	
	CHECK_NNTP();

	r = do_newsnntp_ssl_connect_with_callback(param->nntp,
				 param->server, param->port,
				 etpan_connect_ssl_context_cb, param->account,
				 param->proxy_info);
	result->error = r;
}

int nntp_threaded_connect_ssl(Folder * folder, const char * server, int port, ProxyInfo *proxy_info)
{
	struct connect_param param;
	struct connect_result result;
	chashdatum key;
	chashdatum value;
	newsnntp * nntp, * oldnntp;
	gboolean accept_if_valid = FALSE;

	oldnntp = get_nntp(folder);

	nntp = newsnntp_new(0, NULL);

	if (oldnntp) {
		debug_print("deleting old nntp %p\n", oldnntp);
		delete_nntp(folder, oldnntp);
	}

	key.data = &folder;
	key.len = sizeof(folder);
	value.data = nntp;
	value.len = 0;
	chash_set(session_hash, &key, &value, NULL);

	param.nntp = nntp;
	param.server = server;
	param.port = port;
	param.account = folder->account;
	param.proxy_info = proxy_info;

	if (folder->account)
		accept_if_valid = folder->account->ssl_certs_auto_accept;

	refresh_resolvers();
	threaded_run(folder, &param, &result, connect_ssl_run);

	if (result.error == NEWSNNTP_NO_ERROR && !etpan_skip_ssl_cert_check) {
		if (etpan_certificate_check(nntp->nntp_stream, server, port,
					    accept_if_valid) != TRUE)
			return -1;
	}
	debug_print("connect %d with nntp %p\n", result.error, nntp);
	
	return result.error;
}
#endif

void nntp_threaded_disconnect(Folder * folder)
{
	newsnntp * nntp;
	
	nntp = get_nntp(folder);
	if (nntp == NULL) {
		debug_print("was disconnected\n");
		return;
	}
	
	debug_print("deleting old nntp %p\n", nntp);
	delete_nntp(folder, nntp);
	
	debug_print("disconnect ok\n");
}

void nntp_threaded_cancel(Folder * folder)
{
	newsnntp * nntp;
	
	nntp = get_nntp(folder);
	if (nntp->nntp_stream != NULL)
		mailstream_cancel(nntp->nntp_stream);
}


struct login_param {
	newsnntp * nntp;
	const char * login;
	const char * password;
};

struct login_result {
	int error;
};

static void login_run(struct etpan_thread_op * op)
{
	struct login_param * param;
	struct login_result * result;
	int r;
#ifdef DISABLE_LOG_DURING_LOGIN
	int old_debug;
#endif
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

#ifdef DISABLE_LOG_DURING_LOGIN
	old_debug = mailstream_debug;
	mailstream_debug = 0;
#endif

	r = newsnntp_authinfo_username(param->nntp, param->login);
	/* libetpan returning NO_ERROR means it received resp.code 281:
	   in this case auth. is already successful, no password is needed. */
	if (r == NEWSNNTP_WARNING_REQUEST_AUTHORIZATION_PASSWORD) {
		r = newsnntp_authinfo_password(param->nntp, param->password);
	}
	


#ifdef DISABLE_LOG_DURING_LOGIN
	mailstream_debug = old_debug;
#endif
	
	result->error = r;
	if (param->nntp->nntp_response)
		nntp_logger(0, param->nntp->nntp_response, strlen(param->nntp->nntp_response));

	debug_print("nntp login run - end %i\n", r);
}

int nntp_threaded_login(Folder * folder, const char * login, const char * password)
{
	struct login_param param;
	struct login_result result;
	
	debug_print("nntp login - begin\n");
	
	param.nntp = get_nntp(folder);
	param.login = login;
	param.password = password;

	threaded_run(folder, &param, &result, login_run);
	
	debug_print("nntp login - end\n");
	
	return result.error;
}

struct date_param {
	newsnntp * nntp;
	struct tm * lt;
};

struct date_result {
	int error;
};

static void date_run(struct etpan_thread_op * op)
{
	struct date_param * param;
	struct date_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_date(param->nntp, param->lt);
	
	result->error = r;
	debug_print("nntp date run - end %i\n", r);
}

int nntp_threaded_date(Folder * folder, struct tm *lt)
{
	struct date_param param;
	struct date_result result;
	
	debug_print("nntp date - begin\n");
	
	param.nntp = get_nntp(folder);
	param.lt = lt;

	threaded_run(folder, &param, &result, date_run);
	
	debug_print("nntp date - end\n");
	
	return result.error;
}

struct list_param {
	newsnntp * nntp;
	clist **grouplist;
};

struct list_result {
	int error;
};

static void list_run(struct etpan_thread_op * op)
{
	struct list_param * param;
	struct list_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_list(param->nntp, param->grouplist);
	
	result->error = r;
	debug_print("nntp list run - end %i\n", r);
}

int nntp_threaded_list(Folder * folder, clist **grouplist)
{
	struct list_param param;
	struct list_result result;
	
	debug_print("nntp list - begin\n");
	
	param.nntp = get_nntp(folder);
	param.grouplist = grouplist;

	threaded_run(folder, &param, &result, list_run);
	
	debug_print("nntp list - end\n");
	
	return result.error;
}

struct post_param {
	newsnntp * nntp;
	char *contents;
	size_t len;
};

struct post_result {
	int error;
};

static void post_run(struct etpan_thread_op * op)
{
	struct post_param * param;
	struct post_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_post(param->nntp, param->contents, param->len);
	
	result->error = r;
	debug_print("nntp post run - end %i\n", r);
}

int nntp_threaded_post(Folder * folder, char *contents, size_t len)
{
	struct post_param param;
	struct post_result result;
	
	debug_print("nntp post - begin\n");
	
	param.nntp = get_nntp(folder);
	param.contents = contents;
	param.len = len;

	threaded_run(folder, &param, &result, post_run);
	
	debug_print("nntp post - end\n");
	
	return result.error;
}

struct article_param {
	newsnntp * nntp;
	guint32 num;
	char **contents;
	size_t *len;
};

struct article_result {
	int error;
};

static void article_run(struct etpan_thread_op * op)
{
	struct article_param * param;
	struct article_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_article(param->nntp, param->num, param->contents, param->len);
	
	result->error = r;
	debug_print("nntp article run - end %i\n", r);
}

int nntp_threaded_article(Folder * folder, guint32 num, char **contents, size_t *len)
{
	struct article_param param;
	struct article_result result;
	
	debug_print("nntp article - begin\n");
	
	param.nntp = get_nntp(folder);
	param.num = num;
	param.contents = contents;
	param.len = len;

	threaded_run(folder, &param, &result, article_run);
	
	debug_print("nntp article - end\n");
	
	return result.error;
}

struct group_param {
	newsnntp * nntp;
	const char *group;
	struct newsnntp_group_info **info;
};

struct group_result {
	int error;
};

static void group_run(struct etpan_thread_op * op)
{
	struct group_param * param;
	struct group_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_group(param->nntp, param->group, param->info);
	
	result->error = r;
	debug_print("nntp group run - end %i\n", r);
}

int nntp_threaded_group(Folder * folder, const char *group, struct newsnntp_group_info **info)
{
	struct group_param param;
	struct group_result result;
	
	debug_print("nntp group - begin\n");
	
	param.nntp = get_nntp(folder);
	param.group = group;
	param.info = info;

	threaded_run(folder, &param, &result, group_run);
	
	debug_print("nntp group - end\n");
	
	return result.error;
}

struct mode_reader_param {
	newsnntp * nntp;
};

struct mode_reader_result {
	int error;
};

static void mode_reader_run(struct etpan_thread_op * op)
{
	struct mode_reader_param * param;
	struct mode_reader_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();

	r = newsnntp_mode_reader(param->nntp);
	
	result->error = r;
	debug_print("nntp mode_reader run - end %i\n", r);
}

int nntp_threaded_mode_reader(Folder * folder)
{
	struct mode_reader_param param;
	struct mode_reader_result result;
	
	debug_print("nntp mode_reader - begin\n");
	
	param.nntp = get_nntp(folder);

	threaded_run(folder, &param, &result, mode_reader_run);
	
	debug_print("nntp mode_reader - end\n");
	
	return result.error;
}

struct xover_param {
	newsnntp * nntp;
	guint32 beg;
	guint32 end;
	struct newsnntp_xover_resp_item **result;
	clist **msglist;
};

struct xover_result {
	int error;
};

static void xover_run(struct etpan_thread_op * op)
{
	struct xover_param * param;
	struct xover_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();
	
	if (param->result) {
		r = newsnntp_xover_single(param->nntp, param->beg, param->result);
	} else {
		r = newsnntp_xover_range(param->nntp, param->beg, param->end, param->msglist);
	}
	
	result->error = r;
	debug_print("nntp xover run %d-%d - end %i\n",
			param->beg, param->end, r);
}

int nntp_threaded_xover(Folder * folder, guint32 beg, guint32 end, struct newsnntp_xover_resp_item **single_result, clist **multiple_result)
{
	struct xover_param param;
	struct xover_result result;
	clist *l = NULL, *h = NULL;
	guint32 cbeg = 0, cend = 0;

	debug_print("nntp xover - begin (%d-%d)\n", beg, end);

	h = clist_new();

	/* Request the overview in batches of NNTP_BATCH_SIZE, to prevent
	 * long stalls or libetpan choking on too large server response,
	 * and to allow updating any progress indicators while we work. */
	cbeg = beg;
	while (cbeg <= end && cend <= end) {
		cend = cbeg + (NNTP_BATCH_SIZE - 1);
		if (cend > end)
			cend = end;

		statusbar_progress_all(cbeg - beg, end - beg, 1);
		GTK_EVENTS_FLUSH();

		param.nntp = get_nntp(folder);
		param.beg = cbeg;
		param.end = cend;
		param.result = single_result;
		param.msglist = &l;

		threaded_run(folder, &param, &result, xover_run);

		/* Handle errors */
		if (result.error != NEWSNNTP_NO_ERROR) {
			log_warning(LOG_PROTOCOL, _("couldn't get xover range\n"));
			debug_print("couldn't get xover for %d-%d\n", cbeg, cend);
			if (l != NULL)
				newsnntp_xover_resp_list_free(l);
			newsnntp_xover_resp_list_free(h);
			statusbar_progress_all(0, 0, 0);
			return result.error;
		}

		/* Append the new data (l) to list of results (h). */
		if (l != NULL) {
			debug_print("total items so far %d, items this batch %d\n",
					clist_count(h), clist_count(l));
			clist_concat(h, l);
			clist_free(l);
			l = NULL;
		}

		cbeg += NNTP_BATCH_SIZE;
	}

	statusbar_progress_all(0, 0, 0);
	
	debug_print("nntp xover - end\n");

	*multiple_result = h;

	return result.error;
}

struct xhdr_param {
	newsnntp * nntp;
	const char *header;
	guint32 beg;
	guint32 end;
	clist **hdrlist;
};

struct xhdr_result {
	int error;
};

static void xhdr_run(struct etpan_thread_op * op)
{
	struct xhdr_param * param;
	struct xhdr_result * result;
	int r;
	
	param = op->param;
	result = op->result;

	CHECK_NNTP();
	
	if (param->beg == param->end) {
		r = newsnntp_xhdr_single(param->nntp, param->header, param->beg, param->hdrlist);
	} else {
		r = newsnntp_xhdr_range(param->nntp, param->header, param->beg, param->end, param->hdrlist);
	}
	
	result->error = r;
	debug_print("nntp xhdr '%s %d-%d' run - end %i\n",
			param->header, param->beg, param->end, r);
}

int nntp_threaded_xhdr(Folder * folder, const char *header, guint32 beg, guint32 end, clist **hdrlist)
{
	struct xhdr_param param;
	struct xhdr_result result;
	clist *l = NULL;
	clist *h = *hdrlist;
	guint32 cbeg = 0, cend = 0;

	debug_print("nntp xhdr %s - begin (%d-%d)\n", header, beg, end);

	if (h == NULL)
		h = clist_new();

	/* Request the headers in batches of NNTP_BATCH_SIZE, to prevent
	 * long stalls or libetpan choking on too large server response,
	 * and to allow updating any progress indicators while we work. */
	cbeg = beg;
	while (cbeg <= end && cend <= end) {
		cend = cbeg + NNTP_BATCH_SIZE - 1;
		if (cend > end)
			cend = end;

		statusbar_progress_all(cbeg - beg, end - beg, 1);
		GTK_EVENTS_FLUSH();

		param.nntp = get_nntp(folder);
		param.header = header;
		param.beg = cbeg;
		param.end = cend;
		param.hdrlist = &l;

		threaded_run(folder, &param, &result, xhdr_run);

		/* Handle errors */
		if (result.error != NEWSNNTP_NO_ERROR) {
			log_warning(LOG_PROTOCOL, _("couldn't get xhdr range\n"));
			debug_print("couldn't get xhdr %s %d-%d\n",	header, cbeg, cend);
			if (l != NULL)
				newsnntp_xhdr_free(l);
			newsnntp_xhdr_free(h);
			statusbar_progress_all(0, 0, 0);
			return result.error;
		}

		/* Append the new data (l) to list of results (h). */
		if (l != NULL) {
			debug_print("total items so far %d, items this batch %d\n",
					clist_count(h), clist_count(l));
			clist_concat(h, l);
			clist_free(l);
			l = NULL;
		}

		cbeg += NNTP_BATCH_SIZE;
	}

	statusbar_progress_all(0, 0, 0);
	
	debug_print("nntp xhdr %s - end (%d-%d)\n", header, beg, end);

	*hdrlist = h;

	return result.error;
}

void nntp_main_set_timeout(int sec)
{
	mailstream_network_delay.tv_sec = sec;
	mailstream_network_delay.tv_usec = 0;
}

#else

void nntp_main_init(void)
{
}
void nntp_main_done(gboolean have_connectivity)
{
}
void nntp_main_set_timeout(int sec)
{
}

void nntp_threaded_cancel(Folder * folder);
{
}

#endif
