/* Perl plugin -- Perl Support for Claws Mail
 *
 * Copyright (C) 2004-2022 Holger Berndt and the Claws Mail Team
 *
 * Claws Mail are GTK based, lightweight, and fast e-mail clients
 * Copyright (C) 1999-2022 the Claws Mail Team
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

#include "common/version.h"
#include "common/defs.h"
#include "common/utils.h"
#include "common/claws.h"
#include "common/prefs.h"
#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "account.h"
#include "compose.h"
#include "addrindex.h"
#include "addritem.h"
#include "addr_compl.h"
#include "statusbar.h"
#include "alertpanel.h"
#include "common/hooks.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "common/log.h"
#include "common/plugin.h"
#include "common/tags.h"
#include "file-utils.h"

#ifdef YYEMPTY
# undef YYEMPTY
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#ifdef _
# undef _
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "perl_plugin.h"
#include "perl_gtk.h"


/* XSRETURN_UV was introduced in Perl 5.8.1,
   this fixes things for 5.8.0. */
#ifndef XSRETURN_UV
#  ifndef XST_mUV
#    define XST_mUV(i,v)  (ST(i) = sv_2mortal(newSVuv(v))  )
#  endif /* XST_mUV */
#  define XSRETURN_UV(v) STMT_START { XST_mUV(0,v);  XSRETURN(1); } STMT_END
#endif /* XSRETURN_UV */

/* set this to "1" to recompile the Perl script for every mail,
   even if it hasn't changed */
#define DO_CLEAN "0"

/* distinguish between automatic and manual filtering */
#define AUTO_FILTER 0
#define MANU_FILTER 1

/* embedded Perl stuff */
static PerlInterpreter *my_perl = NULL;
EXTERN_C void xs_init(pTHX);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

/* plugin stuff */
static guint             filtering_hook_id = HOOK_NONE;
static guint             manual_filtering_hook_id = HOOK_NONE;
static MailFilteringData *mail_filtering_data  = NULL;
static MsgInfo           *msginfo              = NULL;
static gboolean          stop_filtering        = FALSE;
static gboolean          manual_filtering      = FALSE;
static gboolean          wrote_filter_log_head = FALSE;
static gint              filter_log_verbosity;
static FILE              *message_file         = NULL;
static gchar             *attribute_key        = NULL;

/* configuration */
static PerlPluginConfig config;

static PrefParam param[] = {
  {"filter_log_verbosity", "2", &config.filter_log_verbosity,
   P_INT, NULL, NULL, NULL},
  {NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};


/* Utility functions */

/* fire and forget */
gint execute_detached(gchar **cmdline)
{
  pid_t pid;
  
  if((pid = fork()) < 0) { /* fork error */
    perror("fork");
    return 0;
  }
  else if(pid > 0) {       /* parent */
    waitpid(pid, NULL, 0);
    return 1;
  }
  else {                   /* child */
    if((pid = fork()) < 0) { /* fork error */
      perror("fork");
      return 0;
    }
    else if(pid > 0) {     /* child */
      /* make grand child an orphan */
      _exit(0);
    }
    else {                 /* grand child */
      execvp(cmdline[0], cmdline);
      perror("execvp");
      _exit(1);
    }
  }
  return 0;
}


/* filter logfile */
#define LOG_MANUAL 1
#define LOG_ACTION 2
#define LOG_MATCH  3

static void filter_log_write(gint type, gchar *text) {
  if(filter_log_verbosity >= type) {
    if(!wrote_filter_log_head) {
      log_message(LOG_PROTOCOL, "From: %s || Subject: %s || Message-ID: %s\n",
        msginfo->from    ? msginfo->from    : "<no From header>",
        msginfo->subject ? msginfo->subject : "<no Subject header>",
        msginfo->msgid   ? msginfo->msgid   : "<no message id>");
      wrote_filter_log_head = TRUE;
    }
    switch(type) {
    case LOG_MANUAL:
      log_message(LOG_PROTOCOL, "    MANUAL: %s\n", text?text:"<no text specified>");
      break;
    case LOG_ACTION:
      log_message(LOG_PROTOCOL, "    ACTION: %s\n", text?text:"<no text specified>");
      break;
    case LOG_MATCH:
      log_message(LOG_PROTOCOL, "    MATCH:  %s\n", text?text:"<no text specified>");
      break;
    default:
      g_warning("Perl plugin: wrong use of filter_log_write");
      break;
    }
  }
}

/* Addressbook interface */

static PerlPluginTimedSList *email_slist = NULL;
static GHashTable *attribute_hash        = NULL;

/* addressbook email collector callback */
static gint add_to_email_slist(ItemPerson *person, const gchar *bookname)
{
  PerlPluginEmailEntry *ee;
  GList *nodeM;

  /* Process each E-Mail address */
  nodeM = person->listEMail;
  while(nodeM) {
    ItemEMail *email = nodeM->data;
    ee = g_new0(PerlPluginEmailEntry,1);
    g_return_val_if_fail(ee != NULL, -1);

    if(email->address != NULL) ee->address  = g_strdup(email->address);
    else                       ee->address  = NULL;
    if(bookname != NULL)       ee->bookname = g_strdup(bookname);
    else                       ee->bookname = NULL;

    email_slist->g_slist = g_slist_prepend(email_slist->g_slist,ee);
    nodeM = g_list_next(nodeM);
  }
  return 0;
}

/* free a GSList of PerlPluginEmailEntry's. */
static void free_PerlPluginEmailEntry_slist(GSList *slist)
{
  GSList *walk;

  if(slist == NULL)
    return;

  walk = slist;
  for(; walk != NULL; walk = g_slist_next(walk)) {
    PerlPluginEmailEntry *ee = (PerlPluginEmailEntry *) walk->data;
    if(ee != NULL) {
      if(ee->address  != NULL) g_free(ee->address);
      if(ee->bookname != NULL) g_free(ee->bookname);
      g_free(ee);
      ee = NULL;
    }
  }
  g_slist_free(slist);

  debug_print("PerlPluginEmailEntry slist freed\n");
}

/* free email_slist */
static void free_email_slist(void)
{
  if(email_slist == NULL)
    return;

  free_PerlPluginEmailEntry_slist(email_slist->g_slist);
  email_slist->g_slist = NULL;

  g_free(email_slist);
  email_slist = NULL;

  debug_print("email_slist freed\n");
}

/* check if tl->g_slist exists and is recent enough */
static gboolean update_PerlPluginTimedSList(PerlPluginTimedSList *tl)
{
  gboolean retVal;
  gchar *indexfile;
  GStatBuf filestat;

  if(tl->g_slist == NULL)
    return TRUE;

  indexfile = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ADDRESSBOOK_INDEX_FILE, NULL);
  if((g_stat(indexfile,&filestat) == 0) && filestat.st_mtime <= tl->mtime)
     retVal = FALSE;
  else
    retVal = TRUE;

  g_free(indexfile);
  return retVal;
}

/* (re)initialize email slist */
static void init_email_slist(void)
{
  gchar *indexfile;
  GStatBuf filestat;

  if(email_slist->g_slist != NULL) {
    free_PerlPluginEmailEntry_slist(email_slist->g_slist);
    email_slist->g_slist = NULL;
  }

  addrindex_load_person_attribute(NULL,add_to_email_slist);

  indexfile = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ADDRESSBOOK_INDEX_FILE, NULL);
  if(g_stat(indexfile,&filestat) == 0)
    email_slist->mtime = filestat.st_mtime;
  g_free(indexfile);
  debug_print("Initialisation of email slist completed\n");
}

/* check if given address is in given addressbook */
static gboolean addr_in_addressbook(gchar *addr, gchar *bookname)
{
  /* If no book is given, check the address completion list
   * (there may be other addresses that are not in the address book,
   * added by other plugins). */
  if(bookname == NULL) {
    gboolean found;
    start_address_completion(NULL);
    found = (complete_matches_found(addr) > 0);
    end_address_completion();
    return found;
  }
  else {
    GSList *walk;

    /* check if email_list exists */
    if(email_slist == NULL) {
      email_slist = g_new0(PerlPluginTimedSList,1);
      email_slist->g_slist = NULL;
      debug_print("email_slist created\n");
    }

    if(update_PerlPluginTimedSList(email_slist))
      init_email_slist();

    walk = email_slist->g_slist;
    for(; walk != NULL; walk = g_slist_next(walk)) {
      PerlPluginEmailEntry *ee = (PerlPluginEmailEntry *) walk->data;
      gchar *a = g_utf8_casefold(ee->address, -1);
      gchar *b = g_utf8_casefold(addr, -1);
      if((!g_utf8_collate(a,b)) &&
         ((bookname == NULL) || (!strcmp(ee->bookname,bookname)))) {
        g_free(a);
        g_free(b);
        return TRUE;
      }
      g_free(a);
      g_free(b);
    }
  }

  return FALSE;
}

/* attribute hash collector callback */
static gint add_to_attribute_hash(ItemPerson *person, const gchar *bookname)
{
  PerlPluginTimedSList *tl;
  PerlPluginAttributeEntry *ae;
  GList *nodeA;
  GList *nodeM;

  nodeA = person->listAttrib;
  /* Process each User Attribute */
  while(nodeA) {
    UserAttribute *attrib = nodeA->data;
    if(attrib->name && !strcmp(attrib->name,attribute_key) ) {
      /* Process each E-Mail address */
      nodeM = person->listEMail;
      while(nodeM) {
  ItemEMail *email = nodeM->data;

  ae = g_new0(PerlPluginAttributeEntry,1);
  g_return_val_if_fail(ae != NULL, -1);

  if(email->address != NULL) ae->address  = g_strdup(email->address);
  else                       ae->address  = NULL;
  if(attrib->value  != NULL) ae->value    = g_strdup(attrib->value);
  else                       ae->value    = NULL;
  if(bookname != NULL)       ae->bookname = g_strdup(bookname);
  else                       ae->bookname = NULL;

  tl = (PerlPluginTimedSList *) g_hash_table_lookup(attribute_hash,attribute_key);
  tl->g_slist = g_slist_prepend(tl->g_slist,ae);

  nodeM = g_list_next(nodeM);
      }
    }
    nodeA = g_list_next(nodeA);
  }
  
  return 0;
}

