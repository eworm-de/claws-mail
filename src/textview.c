/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "intl.h"
#include "main.h"
#include "summaryview.h"
#include "procheader.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "procmime.h"
#include "html.h"
#include "enriched.h"
#include "compose.h"
#include "addressbook.h"
#include "displayheader.h"
#include "account.h"
#include "mimeview.h"
#include "alertpanel.h"

typedef struct _RemoteURI	RemoteURI;

struct _RemoteURI
{
	gchar *uri;

	guint start;
	guint end;
};

static GdkColor quote_colors[3] = {
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0},
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0},
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0}
};

static GdkColor signature_color = {
	(gulong)0,
	(gushort)0x7fff,
	(gushort)0x7fff,
	(gushort)0x7fff
};
	
static GdkColor uri_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static GdkColor emphasis_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0xcfff
};

#if 0
static GdkColor error_color = {
	(gulong)0,
	(gushort)0xefff,
	(gushort)0,
	(gushort)0
};
#endif


#define TEXTVIEW_STATUSBAR_PUSH(textview, str)					    \
{									    \
	gtk_statusbar_push(GTK_STATUSBAR(textview->messageview->statusbar), \
			   textview->messageview->statusbar_cid, str);	    \
}

#define TEXTVIEW_STATUSBAR_POP(textview)						   \
{									   \
	gtk_statusbar_pop(GTK_STATUSBAR(textview->messageview->statusbar), \
			  textview->messageview->statusbar_cid);	   \
}

static void textview_show_ertf		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);
static void textview_add_part		(TextView	*textview,
					 MimeInfo	*mimeinfo);
static void textview_add_parts		(TextView	*textview,
					 MimeInfo	*mimeinfo);
static void textview_write_body		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 const gchar	*charset);
static void textview_show_html		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);

static void textview_write_line		(TextView	*textview,
					 const gchar	*str,
					 CodeConverter	*conv);
static void textview_write_link		(TextView	*textview,
					 const gchar	*str,
					 const gchar	*uri,
					 CodeConverter	*conv);

static GPtrArray *textview_scan_header	(TextView	*textview,
					 FILE		*fp);
static void textview_show_header	(TextView	*textview,
					 GPtrArray	*headers);

static gint textview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 TextView	*textview);
static gboolean textview_uri_button_pressed(GtkTextTag *tag, GObject *obj,
					    GdkEvent *event, GtkTextIter *iter,
					    TextView *textview);
static void textview_smooth_scroll_do		(TextView	*textview,
						 gfloat		 old_value,
						 gfloat		 last_value,
						 gint		 step);
static void textview_smooth_scroll_one_line	(TextView	*textview,
						 gboolean	 up);
static gboolean textview_smooth_scroll_page	(TextView	*textview,
						 gboolean	 up);

static gboolean textview_uri_security_check	(TextView	*textview,
						 RemoteURI	*uri);
static void textview_uri_list_remove_all	(GSList		*uri_list);


static void populate_popup(GtkTextView *textview, GtkMenu *menu,
			   gpointer *dummy)
{
	gtk_menu_detach(menu);
}

TextView *textview_create(void)
{
	TextView *textview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;
	PangoFontDescription *font_desc = NULL;

	debug_print("Creating text view...\n");
	textview = g_new0(TextView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrolledwin, prefs_common.mainview_width, -1);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	/* create GtkSText widgets for single-byte and multi-byte character */
	text = gtk_text_view_new();
	gtk_widget_show(text);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	g_signal_connect(G_OBJECT(text), "populate-popup",
		 G_CALLBACK(populate_popup), NULL);


	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);

	gtk_widget_ensure_style(text);

	if (prefs_common.normalfont)
		font_desc = pango_font_description_from_string
					(prefs_common.normalfont);
	if (font_desc) {
		gtk_widget_modify_font(text, font_desc);
	}
	pango_font_description_free(font_desc);

	gtk_widget_ref(scrolledwin);

	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	g_signal_connect(G_OBJECT(text), "key_press_event",
			 G_CALLBACK(textview_key_pressed),
			 textview);

	gtk_widget_show(scrolledwin);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	gtk_widget_show(vbox);

	textview->vbox             = vbox;
	textview->scrolledwin      = scrolledwin;
	textview->text             = text;
	textview->uri_list         = NULL;
	textview->body_pos         = 0;
	textview->show_all_headers = FALSE;
	textview->last_buttonpress = GDK_NOTHING;
	textview->show_url_msgid   = 0;

	return textview;
}

static void textview_create_tags(GtkTextView *text, TextView *textview)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextTag *tag;

	gtk_text_buffer_create_tag(buffer, "header",
				   "pixels-above-lines", 0,
				   "pixels-above-lines-set", TRUE,
				   "pixels-below-lines", 0,
				   "pixels-below-lines-set", TRUE,
				   "left-margin", 0,
				   "left-margin-set", TRUE,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "header_title",
				   "font", prefs_common.boldfont,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "quote0",
				   "foreground-gdk", &quote_colors[0],
				   NULL);
	gtk_text_buffer_create_tag(buffer, "quote1",
				   "foreground-gdk", &quote_colors[1],
				   NULL);
	gtk_text_buffer_create_tag(buffer, "quote2",
				   "foreground-gdk", &quote_colors[2],
				   NULL);
	gtk_text_buffer_create_tag(buffer, "emphasis",
				   "foreground-gdk", &emphasis_color,
				   NULL);
 	gtk_text_buffer_create_tag(buffer, "signature",
				   "foreground-gdk", &signature_color,
				   NULL);
 	tag = gtk_text_buffer_create_tag(buffer, "link",
					 "foreground-gdk", &uri_color,
					 NULL);

       g_signal_connect(G_OBJECT(tag), "event",
                         G_CALLBACK(textview_uri_button_pressed), textview);
 }

void textview_init(TextView *textview)
{
#warning FIXME_GTK2
#if 0
	gtkut_widget_disable_theme_engine(textview->text);
#endif
	textview_update_message_colors();
	textview_set_all_headers(textview, FALSE);
	textview_set_font(textview, NULL);

	textview_create_tags(GTK_TEXT_VIEW(textview->text), textview);
}

void textview_update_message_colors(void)
{
	GdkColor black = {0, 0, 0, 0};

	if (prefs_common.enable_color) {
		/* grab the quote colors, converting from an int to a GdkColor */
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level1_col,
					       &quote_colors[0]);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level2_col,
					       &quote_colors[1]);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level3_col,
					       &quote_colors[2]);
		gtkut_convert_int_to_gdk_color(prefs_common.uri_col,
					       &uri_color);
		gtkut_convert_int_to_gdk_color(prefs_common.signature_col,
					       &signature_color);
	} else {
		quote_colors[0] = quote_colors[1] = quote_colors[2] = 
			uri_color = emphasis_color = signature_color = black;
	}
}

