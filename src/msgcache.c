/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto & The Sylpheed Claws Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <time.h>

#include "msgcache.h"
#include "utils.h"
#include "procmsg.h"
#include "codeconv.h"

typedef enum
{
	DATA_READ,
	DATA_WRITE,
	DATA_APPEND
} DataOpenMode;

struct _MsgCache {
	GHashTable	*msgnum_table;
	GHashTable	*msgid_table;
	guint		 memusage;
	time_t		 last_access;
};

typedef struct _StringConverter StringConverter;
struct _StringConverter {
	gchar *(*convert) (StringConverter *converter, gchar *srcstr);
	void   (*free)    (StringConverter *converter);
};

typedef struct _StrdupConverter StrdupConverter;
struct _StrdupConverter {
	StringConverter converter;
};

typedef struct _CharsetConverter CharsetConverter;
struct _CharsetConverter {
	StringConverter converter;

	gchar *srccharset;
	gchar *dstcharset;
};

MsgCache *msgcache_new(void)
{
	MsgCache *cache;
	
	cache = g_new0(MsgCache, 1),
	cache->msgnum_table = g_hash_table_new(g_int_hash, g_int_equal);
	cache->msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	cache->last_access = time(NULL);

	return cache;
}

static gboolean msgcache_msginfo_free_func(gpointer num, gpointer msginfo, gpointer user_data)
{
	procmsg_msginfo_free((MsgInfo *)msginfo);
	return TRUE;
}											  

void msgcache_destroy(MsgCache *cache)
{
	g_return_if_fail(cache != NULL);

	g_hash_table_foreach_remove(cache->msgnum_table, msgcache_msginfo_free_func, NULL);
	g_hash_table_destroy(cache->msgid_table);
	g_hash_table_destroy(cache->msgnum_table);
	g_free(cache);
}

void msgcache_add_msg(MsgCache *cache, MsgInfo *msginfo) 
{
	MsgInfo *newmsginfo;

	g_return_if_fail(cache != NULL);
	g_return_if_fail(msginfo != NULL);

	newmsginfo = procmsg_msginfo_new_ref(msginfo);
	g_hash_table_insert(cache->msgnum_table, &newmsginfo->msgnum, newmsginfo);
	if(newmsginfo->msgid != NULL)
		g_hash_table_insert(cache->msgid_table, newmsginfo->msgid, newmsginfo);
	cache->memusage += procmsg_msginfo_memusage(msginfo);
	cache->last_access = time(NULL);

	debug_print("Cache size: %d messages, %d bytes\n", g_hash_table_size(cache->msgnum_table), cache->memusage);
}

void msgcache_remove_msg(MsgCache *cache, guint msgnum)
{
	MsgInfo *msginfo;

	g_return_if_fail(cache != NULL);
	g_return_if_fail(msgnum > 0);

	msginfo = (MsgInfo *) g_hash_table_lookup(cache->msgnum_table, &msgnum);
	if(!msginfo)
		return;

	cache->memusage -= procmsg_msginfo_memusage(msginfo);
	if(msginfo->msgid)
		g_hash_table_remove(cache->msgid_table, msginfo->msgid);
	g_hash_table_remove(cache->msgnum_table, &msginfo->msgnum);
	procmsg_msginfo_free(msginfo);
	cache->last_access = time(NULL);

	debug_print("Cache size: %d messages, %d byte\n", g_hash_table_size(cache->msgnum_table), cache->memusage);
}

void msgcache_update_msg(MsgCache *cache, MsgInfo *msginfo)
{
	MsgInfo *oldmsginfo, *newmsginfo;
	
	g_return_if_fail(cache != NULL);
	g_return_if_fail(msginfo != NULL);

	oldmsginfo = g_hash_table_lookup(cache->msgnum_table, &msginfo->msgnum);
	if(oldmsginfo && oldmsginfo->msgid) 
		g_hash_table_remove(cache->msgid_table, oldmsginfo->msgid);
	if (oldmsginfo) {
		g_hash_table_remove(cache->msgnum_table, &oldmsginfo->msgnum);
		cache->memusage -= procmsg_msginfo_memusage(oldmsginfo);
		procmsg_msginfo_free(oldmsginfo);
	}

	newmsginfo = procmsg_msginfo_new_ref(msginfo);
	g_hash_table_insert(cache->msgnum_table, &newmsginfo->msgnum, newmsginfo);
	if(newmsginfo->msgid)
		g_hash_table_insert(cache->msgid_table, newmsginfo->msgid, newmsginfo);
	cache->memusage += procmsg_msginfo_memusage(newmsginfo);
	cache->last_access = time(NULL);
	
	debug_print("Cache size: %d messages, %d byte\n", g_hash_table_size(cache->msgnum_table), cache->memusage);

	return;
}

