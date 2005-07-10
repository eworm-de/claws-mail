#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_LIBETPAN

#include "imap-thread.h"
#include <imap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <gtk/gtk.h>
#include "etpan-thread-manager.h"

static struct etpan_thread_manager * thread_manager = NULL;
static chash * courier_workaround_hash = NULL;
static chash * imap_hash = NULL;
static chash * session_hash = NULL;
static guint thread_manager_signal = 0;
static GIOChannel * io_channel = NULL;


static gboolean thread_manager_event(GIOChannel * source,
    GIOCondition condition,
    gpointer data)
{
	etpan_thread_manager_loop(thread_manager);
	
	return TRUE;
}


#define ETPAN_DEFAULT_NETWORK_TIMEOUT 60

void imap_main_init(void)
{
	int fd_thread_manager;
	
	mailstream_network_delay.tv_sec = ETPAN_DEFAULT_NETWORK_TIMEOUT;
	mailstream_network_delay.tv_usec = 0;

#if 0
	mailstream_debug = 1;
#endif
	imap_hash = chash_new(CHASH_COPYKEY, CHASH_DEFAULTSIZE);
	session_hash = chash_new(CHASH_COPYKEY, CHASH_DEFAULTSIZE);
	courier_workaround_hash = chash_new(CHASH_COPYKEY, CHASH_DEFAULTSIZE);
	
	thread_manager = etpan_thread_manager_new();
	
	fd_thread_manager = etpan_thread_manager_get_fd(thread_manager);
	
	io_channel = g_io_channel_unix_new(fd_thread_manager);
	
	thread_manager_signal = g_io_add_watch_full(io_channel, 0, G_IO_IN,
						    thread_manager_event,
						    (gpointer) NULL,
						    NULL);
}

void imap_main_set_timeout(int sec)
{
	mailstream_network_delay.tv_sec = sec;
	mailstream_network_delay.tv_usec = 0;
}

void imap_main_done(void)
{
	etpan_thread_manager_stop(thread_manager);
	etpan_thread_manager_join(thread_manager);
	
	g_source_remove(thread_manager_signal);
	g_io_channel_unref(io_channel);
	
	etpan_thread_manager_free(thread_manager);
	
	chash_free(courier_workaround_hash);
	chash_free(session_hash);
	chash_free(imap_hash);
}

void imap_init(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	
	thread = etpan_thread_manager_get_thread(thread_manager);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = thread;
	value.len = 0;
	
	chash_set(imap_hash, &key, &value, NULL);
}

void imap_done(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	int r;
	
	key.data = &folder;
	key.len = sizeof(folder);
	
	r = chash_get(imap_hash, &key, &value);
	if (r < 0)
		return;
	
	thread = value.data;
	
	etpan_thread_unbind(thread);
	
	chash_delete(imap_hash, &key, NULL);
	
	debug_print("remove thread");
}

static struct etpan_thread * get_thread(Folder * folder)
{
	struct etpan_thread * thread;
	chashdatum key;
	chashdatum value;
	
	key.data = &folder;
	key.len = sizeof(folder);
	
	chash_get(imap_hash, &key, &value);
	thread = value.data;
	
	return thread;
}

static mailimap * get_imap(Folder * folder)
{
	mailimap * imap;
	chashdatum key;
	chashdatum value;
	int r;
	
	key.data = &folder;
	key.len = sizeof(folder);
	
	r = chash_get(session_hash, &key, &value);
	if (r < 0)
		return NULL;
	
	imap = value.data;
	
	return imap;
}


static void generic_cb(int cancelled, void * result, void * callback_data)
{
	int * p_finished;
	
	p_finished = callback_data;

	debug_print("generic_cb\n");
	
	* p_finished = 1;
}

static void threaded_run(Folder * folder, void * param, void * result,
			 void (* func)(struct etpan_thread_op * ))
{
	struct etpan_thread_op * op;
	struct etpan_thread * thread;
	int finished;
	
	imap_folder_ref(folder);

	op = etpan_thread_op_new();
	op->param = param;
	op->result = result;
	
	op->cancellable = 0;
	op->run = func;
	op->callback = generic_cb;
	op->callback_data = &finished;
	op->cleanup = NULL;
	
	finished = 0;
	
	thread = get_thread(folder);
	etpan_thread_op_schedule(thread, op);
	
	while (!finished) {
		gtk_main_iteration();
	}
	
	etpan_thread_op_free(op);

	imap_folder_unref(folder);
}


/* connect */

struct connect_param {
	mailimap * imap;
	const char * server;
	int port;
};

struct connect_result {
	int error;
};

static void connect_run(struct etpan_thread_op * op)
{
	int r;
	struct connect_param * param;
	struct connect_result * result;
	
	param = op->param;
	result = op->result;
	
	r = mailimap_socket_connect(param->imap,
				    param->server, param->port);
	
	result->error = r;
}


int imap_threaded_connect(Folder * folder, const char * server, int port)
{
	struct connect_param param;
	struct connect_result result;
	chashdatum key;
	chashdatum value;
	mailimap * imap;
	
	imap = mailimap_new(0, NULL);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = imap;
	value.len = 0;
	chash_set(session_hash, &key, &value, NULL);
	
	param.imap = imap;
	param.server = server;
	param.port = port;
	
	threaded_run(folder, &param, &result, connect_run);
	
	debug_print("connect ok %i\n", result.error);
	
	return result.error;
}


static void connect_ssl_run(struct etpan_thread_op * op)
{
	int r;
	struct connect_param * param;
	struct connect_result * result;
	
	param = op->param;
	result = op->result;
	
	r = mailimap_ssl_connect(param->imap,
				 param->server, param->port);
	
	result->error = r;
}

int imap_threaded_connect_ssl(Folder * folder, const char * server, int port)
{
	struct connect_param param;
	struct connect_result result;
	chashdatum key;
	chashdatum value;
	mailimap * imap;
	
	imap = mailimap_new(0, NULL);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = imap;
	value.len = 0;
	chash_set(session_hash, &key, &value, NULL);
	
	param.imap = imap;
	param.server = server;
	param.port = port;
	
	threaded_run(folder, &param, &result, connect_ssl_run);
	
	debug_print("connect ok\n");
	
	return result.error;
}
	
