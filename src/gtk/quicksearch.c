/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"
#include "menu.h"
#include "prefs_common.h"
#include "description_window.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "quicksearch.h"
#include "folderview.h"
#include "folder.h"

struct _QuickSearch
{
	GtkWidget			*hbox_search;
	GtkWidget			*search_type;
	GtkWidget			*search_type_opt;
	GtkWidget			*search_string_entry;
	GtkWidget			*search_description;

	gboolean			 active;
	gchar				*search_string;
	MatcherList			*matcher_list;

	QuickSearchExecuteCallback	 callback;
	gpointer			 callback_data;
	gboolean			 running;
	gboolean			 has_focus;
	FolderItem			*root_folder_item;
};

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run);
static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active);
static void quicksearch_reset_folder_items(QuickSearch *quicksearch, FolderItem *folder_item);

static void prepare_matcher(QuickSearch *quicksearch)
{
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	if (quicksearch->matcher_list != NULL) {
		matcherlist_free(quicksearch->matcher_list);
		quicksearch->matcher_list = NULL;
	}

	if (search_string == NULL || search_string[0] == '\0') {
		quicksearch_set_active(quicksearch, FALSE);
		return;
	}

	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
		char *newstr = NULL;

		newstr = expand_search_string(search_string);
		if (newstr && newstr[0] != '\0') {
			quicksearch->matcher_list = matcher_parser_get_cond(newstr);
			g_free(newstr);
		} else {
			quicksearch->matcher_list = NULL;
			quicksearch_set_active(quicksearch, FALSE);

			return;
		}
	} else {
		if (quicksearch->search_string != NULL)
			g_free(quicksearch->search_string);
		quicksearch->search_string = g_strdup(search_string);
	}

	quicksearch_set_active(quicksearch, TRUE);
}

static void update_extended_button (QuickSearch *quicksearch)
{
	GtkWidget *btn = quicksearch->search_description;
	
	g_return_if_fail(btn != NULL);
		
	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
		gtk_button_set_label(GTK_BUTTON(btn), _("Extended symbols"));
		gtk_widget_show(btn);
	} else {
		gtk_widget_hide(btn);
	}
	
}

static gboolean searchbar_focus_evt(GtkWidget *widget, GdkEventFocus *event,
			      	  QuickSearch *quicksearch)
{
	quicksearch->has_focus = (event && event->in);
	return FALSE;
}

gboolean quicksearch_has_focus(QuickSearch *quicksearch)
{
	return quicksearch->has_focus;
}

static gboolean searchbar_pressed(GtkWidget *widget, GdkEventKey *event,
			      	  QuickSearch *quicksearch)
{
	if (event != NULL && event->keyval == GDK_Escape) {
		quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
		gtk_widget_grab_focus(GTK_WIDGET(GTK_COMBO(quicksearch->search_string_entry)->entry));
		return TRUE;
	}

	if (event != NULL && event->keyval == GDK_Return) {
		const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

		if (search_string && strlen(search_string) != 0) {
			prefs_common.summary_quicksearch_history =
				add_history(prefs_common.summary_quicksearch_history,
					    search_string);
			gtk_combo_set_popdown_strings(GTK_COMBO(quicksearch->search_string_entry), 
				prefs_common.summary_quicksearch_history);			
		}

		prepare_matcher(quicksearch);

		quicksearch_set_running(quicksearch, TRUE);
		if (quicksearch->callback != NULL)
			quicksearch->callback(quicksearch, quicksearch->callback_data);
		quicksearch_set_running(quicksearch, FALSE);
	 	g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
		gtk_widget_grab_focus(GTK_WIDGET(GTK_COMBO(
			quicksearch->search_string_entry)->entry));
		return TRUE;
	}

	return FALSE; 		
}