MsgInfo *msgcache_get_msg(MsgCache *cache, guint num)
{
	MsgInfo *msginfo;

	g_return_val_if_fail(cache != NULL, NULL);

	msginfo = g_hash_table_lookup(cache->msgnum_table, &num);
	if(!msginfo)
		return NULL;
	cache->last_access = time(NULL);
	
	return procmsg_msginfo_new_ref(msginfo);
}

MsgInfo *msgcache_get_msg_by_id(MsgCache *cache, const gchar *msgid)
{
	MsgInfo *msginfo;
	
	g_return_val_if_fail(cache != NULL, NULL);
	g_return_val_if_fail(msgid != NULL, NULL);

	msginfo = g_hash_table_lookup(cache->msgid_table, msgid);
	if(!msginfo)
		return NULL;
	cache->last_access = time(NULL);
	
	return procmsg_msginfo_new_ref(msginfo);	
}

static void msgcache_get_msg_list_func(gpointer key, gpointer value, gpointer user_data)
{
	MsgInfoList **listptr = user_data;
	MsgInfo *msginfo = value;

	*listptr = g_slist_prepend(*listptr, procmsg_msginfo_new_ref(msginfo));
}

MsgInfoList *msgcache_get_msg_list(MsgCache *cache)
{
	MsgInfoList *msg_list = NULL;

	g_return_val_if_fail(cache != NULL, NULL);

	g_hash_table_foreach((GHashTable *)cache->msgnum_table, msgcache_get_msg_list_func, (gpointer)&msg_list);	
	cache->last_access = time(NULL);
	
	msg_list = g_slist_reverse(msg_list);

	return msg_list;
}

time_t msgcache_get_last_access_time(MsgCache *cache)
{
	g_return_val_if_fail(cache != NULL, 0);
	
	return cache->last_access;
}

gint msgcache_get_memory_usage(MsgCache *cache)
{
	g_return_val_if_fail(cache != NULL, 0);

	return cache->memusage;
}

/*
 *  Cache saving functions
 */

#define READ_CACHE_DATA(data, fp, total_len) \
{ \
	if ((tmp_len = msgcache_read_cache_data_str(fp, &data, conv)) < 0) { \
		procmsg_msginfo_free(msginfo); \
		error = TRUE; \
		break; \
	} \
	total_len += tmp_len; \
}

#define READ_CACHE_DATA_INT(n, fp) \
{ \
	guint32 idata; \
	size_t ni; \
 \
	if ((ni = fread(&idata, 1, sizeof(idata), fp)) != sizeof(idata)) { \
		g_warning("read_int: Cache data corrupted, read %d of %d at " \
			  "offset %d\n", ni, sizeof(idata), ftell(fp)); \
		procmsg_msginfo_free(msginfo); \
		error = TRUE; \
		break; \
	} else \
		n = idata;\
}

#define WRITE_CACHE_DATA_INT(n, fp)		\
{						\
	guint32 idata;				\
						\
	idata = (guint32)n;			\
	fwrite(&idata, sizeof(idata), 1, fp);	\
}

#define WRITE_CACHE_DATA(data, fp) \
{ \
	size_t len; \
	if (data == NULL) \
		len = 0; \
	else \
		len = strlen(data); \
	WRITE_CACHE_DATA_INT(len, fp); \
	if (len > 0) { \
		fwrite(data, len, 1, fp); \
	} \
}

