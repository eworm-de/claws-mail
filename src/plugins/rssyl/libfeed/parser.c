/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>
#include <curl/curl.h>
#include <expat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "feed.h"

#include "parser.h"

enum {
	FEED_TYPE_NONE,
	FEED_TYPE_RDF,
	FEED_TYPE_RSS_20,
	FEED_TYPE_ATOM_03,
	FEED_TYPE_ATOM_10,
	FEED_TYPE_OPML
} FeedTypes;

static void _handler_set(XML_Parser parser, guint type)
{
	if( parser == NULL )
		return;

	switch(type) {
		case FEED_TYPE_RSS_20:
			XML_SetElementHandler(parser,
					feed_parser_rss20_start,
					feed_parser_rss20_end);
			break;

		case FEED_TYPE_RDF:
			XML_SetElementHandler(parser,
					feed_parser_rdf_start,
					feed_parser_rdf_end);
			break;

		case FEED_TYPE_ATOM_10:
			XML_SetElementHandler(parser,
					feed_parser_atom10_start,
					feed_parser_atom10_end);
			break;
	}
}

static void _elparse_start_chooser(void *data,
		const gchar *el, const gchar **attr)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	guint feedtype = FEED_TYPE_NONE;
	gchar *version;

	if( ctx->depth == 0 ) {

		/* RSS 2.0 detected */
		if( !strcmp(el, "rss") ) {
			feedtype = FEED_TYPE_RSS_20;
		} else if( !strcmp(el, "rdf:RDF") ) {
			feedtype = FEED_TYPE_RDF;
		} else if( !strcmp(el, "feed") ) {

			/* ATOM feed detected, let's check version */
			version = feed_parser_get_attribute_value(attr, "xmlns");
			if( version != NULL &&
					(!strcmp(version, "http://www.w3.org/2005/Atom") ||
					 !strcmp(version, "https://www.w3.org/2005/Atom")) )
				feedtype = FEED_TYPE_ATOM_10;
			else
				feedtype = FEED_TYPE_ATOM_03;
		} else {
			/* Not a known feed type */
			ctx->feed->is_valid = FALSE;
		}
	}

	_handler_set(ctx->parser, feedtype);

	ctx->depth++;
}

static void _elparse_end_dummy(void *data, const gchar *el)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;

	if( ctx->str != NULL ) {
		g_string_free(ctx->str, TRUE);
		ctx->str = NULL;
	}

	ctx->depth--;
}

void libfeed_expat_chparse(void *data, const gchar *s, gint len)
{
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	gchar *buf = NULL;
	gint i, xblank = 1;

	buf = g_strndup(s, len);

	/* check if the string is blank, ... */
	for( i = 0; i < strlen(buf); i++ )
		if( !isspace(buf[i]) )
			xblank = 0;

	/* ...because we do not want the blanks if we're just starting new GString */
	if( xblank > 0 && ctx->str == NULL ) {
		g_free(buf);
		return;
	}

	if( ctx->str == NULL ) {
		ctx->str = g_string_sized_new(len + 1);
	}

	g_string_append(ctx->str, buf);
	g_free(buf);
}


void feed_parser_set_expat_handlers(FeedParserCtx *ctx)
{
	XML_SetUserData(ctx->parser, (void *)ctx);

	XML_SetElementHandler(ctx->parser,
			_elparse_start_chooser,
			_elparse_end_dummy);

	XML_SetCharacterDataHandler(ctx->parser,
		libfeed_expat_chparse);

	XML_SetUnknownEncodingHandler(ctx->parser, feed_parser_unknown_encoding_handler,
			NULL);
}

size_t feed_writefunc(void *ptr, size_t size, size_t nmemb, void *data)
{
	gint len = size * nmemb;
	FeedParserCtx *ctx = (FeedParserCtx *)data;
	gint status, err;

	if (!ctx->feed->is_valid) {
		/* We already know that the feed is not valid, so we won't
		 * try parsing it. Just return correct number so libcurl is
		 * happy. */
		return len;
	}

	status = XML_Parse(ctx->parser, ptr, len, FALSE);

	if( status == XML_STATUS_ERROR ) {
		err = XML_GetErrorCode(ctx->parser);
		printf("\nExpat: --- %s\n\n", XML_ErrorString(err));
		ctx->feed->is_valid = FALSE;
	}

	return len;
}

gchar *feed_parser_get_attribute_value(const gchar **attr, const gchar *name)
{
	guint i;

	if( attr == NULL || name == NULL )
		return NULL;

	for( i = 0; attr[i] != NULL && attr[i+1] != NULL; i += 2 ) {
		if( !strcmp( attr[i], name) )
			return (gchar *)attr[i+1];
	}

	/* We haven't found anything. */
	return NULL;
}

#define CHARSIZEUTF32	4