void textview_show_message(TextView *textview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	FILE *fp;
	const gchar *charset = NULL;

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else
		charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	textview_set_font(textview, charset);
	textview_clear(textview);


/*
	if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0) perror("fseek");
	headers = textview_scan_header(textview, fp);
	if (headers) {
		GtkTextView *text = GTK_TEXT_VIEW(textview->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
		GtkTextIter iter;

		textview_show_header(textview, headers);
		procheader_header_array_destroy(headers);

		gtk_text_buffer_get_end_iter(buffer, &iter);
		textview->body_pos = gtk_text_iter_get_offset(&iter);
	}
*/
	textview_add_parts(textview, mimeinfo);

	fclose(fp);

	textview_set_position(textview, 0);
}

void textview_show_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	GtkTextView *text;
	const gchar *charset = NULL;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	if ((mimeinfo->type == MIMETYPE_MULTIPART) ||
	    ((mimeinfo->type == MIMETYPE_MESSAGE) && !g_strcasecmp(mimeinfo->subtype, "rfc822"))) {
		textview_clear(textview);
		textview_add_parts(textview, mimeinfo);
		return;
	}

	if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0)
		perror("fseek");
/*
	headers = textview_scan_header(textview, fp);
*/
	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else
		charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	textview_set_font(textview, charset);

	textview_clear(textview);

/*
	if (headers) {
		GtkTextView *text = GTK_TEXT_VIEW(textview->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
		GtkTextIter iter;

		textview_show_header(textview, headers);
		procheader_header_array_destroy(headers);

		gtk_text_buffer_get_end_iter(buffer, &iter);
		textview->body_pos = gtk_text_iter_get_offset(&iter);
		if (!mimeinfo->main) {
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
		}
	}
*/
	if (mimeinfo->type == MIMETYPE_MULTIPART)
		textview_add_parts(textview, mimeinfo);
	else
		textview_write_body(textview, mimeinfo, charset);

}

static void textview_add_part(TextView *textview, MimeInfo *mimeinfo)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;
	const gchar *name;
	gint charcount;

	g_return_if_fail(mimeinfo != NULL);
	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	charcount = gtk_text_buffer_get_char_count(buffer);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (mimeinfo->type == MIMETYPE_MULTIPART) return;

	if ((mimeinfo->type == MIMETYPE_MESSAGE) && !g_strcasecmp(mimeinfo->subtype, "rfc822")) {
		FILE *fp;

		fp = fopen(mimeinfo->filename, "rb");
		fseek(fp, mimeinfo->offset, SEEK_SET);
		headers = textview_scan_header(textview, fp);
		if (headers) {
			if (charcount > 0)
				gtk_text_buffer_insert(buffer, &iter, "\n", 1);
			textview_show_header(textview, headers);
			procheader_header_array_destroy(headers);
		}
		fclose(fp);
		return;
	}

	name = procmime_mimeinfo_get_parameter(mimeinfo, "filename");
	if (name == NULL)
		name = procmime_mimeinfo_get_parameter(mimeinfo, "name");
	if (name != NULL)
		g_snprintf(buf, sizeof(buf), "\n[%s  %s/%s (%d bytes)]\n",
			   name,
			   procmime_get_type_str(mimeinfo->type),
			   mimeinfo->subtype, mimeinfo->length);
	else
		g_snprintf(buf, sizeof(buf), "\n[%s/%s (%d bytes)]\n",
			   procmime_get_type_str(mimeinfo->type),
			   mimeinfo->subtype, mimeinfo->length);

	if (mimeinfo->type != MIMETYPE_TEXT) {
		gtk_text_buffer_insert(buffer, &iter, buf, -1);
	} else if (mimeinfo->disposition != DISPOSITIONTYPE_ATTACHMENT) {
		if (prefs_common.display_header && (charcount > 0))
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);
		if (textview->messageview->forced_charset)
			charset = textview->messageview->forced_charset;
		else if (prefs_common.force_charset)
			charset = prefs_common.force_charset;
		else
			charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");

		textview_write_body(textview, mimeinfo, charset);
	}
}

#if 0
static gboolean add_parts_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	TextView *textview = (TextView *) data;

	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	textview_add_part(textview, mimeinfo);

	return FALSE;
}

static void textview_add_parts(TextView *textview, MimeInfo *mimeinfo)
{
	g_return_if_fail(mimeinfo != NULL);

	g_node_traverse(mimeinfo->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, add_parts_func, textview);
}
#endif

static void recursive_add_parts(TextView *textview, GNode *node)
{
        GNode * iter;
	MimeInfo *mimeinfo;
        
        mimeinfo = (MimeInfo *) node->data;
        
        textview_add_part(textview, mimeinfo);
        
        if ((mimeinfo->type != MIMETYPE_MULTIPART) &&
            (mimeinfo->type != MIMETYPE_MESSAGE))
                return;
        
        if (strcasecmp(mimeinfo->subtype, "alternative") == 0) {
                GNode * prefered_body;
                int prefered_score;
                
                /*
                  text/plain : score 3
                  text/ *    : score 2
                  other      : score 1
                */
                prefered_body = NULL;
                prefered_score = 0;
                
                for(iter = g_node_first_child(node) ; iter != NULL ;
                    iter = g_node_next_sibling(iter)) {
                        int score;
                        MimeInfo * submime;
                        
                        score = 1;
                        submime = (MimeInfo *) iter->data;
                        if (submime->type == MIMETYPE_TEXT)
                                score = 2;
                        
                        if (submime->subtype != NULL) {
                                if (strcasecmp(submime->subtype, "plain") == 0)
                                        score = 3;
                        }
                        
                        if (score > prefered_score) {
                                prefered_score = score;
                                prefered_body = iter;
                        }
                }
                
                if (prefered_body != NULL) {
                        recursive_add_parts(textview, prefered_body);
                }
        }
        else {
                for(iter = g_node_first_child(node) ; iter != NULL ;
                    iter = g_node_next_sibling(iter)) {
                        recursive_add_parts(textview, iter);
                }
        }
}

static void textview_add_parts(TextView *textview, MimeInfo *mimeinfo)
{
	g_return_if_fail(mimeinfo != NULL);
        
        recursive_add_parts(textview, mimeinfo->node);
}

#define TEXT_INSERT(str) \
	gtk_text_buffer_insert(buffer, &iter, str, -1)

void textview_show_error(TextView *textview)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	TEXT_INSERT(_("This message can't be displayed.\n"));

}

void textview_show_mime_part(TextView *textview, MimeInfo *partinfo)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	if (!partinfo) return;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	TEXT_INSERT(_("The following can be performed on this part by "));
	TEXT_INSERT(_("right-clicking the icon or list item:\n"));

	TEXT_INSERT(_("    To save select 'Save as...' (Shortcut key: 'y')\n"));
	TEXT_INSERT(_("    To display as text select 'Display as text' "));
	TEXT_INSERT(_("(Shortcut key: 't')\n"));
	TEXT_INSERT(_("    To open with an external program select 'Open' "));
	TEXT_INSERT(_("(Shortcut key: 'l'),\n"));
	TEXT_INSERT(_("    (alternately double-click, or click the middle "));
	TEXT_INSERT(_("mouse button),\n"));
	TEXT_INSERT(_("    or 'Open with...' (Shortcut key: 'o')\n"));

}

#undef TEXT_INSERT

