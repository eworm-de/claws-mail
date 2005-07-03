/*
 * etPan! -- a mail user agent
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

#ifndef ETPAN_ERRORS_H

#define ETPAN_ERRORS_H

enum {
  NO_ERROR = 0,
  ERROR_CACHE,
  ERROR_MEMORY,
  ERROR_CONFIG,
  ERROR_INVAL,
  ERROR_PARSE,
  ERROR_MOVE,
  ERROR_COPY,
  ERROR_MSG_LIST,
  ERROR_ENV_LIST,
  ERROR_CONNECT, /* 10 */
  ERROR_FETCH,
  ERROR_APPEND,
  ERROR_SEND,
  ERROR_NOT_IMPLEMENTED,
  ERROR_FILE,
  ERROR_TOO_MUCH_RECIPIENT,
  ERROR_COMMAND,
  ERROR_STREAM,
  ERROR_CANCELLED,
  ERROR_CREATE, /* 20 */
  ERROR_UNKNOWN_PROTOCOL,
  ERROR_COULD_NOT_ENCRYPT,
  ERROR_CHECK,
  ERROR_STATUS,
  ERROR_EXPUNGE,
  ERROR_MARK_AS_SPAM,
  ERROR_BUSY,
  ERROR_NEWSGROUPS_LIST,
  ERROR_IMAP_MAILBOX_LIST,
  ERROR_IMAP_SELECT, /* 30 */
  ERROR_IMAP_CREATE,
  ERROR_PRIVACY,
  ERROR_DIRECTORY_NOT_FOUND,
  ERROR_LDAP_SEARCH,
  ERROR_NOT_SUPPORTED,
  ERROR_COULD_NOT_POSTPONE,
  ERROR_NO_FROM,
  ERROR_AUTH,
};

#endif