static FILE *msgcache_open_data_file(const gchar *file, guint version,
				     DataOpenMode mode,
				     gchar *buf, size_t buf_size)
{
	FILE *fp;
	gint32 data_ver;

	g_return_val_if_fail(file != NULL, NULL);

	if (mode == DATA_WRITE) {
		if ((fp = fopen(file, "wb")) == NULL) {
			FILE_OP_ERROR(file, "fopen");
			return NULL;
		}
		if (change_file_mode_rw(fp, file) < 0)
			FILE_OP_ERROR(file, "chmod");

		WRITE_CACHE_DATA_INT(version, fp);
		return fp;
	}

	/* check version */
	if ((fp = fopen(file, "rb")) == NULL)
		debug_print("Mark/Cache file '%s' not found\n", file);
	else {
		if (buf && buf_size > 0)
			setvbuf(fp, buf, _IOFBF, buf_size);
		if (fread(&data_ver, sizeof(data_ver), 1, fp) != 1 ||
			 version != data_ver) {
			g_message("%s: Mark/Cache version is different (%u != %u). Discarding it.\n",
				  file, data_ver, version);
			fclose(fp);
			fp = NULL;
		}
	}
	
	if (mode == DATA_READ)
		return fp;

	if (fp) {
		/* reopen with append mode */
		fclose(fp);
		if ((fp = fopen(file, "ab")) == NULL)
			FILE_OP_ERROR(file, "fopen");
	} else {
		/* open with overwrite mode if mark file doesn't exist or
		   version is different */
		fp = msgcache_open_data_file(file, version, DATA_WRITE, buf,
					    buf_size);
	}

	return fp;
}

static gint msgcache_read_cache_data_str(FILE *fp, gchar **str, 
					 StringConverter *conv)
{
	gchar *tmpstr = NULL;
	size_t ni;
	guint32 len;

	*str = NULL;
	if ((ni = fread(&len, 1, sizeof(len), fp) != sizeof(len)) ||
	    len > G_MAXINT) {
		g_warning("read_data_str: Cache data (len) corrupted, read %d "
			  "of %d bytes at offset %d\n", ni, sizeof(len), 
			  ftell(fp));
		return -1;
	}

	if (len == 0)
		return 0;

	tmpstr = g_malloc(len + 1);

	if ((ni = fread(tmpstr, 1, len, fp)) != len) {
		g_warning("read_data_str: Cache data corrupted, read %d of %d "
			  "bytes at offset %d\n", 
			  ni, len, ftell(fp));
		g_free(tmpstr);
		return -1;
	}
	tmpstr[len] = 0;

	if (conv != NULL) {
		*str = conv->convert(conv, tmpstr);
		g_free(tmpstr);
	} else 
		*str = tmpstr;

	return len;
}

gchar *strconv_strdup_convert(StringConverter *conv, gchar *srcstr)
{
	return g_strdup(srcstr);
}

gchar *strconv_charset_convert(StringConverter *conv, gchar *srcstr)
{
	CharsetConverter *charsetconv = (CharsetConverter *) conv;

	return conv_codeset_strdup(srcstr, charsetconv->srccharset, charsetconv->dstcharset);
}

void strconv_charset_free(StringConverter *conv)
{
	CharsetConverter *charsetconv = (CharsetConverter *) conv;

	g_free(charsetconv->srccharset);
	g_free(charsetconv->dstcharset);
}