static void textview_write_body(TextView *textview, MimeInfo *mimeinfo,
				const gchar *charset)
{
	FILE *tmpfp;
	gchar buf[BUFFSIZE];
	CodeConverter *conv;

	conv = conv_code_converter_new(charset);

	textview->is_in_signature = FALSE;

	if(mimeinfo->encoding_type != ENC_BINARY && 
	   mimeinfo->encoding_type != ENC_7BIT && 
	   mimeinfo->encoding_type != ENC_8BIT)
		procmime_decode_content(mimeinfo);

	if (!g_strcasecmp(mimeinfo->subtype, "html")) {
		gchar *filename;
		
		filename = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(filename, mimeinfo) == 0) {
			tmpfp = fopen(filename, "rb");
			textview_show_html(textview, tmpfp, conv);
			fclose(tmpfp);
			unlink(filename);
		}
		g_free(filename);
	} else if (!g_strcasecmp(mimeinfo->subtype, "enriched")) {
		gchar *filename;
		
		filename = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(filename, mimeinfo) == 0) {
			tmpfp = fopen(filename, "rb");
			textview_show_ertf(textview, tmpfp, conv);
			fclose(tmpfp);
			unlink(filename);
		}
		g_free(filename);
	} else {
		tmpfp = fopen(mimeinfo->filename, "rb");
		fseek(tmpfp, mimeinfo->offset, SEEK_SET);
		debug_print("Viewing text content of type: %s (length: %d)\n", mimeinfo->subtype, mimeinfo->length);
		while ((fgets(buf, sizeof(buf), tmpfp) != NULL) && 
		       (ftell(tmpfp) <= mimeinfo->offset + mimeinfo->length))
			textview_write_line(textview, buf, conv);
		fclose(tmpfp);
	}

	conv_code_converter_destroy(conv);
}

static void textview_show_html(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	HTMLParser *parser;
	gchar *str;

	parser = html_parser_new(fp, conv);
	g_return_if_fail(parser != NULL);

	while ((str = html_parse(parser)) != NULL) {
	        if (parser->state == HTML_HREF) {
		        /* first time : get and copy the URL */
		        if (parser->href == NULL) {
				/* ALF - the sylpheed html parser returns an empty string,
				 * if still inside an <a>, but already parsed past HREF */
				str = strtok(str, " ");
				if (str) { 
					parser->href = strdup(str);
					/* the URL may (or not) be followed by the
					 * referenced text */
					str = strtok(NULL, "");
				}	
		        }
		        if (str != NULL)
			        textview_write_link(textview, str, parser->href, NULL);
	        } else
		        textview_write_line(textview, str, NULL);
	}
	html_parser_destroy(parser);
}

static void textview_show_ertf(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	ERTFParser *parser;
	gchar *str;

	parser = ertf_parser_new(fp, conv);
	g_return_if_fail(parser != NULL);

	while ((str = ertf_parse(parser)) != NULL) {
		textview_write_line(textview, str, NULL);
	}
	
	ertf_parser_destroy(parser);
}

/* get_uri_part() - retrieves a URI starting from scanpos.
		    Returns TRUE if succesful */
static gboolean get_uri_part(const gchar *start, const gchar *scanpos,
			     const gchar **bp, const gchar **ep)
{
	const gchar *ep_;

	g_return_val_if_fail(start != NULL, FALSE);
	g_return_val_if_fail(scanpos != NULL, FALSE);
	g_return_val_if_fail(bp != NULL, FALSE);
	g_return_val_if_fail(ep != NULL, FALSE);

	*bp = scanpos;

	/* find end point of URI */
	for (ep_ = scanpos; *ep_ != '\0'; ep_++) {
		if (!isgraph(*(const guchar *)ep_) ||
		    !isascii(*(const guchar *)ep_) ||
		    strchr("()<>\"", *ep_))
			break;
	}

	/* no punctuation at end of string */

	/* FIXME: this stripping of trailing punctuations may bite with other URIs.
	 * should pass some URI type to this function and decide on that whether
	 * to perform punctuation stripping */

#define IS_REAL_PUNCT(ch)	(ispunct(ch) && ((ch) != '/')) 

	for (; ep_ - 1 > scanpos + 1 &&
	       IS_REAL_PUNCT(*(const guchar *)(ep_ - 1));
	     ep_--)
		;

#undef IS_REAL_PUNCT

	*ep = ep_;

	return TRUE;		
}

static gchar *make_uri_string(const gchar *bp, const gchar *ep)
{
	return g_strndup(bp, ep - bp);
}

/* valid mail address characters */
#define IS_RFC822_CHAR(ch) \
	(isascii(ch) && \
	 (ch) > 32   && \
	 (ch) != 127 && \
	 !isspace(ch) && \
	 !strchr("(),;<>\"", (ch)))

/* alphabet and number within 7bit ASCII */
#define IS_ASCII_ALNUM(ch)	(isascii(ch) && isalnum(ch))
#define IS_QUOTE(ch) ((ch) == '\'' || (ch) == '"')

static GHashTable *create_domain_tab(void)
{
	static const gchar *toplvl_domains [] = {
	    "museum", "aero",
	    "arpa", "coop", "info", "name", "biz", "com", "edu", "gov",
	    "int", "mil", "net", "org", "ac", "ad", "ae", "af", "ag",
	    "ai", "al", "am", "an", "ao", "aq", "ar", "as", "at", "au",
	    "aw", "az", "ba", "bb", "bd", "be", "bf", "bg", "bh", "bi",
	    "bj", "bm", "bn", "bo", "br", "bs", "bt", "bv", "bw", "by",
	    "bz", "ca", "cc", "cd", "cf", "cg", "ch", "ci", "ck", "cl",
	    "cm", "cn", "co", "cr", "cu", "cv", "cx", "cy", "cz", "de",
	    "dj", "dk", "dm", "do", "dz", "ec", "ee", "eg", "eh", "er",
	    "es", "et", "fi", "fj", "fk", "fm", "fo", "fr", "ga", "gd",
	    "ge", "gf", "gg", "gh", "gi", "gl", "gm", "gn", "gp", "gq",
	    "gr", "gs", "gt", "gu", "gw", "gy", "hk", "hm", "hn", "hr",
	    "ht", "hu", "id", "ie", "il", "im", "in", "io", "iq", "ir",
	    "is", "it", "je", "jm", "jo", "jp", "ke", "kg", "kh", "ki",
	    "km", "kn", "kp", "kr", "kw", "ky", "kz", "la", "lb", "lc",
	    "li", "lk", "lr", "ls", "lt", "lu", "lv", "ly", "ma", "mc",
	    "md", "mg", "mh", "mk", "ml", "mm", "mn", "mo", "mp", "mq",
	    "mr", "ms", "mt", "mu", "mv", "mw", "mx", "my", "mz", "na",
	    "nc", "ne", "nf", "ng", "ni", "nl", "no", "np", "nr", "nu",
	    "nz", "om", "pa", "pe", "pf", "pg", "ph", "pk", "pl", "pm",
	    "pn", "pr", "ps", "pt", "pw", "py", "qa", "re", "ro", "ru",
	    "rw", "sa", "sb", "sc", "sd", "se", "sg", "sh", "si", "sj",
	    "sk", "sl", "sm", "sn", "so", "sr", "st", "sv", "sy", "sz",
	    "tc", "td", "tf", "tg", "th", "tj", "tk", "tm", "tn", "to",
	    "tp", "tr", "tt", "tv", "tw", "tz", "ua", "ug", "uk", "um",
	    "us", "uy", "uz", "va", "vc", "ve", "vg", "vi", "vn", "vu",
            "wf", "ws", "ye", "yt", "yu", "za", "zm", "zw" 
	};
	gint n;
	GHashTable *htab = g_hash_table_new(g_stricase_hash, g_stricase_equal);
	
	g_return_val_if_fail(htab, NULL);
	for (n = 0; n < sizeof toplvl_domains / sizeof toplvl_domains[0]; n++) 
		g_hash_table_insert(htab, (gpointer) toplvl_domains[n], (gpointer) toplvl_domains[n]);
	return htab;
}

