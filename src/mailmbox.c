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

#include "mailmbox.h"

#include <sys/file.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "mailmbox_parse.h"
#include "maillock.h"

#ifdef WIN32
#define snprintf g_snprintf
#endif

/*
  http://www.qmail.org/qmail-manual-html/man5/mbox.html
  RFC 2076
*/

#define TMPDIR "/tmp"

/* mbox is a file with a corresponding filename */

#define UID_HEADER "X-LibEtPan-UID:"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

int mailmbox_write_lock(struct mailmbox_folder * folder)
{
  int r;

  if (folder->read_only)
    return MAILMBOX_ERROR_READONLY;

  r = maillock_write_lock(folder->filename, folder->fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int mailmbox_write_unlock(struct mailmbox_folder * folder)
{
  int r;

  r = maillock_write_unlock(folder->filename, folder->fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int mailmbox_read_lock(struct mailmbox_folder * folder)
{
  int r;

  r = maillock_read_lock(folder->filename, folder->fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}

int mailmbox_read_unlock(struct mailmbox_folder * folder)
{
  int r;

  r = maillock_read_unlock(folder->filename, folder->fd);
  if (r == 0)
    return MAILMBOX_NO_ERROR;
  else
    return MAILMBOX_ERROR_FILE;
}


/*
  map the file into memory.
  the file must be locked.
*/

int mailmbox_map(struct mailmbox_folder * folder)
{
  char * str;
  struct stat buf;
  int res;
  int r;

  r = stat(folder->filename, &buf);
  if (r < 0) {
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

#ifndef WIN32
  if (folder->read_only)
    str = (char *) mmap(0, buf.st_size, PROT_READ,
			MAP_PRIVATE, folder->fd, 0);
  else
    str = (char *) mmap(0, buf.st_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, folder->fd, 0);
  if (str == MAP_FAILED) {
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }
#endif
  
  folder->mapping = str;
  folder->mapping_size = buf.st_size;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

/*
  unmap the file
*/

void mailmbox_unmap(struct mailmbox_folder * folder)
{
#ifndef WIN32
  munmap(folder->mapping, folder->mapping_size);
#endif
  folder->mapping = NULL;
  folder->mapping_size = 0;
}

void mailmbox_sync(struct mailmbox_folder * folder)
{
#ifndef WIN32
  msync(folder->mapping, folder->mapping_size, MS_SYNC);
#endif
}

void mailmbox_timestamp(struct mailmbox_folder * folder)
{
  int r;
  struct stat buf;

  r = stat(folder->filename, &buf);
  if (r < 0)
    folder->mtime = (time_t) -1;
  else
    folder->mtime = buf.st_mtime;
}

/*
  open the file
*/

int mailmbox_open(struct mailmbox_folder * folder)
{
  int fd;
  int read_only;

  if (!folder->read_only) {
    read_only = FALSE;
    fd = open(folder->filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  }

  if (folder->read_only || (fd < 0)) {
    read_only = TRUE;
    fd = open(folder->filename, O_RDONLY);
    if (fd < 0)
      return MAILMBOX_ERROR_FILE_NOT_FOUND;
  }

  folder->fd = fd;
  folder->read_only = read_only;

  return MAILMBOX_NO_ERROR;
}

/*
  close the file
*/

void mailmbox_close(struct mailmbox_folder * folder)
{
  close(folder->fd);
  folder->fd = -1;
}


static int mailmbox_validate_lock(struct mailmbox_folder * folder,
				  int (* custom_lock)(struct mailmbox_folder *),
				  int (* custom_unlock)(struct mailmbox_folder *))
{
  struct stat buf;
  int res;
  int r;

  r = stat(folder->filename, &buf);
  if (r < 0) {
    buf.st_mtime = (time_t) -1;
  }

  if ((buf.st_mtime != folder->mtime) ||
      ((size_t) buf.st_size != folder->mapping_size)) {
    int r;

    mailmbox_unmap(folder);
    mailmbox_close(folder);

    r = mailmbox_open(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }

    r = custom_lock(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }

    r = mailmbox_map(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err_unlock;
    }

    r = mailmbox_parse(folder);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err_unlock;
    }

    folder->mtime = buf.st_mtime;

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


int mailmbox_validate_write_lock(struct mailmbox_folder * folder)
{
  return mailmbox_validate_lock(folder,
				mailmbox_write_lock,
				mailmbox_write_unlock);
}


int mailmbox_validate_read_lock(struct mailmbox_folder * folder)
{
  return mailmbox_validate_lock(folder,
				mailmbox_read_lock,
				mailmbox_read_unlock);
}


/* ********************************************************************** */
/* append messages */

#define MAX_FROM_LINE_SIZE 256

static inline size_t get_line(char * line, size_t length,
			      char ** pnext_line, size_t * pcount)
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

      if (* line == '\n') {
	line ++;

	count ++;
	length --;
	
	break;
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

static inline size_t get_fixed_line_size(char * line, size_t length,
					 char ** pnext_line, size_t * pcount,
					 size_t * pfixed_count)
{
  size_t count;
  char * next_line;
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

static size_t get_fixed_message_size(char * message, size_t size,
				     uint32_t uid, int force_no_uid)
{
  size_t fixed_size;
  size_t cur_token;
  size_t left;
  char * next;
  char * cur;
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
    
    fixed_size += strlen(UID_HEADER " \r\n");
    
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
				      char * line, size_t length,
				      char ** pnext_line, size_t * pcount)
{
  size_t count;
  char * next_line;
  
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
				  char * message, size_t size,
				  uint32_t uid, int force_no_uid)
{
  size_t fixed_size;
  size_t cur_token;
  size_t left;
  int end;
  int r;
  char * cur_src;
  size_t numlen;

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
    numlen = snprintf(str, 20, "%i\r\n", uid);
    str += numlen;
  }

  /* body */

  cur_src = message + cur_token;
  left = size - cur_token;
  while (left > 0) {
    size_t count;
    char * next;

    str = write_fixed_line(str, cur_src, left, &next, &count);
    
    cur_src = next;
    left -= count;
  }

  return str;
}

#define DEFAULT_FROM_LINE "From - Wed Jun 30 21:49:08 1993\n"

int
mailmbox_append_message_list_no_lock(struct mailmbox_folder * folder,
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
  uint32_t i;
  size_t from_size;
  size_t maxuid;
  size_t left;
  size_t crlf_count;

  if (folder->read_only) {
    res = MAILMBOX_ERROR_READONLY;
    goto err;
  }

  date = time(NULL);
  from_size = strlen(DEFAULT_FROM_LINE);
  if (localtime_r(&date, &time_info) != NULL)
    from_size = strftime(from_line, MAX_FROM_LINE_SIZE, "From - %c\n", &time_info);

  maxuid = /* */ folder->max_uid;

  extra_size = 0;
  for(i = 0 ; i < append_tab->len ; i ++) {
    struct mailmbox_append_info * info;

    info = carray_get(append_tab, i);
    extra_size += from_size;
    extra_size += get_fixed_message_size(info->message, info->size,
					 folder->max_uid + i + 1,
					 folder->no_uid);
    extra_size += 2; /* CR LF */
  }

  left = folder->mapping_size;
  crlf_count = 0;
  while (left >= 1) {
    if (folder->mapping[left - 1] == '\n') {
      crlf_count ++;
      left --;
    }
    else if (folder->mapping[left - 1] == '\r') {
      left --;
    }
    else
      break;

    if (crlf_count == 2)
      break;
  }

  old_size = folder->mapping_size;
  mailmbox_unmap(folder);

  if (old_size != 0) {
    if (crlf_count != 2)
      extra_size += (2 - crlf_count) * 2;
  }

#ifndef WIN32
  r = ftruncate(folder->fd, extra_size + old_size);
  if (r < 0) {
    mailmbox_map(folder);
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

  r = mailmbox_map(folder);
  if (r < 0) {
    ftruncate(folder->fd, old_size);
    return MAILMBOX_ERROR_FILE;
  }
#endif

  str = folder->mapping + old_size;

  if (old_size != 0) {
    for(i = 0 ; i < 2 - crlf_count ; i ++) {
      * str = '\r';
      str ++;
      * str = '\n';
      str ++;
    }
  }

  for(i = 0 ; i < append_tab->len ; i ++) {
    struct mailmbox_append_info * info;

    info = carray_get(append_tab, i);

    memcpy(str, from_line, from_size);

    str += strlen(from_line);

    str = write_fixed_message(str, info->message, info->size,
			      folder->max_uid + i + 1,
			      folder->no_uid);

    * str = '\r';
    str ++;
    * str = '\n';
    str ++;
  }

  folder->max_uid += append_tab->len;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int
mailmbox_append_message_list(struct mailmbox_folder * folder,
			     carray * append_tab)
{
  int r;
  int res;
  size_t cur_token;

  r = mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmbox_expunge_no_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  cur_token = folder->mapping_size;

  r = mailmbox_append_message_list_no_lock(folder, append_tab);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  mailmbox_sync(folder);

  r = mailmbox_parse_additionnal(folder, &cur_token);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  mailmbox_timestamp(folder);

  mailmbox_write_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  mailmbox_write_unlock(folder);
 err:
  return res;
}

int
mailmbox_append_message(struct mailmbox_folder * folder,
			char * data, size_t len)
{
  carray * tab;
  struct mailmbox_append_info * append_info;
  int res;
  int r;

  tab = carray_new(1);
  if (tab == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto err;
  }

  append_info = mailmbox_append_info_new(data, len);
  if (append_info == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto free_list;
  }

  r = carray_add(tab, append_info, NULL);
  if (r < 0) {
    res = MAILMBOX_ERROR_MEMORY;
    goto free_append_info;
  }

  r = mailmbox_append_message_list(folder, tab);

  mailmbox_append_info_free(append_info);
  carray_free(tab);

  return r;

 free_append_info:
  mailmbox_append_info_free(append_info);
 free_list:
  carray_free(tab);
 err:
  return res;
}

/* ********************************************************************** */

int mailmbox_fetch_msg_no_lock(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len)
{
  struct mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = (char *) &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }
  
  info = (struct mailmbox_msg_info *) data.data;
  
  if (info->deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  * result = folder->mapping + info->headers;
  * result_len = info->size - info->start_len;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int mailmbox_fetch_msg_headers_no_lock(struct mailmbox_folder * folder,
				       uint32_t num, char ** result,
				       size_t * result_len)
{
  struct mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;
  
  key.data = (char *) &num;
  key.len = sizeof(num);
  
  r = chash_get(folder->hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info = (struct mailmbox_msg_info *) data.data;

  if (info->deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  * result = folder->mapping + info->headers;
  * result_len = info->headers_len;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int mailmbox_fetch_msg(struct mailmbox_folder * folder,
		       uint32_t num, char ** result,
		       size_t * result_len)
{
  int res;
  char * data;
  size_t len;
  int r;
  size_t fixed_size;
  char * end;
  char * new_data;
  
  r = mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmbox_fetch_msg_no_lock(folder, num, &data, &len);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }
  
  /* size with no uid */
  fixed_size = get_fixed_message_size(data, len, 0, 1 /* force no uid */);
  
  new_data = malloc(fixed_size + 1);
  if (new_data == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
  
  end = write_fixed_message(new_data, data, len, 0, 1 /* force no uid */);
  * end = '\0';
  
  * result = new_data;
  * result_len = fixed_size;

  mailmbox_read_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  mailmbox_read_unlock(folder);
 err:
  return res;
}

int mailmbox_fetch_msg_headers(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len)
{
  int res;
  char * data;
  size_t len;
  int r;
  size_t fixed_size;
  char * end;
  char * new_data;

  r = mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmbox_fetch_msg_headers_no_lock(folder, num, &data, &len);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlock;
  }

  /* size with no uid */
  fixed_size = get_fixed_message_size(data, len, 0, 1 /* force no uid */);
  
  new_data = malloc(fixed_size + 1);
  if (new_data == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto unlock;
  }
  
  end = write_fixed_message(new_data, data, len, 0, 1 /* force no uid */);
  * end = '\0';

  * result = new_data;
  * result_len = fixed_size;

  mailmbox_read_unlock(folder);

  return MAILMBOX_NO_ERROR;

 unlock:
  mailmbox_read_unlock(folder);
 err:
  return res;
}

void mailmbox_fetch_result_free(char * msg)
{
        free(msg);
}


int mailmbox_copy_msg_list(struct mailmbox_folder * dest_folder,
			   struct mailmbox_folder * src_folder,
			   carray * tab)
{
  int r;
  int res;
  carray * append_tab;
  uint32_t i;

  r = mailmbox_validate_read_lock(src_folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  append_tab = carray_new(tab->len);
  if (append_tab == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto src_unlock;
  }

  for(i = 0 ; i < tab->len ; i ++) {
    struct mailmbox_append_info * append_info;
    char * data;
    size_t len;
    uint32_t uid;

    uid = * ((uint32_t *) carray_get(tab, i));

    r = mailmbox_fetch_msg_no_lock(src_folder, uid, &data, &len);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto free_list;
    }
    
    append_info = mailmbox_append_info_new(data, len);
    if (append_info == NULL) {
      res = MAILMBOX_ERROR_MEMORY;
      goto free_list;
    }
    
    r = carray_add(append_tab, append_info, NULL);
    if (r < 0) {
      mailmbox_append_info_free(append_info);
      res = MAILMBOX_ERROR_MEMORY;
      goto free_list;
    }
  }    

  r = mailmbox_append_message_list(dest_folder, append_tab);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto src_unlock;
  }

  for(i = 0 ; i < append_tab->len ; i ++) {
    struct mailmbox_append_info * append_info;

    append_info = carray_get(append_tab, i);
    mailmbox_append_info_free(append_info);
  }
  carray_free(append_tab);

  mailmbox_read_unlock(src_folder);

  return MAILMBOX_NO_ERROR;

 free_list:
  for(i = 0 ; i < append_tab->len ; i ++) {
    struct mailmbox_append_info * append_info;

    append_info = carray_get(append_tab, i);
    mailmbox_append_info_free(append_info);
  }
  carray_free(append_tab);
 src_unlock:
  mailmbox_read_unlock(src_folder);
 err:
  return res;
}

int mailmbox_copy_msg(struct mailmbox_folder * dest_folder,
		      struct mailmbox_folder * src_folder,
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
  
  r = mailmbox_copy_msg_list(dest_folder, src_folder, tab);
  res = r;

  free(puid);
 free_array:
  carray_free(tab);
 err:
  return res;
}

static int mailmbox_expunge_to_file_no_lock(char * dest_filename, int dest_fd,
    struct mailmbox_folder * folder,
    size_t * result_size)
{
  int r;
  int res;
  unsigned long i;
  size_t cur_offset;
  char * dest;
  size_t size;

  size = 0;
  for(i = 0 ; i < folder->tab->len ; i ++) {
    struct mailmbox_msg_info * info;

    info = carray_get(folder->tab, i);

    if (!info->deleted) {
      size += info->size + info->padding;

      if (!folder->no_uid) {
	if (!info->written_uid) {
	  uint32_t uid;
	  
	  size += strlen(UID_HEADER " \r\n");
	  
	  uid = info->uid;
	  while (uid >= 10) {
	    uid /= 10;
	    size ++;
	  }
	  size ++;
	}
      }
    }
  }

#ifndef WIN32
  r = ftruncate(dest_fd, size);
  if (r < 0) {
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }

  dest = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, dest_fd, 0);
  if (dest == MAP_FAILED) {
    res = MAILMBOX_ERROR_FILE;
    goto err;
  }
#endif

  cur_offset = 0;
  for(i = 0 ; i < folder->tab->len ; i ++) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->tab, i);

    if (!info->deleted) {
      memcpy(dest + cur_offset, folder->mapping + info->start,
	     info->headers_len + info->start_len);
      cur_offset += info->headers_len + info->start_len;
      
      if (!folder->no_uid) {
	if (!info->written_uid) {
	  size_t numlen;
	  
	  memcpy(dest + cur_offset, UID_HEADER " ", strlen(UID_HEADER " "));
	  cur_offset += strlen(UID_HEADER " ");
	  numlen = snprintf(dest + cur_offset, size - cur_offset,
			    "%i\r\n", info->uid);
	  cur_offset += numlen;
	}
      }
      
      memcpy(dest + cur_offset,
	     folder->mapping + info->headers + info->headers_len,
	     info->size - (info->start_len + info->headers_len)
	     + info->padding);
      
      cur_offset += info->size - (info->start_len + info->headers_len)
	+ info->padding;
    }
  }
  fflush(stdout);

#ifndef WIN32
  munmap(dest, size);
#endif

  * result_size = size;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

int mailmbox_expunge_no_lock(struct mailmbox_folder * folder)
{
  char tmpfile[PATH_MAX];
  int r;
  int res;
  int dest_fd;
  size_t size;

  if (folder->read_only)
    return MAILMBOX_ERROR_READONLY;

  if (((folder->written_uid >= folder->max_uid) || folder->no_uid) &&
      (!folder->changed)) {
    /* no need to expunge */
    return MAILMBOX_NO_ERROR;
  }

  snprintf(tmpfile, PATH_MAX, "%sXXXXXX", folder->filename);
  dest_fd = mkstemp(tmpfile);

  if (dest_fd < 0) {
    res = MAILMBOX_ERROR_FILE;
    goto unlink;
  }

  r = mailmbox_expunge_to_file_no_lock(tmpfile, dest_fd,
				       folder, &size);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unlink;
  }

  close(dest_fd);

  r = rename(tmpfile, folder->filename);
  if (r < 0) {
    res = r;
    goto err;
  }

  mailmbox_unmap(folder);
  mailmbox_close(folder);

  r = mailmbox_open(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmbox_map(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }
      
  r = mailmbox_parse(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }
  
  mailmbox_timestamp(folder);

  folder->changed = FALSE;
  folder->deleted_count = 0;
  
  return MAILMBOX_NO_ERROR;

 unlink:
  close(dest_fd);
  unlink(tmpfile);
 err:
  return res;
}

int mailmbox_expunge(struct mailmbox_folder * folder)
{
  int r;
  int res;

  r = mailmbox_validate_write_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailmbox_expunge_no_lock(folder);
  res = r;

  mailmbox_write_unlock(folder);
 err:
  return res;
}

int mailmbox_delete_msg(struct mailmbox_folder * folder, uint32_t uid)
{
  struct mailmbox_msg_info * info;
  int res;
  chashdatum key;
  chashdatum data;
  int r;

  if (folder->read_only) {
    res = MAILMBOX_ERROR_READONLY;
    goto err;
  }
  
  key.data = (char *) &uid;
  key.len = sizeof(uid);
  
  r = chash_get(folder->hash, &key, &data);
  if (r < 0) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info = (struct mailmbox_msg_info *) data.data;

  if (info->deleted) {
    res = MAILMBOX_ERROR_MSG_NOT_FOUND;
    goto err;
  }

  info->deleted = TRUE;
  folder->changed = TRUE;
  folder->deleted_count ++;

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

int mailmbox_init(char * filename,
		  int force_readonly,
		  int force_no_uid,
		  uint32_t default_written_uid,
		  struct mailmbox_folder ** result_folder)
{
  struct mailmbox_folder * folder;
  int r;
  int res;
  
  folder = mailmbox_folder_new(filename);
  if (folder == NULL) {
    res = MAILMBOX_ERROR_MEMORY;
    goto err;
  }
  folder->no_uid = force_no_uid;
  folder->read_only = force_readonly;
  folder->written_uid = default_written_uid;
  
  folder->changed = FALSE;
  folder->deleted_count = 0;
  
  r = mailmbox_open(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto free;
  }

  r = mailmbox_map(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto close;
  }

  r = mailmbox_validate_read_lock(folder);
  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto unmap;
  }

  mailmbox_read_unlock(folder);

  * result_folder = folder;

  return MAILMBOX_NO_ERROR;

 unmap:
  mailmbox_unmap(folder);
 close:
  mailmbox_close(folder);
 free:
  mailmbox_folder_free(folder);
 err:
  return res;
}


/*
  when MBOX is DONE

  - check for changes

  - unmap the file
  - close file
*/

void mailmbox_done(struct mailmbox_folder * folder)
{
  if (!folder->read_only)
    mailmbox_expunge(folder);
  
  mailmbox_unmap(folder);
  mailmbox_close(folder);

  mailmbox_folder_free(folder);
}