/* free a key of the attribute hash */
static gboolean free_attribute_hash_key(gpointer key, gpointer value, gpointer user_data)
{
  GSList *walk;
  PerlPluginTimedSList *tl;

  debug_print("Freeing key `%s' from attribute_hash\n",key?(char*)key:"");

  tl = (PerlPluginTimedSList *) value;

  if(tl != NULL) {
    if(tl->g_slist != NULL) {
      walk = tl->g_slist;
      for(; walk != NULL; walk = g_slist_next(walk)) {
  PerlPluginAttributeEntry *ae = (PerlPluginAttributeEntry *) walk->data;
  if(ae != NULL) {
    if(ae->address  != NULL) g_free(ae->address);
    if(ae->value    != NULL) g_free(ae->value);
    if(ae->bookname != NULL) g_free(ae->bookname);
    g_free(ae);
    ae = NULL;
  }
      }
      g_slist_free(tl->g_slist);
      tl->g_slist = NULL;
    }
    g_free(tl);
    tl = NULL;
  }

  if(key != NULL) {
    g_free(key);
    key = NULL;
  }

  return TRUE;
}

/* free whole attribute hash */
static void free_attribute_hash(void)
{
  if(attribute_hash == NULL)
    return;

  g_hash_table_foreach_remove(attribute_hash,free_attribute_hash_key,NULL);
  g_hash_table_destroy(attribute_hash);
  attribute_hash = NULL;

  debug_print("attribute_hash freed\n");
}

/* Free the key if it exists. Insert the new key. */
static void insert_attribute_hash(gchar *attr)
{
  PerlPluginTimedSList *tl;
  gchar *indexfile;
  GStatBuf filestat;

  /* Check if key exists. Free it if it does. */
  if((tl = g_hash_table_lookup(attribute_hash,attr)) != NULL) {
    gpointer origkey;
    gpointer value;
    if (g_hash_table_lookup_extended(attribute_hash,attr,&origkey,&value)) {
	  g_hash_table_remove(attribute_hash,origkey);
      free_attribute_hash_key(origkey,value,NULL);
      debug_print("Existing key `%s' freed.\n",attr);
    }
  }

  tl = g_new0(PerlPluginTimedSList,1);
  tl->g_slist = NULL;

  attribute_key = g_strdup(attr);
  g_hash_table_insert(attribute_hash,attribute_key,tl);  

  addrindex_load_person_attribute(attribute_key,add_to_attribute_hash);

  indexfile = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ADDRESSBOOK_INDEX_FILE, NULL);
  if(g_stat(indexfile,&filestat) == 0)
    tl->mtime = filestat.st_mtime;
  g_free(indexfile);

  debug_print("added key `%s' to attribute_hash\n",attribute_key?attribute_key:"");
}

/* check if an update of the attribute hash entry is necessary */
static gboolean update_attribute_hash(const gchar *attr)
{
  PerlPluginTimedSList *tl;

  /* check if key attr exists in the attribute hash */
  if((tl = (PerlPluginTimedSList*) g_hash_table_lookup(attribute_hash,attr)) == NULL)
    return TRUE;

  /* check if entry is recent enough */
  return update_PerlPluginTimedSList(tl);
}

/* given an email address, return attribute value of specific book */
static gchar* get_attribute_value(gchar *email, gchar *attr, gchar *bookname)
{
  GSList *walk;
  PerlPluginTimedSList *tl;

  /* check if attribute hash exists */
  if(attribute_hash == NULL) {
    attribute_hash = g_hash_table_new(g_str_hash,g_str_equal);
    debug_print("attribute_hash created\n");
  }

  if(update_attribute_hash(attr)) {
    debug_print("Initialisation of attribute hash entry `%s' is necessary\n",attr);
    insert_attribute_hash(attr);
  }
  
  if((tl = (PerlPluginTimedSList*) g_hash_table_lookup(attribute_hash,attr)) == NULL)
    return NULL;  

  walk = tl->g_slist;
  for(; walk != NULL; walk = g_slist_next(walk)) {
    PerlPluginAttributeEntry *ae = (PerlPluginAttributeEntry *) walk->data;
    gchar *a, *b;
    a = g_utf8_strdown(ae->address, -1);
    b = g_utf8_strdown(email, -1);
    if(!g_utf8_collate(a, b)) {
      if((bookname == NULL) ||
   ((ae->bookname != NULL) && !strcmp(bookname,ae->bookname))) {
        g_free(a); g_free(b);
  return ae->value;
      }
    }
    g_free(a); g_free(b);
  }
  return NULL;
}

/* free up all memory allocated with lists */
static void free_all_lists(void)
{
  /* email list */
  free_email_slist();

  /* attribute hash */
  free_attribute_hash();
}



/* ClawsMail::C module */

/* Initialization */

