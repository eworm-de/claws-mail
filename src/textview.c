/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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
#include "gtkstext.h"
#include "utils.h"
#include "gtkutils.h"
#include "procmime.h"
#include "html.h"
#include "enriched.h"
#include "compose.h"
#include "addressbook.h"
#include "displayheader.h"
#include "account.h"

#define FONT_LOAD(font, s) \
{ \
	gchar *fontstr, *p; \
 \
	Xstrdup_a(fontstr, s, ); \
	if ((p = strchr(fontstr, ',')) != NULL) *p = '\0'; \
	font = gdk_font_load(fontstr); \
	if (!font) \
		g_warning("Couldn't load the font '%s'\n", fontstr); \
}

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

static GdkFont *text_sb_font;
static GdkFont *text_mb_font;
static gint text_sb_font_orig_ascent;
static gint text_sb_font_orig_descent;
static gint text_mb_font_orig_ascent;
static gint text_mb_font_orig_descent;
static GdkFont *spacingfont;

static void textview_show_ertf		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);
static void textview_add_part		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 FILE		*fp);
static void textview_write_body		(TextView	*textview,
					 MimeInfo	*mimeinfo,
					 FILE		*fp,
					 const gchar	*charset);
static void textview_show_html		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);
static void textview_write_line		(TextView	*textview,
					 const gchar	*str,
					 CodeConverter	*conv);
static void textview_write_link         (TextView	*textview,
                                         const gchar    *url,
					 const gchar	*str,
					 CodeConverter	*conv);
static GPtrArray *textview_scan_header	(TextView	*textview,
					 FILE		*fp);
static void textview_show_header	(TextView	*textview,
					 GPtrArray	*headers);

static gint textview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 TextView	*textview);
static gint textview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 TextView	*textview);
static gint textview_button_released	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 TextView	*textview);

static void textview_uri_list_remove_all(GSList		*uri_list);

static void textview_smooth_scroll_do		(TextView	*textview,
						 gfloat		 old_value,
						 gfloat		 last_value,
						 gint		 step);
static void textview_smooth_scroll_one_line	(TextView	*textview,
						 gboolean	 up);
static gboolean textview_smooth_scroll_page	(TextView	*textview,
						 gboolean	 up);


TextView *textview_create(void)
{
	TextView *textview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin_sb;
	GtkWidget *scrolledwin_mb;
	GtkWidget *text_sb;
	GtkWidget *text_mb;

	debug_print(_("Creating text view...\n"));
	textview = g_new0(TextView, 1);

	scrolledwin_sb = gtk_scrolled_window_new(NULL, NULL);
	scrolledwin_mb = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin_sb),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin_mb),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(scrolledwin_sb, prefs_common.mainview_width, -1);
	gtk_widget_set_usize(scrolledwin_mb, prefs_common.mainview_width, -1);

	/* create GtkSText widgets for single-byte and multi-byte character */
	text_sb = gtk_stext_new(NULL, NULL);
	text_mb = gtk_stext_new(NULL, NULL);
	GTK_STEXT(text_sb)->default_tab_width = 8;
	GTK_STEXT(text_mb)->default_tab_width = 8;
	gtk_widget_show(text_sb);
	gtk_widget_show(text_mb);
	gtk_stext_set_word_wrap(GTK_STEXT(text_sb), TRUE);
	gtk_stext_set_word_wrap(GTK_STEXT(text_mb), TRUE);
	gtk_widget_ensure_style(text_sb);
	gtk_widget_ensure_style(text_mb);
	if (text_sb->style && text_sb->style->font->type == GDK_FONT_FONTSET) {
		GtkStyle *style;
		GdkFont *font;

		FONT_LOAD(font, prefs_common.normalfont);
		if (font) {
			style = gtk_style_copy(text_sb->style);
			gdk_font_unref(style->font);
			style->font = font;
			gtk_widget_set_style(text_sb, style);
		}
	}
	if (text_mb->style && text_mb->style->font->type == GDK_FONT_FONT) {
		GtkStyle *style;
		GdkFont *font;

		font = gdk_fontset_load(prefs_common.normalfont);
		if (font) {
			style = gtk_style_copy(text_mb->style);
			gdk_font_unref(style->font);
			style->font = font;
			gtk_widget_set_style(text_mb, style);
		}
	}
	gtk_widget_ref(scrolledwin_sb);
	gtk_widget_ref(scrolledwin_mb);

	gtk_container_add(GTK_CONTAINER(scrolledwin_sb), text_sb);
	gtk_container_add(GTK_CONTAINER(scrolledwin_mb), text_mb);
	gtk_signal_connect(GTK_OBJECT(text_sb), "key_press_event",
			   GTK_SIGNAL_FUNC(textview_key_pressed),
			   textview);
	gtk_signal_connect_after(GTK_OBJECT(text_sb), "button_press_event",
				 GTK_SIGNAL_FUNC(textview_button_pressed),
				 textview);
	gtk_signal_connect_after(GTK_OBJECT(text_sb), "button_release_event",
				 GTK_SIGNAL_FUNC(textview_button_released),
				 textview);
	gtk_signal_connect(GTK_OBJECT(text_mb), "key_press_event",
			   GTK_SIGNAL_FUNC(textview_key_pressed),
			   textview);
	gtk_signal_connect_after(GTK_OBJECT(text_mb), "button_press_event",
				 GTK_SIGNAL_FUNC(textview_button_pressed),
				 textview);
	gtk_signal_connect_after(GTK_OBJECT(text_mb), "button_release_event",
				 GTK_SIGNAL_FUNC(textview_button_released),
				 textview);

	gtk_widget_show(scrolledwin_sb);
	gtk_widget_show(scrolledwin_mb);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin_sb, TRUE, TRUE, 0);

	gtk_widget_show(vbox);

	textview->vbox             = vbox;
	textview->scrolledwin      = scrolledwin_sb;
	textview->scrolledwin_sb   = scrolledwin_sb;
	textview->scrolledwin_mb   = scrolledwin_mb;
	textview->text             = text_sb;
	textview->text_sb          = text_sb;
	textview->text_mb          = text_mb;
	textview->text_is_mb       = FALSE;
	textview->uri_list         = NULL;
	textview->body_pos         = 0;
	textview->cur_pos          = 0;
	textview->show_all_headers = FALSE;
	textview->last_buttonpress = GDK_NOTHING;

	return textview;
}