static gboolean searchtype_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;

	prefs_common.summary_quicksearch_type = GPOINTER_TO_INT(g_object_get_data(
				   G_OBJECT(GTK_MENU_ITEM(gtk_menu_get_active(
				   GTK_MENU(quicksearch->search_type)))), MENU_VAL_ID));

	/* Show extended search description button, only when Extended is selected */
	update_extended_button(quicksearch);

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
	return TRUE;
}

static gboolean searchtype_recursive_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	gboolean checked = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	
	prefs_common.summary_quicksearch_recurse = checked; 

	/* reselect the search type */
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt), 
				    prefs_common.summary_quicksearch_type);

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
	return TRUE;
}

/*
 * Strings describing how to use Extended Search
 * 
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *search_descr_strings[] = {
	"a",	 N_("all messages"),
	"ag #",  N_("messages whose age is greater than #"),
	"al #",  N_("messages whose age is less than #"),
	"b S",	 N_("messages which contain S in the message body"),
	"B S",	 N_("messages which contain S in the whole message"),
	"c S",	 N_("messages carbon-copied to S"),
	"C S",	 N_("message is either to: or cc: to S"),
	"D",	 N_("deleted messages"), /** how I can filter deleted messages **/
	"e S",	 N_("messages which contain S in the Sender field"),
	"E S",	 N_("true if execute \"S\" succeeds"),
	"f S",	 N_("messages originating from user S"),
	"F",	 N_("forwarded messages"),
	"h S",	 N_("messages which contain header S"),
	"i S",	 N_("messages which contain S in Message-ID header"),
	"I S",	 N_("messages which contain S in inreplyto header"),
	"L",	 N_("locked messages"),
	"n S",	 N_("messages which are in newsgroup S"),
	"N",	 N_("new messages"),
	"O",	 N_("old messages"),
	"p",	 N_("incomplete messages (not entirely downloaded)"),
	"r",	 N_("messages which have been replied to"),
	"R",	 N_("read messages"),
	"s S",	 N_("messages which contain S in subject"),
	"se #",  N_("messages whose score is equal to #"),
	"sg #",  N_("messages whose score is greater than #"),
	"sl #",  N_("messages whose score is lower than #"),
	"Se #",  N_("messages whose size is equal to #"),
	"Sg #",  N_("messages whose size is greater than #"),
	"Ss #",  N_("messages whose size is smaller than #"),
	"t S",	 N_("messages which have been sent to S"),
	"T",	 N_("marked messages"),
	"U",	 N_("unread messages"),
	"x S",	 N_("messages which contain S in References header"),
	"X cmd", N_("messages returning 0 when passed to command"),
	"y S",	 N_("messages which contain S in X-Label header"),
	"",	 "" ,
	"&",	 N_("logical AND operator"),
	"|",	 N_("logical OR operator"),
	"! or ~",	N_("logical NOT operator"),
	"%",	 N_("case sensitive search"),
	"",	 "" ,
	" ",	 N_("all filtering expressions are allowed"),
	NULL,	 NULL 
};
 
static DescriptionWindow search_descr = {
	NULL,
	NULL, 
	2,
	N_("Extended Search symbols"),
	search_descr_strings
};
	
static void search_description_cb(GtkWidget *widget)
{
	description_window_create(&search_descr);
};

static gboolean clear_search_cb(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	
	quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
	
	return TRUE;
};