struct disconnect_param {
	mailimap * imap;
};

struct disconnect_result {
	int error;
};

static void disconnect_run(struct etpan_thread_op * op)
{
	int r;
	struct disconnect_param * param;
	struct disconnect_result * result;
	
	param = op->param;
	result = op->result;
	
	r = mailimap_logout(param->imap);
	
	result->error = r;
}

void imap_threaded_disconnect(Folder * folder)
{
	struct connect_param param;
	struct connect_result result;
	chashdatum key;
	chashdatum value;
	mailimap * imap;
	
	imap = get_imap(folder);
	if (imap == NULL) {
		debug_print("was disconnected\n");
		return;
	}
	
	param.imap = imap;
	
	threaded_run(folder, &param, &result, disconnect_run);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = imap;
	value.len = 0;
	chash_delete(session_hash, &key, NULL);
	
	key.data = &imap;
	key.len = sizeof(imap);
	chash_delete(courier_workaround_hash, &key, NULL);
	
	mailimap_free(imap);
	
	debug_print("disconnect ok\n");
}


struct list_param {
	mailimap * imap;
	const char * base;
	const char * wildcard;
};

struct list_result {
	int error;
	clist * list;
};

static void list_run(struct etpan_thread_op * op)
{
	struct list_param * param;
	struct list_result * result;
	int r;
	clist * list;
	
	param = op->param;
	list = NULL;
	r = mailimap_list(param->imap, param->base,
			  param->wildcard, &list);
	
	result = op->result;
	result->error = r;
	result->list = list;
	debug_print("imap list run - end\n");
}

int imap_threaded_list(Folder * folder, const char * base,
		       const char * wildcard,
		       clist ** p_result)
{
	struct list_param param;
	struct list_result result;
	
	debug_print("imap list - begin\n");
	
	param.imap = get_imap(folder);
	param.base = base;
	param.wildcard = wildcard;
	
	threaded_run(folder, &param, &result, list_run);
	
	* p_result = result.list;
	
	debug_print("imap list - end %p\n", result.list);
	
	return result.error;
}



struct login_param {
	mailimap * imap;
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
	
	param = op->param;
	r = mailimap_login(param->imap,
			   param->login, param->password);
	
	result = op->result;
	result->error = r;
	debug_print("imap login run - end %i\n", r);
}

int imap_threaded_login(Folder * folder,
			const char * login, const char * password)
{
	struct login_param param;
	struct login_result result;
	
	debug_print("imap login - begin\n");
	
	param.imap = get_imap(folder);
	param.login = login;
	param.password = password;
	
	threaded_run(folder, &param, &result, login_run);
	
	debug_print("imap login - end\n");
	
	return result.error;
}


struct status_param {
	mailimap * imap;
	const char * mb;
	struct mailimap_status_att_list * status_att_list;
};

struct status_result {
	int error;
	struct mailimap_mailbox_data_status * data_status;
};

static void status_run(struct etpan_thread_op * op)
{
	struct status_param * param;
	struct status_result * result;
	int r;
	
	param = op->param;
	result = op->result;
	
	r = mailimap_status(param->imap, param->mb,
			    param->status_att_list,
			    &result->data_status);
	
	result->error = r;
	debug_print("imap status run - end %i\n", r);
}

int imap_threaded_status(Folder * folder, const char * mb,
			 struct mailimap_mailbox_data_status ** data_status)
{
	struct status_param param;
	struct status_result result;
	struct mailimap_status_att_list * status_att_list;
	
	debug_print("imap status - begin\n");
	
	status_att_list = mailimap_status_att_list_new_empty();
	mailimap_status_att_list_add(status_att_list,
				     MAILIMAP_STATUS_ATT_MESSAGES);
	mailimap_status_att_list_add(status_att_list,
				     MAILIMAP_STATUS_ATT_RECENT);
	mailimap_status_att_list_add(status_att_list,
				     MAILIMAP_STATUS_ATT_UIDNEXT);
	mailimap_status_att_list_add(status_att_list,
				     MAILIMAP_STATUS_ATT_UIDVALIDITY);
	mailimap_status_att_list_add(status_att_list,
				     MAILIMAP_STATUS_ATT_UNSEEN);
	
	param.imap = get_imap(folder);
	param.mb = mb;
	param.status_att_list = status_att_list;
	
	threaded_run(folder, &param, &result, status_run);
	
	debug_print("imap status - end\n");
	
	* data_status = result.data_status;
	
	return result.error;
}



struct noop_param {
	mailimap * imap;
};

struct noop_result {
	int error;
};

static void noop_run(struct etpan_thread_op * op)
{
	struct noop_param * param;
	struct noop_result * result;
	int r;
	
	param = op->param;
	r = mailimap_noop(param->imap);
	
	result = op->result;
	result->error = r;
	debug_print("imap noop run - end %i\n", r);
}