MsgCache *msgcache_read_cache(FolderItem *item, const gchar *cache_file)
{
	MsgCache *cache;
	FILE *fp;
	MsgInfo *msginfo;
	MsgTmpFlags tmp_flags = 0;
	gchar file_buf[BUFFSIZE];
	guint32 num;
        guint refnum;
	gboolean error = FALSE;
	StringConverter *conv = NULL;
	gchar *srccharset = NULL;
	const gchar *dstcharset = NULL;
	gchar *ref = NULL;
	guint memusage = 0;
	guint tmp_len = 0;
#if 0
	struct timeval start;
	struct timeval end;
	struct timeval diff;
	gettimeofday(&start, NULL);
#endif
	g_return_val_if_fail(cache_file != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	if ((fp = msgcache_open_data_file
		(cache_file, CACHE_VERSION, DATA_READ, file_buf, sizeof(file_buf))) == NULL)
		return NULL;

	debug_print("\tReading message cache from %s...\n", cache_file);

	if (item->stype == F_QUEUE) {
		tmp_flags |= MSG_QUEUED;
	} else if (item->stype == F_DRAFT) {
		tmp_flags |= MSG_DRAFT;
	}

	if (msgcache_read_cache_data_str(fp, &srccharset, NULL) < 0)
		return NULL;
	dstcharset = CS_UTF_8;
	if (srccharset == NULL || dstcharset == NULL) {
		conv = NULL;
	} else if (strcmp(srccharset, dstcharset) == 0) {
		StrdupConverter *strdupconv;

		debug_print("using StrdupConverter\n");

		strdupconv = g_new0(StrdupConverter, 1);
		strdupconv->converter.convert = strconv_strdup_convert;
		strdupconv->converter.free = NULL;

		conv = (StringConverter *) strdupconv;
	} else {
		CharsetConverter *charsetconv;

		debug_print("using CharsetConverter\n");

		charsetconv = g_new0(CharsetConverter, 1);
		charsetconv->converter.convert = strconv_charset_convert;
		charsetconv->converter.free = strconv_charset_free;
		charsetconv->srccharset = g_strdup(srccharset);
		charsetconv->dstcharset = g_strdup(dstcharset);

		conv = (StringConverter *) charsetconv;
	}
	g_free(srccharset);

	cache = msgcache_new();
	g_hash_table_freeze(cache->msgnum_table);
	g_hash_table_freeze(cache->msgid_table);

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		msginfo = procmsg_msginfo_new();
		msginfo->msgnum = num;
		memusage += sizeof(MsgInfo);
		
		READ_CACHE_DATA_INT(msginfo->size, fp);
		READ_CACHE_DATA_INT(msginfo->mtime, fp);
		READ_CACHE_DATA_INT(msginfo->date_t, fp);
		READ_CACHE_DATA_INT(msginfo->flags.tmp_flags, fp);
				
		READ_CACHE_DATA(msginfo->fromname, fp, memusage);

		READ_CACHE_DATA(msginfo->date, fp, memusage);
		READ_CACHE_DATA(msginfo->from, fp, memusage);
		READ_CACHE_DATA(msginfo->to, fp, memusage);
		READ_CACHE_DATA(msginfo->cc, fp, memusage);
		READ_CACHE_DATA(msginfo->newsgroups, fp, memusage);
		READ_CACHE_DATA(msginfo->subject, fp, memusage);
		READ_CACHE_DATA(msginfo->msgid, fp, memusage);
		READ_CACHE_DATA(msginfo->inreplyto, fp, memusage);
		READ_CACHE_DATA(msginfo->xref, fp, memusage);
		
		READ_CACHE_DATA_INT(msginfo->planned_download, fp);
		READ_CACHE_DATA_INT(msginfo->total_size, fp);
		READ_CACHE_DATA_INT(refnum, fp);
		
		for (; refnum != 0; refnum--) {
			ref = NULL;

			READ_CACHE_DATA(ref, fp, memusage);

			if (ref && strlen(ref))
				msginfo->references =
					g_slist_prepend(msginfo->references, ref);
		}
		if (msginfo->references)
			msginfo->references =
				g_slist_reverse(msginfo->references);

		msginfo->folder = item;
		msginfo->flags.tmp_flags |= tmp_flags;

		g_hash_table_insert(cache->msgnum_table, &msginfo->msgnum, msginfo);
		if(msginfo->msgid)
			g_hash_table_insert(cache->msgid_table, msginfo->msgid, msginfo);
	}
	fclose(fp);
	g_hash_table_thaw(cache->msgnum_table);
	g_hash_table_thaw(cache->msgid_table);

	if (conv != NULL) {
		if (conv->free != NULL)
			conv->free(conv);
		g_free(conv);
	}

	if(error) {
		msgcache_destroy(cache);
		return NULL;
	}

	cache->last_access = time(NULL);
	cache->memusage = memusage;

	debug_print("done. (%d items read)\n", g_hash_table_size(cache->msgnum_table));
	debug_print("Cache size: %d messages, %d byte\n", g_hash_table_size(cache->msgnum_table), cache->memusage);
#if 0
	gettimeofday(&end, NULL);
	timersub(&end, &start, &diff);
	printf("spent %d seconds %d usages %d;%d\n", diff.tv_sec, diff.tv_usec, cache->memusage, memusage);
#endif
	return cache;
}

