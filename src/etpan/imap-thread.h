#ifndef IMAP_THREAD_H

#define IMAP_THREAD_H

#include <libetpan/libetpan.h>
#include "folder.h"

#define IMAP_SET_MAX_COUNT 100

void imap_main_set_timeout(int sec);
void imap_main_init(void);
void imap_main_done(void);

void imap_init(Folder * folder);
void imap_done(Folder * folder);

int imap_threaded_connect(Folder * folder, const char * server, int port);
int imap_threaded_connect_ssl(Folder * folder, const char * server, int port);
struct mailimap_capability_data * imap_threaded_capability(Folder *folder);
int imap_threaded_connect_cmd(Folder * folder, const char * command,
			      const char * server, int port);

void imap_threaded_disconnect(Folder * folder);

int imap_threaded_list(Folder * folder, const char * base,
		       const char * wildcard,
		       clist ** p_result);
int imap_threaded_login(Folder * folder,
			const char * login, const char * password,
			const char * type);
int imap_threaded_status(Folder * folder, const char * mb,
		struct mailimap_mailbox_data_status ** data_status,
		guint mask);

int imap_threaded_noop(Folder * folder, unsigned int * p_exists);
int imap_threaded_starttls(Folder * folder, const gchar *host, int port);
int imap_threaded_create(Folder * folder, const char * mb);
int imap_threaded_rename(Folder * folder,
			 const char * mb, const char * new_name);
int imap_threaded_delete(Folder * folder, const char * mb);
int imap_threaded_select(Folder * folder, const char * mb,
			 gint * exists, gint * recent, gint * unseen,
			 guint32 * uid_validity);
int imap_threaded_examine(Folder * folder, const char * mb,
			  gint * exists, gint * recent, gint * unseen,
			  guint32 * uid_validity);

enum {
	IMAP_SEARCH_TYPE_SIMPLE,
	IMAP_SEARCH_TYPE_SEEN,
	IMAP_SEARCH_TYPE_UNSEEN,
	IMAP_SEARCH_TYPE_ANSWERED,
	IMAP_SEARCH_TYPE_FLAGGED,
	IMAP_SEARCH_TYPE_DELETED,
};

int imap_threaded_search(Folder * folder, int search_type,
			 struct mailimap_set * set, clist ** result);

int imap_threaded_fetch_uid(Folder * folder, uint32_t first_index,
			    carray ** result);

void imap_fetch_uid_list_free(carray * uid_list);

int imap_threaded_fetch_content(Folder * folder, uint32_t msg_index,
				int with_body,
				const char * filename);

struct imap_fetch_env_info {
	uint32_t uid;
	char * headers;
	uint32_t size;
	int flags;
};

int imap_threaded_fetch_env(Folder * folder, struct mailimap_set * set,
			    carray ** p_env_list);

void imap_fetch_env_free(carray * env_list);

int imap_threaded_append(Folder * folder, const char * mailbox,
			 const char * filename,
			 struct mailimap_flag_list * flag_list,
			 int *uid);

int imap_threaded_expunge(Folder * folder);

int imap_threaded_copy(Folder * folder, struct mailimap_set * set,
		       const char * mb);

int imap_threaded_store(Folder * folder, struct mailimap_set * set,
			struct mailimap_store_att_flags * store_att_flags);

#endif