QuickSearch *quicksearch_new()
{
	QuickSearch *quicksearch;

	GtkWidget *hbox_search;
	GtkWidget *search_type_opt;
	GtkWidget *search_type;
	GtkWidget *search_string_entry;
	GtkWidget *search_hbbox;
	GtkWidget *search_description;
	GtkWidget *clear_search;
	GtkWidget *menuitem;

	quicksearch = g_new0(QuickSearch, 1);

	/* quick search */
	hbox_search = gtk_hbox_new(FALSE, 0);

	search_type_opt = gtk_option_menu_new();
	gtk_widget_show(search_type_opt);
	gtk_box_pack_start(GTK_BOX(hbox_search), search_type_opt, FALSE, FALSE, 0);

	search_type = gtk_menu_new();
	MENUITEM_ADD (search_type, menuitem, _("Subject"), QUICK_SEARCH_SUBJECT);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("From"), QUICK_SEARCH_FROM);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("To"), QUICK_SEARCH_TO);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("Extended"), QUICK_SEARCH_EXTENDED);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);

	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), gtk_separator_menu_item_new());
	
	menuitem = gtk_check_menu_item_new_with_label(_("Recursive"));
	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), menuitem);
	
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					prefs_common.summary_quicksearch_recurse);
	
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_recursive_changed),
			 quicksearch);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(search_type_opt), search_type);
	
	gtk_option_menu_set_history(GTK_OPTION_MENU(search_type_opt), prefs_common.summary_quicksearch_type);
	
	gtk_widget_show(search_type);
	
	search_string_entry = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox_search), search_string_entry, FALSE, FALSE, 2);
	gtk_combo_set_value_in_list(GTK_COMBO(search_string_entry), FALSE, TRUE);
	gtk_combo_set_case_sensitive(GTK_COMBO(search_string_entry), TRUE);
	if (prefs_common.summary_quicksearch_history) 
		gtk_combo_set_popdown_strings(GTK_COMBO(search_string_entry), 
			prefs_common.summary_quicksearch_history);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(search_string_entry)->entry), "");
	gtk_widget_show(search_string_entry);

	search_hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(search_hbbox), 	
				  GTK_BUTTONBOX_START);

	gtk_box_set_spacing(GTK_BOX(search_hbbox), 5);
		
	if (prefs_common.summary_quicksearch_sticky) {
		clear_search = gtk_button_new_with_label(_("Clear"));
		gtk_box_pack_start(GTK_BOX(search_hbbox), clear_search,
				   FALSE, FALSE, 0);
		gtk_widget_set_size_request(clear_search, 120, -1);
		g_signal_connect(G_OBJECT(clear_search), "clicked",
				 G_CALLBACK(clear_search_cb), quicksearch);
		gtk_widget_show(clear_search);
	}

	search_description = gtk_button_new_with_label(_("Extended Symbols"));
	gtk_box_pack_start(GTK_BOX(search_hbbox), search_description,
			   TRUE, TRUE, 0);
	gtk_widget_show(search_description);
		
	g_signal_connect(G_OBJECT(search_description), "clicked",
			 G_CALLBACK(search_description_cb), NULL);

	gtk_box_pack_start(GTK_BOX(hbox_search), search_hbbox, FALSE, FALSE, 2);				
	gtk_widget_show(search_hbbox);
	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED)
		gtk_widget_show(search_description);
	else
		gtk_widget_hide(search_description);
	
	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry), 
			   "key_press_event",
			   G_CALLBACK(searchbar_pressed),
			   quicksearch);

	
	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			 "focus_in_event",
			 G_CALLBACK(searchbar_focus_evt),
			 quicksearch);

	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			 "focus_out_event",
			 G_CALLBACK(searchbar_focus_evt),
			 quicksearch);
	

	quicksearch->hbox_search = hbox_search;
	quicksearch->search_type = search_type;
	quicksearch->search_type_opt = search_type_opt;
	quicksearch->search_string_entry = search_string_entry;
	quicksearch->search_description = search_description;
	quicksearch->matcher_list = NULL;
	quicksearch->active = FALSE;
	quicksearch->running = FALSE;
	
	update_extended_button(quicksearch);
	
	return quicksearch;
}

GtkWidget *quicksearch_get_widget(QuickSearch *quicksearch)
{
	return quicksearch->hbox_search;
}

void quicksearch_show(QuickSearch *quicksearch)
{
	prepare_matcher(quicksearch);
	gtk_widget_show(quicksearch->hbox_search);
	update_extended_button(quicksearch);
	gtk_widget_grab_focus(
		GTK_WIDGET(GTK_COMBO(quicksearch->search_string_entry)->entry));
}