int imap_threaded_noop(Folder * folder, unsigned int * p_exists)
{
	struct noop_param param;
	struct noop_result result;
	mailimap * imap;
	
	debug_print("imap noop - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	
	threaded_run(folder, &param, &result, noop_run);
	
	if (imap->imap_selection_info != NULL) {
		* p_exists = imap->imap_selection_info->sel_exists;
	}
	else {
		* p_exists = 0;
	}
	
	debug_print("imap noop - end\n");
	
	return result.error;
}


struct starttls_param {
	mailimap * imap;
};

struct starttls_result {
	int error;
};

static void starttls_run(struct etpan_thread_op * op)
{
	struct starttls_param * param;
	struct starttls_result * result;
	int r;
	
	param = op->param;
	r = mailimap_starttls(param->imap);
	
	result = op->result;
	result->error = r;
	debug_print("imap starttls run - end %i\n", r);
}

int imap_threaded_starttls(Folder * folder)
{
	struct starttls_param param;
	struct starttls_result result;
	
	debug_print("imap starttls - begin\n");
	
	param.imap = get_imap(folder);
	
	threaded_run(folder, &param, &result, starttls_run);
	
	debug_print("imap starttls - end\n");
	
	return result.error;
}



struct create_param {
	mailimap * imap;
	const char * mb;
};

struct create_result {
	int error;
};

static void create_run(struct etpan_thread_op * op)
{
	struct create_param * param;
	struct create_result * result;
	int r;
	
	param = op->param;
	r = mailimap_create(param->imap, param->mb);
	
	result = op->result;
	result->error = r;
	debug_print("imap create run - end %i\n", r);
}

int imap_threaded_create(Folder * folder, const char * mb)
{
	struct create_param param;
	struct create_result result;
	
	debug_print("imap create - begin\n");
	
	param.imap = get_imap(folder);
	param.mb = mb;
	
	threaded_run(folder, &param, &result, create_run);
	
	debug_print("imap create - end\n");
	
	return result.error;
}




struct rename_param {
	mailimap * imap;
	const char * mb;
	const char * new_name;
};

struct rename_result {
	int error;
};

static void rename_run(struct etpan_thread_op * op)
{
	struct rename_param * param;
	struct rename_result * result;
	int r;
	
	param = op->param;
	r = mailimap_rename(param->imap, param->mb, param->new_name);
	
	result = op->result;
	result->error = r;
	debug_print("imap rename run - end %i\n", r);
}

int imap_threaded_rename(Folder * folder,
			 const char * mb, const char * new_name)
{
	struct rename_param param;
	struct rename_result result;
	
	debug_print("imap rename - begin\n");
	
	param.imap = get_imap(folder);
	param.mb = mb;
	param.new_name = new_name;
	
	threaded_run(folder, &param, &result, rename_run);
	
	debug_print("imap rename - end\n");
	
	return result.error;
}




struct delete_param {
	mailimap * imap;
	const char * mb;
};

struct delete_result {
	int error;
};

static void delete_run(struct etpan_thread_op * op)
{
	struct delete_param * param;
	struct delete_result * result;
	int r;
	
	param = op->param;
	r = mailimap_delete(param->imap, param->mb);
	
	result = op->result;
	result->error = r;
	debug_print("imap delete run - end %i\n", r);
}

int imap_threaded_delete(Folder * folder, const char * mb)
{
	struct delete_param param;
	struct delete_result result;
	
	debug_print("imap delete - begin\n");
	
	param.imap = get_imap(folder);
	param.mb = mb;
	
	threaded_run(folder, &param, &result, delete_run);
	
	debug_print("imap delete - end\n");
	
	return result.error;
}



struct select_param {
	mailimap * imap;
	const char * mb;
};

struct select_result {
	int error;
};

static void select_run(struct etpan_thread_op * op)
{
	struct select_param * param;
	struct select_result * result;
	int r;
	
	param = op->param;
	r = mailimap_select(param->imap, param->mb);
	
	result = op->result;
	result->error = r;
	debug_print("imap select run - end %i\n", r);
}

int imap_threaded_select(Folder * folder, const char * mb,
			 gint * exists, gint * recent, gint * unseen,
			 guint32 * uid_validity)
{
	struct select_param param;
	struct select_result result;
	mailimap * imap;
	
	debug_print("imap select - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.mb = mb;
	
	threaded_run(folder, &param, &result, select_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	if (imap->imap_selection_info == NULL)
		return MAILIMAP_ERROR_PARSE;
	
	* exists = imap->imap_selection_info->sel_exists;
	* recent = imap->imap_selection_info->sel_recent;
	* unseen = imap->imap_selection_info->sel_unseen;
	* uid_validity = imap->imap_selection_info->sel_uidvalidity;
	
	debug_print("imap select - end\n");
	
	return result.error;
}



struct examine_param {
	mailimap * imap;
	const char * mb;
};

struct examine_result {
	int error;
};

static void examine_run(struct etpan_thread_op * op)
{
	struct examine_param * param;
	struct examine_result * result;
	int r;
	
	param = op->param;
	r = mailimap_examine(param->imap, param->mb);
	
	result = op->result;
	result->error = r;
	debug_print("imap examine run - end %i\n", r);
}

int imap_threaded_examine(Folder * folder, const char * mb,
			  gint * exists, gint * recent, gint * unseen,
			  guint32 * uid_validity)
{
	struct examine_param param;
	struct examine_result result;
	mailimap * imap;
	
	debug_print("imap examine - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.mb = mb;
	
	threaded_run(folder, &param, &result, examine_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	if (imap->imap_selection_info == NULL)
		return MAILIMAP_ERROR_PARSE;
	
	* exists = imap->imap_selection_info->sel_exists;
	* recent = imap->imap_selection_info->sel_recent;
	* unseen = imap->imap_selection_info->sel_unseen;
	* uid_validity = imap->imap_selection_info->sel_uidvalidity;
	
	debug_print("imap examine - end\n");
	
	return result.error;
}




struct search_param {
	mailimap * imap;
	int type;
	struct mailimap_set * set;
};

struct search_result {
	int error;
	clist * search_result;
};

static void search_run(struct etpan_thread_op * op)
{
	struct search_param * param;
	struct search_result * result;
	int r;
	struct mailimap_search_key * key;
	struct mailimap_search_key * uid_key;
	struct mailimap_search_key * search_type_key;
	clist * search_result;
	
	param = op->param;
	
	uid_key = mailimap_search_key_new_uid(param->set);
	
	search_type_key = NULL;
	switch (param->type) {
	case IMAP_SEARCH_TYPE_SIMPLE:
		search_type_key = NULL;
		break;
		
	case IMAP_SEARCH_TYPE_SEEN:
		search_type_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_SEEN,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, 0,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, 0, NULL, NULL, NULL);
		break;
		
	case IMAP_SEARCH_TYPE_UNSEEN:
		search_type_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_UNSEEN,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, 0,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, 0, NULL, NULL, NULL);
		break;
		
	case IMAP_SEARCH_TYPE_ANSWERED:
		search_type_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_ANSWERED,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, 0,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, 0, NULL, NULL, NULL);
		break;
		
	case IMAP_SEARCH_TYPE_FLAGGED:
		search_type_key = mailimap_search_key_new(MAILIMAP_SEARCH_KEY_FLAGGED,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, NULL, NULL, NULL, 0,
							  NULL, NULL, NULL, NULL, NULL,
							  NULL, 0, NULL, NULL, NULL);
		break;
	}
	
	if (search_type_key != NULL) {
		key = mailimap_search_key_new_multiple_empty();
		mailimap_search_key_multiple_add(key, search_type_key);
		mailimap_search_key_multiple_add(key, uid_key);
	}
	else {
		key = uid_key;
	}
	
	r = mailimap_uid_search(param->imap, NULL, key, &search_result);
	
	result = op->result;
	result->error = r;
	result->search_result = search_result;
	debug_print("imap search run - end %i\n", r);
}

int imap_threaded_search(Folder * folder, int search_type,
			 struct mailimap_set * set, clist ** search_result)
{
	struct search_param param;
	struct search_result result;
	mailimap * imap;
	
	debug_print("imap search - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.set = set;
	param.type = search_type;
	
	threaded_run(folder, &param, &result, search_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap search - end\n");
	
	* search_result = result.search_result;
	
	return result.error;
}




static int
uid_list_to_env_list(clist * fetch_result, carray ** result)
{
	clistiter * cur;
	int r;
	int res;
	carray * tab;
	unsigned int i;
	
	tab = carray_new(128);
	if (tab == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto err;
	}
	
	for(cur = clist_begin(fetch_result) ; cur != NULL ;
	    cur = clist_next(cur)) {
		struct mailimap_msg_att * msg_att;
		clistiter * item_cur;
		uint32_t uid;
		size_t size;
		uint32_t * puid;
		
		msg_att = clist_content(cur);
		
		uid = 0;
		size = 0;
		for(item_cur = clist_begin(msg_att->att_list) ;
		    item_cur != NULL ;
		    item_cur = clist_next(item_cur)) {
			struct mailimap_msg_att_item * item;
			
			item = clist_content(item_cur);
			
			switch (item->att_type) {
			case MAILIMAP_MSG_ATT_ITEM_STATIC:
				switch (item->att_data.att_static->att_type) {
				case MAILIMAP_MSG_ATT_UID:
					uid = item->att_data.att_static->att_data.att_uid;
					break;
				}
			}
		}
		
		puid = malloc(sizeof(* puid));
		if (puid == NULL) {
			res = MAILIMAP_ERROR_MEMORY;
			goto free_list;
		}
		* puid = uid;
			
		r = carray_add(tab, puid, NULL);
		if (r < 0) {
			free(puid);
			res = MAILIMAP_ERROR_MEMORY;
			goto free_list;
		}
	}
		
	* result = tab;

	return MAILIMAP_NO_ERROR;
  
 free_list:
	for(i = 0 ; i < carray_count(tab) ; i++)
		mailmessage_free(carray_get(tab, i));
 err:
	return res;
}

static int imap_get_messages_list(mailimap * imap,
				  uint32_t first_index,
				  carray ** result)
{
	carray * env_list;
	int r;
	struct mailimap_fetch_att * fetch_att;
	struct mailimap_fetch_type * fetch_type;
	struct mailimap_set * set;
	clist * fetch_result;
	int res;
	
	set = mailimap_set_new_interval(first_index, 0);
	if (set == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto err;
	}

	fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
	if (fetch_type == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto free_set;
	}

	fetch_att = mailimap_fetch_att_new_uid();
	if (fetch_att == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto free_fetch_type;
	}

	r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
	if (r != MAILIMAP_NO_ERROR) {
		mailimap_fetch_att_free(fetch_att);
		res = MAILIMAP_ERROR_MEMORY;
		goto free_fetch_type;
	}

	r = mailimap_uid_fetch(imap, set,
			       fetch_type, &fetch_result);

	mailimap_fetch_type_free(fetch_type);
	mailimap_set_free(set);

	if (r != MAILIMAP_NO_ERROR) {
		res = r;
		goto err;
	}

	env_list = NULL;
	r = uid_list_to_env_list(fetch_result, &env_list);
	mailimap_fetch_list_free(fetch_result);
	
	* result = env_list;

	return MAILIMAP_NO_ERROR;

 free_fetch_type:
	mailimap_fetch_type_free(fetch_type);
 free_set:
	mailimap_set_free(set);
 err:
	return res;
}




struct fetch_uid_param {
	mailimap * imap;
	uint32_t first_index;
};

struct fetch_uid_result {
	int error;
	carray * fetch_result;
};

static void fetch_uid_run(struct etpan_thread_op * op)
{
	struct fetch_uid_param * param;
	struct fetch_uid_result * result;
	carray * fetch_result;
	int r;
	
	param = op->param;
	
	fetch_result = NULL;
	r = imap_get_messages_list(param->imap, param->first_index,
				   &fetch_result);
	
	result = op->result;
	result->error = r;
	result->fetch_result = fetch_result;
	debug_print("imap fetch_uid run - end %i\n", r);
}

int imap_threaded_fetch_uid(Folder * folder, uint32_t first_index,
			    carray ** fetch_result)
{
	struct fetch_uid_param param;
	struct fetch_uid_result result;
	mailimap * imap;
	
	debug_print("imap fetch_uid - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.first_index = first_index;
	
	threaded_run(folder, &param, &result, fetch_uid_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap fetch_uid - end\n");
	
	* fetch_result = result.fetch_result;
	
	return result.error;
}


void imap_fetch_uid_list_free(carray * uid_list)
{
	unsigned int i;
	
	for(i = 0 ; i < carray_count(uid_list) ; i ++) {
		uint32_t * puid;
		
		puid = carray_get(uid_list, i);
		free(puid);
	}
	carray_free(uid_list);
}



static int imap_fetch(mailimap * imap,
		      uint32_t msg_index,
		      char ** result,
		      size_t * result_len)
{
	int r;
	struct mailimap_set * set;
	struct mailimap_fetch_att * fetch_att;
	struct mailimap_fetch_type * fetch_type;
	clist * fetch_result;
	struct mailimap_msg_att * msg_att;
	struct mailimap_msg_att_item * msg_att_item;
	char * text;
	size_t text_length;
	int res;
	clistiter * cur;
	struct mailimap_section * section;

	set = mailimap_set_new_single(msg_index);
	if (set == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto err;
	}

	section = mailimap_section_new(NULL);
	if (section == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto free_set;
	}
  
	fetch_att = mailimap_fetch_att_new_body_peek_section(section);
	if (fetch_att == NULL) {
		mailimap_section_free(section);
		res = MAILIMAP_ERROR_MEMORY;
		goto free_set;
	}
  
	fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
	if (fetch_type == NULL) {
		res = MAILIMAP_ERROR_MEMORY;
		goto free_fetch_att;
	}

	r = mailimap_uid_fetch(imap, set,
			       fetch_type, &fetch_result);
  
	mailimap_fetch_type_free(fetch_type);
	mailimap_set_free(set);
  
	switch (r) {
	case MAILIMAP_NO_ERROR:
		break;
	default:
		return r;
	}
  
	if (clist_begin(fetch_result) == NULL) {
		mailimap_fetch_list_free(fetch_result);
		return MAILIMAP_ERROR_FETCH;
	}

	msg_att = clist_begin(fetch_result)->data;

	text = NULL;
	text_length = 0;

	for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
	    cur = clist_next(cur)) {
		msg_att_item = clist_content(cur);

		if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
			if (msg_att_item->att_data.att_static->att_type ==
			    MAILIMAP_MSG_ATT_BODY_SECTION) {
				text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
				/* detach */
				msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
				text_length =
					msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
			}
		}
	}

	mailimap_fetch_list_free(fetch_result);

	if (text == NULL)
		return MAILIMAP_ERROR_FETCH;

	* result = text;
	* result_len = text_length;
  
	return MAILIMAP_NO_ERROR;

 free_fetch_att:
	mailimap_fetch_att_free(fetch_att);
 free_set:
	mailimap_set_free(set);
 err:
	return res;
}

static int imap_fetch_header(mailimap * imap,
			     uint32_t msg_index,
			     char ** result,
			     size_t * result_len)
{
  int r;
  struct mailimap_set * set;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  clist * fetch_result;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * msg_att_item;
  char * text;
  size_t text_length;
  int res;
  clistiter * cur;
  struct mailimap_section * section;
  
  set = mailimap_set_new_single(msg_index);
  if (set == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }

  section = mailimap_section_new_header();
  if (section == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    res = MAILIMAP_ERROR_MEMORY;
    goto free_set;
  }
  
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  if (fetch_type == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_fetch_att;
  }

  r = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);
  
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return r;
  }

  if (clist_begin(fetch_result) == NULL) {
    mailimap_fetch_list_free(fetch_result);
    return MAILIMAP_ERROR_FETCH;
  }

  msg_att = clist_begin(fetch_result)->data;

  text = NULL;
  text_length = 0;

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    msg_att_item = clist_content(cur);

    if (msg_att_item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
      if (msg_att_item->att_data.att_static->att_type ==
	  MAILIMAP_MSG_ATT_BODY_SECTION) {
	text = msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part;
	msg_att_item->att_data.att_static->att_data.att_body_section->sec_body_part = NULL;
	text_length =
	  msg_att_item->att_data.att_static->att_data.att_body_section->sec_length;
      }
    }
  }

  mailimap_fetch_list_free(fetch_result);

  if (text == NULL)
    return MAILIMAP_ERROR_FETCH;

  * result = text;
  * result_len = text_length;

  return MAILIMAP_NO_ERROR;

 free_fetch_att:
  mailimap_fetch_att_free(fetch_att);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}



struct fetch_content_param {
	mailimap * imap;
	uint32_t msg_index;
	const char * filename;
	int with_body;
};

struct fetch_content_result {
	int error;
};

static void fetch_content_run(struct etpan_thread_op * op)
{
	struct fetch_content_param * param;
	struct fetch_content_result * result;
	char * content;
	size_t content_size;
	int r;
	int fd;
	FILE * f;
	
	param = op->param;
	
	content = NULL;
	content_size = 0;
	if (param->with_body)
		r = imap_fetch(param->imap, param->msg_index,
			       &content, &content_size);
	else
		r = imap_fetch_header(param->imap, param->msg_index,
				      &content, &content_size);
	
	result = op->result;
	result->error = r;
	
	if (r == MAILIMAP_NO_ERROR) {
		fd = open(param->filename, O_RDWR | O_CREAT, 0600);
		if (fd < 0) {
			result->error = MAILIMAP_ERROR_FETCH;
			goto free;
		}
		
		f = fdopen(fd, "wb");
		if (f == NULL) {
			result->error = MAILIMAP_ERROR_FETCH;
			goto close;
		}
		
		r = fwrite(content, 1, content_size, f);
		if (r == 0) {
			goto fclose;
		}
		
		r = fclose(f);
		if (r == EOF) {
			unlink(param->filename);
			goto close;
		}
		goto free;
		
	fclose:
		fclose(f);
		goto unlink;
	close:
		close(fd);
	unlink:
		unlink(param->filename);
	
	free:
		if (mmap_string_unref(content) != 0)
			free(content);
	}
	
	debug_print("imap fetch_content run - end %i\n", r);
}

int imap_threaded_fetch_content(Folder * folder, uint32_t msg_index,
				int with_body,
				const char * filename)
{
	struct fetch_content_param param;
	struct fetch_content_result result;
	mailimap * imap;
	
	debug_print("imap fetch_content - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.msg_index = msg_index;
	param.filename = filename;
	param.with_body = with_body;
	
	threaded_run(folder, &param, &result, fetch_content_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap fetch_content - end\n");
	
	return result.error;
}



static int imap_flags_to_flags(struct mailimap_msg_att_dynamic * att_dyn)
{
	int flags;
	clist * flag_list;
	clistiter * cur;
	
	flags = MSG_UNREAD;
	
	flag_list = att_dyn->att_list;
	if (flag_list == NULL)
		return flags;
	
	for(cur = clist_begin(flag_list) ; cur != NULL ;
	    cur = clist_next(cur)) {
		struct mailimap_flag_fetch * flag_fetch;
			
		flag_fetch = clist_content(cur);
		if (flag_fetch->fl_type == MAILIMAP_FLAG_FETCH_RECENT)
			flags |= MSG_NEW;
		else {
			switch (flag_fetch->fl_flag->fl_type) {
			case MAILIMAP_FLAG_ANSWERED:
				flags |= MSG_REPLIED;
				break;
			case MAILIMAP_FLAG_FLAGGED:
				flags |= MSG_MARKED;
				break;
			case MAILIMAP_FLAG_DELETED:
				flags |= MSG_DELETED;
				break;
			case MAILIMAP_FLAG_SEEN:
				flags &= ~MSG_UNREAD;
				flags &= ~MSG_NEW;
				break;
			}
		}
	}
	
	return flags;
}

static int imap_get_msg_att_info(struct mailimap_msg_att * msg_att,
				 uint32_t * puid,
				 char ** pheaders,
				 size_t * pref_size,
				 struct mailimap_msg_att_dynamic ** patt_dyn)
{
  clistiter * item_cur;
  uint32_t uid;
  char * headers;
  size_t ref_size;
  struct mailimap_msg_att_dynamic * att_dyn;

  uid = 0;
  headers = NULL;
  ref_size = 0;
  att_dyn = NULL;

  for(item_cur = clist_begin(msg_att->att_list) ; item_cur != NULL ;
      item_cur = clist_next(item_cur)) {
    struct mailimap_msg_att_item * item;

    item = clist_content(item_cur);
      
    switch (item->att_type) {
    case MAILIMAP_MSG_ATT_ITEM_STATIC:
      switch (item->att_data.att_static->att_type) {
      case MAILIMAP_MSG_ATT_UID:
	uid = item->att_data.att_static->att_data.att_uid;
	break;

      case MAILIMAP_MSG_ATT_BODY_SECTION:
	if (headers == NULL) {
	  headers = item->att_data.att_static->att_data.att_body_section->sec_body_part;
	}
	break;
      case MAILIMAP_MSG_ATT_RFC822_SIZE:
	      ref_size = item->att_data.att_static->att_data.att_rfc822_size;
	      break;
      }
      break;
      
    case MAILIMAP_MSG_ATT_ITEM_DYNAMIC:
      if (att_dyn == NULL) {
	att_dyn = item->att_data.att_dyn;
      }
      break;
    }
  }

  if (puid != NULL)
    * puid = uid;
  if (pheaders != NULL)
    * pheaders = headers;
  if (pref_size != NULL)
    * pref_size = ref_size;
  if (patt_dyn != NULL)
    * patt_dyn = att_dyn;

  return MAIL_NO_ERROR;
}

static struct imap_fetch_env_info *
fetch_to_env_info(struct mailimap_msg_att * msg_att)
{
	struct imap_fetch_env_info * info;
	uint32_t uid;
	char * headers;
	size_t size;
	struct mailimap_msg_att_dynamic * att_dyn;
	
	imap_get_msg_att_info(msg_att, &uid, &headers, &size,
			      &att_dyn);
	
	info = malloc(sizeof(* info));
	info->uid = uid;
	info->headers = strdup(headers);
	info->size = size;
	info->flags = imap_flags_to_flags(att_dyn);
	
	return info;
}

static int
imap_fetch_result_to_envelop_list(clist * fetch_result,
				  carray ** p_env_list)
{
	clistiter * cur;
	unsigned int i;
	carray * env_list;
  
	i = 0;
  
	env_list = carray_new(16);
  
	for(cur = clist_begin(fetch_result) ; cur != NULL ;
	    cur = clist_next(cur)) {
		struct mailimap_msg_att * msg_att;
		struct imap_fetch_env_info * env_info;
    
		msg_att = clist_content(cur);

		env_info = fetch_to_env_info(msg_att);
		carray_add(env_list, env_info, NULL);
	}
  
	* p_env_list = env_list;
  
	return MAIL_NO_ERROR;
}

int imap_add_envelope_fetch_att(struct mailimap_fetch_type * fetch_type)
{
	struct mailimap_fetch_att * fetch_att;
	int r;
	char * header;
	clist * hdrlist;
	struct mailimap_header_list * imap_hdrlist;
	struct mailimap_section * section;

	hdrlist = clist_new();
  
	header = strdup("Date");
	r = clist_append(hdrlist, header);
	header = strdup("From");
	r = clist_append(hdrlist, header);
	header = strdup("To");
	r = clist_append(hdrlist, header);
	header = strdup("Cc");
	r = clist_append(hdrlist, header);
	header = strdup("Subject");
	r = clist_append(hdrlist, header);
	header = strdup("Message-ID");
	r = clist_append(hdrlist, header);
	header = strdup("References");
	r = clist_append(hdrlist, header);
	header = strdup("In-Reply-To");
	r = clist_append(hdrlist, header);
  
	imap_hdrlist = mailimap_header_list_new(hdrlist);
	section = mailimap_section_new_header_fields(imap_hdrlist);
	fetch_att = mailimap_fetch_att_new_body_peek_section(section);
	mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  
	return MAIL_NO_ERROR;
}

int imap_add_header_fetch_att(struct mailimap_fetch_type * fetch_type)
{
	struct mailimap_fetch_att * fetch_att;
	struct mailimap_section * section;
	
	section = mailimap_section_new_header();
	fetch_att = mailimap_fetch_att_new_body_peek_section(section);
	mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
	
	return MAIL_NO_ERROR;
}

static int
imap_get_envelopes_list(mailimap * imap, struct mailimap_set * set,
			carray ** p_env_list)
{
	struct mailimap_fetch_att * fetch_att;
	struct mailimap_fetch_type * fetch_type;
	int res;
	clist * fetch_result;
	int r;
	carray * env_list;
	chashdatum key;
	chashdatum value;
	
	fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  
	/* uid */
	fetch_att = mailimap_fetch_att_new_uid();
	r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  
	/* flags */
	fetch_att = mailimap_fetch_att_new_flags();
	r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  
	/* rfc822 size */
	fetch_att = mailimap_fetch_att_new_rfc822_size();
	r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  
	/* headers */
	key.data = &imap;
	key.len = sizeof(imap);
	r = chash_get(courier_workaround_hash, &key, &value);
	if (r < 0)
		r = imap_add_envelope_fetch_att(fetch_type);
	else
		r = imap_add_header_fetch_att(fetch_type);
	
	r = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);
	
	switch (r) {
	case MAILIMAP_NO_ERROR:
		break;
	default:
		mailimap_fetch_type_free(fetch_type);
		return r;
	}
	
	if (clist_begin(fetch_result) == NULL) {
		res = MAILIMAP_ERROR_FETCH;
		goto err;
	}
	
	r = imap_fetch_result_to_envelop_list(fetch_result, &env_list);
	mailimap_fetch_list_free(fetch_result);
	
	if (r != MAILIMAP_NO_ERROR) {
		mailimap_fetch_type_free(fetch_type);
		res = MAILIMAP_ERROR_MEMORY;
		goto err;
	}
	
	mailimap_fetch_type_free(fetch_type);
	
	* p_env_list = env_list;
	
	return MAILIMAP_NO_ERROR;
  
 err:
	return res;
}

struct fetch_env_param {
	mailimap * imap;
	struct mailimap_set * set;
};

struct fetch_env_result {
	carray * fetch_env_result;
	int error;
};

static void fetch_env_run(struct etpan_thread_op * op)
{
	struct fetch_env_param * param;
	struct fetch_env_result * result;
	carray * env_list;
	int r;
	
	param = op->param;
	
	env_list = NULL;
	r = imap_get_envelopes_list(param->imap, param->set,
				    &env_list);
	
	result = op->result;
	result->error = r;
	result->fetch_env_result = env_list;
	
	debug_print("imap fetch_env run - end %i\n", r);
}

int imap_threaded_fetch_env(Folder * folder, struct mailimap_set * set,
			    carray ** p_env_list)
{
	struct fetch_env_param param;
	struct fetch_env_result result;
	mailimap * imap;
	
	debug_print("imap fetch_env - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.set = set;
	
	threaded_run(folder, &param, &result, fetch_env_run);
	
	if (result.error != MAILIMAP_NO_ERROR) {
		chashdatum key;
		chashdatum value;
		int r;
		
		key.data = &imap;
		key.len = sizeof(imap);
		r = chash_get(courier_workaround_hash, &key, &value);
		if (r < 0) {
			value.data = NULL;
			value.len = 0;
			chash_set(courier_workaround_hash, &key, &value, NULL);
			
			threaded_run(folder, &param, &result, fetch_env_run);
		}
	}
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap fetch_env - end\n");
	
	* p_env_list = result.fetch_env_result;
	
	return result.error;
}

void imap_fetch_env_free(carray * env_list)
{
	unsigned int i;
	
	for(i = 0 ; i < carray_count(env_list) ; i ++) {
		struct imap_fetch_env_info * env_info;
		
		env_info = carray_get(env_list, i);
		free(env_info->headers);
		free(env_info);
	}
	carray_free(env_list);
}





struct append_param {
	mailimap * imap;
	const char * mailbox;
	const char * filename;
	struct mailimap_flag_list * flag_list;
};

struct append_result {
	int error;
};

static void append_run(struct etpan_thread_op * op)
{
	struct append_param * param;
	struct append_result * result;
	int r;
	char * data;
	size_t size;
	struct stat stat_buf;
	int fd;
	
	param = op->param;
	result = op->result;
	
	r = stat(param->filename, &stat_buf);
	if (r < 0) {
		result->error = MAILIMAP_ERROR_APPEND;
		return;
	}
	size = stat_buf.st_size;
	
	fd = open(param->filename, O_RDONLY);
	if (fd < 0) {
		result->error = MAILIMAP_ERROR_APPEND;
		return;
	}
	
	data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == (void *) MAP_FAILED) {
		close(fd);
		result->error = MAILIMAP_ERROR_APPEND;
		return;
	}
	
	r = mailimap_append(param->imap, param->mailbox,
			    param->flag_list, NULL,
			    data, size);
	
	munmap(data, size);
	close(fd);
	
	result->error = r;
	
	debug_print("imap append run - end %i\n", r);
}

int imap_threaded_append(Folder * folder, const char * mailbox,
			 const char * filename,
			 struct mailimap_flag_list * flag_list)
{
	struct append_param param;
	struct append_result result;
	mailimap * imap;
	
	debug_print("imap append - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.mailbox = mailbox;
	param.filename = filename;
	param.flag_list = flag_list;
	
	threaded_run(folder, &param, &result, append_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap append - end\n");
	
	return result.error;
}




struct expunge_param {
	mailimap * imap;
};

struct expunge_result {
	int error;
};

static void expunge_run(struct etpan_thread_op * op)
{
	struct expunge_param * param;
	struct expunge_result * result;
	int r;
	
	param = op->param;
	r = mailimap_expunge(param->imap);
	
	result = op->result;
	result->error = r;
	debug_print("imap expunge run - end %i\n", r);
}

int imap_threaded_expunge(Folder * folder)
{
	struct expunge_param param;
	struct expunge_result result;
	
	debug_print("imap expunge - begin\n");
	
	param.imap = get_imap(folder);
	
	threaded_run(folder, &param, &result, expunge_run);
	
	debug_print("imap expunge - end\n");
	
	return result.error;
}


struct copy_param {
	mailimap * imap;
	struct mailimap_set * set;
	const char * mb;
};

struct copy_result {
	int error;
};

static void copy_run(struct etpan_thread_op * op)
{
	struct copy_param * param;
	struct copy_result * result;
	int r;
	
	param = op->param;
	
	r = mailimap_uid_copy(param->imap, param->set, param->mb);
	
	result = op->result;
	result->error = r;
	
	debug_print("imap copy run - end %i\n", r);
}

int imap_threaded_copy(Folder * folder, struct mailimap_set * set,
		       const char * mb)
{
	struct copy_param param;
	struct copy_result result;
	mailimap * imap;
	
	debug_print("imap copy - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.set = set;
	param.mb = mb;
	
	threaded_run(folder, &param, &result, copy_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap copy - end\n");
	
	return result.error;
}



struct store_param {
	mailimap * imap;
	struct mailimap_set * set;
	struct mailimap_store_att_flags * store_att_flags;
};

struct store_result {
	int error;
};

static void store_run(struct etpan_thread_op * op)
{
	struct store_param * param;
	struct store_result * result;
	int r;
	
	param = op->param;
	
	r = mailimap_uid_store(param->imap, param->set,
			       param->store_att_flags);
	
	result = op->result;
	result->error = r;
	
	debug_print("imap store run - end %i\n", r);
}

int imap_threaded_store(Folder * folder, struct mailimap_set * set,
			struct mailimap_store_att_flags * store_att_flags)
{
	struct store_param param;
	struct store_result result;
	mailimap * imap;
	
	debug_print("imap store - begin\n");
	
	imap = get_imap(folder);
	param.imap = imap;
	param.set = set;
	param.store_att_flags = store_att_flags;
	
	threaded_run(folder, &param, &result, store_run);
	
	if (result.error != MAILIMAP_NO_ERROR)
		return result.error;
	
	debug_print("imap store - end\n");
	
	return result.error;
}



static void do_exec_command(int fd, const char * command,
			    const char * servername, uint16_t port)
{
	int i, maxopen;
  
	if (fork() > 0) {
		/* Fork again to become a child of init rather than
		   the etpan client. */
		exit(0);
	}
  
	if (servername)
		setenv("ETPANSERVER", servername, 1);
	else
		unsetenv("ETPANSERVER");
  
	if (port) {
		char porttext[20];
    
		snprintf(porttext, sizeof(porttext), "%d", port);
		setenv("ETPANPORT", porttext, 1);
	}
	else {
		unsetenv("ETPANPORT");
	}
  
	/* Not a lot we can do if there's an error other than bail. */
	if (dup2(fd, 0) == -1)
		exit(1);
	if (dup2(fd, 1) == -1)
		exit(1);
  
	/* Should we close stderr and reopen /dev/null? */
  
	maxopen = sysconf(_SC_OPEN_MAX);
	for (i=3; i < maxopen; i++)
		close(i);
  
#ifdef TIOCNOTTY
	/* Detach from the controlling tty if we have one. Otherwise,
	   SSH might do something stupid like trying to use it instead
	   of running $SSH_ASKPASS. Doh. */
	fd = open("/dev/tty", O_RDONLY);
	if (fd != -1) {
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
#endif /* TIOCNOTTY */

	execl("/bin/sh", "/bin/sh", "-c", command, NULL);
  
	/* Eep. Shouldn't reach this */
	exit(1);
}

static int subcommand_connect(const char *command,
			      const char *servername, uint16_t port)
{
	/* SEB unsupported on Windows */
	int sockfds[2];
	pid_t childpid;
  
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfds))
		return -1;
  
	childpid = fork();
	if (!childpid) {
		do_exec_command(sockfds[1], command, servername, port);
	}
	else if (childpid == -1) {
		close(sockfds[0]);
		close(sockfds[1]);
		return -1;
	}
  
	close(sockfds[1]);
  
	/* Reap child, leaving grandchild process to run */
	waitpid(childpid, NULL, 0);
  
	return sockfds[0];
}

int socket_connect_cmd(mailimap * imap, const char * command,
		       const char * server, int port)
{
	int fd;
	mailstream * s;
	int r;
	
	fd = subcommand_connect(command, server, port);
	if (fd < 0)
		return MAILIMAP_ERROR_STREAM;
	
	s = mailstream_socket_open(fd);
	if (s == NULL) {
		close(fd);
		return MAILIMAP_ERROR_STREAM;
	}
	
	r = mailimap_connect(imap, s);
	if (r != MAILIMAP_NO_ERROR) {
		mailstream_close(s);
		return r;
	}
	
	return MAILIMAP_NO_ERROR;
}

/* connect cmd */

struct connect_cmd_param {
	mailimap * imap;
	const char * command;
	const char * server;
	int port;
};

struct connect_cmd_result {
	int error;
};

static void connect_cmd_run(struct etpan_thread_op * op)
{
	int r;
	struct connect_cmd_param * param;
	struct connect_cmd_result * result;
	
	param = op->param;
	result = op->result;
	
	r = socket_connect_cmd(param->imap, param->command,
			       param->server, param->port);
	
	result->error = r;
}


int imap_threaded_connect_cmd(Folder * folder, const char * command,
			      const char * server, int port)
{
	struct connect_cmd_param param;
	struct connect_cmd_result result;
	chashdatum key;
	chashdatum value;
	mailimap * imap;
	
	imap = mailimap_new(0, NULL);
	
	key.data = &folder;
	key.len = sizeof(folder);
	value.data = imap;
	value.len = 0;
	chash_set(session_hash, &key, &value, NULL);
	
	param.imap = imap;
	param.command = command;
	param.server = server;
	param.port = port;
	
	threaded_run(folder, &param, &result, connect_cmd_run);
	
	debug_print("connect_cmd ok %i\n", result.error);
	
	return result.error;
}
#else

void imap_main_init(void)
{
}
void imap_main_done(void)
{
}
void imap_main_set_timeout(int sec)
{
}

#endif