static gboolean is_toplvl_domain(GHashTable *tab, const gchar *first, const gchar *last)
{
	const gint MAX_LVL_DOM_NAME_LEN = 6;
	gchar buf[MAX_LVL_DOM_NAME_LEN + 1];
	const gchar *m = buf + MAX_LVL_DOM_NAME_LEN + 1;
	register gchar *p;
	
	if (last - first > MAX_LVL_DOM_NAME_LEN || first > last)
		return FALSE;

	for (p = buf; p < m &&  first < last; *p++ = *first++)
		;
	*p = 0;

	return g_hash_table_lookup(tab, buf) != NULL;
}

/* get_email_part() - retrieves an email address. Returns TRUE if succesful */
static gboolean get_email_part(const gchar *start, const gchar *scanpos,
			       const gchar **bp, const gchar **ep)
{
	/* more complex than the uri part because we need to scan back and forward starting from
	 * the scan position. */
	gboolean result = FALSE;
	const gchar *bp_ = NULL;
	const gchar *ep_ = NULL;
	static GHashTable *dom_tab;
	const gchar *last_dot = NULL;
	const gchar *prelast_dot = NULL;
	const gchar *last_tld_char = NULL;

	/* the informative part of the email address (describing the name
	 * of the email address owner) may contain quoted parts. the
	 * closure stack stores the last encountered quotes. */
	gchar closure_stack[128];
	gchar *ptr = closure_stack;

	g_return_val_if_fail(start != NULL, FALSE);
	g_return_val_if_fail(scanpos != NULL, FALSE);
	g_return_val_if_fail(bp != NULL, FALSE);
	g_return_val_if_fail(ep != NULL, FALSE);

	if (!dom_tab)
		dom_tab = create_domain_tab();
	g_return_val_if_fail(dom_tab, FALSE);	

	/* scan start of address */
	for (bp_ = scanpos - 1;
	     bp_ >= start && IS_RFC822_CHAR(*(const guchar *)bp_); bp_--)
		;

	/* TODO: should start with an alnum? */
	bp_++;
	for (; bp_ < scanpos && !IS_ASCII_ALNUM(*(const guchar *)bp_); bp_++)
		;

	if (bp_ != scanpos) {
		/* scan end of address */
		for (ep_ = scanpos + 1;
		     *ep_ && IS_RFC822_CHAR(*(const guchar *)ep_); ep_++)
			if (*ep_ == '.') {
				prelast_dot = last_dot;
				last_dot = ep_;
		 		if (*(last_dot + 1) == '.') {
					if (prelast_dot == NULL)
	        				return FALSE;
					last_dot = prelast_dot;
					break;
				}
			}

		/* TODO: really should terminate with an alnum? */
		for (; ep_ > scanpos && !IS_ASCII_ALNUM(*(const guchar *)ep_);
		     --ep_)
			;
		ep_++;

		if (last_dot == NULL)
			return FALSE;
		if (last_dot >= ep_)
			last_dot = prelast_dot;
		if (last_dot == NULL || (scanpos + 1 >= last_dot))
			return FALSE;
		last_dot++;

		for (last_tld_char = last_dot; last_tld_char < ep_; last_tld_char++)
			if (*last_tld_char == '?')
				break;

		if (is_toplvl_domain(dom_tab, last_dot, last_tld_char))
			result = TRUE;

		*ep = ep_;
		*bp = bp_;
	}

	if (!result) return FALSE;

	/* skip if it's between quotes "'alfons@proteus.demon.nl'" <alfons@proteus.demon.nl> */
	if (bp_ - 1 > start && IS_QUOTE(*(bp_ - 1)) && IS_QUOTE(*ep_)) 
		return FALSE;

	/* see if this is <bracketed>; in this case we also scan for the informative part. */
	if (bp_ - 1 <= start || *(bp_ - 1) != '<' || *ep_ != '>')
		return TRUE;

#define FULL_STACK()	((size_t) (ptr - closure_stack) >= sizeof closure_stack)
#define IN_STACK()	(ptr > closure_stack)
/* has underrun check */
#define POP_STACK()	if(IN_STACK()) --ptr
/* has overrun check */
#define PUSH_STACK(c)	if(!FULL_STACK()) *ptr++ = (c); else return TRUE
/* has underrun check */
#define PEEK_STACK()	(IN_STACK() ? *(ptr - 1) : 0)

	ep_++;

	/* scan for the informative part. */
	for (bp_ -= 2; bp_ >= start; bp_--) {
		/* if closure on the stack keep scanning */
		if (PEEK_STACK() == *bp_) {
			POP_STACK();
			continue;
		}
		if (*bp_ == '\'' || *bp_ == '"') {
			PUSH_STACK(*bp_);
			continue;
		}

		/* if nothing in the closure stack, do the special conditions
		 * the following if..else expression simply checks whether 
		 * a token is acceptable. if not acceptable, the clause
		 * should terminate the loop with a 'break' */
		if (!PEEK_STACK()) {
			if (*bp_ == '-'
			&& (((bp_ - 1) >= start) && isalnum(*(bp_ - 1)))
			&& (((bp_ + 1) < ep_)    && isalnum(*(bp_ + 1)))) {
				/* hyphens are allowed, but only in
				   between alnums */
			} else if (!ispunct(*bp_)) {
				/* but anything not being a punctiation
				   is ok */
			} else {
				break; /* anything else is rejected */
			}
		}
	}

	bp_++;

#undef PEEK_STACK
#undef PUSH_STACK
#undef POP_STACK
#undef IN_STACK
#undef FULL_STACK

	/* scan forward (should start with an alnum) */
	for (; *bp_ != '<' && isspace(*bp_) && *bp_ != '"'; bp_++)
		;

	*ep = ep_;
	*bp = bp_;

	return result;
}

#undef IS_QUOTE
#undef IS_ASCII_ALNUM
#undef IS_RFC822_CHAR

static gchar *make_email_string(const gchar *bp, const gchar *ep)
{
	/* returns a mailto: URI; mailto: is also used to detect the
	 * uri type later on in the button_pressed signal handler */
	gchar *tmp;
	gchar *result;

	tmp = g_strndup(bp, ep - bp);
	result = g_strconcat("mailto:", tmp, NULL);
	g_free(tmp);

	return result;
}

static gchar *make_http_string(const gchar *bp, const gchar *ep)
{
	/* returns an http: URI; */
	gchar *tmp;
	gchar *result;

	tmp = g_strndup(bp, ep - bp);
	result = g_strconcat("http://", tmp, NULL);
	g_free(tmp);

	return result;
}