void quicksearch_hide(QuickSearch *quicksearch)
{
	quicksearch_set_active(quicksearch, FALSE);
	gtk_widget_hide(quicksearch->hbox_search);
}

void quicksearch_set(QuickSearch *quicksearch, QuickSearchType type,
		     const gchar *matchstring)
{
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    type);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry),
			   matchstring);
	prefs_common.summary_quicksearch_type = type;

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);	
	quicksearch_set_running(quicksearch, FALSE);
}

gboolean quicksearch_is_active(QuickSearch *quicksearch)
{
	return quicksearch->active;
}

static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active)
{
	quicksearch->active = active;
	if (!active) {
		quicksearch_reset_cur_folder_item(quicksearch);
	}
}

void quicksearch_set_execute_callback(QuickSearch *quicksearch,
				      QuickSearchExecuteCallback callback,
				      gpointer data)
{
	quicksearch->callback = callback;
	quicksearch->callback_data = data;
}

gboolean quicksearch_match(QuickSearch *quicksearch, MsgInfo *msginfo)
{
	gchar *searched_header = NULL;

	if (!quicksearch->active)
		return TRUE;

	switch (prefs_common.summary_quicksearch_type) {
	case QUICK_SEARCH_SUBJECT:
		searched_header = msginfo->subject;
		break;
	case QUICK_SEARCH_FROM:
		searched_header = msginfo->from;
		break;
	case QUICK_SEARCH_TO:
		searched_header = msginfo->to;
		break;
	case QUICK_SEARCH_EXTENDED:
		break;
	default:
		debug_print("unknown search type (%d)\n", prefs_common.summary_quicksearch_type);
		break;
	}

	if (prefs_common.summary_quicksearch_type != QUICK_SEARCH_EXTENDED && 
	    quicksearch->search_string &&
            searched_header && strcasestr(searched_header, quicksearch->search_string) != NULL)
		return TRUE;
	else if ((quicksearch->matcher_list != NULL) && 
	         matcherlist_match(quicksearch->matcher_list, msginfo))
		return TRUE;

	return FALSE;
}

