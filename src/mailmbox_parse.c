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

#include "mailmbox_parse.h"

#include "mailmbox.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define UID_HEADER "X-LibEtPan-UID:"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum {
  UNSTRUCTURED_START,
  UNSTRUCTURED_CR,
  UNSTRUCTURED_LF,
  UNSTRUCTURED_WSP,
  UNSTRUCTURED_OUT
};

/* begin extracted from imf/mailimf.c */

static int mailimf_char_parse(char * message, size_t length,
		       size_t * index, char token)
{
  size_t cur_token;

  cur_token = * index;

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  if (message[cur_token] == token) {
    cur_token ++;
    * index = cur_token;
    return MAILIMF_NO_ERROR;
  }
  else
    return MAILIMF_ERROR_PARSE;
}

int mailimf_crlf_parse(char * message, size_t length, size_t * index)
{
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_char_parse(message, length, &cur_token, '\r');
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_char_parse(message, length, &cur_token, '\n');
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  return MAILIMF_NO_ERROR;
}


int mailimf_ignore_field_parse(char * message, size_t length,
			       size_t * index)
{
  int has_field;
  size_t cur_token;
  int state;
  size_t terminal;

  has_field = FALSE;
  cur_token = * index;

  terminal = cur_token;
  state = UNSTRUCTURED_START;

  /* check if this is not a beginning CRLF */

  if (cur_token >= length)
    return MAILIMF_ERROR_PARSE;

  switch (message[cur_token]) {
  case '\r':
    return MAILIMF_ERROR_PARSE;
  case '\n':
    return MAILIMF_ERROR_PARSE;
  }

  while (state != UNSTRUCTURED_OUT) {

    switch(state) {
    case UNSTRUCTURED_START:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_CR:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    case UNSTRUCTURED_LF:
      if (cur_token >= length) {
	terminal = cur_token;
	state = UNSTRUCTURED_OUT;
	break;
      }

      switch(message[cur_token]) {
      case '\t':
      case ' ':
	state = UNSTRUCTURED_WSP;
	break;
      default:
	terminal = cur_token;
	state = UNSTRUCTURED_OUT;
	break;
      }
      break;
    case UNSTRUCTURED_WSP:
      if (cur_token >= length)
	return MAILIMF_ERROR_PARSE;

      switch(message[cur_token]) {
      case '\r':
	state = UNSTRUCTURED_CR;
	break;
      case '\n':
	state = UNSTRUCTURED_LF;
	break;
      case ':':
	has_field = TRUE;
	state = UNSTRUCTURED_START;
	break;
      default:
	state = UNSTRUCTURED_START;
	break;
      }
      break;
    }

    cur_token ++;
  }

  if (!has_field)
    return MAILIMF_ERROR_PARSE;

  * index = terminal;

  return MAILIMF_NO_ERROR;
}

/* end - extracted from imf/mailimf.c */

static inline int
mailmbox_fields_parse(char * str, size_t length,
		      size_t * index,
		      guint * puid,
		      size_t * phlen)
{
  size_t cur_token;
  int r;
  size_t hlen;
  size_t uid;
  int end;

  cur_token = * index;

  end = FALSE;
  uid = 0;
  while (!end) {
    size_t begin;

    begin = cur_token;

    r = mailimf_ignore_field_parse(str, length, &cur_token);
    switch (r) {
    case MAILIMF_NO_ERROR:
      if (str[begin] == 'X') {

	if (strncasecmp(str + begin, UID_HEADER, strlen(UID_HEADER)) == 0) {
	  begin += strlen(UID_HEADER);

	  while (str[begin] == ' ')
	    begin ++;
	  
	  uid = strtoul(str + begin, NULL, 10);
	}
      }
      
      break;
    case MAILIMF_ERROR_PARSE:
    default:
      end = TRUE;
      break;
    }
  }

  hlen = cur_token - * index;

  * phlen = hlen;
  * puid = uid;
  * index = cur_token;

  return MAILMBOX_NO_ERROR;
}

enum {
  IN_MAIL,
  FIRST_CR,
  FIRST_LF,
  SECOND_CR,
  SECOND_LF,
  PARSING_F,
  PARSING_R,
  PARSING_O,
  PARSING_M,
  OUT_MAIL
};