#define ADD_TXT_POS(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs\n"); \
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, \
							 linebuf, -1, \
							 fg_tag, NULL); \
		return; \
	}

/* textview_make_clickable_parts() - colorizes clickable parts */
static void textview_make_clickable_parts(TextView *textview,
					  const gchar *fg_tag,
					  const gchar *uri_tag,
					  const gchar *linebuf)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;

	/* parse table - in order of priority */
	struct table {
		const gchar *needle; /* token */

		/* token search function */
		gchar    *(*search)	(const gchar *haystack,
					 const gchar *needle);
		/* part parsing function */
		gboolean  (*parse)	(const gchar *start,
					 const gchar *scanpos,
					 const gchar **bp_,
					 const gchar **ep_);
		/* part to URI function */
		gchar    *(*build_uri)	(const gchar *bp,
					 const gchar *ep);
	};

	static struct table parser[] = {
		{"http://",  strcasestr, get_uri_part,   make_uri_string},
		{"https://", strcasestr, get_uri_part,   make_uri_string},
		{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
		{"www.",     strcasestr, get_uri_part,   make_http_string},
		{"mailto:",  strcasestr, get_uri_part,   make_uri_string},
		{"@",        strcasestr, get_email_part, make_email_string}
	};
	const gint PARSE_ELEMS = sizeof parser / sizeof parser[0];

	gint  n;
	const gchar *walk, *bp, *ep;

	struct txtpos {
		const gchar	*bp, *ep;	/* text position */
		gint		 pti;		/* index in parse table */
		struct txtpos	*next;		/* next */
	} head = {NULL, NULL, 0,  NULL}, *last = &head;

	gtk_text_buffer_get_end_iter(buffer, &iter);

	/* parse for clickable parts, and build a list of begin and end positions  */
	for (walk = linebuf, n = 0;;) {
		gint last_index = PARSE_ELEMS;
		gchar *scanpos = NULL;

		/* FIXME: this looks phony. scanning for anything in the parse table */
		for (n = 0; n < PARSE_ELEMS; n++) {
			gchar *tmp;

			tmp = parser[n].search(walk, parser[n].needle);
			if (tmp) {
				if (scanpos == NULL || tmp < scanpos) {
					scanpos = tmp;
					last_index = n;
				}
			}					
		}

		if (scanpos) {
			/* check if URI can be parsed */
			if (parser[last_index].parse(walk, scanpos, &bp, &ep)
			    && (size_t) (ep - bp - 1) > strlen(parser[last_index].needle)) {
					ADD_TXT_POS(bp, ep, last_index);
					walk = ep;
			} else
				walk = scanpos +
					strlen(parser[last_index].needle);
		} else
			break;
	}

	/* colorize this line */
	if (head.next) {
		const gchar *normal_text = linebuf;

		/* insert URIs */
		for (last = head.next; last != NULL;
		     normal_text = last->ep, last = last->next) {
			RemoteURI *uri;

			uri = g_new(RemoteURI, 1);
			if (last->bp - normal_text > 0)
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter,
					 normal_text,
					 last->bp - normal_text,
					 fg_tag, NULL);
			uri->uri = parser[last->pti].build_uri(last->bp,
							       last->ep);
			uri->start = gtk_text_iter_get_offset(&iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter,
				 last->bp, last->ep - last->bp,
				 uri_tag, NULL);
			uri->end = gtk_text_iter_get_offset(&iter);
			textview->uri_list =
				g_slist_append(textview->uri_list, uri);
		}

		if (*normal_text)
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
								 normal_text, -1,
								 fg_tag, NULL);
	} else {
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
							 linebuf, -1,
							 fg_tag, NULL);
	}
}

#undef ADD_TXT_POS

static void textview_write_line(TextView *textview, const gchar *str,
				CodeConverter *conv)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	gchar *fg_color;
	gint quotelevel = -1;
	gchar quote_tag_str[10];

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

#warning FIXME_GTK2
#if 0
	if (!conv) {
		if (textview->text_is_mb)
			conv_localetodisp(buf, sizeof(buf), str);
		else
			strncpy2(buf, str, sizeof(buf));
	} else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_localetodisp(buf, sizeof(buf), str);
	else if (textview->text_is_mb)
		conv_unreadable_locale(buf);
#else
	if (!conv)
		strncpy2(buf, str, sizeof(buf));
	else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_localetodisp(buf, sizeof(buf), str);
#endif

	strcrchomp(buf);
	if (prefs_common.conv_mb_alnum) conv_mb_alnum(buf);
	fg_color = NULL;

	/* change color of quotation
	   >, foo>, _> ... ok, <foo>, foo bar>, foo-> ... ng
	   Up to 3 levels of quotations are detected, and each
	   level is colored using a different color. */
	if (prefs_common.enable_color 
	    && line_has_quote_char(buf, prefs_common.quote_chars)) {
		quotelevel = get_quote_level(buf, prefs_common.quote_chars);

		/* set up the correct foreground color */
		if (quotelevel > 2) {
			/* recycle colors */
			if (prefs_common.recycle_quote_colors)
				quotelevel %= 3;
			else
				quotelevel = 2;
		}
	}

	if (quotelevel == -1)
		fg_color = NULL;
	else {
		g_snprintf(quote_tag_str, sizeof(quote_tag_str),
			   "quote%d", quotelevel);
		fg_color = quote_tag_str;
	}

	if (prefs_common.enable_color && (strcmp(buf,"-- \n") == 0 || textview->is_in_signature)) {
		fg_color = "signature";
		textview->is_in_signature = TRUE;
	}

	if (prefs_common.enable_color)
		textview_make_clickable_parts(textview, fg_color, "link", buf);
  	else
		textview_make_clickable_parts(textview, fg_color, NULL, buf);
}

void textview_write_link(TextView *textview, const gchar *str,
			 const gchar *uri, CodeConverter *conv)
{
    	GdkColor *link_color = NULL;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	gchar *bufp;
	RemoteURI *r_uri;

	if (*str == '\0')
		return;

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

#warning FIXME_GTK2
#if 0
	if (!conv) {
		if (textview->text_is_mb)
			conv_localetodisp(buf, sizeof(buf), str);
		else
			strncpy2(buf, str, sizeof(buf));
	} else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_localetodisp(buf, sizeof(buf), str);
	else if (textview->text_is_mb)
		conv_unreadable_locale(buf);
#else
	if (!conv)
		strncpy2(buf, str, sizeof(buf));
	else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_localetodisp(buf, sizeof(buf), str);
#endif

	strcrchomp(buf);

	gtk_text_buffer_get_end_iter(buffer, &iter);

	for (bufp = buf; isspace(*(guchar *)bufp); bufp++)
		gtk_text_buffer_insert(buffer, &iter, bufp, 1);

    	if (prefs_common.enable_color) {
		link_color = &uri_color;
    	}
	r_uri = g_new(RemoteURI, 1);
	r_uri->uri = g_strdup(uri);
	r_uri->start = gtk_text_iter_get_offset(&iter);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, bufp, -1,
						 "link", NULL);
	r_uri->end = gtk_text_iter_get_offset(&iter);
	textview->uri_list = g_slist_append(textview->uri_list, r_uri);
}