void textview_init(TextView *textview)
{
	gtkut_widget_disable_theme_engine(textview->text_sb);
	gtkut_widget_disable_theme_engine(textview->text_mb);
	textview_update_message_colors();
	textview_set_all_headers(textview, FALSE);
	textview_set_font(textview, NULL);
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
	} else {
		quote_colors[0] = quote_colors[1] = quote_colors[2] = 
			uri_color = emphasis_color = black;
	}
}

void textview_show_message(TextView *textview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	GtkSText *text = GTK_STEXT(textview->text);
	FILE *fp;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else if (mimeinfo->charset)
		charset = mimeinfo->charset;
	textview_set_font(textview, charset);
	textview_clear(textview);
	textview->body_pos = 0;
	textview->cur_pos  = 0;

	gtk_stext_freeze(text);

	if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0) perror("fseek");
	headers = textview_scan_header(textview, fp);
	if (headers) {
		textview_show_header(textview, headers);
		procheader_header_array_destroy(headers);
		textview->body_pos = gtk_stext_get_length(text);
	}

	while (mimeinfo != NULL) {
		textview_add_part(textview, mimeinfo, fp);
		if (mimeinfo->parent && mimeinfo->parent->content_type &&
		    !strcasecmp(mimeinfo->parent->content_type,
				"multipart/alternative"))
			mimeinfo = mimeinfo->parent->next;
		else
			mimeinfo = procmime_mimeinfo_next(mimeinfo);
	}

	gtk_stext_thaw(text);

	fclose(fp);
}

void textview_show_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gchar buf[BUFFSIZE];
	const gchar *boundary = NULL;
	gint boundary_len = 0;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	if (mimeinfo->mime_type == MIME_MULTIPART) return;

	if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	if (!boundary && (mimeinfo->mime_type == MIME_TEXT || 
			  mimeinfo->mime_type == MIME_TEXT_HTML || 
			  mimeinfo->mime_type == MIME_TEXT_ENRICHED)) {
	
		if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0)
			perror("fseek");
		headers = textview_scan_header(textview, fp);
	} else {
		if (mimeinfo->mime_type == MIME_TEXT && mimeinfo->parent) {
			glong fpos;
			MimeInfo *parent = mimeinfo->parent;

			while (parent->parent) {
				if (parent->main &&
				    parent->main->mime_type ==
					MIME_MESSAGE_RFC822)
					break;
				parent = parent->parent;
			}

			if ((fpos = ftell(fp)) < 0)
				perror("ftell");
			else if (fseek(fp, parent->fpos, SEEK_SET) < 0)
				perror("fseek");
			else {
				headers = textview_scan_header(textview, fp);
				if (fseek(fp, fpos, SEEK_SET) < 0)
					perror("fseek");
			}
		}
		/* skip MIME part headers */
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	/* display attached RFC822 single text message */
	if (mimeinfo->parent && mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
		if (headers) procheader_header_array_destroy(headers);
		if (!mimeinfo->sub || mimeinfo->sub->children) return;
		headers = textview_scan_header(textview, fp);
		mimeinfo = mimeinfo->sub;
	} else if (!mimeinfo->parent &&
		   mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
		if (headers) procheader_header_array_destroy(headers);
		if (!mimeinfo->sub) return;
		headers = textview_scan_header(textview, fp);
		mimeinfo = mimeinfo->sub;
	}

	if (prefs_common.force_charset)
		charset = prefs_common.force_charset;
	else if (mimeinfo->charset)
		charset = mimeinfo->charset;
	textview_set_font(textview, charset);

	textview_clear(textview);
	gtk_stext_freeze(text);

	textview->body_pos = 0;
	textview->cur_pos  = 0;

	if (headers) {
		textview_show_header(textview, headers);
		gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
		procheader_header_array_destroy(headers);
		textview->body_pos = gtk_stext_get_length(text);
	}

	textview_write_body(textview, mimeinfo, fp, charset);

	gtk_stext_thaw(text);
}