/* allow Mutt-like patterns in quick search */
gchar *expand_search_string(const gchar *search_string)
{
	int i = 0;
	gchar term_char, save_char;
	gchar *cmd_start, *cmd_end;
	GString *matcherstr;
	gchar *returnstr = NULL;
	gchar *copy_str;
	gboolean casesens, dontmatch;
	/* list of allowed pattern abbreviations */
	struct {
		gchar		*abbreviated;	/* abbreviation */
		gchar		*command;	/* actual matcher command */ 
		gint		numparams;	/* number of params for cmd */
		gboolean	qualifier;	/* do we append regexpcase */
		gboolean	quotes;		/* do we need quotes */
	}
	cmds[] = {
		{ "a",	"all",				0,	FALSE,	FALSE },
		{ "ag",	"age_greater",			1,	FALSE,	FALSE },
		{ "al",	"age_lower",			1,	FALSE,	FALSE },
		{ "b",	"body_part",			1,	TRUE,	TRUE  },
		{ "B",	"message",			1,	TRUE,	TRUE  },
		{ "c",	"cc",				1,	TRUE,	TRUE  },
		{ "C",	"to_or_cc",			1,	TRUE,	TRUE  },
		{ "D",	"deleted",			0,	FALSE,	FALSE },
		{ "e",	"header \"Sender\"",		1,	TRUE,	TRUE  },
		{ "E",	"execute",			1,	FALSE,	TRUE  },
		{ "f",	"from",				1,	TRUE,	TRUE  },
		{ "F",	"forwarded",			0,	FALSE,	FALSE },
		{ "h",	"headers_part",			1,	TRUE,	TRUE  },
		{ "i",	"header \"Message-ID\"",	1,	TRUE,	TRUE  },
		{ "I",	"inreplyto",			1,	TRUE,	TRUE  },
		{ "L",	"locked",			0,	FALSE,	FALSE },
		{ "n",	"newsgroups",			1,	TRUE,	TRUE  },
		{ "N",	"new",				0,	FALSE,	FALSE },
		{ "O",	"~new",				0,	FALSE,	FALSE },
		{ "r",	"replied",			0,	FALSE,	FALSE },
		{ "R",	"~unread",			0,	FALSE,	FALSE },
		{ "s",	"subject",			1,	TRUE,	TRUE  },
		{ "se",	"score_equal",			1,	FALSE,	FALSE },
		{ "sg",	"score_greater",		1,	FALSE,	FALSE },
		{ "sl",	"score_lower",			1,	FALSE,	FALSE },
		{ "Se",	"size_equal",			1,	FALSE,	FALSE },
		{ "Sg",	"size_greater",			1,	FALSE,	FALSE },
		{ "Ss",	"size_smaller",			1,	FALSE,	FALSE },
		{ "t",	"to",				1,	TRUE,	TRUE  },
		{ "T",	"marked",			0,	FALSE,	FALSE },
		{ "U",	"unread",			0,	FALSE,	FALSE },
		{ "x",	"header \"References\"",	1,	TRUE,	TRUE  },
		{ "X",  "test",				1,	FALSE,  FALSE }, 
		{ "y",	"header \"X-Label\"",		1,	TRUE,	TRUE  },
		{ "&",	"&",				0,	FALSE,	FALSE },
		{ "|",	"|",				0,	FALSE,	FALSE },
		{ "p",	"partial",			0,	FALSE, 	FALSE },
		{ NULL,	NULL,				0,	FALSE,	FALSE }
	};

	if (search_string == NULL)
		return NULL;

	copy_str = g_strdup(search_string);

	matcherstr = g_string_sized_new(16);
	cmd_start = copy_str;
	while (cmd_start && *cmd_start) {
		/* skip all white spaces */
		while (*cmd_start && isspace((guchar)*cmd_start))
			cmd_start++;
		cmd_end = cmd_start;

		/* extract a command */
		while (*cmd_end && !isspace((guchar)*cmd_end))
			cmd_end++;

		/* save character */
		save_char = *cmd_end;
		*cmd_end = '\0';

		dontmatch = FALSE;
		casesens = FALSE;

		/* ~ and ! mean logical NOT */
		if (*cmd_start == '~' || *cmd_start == '!')
		{
			dontmatch = TRUE;
			cmd_start++;
		}
		/* % means case sensitive match */
		if (*cmd_start == '%')
		{
			casesens = TRUE;
			cmd_start++;
		}

		/* find matching abbreviation */
		for (i = 0; cmds[i].command; i++) {
			if (!strcmp(cmd_start, cmds[i].abbreviated)) {
				/* restore character */
				*cmd_end = save_char;

				/* copy command */
				if (matcherstr->len > 0) {
					g_string_append(matcherstr, " ");
				}
				if (dontmatch)
					g_string_append(matcherstr, "~");
				g_string_append(matcherstr, cmds[i].command);
				g_string_append(matcherstr, " ");

				/* stop if no params required */
				if (cmds[i].numparams == 0)
					break;

				/* extract a parameter, allow quotes */
				while (*cmd_end && isspace((guchar)*cmd_end))
					cmd_end++;

				cmd_start = cmd_end;
				if (*cmd_start == '"') {
					term_char = '"';
					cmd_end++;
				}
				else
					term_char = ' ';

				/* extract actual parameter */
				while ((*cmd_end) && (*cmd_end != term_char))
					cmd_end++;

				if (*cmd_end == '"')
					cmd_end++;

				save_char = *cmd_end;
				*cmd_end = '\0';

				if (cmds[i].qualifier) {
					if (casesens)
						g_string_append(matcherstr, "regexp ");
					else
						g_string_append(matcherstr, "regexpcase ");
				}

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* copy actual parameter */
				g_string_append(matcherstr, cmd_start);

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* restore original character */
				*cmd_end = save_char;

				break;
			}
		}

		if (*cmd_end)
			cmd_end++;
		cmd_start = cmd_end;
	}

	g_free(copy_str);

	/* return search string if no match is found to allow 
	   all available filtering expressions in quicksearch */
	if (matcherstr->len > 0) returnstr = matcherstr->str;
	else returnstr = g_strdup(search_string);

	g_string_free(matcherstr, FALSE);
	return returnstr;
}

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run)
{
	quicksearch->running = run;
}