void textview_clear(TextView *textview)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_set_text(buffer, "\0", -1);

	TEXTVIEW_STATUSBAR_POP(textview);
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;

	textview->body_pos = 0;
}

void textview_destroy(TextView *textview)
{
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;

	g_free(textview);
}

void textview_set_all_headers(TextView *textview, gboolean all_headers)
{
	textview->show_all_headers = all_headers;
}

void textview_set_font(TextView *textview, const gchar *codeset)
{
	if (prefs_common.textfont) {
		PangoFontDescription *font_desc = NULL;

		if (prefs_common.textfont)
			font_desc = pango_font_description_from_string
						(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(textview->text, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(textview->text),
					     prefs_common.line_space / 2);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(textview->text),
					     prefs_common.line_space / 2);
	if (prefs_common.head_space) {
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview->text), 6);
	} else {
		gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview->text), 0);
	}
}

void textview_set_text(TextView *textview, const gchar *text)
{
	GtkTextView *view;
	GtkTextBuffer *buffer;

	g_return_if_fail(textview != NULL);
	g_return_if_fail(text != NULL);

	textview_clear(textview);

	view = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(view);
	gtk_text_buffer_set_text(buffer, text, strlen(text));
}

enum
{
	H_DATE		= 0,
	H_FROM		= 1,
	H_TO		= 2,
	H_NEWSGROUPS	= 3,
	H_SUBJECT	= 4,
	H_CC		= 5,
	H_REPLY_TO	= 6,
	H_FOLLOWUP_TO	= 7,
	H_X_MAILER	= 8,
	H_X_NEWSREADER	= 9,
	H_USER_AGENT	= 10,
	H_ORGANIZATION	= 11,
};

void textview_set_position(TextView *textview, gint pos)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, pos);
	gtk_text_buffer_place_cursor(buffer, &iter);
}

static GPtrArray *textview_scan_header(TextView *textview, FILE *fp)
{
	gchar buf[BUFFSIZE];
	GPtrArray *headers, *sorted_headers;
	GSList *disphdr_list;
	Header *header;
	gint i;

	g_return_val_if_fail(fp != NULL, NULL);

	if (textview->show_all_headers)
		return procheader_get_header_array_asis(fp);

	if (!prefs_common.display_header) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
		return NULL;
	}

	headers = procheader_get_header_array_asis(fp);

	sorted_headers = g_ptr_array_new();

	for (disphdr_list = prefs_common.disphdr_list; disphdr_list != NULL;
	     disphdr_list = disphdr_list->next) {
		DisplayHeaderProp *dp =
			(DisplayHeaderProp *)disphdr_list->data;

		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);

			if (procheader_headername_equal(header->name,
							dp->name)) {
				if (dp->hidden)
					procheader_header_free(header);
				else
					g_ptr_array_add(sorted_headers, header);

				g_ptr_array_remove_index(headers, i);
				i--;
			}
		}
	}

	if (prefs_common.show_other_header) {
		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			g_ptr_array_add(sorted_headers, header);
		}
		g_ptr_array_free(headers, TRUE);
	} else
		procheader_header_array_destroy(headers);


	return sorted_headers;
}

static void textview_show_header(TextView *textview, GPtrArray *headers)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;
	Header *header;
	gint i;

	g_return_if_fail(headers != NULL);

	for (i = 0; i < headers->len; i++) {
		header = g_ptr_array_index(headers, i);
		g_return_if_fail(header->name != NULL);

		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, header->name, -1,
			 "header_title", "header", NULL);
		if (header->name[strlen(header->name) - 1] != ' ')
#warning FIXME_GTK2
#if 0
			gtk_stext_insert(text, textview->boldfont,
					NULL, NULL, " ", 1);
#else
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, " ", 1,
				 "header_title", "header", NULL);
#endif

		if (procheader_headername_equal(header->name, "Subject") ||
		    procheader_headername_equal(header->name, "From")    ||
		    procheader_headername_equal(header->name, "To")      ||
		    procheader_headername_equal(header->name, "Cc"))
			unfold_line(header->body);

#warning FIXME_GTK2
#if 0
		if (textview->text_is_mb == TRUE)
			conv_unreadable_locale(header->body);
#endif

		if (prefs_common.enable_color &&
		    (procheader_headername_equal(header->name, "X-Mailer") ||
		     procheader_headername_equal(header->name,
						 "X-Newsreader")) &&
		    strstr(header->body, "Sylpheed") != NULL) {
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, header->body, -1,
				 "header", "emphasis", NULL);
		} else if (prefs_common.enable_color) {
			textview_make_clickable_parts(textview, "header", "link",
						      header->body);
		} else {
			textview_make_clickable_parts(textview, "header", NULL,
						      header->body);
		}
		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "\n", 1,
							 "header", NULL);
	}
}

gboolean textview_search_string(TextView *textview, const gchar *str,
				gboolean case_sens)
{
#warning FIXME_GTK2 /* currently, these search functions ignores case_sens */
#if 0
	GtkSText *text = GTK_STEXT(textview->text);
	gint pos;
	gint len;

	g_return_val_if_fail(str != NULL, FALSE);

	len = get_mbs_len(str);
	g_return_val_if_fail(len >= 0, FALSE);

	pos = textview->cur_pos;
	if (pos < textview->body_pos)
		pos = textview->body_pos;

	if ((pos = gtkut_stext_find(text, pos, str, case_sens)) != -1) {
		gtk_editable_set_position(GTK_EDITABLE(text), pos + len);
		gtk_editable_select_region(GTK_EDITABLE(text), pos, pos + len);
		textview_set_position(textview, pos + len);
		return TRUE;
	}

	return FALSE;
#else
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextMark *mark;
	GtkTextIter iter, start, end, *pos;
	gboolean found;
	gint insert_offset, selbound_offset;

	/* reset selection */
	mark = gtk_text_buffer_get_mark(buffer, "insert");
	gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
	insert_offset = gtk_text_iter_get_offset(&start);
	mark = gtk_text_buffer_get_mark(buffer, "selection_bound");
	gtk_text_buffer_get_iter_at_mark(buffer, &end, mark);
	selbound_offset = gtk_text_iter_get_offset(&end);

	pos = insert_offset > selbound_offset ? &start : &end;
	gtk_text_buffer_place_cursor(buffer, pos);

	/* search */
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	found = gtk_text_iter_forward_search(&iter, str,
					     GTK_TEXT_SEARCH_VISIBLE_ONLY,
					     &start, &end, NULL);
	if (found) {
		gtk_text_buffer_place_cursor(buffer, &start);
		gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &end);
		mark = gtk_text_buffer_get_mark(buffer, "insert");
		gtk_text_view_scroll_mark_onscreen(text, mark);
	}

	return found;
#endif
}