static void textview_add_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gchar buf[BUFFSIZE];
	const gchar *boundary = NULL;
	gint boundary_len = 0;
	const gchar *charset = NULL;
	GPtrArray *headers = NULL;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(fp != NULL);

	if (mimeinfo->mime_type == MIME_MULTIPART) return;

	if (!mimeinfo->parent &&
	    mimeinfo->mime_type != MIME_TEXT &&
	    mimeinfo->mime_type != MIME_TEXT_HTML &&
	    mimeinfo->mime_type != MIME_TEXT_ENRICHED)
		return;

	if (fseek(fp, mimeinfo->fpos, SEEK_SET) < 0) {
		perror("fseek");
		return;
	}

	if (mimeinfo->parent && mimeinfo->parent->boundary) {
		boundary = mimeinfo->parent->boundary;
		boundary_len = strlen(boundary);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	if (mimeinfo->mime_type == MIME_MESSAGE_RFC822) {
		headers = textview_scan_header(textview, fp);
		if (headers) {
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
			textview_show_header(textview, headers);
			procheader_header_array_destroy(headers);
		}
		return;
	}

	gtk_stext_freeze(text);

	if (mimeinfo->filename || mimeinfo->name)
		g_snprintf(buf, sizeof(buf), "\n[%s  %s (%d bytes)]\n",
			   mimeinfo->filename ? mimeinfo->filename :
			   mimeinfo->name,
			   mimeinfo->content_type, mimeinfo->size);
	else
		g_snprintf(buf, sizeof(buf), "\n[%s (%d bytes)]\n",
			   mimeinfo->content_type, mimeinfo->size);

	if (mimeinfo->mime_type != MIME_TEXT &&
	    mimeinfo->mime_type != MIME_TEXT_HTML &&
	    mimeinfo->mime_type != MIME_TEXT_ENRICHED) {
		gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);
	} else {
		if (!mimeinfo->main &&
		    mimeinfo->parent &&
		    mimeinfo->parent->children != mimeinfo)
			gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);
		else
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
		if (prefs_common.force_charset)
			charset = prefs_common.force_charset;
		else if (mimeinfo->charset)
			charset = mimeinfo->charset;
		textview_write_body(textview, mimeinfo, fp, charset);
	}

	gtk_stext_thaw(text);
}

#define TEXT_INSERT(str) \
	gtk_stext_insert(text, textview->msgfont, NULL, NULL, str, -1)

void textview_show_mime_part(TextView *textview, MimeInfo *partinfo)
{
	GtkSText *text;

	if (!partinfo) return;

	textview_set_font(textview, NULL);
	text = GTK_STEXT(textview->text);
	textview_clear(textview);

	gtk_stext_freeze(text);

	TEXT_INSERT(_("To save this part, pop up the context menu with "));
	TEXT_INSERT(_("right click and select `Save as...', "));
	TEXT_INSERT(_("or press `y' key.\n\n"));

	TEXT_INSERT(_("To display this part as a text message, select "));
	TEXT_INSERT(_("`Display as text', or press `t' key.\n\n"));

	TEXT_INSERT(_("To open this part with external program, select "));
	TEXT_INSERT(_("`Open' or `Open with...', "));
	TEXT_INSERT(_("or double-click, or click the center button, "));
	TEXT_INSERT(_("or press `l' key."));

	gtk_stext_thaw(text);
}