/* ClawsMail::C::filter_init(int) */
static XS(XS_ClawsMail_filter_init)
{
  int flag;
  /* flags:
   *
   *    msginfo
   *          1 size
   *          2 date
   *          3 from
   *          4 to
   *          5 cc
   *          6 newsgroups
   *          7 subject
   *          8 msgid
   *          9 inreplyto
   *         10 xref
   *         11 xface
   *         12 dispositionnotificationto
   *         13 returnreceiptto
   *         14 references
   *         15 score
   *         16 not used anymore
   *         17 plaintext_file
   *         18 not used anymore
   *         19 hidden
   *         20 message file path
   *         21 partial_recv
   *         22 total_size
   *         23 account_server
   *         24 account_login
   *         25 planned_download
   *
   *    general
   *        100 manual
   */
  char *charp;
  gchar buf[BUFFSIZE];
  GSList *walk;
  int ii;
  gchar *xface;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::init");
    XSRETURN_UNDEF;
  }
  flag = SvIV(ST(0));
  switch(flag) {

    /* msginfo */
  case  1:
    if (msginfo->size) {
      XSRETURN_UV(msginfo->size);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  2:
    if (msginfo->date) {
      XSRETURN_PV(msginfo->date);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  3:
    if (msginfo->from) {
      XSRETURN_PV(msginfo->from);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  4:
    if (msginfo->to) {
      XSRETURN_PV(msginfo->to);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  5:
    if (msginfo->cc) {
      XSRETURN_PV(msginfo->cc);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  6:
    if (msginfo->newsgroups) {
      XSRETURN_PV(msginfo->newsgroups);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  7:
    if (msginfo->subject) {
      XSRETURN_PV(msginfo->subject);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  8:
    if (msginfo->msgid) {
      XSRETURN_PV(msginfo->msgid);
    }
    else {
      XSRETURN_UNDEF;
    }
  case  9:
    if (msginfo->inreplyto) {
      XSRETURN_PV(msginfo->inreplyto);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 10:
    if (msginfo->xref) {
      XSRETURN_PV(msginfo->xref);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 11:
    xface = procmsg_msginfo_get_avatar(msginfo, AVATAR_XFACE);
    if (xface) {
      XSRETURN_PV(xface);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 12:
    if (msginfo->extradata && msginfo->extradata->dispositionnotificationto) {
      XSRETURN_PV(msginfo->extradata->dispositionnotificationto);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 13:
    if (msginfo->extradata && msginfo->extradata->returnreceiptto) {
      XSRETURN_PV(msginfo->extradata->returnreceiptto);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 14:
    EXTEND(SP, g_slist_length(msginfo->references));
    ii = 0;
    for(walk = msginfo->references; walk != NULL; walk = g_slist_next(walk))
      XST_mPV(ii++,walk->data ? (gchar*) walk->data: "");
    if (ii) {
      XSRETURN(ii);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 15:
    if (msginfo->score) {
      XSRETURN_IV(msginfo->score);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 17:
    if (msginfo->plaintext_file) {
      XSRETURN_PV(msginfo->plaintext_file);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 19:
    if (msginfo->hidden) {
      XSRETURN_IV(msginfo->hidden);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 20:
    if((charp = procmsg_get_message_file_path(msginfo)) != NULL) {
      strncpy2(buf,charp,sizeof(buf));
      g_free(charp);
      XSRETURN_PV(buf);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 21:
    if (msginfo->extradata && msginfo->extradata->partial_recv)  {
      XSRETURN_PV(msginfo->extradata->partial_recv);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 22:
    if (msginfo->total_size) {
      XSRETURN_IV(msginfo->total_size);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 23:
    if (msginfo->extradata && msginfo->extradata->account_server) {
      XSRETURN_PV(msginfo->extradata->account_server);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 24:
    if (msginfo->extradata && msginfo->extradata->account_login) {
      XSRETURN_PV(msginfo->extradata->account_login);
    }
    else {
      XSRETURN_UNDEF;
    }
  case 25:
    if (msginfo->planned_download) {
      XSRETURN_IV(msginfo->planned_download);
    }
    else {
      XSRETURN_UNDEF;
    }

    /* general */
  case 100:
    if(manual_filtering) {
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  default:
    g_warning("Perl plugin: wrong argument to ClawsMail::C::init");
    XSRETURN_UNDEF;    
  }
}

/* ClawsMail::C::open_mail_file */
static XS(XS_ClawsMail_open_mail_file)
{
  char *file;

  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::open_mail_file");
    XSRETURN_UNDEF;
  }
  file = procmsg_get_message_file_path(msginfo);
  if(!file) {
    XSRETURN_UNDEF;
  }
  if((message_file = claws_fopen(file, "rb")) == NULL) {
    FILE_OP_ERROR(file, "claws_fopen");
    g_warning("Perl plugin: file open error in ClawsMail::C::open_mail_file");
    g_free(file);
    XSRETURN_UNDEF;
  }
  g_free(file);
}

/* ClawsMail::C::close_mail_file */
static XS(XS_ClawsMail_close_mail_file)
{
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::close_mail_file");
    XSRETURN_UNDEF;
  }
  if(message_file != NULL)
    claws_fclose(message_file);
  XSRETURN_YES;
}

/* ClawsMail::C::get_next_header */
static XS(XS_ClawsMail_get_next_header)
{
  gchar *buf;
  Header *header;

  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::get_next_header");
    XSRETURN_EMPTY;
  }
  if(message_file == NULL) {
    g_warning("Perl plugin: message file not open. Use ClawsMail::C::open_message_file first");
    XSRETURN_EMPTY;
  }
  if(procheader_get_one_field(&buf, message_file, NULL) != -1) {
    header = procheader_parse_header(buf);
    EXTEND(SP, 2);
    if(header) {
      XST_mPV(0,header->name);
      XST_mPV(1,header->body);
      procheader_header_free(header);
    }
    else {
      XST_mPV(0,"");
      XST_mPV(1,"");
    }
    g_free(buf);
    XSRETURN(2);
  }
  else {
    XSRETURN_EMPTY;
  }
}

/* ClawsMail::C::get_next_body_line */
static XS(XS_ClawsMail_get_next_body_line)
{
  gchar buf[BUFFSIZE];

  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::get_next_body_line");
    XSRETURN_UNDEF;
  }
  if(message_file == NULL) {
    g_warning("Perl plugin: message file not open. Use ClawsMail::C::open_message_file first");
    XSRETURN_UNDEF;
  }
  if(claws_fgets(buf, sizeof(buf), message_file) != NULL) {
    XSRETURN_PV(buf);
  }
  else {
    XSRETURN_UNDEF;
  }
}


/* Filter matchers */

/* ClawsMail::C::check_flag(int) */
static XS(XS_ClawsMail_check_flag)
{
  int flag;
  /* flags:  1 marked
   *         2 unread
   *         3 deleted
   *       4 new
   *       5 replied
   *       6 forwarded
   *       7 locked
   *         8 ignore thread
   */

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::check_flag");
    XSRETURN_UNDEF;
  }
  flag = SvIV(ST(0));

  switch(flag) {
  case 1:
    if(MSG_IS_MARKED(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"marked");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 2:
    if(MSG_IS_UNREAD(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"unread");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 3:
    if(MSG_IS_DELETED(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"deleted");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 4:
    if(MSG_IS_NEW(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"new");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 5:
    if(MSG_IS_REPLIED(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"replied");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 6:
    if(MSG_IS_FORWARDED(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"forwarded");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 7:
    if(MSG_IS_LOCKED(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"locked");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  case 8:
    if(MSG_IS_IGNORE_THREAD(msginfo->flags)) {
      filter_log_write(LOG_MATCH,"ignore_thread");
      XSRETURN_YES;
    }
    else {
      XSRETURN_NO;
    }
  default:
    g_warning("Perl plugin: unknown argument to ClawsMail::C::check_flag");
    XSRETURN_UNDEF;
  }
}

/* ClawsMail::C::colorlabel(int) */
static XS(XS_ClawsMail_colorlabel)
{
  int color;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::colorlabel");
    XSRETURN_UNDEF;
  }
  color = SvIV(ST(0));

  if((MSG_GET_COLORLABEL_VALUE(msginfo->flags) == (guint32)color)) {
    filter_log_write(LOG_MATCH,"colorlabel");
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}

/* ClawsMail::C::age_greater(int) */
static XS(XS_ClawsMail_age_greater)
{
  int age;
  time_t t;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::age_greater");
    XSRETURN_UNDEF;
  }
  age = SvIV(ST(0));
  t = time(NULL);
  if(((t - msginfo->date_t) / 86400) >= age) {
    filter_log_write(LOG_MATCH,"age_greater");
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}

/* ClawsMail::C::age_lower(int) */
static XS(XS_ClawsMail_age_lower)
{
  int age;
  time_t t;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::age_lower");
    XSRETURN_UNDEF;
  }
  age = SvIV(ST(0));
  t = time(NULL);
  if(((t - msginfo->date_t) / 86400) <= age) {
    filter_log_write(LOG_MATCH,"age_lower");
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}

/* ClawsMail::C::tagged() */
static XS(XS_ClawsMail_tagged)
{
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::tagged");
    XSRETURN_UNDEF;
  }

  if (msginfo->tags) {
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}

/* ClawsMail::C::get_tags() */
static XS(XS_ClawsMail_get_tags)
{
  guint iTag;
  guint num_tags;
  GSList *walk;

  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::get_tags");
    XSRETURN_UNDEF;
  }

  num_tags = g_slist_length(msginfo->tags);

  EXTEND(SP, num_tags);
  iTag = 0;
  for(walk = msginfo->tags; walk != NULL; walk = g_slist_next(walk)) {
    const char *tag_str;
    tag_str = tags_get_tag(GPOINTER_TO_INT(walk->data));
    XST_mPV(iTag++, tag_str ? tag_str: "");
  }

  XSRETURN(num_tags);
}



/* ClawsMail::C::set_tag(char*) */
static XS(XS_ClawsMail_set_tag)
{
  gchar *tag_str;
  gint tag_id;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::set_tag");
    XSRETURN_UNDEF;
  }

  tag_str = SvPV_nolen(ST(0));
  tag_id = tags_get_id_for_str(tag_str);
  if(tag_id == -1) {
    g_warning("Perl plugin: set_tag requested setting of a non-existing tag");
    XSRETURN_UNDEF;
  }

  procmsg_msginfo_update_tags(msginfo, TRUE, tag_id);

  XSRETURN_YES;
}

/* ClawsMail::C::unset_tag(char*) */
static XS(XS_ClawsMail_unset_tag)
{
  gchar *tag_str;
  gint tag_id;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::unset_tag");
    XSRETURN_UNDEF;
  }

  tag_str = SvPV_nolen(ST(0));
  tag_id = tags_get_id_for_str(tag_str);
  if(tag_id == -1) {
    g_warning("Perl plugin: unset_tag requested setting of a non-existing tag");
    XSRETURN_UNDEF;
  }

  procmsg_msginfo_update_tags(msginfo, FALSE, tag_id);

  XSRETURN_YES;
}

/* ClawsMail::C::clear_tags() */
static XS(XS_ClawsMail_clear_tags)
{
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::clear_tags");
    XSRETURN_UNDEF;
  }

  procmsg_msginfo_clear_tags(msginfo);
  XSRETURN_YES;
}


/* ClawsMail::C::make_sure_tag_exists(char*) */
static XS(XS_ClawsMail_make_sure_tag_exists)
{
  gchar *tag_str;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::make_sure_tag_exists");
    XSRETURN_UNDEF;
  }

  tag_str = SvPV_nolen(ST(0));

  if(IS_NOT_RESERVED_TAG(tag_str) == FALSE) {
    g_warning("Perl plugin: trying to create a tag with a reserved name: %s", tag_str);
    XSRETURN_UNDEF;
  }

  tags_add_tag(tag_str);

  XSRETURN_YES;
}



/* ClawsMail::C::make_sure_folder_exists(char*) */
static XS(XS_ClawsMail_make_sure_folder_exists)
{
  gchar *identifier;
  FolderItem *item;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::make_sure_folder_exists");
    XSRETURN_UNDEF;
  }

  identifier = SvPV_nolen(ST(0));
  item = folder_get_item_from_identifier(identifier);
  if(item) {
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}


/* ClawsMail::C::addr_in_addressbook(char* [, char*]) */
static XS(XS_ClawsMail_addr_in_addressbook)
{
  gchar *addr;
  gchar *bookname;
  gboolean found;

  dXSARGS;
  if(items != 1 && items != 2) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::addr_in_addressbook");
    XSRETURN_UNDEF;
  }

  addr = SvPV_nolen(ST(0));

  if(items == 1) {
    found = addr_in_addressbook(addr,NULL);
  }
  else {
    bookname = SvPV_nolen(ST(1));
    found = addr_in_addressbook(addr,bookname);
  }

  if(found) {
    filter_log_write(LOG_MATCH,"addr_in_addressbook");
    XSRETURN_YES;
  }
  else {
    XSRETURN_NO;
  }
}


/* Filter actions */

/* ClawsMail::C::set_flag(int) */
static XS(XS_ClawsMail_set_flag)
{
  int flag;
  /* flags:  1 mark
   *         2 mark as unread
   *         7 lock
   */

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::set_flag");
    XSRETURN_UNDEF;
  }
  flag = SvIV(ST(0));

  switch(flag) {
  case 1:
    MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
    procmsg_msginfo_set_flags(msginfo, MSG_MARKED,0);
    filter_log_write(LOG_ACTION,"mark");
    XSRETURN_YES;
  case 2:
    MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
    procmsg_msginfo_set_flags(msginfo, MSG_UNREAD,0);
    filter_log_write(LOG_ACTION,"mark_as_unread");
    XSRETURN_YES;
  case 7:
    MSG_SET_PERM_FLAGS(msginfo->flags, MSG_LOCKED);
    procmsg_msginfo_set_flags(msginfo, MSG_LOCKED,0);
    filter_log_write(LOG_ACTION,"lock");
    XSRETURN_YES;
  default:
    g_warning("Perl plugin: unknown argument to ClawsMail::C::set_flag");
    XSRETURN_UNDEF;
  }
}

/* ClawsMail::C::unset_flag(int) */
static XS(XS_ClawsMail_unset_flag)
{
  int flag;
  /*
   * flags:  1 unmark
   *         2 mark as read
   *         7 unlock
   */

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::unset_flag");
    XSRETURN_UNDEF;
  }
  flag = SvIV(ST(0));

  switch(flag) {
  case 1:
    MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
    procmsg_msginfo_unset_flags(msginfo, MSG_MARKED,0);
    filter_log_write(LOG_ACTION,"unmark");
    XSRETURN_YES;
  case 2:
    MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_UNREAD | MSG_NEW);
    procmsg_msginfo_unset_flags(msginfo, MSG_UNREAD | MSG_NEW,0);
    filter_log_write(LOG_ACTION,"mark_as_read");
    XSRETURN_YES;
  case 7:
    MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_LOCKED);
    procmsg_msginfo_unset_flags(msginfo, MSG_LOCKED,0);
    filter_log_write(LOG_ACTION,"unlock");
    XSRETURN_YES;
  default:
    g_warning("Perl plugin: unknown argument to ClawsMail::C::unset_flag");
    XSRETURN_UNDEF;
  }
}

/* ClawsMail::C::move(char*) */
static XS(XS_ClawsMail_move)
{
  gchar *targetfolder;
  gchar *logtext;
  FolderItem *dest_folder;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::move");
    XSRETURN_UNDEF;
  }

  targetfolder = SvPV_nolen(ST(0));
  dest_folder = folder_find_item_from_identifier(targetfolder);

  if (!dest_folder) {
    g_warning("Perl plugin: move: folder not found '%s'",
      targetfolder ? targetfolder :"");
    XSRETURN_UNDEF;
  }
  if (folder_item_move_msg(dest_folder, msginfo) == -1) {
    g_warning("Perl plugin: move: could not move message");
    XSRETURN_UNDEF;
  }
  stop_filtering = TRUE;
  logtext = g_strconcat("move to ", targetfolder, NULL);
  filter_log_write(LOG_ACTION, logtext);
  g_free(logtext);
  XSRETURN_YES;
}

/* ClawsMail::C::copy(char*) */
static XS(XS_ClawsMail_copy)
{
  char *targetfolder;
  gchar *logtext;
  FolderItem *dest_folder;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::copy");
    XSRETURN_UNDEF;
  }
  targetfolder = SvPV_nolen(ST(0));
  dest_folder = folder_find_item_from_identifier(targetfolder);

  if (!dest_folder) {
    g_warning("Perl plugin: copy: folder not found '%s'",
      targetfolder ? targetfolder :"");
    XSRETURN_UNDEF;
  }
  if (folder_item_copy_msg(dest_folder, msginfo) == -1) {
    g_warning("Perl plugin: copy: could not copy message");
    XSRETURN_UNDEF;
  }
  logtext = g_strconcat("copy to ", targetfolder, NULL);
  filter_log_write(LOG_ACTION, logtext);
  g_free(logtext);
  XSRETURN_YES;
}

/* ClawsMail::C::delete */
static XS(XS_ClawsMail_delete)
{
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::delete");
    XSRETURN_UNDEF;
  }
  folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
  stop_filtering = TRUE;
  filter_log_write(LOG_ACTION, "delete");
  XSRETURN_YES;
}

/* ClawsMail::C::hide */
static XS(XS_ClawsMail_hide)
{
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::hide");
    XSRETURN_UNDEF;
  }
  msginfo->hidden = TRUE;
  filter_log_write(LOG_ACTION, "hide");
  XSRETURN_YES;
}


/* ClawsMail::C::color(int) */
static XS(XS_ClawsMail_color)
{
  int color;
  gchar *logtext;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::color");
    XSRETURN_UNDEF;
  }
  color = SvIV(ST(0));
  procmsg_msginfo_unset_flags(msginfo, MSG_CLABEL_FLAG_MASK, 0); 
  procmsg_msginfo_set_flags(msginfo, MSG_COLORLABEL_TO_FLAGS(color), 0);
  MSG_SET_COLORLABEL_VALUE(msginfo->flags,color);

  logtext = g_strdup_printf("color: %d", color);
  filter_log_write(LOG_ACTION, logtext);
  g_free(logtext);

  XSRETURN_YES;
}

/* ClawsMail::C::change_score(int) */
static XS(XS_ClawsMail_change_score)
{
  int score;
  gchar *logtext;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::change_score");
    XSRETURN_UNDEF;
  }
  score = SvIV(ST(0));
  msginfo->score += score;

  logtext = g_strdup_printf("change score: %+d", score);
  filter_log_write(LOG_ACTION, logtext);
  g_free(logtext);

  XSRETURN_IV(msginfo->score);
}

/* ClawsMail::C::set_score(int) */
static XS(XS_ClawsMail_set_score)
{
  int score;
  gchar *logtext;

  dXSARGS;
  if(items != 1) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::set_score");
    XSRETURN_UNDEF;
  }
  score = SvIV(ST(0));
  msginfo->score = score;

  logtext = g_strdup_printf("set score: %d", score);
  filter_log_write(LOG_ACTION, logtext);
  g_free(logtext);

  XSRETURN_IV(msginfo->score);
}

/* ClawsMail::C::forward(int,int,char*) */
static XS(XS_ClawsMail_forward)
{
  int flag;
  /* flags:  1 forward
   *         2 forward as attachment
   */
  int account_id,val;
  char *dest;
  gchar *logtext;
  PrefsAccount *account;
  Compose *compose;

  dXSARGS;
  if(items != 3) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::forward");
    XSRETURN_UNDEF;
  }

  flag = SvIV(ST(0));
  account_id = SvIV(ST(1));
  dest = SvPV_nolen(ST(2));

  account = account_find_from_id(account_id);
  compose = compose_forward(account, msginfo,
          flag == 1 ? FALSE : TRUE,
          NULL, TRUE, TRUE);
  compose_entry_append(compose, dest,
           compose->account->protocol == A_NNTP ?
           COMPOSE_NEWSGROUPS : COMPOSE_TO, PREF_NONE);

  val = compose_send(compose);

  if(val == 0) {

    logtext = g_strdup_printf("forward%s to %s",
            flag==2 ? " as attachment" : "",
            dest    ? dest : "<unknown destination>");
    filter_log_write(LOG_ACTION, logtext);
    g_free(logtext);

    XSRETURN_YES;
  }
  else {
    XSRETURN_UNDEF;
  }
}

/* ClawsMail::C::redirect(int,char*) */
static XS(XS_ClawsMail_redirect)
{
  int account_id,val;
  char *dest;
  gchar *logtext;
  PrefsAccount *account;
  Compose *compose;

  dXSARGS;
  if(items != 2) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::redirect");
    XSRETURN_UNDEF;
  }

  account_id = SvIV(ST(0));
  dest = SvPV_nolen(ST(1));

  account = account_find_from_id(account_id);
  compose = compose_redirect(account, msginfo, TRUE);
  
  if (compose->account->protocol == A_NNTP) {
    XSRETURN_UNDEF;
  }
  else
    compose_entry_append(compose, dest, COMPOSE_TO, PREF_NONE);

  val = compose_send(compose);
  
  if(val == 0) {
    
    logtext = g_strdup_printf("redirect to %s",
            dest ? dest : "<unknown destination>");
    filter_log_write(LOG_ACTION, logtext);
    g_free(logtext);

    XSRETURN_YES;
  }
  else {
    XSRETURN_UNDEF;
  }
}


/* Utilities */

/* ClawsMail::C::move_to_trash */
static XS(XS_ClawsMail_move_to_trash)
{
  FolderItem *dest_folder;
  
  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::move_to_trash");
    XSRETURN_UNDEF;
  }
  dest_folder = folder_get_default_trash();
  if (!dest_folder) {
    g_warning("Perl plugin: move_to_trash: Trash folder not found");
    XSRETURN_UNDEF;
  }
  if (folder_item_move_msg(dest_folder, msginfo) == -1) {
    g_warning("Perl plugin: move_to_trash: could not move message to trash");
    XSRETURN_UNDEF;
  }
  stop_filtering = TRUE;
  filter_log_write(LOG_ACTION, "move_to_trash");
  XSRETURN_YES;
}

/* ClawsMail::C::abort */
static XS(XS_ClawsMail_abort)
{
  FolderItem *inbox_folder;

  dXSARGS;
  if(items != 0) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::abort");
    XSRETURN_UNDEF;
  }
  if(!manual_filtering) {
    inbox_folder = folder_get_default_inbox();
    if (!inbox_folder) {
      g_warning("Perl plugin: abort: inbox folder not found");
      XSRETURN_UNDEF;
    }
    if (folder_item_move_msg(inbox_folder, msginfo) == -1) {
      g_warning("Perl plugin: abort: could not move message to default inbox");
      XSRETURN_UNDEF;
    }
    filter_log_write(LOG_ACTION, "abort -- message moved to default inbox");
  }
  else
    filter_log_write(LOG_ACTION, "abort");

  stop_filtering = TRUE;
  XSRETURN_YES;
}

/* ClawsMail::C::get_attribute_value(char*,char*[,char*]) */
static XS(XS_ClawsMail_get_attribute_value)
{
  char *addr;
  char *attr;
  char *attribute_value;
  char *bookname;

  dXSARGS;
  if(items != 2 && items != 3) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::get_attribute_value");
    XSRETURN_UNDEF;
  }
  addr = SvPV_nolen(ST(0));
  attr = SvPV_nolen(ST(1));

  if(items == 2)
    attribute_value = get_attribute_value(addr,attr,NULL);
  else {
    bookname = SvPV_nolen(ST(2));
    attribute_value = get_attribute_value(addr,attr,bookname);
  }

  if(attribute_value) {
    XSRETURN_PV(attribute_value);
  }
  XSRETURN_PV("");
}