gboolean quicksearch_is_running(QuickSearch *quicksearch) 
{
	return quicksearch->running;
}

void quicksearch_pass_key(QuickSearch *quicksearch, guint val, GdkModifierType mod)
{
	GtkEntry *entry = GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry);
	gint curpos = gtk_editable_get_position(GTK_EDITABLE(entry));
	char *str = g_strdup(gtk_entry_get_text(entry));
	char *begin = str;
	char *end = NULL;
	char *new = NULL;
	
	if (mod == GDK_SHIFT_MASK)
		val = toupper(val);
	
	if (curpos < strlen(str)-1) {
		end = g_strdup(str+curpos);
		*(str+curpos) = '\0';
		new = g_strdup_printf("%s%c%s", begin, val, end);
		gtk_entry_set_text(entry, new);
		g_free(end);
	} else {
		new = g_strdup_printf("%s%c", begin, val);
		gtk_entry_set_text(entry, new);
	}
	g_free(str);
	g_free(new);
	gtk_editable_set_position(GTK_EDITABLE(entry), curpos+1);
	
}

static gboolean quicksearch_match_subfolder(QuickSearch *quicksearch, 
				 FolderItem *src)
{
	GSList *msglist = folder_item_get_msg_list(src);
	GSList *cur;
	gboolean result = FALSE;
	
	for (cur = msglist; cur != NULL; cur = cur->next) {
		MsgInfo *msg = (MsgInfo *)cur->data;
		if (quicksearch_match(quicksearch, msg)) {
			procmsg_msginfo_free(msg);
			result = TRUE;
			break;
		}
		procmsg_msginfo_free(msg);
	}

	g_slist_free(msglist);
	return result;
}

void quicksearch_search_subfolders(QuickSearch *quicksearch, 
				   FolderView *folderview,
				   FolderItem *folder_item)
{
	FolderItem *cur = NULL;
	GNode *node = folder_item->node->children;
	
	if (!prefs_common.summary_quicksearch_recurse)
		return;

	for (; node != NULL; node = node->next) {
		cur = FOLDER_ITEM(node->data);
		if (quicksearch_match_subfolder(quicksearch, cur)) {
			folderview_update_search_icon(cur, TRUE);
		} else {
			folderview_update_search_icon(cur, FALSE);
		}
		if (cur->node->children)
			quicksearch_search_subfolders(quicksearch,
						      folderview,
						      cur);
	}
	quicksearch->root_folder_item = folder_item;
}

static void quicksearch_reset_folder_items(QuickSearch *quicksearch,
				    FolderItem *folder_item)
{
	FolderItem *cur = NULL;
	GNode *node = (folder_item && folder_item->node) ? 
			folder_item->node->children : NULL;
	
	for (; node != NULL; node = node->next) {
		cur = FOLDER_ITEM(node->data);
		folderview_update_search_icon(cur, FALSE);
		if (cur->node->children)
			quicksearch_reset_folder_items(quicksearch,
						       cur);
	}
}

void quicksearch_reset_cur_folder_item(QuickSearch *quicksearch)
{
	if (quicksearch->root_folder_item)
		quicksearch_reset_folder_items(quicksearch, 
					       quicksearch->root_folder_item);
	
	quicksearch->root_folder_item = NULL;
}