#if USE_GPGME
void textview_show_signature_part(TextView *textview, MimeInfo *partinfo)
{
	GtkSText *text;

	if (!partinfo) return;

	textview_set_font(textview, NULL);
	text = GTK_STEXT(textview->text);
	textview_clear(textview);

	gtk_stext_freeze(text);

	if (partinfo->sigstatus_full == NULL) {
		TEXT_INSERT(_("This signature has not been checked yet.\n"));
		TEXT_INSERT(_("To check it, pop up the context menu with\n"));
		TEXT_INSERT(_("right click and select `Check signature'.\n"));
	} else {
		TEXT_INSERT(partinfo->sigstatus_full);
	}

	gtk_stext_thaw(text);
}
#endif /* USE_GPGME */

#undef TEXT_INSERT

static void textview_write_body(TextView *textview, MimeInfo *mimeinfo,
				FILE *fp, const gchar *charset)
{
	FILE *tmpfp;
	gchar buf[BUFFSIZE];
	CodeConverter *conv;

	conv = conv_code_converter_new(charset);

	tmpfp = procmime_decode_content(NULL, fp, mimeinfo);
	if (tmpfp) {
		if (mimeinfo->mime_type == MIME_TEXT_HTML)
			textview_show_html(textview, tmpfp, conv);
		else if (mimeinfo->mime_type == MIME_TEXT_ENRICHED)
			textview_show_ertf(textview, tmpfp, conv);
		else
			while (fgets(buf, sizeof(buf), tmpfp) != NULL)
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
	gchar* url = NULL;

	parser = html_parser_new(fp, conv);
	g_return_if_fail(parser != NULL);

	while ((str = html_parse(parser)) != NULL) {
	        if (parser->state == HTML_HREF) {
		        /* first time : get and copy the URL */
		        if (url == NULL) {
				/* ALF - the sylpheed html parser returns an empty string,
				 * if still inside an <a>, but already parsed past HREF */
				str = strtok(str, " ");
				if (str) { 
					url = strdup(str);
					/* the URL may (or not) be followed by the
					 * referenced text */
					str = strtok(NULL, "");
				}	
		        }
		        if (str != NULL) {
			        textview_write_link(textview, url, str, NULL);
		        }
	        } else {
		        if (url != NULL) {
		                free(url);
				url = NULL;
			}
		        textview_write_line(textview, str, NULL);
	        }
	}
	html_parser_destroy(parser);
}

static void textview_show_ertf(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	ERTFParser *parser;
	gchar *str;
	gchar* url = NULL;

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
		if (!isgraph(*ep_) || !isascii(*ep_) || strchr("()<>\"", *ep_))
			break;
	}

	/* no punctuation at end of string */

	/* FIXME: this stripping of trailing punctuations may bite with other URIs.
	 * should pass some URI type to this function and decide on that whether
	 * to perform punctuation stripping */

#define IS_REAL_PUNCT(ch)	(ispunct(ch) && ((ch) != '/')) 

	for (; ep_ - 1 > scanpos + 1 && IS_REAL_PUNCT(*(ep_ - 1)); ep_--)
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
	 !strchr("()<>\"", (ch)))

/* alphabet and number within 7bit ASCII */
#define IS_ASCII_ALNUM(ch)	(isascii(ch) && isalnum(ch))
#define IS_QUOTE(ch) ((ch) == '\'' || (ch) == '"')

