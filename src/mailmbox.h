#ifndef MAILMBOX_H

#define MAILMBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailmbox_types.h"

int
mailmbox_append_message_list(struct mailmbox_folder * folder,
			     carray * append_tab);

int
mailmbox_append_message(struct mailmbox_folder * folder,
			char * data, size_t len);

int mailmbox_fetch_msg(struct mailmbox_folder * folder,
		       uint32_t num, char ** result,
		       size_t * result_len);

int mailmbox_fetch_msg_headers(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len);

void mailmbox_fetch_result_free(char * msg);

int mailmbox_copy_msg_list(struct mailmbox_folder * dest_folder,
			   struct mailmbox_folder * src_folder,
			   carray * tab);

int mailmbox_copy_msg(struct mailmbox_folder * dest_folder,
		      struct mailmbox_folder * src_folder,
		      uint32_t uid);

int mailmbox_expunge(struct mailmbox_folder * folder);

int mailmbox_delete_msg(struct mailmbox_folder * folder, uint32_t uid);

int mailmbox_init(char * filename,
		  int force_readonly,
		  int force_no_uid,
		  uint32_t default_written_uid,
		  struct mailmbox_folder ** result_folder);

void mailmbox_done(struct mailmbox_folder * folder);

/* low-level access primitives */

int mailmbox_write_lock(struct mailmbox_folder * folder);

int mailmbox_write_unlock(struct mailmbox_folder * folder);

int mailmbox_read_lock(struct mailmbox_folder * folder);

int mailmbox_read_unlock(struct mailmbox_folder * folder);


/* memory map */

int mailmbox_map(struct mailmbox_folder * folder);

void mailmbox_unmap(struct mailmbox_folder * folder);

void mailmbox_sync(struct mailmbox_folder * folder);


/* open & close file */

int mailmbox_open(struct mailmbox_folder * folder);

void mailmbox_close(struct mailmbox_folder * folder);


/* validate cache */

#if 0
int mailmbox_validate_cache(struct mailmbox_folder * folder);
#endif

int mailmbox_validate_write_lock(struct mailmbox_folder * folder);

int mailmbox_validate_read_lock(struct mailmbox_folder * folder);


/* fetch message */

int mailmbox_fetch_msg_no_lock(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len);

int mailmbox_fetch_msg_headers_no_lock(struct mailmbox_folder * folder,
				       uint32_t num, char ** result,
				       size_t * result_len);

/* append message */

int
mailmbox_append_message_list_no_lock(struct mailmbox_folder * folder,
				     carray * append_tab);

int mailmbox_expunge_no_lock(struct mailmbox_folder * folder);

#ifdef __cplusplus
}
#endif

#endif