/* ClawsMail::C::filter_log(char*,char*) */
static XS(XS_ClawsMail_filter_log)
{
  char *text;
  char *type;
  
  dXSARGS;
  if(items != 2) {
    g_warning("Perl plugin: wrong number of arguments to ClawsMail::C::filter_log");
    XSRETURN_UNDEF;
  }
  type = SvPV_nolen(ST(0));
  text = SvPV_nolen(ST(1));
  if(!strcmp(type, "LOG_ACTION"))
    filter_log_write(LOG_ACTION, text);
  else if(!strcmp(type, "LOG_MANUAL"))
    filter_log_write(LOG_MANUAL, text);
  else if(!strcmp(type, "LOG_MATCH"))
    filter_log_write(LOG_MATCH, text);
  else {
    g_warning("Perl plugin: ClawsMail::C::filter_log -- wrong first argument");
    XSRETURN_UNDEF;
  }  
  XSRETURN_YES;
}

/* ClawsMail::C::filter_log_verbosity(int) */
static XS(XS_ClawsMail_filter_log_verbosity)
{
  int retval;

  dXSARGS;
  if(items != 1 && items != 0) {
    g_warning("Perl plugin: wrong number of arguments to "
    "ClawsMail::C::filter_log_verbosity");
    XSRETURN_UNDEF;
  }
  retval = filter_log_verbosity;

  if(items == 1)
    filter_log_verbosity = SvIV(ST(0));

  XSRETURN_IV(retval);
}