/* get_email_part() - retrieves an email address. Returns TRUE if succesful */
static gboolean get_email_part(const gchar *start, const gchar *scanpos,
			       const gchar **bp, const gchar **ep)
{
	/* more complex than the uri part because we need to scan back and forward starting from
	 * the scan position. */
	gboolean result = FALSE;
	const gchar *bp_ = NULL;
	const gchar *ep_ = NULL;

	/* the informative part of the email address (describing the name
	 * of the email address owner) may contain quoted parts. the
	 * closure stack stores the last encountered quotes. */
	gchar closure_stack[128];
	gchar *ptr = closure_stack;

	g_return_val_if_fail(start != NULL, FALSE);
	g_return_val_if_fail(scanpos != NULL, FALSE);
	g_return_val_if_fail(bp != NULL, FALSE);
	g_return_val_if_fail(ep != NULL, FALSE);

	/* scan start of address */
	for (bp_ = scanpos - 1; bp_ >= start && IS_RFC822_CHAR(*bp_); bp_--)
		;

	/* TODO: should start with an alnum? */
	bp_++;
	for (; bp_ < scanpos && !IS_ASCII_ALNUM(*bp_); bp_++)
		;

	if (bp_ != scanpos) {
		/* scan end of address */
		for (ep_ = scanpos + 1; *ep_ && IS_RFC822_CHAR(*ep_); ep_++)
			;

		/* TODO: really should terminate with an alnum? */
		for (; ep_ > scanpos && !IS_ASCII_ALNUM(*ep_); --ep_)
			;
		ep_++;

		if (ep_ > scanpos + 1) {
			*ep = ep_;
			*bp = bp_;
			result = TRUE;
		}
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

#define ADD_TXT_POS(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs\n"); \
		gtk_stext_insert(text, textview->msgfont, fg_color, NULL, \
				linebuf, -1); \
		return; \
	}

/* textview_make_clickable_parts() - colorizes clickable parts */
static void textview_make_clickable_parts(TextView *textview,
					  GdkFont *font,
					  GdkColor *fg_color,
					  GdkColor *uri_color,
					  const gchar *linebuf)
{
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
		{"www.",     strcasestr, get_uri_part,	 make_uri_string},
		{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
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

	GtkSText *text = GTK_STEXT(textview->text);

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
			if (parser[last_index].parse(linebuf, scanpos, &bp, &ep)
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
				gtk_stext_insert(text, font,
						fg_color, NULL,
						normal_text,
						last->bp - normal_text);
			uri->uri = parser[last->pti].build_uri(last->bp,
							       last->ep);
			uri->start = gtk_stext_get_point(text);
			gtk_stext_insert(text, font, uri_color,
					NULL, last->bp, last->ep - last->bp);
			uri->end = gtk_stext_get_point(text);
			textview->uri_list =
				g_slist_append(textview->uri_list, uri);
		}

		if (*normal_text)
			gtk_stext_insert(text, font, fg_color,
					NULL, normal_text, -1);
	} else
		gtk_stext_insert(text, font, fg_color, NULL, linebuf, -1);
}

#undef ADD_TXT_POS

/* This function writes str as a double-clickable link with the given url. */ 
static void textview_write_link(TextView *textview, const gchar *url,
                                const gchar *str, CodeConverter *conv)
{
    GdkColor *link_color = NULL;
    RemoteURI* uri;
    GtkSText *text = GTK_STEXT(textview->text);
    gchar buf[BUFFSIZE];

    /* this part is taken from textview_write_line. Right now the only place
     * that calls this function passes NULL for conv, but you never know. */
#if 0
    if (!conv)
	    strncpy2(buf, str, sizeof(buf));
    else if (conv_convert(conv, buf, sizeof(buf), str) < 0) {
       	    gtk_stext_insert(text, textview->msgfont,
		            prefs_common.enable_color
		            ? &error_color : NULL, NULL,
		            "*** Warning: code conversion failed ***\n",
		            -1);
	    return;
    }
#endif

    /* this part is based on the code in make_clickable_parts */
    if (prefs_common.enable_color) {
	link_color = &uri_color;
    }
    uri = g_new(RemoteURI, 1);
    uri->uri = g_strdup(url);
    uri->start = gtk_stext_get_point(text);
    gtk_stext_insert(text, textview->msgfont, link_color, NULL, buf,
	            strlen(buf));
    uri->end = gtk_stext_get_point(text);
    textview->uri_list = g_slist_append(textview->uri_list, uri);
}

static void textview_write_line(TextView *textview, const gchar *str,
				CodeConverter *conv)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gchar buf[BUFFSIZE];
	GdkColor *fg_color;
	gint quotelevel = -1;

#if 0
	if (!conv)
		strncpy2(buf, str, sizeof(buf));
	else if (conv_convert(conv, buf, sizeof(buf), str) < 0) {
		gtk_stext_insert(text, textview->msgfont,
				prefs_common.enable_color
				? &error_color : NULL, NULL,
				"*** Warning: code conversion failed ***\n",
				-1);
		return;
	}
#endif
	if (!conv || conv_convert(conv, buf, sizeof(buf), str) < 0)
		strncpy2(buf, str, sizeof(buf));

	strcrchomp(buf);
	if (prefs_common.conv_mb_alnum) conv_mb_alnum(buf);
	fg_color = NULL;

	/* change color of quotation
	   >, foo>, _> ... ok, <foo>, foo bar>, foo-> ... ng
	   Up to 3 levels of quotations are detected, and each
	   level is colored using a different color. */
	if (prefs_common.enable_color && strchr(buf, '>')) {
		quotelevel = get_quote_level(buf);

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
	else
		fg_color = &quote_colors[quotelevel];

	if (prefs_common.head_space && spacingfont && buf[0] != '\n')
		gtk_stext_insert(text, spacingfont, NULL, NULL, " ", 1);

	if (prefs_common.enable_color)
		textview_make_clickable_parts(textview, textview->msgfont,
					      fg_color, &uri_color, buf);
	else
		textview_make_clickable_parts(textview, textview->msgfont,
					      fg_color, NULL, buf);
}

void textview_clear(TextView *textview)
{
	GtkSText *text = GTK_STEXT(textview->text);

	gtk_stext_freeze(text);
	gtk_stext_set_point(text, 0);
	gtk_stext_forward_delete(text, gtk_stext_get_length(text));
	gtk_stext_thaw(text);

	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;
}

void textview_destroy(TextView *textview)
{
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;

	if (!textview->scrolledwin_sb->parent)
		gtk_widget_destroy(textview->scrolledwin_sb);
	if (!textview->scrolledwin_mb->parent)
		gtk_widget_destroy(textview->scrolledwin_mb);

	if (textview->msgfont)
		gdk_font_unref(textview->msgfont);
	if (textview->boldfont)
		gdk_font_unref(textview->boldfont);

	g_free(textview);
}

void textview_set_all_headers(TextView *textview, gboolean all_headers)
{
	textview->show_all_headers = all_headers;
}

void textview_set_font(TextView *textview, const gchar *codeset)
{
	gboolean use_fontset = TRUE;

	/* In multi-byte mode, GtkSText can't display 8bit characters
	   correctly, so it must be single-byte mode. */
	if (MB_CUR_MAX > 1) {
		if (codeset) {
			if (!g_strncasecmp(codeset, "ISO-8859-", 9) ||
			    !g_strcasecmp(codeset, "BALTIC"))
				use_fontset = FALSE;
			else if (conv_get_current_charset() != C_EUC_JP &&
				 (!g_strncasecmp(codeset, "KOI8-", 5) ||
				  !g_strncasecmp(codeset, "CP", 2)    ||
				  !g_strncasecmp(codeset, "WINDOWS-", 8)))
				use_fontset = FALSE;
		}
	} else
		use_fontset = FALSE;

	if (textview->text_is_mb && !use_fontset) {
		GtkWidget *parent;

		parent = textview->scrolledwin_mb->parent;
		gtkut_container_remove(GTK_CONTAINER(parent),
				       textview->scrolledwin_mb);
		gtk_container_add(GTK_CONTAINER(parent),
				  textview->scrolledwin_sb);

		textview->text = textview->text_sb;
		textview->text_is_mb = FALSE;
	} else if (!textview->text_is_mb && use_fontset) {
		GtkWidget *parent;

		parent = textview->scrolledwin_sb->parent;
		gtkut_container_remove(GTK_CONTAINER(parent),
				       textview->scrolledwin_sb);
		gtk_container_add(GTK_CONTAINER(parent),
				  textview->scrolledwin_mb);

		textview->text = textview->text_mb;
		textview->text_is_mb = TRUE;
	}

	if (prefs_common.textfont) {
		GdkFont *font;

		if (use_fontset) {
			if (text_mb_font) {
				text_mb_font->ascent = text_mb_font_orig_ascent;
				text_mb_font->descent = text_mb_font_orig_descent;
			}
			font = gdk_fontset_load(prefs_common.textfont);
			if (font && text_mb_font != font) {
				if (text_mb_font)
					gdk_font_unref(text_mb_font);
				text_mb_font = font;
				text_mb_font_orig_ascent = font->ascent;
				text_mb_font_orig_descent = font->descent;
			}
		} else {
			if (text_sb_font) {
				text_sb_font->ascent = text_sb_font_orig_ascent;
				text_sb_font->descent = text_sb_font_orig_descent;
			}
			if (MB_CUR_MAX > 1) {
				FONT_LOAD(font, "-*-courier-medium-r-normal--14-*-*-*-*-*-iso8859-1");
			} else {
				FONT_LOAD(font, prefs_common.textfont);
			}
			if (font && text_sb_font != font) {
				if (text_sb_font)
					gdk_font_unref(text_sb_font);
				text_sb_font = font;
				text_sb_font_orig_ascent = font->ascent;
				text_sb_font_orig_descent = font->descent;
			}
		}

		if (font) {
			gint ascent, descent;

			descent = prefs_common.line_space / 2;
			ascent  = prefs_common.line_space - descent;
			font->ascent  += ascent;
			font->descent += descent;

			if (textview->msgfont)
				gdk_font_unref(textview->msgfont);
			textview->msgfont = font;
			gdk_font_ref(font);
		}
	}

	if (!textview->boldfont && prefs_common.boldfont)
		FONT_LOAD(textview->boldfont, prefs_common.boldfont);
	if (!spacingfont)
		spacingfont = gdk_font_load("-*-*-medium-r-normal--6-*");
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
	if (pos < 0) {
		textview->cur_pos =
			gtk_stext_get_length(GTK_STEXT(textview->text));
	} else {
		textview->cur_pos = pos;
	}
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
	}

	g_ptr_array_free(headers, FALSE);

	return sorted_headers;
}

