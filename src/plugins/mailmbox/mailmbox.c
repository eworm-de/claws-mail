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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "mailmbox.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "mmapstring.h"
#include "mailmbox_parse.h"
#include "maillock.h"
#include "file-utils.h"
#include "utils.h"

/*
  http://www.qmail.org/qmail-manual-html/man5/mbox.html
  RFC 2076
*/

/* mbox is a file with a corresponding filename */

#define UID_HEADER "X-LibEtPan-UID:"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

int claws_mailmbox_write_lock(struct claws_mailmbox_folder * folder)
{
  int r;

  if (folder->mb_read_only)
    return MAILMBOX_ERROR_READONLY;

  r = maillock_write_lock(folder->mb_filename, folder->mb_fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int claws_mailmbox_write_unlock(struct claws_mailmbox_folder * folder)
{
  int r;

  r = maillock_write_unlock(folder->mb_filename, folder->mb_fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int claws_mailmbox_read_lock(struct claws_mailmbox_folder * folder)
{
  int r;

  r = maillock_read_lock(folder->mb_filename, folder->mb_fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int claws_mailmbox_read_unlock(struct claws_mailmbox_folder * folder)
{
  int r;

  r = maillock_read_unlock(folder->mb_filename, folder->mb_fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}


/*
  map the file into memory.
  the file must be locked.
*/

int claws_mailmbox_map(struct claws_mailmbox_folder * folder)
{
  char * str;
  GStatBuf buf;
  int res;
  int r;

  r = g_stat(folder->mb_filename, &buf);
  if (r < 0) {
    debug_print("stat failed %d\n", r);
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

  if (buf.st_size == 0) {
    folder->mb_mapping = NULL;
    folder->mb_mapping_size = 0;
    return MAILMBOX_NO_ERROR;
  }
  if (folder->mb_read_only)
    str = (char *) mmap(NULL, buf.st_size, PROT_READ,
			MAP_PRIVATE, folder->mb_fd, 0);
  else
    str = (char *) mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, folder->mb_fd, 0);
  if (str == MAP_FAILED) {
    perror("mmap");
    debug_print("map of %lld bytes failed\n", (long long)buf.st_size);
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }
  
  folder->mb_mapping = str;
  folder->mb_mapping_size = buf.st_size;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

/*
  unmap the file
*/

void claws_mailmbox_unmap(struct claws_mailmbox_folder * folder)
{
  munmap(folder->mb_mapping, folder->mb_mapping_size);
  folder->mb_mapping = NULL;
  folder->mb_mapping_size = 0;
}

void claws_mailmbox_sync(struct claws_mailmbox_folder * folder)
{
  msync(folder->mb_mapping, folder->mb_mapping_size, MS_SYNC);
}

void claws_mailmbox_timestamp(struct claws_mailmbox_folder * folder)
{
  int r;
  GStatBuf buf;

  r = g_stat(folder->mb_filename, &buf);
  if (r < 0)
    folder->mb_mtime = (time_t) -1;
  else
    folder->mb_mtime = buf.st_mtime;
}

/*
  open the file
*/

int claws_mailmbox_open(struct claws_mailmbox_folder * folder)
{
  int fd = -1;
  int read_only;

  if (!folder->mb_read_only) {
    read_only = FALSE;
    fd = open(folder->mb_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  }

  if (folder->mb_read_only || (fd < 0)) {
    read_only = TRUE;
    fd = open(folder->mb_filename, O_RDONLY);
    if (fd < 0)
      return MAILMBOX_ERROR_FILE_NOT_FOUND;
  }

  folder->mb_fd = fd;
  folder->mb_read_only = read_only;

  return MAILMBOX_NO_ERROR;
}

/*
  close the file
*/

void claws_mailmbox_close(struct claws_mailmbox_folder * folder)
{
  close(folder->mb_fd);
  folder->mb_fd = -1;
}


static int claws_mailmbox_validate_lock(struct claws_mailmbox_folder * folder,
    int (* custom_lock)(struct claws_mailmbox_folder *),
    int (* custom_unlock)(struct claws_mailmbox_folder *))
{
  GStatBuf buf;
  int res;
  int r;

  r = g_stat(folder->mb_filename, &buf);
  if (r < 0) {
    buf.st_mtime = (time_t) -1;
  }

  if ((buf.st_mtime != folder->mb_mtime) ||
      ((size_t) buf.st_size != folder->mb_mapping_size)) {
    int r;

    claws_mailmbox_unmap(folder);
    claws_mailmbox_close(folder);

    r = claws_mailmbox_open(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }

    r = custom_lock(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }

    r = claws_mailmbox_map(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err_unlock;
    }

    r = claws_mailmbox_parse(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err_unlock;
    }

    folder->mb_mtime = buf.st_mtime;

    return MAILMBOX_NO_ERROR;
  }
  else {
    r = custom_lock(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  return MAILMBOX_NO_ERROR;

 err_unlock:
  custom_unlock(folder);
 err:
  return res;
}


int claws_mailmbox_validate_write_lock(struct claws_mailmbox_folder * folder)
{
  return claws_mailmbox_validate_lock(folder,
				claws_mailmbox_write_lock,
				claws_mailmbox_write_unlock);
}


int claws_mailmbox_validate_read_lock(struct claws_mailmbox_folder * folder)
{
  return claws_mailmbox_validate_lock(folder,
				claws_mailmbox_read_lock,
				claws_mailmbox_read_unlock);
}


/* ********************************************************************** */
/* append messages */

#define MAX_FROM_LINE_SIZE 256

static inline size_t get_line(const char * line, size_t length,
			      const char ** pnext_line, size_t * pcount)
{
  size_t count;

  count = 0;

  while (1) {
    if (length == 0)
      break;

    if (* line == '\r') {
      line ++;

      count ++;
      length --;

      if (length > 0) {
        if (* line == '\n') {
          line ++;
          
          count ++;
          length --;
          
          break;
        }
      }
    }
    else if (* line == '\n') {
      line ++;

      count ++;
      length --;

      break;
    }
    else {
      line ++;
      length --;
      count ++;
    }
  }

  * pnext_line = line;
  * pcount = count;

  return count;
}

/*
  TODO : should strip \r\n if any
  see also in write_fixed_line
*/

static inline size_t get_fixed_line_size(const char * line, size_t length,
    const char ** pnext_line, size_t * pcount,
    size_t * pfixed_count)
{
  size_t count;
  const char * next_line;
  size_t fixed_count;
  
  if (!get_line(line, length, &next_line, &count))
    return 0;

  fixed_count = count;
  if (count >= 5) {
    if (line[0] == 'F') {
      if (strncmp(line, "From ", 5) == 0)
	fixed_count ++;
    }
  }

  * pnext_line = next_line;
  * pcount = count;
  * pfixed_count = fixed_count;

  return count;
}

static size_t get_fixed_message_size(const char * message, size_t size,
				     uint32_t uid, int force_no_uid)
{
  size_t fixed_size;
  size_t cur_token;
  size_t left;
  const char * next;
  const char * cur;
  int end;
  int r;
  uint32_t tmp_uid;

  cur_token = 0;

  fixed_size = 0;

  /* headers */

  end = FALSE;
  while (!end) {
    size_t begin;
    int ignore;

    ignore = FALSE;
    begin = cur_token;
    if (cur_token + strlen(UID_HEADER) <= size) {
      if (message[cur_token] == 'X') {
	if (strncasecmp(message + cur_token, UID_HEADER,
			strlen(UID_HEADER)) == 0) {
	  ignore = TRUE;
	}
      }
    }

    r = mailimf_ignore_field_parse(message, size, &cur_token);
    switch (r) {
    case MAILIMF_NO_ERROR:
      if (!ignore)
	fixed_size += cur_token - begin;
      break;
    case MAILIMF_ERROR_PARSE:
    default:
      end = TRUE;
      break;
    }
  }

  if (!force_no_uid) {
    /* UID header */

#if CRLF_BADNESS    
    fixed_size += strlen(UID_HEADER " \r\n");
#else
    fixed_size += strlen(UID_HEADER " \n");
#endif
    
    tmp_uid = uid;
    while (tmp_uid >= 10) {
      tmp_uid /= 10;
      fixed_size ++;
    }
    fixed_size ++;
  }

  /* body */

  left = size - cur_token;
  next = message + cur_token;
  while (left > 0) {
    size_t count;
    size_t fixed_count;

    cur = next;

    if (!get_fixed_line_size(cur, left, &next, &count, &fixed_count))
      break;

    fixed_size += fixed_count;
    left -= count;
  }

  return fixed_size;
}

static inline char * write_fixed_line(char * str,
    const char * line, size_t length,
    const char ** pnext_line, size_t * pcount)
{
  size_t count;
  const char * next_line;
  
  if (!get_line(line, length, &next_line, &count))
    return str;

  if (count >= 5) {
    if (line[0] == 'F') {
      if (strncmp(line, "From ", 5) == 0) {
	* str = '>';
	str ++;
      }
    }
  }

  memcpy(str, line, count);
  
  * pnext_line = next_line;
  * pcount = count;
  str += count;

  return str;
}

static char * write_fixed_message(char * str,
				  const char * message, size_t size,
				  uint32_t uid, int force_no_uid)
{
  size_t cur_token;
  size_t left;
  int end;
  int r;
  const char * cur_src;
  size_t numlen;

  cur_token = 0;

  /* headers */

  end = FALSE;
  while (!end) {
    size_t begin;
    int ignore;

    ignore = FALSE;
    begin = cur_token;
    if (cur_token + strlen(UID_HEADER) <= size) {
      if (message[cur_token] == 'X') {
	if (strncasecmp(message + cur_token, UID_HEADER,
			strlen(UID_HEADER)) == 0) {
	  ignore = TRUE;
	}
      }
    }

    r = mailimf_ignore_field_parse(message, size, &cur_token);
    switch (r) {
    case MAILIMF_NO_ERROR:
      if (!ignore) {
	memcpy(str, message + begin, cur_token - begin);
	str += cur_token - begin;
      }
      break;
    case MAILIMF_ERROR_PARSE:
    default:
      end = TRUE;
      break;
    }
  }

  if (!force_no_uid) {
    /* UID header */
    
    memcpy(str, UID_HEADER " ", strlen(UID_HEADER " "));
    str += strlen(UID_HEADER " ");
#if CRLF_BADNESS    
    numlen = snprintf(str, 20, "%i\r\n", uid);
#else
    numlen = snprintf(str, 20, "%i\n", uid);
#endif
    str += numlen;
  }

  /* body */

  cur_src = message + cur_token;
  left = size - cur_token;
  while (left > 0) {
    size_t count = 0;
    const char * next = NULL;

    str = write_fixed_line(str, cur_src, left, &next, &count);
    
    cur_src = next;
    left -= count;
  }

  return str;
}

#define DEFAULT_FROM_LINE "From - Wed Jun 30 21:49:08 1993\n"

int
claws_mailmbox_append_message_list_no_lock(struct claws_mailmbox_folder * folder,
				     carray * append_tab)
{
  size_t extra_size;
  int r;
  char from_line[MAX_FROM_LINE_SIZE] = DEFAULT_FROM_LINE;
  struct tm time_info;
  time_t date;
  int res;
  size_t old_size;
  char * str;
  unsigned int i;
  size_t from_size;
  size_t left;
  size_t crlf_count;

  if (folder->mb_read_only) {
    res = MAILMBOX_ERROR_READONLY;
    goto err;
  }

  date = time(NULL);
  from_size = strlen(DEFAULT_FROM_LINE);
  if (localtime_r(&date, &time_info) != NULL)
    from_size = strftime(from_line, MAX_FROM_LINE_SIZE, "From - %a %b %_2d %T %Y\n", &time_info);

  extra_size = 0;
  for(i = 0 ; i < carray_count(append_tab) ; i ++) {
    struct claws_mailmbox_append_info * info;

    info = carray_get(append_tab, i);
    extra_size += from_size;
    extra_size += get_fixed_message_size(info->ai_message, info->ai_size,
        folder->mb_max_uid + i + 1,
        folder->mb_no_uid);
#if CRLF_BADNESS
    extra_size += 2; /* CR LF */
#else
    extra_size += 1; /* CR LF */
#endif    
  }

  left = folder->mb_mapping_size;
  crlf_count = 0;
  while (left >= 1) {
    if (folder->mb_mapping[left - 1] == '\n') {
      crlf_count ++;
      left --;
    }
#if CRLF_BADNESS    
    else if (folder->mb_mapping[left - 1] == '\r') {
      left --;
    }
#endif
    else
      break;

    if (crlf_count == 2)
      break;
  }

  old_size = folder->mb_mapping_size;
  claws_mailmbox_unmap(folder);

  if (old_size != 0) {
    if (crlf_count != 2)
#if CRLF_BADNESS
      extra_size += (2 - crlf_count) * 2;
#else
      /* Need the number of LFs, not CRLFs */
      extra_size += (2 - crlf_count) * 1; /* 2 */
#endif      
  }

  r = ftruncate(folder->mb_fd, extra_size + old_size);
  if (r < 0) {
    debug_print("ftruncate failed with %d\n", r);
    claws_mailmbox_map(folder);
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

  r = claws_mailmbox_map(folder);
  if (r < 0) {
    debug_print("claws_mailmbox_map failed with %d\n", r);
    r = ftruncate(folder->mb_fd, old_size);
    if (r < 0)
      debug_print("ftruncate failed with %d\n", r);
    return MAILMBOX_ERROR_FILE;
  }

  str = folder->mb_mapping + old_size;

  if (old_size != 0) {
    for(i = 0 ; i < 2 - crlf_count ; i ++) {
#if CRLF_BADNESS    
      * str = '\r';
      str ++;
#endif      
      * str = '\n';
      str ++;
    }
  }

  for(i = 0 ; i < carray_count(append_tab) ; i ++) {
    struct claws_mailmbox_append_info * info;

    info = carray_get(append_tab, i);

    memcpy(str, from_line, from_size);

    str += strlen(from_line);

    str = write_fixed_message(str, info->ai_message, info->ai_size,
        folder->mb_max_uid + i + 1,
        folder->mb_no_uid);

#if CRLF_BADNESS
    * str = '\r';
    str ++;
#endif    
    * str = '\n';
    str ++;
  }

  folder->mb_max_uid += carray_count(append_tab);

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int
claws_mailmbox_append_message_list(struct claws_mailmbox_folder * folder,
			     carray * append_tab)
{
  int r;
  int res;
  size_t cur_token;

  r = claws_mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = claws_mailmbox_expunge_no_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  cur_token = folder->mb_mapping_size;

  r = claws_mailmbox_append_message_list_no_lock(folder, append_tab);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  claws_mailmbox_sync(folder);

  r = claws_mailmbox_parse_additionnal(folder, &cur_token);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  claws_mailmbox_timestamp(folder);

  claws_mailmbox_write_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  claws_mailmbox_write_unlock(folder);
 err:
  return res;
}

int
claws_mailmbox_append_message(struct claws_mailmbox_folder * folder,
			const char * data, size_t len)
{
  carray * tab;
  struct claws_mailmbox_append_info * append_info;
  int res;
  int r;

  tab = carray_new(1);
  if (tab == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto err;
  }

  append_info = claws_mailmbox_append_info_new(data, len);
  if (append_info == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto free_list;
  }

  r = carray_add(tab, append_info, NULL);
  if (r < 0) {
    res = MAILMBOX_ERROR_MEMORY;
    goto free_append_info;
  }

  r = claws_mailmbox_append_message_list(folder, tab);

  claws_mailmbox_append_info_free(append_info);
  carray_free(tab);

  return r;

 free_append_info:
  claws_mailmbox_append_info_free(append_info);
 free_list:
  carray_free(tab);
 err:
  return res;
}

/* ********************************************************************** */

int claws_mailmbox_fetch_msg_no_lock(struct claws_mailmbox_folder * folder,
			       uint32_t num, const char ** result,
			       size_t * result_len)
{
  struct claws_mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }
  
  info = data.data;
  
  if (info->msg_deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  * result = folder->mb_mapping + info->msg_headers;
  * result_len = info->msg_size - info->msg_start_len;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int claws_mailmbox_fetch_msg_headers_no_lock(struct claws_mailmbox_folder * folder,
				       uint32_t num, const char ** result,
				       size_t * result_len)
{
  struct claws_mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info = data.data;

  if (info->msg_deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  * result = folder->mb_mapping + info->msg_headers;
  * result_len = info->msg_headers_len;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int claws_mailmbox_fetch_msg(struct claws_mailmbox_folder * folder,
		       uint32_t num, const char ** result,
		       size_t * result_len)
{
  MMAPString * mmapstr;
  int res;
  const char * data;
  size_t len;
  int r;
  size_t fixed_size;
  char * end;
  
  r = claws_mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = claws_mailmbox_fetch_msg_no_lock(folder, num, &data, &len);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }
  
  /* size with no uid */
  fixed_size = get_fixed_message_size(data, len, 0, 1 /* force no uid */);
  
#if 0
  mmapstr = mmap_string_new_len(data, fixed_size);
  if (mmapstr == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
#endif
  mmapstr = mmap_string_sized_new(fixed_size);
  if (mmapstr == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
  
  end = write_fixed_message(mmapstr->str, data, len, 0, 1 /* force no uid */);
  * end = '\0';
  mmapstr->len = fixed_size;
  
  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    mmap_string_free(mmapstr);
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  claws_mailmbox_read_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  claws_mailmbox_read_unlock(folder);
 err:
  return res;
}

int claws_mailmbox_fetch_msg_headers(struct claws_mailmbox_folder * folder,
			       uint32_t num, const char ** result,
			       size_t * result_len)
{
  MMAPString * mmapstr;
  int res;
  const char * data;
  size_t len;
  int r;
  size_t fixed_size;
  char * end;

  r = claws_mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = claws_mailmbox_fetch_msg_headers_no_lock(folder, num, &data, &len);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

#if 0
  mmapstr = mmap_string_new_len(data, len);
  if (mmapstr == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
#endif
  /* size with no uid */
  fixed_size = get_fixed_message_size(data, len, 0, 1 /* force no uid */);
  
  mmapstr = mmap_string_sized_new(fixed_size);
  if (mmapstr == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
  
  end = write_fixed_message(mmapstr->str, data, len, 0, 1 /* force no uid */);
  * end = '\0';
  mmapstr->len = fixed_size;

  r = mmap_string_ref(mmapstr);
  if (r < 0) {
    mmap_string_free(mmapstr);
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }

  * result = mmapstr->str;
  * result_len = mmapstr->len;

  claws_mailmbox_read_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  claws_mailmbox_read_unlock(folder);
 err:
  return res;
}

void claws_mailmbox_fetch_result_free(char * msg)
{
  mmap_string_unref(msg);
}


int claws_mailmbox_copy_msg_list(struct claws_mailmbox_folder * dest_folder,
			   struct claws_mailmbox_folder * src_folder,
			   carray * tab)
{
  int r;
  int res;
  carray * append_tab;
  unsigned int i;

  r = claws_mailmbox_validate_read_lock(src_folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  append_tab = carray_new(carray_count(tab));
  if (append_tab == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto src_unlock;
  }

  for(i = 0 ; i < carray_count(tab) ; i ++) {
    struct claws_mailmbox_append_info * append_info;
    const char * data;
    size_t len;
    uint32_t uid;

    uid = * ((uint32_t *) carray_get(tab, i));

    r = claws_mailmbox_fetch_msg_no_lock(src_folder, uid, &data, &len);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto free_list;
    }
    
    append_info = claws_mailmbox_append_info_new(data, len);
    if (append_info == NULL) {
      res = MAILMBOX_ERROR_MEMORY;
      goto free_list;
    }
    
    r = carray_add(append_tab, append_info, NULL);
    if (r < 0) {
      claws_mailmbox_append_info_free(append_info);
      res = MAILMBOX_ERROR_MEMORY;
      goto free_list;
    }
  }    

  r = claws_mailmbox_append_message_list(dest_folder, append_tab);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto free_list;
  }

  for(i = 0 ; i < carray_count(append_tab) ; i ++) {
    struct claws_mailmbox_append_info * append_info;

    append_info = carray_get(append_tab, i);
    claws_mailmbox_append_info_free(append_info);
  }
  carray_free(append_tab);

  claws_mailmbox_read_unlock(src_folder);

  return MAILMBOX_NO_ERROR;

 free_list:
  for(i = 0 ; i < carray_count(append_tab) ; i ++) {
    struct claws_mailmbox_append_info * append_info;

    append_info = carray_get(append_tab, i);
    claws_mailmbox_append_info_free(append_info);
  }
  carray_free(append_tab);
 src_unlock:
  claws_mailmbox_read_unlock(src_folder);
 err:
  return res;
}

int claws_mailmbox_copy_msg(struct claws_mailmbox_folder * dest_folder,
		      struct claws_mailmbox_folder * src_folder,
		      uint32_t uid)
{
  carray * tab;
  int res;
  uint32_t * puid;
  int r;

  tab = carray_new(1);
  if (tab == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto err;
  }

  puid = malloc(sizeof(* puid));
  if (puid == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto free_array;
  }
  * puid = uid;
  
  r = claws_mailmbox_copy_msg_list(dest_folder, src_folder, tab);
  res = r;

  free(puid);
 free_array:
  carray_free(tab);
 err:
  return res;
}

static int claws_mailmbox_expunge_to_file_no_lock(char * dest_filename, int dest_fd,
    struct claws_mailmbox_folder * folder,
    size_t * result_size)
{
  int r;
  int res;
  unsigned long i;
  size_t cur_offset;
  char * dest = NULL;
  size_t size;

  size = 0;
  for(i = 0 ; i < carray_count(folder->mb_tab) ; i ++) {
    struct claws_mailmbox_msg_info * info;

    info = carray_get(folder->mb_tab, i);

    if (!info->msg_deleted) {
      size += info->msg_size + info->msg_padding;

      if (!folder->mb_no_uid) {
	if (!info->msg_written_uid) {
	  uint32_t uid;
	 
#if CRLF_BADNESS
	  size += strlen(UID_HEADER " \r\n");
#else
	  size += strlen(UID_HEADER " \n");
#endif	  
	  uid = info->msg_uid;
	  while (uid >= 10) {
	    uid /= 10;
	    size ++;
	  }
	  size ++;
	}
      }
    }
  }

  r = ftruncate(dest_fd, size);
  if (r < 0) {
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

  if (size) {
    dest = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, dest_fd, 0);
    if (dest == MAP_FAILED) {
      res = MAILMBOX_ERROR_FILE;
      goto err;
    }
  }

  cur_offset = 0;
  for(i = 0 ; i < carray_count(folder->mb_tab) ; i ++) {
    struct claws_mailmbox_msg_info * info;
    
    info = carray_get(folder->mb_tab, i);

    if (!info->msg_deleted) {
      memcpy(dest + cur_offset, folder->mb_mapping + info->msg_start,
	     info->msg_headers_len + info->msg_start_len);
      cur_offset += info->msg_headers_len + info->msg_start_len;
      
      if (!folder->mb_no_uid) {
	if (!info->msg_written_uid) {
	  size_t numlen;
	  
	  memcpy(dest + cur_offset, UID_HEADER " ", strlen(UID_HEADER " "));
	  cur_offset += strlen(UID_HEADER " ");
#if CRLF_BADNESS
 	  numlen = snprintf(dest + cur_offset, size - cur_offset,
			    "%i\r\n", info->msg_uid);
#else
	  numlen = snprintf(dest + cur_offset, size - cur_offset,
			    "%i\n", info->msg_uid);
#endif			    
	  cur_offset += numlen;
	}
      }
      
      memcpy(dest + cur_offset,
	     folder->mb_mapping + info->msg_headers + info->msg_headers_len,
	     info->msg_size - (info->msg_start_len + info->msg_headers_len)
	     + info->msg_padding);
      
      cur_offset += info->msg_size -
        (info->msg_start_len + info->msg_headers_len)
	+ info->msg_padding;
    }
  }
  fflush(stdout);

  if (size) {
    msync(dest, size, MS_SYNC);
    munmap(dest, size);
  }

  * result_size = size;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int claws_mailmbox_expunge_no_lock(struct claws_mailmbox_folder * folder)
{
  char tmpfile[PATH_MAX + 8]; /* for the extra Xs */
  int r;
  int res;
  int dest_fd;
  size_t size;

  if (folder->mb_read_only)
    return MAILMBOX_ERROR_READONLY;

  if (((folder->mb_written_uid >= folder->mb_max_uid) || folder->mb_no_uid) &&
      (!folder->mb_changed)) {
    /* no need to expunge */
    return MAILMBOX_NO_ERROR;
  }

  snprintf(tmpfile, sizeof(tmpfile), "%sXXXXXX", folder->mb_filename);
  dest_fd = g_mkstemp(tmpfile);

  if (dest_fd < 0) {
    res = MAILMBOX_ERROR_FILE;
    goto unlink;
  }

  r = claws_mailmbox_expunge_to_file_no_lock(tmpfile, dest_fd,
				       folder, &size);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlink;
  }

  close(dest_fd);

  r = rename(tmpfile, folder->mb_filename);
  if (r < 0) {
    res = r;
    goto err;
  }

  claws_mailmbox_unmap(folder);
  claws_mailmbox_close(folder);

  r = claws_mailmbox_open(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = claws_mailmbox_map(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }
      
  r = claws_mailmbox_parse(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }
  
  claws_mailmbox_timestamp(folder);

  folder->mb_changed = FALSE;
  folder->mb_deleted_count = 0;
  
  return MAILMBOX_NO_ERROR;

 unlink:
  close(dest_fd);
  unlink(tmpfile);
 err:
  return res;
}

int claws_mailmbox_expunge(struct claws_mailmbox_folder * folder)
{
  int r;
  int res;

  r = claws_mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = claws_mailmbox_expunge_no_lock(folder);
  res = r;

  claws_mailmbox_write_unlock(folder);
 err:
  return res;
}

int claws_mailmbox_delete_msg(struct claws_mailmbox_folder * folder, uint32_t uid)
{
  struct claws_mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;

  if (folder->mb_read_only) {
    res = MAILMBOX_ERROR_READONLY;
    goto err;
  }
  
  key.data = &uid;
  key.len = sizeof(uid);
  
  r = chash_get(folder->mb_hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info = data.data;

  if (info->msg_deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info->msg_deleted = TRUE;
  folder->mb_changed = TRUE;
  folder->mb_deleted_count ++;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}


/*
  INIT of MBOX

  - open file
  - map the file

  - lock the file

  - parse memory

  - unlock the file
*/

int claws_mailmbox_init(const char * filename,
		  int force_readonly,
		  int force_no_uid,
		  uint32_t default_written_uid,
		  struct claws_mailmbox_folder ** result_folder)
{
  struct claws_mailmbox_folder * folder;
  int r;
  int res;
  
  folder = claws_mailmbox_folder_new(filename);
  if (folder == NULL) {
    debug_print("folder is null for %s\n", filename);
    res = MAILMBOX_ERROR_MEMORY;
    goto err;
  }
  folder->mb_no_uid = force_no_uid;
  folder->mb_read_only = force_readonly;
  folder->mb_written_uid = default_written_uid;
  
  folder->mb_changed = FALSE;
  folder->mb_deleted_count = 0;
  
  r = claws_mailmbox_open(folder);
  if (r != MAILMBOX_NO_ERROR) {
    debug_print("folder can't be opened %d\n", r);
    res = r;
    goto free;
  }

  r = claws_mailmbox_map(folder);
  if (r != MAILMBOX_NO_ERROR) {
    debug_print("folder can't be mapped %d\n", r);
    res = r;
    goto close;
  }

  r = claws_mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    debug_print("folder can't be locked %d\n", r);
    res = r;
    goto unmap;
  }

  claws_mailmbox_read_unlock(folder);

  * result_folder = folder;

  return MAILMBOX_NO_ERROR;

 unmap:
  claws_mailmbox_unmap(folder);
 close:
  claws_mailmbox_close(folder);
 free:
  claws_mailmbox_folder_free(folder);
 err:
  return res;
}


/*
  when MBOX is DONE

  - check for changes

  - unmap the file
  - close file
*/

void claws_mailmbox_done(struct claws_mailmbox_folder * folder)
{
  if (!folder->mb_read_only)
    claws_mailmbox_expunge(folder);
  
  claws_mailmbox_unmap(folder);
  claws_mailmbox_close(folder);

  claws_mailmbox_folder_free(folder);
}