void msgcache_read_mark(MsgCache *cache, const gchar *mark_file)
{
	FILE *fp;
	MsgInfo *msginfo;
	MsgPermFlags perm_flags;
	guint32 num;

	if ((fp = msgcache_open_data_file(mark_file, MARK_VERSION, DATA_READ, NULL, 0)) == NULL)
		return;

	debug_print("\tReading message marks from %s...\n", mark_file);

	while (fread(&num, sizeof(num), 1, fp) == 1) {
		if (fread(&perm_flags, sizeof(perm_flags), 1, fp) != 1) break;

		msginfo = g_hash_table_lookup(cache->msgnum_table, &num);
		if(msginfo) {
			msginfo->flags.perm_flags = perm_flags;
		}
	}
	fclose(fp);
}

void msgcache_write_cache(MsgInfo *msginfo, FILE *fp)
{
	MsgTmpFlags flags = msginfo->flags.tmp_flags & MSG_CACHED_FLAG_MASK;
	GSList *cur;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(msginfo->size, fp);
	WRITE_CACHE_DATA_INT(msginfo->mtime, fp);
	WRITE_CACHE_DATA_INT(msginfo->date_t, fp);
	WRITE_CACHE_DATA_INT(flags, fp);

	WRITE_CACHE_DATA(msginfo->fromname, fp);

	WRITE_CACHE_DATA(msginfo->date, fp);
	WRITE_CACHE_DATA(msginfo->from, fp);
	WRITE_CACHE_DATA(msginfo->to, fp);
	WRITE_CACHE_DATA(msginfo->cc, fp);
	WRITE_CACHE_DATA(msginfo->newsgroups, fp);
	WRITE_CACHE_DATA(msginfo->subject, fp);
	WRITE_CACHE_DATA(msginfo->msgid, fp);
	WRITE_CACHE_DATA(msginfo->inreplyto, fp);
	WRITE_CACHE_DATA(msginfo->xref, fp);
	WRITE_CACHE_DATA_INT(msginfo->planned_download, fp);
	WRITE_CACHE_DATA_INT(msginfo->total_size, fp);
        
	WRITE_CACHE_DATA_INT(g_slist_length(msginfo->references), fp);

	for (cur = msginfo->references; cur != NULL; cur = cur->next) {
		WRITE_CACHE_DATA((gchar *)cur->data, fp);
	}
}

static void msgcache_write_flags(MsgInfo *msginfo, FILE *fp)
{
	MsgPermFlags flags = msginfo->flags.perm_flags;

	WRITE_CACHE_DATA_INT(msginfo->msgnum, fp);
	WRITE_CACHE_DATA_INT(flags, fp);
}

struct write_fps
{
	FILE *cache_fp;
	FILE *mark_fp;
};

static void msgcache_write_func(gpointer key, gpointer value, gpointer user_data)
{
	MsgInfo *msginfo;
	struct write_fps *write_fps;

	msginfo = (MsgInfo *)value;
	write_fps = user_data;

	msgcache_write_cache(msginfo, write_fps->cache_fp);
	msgcache_write_flags(msginfo, write_fps->mark_fp);
}

gint msgcache_write(const gchar *cache_file, const gchar *mark_file, MsgCache *cache)
{
	struct write_fps write_fps;

	g_return_val_if_fail(cache_file != NULL, -1);
	g_return_val_if_fail(mark_file != NULL, -1);
	g_return_val_if_fail(cache != NULL, -1);

	write_fps.cache_fp = msgcache_open_data_file(cache_file, CACHE_VERSION,
		DATA_WRITE, NULL, 0);
	if (write_fps.cache_fp == NULL)
		return -1;

	WRITE_CACHE_DATA(CS_UTF_8, write_fps.cache_fp);

	write_fps.mark_fp = msgcache_open_data_file(mark_file, MARK_VERSION,
		DATA_WRITE, NULL, 0);
	if (write_fps.mark_fp == NULL) {
		fclose(write_fps.cache_fp);
		return -1;
	}

	debug_print("\tWriting message cache to %s and %s...\n", cache_file, mark_file);

	if (change_file_mode_rw(write_fps.cache_fp, cache_file) < 0)
		FILE_OP_ERROR(cache_file, "chmod");

	g_hash_table_foreach(cache->msgnum_table, msgcache_write_func, (gpointer)&write_fps);

	fclose(write_fps.cache_fp);
	fclose(write_fps.mark_fp);

	cache->last_access = time(NULL);

	debug_print("done.\n");
	return 0;
}

