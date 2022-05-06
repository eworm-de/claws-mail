/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2021 The Claws Mail Team
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

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"
#include "mainwindow.h"
#include "colorlabel.h"

#define SAFE_STRING(str) \
	(str) ? (str) : ""

static struct MessageColorButtons {
	/* program colors */
	GtkWidget *color[COL_LAST_COLOR_INDEX];
	/* custom colors */
	GtkWidget *custom_color[COLORLABELS];
} color_buttons;

typedef struct _MsgColorsPage
{
	PrefsPage page;

	GtkWidget *window;
	
	GtkWidget *checkbtn_enable_colors;
	GtkWidget *checkbtn_enable_bgcolors;
	GtkWidget *checkbtn_recycle_colors;

	/* custom colors */
	GtkWidget *entry_custom_colorlabel[COLORLABELS];
} MsgColorsPage;

static void prefs_msg_colors_reset_custom_colors(GtkWidget *widget,
						 gpointer	 data);

#define COLOR_BUTTON_PACK_START(gtkbox, colorid, text) \
	color_buttons.color[colorid] = gtk_color_button_new_with_rgba( \
					&prefs_common.color[colorid]); \
	gtk_color_button_set_title(GTK_COLOR_BUTTON(color_buttons.color[colorid]), text); \
	gtk_widget_show(color_buttons.color[colorid]); \
	gtk_box_pack_start(GTK_BOX(gtkbox), color_buttons.color[colorid], FALSE, FALSE, 0); \
	gtk_widget_set_tooltip_text(GTK_WIDGET(color_buttons.color[colorid]), text)

#define COLOR_BUTTON_PACK_END(gtkbox, colorid, text) \
	color_buttons.color[colorid] = gtk_color_button_new_with_rgba( \
					&prefs_common.color[colorid]); \
	gtk_color_button_set_title(GTK_COLOR_BUTTON(color_buttons.color[colorid]), text); \
	gtk_widget_show(color_buttons.color[colorid]); \
	gtk_box_pack_end(GTK_BOX(gtkbox), color_buttons.color[colorid], FALSE, FALSE, 0); \
	gtk_widget_set_tooltip_text(GTK_WIDGET(color_buttons.color[colorid]), text)

#define COLOR_LABEL_PACK_START(gtkbox, colorid, labeltext) \
	label[colorid] = gtk_label_new (labeltext); \
	gtk_widget_show (label[colorid]); \
	gtk_box_pack_start (GTK_BOX(gtkbox), label[colorid], FALSE, FALSE, 0)

#define COLOR_LABEL_PACK_END(gtkbox, colorid, labeltext) \
	label[colorid] = gtk_label_new (labeltext); \
	gtk_widget_show (label[colorid]); \
	gtk_box_pack_end (GTK_BOX(gtkbox), label[colorid], FALSE, FALSE, 0)