static void textview_show_header(TextView *textview, GPtrArray *headers)
{
	GtkSText *text = GTK_STEXT(textview->text);
	Header *header;
	gint i;

	g_return_if_fail(headers != NULL);

	gtk_stext_freeze(text);

	for (i = 0; i < headers->len; i++) {
		header = g_ptr_array_index(headers, i);
		g_return_if_fail(header->name != NULL);

		gtk_stext_insert(text, textview->boldfont, NULL, NULL,
				header->name, -1);
		if (header->name[strlen(header->name) - 1] != ' ')
			gtk_stext_insert(text, textview->boldfont,
					NULL, NULL, " ", 1);

		if (procheader_headername_equal(header->name, "Subject") ||
		    procheader_headername_equal(header->name, "From")    ||
		    procheader_headername_equal(header->name, "To")      ||
		    procheader_headername_equal(header->name, "Cc"))
			unfold_line(header->body);

		if (prefs_common.enable_color &&
		    (procheader_headername_equal(header->name, "X-Mailer") ||
		     procheader_headername_equal(header->name,
						 "X-Newsreader")) &&
		    strstr(header->body, "Sylpheed") != NULL)
			gtk_stext_insert(text, NULL, &emphasis_color, NULL,
					header->body, -1);
		else if (prefs_common.enable_color) {
			textview_make_clickable_parts(textview,
						      NULL, NULL, &uri_color,
						      header->body);
		} else {
			textview_make_clickable_parts(textview,
						      NULL, NULL, NULL,
						      header->body);
		}
		gtk_stext_insert(text, textview->msgfont, NULL, NULL, "\n", 1);
	}

	gtk_stext_thaw(text);
}