enum {
	LEP_ICONV_OK,
	LEP_ICONV_FAILED,
	LEP_ICONV_ILSEQ,
	LEP_ICONV_INVAL,
	LEP_ICONV_UNKNOWN
};

static gint giconv_utf32_char(GIConv cd, const gchar *inbuf, size_t insize,
		guint32 *p_value)
{
#ifdef HAVE_ICONV
	size_t outsize;
	guchar outbuf[CHARSIZEUTF32];
	gchar *outbufp;
	gint r;

	outsize = sizeof(outbuf);
	outbufp = (gchar *)outbuf;
#ifdef HAVE_ICONV_PROTO_CONST
	r = g_iconv(cd, (const gchar **)&inbuf, &insize,
			&outbufp, &outsize);
#else
	r = g_iconv(cd, (gchar **)&inbuf, &insize,
			&outbufp, &outsize);
#endif
	if( r == -1 ) {
		g_iconv(cd, 0, 0, 0, 0);
		switch(errno) {
		case EILSEQ:
			return LEP_ICONV_ILSEQ;
		case EINVAL:
			return LEP_ICONV_INVAL;
		default:
			return LEP_ICONV_UNKNOWN;
		}
	} else {
		guint32 value;
		guint i;

		if( (insize > 0) || (outsize > 0) )
			return LEP_ICONV_FAILED;

		value = 0;
		for( i = 0; i < sizeof(outbuf); i++ ) {
			value = (value << 8) + outbuf[i];
		}
		*p_value = value;
		return LEP_ICONV_OK;
	}
#else
	return LEP_ICONV_FAILED;
#endif
}

static gint feed_parser_setup_unknown_encoding(const gchar *charset,
		XML_Encoding *info)
{
	GIConv cd;
	gint flag, r;
	gchar buf[4];
	guint i, j, k;
	guint32 value;

	cd = g_iconv_open("UTF-32BE", charset);
	if( cd == (GIConv) -1 )
		return -1;

	flag = 0;
	for( i = 0; i < 256; i++ ) {
		/* first char */
		buf[0] = i;
		info->map[i] = 0;
		r = giconv_utf32_char(cd, buf, 1, &value);
		if( r == LEP_ICONV_OK) {
			info->map[i] = value;
		} else if( r != LEP_ICONV_INVAL ) {
		} else {
			for( j = 0; j < 256; j++ ) {
				/* second char */
				buf[1] = j;
				r = giconv_utf32_char(cd, buf, 2, &value);
				if( r == LEP_ICONV_OK ) {
					flag = 1;
					info->map[i] = -2;
				} else if( r != LEP_ICONV_INVAL ) {
				} else {
					for( k = 0; k < 256; k++ ) {
						/* third char */
						buf[2] = k;
						r = giconv_utf32_char(cd, buf, 3, &value);
						if( r == LEP_ICONV_OK) {
							info->map[i] = -3;
						}
					}
				}
			}
		}
	}

	g_iconv_close(cd);

	return flag;
}

struct FeedParserUnknownEncoding {
	gchar *charset;
	GIConv cd;
};

static gint feed_parser_unknown_encoding_convert(void *data, const gchar *s)
{
	gint r;
	struct FeedParserUnknownEncoding *enc_data;
	size_t insize;
	guint32 value;

	enc_data = data;
	insize = 4;

	if( s == NULL )
		return -1;

	r = giconv_utf32_char(enc_data->cd, s, insize, &value);
	if( r != LEP_ICONV_OK )
		return -1;

	return 0;
}

static void feed_parser_unknown_encoding_data_free(void *data)
{
	struct FeedParserUnknownEncoding *enc_data;

	enc_data = data;
	g_free(enc_data->charset);
	g_iconv_close(enc_data->cd);
	g_free(enc_data);
}

int feed_parser_unknown_encoding_handler(void *encdata, const XML_Char *name,
		XML_Encoding *info)
{
	GIConv cd;
	struct FeedParserUnknownEncoding *data;
	int result;

	result = feed_parser_setup_unknown_encoding(name, info);
	if( result == 0 ) {
		info->data = NULL;
		info->convert = NULL;
		info->release = NULL;
		return XML_STATUS_OK;
	}

	cd = g_iconv_open("UTF-32BE", name);
	if( cd == (GIConv)-1 )
		return XML_STATUS_ERROR;

	data = g_malloc( sizeof(*data) );
	if( data == NULL ) {
		g_iconv_close(cd);
		return XML_STATUS_ERROR;
	}

	data->charset = g_strdup(name);
	if( data->charset == NULL ) {
		g_free(data);
		g_iconv_close(cd);
		return XML_STATUS_ERROR;
	}

	data->cd = cd;
	info->data = data;
	info->convert = feed_parser_unknown_encoding_convert;
	info->release = feed_parser_unknown_encoding_data_free;

	return XML_STATUS_OK;
}