static void prefs_msg_colors_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	    gpointer data)
{
	MsgColorsPage *prefs_msg_colors = (MsgColorsPage *) _page;
	
	GtkWidget *notebook;
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *checkbtn_enable_colors;
	GtkWidget *label_quote_level1;
	GtkWidget *label_quote_level2;
	GtkWidget *label_quote_level3;
	GtkWidget *checkbtn_enable_bgcolors;
	GtkWidget *checkbtn_recycle_colors;
	GtkWidget *hbox;
	GtkWidget *label[COL_LAST_COLOR_INDEX];
	GtkWidget *frame_msg;
	GtkWidget *frame_folder;
	GtkWidget *frame_quote;
	GtkWidget *vbox3;
	GtkWidget *hbox_quote;
	GtkWidget *vbox_quotefg;
	GtkWidget *vbox_quotebg;
	GtkWidget *frame_diff;
	GtkWidget *vbox4;
	/* custom colors */
	GtkWidget *hbox_custom_colors;
	GtkWidget *vbox_custom_colors;
	GtkWidget *vbox_custom_colors1;
	GtkWidget *vbox_custom_colors2;
 	GtkWidget *hbox_reset_custom_colors;
	GtkWidget *btn_reset_custom_colors;
	GtkWidget *hbox_custom_color[COLORLABELS];
	GtkWidget *entry_custom_colorlabel[COLORLABELS];
	gint c;
	gchar *tooltip_btn_text = NULL;
	gchar *tooltip_entry_text = NULL;
	gchar *title = NULL;

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	
	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VBOX_BORDER);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1,
				 gtk_label_new(_("Other")));

	vbox2 = gtkut_get_options_frame(vbox1, &frame_msg, _("Message view"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);
	PACK_CHECK_BUTTON (hbox, checkbtn_enable_colors,
			   _("Enable coloration of message text"));	

	hbox_quote = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox_quote);
	vbox_quotefg = gtk_box_new(GTK_ORIENTATION_VERTICAL, VBOX_BORDER);
	gtk_widget_show (vbox_quotefg);
	vbox_quotebg = gtk_box_new(GTK_ORIENTATION_VERTICAL, VBOX_BORDER);
	gtk_widget_show (vbox_quotebg);
	vbox3 = gtkut_get_options_frame(vbox2, &frame_quote, _("Quote"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, frame_quote);

	gtk_box_pack_start (GTK_BOX (vbox3), hbox_quote, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox_quote), vbox_quotefg, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox_quote), vbox_quotebg, FALSE, TRUE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotefg), hbox, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON (hbox, checkbtn_recycle_colors,
			   _("Cycle quote colors"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, checkbtn_recycle_colors);

	CLAWS_SET_TIP(checkbtn_recycle_colors,
			     _("If there are more than 3 quote levels, the colors will be reused"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotefg), hbox, FALSE, TRUE, 0);

	label_quote_level1 = gtk_label_new (_("1st Level"));
	gtk_widget_show(label_quote_level1);
  	gtk_box_pack_start (GTK_BOX(hbox), label_quote_level1, 
			    FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label_quote_level1);

	COLOR_LABEL_PACK_END(hbox, COL_QUOTE_LEVEL1, _("Text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label[COL_QUOTE_LEVEL1]);

	COLOR_BUTTON_PACK_END(hbox, COL_QUOTE_LEVEL1,
			      C_("Tooltip", "Pick color for 1st level text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors,
			       color_buttons.color[COL_QUOTE_LEVEL1]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotefg), hbox, FALSE, TRUE, 0);

	label_quote_level2 = gtk_label_new (_("2nd Level"));
	gtk_widget_show(label_quote_level2);
  	gtk_box_pack_start (GTK_BOX(hbox), label_quote_level2, 
			    FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label_quote_level2);

	COLOR_LABEL_PACK_END(hbox, COL_QUOTE_LEVEL2, _("Text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label[COL_QUOTE_LEVEL2]);

	COLOR_BUTTON_PACK_END(hbox, COL_QUOTE_LEVEL2,
			      C_("Tooltip", "Pick color for 2nd level text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors,
			       color_buttons.color[COL_QUOTE_LEVEL2]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotefg), hbox, FALSE, TRUE, 0);

	label_quote_level3 = gtk_label_new (_("3rd Level"));
	gtk_widget_show(label_quote_level3);
  	gtk_box_pack_start (GTK_BOX(hbox), label_quote_level3, 
			    FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label_quote_level3);

	COLOR_LABEL_PACK_END(hbox, COL_QUOTE_LEVEL3, _("Text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label[COL_QUOTE_LEVEL3]);

	COLOR_BUTTON_PACK_END(hbox, COL_QUOTE_LEVEL3,
			      C_("Tooltip", "Pick color for 3rd level text"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors,
			       color_buttons.color[COL_QUOTE_LEVEL3]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotebg), hbox, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON (hbox, checkbtn_enable_bgcolors,
			   _("Enable coloration of text background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, checkbtn_enable_bgcolors);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotebg), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_QUOTE_LEVEL1_BG,
				C_("Tooltip and Dialog title",
				   "Pick color for 1st level text background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors,
			       color_buttons.color[COL_QUOTE_LEVEL1_BG]);

	COLOR_LABEL_PACK_START(hbox, COL_QUOTE_LEVEL1_BG, _("Background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors, label[COL_QUOTE_LEVEL1_BG]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotebg), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_QUOTE_LEVEL2_BG,
				C_("Tooltip and Dialog title",
				   "Pick color for 2nd level text background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors,
			       color_buttons.color[COL_QUOTE_LEVEL2_BG]);

	COLOR_LABEL_PACK_START(hbox, COL_QUOTE_LEVEL2_BG, _("Background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors, label[COL_QUOTE_LEVEL2_BG]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, hbox);
	gtk_box_pack_start (GTK_BOX (vbox_quotebg), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_QUOTE_LEVEL3_BG,
				C_("Tooltip and Dialog title",
				   "Pick color for 3rd level text background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors,
			       color_buttons.color[COL_QUOTE_LEVEL3_BG]);

	COLOR_LABEL_PACK_START(hbox, COL_QUOTE_LEVEL3_BG, _("Background"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_bgcolors, label[COL_QUOTE_LEVEL3_BG]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_URI,
				C_("Tooltip and Dialog title",
				   "Pick color for links"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors,
			       color_buttons.color[COL_URI]);

	COLOR_LABEL_PACK_START(hbox, COL_URI, _("URI link"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label[COL_URI]);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_SIGNATURE,
				C_("Tooltip and Dialog title",
				   "Pick color for signatures"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors,
			       color_buttons.color[COL_SIGNATURE]);

	COLOR_LABEL_PACK_START(hbox, COL_SIGNATURE, _("Signatures"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, label[COL_SIGNATURE]);

	vbox4 = gtkut_get_options_frame(vbox2, &frame_diff,
	/* TRANSLATORS: A patch is a text file listing the differences between 2 or more different */
	/* versions of the same text file */
			_("Patch messages and attachments"));
	SET_TOGGLE_SENSITIVITY(checkbtn_enable_colors, frame_diff);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_DIFF_ADDED,
				C_("Tooltip and Dialog title",
				   "Pick color for inserted lines"));

	COLOR_LABEL_PACK_START(hbox, COL_DIFF_ADDED, _("Inserted lines"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_DIFF_DELETED,
				C_("Tooltip and Dialog title",
				   "Pick color for removed lines"));

	COLOR_LABEL_PACK_START(hbox, COL_DIFF_DELETED,
			       _("Removed lines"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_DIFF_HUNK,
	/* TRANSLATORS: A hunk is a section of the patch indicating how the files differ */
				C_("Tooltip and Dialog title",
				   "Pick color for hunk lines"));

	COLOR_LABEL_PACK_START(hbox, COL_DIFF_HUNK,
	/* TRANSLATORS: A hunk is a section of the patch indicating how the files differ */
			       _("Hunk lines"));

	vbox2 = gtkut_get_options_frame(vbox1, &frame_folder, _("Folder list"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, TRUE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_TGT_FOLDER,
				C_("Tooltip and Dialog title",
				   "Pick color for Target folder."));
	COLOR_LABEL_PACK_START(hbox, COL_TGT_FOLDER, _("Target folder"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(label[COL_TGT_FOLDER]), 
				    C_("Tooltip", "Target folder is used when the option "
				       "'Execute immediately when moving or "
				       "deleting messages' is turned off"));


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	COLOR_BUTTON_PACK_START(hbox, COL_NEW,
				C_("Tooltip and Dialog title",
				   "Pick color for folders containing new messages"));

	COLOR_LABEL_PACK_START(hbox, COL_NEW,
			       _("Folder containing new messages"));

	/* custom colors */
	vbox_custom_colors = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_widget_show (vbox_custom_colors);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_custom_colors), VBOX_BORDER);
	gtk_notebook_prepend_page(GTK_NOTEBOOK(notebook), vbox_custom_colors,
				 gtk_label_new(_("Color labels")));

	hbox_custom_colors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_custom_colors);
	gtk_box_pack_start(GTK_BOX (vbox_custom_colors), hbox_custom_colors,
				   FALSE, TRUE, 0);

	vbox_custom_colors1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_widget_show (vbox_custom_colors1);
	gtk_box_pack_start (GTK_BOX (hbox_custom_colors), vbox_custom_colors1, FALSE, FALSE, 0);

	vbox_custom_colors2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_widget_show (vbox_custom_colors2);
	gtk_box_pack_start (GTK_BOX (hbox_custom_colors), vbox_custom_colors2, FALSE, FALSE, 0);

	for (c = 0; c < (COLORLABELS>>1)+(COLORLABELS&1); c++) {
		/* TRANSLATORS: 'color %d' refers to the filtering/processing 
		   rule name and should not be translated */
		tooltip_btn_text = g_strdup_printf(C_("Tooltip", "Pick color for 'color %d'"), c+1);

		/* TRANSLATORS: 'color %d' refers to the filtering/processing 
		   rule name and should not be translated */
		tooltip_entry_text = g_strdup_printf(_("Set label for 'color %d'"), c+1);

		hbox_custom_color[c] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
		gtk_widget_show(hbox_custom_color[c]);
		gtk_box_pack_start(GTK_BOX (vbox_custom_colors1), hbox_custom_color[c],
				   FALSE, TRUE, 0);

		color_buttons.custom_color[c] = gtk_color_button_new_with_rgba(
						&prefs_common.custom_colorlabel[c].color);
		title = g_strdup_printf(C_("Dialog title", "Pick color for 'color %d'"), c+1);
		gtk_color_button_set_title(GTK_COLOR_BUTTON(color_buttons.custom_color[c]),
				   	   title);
		g_free(title);
		gtk_widget_show(color_buttons.custom_color[c]);
  		gtk_box_pack_start(GTK_BOX (hbox_custom_color[c]), color_buttons.custom_color[c],
				   FALSE, FALSE, 0);

		CLAWS_SET_TIP(color_buttons.custom_color[c],
			     	     tooltip_btn_text);
		g_free(tooltip_btn_text);

		entry_custom_colorlabel[c] = gtk_entry_new();
		gtk_widget_show (entry_custom_colorlabel[c]);
  		gtk_box_pack_start(GTK_BOX (hbox_custom_color[c]), entry_custom_colorlabel[c],
				   FALSE, FALSE, 0);
		CLAWS_SET_TIP(entry_custom_colorlabel[c],
			     	     tooltip_entry_text);
		g_free(tooltip_entry_text);
	}

	for (c = (COLORLABELS>>1)+(COLORLABELS&1); c < COLORLABELS; c++) {
		/* TRANSLATORS: 'color %d' refers to the filtering/processing 
		   rule name and should not be translated */
		tooltip_btn_text = g_strdup_printf(C_("Tooltip", "Pick color for 'color %d'"), c+1);

		/* TRANSLATORS: 'color %d' refers to the filtering/processing 
		   rule name and should not be translated */
		tooltip_entry_text = g_strdup_printf(_("Set label for 'color %d'"), c+1);

		hbox_custom_color[c] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
		gtk_widget_show(hbox_custom_color[c]);
		gtk_box_pack_start(GTK_BOX (vbox_custom_colors2), hbox_custom_color[c],
				   FALSE, TRUE, 0);

		color_buttons.custom_color[c] = gtk_color_button_new_with_rgba(
						&prefs_common.custom_colorlabel[c].color);
		title = g_strdup_printf(C_("Dialog title", "Pick color for 'color %d'"), c+1);
		gtk_color_button_set_title(GTK_COLOR_BUTTON(color_buttons.custom_color[c]),
				   	   title);
		g_free(title);
		gtk_widget_show(color_buttons.custom_color[c]);
  		gtk_box_pack_start(GTK_BOX (hbox_custom_color[c]), color_buttons.custom_color[c],
				   FALSE, FALSE, 0);
		CLAWS_SET_TIP(color_buttons.custom_color[c],
			     	     tooltip_btn_text);
		g_free(tooltip_btn_text);

		entry_custom_colorlabel[c] = gtk_entry_new();
		gtk_widget_show (entry_custom_colorlabel[c]);
  		gtk_box_pack_start(GTK_BOX (hbox_custom_color[c]), entry_custom_colorlabel[c],
				   FALSE, FALSE, 0);
		CLAWS_SET_TIP(entry_custom_colorlabel[c],
			     	     tooltip_entry_text);

		g_free(tooltip_entry_text);
	}

	hbox_reset_custom_colors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, VBOX_BORDER);
	gtk_widget_show (hbox_reset_custom_colors);
	gtk_box_pack_start(GTK_BOX (vbox_custom_colors), hbox_reset_custom_colors,
			   FALSE, FALSE, 0);

	btn_reset_custom_colors = gtk_button_new_with_label(_(" Use default "));
	gtk_widget_show(btn_reset_custom_colors);
	gtk_box_pack_start(GTK_BOX(hbox_reset_custom_colors), btn_reset_custom_colors,
		FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(btn_reset_custom_colors), "clicked",
			 G_CALLBACK(prefs_msg_colors_reset_custom_colors), prefs_msg_colors);

	for (c = 0; c < COLORLABELS; c++) {
		gtk_entry_set_text(GTK_ENTRY (entry_custom_colorlabel[c]), 
				   gettext(SAFE_STRING (prefs_common.custom_colorlabel[c].label)));
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_colors),
				     prefs_common.enable_color);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_bgcolors),
				     prefs_common.enable_bgcolor);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_recycle_colors),
				     prefs_common.recycle_quote_colors);

	prefs_msg_colors->checkbtn_enable_colors 	= checkbtn_enable_colors;
	prefs_msg_colors->checkbtn_enable_bgcolors 	= checkbtn_enable_bgcolors;
	prefs_msg_colors->checkbtn_recycle_colors	= checkbtn_recycle_colors;

	for (c = 0; c < COLORLABELS; c++) {
		prefs_msg_colors->entry_custom_colorlabel[c] = entry_custom_colorlabel[c];
	}
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
		
	prefs_msg_colors->page.widget = notebook;
}

#undef COLOR_BUTTON_PACK_START
#undef COLOR_BUTTON_PACK_END
#undef COLOR_LABEL_PACK_START
#undef COLOR_LABEL_PACK_END

#define COLOR_OTHER_SAVE(colorid) \
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_buttons.color[colorid]), \
				   &prefs_common.color[colorid])

static void prefs_msg_colors_save(PrefsPage *_page)
{
	MsgColorsPage *page = (MsgColorsPage *) _page;
	gint c;

	prefs_common.enable_color = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_colors));
	prefs_common.enable_bgcolor = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_bgcolors));
	prefs_common.recycle_quote_colors =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_recycle_colors));

	/* custom colors */
	for (c = 0; c < COLORLABELS; c++) {
		g_free(prefs_common.custom_colorlabel[c].label);
		prefs_common.custom_colorlabel[c].label =
			gtk_editable_get_chars(GTK_EDITABLE(page->entry_custom_colorlabel[c]), 0, -1);
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_buttons.custom_color[c]),
				   &prefs_common.custom_colorlabel[c].color);
	}
	colorlabel_update_colortable_from_prefs();
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL1);
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL2);
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL3);
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL1_BG);
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL2_BG);
	COLOR_OTHER_SAVE(COL_QUOTE_LEVEL3_BG);
	COLOR_OTHER_SAVE(COL_URI);
	COLOR_OTHER_SAVE(COL_SIGNATURE);
	COLOR_OTHER_SAVE(COL_DIFF_ADDED);
	COLOR_OTHER_SAVE(COL_DIFF_DELETED);
	COLOR_OTHER_SAVE(COL_DIFF_HUNK);
	COLOR_OTHER_SAVE(COL_TGT_FOLDER);
	COLOR_OTHER_SAVE(COL_NEW);

	main_window_reflect_prefs_all();
	main_window_reflect_prefs_custom_colors(mainwindow_get_mainwindow());
}
#undef COLOR_OTHER_SAVE