/* register extensions */ 
EXTERN_C void xs_init(pTHX)
{
  char *file = __FILE__;
  dXSUB_SYS;
  newXS("DynaLoader::boot_DynaLoader",    boot_DynaLoader,               file);
  newXS("ClawsMail::C::filter_init",  XS_ClawsMail_filter_init,  "ClawsMail::C");
  newXS("ClawsMail::C::check_flag",   XS_ClawsMail_check_flag,   "ClawsMail::C");
  newXS("ClawsMail::C::age_greater",  XS_ClawsMail_age_greater,  "ClawsMail::C");
  newXS("ClawsMail::C::age_lower",    XS_ClawsMail_age_lower,    "ClawsMail::C");
  newXS("ClawsMail::C::tagged",       XS_ClawsMail_tagged,       "ClawsMail::C");
  newXS("ClawsMail::C::set_flag",     XS_ClawsMail_set_flag,     "ClawsMail::C");
  newXS("ClawsMail::C::unset_flag",   XS_ClawsMail_unset_flag,   "ClawsMail::C");
  newXS("ClawsMail::C::delete",       XS_ClawsMail_delete,       "ClawsMail::C");
  newXS("ClawsMail::C::move",         XS_ClawsMail_move,         "ClawsMail::C");
  newXS("ClawsMail::C::copy",         XS_ClawsMail_copy,         "ClawsMail::C");
  newXS("ClawsMail::C::color",        XS_ClawsMail_color,        "ClawsMail::C");
  newXS("ClawsMail::C::colorlabel",   XS_ClawsMail_colorlabel,   "ClawsMail::C");
  newXS("ClawsMail::C::change_score", XS_ClawsMail_change_score, "ClawsMail::C");
  newXS("ClawsMail::C::set_score",    XS_ClawsMail_set_score,    "ClawsMail::C");
  newXS("ClawsMail::C::hide",         XS_ClawsMail_hide,         "ClawsMail::C");
  newXS("ClawsMail::C::forward",      XS_ClawsMail_forward,      "ClawsMail::C");
  newXS("ClawsMail::C::redirect",     XS_ClawsMail_redirect,     "ClawsMail::C");
  newXS("ClawsMail::C::set_tag",      XS_ClawsMail_set_tag,      "ClawsMail::C");
  newXS("ClawsMail::C::unset_tag",    XS_ClawsMail_unset_tag,    "ClawsMail::C");
  newXS("ClawsMail::C::clear_tags",   XS_ClawsMail_clear_tags,   "ClawsMail::C");
  newXS("ClawsMail::C::make_sure_folder_exists",
  XS_ClawsMail_make_sure_folder_exists,"ClawsMail::C");
  newXS("ClawsMail::C::make_sure_tag_exists", XS_ClawsMail_make_sure_tag_exists,"ClawsMail::C");
  newXS("ClawsMail::C::get_tags", XS_ClawsMail_get_tags,"ClawsMail::C");
  newXS("ClawsMail::C::addr_in_addressbook",
  XS_ClawsMail_addr_in_addressbook,"ClawsMail::C");
  newXS("ClawsMail::C::open_mail_file",
  XS_ClawsMail_open_mail_file,"ClawsMail::C");
  newXS("ClawsMail::C::close_mail_file",
  XS_ClawsMail_close_mail_file,"ClawsMail::C");
  newXS("ClawsMail::C::get_next_header",
  XS_ClawsMail_get_next_header,"ClawsMail::C");
  newXS("ClawsMail::C::get_next_body_line",
  XS_ClawsMail_get_next_body_line,"ClawsMail::C");
  newXS("ClawsMail::C::move_to_trash",XS_ClawsMail_move_to_trash,"ClawsMail::C");
  newXS("ClawsMail::C::abort",        XS_ClawsMail_abort,        "ClawsMail::C");
  newXS("ClawsMail::C::get_attribute_value",
  XS_ClawsMail_get_attribute_value,"ClawsMail::C");
  newXS("ClawsMail::C::filter_log",   XS_ClawsMail_filter_log,   "ClawsMail::C");
  newXS("ClawsMail::C::filter_log_verbosity",
  XS_ClawsMail_filter_log_verbosity, "ClawsMail::C");
}

/*
 * The workhorse.
 * Returns: 0 on success
 *          1 error in scriptfile or invocation of external
 *            editor              -> retry
 *          2 error in scriptfile -> abort
 * (Yes, I know..)
 */
static int perl_load_file(void)
{
  gchar *args[] = {"", DO_CLEAN, NULL};
  gchar *noargs[] = { NULL };
  gchar *perlfilter;
  gchar **cmdline;
  gchar buf[1024];
  gchar *pp;
  STRLEN n_a;

  call_argv("ClawsMail::Filter::Matcher::filter_init_",
      G_DISCARD | G_EVAL | G_NOARGS,noargs);
  /* check $@ */
  if(SvTRUE(ERRSV)) {
    debug_print("%s", SvPV(ERRSV,n_a));
    return 1; 
  }
  perlfilter = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PERLFILTER, NULL);
  args[0] = perlfilter;
  call_argv("ClawsMail::Persistent::eval_file",
      G_DISCARD | G_EVAL, args);
  g_free(perlfilter);
  if(SvTRUE(ERRSV)) {
    AlertValue val;
    gchar *message;

    if(strstr(SvPV(ERRSV,n_a),"intended"))
      return 0;

    debug_print("%s", SvPV(ERRSV,n_a));
    message = g_strdup_printf(_("Error processing Perl script file: "
            "(line numbers may not be valid)\n%s"),
            SvPV(ERRSV,n_a));
    val = alertpanel(_("Perl Plugin error"), message, NULL, _("Retry"), NULL,
		     _("Abort"), NULL, _("Edit"), ALERTFOCUS_FIRST);
    g_free(message);

    if(val == G_ALERTOTHER) {
      /* Open PERLFILTER in an external editor */
      perlfilter = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PERLFILTER, NULL);
      if (prefs_common_get_ext_editor_cmd() &&
    (pp = strchr(prefs_common_get_ext_editor_cmd(), '%')) &&
    *(pp + 1) == 's' && !strchr(pp + 2, '%')) {
  g_snprintf(buf, sizeof(buf), prefs_common_get_ext_editor_cmd(), perlfilter);
      }
      else {
  if (prefs_common_get_ext_editor_cmd())
    g_warning("Perl plugin: External editor command-line is invalid: `%s'",
        prefs_common_get_ext_editor_cmd());
  g_snprintf(buf, sizeof(buf), "emacs %s", perlfilter);
      }
      g_free(perlfilter);
      cmdline = strsplit_with_quote(buf, " ", 1024);
      execute_detached(cmdline);
      g_strfreev(cmdline);
      return 1;
    }
    else if(val == G_ALERTDEFAULT)
      return 1;
    else
      return 2;
  }

  return 0;
}


