/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2002 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 */

#ifndef MAILMBOX_H

#define MAILMBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailmbox_types.h"

int
claws_mailmbox_append_message_list(struct claws_mailmbox_folder * folder,
			     carray * append_tab);

int
claws_mailmbox_append_message(struct claws_mailmbox_folder * folder,
			const char * data, size_t len);

int claws_mailmbox_fetch_msg(struct claws_mailmbox_folder * folder,
		       uint32_t num, const char ** result,
		       size_t * result_len);

int claws_mailmbox_fetch_msg_headers(struct claws_mailmbox_folder * folder,
			       uint32_t num, const char ** result,
			       size_t * result_len);

void claws_mailmbox_fetch_result_free(char * msg);

int claws_mailmbox_copy_msg_list(struct claws_mailmbox_folder * dest_folder,
			   struct claws_mailmbox_folder * src_folder,
			   carray * tab);

int claws_mailmbox_copy_msg(struct claws_mailmbox_folder * dest_folder,
		      struct claws_mailmbox_folder * src_folder,
		      uint32_t uid);

int claws_mailmbox_expunge(struct claws_mailmbox_folder * folder);

int claws_mailmbox_delete_msg(struct claws_mailmbox_folder * folder, uint32_t uid);

int claws_mailmbox_init(const char * filename,
		  int force_readonly,
		  int force_no_uid,
		  uint32_t default_written_uid,
		  struct claws_mailmbox_folder ** result_folder);

void claws_mailmbox_done(struct claws_mailmbox_folder * folder);

/* low-level access primitives */

int claws_mailmbox_write_lock(struct claws_mailmbox_folder * folder);

int claws_mailmbox_write_unlock(struct claws_mailmbox_folder * folder);

int claws_mailmbox_read_lock(struct claws_mailmbox_folder * folder);

int claws_mailmbox_read_unlock(struct claws_mailmbox_folder * folder);


/* memory map */

int claws_mailmbox_map(struct claws_mailmbox_folder * folder);

void claws_mailmbox_unmap(struct claws_mailmbox_folder * folder);

void claws_mailmbox_sync(struct claws_mailmbox_folder * folder);


/* open & close file */

int claws_mailmbox_open(struct claws_mailmbox_folder * folder);

void claws_mailmbox_close(struct claws_mailmbox_folder * folder);


/* validate cache */

int claws_mailmbox_validate_write_lock(struct claws_mailmbox_folder * folder);

int claws_mailmbox_validate_read_lock(struct claws_mailmbox_folder * folder);


/* fetch message */

int claws_mailmbox_fetch_msg_no_lock(struct claws_mailmbox_folder * folder,
			       uint32_t num, const char ** result,
			       size_t * result_len);

int claws_mailmbox_fetch_msg_headers_no_lock(struct claws_mailmbox_folder * folder,
				       uint32_t num, const char ** result,
				       size_t * result_len);

/* append message */

int
claws_mailmbox_append_message_list_no_lock(struct claws_mailmbox_folder * folder,
				     carray * append_tab);

int claws_mailmbox_expunge_no_lock(struct claws_mailmbox_folder * folder);

#ifdef __cplusplus
}
#endif

#endif
