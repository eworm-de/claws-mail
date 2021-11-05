/*
 * Copyright (C) 2006 Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <curl/curl.h>
#include <expat.h>
#include <string.h>
#include <ctype.h>

#include <common/codeconv.h>

#include "feed.h"

#include "parser.h"
#include "parser_opml.h"

static void _opml_parser_start(void *data, const gchar *el, const gchar **attr)
{
	OPMLProcessCtx *ctx = (OPMLProcessCtx *)data;
	gchar *title = NULL, *type = NULL, *url = NULL, *tmp = NULL;

	if( ctx->body_reached ) {
		if( ctx->depth >= 2 && !strcmp(el, "outline") ) {
			title = feed_parser_get_attribute_value(attr, "title");
			type = feed_parser_get_attribute_value(attr, "type");
			if( type != NULL && strcmp(type, "folder") ) {
				url = feed_parser_get_attribute_value(attr, "xmlUrl");

				if( url != NULL ) {
					if( !strncmp(url, "feed://", 7) )
						tmp = g_strdup(url+7);
					else if( !strncmp(url, "feed:", 5) )
						tmp = g_strdup(url+5);
				
					if( tmp != NULL ) {
						g_free(url);
						url = tmp;
					}
				}
			}

			if( ctx->user_function != NULL ) {
				ctx->user_function(title, url, ctx->depth, ctx->user_data);
			}
		}
	}

	if( ctx->depth == 1 ) {
		if( !strcmp(el, "body") ) {
			ctx->body_reached = TRUE;
		}
	}

	ctx->depth++;
}

static void _opml_parser_end(void *data, const gchar *el)
{
	OPMLProcessCtx *ctx = (OPMLProcessCtx *)data;

	ctx->depth--;
}

void opml_process(gchar *path, OPMLProcessFunc function, gpointer data)
{
	OPMLProcessCtx *ctx = NULL;
	gchar *contents = NULL;
	GError *error = NULL;
	gint status, err;

	/* Initialize our context */
	ctx = g_malloc( sizeof(OPMLProcessCtx) );
	ctx->parser = XML_ParserCreate(NULL);
	ctx->depth = 0;
	ctx->str = NULL;
	ctx->user_function = function;
	ctx->body_reached = FALSE;
	ctx->user_data = data;

	/* Set expat parser handlers */
	XML_SetUserData(ctx->parser, (void *)ctx);
	XML_SetElementHandler(ctx->parser,
			_opml_parser_start,
			_opml_parser_end);
	XML_SetCharacterDataHandler(ctx->parser, libfeed_expat_chparse);
	XML_SetUnknownEncodingHandler(ctx->parser,
			feed_parser_unknown_encoding_handler, NULL);

	if( !g_file_get_contents(path, &contents, NULL, &error) ) {
		g_warning("error: '%s'", error->message);
		g_error_free(error);
	}    

	if( contents ) {
/*
		lines = g_strsplit(contents, '\n', 0);

		while( lines[i] ) {
			status = XML_Parse(ctx->parser, lines[i], strlen(lines[i]), FALSE);
			if( status == XML_STATUS_ERROR ) {
				err = XML_GetErrorCode(ctx->parser);
				sprintf(stderr, "\nExpat: --- %s\n\n", XML_ErrorString(err));
			}
		}
*/
		status = XML_Parse(ctx->parser, contents, strlen(contents), FALSE);
		err = XML_GetErrorCode(ctx->parser);
		fprintf(stderr, "\nExpat: --- %s (%s)\n\n", XML_ErrorString(err),
			(status == XML_STATUS_OK ? "OK" : "NOT OK"));

		XML_Parse(ctx->parser, "", 0, TRUE);
	}    

	XML_ParserFree(ctx->parser);
	if (ctx->str != NULL)
		g_string_free(ctx->str, TRUE);
	g_free(ctx);
}