gboolean textview_search_string_backward(TextView *textview, const gchar *str,
					 gboolean case_sens)
{
#warning FIXME_GTK2
#if 0
	GtkSText *text = GTK_STEXT(textview->text);
	gint pos;
	wchar_t *wcs;
	gint len;
	gint text_len;
	gboolean found = FALSE;

	g_return_val_if_fail(str != NULL, FALSE);

	wcs = strdup_mbstowcs(str);
	g_return_val_if_fail(wcs != NULL, FALSE);
	len = wcslen(wcs);
	pos = textview->cur_pos;
	text_len = gtk_stext_get_length(text);
	if (text_len - textview->body_pos < len) {
		g_free(wcs);
		return FALSE;
	}
	if (pos <= textview->body_pos || text_len - pos < len)
		pos = text_len - len;

	for (; pos >= textview->body_pos; pos--) {
		if (gtk_stext_match_string(text, pos, wcs, len, case_sens)
		    == TRUE) {
			gtk_editable_set_position(GTK_EDITABLE(text), pos);
			gtk_editable_select_region(GTK_EDITABLE(text),
						   pos, pos + len);
			textview_set_position(textview, pos - 1);
			found = TRUE;
			break;
		}
		if (pos == textview->body_pos) break;
	}

	g_free(wcs);
	return found;
#else
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextMark *mark;
	GtkTextIter iter, start, end, *pos;
	gboolean found;
	gint insert_offset, selbound_offset;

	/* reset selection */
	mark = gtk_text_buffer_get_mark(buffer, "insert");
	gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
	insert_offset = gtk_text_iter_get_offset(&start);
	mark = gtk_text_buffer_get_mark(buffer, "selection_bound");
	gtk_text_buffer_get_iter_at_mark(buffer, &end, mark);
	selbound_offset = gtk_text_iter_get_offset(&end);

	pos = insert_offset < selbound_offset ? &start : &end;
	gtk_text_buffer_place_cursor(buffer, pos);

	/* search */
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	found = gtk_text_iter_backward_search(&iter, str,
					      GTK_TEXT_SEARCH_VISIBLE_ONLY,
					      &start, &end, NULL);
	if (found) {
		gtk_text_buffer_place_cursor(buffer, &end);
		gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &start);
		mark = gtk_text_buffer_get_mark(buffer, "insert");
		gtk_text_view_scroll_mark_onscreen(text, mark);
	}

	return found;
#endif
}

void textview_scroll_one_line(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;

	if (prefs_common.enable_smooth_scroll) {
		textview_smooth_scroll_one_line(textview, up);
		return;
	}

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			vadj->value +=
				vadj->step_increment * 4;
			vadj->value =
				MIN(vadj->value, upper);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	} else {
		if (vadj->value > 0.0) {
			vadj->value -=
				vadj->step_increment * 4;
			vadj->value =
				MAX(vadj->value, 0.0);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	}
}

gboolean textview_scroll_page(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat page_incr;

	if (prefs_common.enable_smooth_scroll)
		return textview_smooth_scroll_page(textview, up);

	if (prefs_common.scroll_halfpage)
		page_incr = vadj->page_increment / 2;
	else
		page_incr = vadj->page_increment;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			vadj->value += page_incr;
			vadj->value = MIN(vadj->value, upper);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	} else {
		if (vadj->value > 0.0) {
			vadj->value -= page_incr;
			vadj->value = MAX(vadj->value, 0.0);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	}

	return TRUE;
}

static void textview_smooth_scroll_do(TextView *textview,
				      gfloat old_value, gfloat last_value,
				      gint step)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gint change_value;
	gboolean up;
	gint i;

	if (old_value < last_value) {
		change_value = last_value - old_value;
		up = FALSE;
	} else {
		change_value = old_value - last_value;
		up = TRUE;
	}

#warning FIXME_GTK2
	/* gdk_key_repeat_disable(); */

	for (i = step; i <= change_value; i += step) {
		vadj->value = old_value + (up ? -i : i);
		g_signal_emit_by_name(G_OBJECT(vadj),
				      "value_changed", 0);
	}

	vadj->value = last_value;
	g_signal_emit_by_name(G_OBJECT(vadj), "value_changed", 0);

#warning FIXME_GTK2
	/* gdk_key_repeat_restore(); */
}

static void textview_smooth_scroll_one_line(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat old_value;
	gfloat last_value;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			old_value = vadj->value;
			last_value = vadj->value +
				vadj->step_increment * 4;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	} else {
		if (vadj->value > 0.0) {
			old_value = vadj->value;
			last_value = vadj->value -
				vadj->step_increment * 4;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	}
}

static gboolean textview_smooth_scroll_page(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = text->vadjustment;
	gfloat upper;
	gfloat page_incr;
	gfloat old_value;
	gfloat last_value;

	if (prefs_common.scroll_halfpage)
		page_incr = vadj->page_increment / 2;
	else
		page_incr = vadj->page_increment;

	if (!up) {
		upper = vadj->upper - vadj->page_size;
		if (vadj->value < upper) {
			old_value = vadj->value;
			last_value = vadj->value + page_incr;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	} else {
		if (vadj->value > 0.0) {
			old_value = vadj->value;
			last_value = vadj->value - page_incr;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	}

	return TRUE;
}

#warning FIXME_GTK2
#if 0
#define KEY_PRESS_EVENT_STOP() \
	if (gtk_signal_n_emissions_by_name \
		(G_OBJECT(widget), "key_press_event") > 0) { \
		g_signal_stop_emission_by_name(G_OBJECT(widget), \
					       "key_press_event"); \
	}
#else
#define KEY_PRESS_EVENT_STOP() \
	g_signal_stop_emission_by_name(G_OBJECT(widget), \
				       "key_press_event");
#endif

static gint textview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 TextView *textview)
{
	SummaryView *summaryview = NULL;
	MessageView *messageview = textview->messageview;

	if (!event) return FALSE;
	if (messageview->mainwin)
		summaryview = messageview->mainwin->summaryview;

	switch (event->keyval) {
	case GDK_Tab:
	case GDK_Home:
	case GDK_Left:
	case GDK_Up:
	case GDK_Right:
	case GDK_Down:
	case GDK_Page_Up:
	case GDK_Page_Down:
	case GDK_End:
	case GDK_Control_L:
	case GDK_Control_R:
		break;
	case GDK_space:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		else
			textview_scroll_page(textview, FALSE);
		break;
	case GDK_BackSpace:
		textview_scroll_page(textview, TRUE);
		break;
	case GDK_Return:
		textview_scroll_one_line(textview,
					 (event->state & GDK_MOD1_MASK) != 0);
		break;
	case GDK_Delete:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	case GDK_y:
	case GDK_t:
	case GDK_l:
		if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) == 0) {
			KEY_PRESS_EVENT_STOP();
			mimeview_pass_key_press_event(messageview->mimeview,
						      event);
			break;
		}
		/* possible fall through */
	default:
		if (summaryview &&
		    event->window != messageview->mainwin->window->window) {
			GdkEventKey tmpev = *event;

			tmpev.window = messageview->mainwin->window->window;
			KEY_PRESS_EVENT_STOP();
			gtk_widget_event(messageview->mainwin->window,
					 (GdkEvent *)&tmpev);
		}
		break;
	}

	return TRUE;
}

static gint show_url_timeout_cb(gpointer data)
{
	TextView *textview = (TextView *)data;
	
	TEXTVIEW_STATUSBAR_POP(textview);
	textview->show_url_timeout_tag = 0;
	return FALSE;
}