/* let there be magic */
static int perl_init(void)
{
  int exitstatus;
  char *initialize[] = { "", "-w", "-e", "1;"};
  /* The `persistent' module is taken from the Perl documentation
     and has only slightly been modified. */
  const char perl_persistent[] = {
"package ClawsMail::Persistent;\n"
"\n"
"use strict;\n"
"our %Cache;\n"
"use Symbol qw(delete_package);\n"
"\n"
"sub valid_package_name {\n"
"    my($string) = @_;\n"
"    $string =~ s/([^A-Za-z0-9\\/])/sprintf(\"_%2x\",unpack(\"C\",$1))/eg;\n"
"    # second pass only for words starting with a digit\n"
"    $string =~ s|/(\\d)|sprintf(\"/_%2x\",unpack(\"C\",$1))|eg;\n"
"    \n"
"    # Dress it up as a real package name\n"
"    $string =~ s|/|::|g;\n"
"    return \"ClawsMail\" . $string;\n"
"}\n"
"\n"
"sub eval_file {\n"
"    my($file, $delete) = @_;\n"
"    my $package = valid_package_name($file);\n"
"    my $mtime = -M $file;\n"
"    if(!(defined $Cache{$package}{mtime} &&\n"
"   $Cache{$package}{mtime} <= $mtime)) {\n"
"      delete_package($package) if defined $Cache{$package}{mtime};\n"
"  local *FH;\n"
"  open FH, $file or die \"Failed to open '$file': $!\";\n"
"  local($/) = undef;\n"
"  my $sub = <FH>;\n"
"  close FH;\n"
"  #wrap the code into a subroutine inside our unique package\n"
"  my $eval = qq{package $package;\n"
"          use ClawsMail::Filter::Matcher;\n"
"          use ClawsMail::Filter::Action;\n"
"          use ClawsMail::Utils;\n"
"          sub handler { $sub; }};\n"
"  {\n"
"      # hide our variables within this block\n"
"      my($file,$mtime,$package,$sub);\n"
"      eval $eval;\n"
"  }\n"
"  die $@ if $@;\n"
"  #cache it unless we're cleaning out each time\n"
"  $Cache{$package}{mtime} = $mtime unless $delete;\n"
"    }\n"
"    eval {$package->handler;};\n"
"    die $@ if $@;\n"
"    delete_package($package) if $delete;\n"
"}\n"
  };
  const char perl_filter_matcher[] = {
"BEGIN {$INC{'ClawsMail/Filter/Matcher.pm'} = 1;}\n"
"package ClawsMail::Filter::Matcher;\n"
"use locale;\n"
"use base qw(Exporter);\n"
"use strict;\n"
"our @EXPORT =   (qw(header body filepath manual),\n"
"     qw(filter_log_verbosity filter_log),\n"
"     qw(all marked unread deleted new replied),\n"
"     qw(forwarded locked colorlabel match matchcase),\n"
"     qw(regexp regexpcase test),\n"
"     qw(to cc subject from to_or_cc newsgroups inreplyto),\n"
"     qw(references body_part headers_part headers_cont message),\n"
"     qw(size_greater size_smaller size_equal),\n"
"     qw(score_greater score_lower score_equal),\n"
"     qw(age_greater age_lower partial tagged $permanent));\n"
"# Global Variables\n"
"our(%header,$body,%msginfo,$mail_done,$manual);\n"
"our %colors = ('none'     =>  0,'orange'   =>  1,'red'  =>  2,\n"
"            'pink'     =>  3,'sky blue' =>  4,'blue' =>  5,\n"
"             'green'    =>  6,'brown'    =>  7);\n"
"# For convenience\n"
"sub lc2_ {\n"
"    my $arg = shift;\n"
"    if(defined $arg) {\n"
"        return lc $arg;\n"
"    }\n"
"    else {\n"
"        return \"\";\n"
"    }\n"
"}\n"
"sub to           { return \"to\";            }\n"
"sub cc           { return \"cc\";            }\n"
"sub from         { return \"from\";          }\n"
"sub subject      { return \"subject\";       }\n"
"sub to_or_cc     { return \"to_or_cc\";      }\n"
"sub newsgroups   { return \"newsgroups\";    }\n"
"sub inreplyto    { return \"in-reply-to\";   }\n"
"sub references   { return \"references\";    }\n"
"sub body_part    { return \"body_part\";     }\n"
"sub headers_part { return \"headers_part\";  }\n"
"sub headers_cont { return \"headers_cont\";  }\n"
"sub message      { return \"message\";       }\n"
"# access the mail directly\n"
"sub header {\n"
"    my $key = shift;\n"
"    if(not defined $key) {\n"
"  init_();\n"
"  return keys %header;\n"
"    }\n"
"    $key = lc2_ $key; $key =~ s/:$//;\n"
"    init_() unless exists $header{$key};\n"
"    if(exists $header{$key}) {\n"
"  wantarray ? return @{$header{$key}} : return $header{$key}->[-1];\n"
"    }\n"
"    return undef;\n"
"}\n"
"sub body {init_();return $body;}\n"
"sub filepath {return $msginfo{\"filepath\"};}\n"
"sub manual {\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"manual\") if $manual;\n"
"    return $manual;\n"
"}\n"
"sub filter_log {\n"
"    my $arg1 = shift;\n"
"    my $arg2 = shift;\n"
"    return ClawsMail::C::filter_log($arg1,$arg2)\n"
"  if defined($arg2);\n"
"    return ClawsMail::C::filter_log(\"LOG_MANUAL\",$arg1);\n"
"}\n"
"sub filter_log_verbosity {\n"
"    $_ = shift;\n"
"    return ClawsMail::C::filter_log_verbosity($_)\n"
"  if defined($_);\n"
"    return ClawsMail::C::filter_log_verbosity();\n"
"}\n"
"# Public Matcher Tests\n"
"sub all { ClawsMail::C::filter_log(\"LOG_MATCH\",\"all\");return 1; }\n"
"sub marked        { return ClawsMail::C::check_flag(1);}\n"
"sub unread        { return ClawsMail::C::check_flag(2);}\n"
"sub deleted       { return ClawsMail::C::check_flag(3);}\n"
"sub new           { return ClawsMail::C::check_flag(4);}\n"
"sub replied       { return ClawsMail::C::check_flag(5);}\n"
"sub forwarded     { return ClawsMail::C::check_flag(6);}\n"
"sub locked        { return ClawsMail::C::check_flag(7);}\n"
"sub ignore_thread { return ClawsMail::C::check_flag(8);}\n"
"sub age_greater  {return ClawsMail::C::age_greater(@_);}\n"
"sub age_lower    {return ClawsMail::C::age_lower(@_);  }\n"
"sub tagged       {return ClawsMail::C::tagged(@_);  }\n"
"sub score_equal {\n"
"    my $my_score = shift;\n"
"    return 0 unless (defined($msginfo{\"score\"}) and defined($my_score));\n"
"    if($my_score == $msginfo{\"score\"}) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"score_equal\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub score_greater {\n"
"    my $my_score = shift;\n"
"    return 0 unless (defined($msginfo{\"score\"}) and defined($my_score));\n"
"    if($msginfo{\"score\"} > $my_score) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"score_greater\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub score_lower {\n"
"    my $my_score = shift;\n"
"    return 0 unless (defined($msginfo{\"score\"}) and defined($my_score));\n"
"    if($msginfo{\"score\"} < $my_score) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"score_lower\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub colorlabel {\n"
"    my $color = shift;\n"
"    $color = lc2_ $color;\n"
"    $color = $colors{$color} if exists $colors{$color};\n"
"    $color = 0 if $color =~ m/\\D/;\n"
"    return ClawsMail::C::colorlabel($color);\n"
"}\n"
"sub size_greater {\n"
"    my $my_size = shift;\n"
"    return 0 unless (defined($msginfo{\"size\"}) and defined($my_size));\n"
"    if($msginfo{\"size\"} > $my_size) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"size_greater\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub size_smaller {\n"
"    my $my_size = shift;\n"
"    return 0 unless (defined($msginfo{\"size\"}) and defined($my_size));\n"
"    if($msginfo{\"size\"} < $my_size) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"size_smaller\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub size_equal {\n"
"    my $my_size = shift;\n"
"    return 0 unless (defined($msginfo{\"size\"}) and defined($my_size));\n"
"    if($msginfo{\"size\"} == $my_size) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"size_equal\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub partial {\n"
"    return 0 unless defined($msginfo{\"total_size\"})\n"
"  and defined($msginfo{\"size\"});\n"
"    if($msginfo{\"total_size\"} != 0\n"
"       && $msginfo{\"size\"} != $msginfo{\"total_size\"}) {\n"
"  ClawsMail::C::filter_log(\"LOG_MATCH\",\"partial\");\n"
"  return 1;\n"
"    }else{return 0;}\n"
"}\n"
"sub test {\n"
"   $_ = shift; my $command = \"\"; my $hl=\"\"; my $re=\"\"; my $retval;\n"
"   my $cmdline = $_;\n"
"   s/\\\"/\"/g; #fool stupid emacs perl mode\";\n"
"   s/([^%]*)//; $command .= $1;\n"
"   while($_) {\n"
"       if   (/^%%/){s/^%%([^%]*)//;$command .= \"\\\\%\".$1; next;}\n"
"       elsif(/^%s/){s/^%s([^%]*)//;$hl=header(\"subject\");$re=$1;}\n"
"       elsif(/^%f/){s/^%f([^%]*)//;$hl=header(\"from\");$re=$1;}\n"
"       elsif(/^%t/){s/^%t([^%]*)//;$hl=header(\"to\");$re=$1;}\n"
"       elsif(/^%c/){s/^%c([^%]*)//;$hl=header(\"cc\");$re=$1;}\n"
"       elsif(/^%d/){s/^%d([^%]*)//;$hl=header(\"date\");$re=$1;}\n"
"       elsif(/^%i/){s/^%i([^%]*)//;$hl=header(\"message-id\");$re=$1;}\n"
"       elsif(/^%n/){s/^%n([^%]*)//;$hl=header(\"newsgroups\");$re=$1;}\n"
"       elsif(/^%r/){s/^%r([^%]*)//;$hl=header(\"references\");$re=$1;}\n"
"       elsif(/^%F/){s/^%F([^%]*)//;$hl=filepath();$re=$1;}\n"
"       else        {s/^(%[^%]*)//; $command .= $1;}\n"
"       $command .= \"\\Q$hl\\E\" if defined $hl;$hl=\"\";\n"
"       $command .= $re;$re=\"\";\n"
"   }\n"
"   $retval = !(system($command)>>8);\n"
"   ClawsMail::C::filter_log(\"LOG_MATCH\",\"test: $cmdline\")\n"
"       if $retval;\n"
"   return $retval;\n"
"}\n"
"sub matchcase {\n"
"    my $retval;\n"
"    $retval = match_(@_,\"i\");\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"matchcase: $_[0], $_[1]\")\n"
"  if $retval;\n"
"    return $retval;\n"
"}\n"
"sub match {\n"
"    my $retval;\n"
"    $retval = match_(@_);\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"match: $_[0], $_[1]\")\n"
"  if $retval;\n"
"    return $retval;\n"
"}\n"
"sub regexpcase {\n"
"    my $retval;\n"
"    $retval = match_(@_,\"ri\");\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"regexpcase: $_[0], $_[1]\")\n"
"  if $retval;\n"
"    return $retval;\n"
"}\n"
"sub regexp {\n"
"    my $retval;\n"
"    $retval = match_(@_,\"r\");\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"regexp: $_[0], $_[1]\")\n"
"  if $retval;\n"
"    return $retval;\n"
"}\n"
"# Internals\n"
"sub add_header_entries_ {\n"
"    my($key,@values) = @_; $key = lc2_ $key; $key =~ s/:$//;\n"
"    $header{$key} = [] unless exists $header{$key};\n"
"    push @{$header{$key}},@values;\n"
"}\n"
"# read whole mail\n"
"sub init_ {\n"
"    return 0 if $mail_done;\n"
"    ClawsMail::C::open_mail_file();\n"
"    read_headers_();\n"
"    read_body_();\n"
"    ClawsMail::C::close_mail_file();\n"
"    $mail_done = 1;\n"
"}\n"
"sub filter_init_ {\n"
"    %header = (); %msginfo = (); undef $body; $mail_done = 0;\n"
"    $manual                        = ClawsMail::C::filter_init(100);\n"
"    $msginfo{\"size\"}               = ClawsMail::C::filter_init( 1) ;\n"
"    add_header_entries_(\"date\",      ClawsMail::C::filter_init( 2));\n"
"    add_header_entries_(\"from\",      ClawsMail::C::filter_init( 3));\n"
"    add_header_entries_(\"to\",        ClawsMail::C::filter_init( 4));\n"
"    add_header_entries_(\"cc\",        ClawsMail::C::filter_init( 5));\n"
"    add_header_entries_(\"newsgroups\",ClawsMail::C::filter_init( 6));\n"
"    add_header_entries_(\"subject\",   ClawsMail::C::filter_init( 7));\n"
"    add_header_entries_(\"msgid\",     ClawsMail::C::filter_init( 8));\n"
"    add_header_entries_(\"inreplyto\", ClawsMail::C::filter_init( 9));\n"
"    add_header_entries_(\"xref\",      ClawsMail::C::filter_init(10));\n"
"    add_header_entries_(\"xface\",     ClawsMail::C::filter_init(11));\n"
"    add_header_entries_(\"dispositionnotificationto\",\n"
"                   ClawsMail::C::filter_init(12));\n"
"    add_header_entries_(\"returnreceiptto\",\n"
"                   ClawsMail::C::filter_init(13));\n"
"    add_header_entries_(\"references\",ClawsMail::C::filter_init(14));\n"
"    $msginfo{\"score\"}              = ClawsMail::C::filter_init(15);\n"
"    $msginfo{\"plaintext_file\"}     = ClawsMail::C::filter_init(17);\n"
"    $msginfo{\"hidden\"}             = ClawsMail::C::filter_init(19);\n"
"    $msginfo{\"filepath\"}           = ClawsMail::C::filter_init(20);\n"
"    $msginfo{\"partial_recv\"}       = ClawsMail::C::filter_init(21);\n"
"    $msginfo{\"total_size\"}         = ClawsMail::C::filter_init(22);\n"
"    $msginfo{\"account_server\"}     = ClawsMail::C::filter_init(23);\n"
"    $msginfo{\"account_login\"}      = ClawsMail::C::filter_init(24);\n"
"    $msginfo{\"planned_download\"}   = ClawsMail::C::filter_init(25);\n"
"} \n"
"sub read_headers_ {\n"
"    my($key,$value);\n"
"    %header = ();\n"
"    while(($key,$value) = ClawsMail::C::get_next_header()) {\n"
"  next unless $key =~ /:$/;\n"
"  add_header_entries_($key,$value);\n"
"    }\n"
"}\n"
"sub read_body_ {\n"
"    my $line;\n"
"    while(defined($line = ClawsMail::C::get_next_body_line())) {\n"
"  $body .= $line;\n"
"    }    \n"
"}\n"
"sub match_ {\n"
"  my ($where,$what,$modi) = @_; $modi ||= \"\";\n"
"  my $nocase=\"\"; $nocase = \"1\" if (index($modi,\"i\") != -1);\n"
"  my $regexp=\"\"; $regexp = \"1\" if (index($modi,\"r\") != -1);\n"
"  if($where eq \"to_or_cc\") {\n"
"    if(not $regexp) { \n"
"      return ((index(header(\"to\"),$what) != -1) or\n"
"        (index(header(\"cc\"),$what) != -1)) unless $nocase;\n"
"      return ((index(lc2_(header(\"to\")),lc2_($what)) != -1) or\n"
"        (index(lc2_(header(\"cc\")),lc2_($what)) != -1))\n"
"    } else {\n"
"      return ((header(\"to\") =~ m/$what/) or\n"
"        (header(\"cc\") =~ m/$what/)) unless $nocase;\n"
"      return ((header(\"to\") =~ m/$what/i) or\n"
"        (header(\"cc\") =~ m/$what/i));\n"
"    }\n"
"  } elsif($where eq \"body_part\") {\n"
"    my $mybody = body(); $mybody =~ s/\\s+/ /g;\n"
"    if(not $regexp) {\n"
"      return (index($mybody,$what) != -1) unless $nocase;\n"
"      return (index(lc2_($mybody),lc2_($what)) != -1);\n"
"    } else {\n"
"      return ($body =~ m/$what/) unless $nocase;\n"
"      return ($body =~ m/$what/i);\n"
"    }\n"
"  } elsif($where eq \"headers_part\") {\n"
"    my $myheader = header_as_string_();\n"
"    if(not $regexp) {\n"
"      $myheader =~ s/\\s+/ /g;\n"
"      return (index($myheader,$what) != -1) unless $nocase;\n"
"      return (index(lc2_($myheader),lc2_($what)) != -1);\n"
"    } else {\n"
"      return ($myheader =~ m/$what/) unless $nocase;\n"
"      return ($myheader =~ m/$what/i);\n"
"    }\n"
"  } elsif($where eq \"headers_cont\") {\n"
"    (my $myheader = header_as_string_()) =~ s{^\\S+:\\s*}{};\n"
"    if(not $regexp) {\n"
"      $myheader =~ s/\\s+/ /g;\n"
"      return (index($myheader,$what) != -1) unless $nocase;\n"
"      return (index(lc2_($myheader),lc2_($what)) != -1);\n"
"    } else {\n"
"      return ($myheader =~ m/$what/) unless $nocase;\n"
"      return ($myheader =~ m/$what/i);\n"
"    }\n"
"  } elsif($where eq \"message\") {\n"
"    my $message = header_as_string_();\n"
"    $message .= \"\\n\".body();\n"
"    if(not $regexp) {\n"
"      $message =~ s/\\s+/ /g;\n"
"      return (index($message,$what) != -1) unless $nocase;\n"
"      return (index(lc2_($message),lc2_($what)) != -1);\n"
"    } else {\n"
"      return ($message =~ m/$what/) unless $nocase;\n"
"      return ($message =~ m/$what/i);\n"
"    }\n"
"  } elsif($where eq \"tag\") {\n"
"    my $found = 0;\n"
"    sub ClawsMail::Utils::get_tags;"
"    foreach my $tag (ClawsMail::Utils::get_tags) {\n"
"      if(not $regexp) {\n"
"        if($nocase) {\n"
"          $found = (index(lc2_($tag),lc2_($what)) != -1);\n"
"        } else {\n"
"          $found = (index($tag,$what) != -1);\n"
"        }\n"
"      } else {\n"
"        if ($nocase) {\n"
"          $found = ($tag =~ m/$what/i);\n"
"        } else {\n"
"          $found = ($tag =~ m/$what/);\n"
"        }\n"
"      }\n"
"      last if $found;\n"
"    }\n"
"    return $found;"
"  } else {\n"
"    $where = lc2_ $where;\n"
"    my $myheader = header(lc2_ $where); $myheader ||= \"\";\n"
"    return 0 unless $myheader;\n"
"    if(not $regexp) {    \n"
"      return (index(header($where),$what) != -1) unless $nocase;\n"
"      return (index(lc2_(header($where)),lc2_($what)) != -1);\n"
"    } else {\n"
"      return (header($where) =~ m/$what/) unless $nocase;\n"
"      return (header($where) =~ m/$what/i);\n"
"    } \n"
"  }\n"
"}\n"
"sub header_as_string_ {\n"
"    my $headerstring=\"\";\n"
"    my @headerkeys = header(); my(@fields,$field);\n"
"    foreach $field (@headerkeys) {\n"
"  @fields = header($field);\n"
"  foreach (@fields) {\n"
"      $headerstring .= $field.\": \".$_.\"\\n\";\n"
"  }\n"
"    }\n"
"    return $headerstring;\n"
"}\n"
"our $permanent = \"\";\n"
"1;\n"
  };
  const char perl_filter_action[] = {
"BEGIN {$INC{'ClawsMail/Filter/Action.pm'} = 1;}\n"
"package ClawsMail::Filter::Action;\n"
"use base qw(Exporter);\n"
"our @EXPORT = (qw(mark unmark dele mark_as_unread mark_as_read),\n"
"         qw(lock unlock move copy color execute),\n"
"         qw(hide set_score change_score stop exit),\n"
"         qw(forward forward_as_attachment redirect),\n"
"        qw(set_tag unset_tag clear_tags),\n"
"         );\n"
"our %colors = ('none'     =>  0,'orange' =>  1,\n"
"             'red'      =>  2,'pink'   =>  3,\n"
"             'sky blue' =>  4,'blue'   =>  5,\n"
"             'green'    =>  6,'brown'  =>  7);\n"
"sub mark           { ClawsMail::C::set_flag  (1);}\n"
"sub unmark         { ClawsMail::C::unset_flag(1);}\n"
"sub mark_as_unread { ClawsMail::C::set_flag  (2);}\n"
"sub mark_as_read   { ClawsMail::C::unset_flag(2);}\n"
"sub lock           { ClawsMail::C::set_flag  (7);}\n"
"sub unlock         { ClawsMail::C::unset_flag(7);}\n"
"sub copy           { ClawsMail::C::copy     (@_);}\n"
"sub forward        { ClawsMail::C::forward(1,@_);}\n"
"sub forward_as_attachment {ClawsMail::C::forward(2,@_);}\n"
"sub redirect       { ClawsMail::C::redirect(@_); }\n"
"sub hide           { ClawsMail::C::hide();       }\n"
"sub exit {\n"
"    ClawsMail::C::filter_log(\"LOG_ACTION\",\"exit\");\n"
"    stop(1);\n"
"}\n"
"sub stop {\n"
"    my $nolog = shift;\n"
"    ClawsMail::C::filter_log(\"LOG_ACTION\", \"stop\")\n"
"  unless defined($nolog);\n"
"    die 'intended';\n"
"}\n"
"sub set_score {\n"
"    $ClawsMail::Filter::Matcher::msginfo{\"score\"} =\n"
"  ClawsMail::C::set_score(@_);\n"
"}\n"
"sub change_score {\n"
"    $ClawsMail::Filter::Matcher::msginfo{\"score\"} =\n"
"  ClawsMail::C::change_score(@_);\n"
"}\n"
"sub execute {\n"
"    my $flv; my $cmd = shift; return 0 unless defined($cmd);\n"
"    $flv = ClawsMail::C::filter_log_verbosity(0);\n"
"    ClawsMail::Filter::Matcher::test($cmd);\n"
"    ClawsMail::C::filter_log_verbosity($flv);\n"
"    ClawsMail::C::filter_log(\"LOG_ACTION\", \"execute: $cmd\");\n"
"    1;\n"
"}\n"
"sub move { ClawsMail::C::move(@_); stop(1);}\n"
"sub dele { ClawsMail::C::delete(); stop(1);}\n"
"sub color {\n"
"    ($color) = @_;$color = lc2_ $color;\n"
"    $color = $colors{$color} if exists $colors{$color};\n"
"    $color = 0 if $color =~ m/\\D/;\n"
"    ClawsMail::C::color($color);\n"
"}\n"
"sub set_tag { ClawsMail::C::set_tag(@_);}\n"
"sub unset_tag { ClawsMail::C::unset_tag(@_);}\n"
"sub clear_tags { ClawsMail::C::clear_tags(@_);}\n"
"1;\n"
  };
  const char perl_utils[] = {
"BEGIN {$INC{'ClawsMail/Utils.pm'} = 1;}\n"
"package ClawsMail::Utils;\n"
"use base qw(Exporter);\n"
"our @EXPORT = (\n"
"             qw(SA_is_spam extract_addresses move_to_trash abort),\n"
"             qw(addr_in_addressbook from_in_addressbook),\n"
"             qw(get_attribute_value make_sure_folder_exists),\n"
"            qw(make_sure_tag_exists get_tags),\n"
"             );\n"
"# Spam\n"
"sub SA_is_spam {\n"
"    my $retval;\n"
"    $retval = not ClawsMail::Filter::Matcher::test('spamc -c < %F > /dev/null');\n"
"    ClawsMail::C::filter_log(\"LOG_MATCH\",\"SA_is_spam\") if $retval;\n"
"    return $retval;\n"
"}\n"
"# simple extract email addresses from a header field\n"
"sub extract_addresses {\n"
"  my $hf = shift; return undef unless defined($hf);\n"
"  my @addr = ();\n"
"  while($hf =~ m/[-.+\\w]+\\@[-.+\\w]+/) {\n"
"    $hf =~ s/^.*?([-.+\\w]+\\@[-.+\\w]+)//;\n"
"    push @addr,$1;\n"
"  }\n"
"  push @addr,\"\" unless @addr;\n"
"  return @addr;\n"
"}\n"
"# move to trash\n"
"sub move_to_trash {\n"
"    ClawsMail::C::move_to_trash();\n"
"    ClawsMail::Filter::Action::stop(1);\n"
"}\n"
"# make sure a folder with a given id exists\n"
"sub make_sure_folder_exists {\n"
"    ClawsMail::C::make_sure_folder_exists(@_);\n"
"}\n"
"sub make_sure_tag_exists {\n"
"    ClawsMail::C::make_sure_tag_exists(@_);\n"
"}\n"
"sub get_tags {\n"
"    ClawsMail::C::get_tags(@_);\n"
"}\n"
"# abort: stop() and do not continue with built-in filtering\n"
"sub abort {\n"
"    ClawsMail::C::abort();\n"
"    ClawsMail::Filter::Action::stop(1);\n"
"}\n"
"# addressbook query\n"
"sub addr_in_addressbook {\n"
"    return ClawsMail::C::addr_in_addressbook(@_) if @_;\n"
"    return 0;\n"
"}\n"
"sub from_in_addressbook {\n"
"    my ($from) = extract_addresses(ClawsMail::Filter::Matcher::header(\"from\"));\n"
"    return 0 unless $from;\n"
"    return addr_in_addressbook($from,@_);\n"
"}\n"
"sub get_attribute_value {\n"
"    my $email = shift; my $key = shift;\n"
"    return \"\" unless ($email and $key);\n"
"    return ClawsMail::C::get_attribute_value($email,$key,@_);\n"
"}\n"
"1;\n"
  };

  if((my_perl = perl_alloc()) == NULL) {
    g_warning("Perl plugin: not enough memory to allocate Perl interpreter");
    return -1;
  }
  PL_perl_destruct_level = 1;
  perl_construct(my_perl);

  exitstatus = perl_parse(my_perl, xs_init, 4, initialize, NULL);
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  eval_pv(perl_filter_matcher,TRUE);
  eval_pv(perl_filter_action,TRUE);
  eval_pv(perl_persistent,TRUE);
  eval_pv(perl_utils,TRUE);
  return exitstatus;
}

static gboolean my_filtering_hook(gpointer source, gpointer data)
{
  int retry;

  g_return_val_if_fail(source != NULL, FALSE);

  mail_filtering_data = (MailFilteringData *) source;
  msginfo = mail_filtering_data->msginfo;
  if (!msginfo)
    return FALSE;
  stop_filtering = FALSE;
  wrote_filter_log_head = FALSE;
  filter_log_verbosity = config.filter_log_verbosity;
  if(GPOINTER_TO_UINT(data) == AUTO_FILTER)
    manual_filtering = FALSE;
  else if(GPOINTER_TO_UINT(data) == MANU_FILTER)
    manual_filtering = TRUE;
  else
    debug_print("Invalid user data ignored.\n");

  if(!manual_filtering)
    statusbar_print_all("Perl Plugin: filtering message...");

  /* Process Skript File */
  retry = perl_load_file();
  while(retry == 1) {
    debug_print("Error processing Perl script file. Retrying..\n");
    retry = perl_load_file();
  }
  if(retry == 2) {
    debug_print("Error processing Perl script file. Aborting..\n");
    stop_filtering = FALSE;
  }
  return stop_filtering;
}

static void perl_plugin_save_config(void)
{
  PrefFile *pfile;
  gchar *rcpath;

  debug_print("Saving Perl plugin Configuration\n");

  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  pfile = prefs_write_open(rcpath);
  g_free(rcpath);
  if (!pfile || (prefs_set_block_label(pfile, "PerlPlugin") < 0))
    return;
  
  if (prefs_write_param(param, pfile->fp) < 0) {
    g_warning("failed to write Perl plugin configuration to file");
    prefs_file_close_revert(pfile);
    return;
  }
        if (fprintf(pfile->fp, "\n") < 0) {
    FILE_OP_ERROR(rcpath, "fprintf");
    prefs_file_close_revert(pfile);
  } else
          prefs_file_close(pfile);
}

gint plugin_init(gchar **error)
{
  int argc;
  char **argv;
  char **env;
  int status = 0;
  FILE *fp;
  gchar *perlfilter;
  gchar *rcpath;

  /* version check */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,7,4,6),
        VERSION_NUMERIC, "Perl", error))
    return -1;

  /* register hook for automatic and manual filtering */
  filtering_hook_id = hooks_register_hook(MAIL_FILTERING_HOOKLIST,
            my_filtering_hook,
            GUINT_TO_POINTER(AUTO_FILTER));
  if(filtering_hook_id == HOOK_NONE) {
    *error = g_strdup("Failed to register mail filtering hook");
    return -1;
  }
  manual_filtering_hook_id = hooks_register_hook(MAIL_MANUAL_FILTERING_HOOKLIST,
             my_filtering_hook,
             GUINT_TO_POINTER(MANU_FILTER));
  if(manual_filtering_hook_id == HOOK_NONE) {
    hooks_unregister_hook(MAIL_FILTERING_HOOKLIST, filtering_hook_id);
    *error = g_strdup("Failed to register manual mail filtering hook");
    return -1;
  }

  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  prefs_read_config(param, "PerlPlugin", rcpath, NULL);
  g_free(rcpath);

  /* make sure we have at least an empty scriptfile */
  perlfilter = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PERLFILTER, NULL);
  if((fp = claws_fopen(perlfilter, "a")) == NULL) {
    *error = g_strdup("Failed to create blank scriptfile");
    g_free(perlfilter);
    hooks_unregister_hook(MAIL_FILTERING_HOOKLIST,
        filtering_hook_id);
    hooks_unregister_hook(MAIL_MANUAL_FILTERING_HOOKLIST,
        manual_filtering_hook_id);
    return -1;
  }
  /* chmod for security */
  if (change_file_mode_rw(fp, perlfilter) < 0) {
    FILE_OP_ERROR(perlfilter, "chmod");
    g_warning("Perl plugin: can't change file mode");
  }
  claws_fclose(fp);
  g_free(perlfilter);

  argc = 1;
  argv = g_new0(char*, 1);
  argv[0] = NULL;
  env = g_new0(char*, 1);
  env[0] = NULL;


  /* Initialize Perl Interpreter */
  PERL_SYS_INIT3(&argc, &argv, &env);
  g_free(argv);
  g_free(env);
  if(my_perl == NULL)
    status = perl_init();
  if(status) {
    *error = g_strdup("Failed to load Perl Interpreter\n");
    hooks_unregister_hook(MAIL_FILTERING_HOOKLIST,
        filtering_hook_id);
    hooks_unregister_hook(MAIL_MANUAL_FILTERING_HOOKLIST,
        manual_filtering_hook_id);
    return -1;
  }

  perl_gtk_init();
  debug_print("Perl Plugin loaded\n");
  return 0;
}

gboolean plugin_done(void)
{
  hooks_unregister_hook(MAIL_FILTERING_HOOKLIST,
      filtering_hook_id);
  hooks_unregister_hook(MAIL_MANUAL_FILTERING_HOOKLIST,
      manual_filtering_hook_id);
  
  free_all_lists();

  if(my_perl != NULL) {
    PL_perl_destruct_level = 1;
    perl_destruct(my_perl);
    perl_free(my_perl);
  }
  PERL_SYS_TERM();

  perl_plugin_save_config();

  perl_gtk_done();
  debug_print("Perl Plugin unloaded\n");
  return TRUE;
}

const gchar *plugin_name(void)
{
  return "Perl";
}

const gchar *plugin_desc(void)
{
  return _("This plugin provides a Perl scripting interface for mail filters.\n"
    "Feedback to <berndth@gmx.de> is welcome.\n");
}

const gchar *plugin_type(void)
{
  return "GTK3";
}

const gchar *plugin_licence(void)
{
  return "GPL3+";
}

const gchar *plugin_version(void)
{
  return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
  static struct PluginFeature features[] =
    { {PLUGIN_FILTERING, N_("Perl integration")},
      {PLUGIN_NOTHING, NULL}};
  return features;
}