static inline int
mailmbox_single_parse(char * str, size_t length,
		      size_t * index,
		      size_t * pstart,
		      size_t * pstart_len,
		      size_t * pheaders,
		      size_t * pheaders_len,
		      size_t * pbody,
		      size_t * pbody_len,
		      size_t * psize,
		      size_t * ppadding,
		      guint * puid)
{
  size_t cur_token;
  size_t start;
  size_t start_len;
  size_t headers;
  size_t headers_len;
  size_t body;
  size_t end;
  size_t next;
  size_t message_length;
  guint uid;
  int r;
#if 0
  int in_mail_data;
#endif
#if 0
  size_t begin;
#endif

  int state;

  cur_token = * index;

  if (cur_token >= length)
    return MAILMBOX_ERROR_PARSE;

  start = cur_token;
  start_len = 0;
  headers = cur_token;

  if (cur_token + 5 < length) {
    if (strncmp(str + cur_token, "From ", 5) == 0) {
      cur_token += 5;
      while (str[cur_token] != '\n') {
        cur_token ++;
        if (cur_token >= length)
          break;
      }
      if (cur_token < length) {
        cur_token ++;
        headers = cur_token;
        start_len = headers - start;
      }
    }
  }

  next = length;

  r = mailmbox_fields_parse(str, length, &cur_token,
			    &uid, &headers_len);
  if (r != MAILMBOX_NO_ERROR)
    return r;

  /* save position */
#if 0
  begin = cur_token;
#endif
  
  mailimf_crlf_parse(str, length, &cur_token);

#if 0
  if (str[cur_token] == 'F') {
    printf("start !\n");
    printf("%50.50s\n", str + cur_token);
    getchar();
  }
#endif
  
  body = cur_token;

  /* restore position */
  /*  cur_token = begin; */

  state = FIRST_LF;

  end = length;

#if 0
  in_mail_data = 0;
#endif
  while (state != OUT_MAIL) {

    if (cur_token >= length) {
      if (state == IN_MAIL)
	end = length;
      next = length;
      break;
    }

    switch(state) {
    case IN_MAIL:
      switch(str[cur_token]) {
      case '\r':
        state = FIRST_CR;
        break;
      case '\n':
        state = FIRST_LF;
        break;
      case 'F':
        if (cur_token == body) {
          end = cur_token;
          next = cur_token;
          state = PARSING_F;
        }
        break;
#if 0
      default:
        in_mail_data = 1;
        break;
#endif
      }
      break;
      
    case FIRST_CR:
      end = cur_token;
      switch(str[cur_token]) {
      case '\r':
        state = SECOND_CR;
        break;
      case '\n':
        state = FIRST_LF;
        break;
      default:
        state = IN_MAIL;
#if 0
        in_mail_data = 1;
#endif
        break;
      }
      break;
      
    case FIRST_LF:
      end = cur_token;
      switch(str[cur_token]) {
      case '\r':
        state = SECOND_CR;
        break;
      case '\n':
        state = SECOND_LF;
        break;
      default:
        state = IN_MAIL;
#if 0
        in_mail_data = 1;
#endif
        break;
      }
      break;
      
    case SECOND_CR:
      switch(str[cur_token]) {
        case '\r':
          end = cur_token;
          break;
        case '\n':
          state = SECOND_LF;
          break;
        case 'F':
          next = cur_token;
          state = PARSING_F;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;

    case SECOND_LF:
      switch(str[cur_token]) {
        case '\r':
          state = SECOND_CR;
          break;
        case '\n':
          end = cur_token;
          break;
        case 'F':
          next = cur_token;
          state = PARSING_F;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_F:
      switch(str[cur_token]) {
        case 'r':
          state = PARSING_R;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_R:
      switch(str[cur_token]) {
        case 'o':
          state = PARSING_O;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;
      
    case PARSING_O:
      switch(str[cur_token]) {
        case 'm':
          state = PARSING_M;
          break;
        default:
          state = IN_MAIL;
#if 0
          in_mail_data = 1;
#endif
          break;
      }
      break;

    case PARSING_M:
      switch(str[cur_token]) {
        case ' ':
          state = OUT_MAIL;
          break;
      default:
          state = IN_MAIL;
          break;
      }
      break;
    }
    
    cur_token ++;
  }

  message_length = end - start;

  * pstart = start;
  * pstart_len = start_len;
  * pheaders = headers;
  * pheaders_len = headers_len;
  * pbody = body;
  * pbody_len = end - body;
  * psize = message_length;
  * ppadding = next - end;
  * puid = uid;

  * index = next;

  return MAILMBOX_NO_ERROR;
}


int
mailmbox_parse_additionnal(struct mailmbox_folder * folder,
			   size_t * index)
{
  size_t cur_token;

  size_t start;
  size_t start_len;
  size_t headers;
  size_t headers_len;
  size_t body;
  size_t body_len;
  size_t size;
  size_t padding;
  guint uid;
  int r;
  int res;

  guint max_uid;
  guint first_index;
  guint i;
  guint j;

  cur_token = * index;

  /* remove temporary UID that we will parse */

  first_index = folder->tab->len;

  for(i = 0 ; i < folder->tab->len ; i++) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->tab, i);

    if (info->start < cur_token) {
      continue;
    }

    if (!info->written_uid) {
      chashdatum key;
      
      key.data = (char *) &info->uid;
      key.len = sizeof(info->uid);
      
      chash_delete(folder->hash, &key, NULL);
      carray_delete_fast(folder->tab, i);
      mailmbox_msg_info_free(info);
      if (i < first_index)
	first_index = i;
    }
  }

  /* make a sequence in the table */

  max_uid = folder->written_uid;

  i = 0;
  j = 0;
  while (i < folder->tab->len) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->tab, i);
    if (info != NULL) {
      carray_set(folder->tab, j, info);

      if (info->uid > max_uid)
	max_uid = info->uid;

      info->index = j;
      j ++;
    }
    i ++;
  }
  carray_set_size(folder->tab, j);

  /* parse content */

  first_index = j;

  while (1) {
    struct mailmbox_msg_info * info;
    chashdatum key;
    chashdatum data;
    
    r = mailmbox_single_parse(folder->mapping, folder->mapping_size,
			      &cur_token,
			      &start, &start_len,
			      &headers, &headers_len,
			      &body, &body_len,
			      &size, &padding, &uid);
    if (r == MAILMBOX_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILMBOX_ERROR_PARSE)
      break;
    else {
      res = r;
      goto err;
    }
    
    key.data = (char *) &uid;
    key.len = sizeof(uid);
    
    r = chash_get(folder->hash, &key, &data);
    if (r == 0) {
      info = (struct mailmbox_msg_info *) data.data;
      
      if (!info->written_uid) {
	/* some new mail has been written and override an
	   existing temporary UID */
        
	chash_delete(folder->hash, &key, NULL);
	info->uid = 0;

	if (info->index < first_index)
	  first_index = info->index;
      }
      else
        uid = 0;
    }

    if (uid > max_uid)
      max_uid = uid;

    r = mailmbox_msg_info_update(folder,
				 start, start_len, headers, headers_len,
				 body, body_len, size, padding, uid);
    if (r != MAILMBOX_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  * index = cur_token;

  folder->written_uid = max_uid;

  /* attribute uid */

  for(i = first_index ; i < folder->tab->len ; i ++) {
    struct mailmbox_msg_info * info;
    chashdatum key;
    chashdatum data;

    info = carray_get(folder->tab, i);

    if (info->uid != 0) {
      continue;
    }

    max_uid ++;
    info->uid = max_uid;
    
    key.data = (char *) &info->uid;
    key.len = sizeof(info->uid);
    data.data = (char *) info;
    data.len = 0;
    
    r = chash_set(folder->hash, &key, &data, NULL);
    if (r < 0) {
      res = MAILMBOX_ERROR_MEMORY;
      goto err;
    }
  }

  folder->max_uid = max_uid;

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}

static void flush_uid(struct mailmbox_folder * folder)
{
  guint i;
  
  for(i = 0 ; i < folder->tab->len ; i++) {
    struct mailmbox_msg_info * info;
    
    info = carray_get(folder->tab, i);
    if (info != NULL)
      mailmbox_msg_info_free(info);
  }
  
  chash_clear(folder->hash);
  carray_set_size(folder->tab, 0);
}

int mailmbox_parse(struct mailmbox_folder * folder)
{
  int r;
  int res;
  size_t cur_token;

  flush_uid(folder);
  
  cur_token = 0;

  r = mailmbox_parse_additionnal(folder, &cur_token);

  if (r != MAILMBOX_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAILMBOX_NO_ERROR;

 err:
  return res;
}