/*!
 *\brief    Check to see if a web URL has been disguised as a different
 *          URL (possible with HTML email).
 *
 *\param    uri The uri to check
 *
 *\param    textview The TextView the URL is contained in
 *
 *\return   gboolean TRUE if the URL is ok, or if the user chose to open
 *          it anyway, otherwise FALSE          
 */
static gboolean uri_security_check(RemoteURI *uri, TextView *textview) 
{
	gchar *clicked_str;
	gboolean retval = TRUE;

	if (g_strncasecmp(uri->uri, "http:", 5) &&
	    g_strncasecmp(uri->uri, "https:", 6) &&
	    g_strncasecmp(uri->uri, "www.", 4)) 
		return retval;

	clicked_str = gtk_editable_get_chars(GTK_EDITABLE(textview->text),
					     uri->start,
					     uri->end);
	if (clicked_str == NULL)
		return TRUE;

	if (strcmp(clicked_str, uri->uri) &&
	    (!g_strncasecmp(clicked_str, "http:",  5) ||
	     !g_strncasecmp(clicked_str, "https:", 6) ||
	     !g_strncasecmp(clicked_str, "www.",   4))) {
		gchar *str;
		retval = FALSE;

		/* allow uri->uri    == http://somewhere.com
		   and   clicked_str ==        somewhere.com */
		str = g_strconcat("http://", clicked_str, NULL);

		if (!g_strcasecmp(str, uri->uri))
			retval = TRUE;
		g_free(str);
	}

	if (retval == FALSE) {
		gchar *msg = NULL;
		AlertValue resp;

		msg = g_strdup_printf(_("The real URL (%s) is different from\n"
					"the apparent URL (%s).  \n"
					"Open it anyway?"),
					uri->uri, clicked_str);
		resp = alertpanel(_("Warning"), 
				  msg,
				  _("Yes"), 
				  _("No"),
				  NULL);
		g_free(msg);
		if (resp == G_ALERTDEFAULT)
			retval = TRUE;
	} 
	g_free(clicked_str);
	return retval;
}

static gboolean textview_uri_button_pressed(GtkTextTag *tag, GObject *obj,
					    GdkEvent *event, GtkTextIter *iter,
					    TextView *textview)
{
	GtkTextIter start_iter, end_iter;
	gint start_pos, end_pos;
	GdkEventButton *bevent;
	GSList *cur;
	gchar *trimmed_uri;

	if (event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS
		&& event->type != GDK_MOTION_NOTIFY)
		return FALSE;

	bevent = (GdkEventButton *) event;

	/* get start and end positions */
        start_iter = *iter;
        if(!gtk_text_iter_backward_to_tag_toggle(&start_iter, tag)) {
		debug_print("Can't find start.");
		return FALSE;
	}
	start_pos = gtk_text_iter_get_offset(&start_iter);

	end_iter = *iter;
	if(!gtk_text_iter_forward_to_tag_toggle(&end_iter, tag)) {
		debug_print("Can't find end");
		return FALSE;
	}
	end_pos = gtk_text_iter_get_offset(&end_iter);

	/* search current uri */
	for (cur = textview->uri_list; cur != NULL; cur = cur->next) {
		RemoteURI *uri = (RemoteURI *)cur->data;

		if (start_pos != uri->start || end_pos !=  uri->end)
			continue;

		trimmed_uri = trim_string(uri->uri, 60);
		/* hover or single click: display url in statusbar */

		if (event->type == GDK_MOTION_NOTIFY
		    || (event->type == GDK_BUTTON_PRESS && bevent->button == 1)) {
			if (textview->messageview->mainwin) {
				TEXTVIEW_STATUSBAR_PUSH(textview, trimmed_uri);
				textview->show_url_timeout_tag = gtk_timeout_add
					(4000, show_url_timeout_cb, textview);
			}
			return TRUE;
		}
		/* doubleclick: open compose / add address / browser */
		if ((event->type == GDK_2BUTTON_PRESS && bevent->button == 1) ||
			bevent->button == 2 || bevent->button == 3) {
			if (!g_strncasecmp(uri->uri, "mailto:", 7)) {
				if (bevent->button == 3) {
					gchar *fromname, *fromaddress;
						
					/* extract url */
					fromaddress = g_strdup(uri->uri + 7);
					/* Hiroyuki: please put this function in utils.c! */
					fromname = procheader_get_fromname(fromaddress);
					extract_address(fromaddress);
					g_message("adding from textview %s <%s>", fromname, fromaddress);
					/* Add to address book - Match */
					addressbook_add_contact( fromname, fromaddress, NULL );
						
					g_free(fromaddress);
					g_free(fromname);
				} else {
					PrefsAccount *account = NULL;

					if (textview->messageview && textview->messageview->msginfo &&
					    textview->messageview->msginfo->folder) {
						FolderItem   *folder_item;

						folder_item = textview->messageview->msginfo->folder;
						if (folder_item->prefs && folder_item->prefs->enable_default_account)
							account = account_find_from_id(folder_item->prefs->default_account);
					}
					compose_new(account, uri->uri + 7, NULL);
				}
			} else {
				if (textview_uri_security_check(textview, uri) == TRUE) 
					open_uri(uri->uri,
						 prefs_common.uri_cmd);
				return TRUE;
			}
		}
		g_free(trimmed_uri);
	}

	return FALSE;
}

/*!
 *\brief    Check to see if a web URL has been disguised as a different
 *          URL (possible with HTML email).
 *
 *\param    uri The uri to check
 *
 *\param    textview The TextView the URL is contained in
 *
 *\return   gboolean TRUE if the URL is ok, or if the user chose to open
 *          it anyway, otherwise FALSE          
 */
static gboolean textview_uri_security_check(TextView *textview, RemoteURI *uri)
{
	gchar *visible_str;
	gboolean retval = TRUE;

	if (is_uri_string(uri->uri) == FALSE)
		return TRUE;

	visible_str = gtk_editable_get_chars(GTK_EDITABLE(textview->text),
					     uri->start, uri->end);
	if (visible_str == NULL)
		return TRUE;

	if (strcmp(visible_str, uri->uri) != 0 && is_uri_string(visible_str)) {
		gchar *uri_path;
		gchar *visible_uri_path;

		uri_path = get_uri_path(uri->uri);
		visible_uri_path = get_uri_path(visible_str);
		if (strcmp(uri_path, visible_uri_path) != 0)
			retval = FALSE;
	}

	if (retval == FALSE) {
		gchar *msg;
		AlertValue aval;

		msg = g_strdup_printf(_("The real URL (%s) is different from\n"
					"the apparent URL (%s).\n"
					"Open it anyway?"),
				      uri->uri, visible_str);
		aval = alertpanel(_("Warning"), msg, _("Yes"), _("No"), NULL);
		g_free(msg);
		if (aval == G_ALERTDEFAULT)
			retval = TRUE;
	}

	g_free(visible_str);

	return retval;
}

static void textview_uri_list_remove_all(GSList *uri_list)
{
	GSList *cur;

	for (cur = uri_list; cur != NULL; cur = cur->next) {
		if (cur->data) {
			g_free(((RemoteURI *)cur->data)->uri);
			g_free(cur->data);
		}
	}

	g_slist_free(uri_list);
}