gboolean textview_search_string(TextView *textview, const gchar *str,
				gboolean case_sens)
{
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
	if (pos < textview->body_pos)
		pos = textview->body_pos;
	text_len = gtk_stext_get_length(text);
	if (text_len - pos < len) {
		g_free(wcs);
		return FALSE;
	}

	for (; pos < text_len; pos++) {
		if (text_len - pos < len) break;
		if (gtkut_stext_match_string(text, pos, wcs, len, case_sens)
		    == TRUE) {
			gtk_widget_hide(GTK_WIDGET(textview->scrolledwin));
			gtk_editable_set_position(GTK_EDITABLE(text),
						  pos + len);
			gtk_editable_select_region(GTK_EDITABLE(text),
						   pos, pos + len);
			gtk_widget_show(GTK_WIDGET(textview->scrolledwin));
			textview_set_position(textview, pos + len);
			found = TRUE;
			break;
		}
		if (text_len - pos == len) break;
	}
	g_free(wcs);
	return found;
}

gboolean textview_search_string_backward(TextView *textview, const gchar *str,
					 gboolean case_sens)
{
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
		if (gtkut_stext_match_string(text, pos, wcs, len, case_sens)
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
}

void textview_scroll_one_line(TextView *textview, gboolean up)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gfloat upper;

	if (prefs_common.enable_smooth_scroll) {
		textview_smooth_scroll_one_line(textview, up);
		return;
	}

	if (!up) {
		upper = text->vadj->upper - text->vadj->page_size;
		if (text->vadj->value < upper) {
			text->vadj->value +=
				text->vadj->step_increment * 4;
			text->vadj->value =
				MIN(text->vadj->value, upper);
			gtk_signal_emit_by_name(GTK_OBJECT(text->vadj),
						"value_changed");
		}
	} else {
		if (text->vadj->value > 0.0) {
			text->vadj->value -=
				text->vadj->step_increment * 4;
			text->vadj->value =
				MAX(text->vadj->value, 0.0);
			gtk_signal_emit_by_name(GTK_OBJECT(text->vadj),
						"value_changed");
		}
	}
}

gboolean textview_scroll_page(TextView *textview, gboolean up)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gfloat upper;
	gfloat page_incr;

	if (prefs_common.enable_smooth_scroll)
		return textview_smooth_scroll_page(textview, up);

	if (prefs_common.scroll_halfpage)
		page_incr = text->vadj->page_increment / 2;
	else
		page_incr = text->vadj->page_increment;

	if (!up) {
		upper = text->vadj->upper - text->vadj->page_size;
		if (text->vadj->value < upper) {
			text->vadj->value += page_incr;
			text->vadj->value = MIN(text->vadj->value, upper);
			gtk_signal_emit_by_name(GTK_OBJECT(text->vadj),
						"value_changed");
		} else
			return FALSE;
	} else {
		if (text->vadj->value > 0.0) {
			text->vadj->value -= page_incr;
			text->vadj->value = MAX(text->vadj->value, 0.0);
			gtk_signal_emit_by_name(GTK_OBJECT(text->vadj),
						"value_changed");
		} else
			return FALSE;
	}

	return TRUE;
}