static void prefs_msg_colors_reset_custom_colors(GtkWidget *widget, gpointer data)
{
	MsgColorsPage *page = (MsgColorsPage *) data;
	GdkRGBA rgba;
	gint c;

	for (c = 0; c < COLORLABELS; c++) {
		rgba = colorlabel_get_default_color(c);
		prefs_common.custom_colorlabel[c].color = rgba;
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_buttons.custom_color[c]), &rgba);
		gtk_entry_set_text(GTK_ENTRY(page->entry_custom_colorlabel[c]),
						gettext(SAFE_STRING (colorlabel_get_color_default_text(c))));
	}
}

static void prefs_msg_colors_destroy_widget(PrefsPage *_page)
{
}

MsgColorsPage *prefs_msg_colors;

void prefs_msg_colors_init(void)
{
	MsgColorsPage *page;
	static gchar *path[3];

	path[0] = _("Display");
	path[1] = _("Colors");
	path[2] = NULL;

	page = g_new0(MsgColorsPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_msg_colors_create_widget;
	page->page.destroy_widget = prefs_msg_colors_destroy_widget;
	page->page.save_page = prefs_msg_colors_save;
	page->page.weight = 165.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_msg_colors = page;
}

void prefs_msg_colors_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_msg_colors);
	g_free(prefs_msg_colors);
}

