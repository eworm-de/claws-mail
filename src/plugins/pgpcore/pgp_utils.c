/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2013 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_GPGME

#include <glib.h>

#include "pgp_utils.h"
#include "codeconv.h"
#include "file-utils.h"

gchar *get_part_as_string(MimeInfo *mimeinfo)
{
	gchar *textdata = NULL;
	gchar *filename = NULL;
	FILE *fp;

	cm_return_val_if_fail(mimeinfo != NULL, 0);
	procmime_decode_content(mimeinfo);
	
	if (mimeinfo->content == MIMECONTENT_MEM)
		textdata = g_strdup(mimeinfo->data.mem);
	else {
		filename = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(filename, mimeinfo) < 0) {
			g_warning("error dumping temporary file '%s'", filename);
			g_free(filename);
			return NULL;
		}
		fp = claws_fopen(filename,"rb");
		if (!fp) {
			g_warning("error opening temporary file '%s'", filename);

			g_free(filename);
			return NULL;
		}
		textdata = file_read_stream_to_str_no_recode(fp);

		claws_fclose(fp);
		g_unlink(filename);
		g_free(filename);
	}

	if (!g_utf8_validate(textdata, -1, NULL)) {
		gchar *tmp = NULL;
		codeconv_set_strict(TRUE);
		if (procmime_mimeinfo_get_parameter(mimeinfo, "charset")) {
			tmp = conv_codeset_strdup(textdata,
				procmime_mimeinfo_get_parameter(mimeinfo, "charset"),
				CS_UTF_8);
		}
		if (!tmp) {
			tmp = conv_codeset_strdup(textdata,
				conv_get_locale_charset_str_no_utf8(), 
				CS_UTF_8);
		}
		codeconv_set_strict(FALSE);
		if (!tmp) {
			tmp = conv_codeset_strdup(textdata,
				conv_get_locale_charset_str_no_utf8(), 
				CS_UTF_8);
		}
		if (tmp) {
			g_free(textdata);
			textdata = tmp;
		}
	}

	return textdata;	
}

gchar *pgp_locate_armor_header(gchar *textdata, const gchar *armor_header)
{
	gchar *pos;

	pos = strstr(textdata, armor_header);
	/*
	 * It's only a valid armor header if it's at the
	 * beginning of the buffer or a new line.
	 */
	if (pos != NULL && (pos == textdata || *(pos-1) == '\n'))
	{
	      return pos;
	}
	return NULL;
}
#endif /* USE_GPGME */