static void textview_smooth_scroll_do(TextView *textview,
				      gfloat old_value, gfloat last_value,
				      gint step)
{
	GtkSText *text = GTK_STEXT(textview->text);
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

	gdk_key_repeat_disable();

	for (i = step; i <= change_value; i += step) {
		text->vadj->value = old_value + (up ? -i : i);
		gtk_signal_emit_by_name(GTK_OBJECT(text->vadj),
					"value_changed");
	}

	text->vadj->value = last_value;
	gtk_signal_emit_by_name(GTK_OBJECT(text->vadj), "value_changed");

	gdk_key_repeat_restore();
}

static void textview_smooth_scroll_one_line(TextView *textview, gboolean up)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gfloat upper;
	gfloat old_value;
	gfloat last_value;

	if (!up) {
		upper = text->vadj->upper - text->vadj->page_size;
		if (text->vadj->value < upper) {
			old_value = text->vadj->value;
			last_value = text->vadj->value +
				text->vadj->step_increment * 4;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	} else {
		if (text->vadj->value > 0.0) {
			old_value = text->vadj->value;
			last_value = text->vadj->value -
				text->vadj->step_increment * 4;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	}
}

static gboolean textview_smooth_scroll_page(TextView *textview, gboolean up)
{
	GtkSText *text = GTK_STEXT(textview->text);
	gfloat upper;
	gfloat page_incr;
	gfloat old_value;
	gfloat last_value;

	if (prefs_common.scroll_halfpage)
		page_incr = text->vadj->page_increment / 2;
	else
		page_incr = text->vadj->page_increment;

	if (!up) {
		upper = text->vadj->upper - text->vadj->page_size;
		if (text->vadj->value < upper) {
			old_value = text->vadj->value;
			last_value = text->vadj->value + page_incr;
			last_value = MIN(last_value, upper);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	} else {
		if (text->vadj->value > 0.0) {
			old_value = text->vadj->value;
			last_value = text->vadj->value - page_incr;
			last_value = MAX(last_value, 0.0);

			textview_smooth_scroll_do(textview, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	}

	return TRUE;
}

#define KEY_PRESS_EVENT_STOP() \
	if (gtk_signal_n_emissions_by_name \
		(GTK_OBJECT(widget), "key_press_event") > 0) { \
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), \
					     "key_press_event"); \
	}

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
	case GDK_n:
	case GDK_N:
	case GDK_p:
	case GDK_P:
	case GDK_y:
	case GDK_t:
	case GDK_l:
		if (messageview->type == MVIEW_MIME) {
			KEY_PRESS_EVENT_STOP();
			mimeview_pass_key_press_event(messageview->mimeview,
						      event);
			break;
		}
		/* fall through */
	default:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	}

	return TRUE;
}

static gint textview_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    TextView *textview)
{
	if (event)
		textview->last_buttonpress = event->type;
	return FALSE;
}

static gint textview_button_released(GtkWidget *widget, GdkEventButton *event,
				    TextView *textview)
{
	textview->cur_pos = 
		gtk_editable_get_position(GTK_EDITABLE(textview->text));

	if (event && 
	    ((event->button == 1 && textview->last_buttonpress == GDK_2BUTTON_PRESS)
	     || event->button == 2 || event->button == 3)) {
		GSList *cur;

		/* double click seems to set the cursor after the current
		 * word. The cursor position needs fixing, otherwise the
		 * last word of a clickable zone will not work */
		if (event->button == 1 && textview->last_buttonpress == GDK_2BUTTON_PRESS) {
		        textview->cur_pos--;
		}

		for (cur = textview->uri_list; cur != NULL; cur = cur->next) {
			RemoteURI *uri = (RemoteURI *)cur->data;

			if (textview->cur_pos >= uri->start &&
			    textview->cur_pos <  uri->end) {
				if (!g_strncasecmp(uri->uri, "mailto:", 7)) {
					if (event->button == 3) {
						gchar *fromname, *fromaddress;
						GdkEventButton tmpev;	
						
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
						FolderItem   *folder_item;

						if (textview->messageview && textview->messageview->mainwin 
						&&  textview->messageview->mainwin->summaryview 
						&&  textview->messageview->mainwin->summaryview->folder_item) {
							folder_item = textview->messageview->mainwin->summaryview->folder_item;
							if (folder_item->prefs && folder_item->prefs->enable_default_account)
								account = account_find_from_id(folder_item->prefs->default_account);
						}
						compose_new_with_recipient
							(account, uri->uri + 7);
					}
				} else {
					open_uri(uri->uri,
						 prefs_common.uri_cmd);
				}
			}
		}
	}
	if (event)
		textview->last_buttonpress = event->type;
	return FALSE;
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
