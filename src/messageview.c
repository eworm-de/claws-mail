/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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

#include "config.h"
#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "main.h"
#include "messageview.h"
#include "message_search.h"
#include "headerview.h"
#include "summaryview.h"
#include "textview.h"
#include "mimeview.h"
#include "menu.h"
#include "about.h"
#include "filesel.h"
#include "foldersel.h"
#include "sourcewindow.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "procmsg.h"
#include "procheader.h"
#include "procmime.h"
#include "account.h"
#include "action.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "gtkutils.h"
#include "utils.h"
#include "send_message.h"
#include "stock_pixmap.h"
#include "hooks.h"
#include "filtering.h"
#include "partial_download.h"
#include "uri_opener.h"
#include "inc.h"
#include "log.h"
#include "privacy.h"
#include "combobox.h"
#include "printing.h"
#include "quoted-printable.h"
#include "version.h"
#include "statusbar.h"
#include "folder_item_prefs.h"
#include "avatars.h"
#include "file-utils.h"

#ifndef USE_ALT_ADDRBOOK
	#include "addressbook.h"
#else
	#include "addressadd.h"
	#include "addressbook-dbus.h"
#endif
static GList *messageview_list = NULL;

static gint messageview_delete_cb	(GtkWidget		*widget,
					 GdkEventAny		*event,
					 MessageView		*messageview);
static void messageview_size_allocate_cb(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MessageView	*messageview);
static void return_receipt_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void return_receipt_send_clicked (NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void partial_recv_dload_clicked 	(NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_del_clicked 	(NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_unmark_clicked (NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void save_as_cb			(GtkAction	*action,
					 gpointer	 data);
static void page_setup_cb		(GtkAction	*action,
					 gpointer	 data);
static void print_cb			(GtkAction	*action,
					 gpointer	 data);
static void close_cb			(GtkAction	*action,
					 gpointer	 data);
static void copy_cb			(GtkAction	*action,
					 gpointer	 data);
static void allsel_cb			(GtkAction	*action,
					 gpointer	 data);
static void search_cb			(GtkAction	*action,
					 gpointer	 data);

static void prev_cb			(GtkAction	*action,
					 gpointer	 data);
static void next_cb			(GtkAction	*action,
					 gpointer	 data);
static void prev_unread_cb		(GtkAction	*action,
					 gpointer	 data);
static void next_unread_cb		(GtkAction	*action,
					 gpointer	 data);
static void prev_new_cb			(GtkAction	*action,
					 gpointer	 data);
static void next_new_cb			(GtkAction	*action,
					 gpointer	 data);
static void prev_marked_cb		(GtkAction	*action,
					 gpointer	 data);
static void next_marked_cb		(GtkAction	*action,
					 gpointer	 data);
static void prev_labeled_cb		(GtkAction	*action,
					 gpointer	 data);
static void next_labeled_cb		(GtkAction	*action,
					 gpointer	 data);
static void prev_history_cb		(GtkAction	*action,
					 gpointer	 data);
static void next_history_cb		(GtkAction	*action,
					 gpointer	 data);
static void parent_cb			(GtkAction	*action,
					 gpointer	 data);
static void goto_unread_folder_cb	(GtkAction	*action,
					 gpointer	 data);
static void goto_folder_cb		(GtkAction	*action,
					 gpointer	 data);

static void scroll_prev_line_cb         (GtkAction      *action,
                                         gpointer        data);
static void scroll_next_line_cb         (GtkAction      *action,
                                          gpointer        data);
static void scroll_next_page_cb         (GtkAction      *action,
                                         gpointer        data);
static void scroll_prev_page_cb         (GtkAction      *action,
                                         gpointer        data);

static void set_charset_cb		(GtkAction *action, GtkRadioAction *current, gpointer data);
static void set_decode_cb		(GtkAction *action, GtkRadioAction *current, gpointer data);

static void view_source_cb		(GtkAction	*action,
					 gpointer	 data);

static void show_all_header_cb		(GtkToggleAction	*action,
					 gpointer	 data);
static void msg_hide_quotes_cb		(GtkToggleAction	*action,
					 gpointer	 data);

static void compose_cb			(GtkAction	*action,
					 gpointer	 data);
static void reply_cb			(GtkAction	*action,
					 gpointer	 data);

static PrefsAccount *select_account_from_list
					(GList		*ac_list,
					 gboolean	 has_accounts);
static void addressbook_open_cb		(GtkAction	*action,
					 gpointer	 data);
static void add_address_cb		(GtkAction	*action,
					 gpointer	 data);
static void create_filter_cb		(GtkAction	*action,
					 gpointer	 data);
static void create_processing_cb	(GtkAction	*action,
					 gpointer	 data);
static void open_urls_cb		(GtkAction	*action,
					 gpointer	 data);

static void about_cb			(GtkAction	*action,
					 gpointer	 data);
static void messageview_update		(MessageView	*msgview,
					 MsgInfo	*old_msginfo);
static gboolean messageview_update_msg	(gpointer source, gpointer data);

static void save_part_as_cb(GtkAction *action, gpointer data);
static void view_part_as_text_cb(GtkAction *action, gpointer data);
static void open_part_cb(GtkAction *action, gpointer data);
#ifndef G_OS_WIN32
static void open_part_with_cb(GtkAction *action, gpointer data);
#endif
static void check_signature_cb(GtkAction *action, gpointer data);
static void goto_next_part_cb(GtkAction *action, gpointer data);
static void goto_prev_part_cb(GtkAction *action, gpointer data);

static void messageview_nothing_cb	   (GtkAction *action, gpointer data)
{

}

static GList *msgview_list = NULL;
static GtkActionEntry msgview_entries[] =
{
	{"Menu",                                     NULL, "Menu", NULL, NULL, NULL },
/* menus */
	{"File",                                     NULL, N_("_File"), NULL, NULL, NULL },
	{"Edit",                                     NULL, N_("_Edit"), NULL, NULL, NULL },
	{"View",                                     NULL, N_("_View"), NULL, NULL, NULL },
	{"Message",                                  NULL, N_("_Message"), NULL, NULL, NULL },
	{"Tools",                                    NULL, N_("_Tools"), NULL, NULL, NULL },
	{"Help",                                     NULL, N_("_Help"), NULL, NULL, NULL },
	{"PlaceHolder",                              NULL, "Placeholder", NULL, NULL, G_CALLBACK(messageview_nothing_cb) },

/* File menu */
	{"File/SaveAs",                              NULL, N_("_Save email as..."), "<control>S", NULL, G_CALLBACK(save_as_cb) },
	{"File/SavePartAs",                          NULL, N_("_Save part as..."), "Y", NULL, G_CALLBACK(save_part_as_cb) },
	{"File/PageSetup",                           NULL, N_("Page setup..."), NULL, NULL, G_CALLBACK(page_setup_cb) },
	{"File/Print",                               NULL, N_("_Print..."), "<control>P", NULL, G_CALLBACK(print_cb) },
	{"File/---",                                 NULL, "---", NULL, NULL, NULL },
	{"File/Close",                               NULL, N_("_Close"), "<control>W", NULL, G_CALLBACK(close_cb) },

/* Edit menu */
	{"Edit/Copy",                                NULL, N_("_Copy"), "<control>C", NULL, G_CALLBACK(copy_cb) },
	{"Edit/SelectAll",                           NULL, N_("_Select all"), "<control>A", NULL, G_CALLBACK(allsel_cb) },
	{"Edit/---",                                 NULL, "---", NULL, NULL, NULL },
	{"Edit/Find",                                NULL, N_("_Find"), "<control>F", NULL, G_CALLBACK(search_cb) },
	
/* View menu */
	{"View/Goto",                                NULL, N_("_Go to"), NULL, NULL, NULL },
	{"View/Goto/Prev",                           NULL, N_("_Previous message"), "P", NULL, G_CALLBACK(prev_cb) },
	{"View/Goto/Next",                           NULL, N_("_Next message"), "N", NULL, G_CALLBACK(next_cb) },
	{"View/Goto/---",                            NULL, "---", NULL, NULL, NULL },
	{"View/Goto/PrevUnread",                     NULL, N_("P_revious unread message"), "<shift>P", NULL, G_CALLBACK(prev_unread_cb) },
	{"View/Goto/NextUnread",                     NULL, N_("N_ext unread message"), "<shift>N", NULL, G_CALLBACK(next_unread_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevNew",                        NULL, N_("Previous ne_w message"), NULL, NULL, G_CALLBACK(prev_new_cb) },
	{"View/Goto/NextNew",                        NULL, N_("Ne_xt new message"), NULL, NULL, G_CALLBACK(next_new_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevMarked",                     NULL, N_("Previous _marked message"), NULL, NULL, G_CALLBACK(prev_marked_cb) },
	{"View/Goto/NextMarked",                     NULL, N_("Next m_arked message"), NULL, NULL, G_CALLBACK(next_marked_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevLabeled",                    NULL, N_("Previous _labeled message"), NULL, NULL, G_CALLBACK(prev_labeled_cb) },
	{"View/Goto/NextLabeled",                    NULL, N_("Next la_beled message"), NULL, NULL, G_CALLBACK(next_labeled_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/PrevHistory",                    NULL, N_("Previous opened message"), "<alt>Left", NULL, G_CALLBACK(prev_history_cb) },
	{"View/Goto/NextHistory",                    NULL, N_("Next opened message"), "<alt>Right", NULL, G_CALLBACK(next_history_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/ParentMessage",                  NULL, N_("Parent message"), "<control>Up", NULL, G_CALLBACK(parent_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/NextUnreadFolder",               NULL, N_("Next unread _folder"), "<shift>G", NULL, G_CALLBACK(goto_unread_folder_cb) },
	{"View/Goto/Folder",                         NULL, N_("F_older..."), "G", NULL, G_CALLBACK(goto_folder_cb) },
	/* {"View/Goto/---",                         NULL, "---", NULL, NULL, NULL }, */
	{"View/Goto/NextPart",                       NULL, N_("Next part"), "A", NULL, G_CALLBACK(goto_next_part_cb) },
	{"View/Goto/PrevPart",                       NULL, N_("Previous part"), "Z", NULL, G_CALLBACK(goto_prev_part_cb) },
	{"View/Scroll",                              NULL, N_("Message scroll"), NULL, NULL, NULL },
	{"View/Scroll/PrevLine",                     NULL, N_("Previous line"), NULL, NULL, G_CALLBACK(scroll_prev_line_cb) },
	{"View/Scroll/NextLine",                     NULL, N_("Next line"), NULL, NULL, G_CALLBACK(scroll_next_line_cb) },
	{"View/Scroll/PrevPage",                     NULL, N_("Previous page"), NULL, NULL, G_CALLBACK(scroll_prev_page_cb) },
	{"View/Scroll/NextPage",                     NULL, N_("Next page"), NULL, NULL, G_CALLBACK(scroll_next_page_cb) },
	/* {"View/Scroll/---",                       NULL, "---", NULL, NULL, NULL }, */

	{"View/Encoding",                            NULL, N_("Character _encoding"), NULL, NULL, NULL }, /* set_charset_cb */
	{"View/Encoding/---",                        NULL, "---", NULL, NULL, NULL },
#define ENC_ACTION(cs_char,c_char,string) \
	{ "View/Encoding/" cs_char, NULL, N_(string), NULL, NULL, c_char }

	{"View/Encoding/Western",                    NULL, N_("Western European"), NULL, NULL, NULL },
	{"View/Encoding/Baltic",                     NULL, N_("Baltic"), NULL, NULL, NULL },
	{"View/Encoding/Hebrew",                     NULL, N_("Hebrew"), NULL, NULL, NULL },
	{"View/Encoding/Arabic",                     NULL, N_("Arabic"), NULL, NULL, NULL },
	{"View/Encoding/Cyrillic",                   NULL, N_("Cyrillic"), NULL, NULL, NULL },
	{"View/Encoding/Japanese",                   NULL, N_("Japanese"), NULL, NULL, NULL },
	{"View/Encoding/Chinese",                    NULL, N_("Chinese"), NULL, NULL, NULL },
	{"View/Encoding/Korean",                     NULL, N_("Korean"), NULL, NULL, NULL },
	{"View/Encoding/Thai",                       NULL, N_("Thai"), NULL, NULL, NULL },

	{"View/Decode",                              NULL, N_("Decode"), NULL, NULL, NULL }, /* set_decode_cb */
	{"View/Decode/---",                          NULL, "---", NULL, NULL, NULL },

#define DEC_ACTION(cs_type,c_type,string) \
	{ "View/Decode/" cs_type, NULL, N_(string), NULL, NULL, c_type }

	{"View/---",                                 NULL, "---", NULL, NULL, NULL },
	{"View/MessageSource",                       NULL, N_("Mess_age source"), "<control>U", NULL, G_CALLBACK(view_source_cb) },
	{"View/Part",                                NULL, N_("Message part"), NULL, NULL, NULL },
	{"View/Part/AsText",                         NULL, N_("View as text"), "T", NULL, G_CALLBACK(view_part_as_text_cb) },
	{"View/Part/Open",                           NULL, N_("Open"), "L", NULL, G_CALLBACK(open_part_cb) },
#ifndef G_OS_WIN32
	{"View/Part/OpenWith",                       NULL, N_("Open with..."), "O", NULL, G_CALLBACK(open_part_with_cb) },
#endif

	{"View/Quotes",                              NULL, N_("Quotes"), NULL, NULL, NULL }, 

/* Message menu */
	{"Message/Compose",                          NULL, N_("Compose _new message"), "<control>M", NULL, G_CALLBACK(compose_cb) },
	{"Message/---",                              NULL, "---", NULL, NULL, NULL },

	{"Message/Reply",                            NULL, N_("_Reply"), "<control>R", NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_REPLY */
	{"Message/ReplyTo",                          NULL, N_("Repl_y to"), NULL, NULL, NULL }, 
	{"Message/ReplyTo/All",                      NULL, N_("_All"), "<control><shift>R", NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_REPLY_TO_ALL */
	{"Message/ReplyTo/Sender",                   NULL, N_("_Sender"), NULL, NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_REPLY_TO_SENDER */
	{"Message/ReplyTo/List",                     NULL, N_("Mailing _list"), "<control>L", NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_REPLY_TO_LIST */
	/* {"Message/---",                           NULL, "---", NULL, NULL, NULL }, */

	{"Message/Forward",                          NULL, N_("_Forward"), "<control><alt>F", NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_FORWARD_INLINE */
	{"Message/ForwardAtt",                       NULL, N_("For_ward as attachment"), NULL, NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_FORWARD_AS_ATTACH */
	{"Message/Redirect",                         NULL, N_("Redirec_t"), NULL, NULL, G_CALLBACK(reply_cb) }, /* COMPOSE_REDIRECT */
	{"Message/CheckSignature",                   NULL, N_("Check signature"), "C", NULL, G_CALLBACK(check_signature_cb) },

/* Tools menu */	
	{"Tools/AddressBook",                        NULL, N_("_Address book"), "<control><shift>A", NULL, G_CALLBACK(addressbook_open_cb) }, 
	{"Tools/AddSenderToAB",                      NULL, N_("Add sender to address boo_k"), NULL, NULL, G_CALLBACK(add_address_cb) }, 
	{"Tools/---",                                NULL, "---", NULL, NULL, NULL },

	{"Tools/CreateFilterRule",                   NULL, N_("_Create filter rule"), NULL, NULL, NULL },
	{"Tools/CreateFilterRule/Automatically",     NULL, N_("_Automatically"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_AUTO */
	{"Tools/CreateFilterRule/ByFrom",            NULL, N_("By _From"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_FROM */
	{"Tools/CreateFilterRule/ByTo",              NULL, N_("By _To"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_TO     */
	{"Tools/CreateFilterRule/BySubject",         NULL, N_("By _Subject"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_SUBJECT */
	{"Tools/CreateFilterRule/BySender",          NULL, N_("By S_ender"), NULL, NULL, G_CALLBACK(create_filter_cb) }, /* FILTER_BY_SENDER */

	{"Tools/CreateProcessingRule",               NULL, N_("Create processing rule"), NULL, NULL, NULL },
	{"Tools/CreateProcessingRule/Automatically", NULL, N_("_Automatically"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/ByFrom",        NULL, N_("By _From"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/ByTo",          NULL, N_("By _To"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/BySubject",     NULL, N_("By _Subject"), NULL, NULL, G_CALLBACK(create_processing_cb) }, 
	{"Tools/CreateProcessingRule/BySender",      NULL, N_("By S_ender"), NULL, NULL, G_CALLBACK(create_processing_cb) },
	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */

	{"Tools/ListUrls",                           NULL, N_("List _URLs..."), "<control><shift>U", NULL, G_CALLBACK(open_urls_cb) }, 

	/* {"Tools/---",                             NULL, "---", NULL, NULL, NULL }, */
	{"Tools/Actions",                            NULL, N_("Actio_ns"), NULL, NULL, NULL },
	{"Tools/Actions/PlaceHolder",                NULL, "Placeholder", NULL, NULL, G_CALLBACK(messageview_nothing_cb) },

/* Help menu */
	{"Help/About",                               NULL, N_("_About"), NULL, NULL, G_CALLBACK(about_cb) }, 
};

static GtkToggleActionEntry msgview_toggle_entries[] =
{
	{"View/AllHeaders",         NULL, N_("Show all _headers"), "<control>H", NULL, G_CALLBACK(show_all_header_cb), FALSE }, /* toggle */
	{"View/Quotes/CollapseAll", NULL, N_("_Collapse all"), "<control><shift>Q", NULL, G_CALLBACK(msg_hide_quotes_cb), FALSE }, /* 1 toggle */
	{"View/Quotes/Collapse2",   NULL, N_("Collapse from level _2"), NULL, NULL, G_CALLBACK(msg_hide_quotes_cb), FALSE }, /* 2 toggle */
	{"View/Quotes/Collapse3",   NULL, N_("Collapse from level _3"), NULL, NULL, G_CALLBACK(msg_hide_quotes_cb), FALSE }, /* 3 toggle */
};

static GtkRadioActionEntry msgview_radio_enc_entries[] =
{
	ENC_ACTION(CS_AUTO, C_AUTO, N_("_Automatic")), /* RADIO set_charset_cb */
	ENC_ACTION(CS_US_ASCII, C_US_ASCII, N_("7bit ASCII (US-ASC_II)")), /* RADIO set_charset_cb */
	ENC_ACTION(CS_UTF_8, C_UTF_8, N_("Unicode (_UTF-8)")), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_ISO_8859_1, C_ISO_8859_1, "ISO-8859-_1"), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_ISO_8859_15, C_ISO_8859_15, "ISO-8859-15"), /* RADIO set_charset_cb */
	ENC_ACTION("Western/"CS_WINDOWS_1252, C_WINDOWS_1252, "Windows-1252"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_2, C_ISO_8859_2, N_("Central European (ISO-8859-_2)")), /* RADIO set_charset_cb */
	ENC_ACTION("Baltic/"CS_ISO_8859_13, C_ISO_8859_13, "ISO-8859-13"), /* RADIO set_charset_cb */
	ENC_ACTION("Baltic/"CS_ISO_8859_4, C_ISO_8859_14, "ISO-8859-_4"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_7, C_ISO_8859_7, N_("Greek (ISO-8859-_7)")), /* RADIO set_charset_cb */
	ENC_ACTION("Hebrew/"CS_ISO_8859_8, C_ISO_8859_8, "ISO-8859-_8"), /* RADIO set_charset_cb */
	ENC_ACTION("Hebrew/"CS_WINDOWS_1255, C_WINDOWS_1255, "Windows-1255"), /* RADIO set_charset_cb */
	ENC_ACTION("Arabic/"CS_ISO_8859_6, C_ISO_8859_6, "ISO-8859-_6"), /* RADIO set_charset_cb */
	ENC_ACTION("Arabic/"CS_WINDOWS_1256, C_WINDOWS_1256, "Windows-1256"), /* RADIO set_charset_cb */
	ENC_ACTION(CS_ISO_8859_9, C_ISO_8859_9, N_("Turkish (ISO-8859-_9)")), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_ISO_8859_5, C_ISO_8859_5, "ISO-8859-_5"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_KOI8_R, C_KOI8_R, "KOI8-_R"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_MACCYR, C_MACCYR, "MAC_CYR"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_KOI8_U, C_KOI8_U, "KOI8-_U"), /* RADIO set_charset_cb */
	ENC_ACTION("Cyrillic/"CS_WINDOWS_1251, C_WINDOWS_1251, "Windows-1251"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_ISO_2022_JP, C_ISO_2022_JP, "ISO-2022-_JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_ISO_2022_JP_2, C_ISO_2022_JP_2, "ISO-2022-JP-_2"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_EUC_JP, C_EUC_JP, "_EUC-JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Japanese/"CS_SHIFT_JIS, C_SHIFT_JIS, "_Shift-JIS"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GB18030, C_GB18030, "_GB18030"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GB2312, C_GB2312, "_GB2312"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_GBK, C_GBK, "GB_K"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_BIG5, C_BIG5, "_Big5-JP"), /* RADIO set_charset_cb */
	ENC_ACTION("Chinese/"CS_EUC_TW, C_EUC_TW, "EUC-_TW"), /* RADIO set_charset_cb */
	ENC_ACTION("Korean/"CS_EUC_KR, C_EUC_KR, "_EUC-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Korean/"CS_ISO_2022_KR, C_ISO_2022_KR, "_ISO-2022-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Thai/"CS_TIS_620, C_TIS_620, "_TIS-620-KR"), /* RADIO set_charset_cb */
	ENC_ACTION("Thai/"CS_WINDOWS_874, C_WINDOWS_874, "_Windows-874"), /* RADIO set_charset_cb */
};

static GtkRadioActionEntry msgview_radio_dec_entries[] =
{
	DEC_ACTION("AutoDetect", 0, N_("_Auto detect")),	/* set_decode_cb */
	/* --- */
	DEC_ACTION("8bit", ENC_8BIT, "_8bit"),
	DEC_ACTION("QP", ENC_QUOTED_PRINTABLE, "_Quoted printable"),
	DEC_ACTION("B64", ENC_BASE64, "_Base64"),
	DEC_ACTION("Uuencode", ENC_X_UUENCODE, "_Uuencode"),
};

MessageView *messageview_create(MainWindow *mainwin)
{
	MessageView *messageview;
	GtkWidget *vbox;
	HeaderView *headerview;
	MimeView *mimeview;
	NoticeView *noticeview;

	debug_print("Creating message view...\n");
	messageview = g_new0(MessageView, 1);

	headerview = headerview_create();

	noticeview = noticeview_create(mainwin);

	mimeview = mimeview_create(mainwin);
	mimeview->textview = textview_create();
	mimeview->textview->messageview = messageview;
	mimeview->messageview = messageview;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(vbox), "messageview");
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(headerview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(noticeview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
                           GTK_WIDGET_PTR(mimeview), TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	messageview->vbox        = vbox;
	messageview->new_window  = FALSE;
	messageview->window      = NULL;
	messageview->headerview  = headerview;
	messageview->mimeview    = mimeview;
	messageview->noticeview = noticeview;
	messageview->mainwin    = mainwin;

	messageview->statusbar     = NULL;
	messageview->statusbar_cid = 0;

	messageview->show_full_text= FALSE;
	messageview->update_needed = FALSE;

	messageview->msginfo_update_callback_id =
		hooks_register_hook(MSGINFO_UPDATE_HOOKLIST, messageview_update_msg, (gpointer) messageview);

	return messageview;
}

const GList *messageview_get_msgview_list(void)
{
	return msgview_list;
}

void messageview_update_actions_menu(MessageView *msgview)
{
	/* Messages opened in a new window do not have a menu bar */
	if (msgview->menubar == NULL)
		return;
	action_update_msgview_menu(msgview->ui_manager, "/Menu/Tools/Actions", msgview);
}

static void messageview_add_toolbar(MessageView *msgview, GtkWidget *window) 
{
 	GtkWidget *handlebox;
	GtkWidget *vbox;
	GtkWidget *menubar;
#ifndef GENERIC_UMPC
	GtkWidget *statusbar = NULL;
#endif
	GtkActionGroup *action_group;


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);	

	msgview->ui_manager = gtk_ui_manager_new();
	action_group = cm_menu_create_action_group_full(msgview->ui_manager,"Menu", msgview_entries,
			G_N_ELEMENTS(msgview_entries), (gpointer)msgview);
	gtk_action_group_add_toggle_actions(action_group, msgview_toggle_entries,
			G_N_ELEMENTS(msgview_toggle_entries), (gpointer)msgview);
	gtk_action_group_add_radio_actions(action_group, msgview_radio_enc_entries,
			G_N_ELEMENTS(msgview_radio_enc_entries), C_AUTO, G_CALLBACK(set_charset_cb), (gpointer)msgview);
	gtk_action_group_add_radio_actions(action_group, msgview_radio_dec_entries,
			G_N_ELEMENTS(msgview_radio_dec_entries), C_AUTO, G_CALLBACK(set_decode_cb), (gpointer)msgview);

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/", "Menu", NULL, GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "File", "File", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "Edit", "Edit", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "View", "View", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "Message", "Message", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "Tools", "Tools", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu", "Help", "Help", GTK_UI_MANAGER_MENU)

/* File menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "SaveAs", "File/SaveAs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "SavePartAs", "File/SavePartAs", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "Separator1", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "PageSetup", "File/PageSetup", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "Print", "File/Print", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "Separator2", "File/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/File", "Close", "File/Close", GTK_UI_MANAGER_MENUITEM)

/* Edit menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Edit", "Copy", "Edit/Copy", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Edit", "SelectAll", "Edit/SelectAll", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Edit", "Separator1", "Edit/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Edit", "Find", "Edit/Find", GTK_UI_MANAGER_MENUITEM)

/* View menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Goto", "View/Goto", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Prev", "View/Goto/Prev", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Next", "View/Goto/Next", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator1", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevUnread", "View/Goto/PrevUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextUnread", "View/Goto/NextUnread", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator2", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevNew", "View/Goto/PrevNew", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextNew", "View/Goto/NextNew", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator3", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevMarked", "View/Goto/PrevMarked", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextMarked", "View/Goto/NextMarked", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator4", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevLabeled", "View/Goto/PrevLabeled", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextLabeled", "View/Goto/NextLabeled", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator5", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevHistory", "View/Goto/PrevHistory", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextHistory", "View/Goto/NextHistory", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator6", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "ParentMessage", "View/Goto/ParentMessage", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator7", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextUnreadFolder", "View/Goto/NextUnreadFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Folder", "View/Goto/Folder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "Separator8", "View/Goto/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "NextPart", "View/Goto/NextPart", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Goto", "PrevPart", "View/Goto/PrevPart", GTK_UI_MANAGER_MENUITEM)

        MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Scroll", "View/Scroll", GTK_UI_MANAGER_MENU)
        MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Scroll", "PrevLine", "View/Scroll/PrevLine", GTK_UI_MANAGER_MENUITEM)
        MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Scroll", "NextLine", "View/Scroll/NextLine", GTK_UI_MANAGER_MENUITEM)
        MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Scroll", "PrevPage", "View/Scroll/PrevPage", GTK_UI_MANAGER_MENUITEM)
        MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Scroll", "NextPage", "View/Scroll/NextPage", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Separator1", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Encoding", "View/Encoding", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_AUTO, "View/Encoding/"CS_AUTO, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Separator1", "View/Encoding/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_US_ASCII, "View/Encoding/"CS_US_ASCII, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_UTF_8, "View/Encoding/"CS_UTF_8, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Separator2", "View/Encoding/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Western", "View/Encoding/Western", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Western", CS_ISO_8859_1, "View/Encoding/Western/"CS_ISO_8859_1, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Western", CS_ISO_8859_15, "View/Encoding/Western/"CS_ISO_8859_15, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Western", CS_WINDOWS_1252, "View/Encoding/Western/"CS_WINDOWS_1252, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_2, "View/Encoding/"CS_ISO_8859_2, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Baltic", "View/Encoding/Baltic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Baltic", CS_ISO_8859_13, "View/Encoding/Baltic/"CS_ISO_8859_13, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Baltic", CS_ISO_8859_4, "View/Encoding/Baltic/"CS_ISO_8859_4, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_7, "View/Encoding/"CS_ISO_8859_7, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Hebrew", "View/Encoding/Hebrew", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Hebrew", CS_ISO_8859_8, "View/Encoding/Hebrew/"CS_ISO_8859_8, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Hebrew", CS_WINDOWS_1255, "View/Encoding/Hebrew/"CS_WINDOWS_1255, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Arabic", "View/Encoding/Arabic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Arabic", CS_ISO_8859_6, "View/Encoding/Arabic/"CS_ISO_8859_6, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Arabic", CS_WINDOWS_1256, "View/Encoding/Arabic/"CS_WINDOWS_1256, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", CS_ISO_8859_9, "View/Encoding/"CS_ISO_8859_9, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Cyrillic", "View/Encoding/Cyrillic", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_ISO_8859_5, "View/Encoding/Cyrillic/"CS_ISO_8859_5, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_KOI8_R, "View/Encoding/Cyrillic/"CS_KOI8_R, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_MACCYR, "View/Encoding/Cyrillic/"CS_MACCYR, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_KOI8_U, "View/Encoding/Cyrillic/"CS_KOI8_U, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Cyrillic", CS_WINDOWS_1251, "View/Encoding/Cyrillic/"CS_WINDOWS_1251, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Japanese", "View/Encoding/Japanese", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Japanese", CS_ISO_2022_JP, "View/Encoding/Japanese/"CS_ISO_2022_JP, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Japanese", CS_ISO_2022_JP_2, "View/Encoding/Japanese/"CS_ISO_2022_JP_2, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Japanese", CS_EUC_JP, "View/Encoding/Japanese/"CS_EUC_JP, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Japanese", CS_SHIFT_JIS, "View/Encoding/Japanese/"CS_SHIFT_JIS, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Chinese", "View/Encoding/Chinese", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Chinese", CS_GB18030, "View/Encoding/Chinese/"CS_GB18030, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Chinese", CS_GB2312, "View/Encoding/Chinese/"CS_GB2312, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Chinese", CS_GBK, "View/Encoding/Chinese/"CS_GBK, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Chinese", CS_BIG5, "View/Encoding/Chinese/"CS_BIG5, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Chinese", CS_EUC_TW, "View/Encoding/Chinese/"CS_EUC_TW, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Korean", "View/Encoding/Korean", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Korean", CS_EUC_KR, "View/Encoding/Korean/"CS_EUC_KR, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Korean", CS_ISO_2022_KR, "View/Encoding/Korean/"CS_ISO_2022_KR, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding", "Thai", "View/Encoding/Thai", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Thai", CS_TIS_620, "View/Encoding/Thai/"CS_TIS_620, GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Encoding/Thai", CS_WINDOWS_874, "View/Encoding/Thai/"CS_WINDOWS_874, GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Decode", "View/Decode", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "AutoDetect", "View/Decode/AutoDetect", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "Separator1", "View/Decode/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "8bit", "View/Decode/8bit", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "QP", "View/Decode/QP", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "B64", "View/Decode/B64", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Decode", "Uuencode", "View/Decode/Uuencode", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Separator2", "View/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "MessageSource", "View/MessageSource", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "AllHeaders", "View/AllHeaders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Quotes", "View/Quotes", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Quotes", "CollapseAll", "View/Quotes/CollapseAll", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Quotes", "Collapse2", "View/Quotes/Collapse2", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Quotes", "Collapse3", "View/Quotes/Collapse3", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View", "Part", "View/Part", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Part", "AsText", "View/Part/AsText", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Part", "Open", "View/Part/Open", GTK_UI_MANAGER_MENUITEM)
#ifndef G_OS_WIN32
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/View/Part", "OpenWith", "View/Part/OpenWith", GTK_UI_MANAGER_MENUITEM)
#endif

/* Message menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Compose", "Message/Compose", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Separator1", "Message/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Reply", "Message/Reply", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "ReplyTo", "Message/ReplyTo", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message/ReplyTo", "All", "Message/ReplyTo/All", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message/ReplyTo", "Sender", "Message/ReplyTo/Sender", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message/ReplyTo", "List", "Message/ReplyTo/List", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Separator2", "Message/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Forward", "Message/Forward", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "ForwardAtt", "Message/ForwardAtt", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "Redirect", "Message/Redirect", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Message", "CheckSignature", "Message/CheckSignature", GTK_UI_MANAGER_MENUITEM)

/* Tools menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "AddressBook", "Tools/AddressBook", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "AddSenderToAB", "Tools/AddSenderToAB", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "Separator1", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "CreateFilterRule", "Tools/CreateFilterRule", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateFilterRule", "Automatically", "Tools/CreateFilterRule/Automatically", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateFilterRule", "ByFrom", "Tools/CreateFilterRule/ByFrom", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateFilterRule", "ByTo", "Tools/CreateFilterRule/ByTo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateFilterRule", "BySubject", "Tools/CreateFilterRule/BySubject", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateFilterRule", "BySender", "Tools/CreateFilterRule/BySender", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "CreateProcessingRule", "Tools/CreateProcessingRule", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateProcessingRule", "Automatically", "Tools/CreateProcessingRule/Automatically", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateProcessingRule", "ByFrom", "Tools/CreateProcessingRule/ByFrom", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateProcessingRule", "ByTo", "Tools/CreateProcessingRule/ByTo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateProcessingRule", "BySubject", "Tools/CreateProcessingRule/BySubject", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/CreateProcessingRule", "BySender", "Tools/CreateProcessingRule/BySender", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "Separator2", "Tools/---", GTK_UI_MANAGER_SEPARATOR)
	
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "ListUrls", "Tools/ListUrls", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "Separator3", "Tools/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools", "Actions", "Tools/Actions", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Tools/Actions", "PlaceHolder", "Tools/Actions/PlaceHolder", GTK_UI_MANAGER_MENUITEM)

/* Help menu */
	MENUITEM_ADDUI_MANAGER(msgview->ui_manager, "/Menu/Help", "About", "Help/About", GTK_UI_MANAGER_MENUITEM)

	menubar = gtk_ui_manager_get_widget(msgview->ui_manager, "/Menu");
	gtk_widget_show_all(menubar);
	gtk_window_add_accel_group(GTK_WINDOW(window), gtk_ui_manager_get_accel_group(msgview->ui_manager));

	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	cm_toggle_menu_set_active_full(msgview->ui_manager, "Menu/View/AllHeaders",
					prefs_common.show_all_headers);

	handlebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	gtk_widget_realize(handlebox);
	msgview->toolbar = toolbar_create(TOOLBAR_MSGVIEW, handlebox,
					  (gpointer)msgview);
#ifndef GENERIC_UMPC
	statusbar = gtk_statusbar_new();
	gtk_widget_show(statusbar);
	gtk_box_pack_end(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
	msgview->statusbar = statusbar;
	msgview->statusbar_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Message View");
#else
	msgview->statusbar = NULL;
	msgview->statusbar_cid = 0;
#endif


	msgview->handlebox = handlebox;
	msgview->menubar   = menubar;

	gtk_container_add(GTK_CONTAINER(vbox),
			  GTK_WIDGET_PTR(msgview));

	messageview_update_actions_menu(msgview);

	msgview_list = g_list_append(msgview_list, msgview);
}

static MessageView *messageview_create_with_new_window_visible(MainWindow *mainwin, gboolean show)
{
	MessageView *msgview;
	GtkWidget *window;
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "messageview");
	gtk_window_set_title(GTK_WINDOW(window), _("Claws Mail - Message View"));
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	gtk_window_set_default_size(GTK_WINDOW(window), prefs_common.msgwin_width,
			prefs_common.msgwin_height);

	if (!geometry.min_height) {
		geometry.min_width = 320;
		geometry.min_height = 200;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);

#ifdef G_OS_WIN32
	gtk_window_move(GTK_WINDOW(window), 48, 48);
#endif

	msgview = messageview_create(mainwin);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(messageview_size_allocate_cb),
			 msgview);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(messageview_delete_cb), msgview);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), msgview);
	messageview_add_toolbar(msgview, window);

	if (show) {
		gtk_widget_show(window);
	} else {
		gtk_widget_realize(window);
	}

	msgview->new_window = TRUE;
	msgview->window = window;
	msgview->visible = TRUE;

	toolbar_set_style(msgview->toolbar->toolbar, msgview->handlebox, 
			  prefs_common.toolbar_style);
	messageview_init(msgview);

	if (show) {
		GTK_EVENTS_FLUSH();
		gtk_widget_grab_focus(msgview->mimeview->textview->text);
	}

	return msgview;
}

MessageView *messageview_create_with_new_window(MainWindow *mainwin)
{
	return messageview_create_with_new_window_visible(mainwin, TRUE);
}
void messageview_init(MessageView *messageview)
{
	headerview_init(messageview->headerview);
	mimeview_init(messageview->mimeview);
	/*messageview_set_font(messageview);*/

	noticeview_hide(messageview->noticeview);
}

static void notification_convert_header(gchar **dest,
					const gchar *src_,
					gint header_len)
{
	char *src;

	cm_return_if_fail(src_ != NULL);

	if (header_len < 1) {
		*dest = g_strdup("");
		return;
	}

	Xstrndup_a(src, src_, strlen(src_), return);

	remove_return(src);

	if (is_ascii_str(src)) {
		*dest = g_strdup(src);
		return;
	} else {
		*dest = g_malloc(BUFFSIZE);
		conv_encode_header(*dest, BUFFSIZE, src, header_len, FALSE);
	}
}

static gint disposition_notification_send(MsgInfo *msginfo)
{
	gchar *buf = NULL;
	gchar tmp[MAXPATHLEN + 1];
	FILE *fp;
	GList *ac_list;
	PrefsAccount *account = NULL;
	gint ok;
	gchar *to;
	FolderItem *queue, *outbox;
	gint num;
	gchar *path;
        gchar *addr;
        gchar *addrp;
	gchar *foo = NULL;
	gboolean queued_removed = FALSE;
	gchar *boundary = NULL;
	gchar buf_date[RFC822_DATE_BUFFSIZE];
	gchar *date = NULL;
	gchar *orig_to = NULL;
	gchar *enc_sub = NULL;

	if (!msginfo->extradata)
		return -1;
	if (!msginfo->extradata->returnreceiptto && 
	    !msginfo->extradata->dispositionnotificationto) 
		return -1;

	/* RFC2298: Test for Return-Path */
	if (msginfo->extradata->dispositionnotificationto)
		to = msginfo->extradata->dispositionnotificationto;
	else
		to = msginfo->extradata->returnreceiptto;

	ok = procheader_get_header_from_msginfo(msginfo, &buf, "Return-Path:");
	if (ok == 0) {
		gchar *to_addr = g_strdup(to);
		extract_address(to_addr);
		extract_address(buf);
		ok = strcasecmp(to_addr, buf);
		g_free(to_addr);
	} else {
		buf = g_strdup(_("<No Return-Path found>"));
	}
	
	if (ok != 0) {
		AlertValue val;
		gchar *message;
		message = g_markup_printf_escaped(
		  _("The notification address to which the return receipt is\n"
		    "to be sent does not correspond to the return path:\n"
		    "Notification address: %s\n"
		    "Return path: %s\n"
		    "It is advised to not send the return receipt."),
		  to, buf);
		val = alertpanel_full(_("Warning"), message,
				      NULL, _("_Don't Send"), NULL, _("_Send"),
				      NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				      NULL, ALERT_WARNING);
		g_free(message);				
		if (val != G_ALERTALTERNATE) {
			g_free(buf);
			return -1;
		}
	}
	g_free(buf);
	buf = NULL;

	ac_list = account_find_all_from_address(NULL, msginfo->to);
	ac_list = account_find_all_from_address(ac_list, msginfo->cc);

	if (ac_list == NULL) {
		ac_list = account_find_all();
		if ((account = select_account_from_list(ac_list, FALSE)) == NULL)
			return -1;
	} else if (g_list_length(ac_list) > 1) {
		if ((account = select_account_from_list(ac_list, TRUE)) == NULL)
			return -1;
	} else if (ac_list != NULL)
		account = (PrefsAccount *) ac_list->data;
	g_list_free(ac_list);

	if (account == NULL)
		account = account_get_default();
	if (!account || account->protocol == A_NNTP) {
		alertpanel_error(_("Account for sending mail is not specified.\n"
				   "Please select a mail account before sending."));
		return -1;
	}

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg%p",
		   get_rc_dir(), G_DIR_SEPARATOR, msginfo);

	if ((fp = claws_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "claws_fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning("can't change file mode");
	}
	
	addr = g_strdup(to);
	
	extract_address(addr);
	addrp = addr;
	
	/* write queue headers */
	ok = fprintf(fp, "AF:\n"
		    "NF:0\n"
		    "PS:10\n"
		    "SRH:1\n"
		    "SFN:\n"
		    "DSR:\n"
		    "MID:\n"
		    "CFG:\n"
		    "PT:0\n"
		    "S:%s\n"
		    "RQ:\n"
		    "SSV:%s\n"
		    "SSH:\n"
		    "R:<%s>\n", 
		    account->address,
		    account->smtp_server?account->smtp_server:"",
		    addrp);

	g_free(addrp);
	if (ok < 0)
		goto FILE_ERROR;
	
	/* check whether we need to save the message */
	outbox = account_get_special_folder(account, F_OUTBOX); 
	if (folder_get_default_outbox() == outbox && !prefs_common.savemsg)
		outbox = NULL;
	if (outbox) {
		path = folder_item_get_identifier(outbox);
		ok = fprintf(fp, "SCF:%s\n", path);
		g_free(path);
		
		if (ok < 0)
			goto FILE_ERROR;
	}		

	if (fprintf(fp, "X-Claws-End-Special-Headers: 1\n") < 0)
		goto FILE_ERROR;

	/* Date */
	get_rfc822_date(buf_date, sizeof(buf_date));
	if (fprintf(fp, "Date: %s\n", buf_date) < 0)
		goto FILE_ERROR;

	/* From */
	if (account->name && *account->name) {
		notification_convert_header(&buf, account->name, strlen("From: "));
		if (buf == NULL)
			goto FILE_ERROR;
		if (fprintf(fp, "From: %s <%s>\n", buf, account->address) < 0) {
			g_free(buf);
			goto FILE_ERROR;
		}
		g_free(buf);
		buf = NULL;
	} else {
		if (fprintf(fp, "From: %s\n", account->address) < 0)
			goto FILE_ERROR;
	}

	if (fprintf(fp, "To: %s\n", to) < 0)
		goto FILE_ERROR;

	/* Subject */
	notification_convert_header(&buf, msginfo->subject, strlen("Subject: "));
	if (buf == NULL)
		goto FILE_ERROR;
	if (fprintf(fp, "Subject: Disposition notification: %s\n", buf) < 0) {
		g_free(buf);
		goto FILE_ERROR;
	}
	g_free(buf);
	buf = NULL;

	/* Message ID */
	if (account->gen_msgid) {
		gchar *addr = prefs_account_generate_msgid(account);
		if (fprintf(fp, "Message-ID: <%s>\n", addr) < 0) {
			g_free(addr);
			goto FILE_ERROR;
		}
		g_free(addr);
	}

	boundary = generate_mime_boundary("DN");
	date = g_strdup(buf_date);
	if (msginfo->to) {
		orig_to = g_strdup(msginfo->to);
		extract_address(orig_to);
	}
	if (msginfo->subject && *(msginfo->subject)) {
		enc_sub = g_malloc0(strlen(msginfo->subject)*8);
		qp_encode_line(enc_sub, (const guchar *)msginfo->subject);
		g_strstrip(enc_sub);
	}
	ok = fprintf(fp,"MIME-Version: 1.0\n"
			"Content-Type: multipart/report; report-type=disposition-notification;\n"
			"  boundary=\"%s\"\n"
			"\n"
			"--%s\n"
			"Content-Type: text/plain; charset=UTF-8\n"
			"Content-Transfer-Encoding: quoted-printable\n"
			"\n"
			"The message sent on: %s\n"
			"To: %s\n"
			"With subject: \"%s\"\n"
			"has been displayed at %s.\n"
			"\n"
			"There is no guarantee that the message has been read or understood.\n"
			"\n"
			"--%s\n"
			"Content-Type: message/disposition-notification\n"
			"\n"
			"Reporting-UA: %s\n"
			"Original-Recipient: rfc822;%s\n"
			"Final-Recipient: rfc822;%s\n"
			"Original-Message-ID: <%s>\n"
			"Disposition: manual-action/MDN-sent-manually; displayed\n"
			"\n"
			"--%s\n"
			"Content-Type: application/octet-stream\n"
			"\n"
			"Reporting-UA: %s\n"
			"Original-Recipient: rfc822;%s\n"
			"Final-Recipient: rfc822;%s\n"
			"Original-Message-ID: <%s>\n"
			"Disposition: manual-action/MDN-sent-manually; displayed\n"
			"\n"
			"--%s--\n", 
			boundary, 
			boundary,
			msginfo->date, 
			orig_to?orig_to:"No To:",
			enc_sub?enc_sub:"No subject",
			date,
			boundary,
			PROG_VERSION,
			orig_to?orig_to:"No To:",
			account->address,
			msginfo->msgid?msginfo->msgid:"NO MESSAGE ID",
			boundary,
			PROG_VERSION,
			orig_to?orig_to:"No To:",
			account->address,
			msginfo->msgid?msginfo->msgid:"NO MESSAGE ID",
			boundary);

	g_free(enc_sub);
	g_free(orig_to);
	g_free(date);
	g_free(boundary);

	if (ok < 0)
		goto FILE_ERROR;	

	if (claws_safe_fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "claws_fclose");
		claws_unlink(tmp);
		return -1;
	}

	/* put it in queue */
	queue = account_get_special_folder(account, F_QUEUE);
	if (!queue) queue = folder_get_default_queue();
	if (!queue) {
		g_warning("can't find queue folder");
		claws_unlink(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, NULL, TRUE)) < 0) {
		g_warning("can't queue the message");
		claws_unlink(tmp);
		return -1;
	}
		
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(TRUE,
		_("Claws Mail needs network access in order "
		  "to send this email.")))
		return 0;

	/* send it */
	path = folder_item_fetch_msg(queue, num);
	ok = procmsg_send_message_queue_with_lock(path, &foo, queue, num, &queued_removed);
	g_free(path);
	g_free(foo);
	if (ok == 0 && !queued_removed)
		folder_item_remove_msg(queue, num);

	return ok;

FILE_ERROR:
	claws_fclose(fp);
	claws_unlink(tmp);
	return -1;
}

static gboolean find_encrypted_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	MimeInfo **encinfo = (MimeInfo **) data;
	
	if (privacy_mimeinfo_is_encrypted(mimeinfo)) {
		*encinfo = mimeinfo;
		return TRUE;
	}
	
	return FALSE;
}

static MimeInfo *find_encrypted_part(MimeInfo *rootinfo)
{
	MimeInfo *encinfo = NULL;

	g_node_traverse(rootinfo->node, G_IN_ORDER, G_TRAVERSE_ALL, -1,
		find_encrypted_func, &encinfo);
	
	return encinfo;
}

static gboolean find_broken_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	MimeInfo **brokeninfo = (MimeInfo **) data;
	
	if (mimeinfo->broken) {
		*brokeninfo = mimeinfo;
		return TRUE;
	}
	
	return FALSE;
}

static MimeInfo *find_broken_part(MimeInfo *rootinfo)
{
	MimeInfo *brokeninfo = NULL;

	g_node_traverse(rootinfo->node, G_IN_ORDER, G_TRAVERSE_ALL, -1,
		find_broken_func, &brokeninfo);
	
	return brokeninfo;
}

static void messageview_register_nav(MessageView *messageview)
{
	gchar *id;
	gint pos = -1;
	GList *existing;

	cm_return_if_fail(messageview);
	cm_return_if_fail(messageview->msginfo);

	id = procmsg_msginfo_get_identifier(messageview->msginfo);
	existing = g_list_find_custom(messageview->trail, id, (GCompareFunc)g_strcmp0);

	if (existing != NULL)
		pos = g_list_position(messageview->trail, existing);
	else
		pos = -1;

	if (pos != -1) {
		messageview->trail_pos = pos;
		g_free(id);
	} else {
		/* Cut the end of the list */
		GList *end = g_list_nth(messageview->trail, messageview->trail_pos + 1);
		if (end) {
			if (end->prev) {
				end->prev->next = NULL;
				end->prev = NULL;
				list_free_strings_full(end);
			} else {
				list_free_strings_full(messageview->trail);
				messageview->trail = NULL;
			}
		}
		messageview->trail = g_list_append(messageview->trail, id);
		messageview->trail_pos = (gint)g_list_length(messageview->trail) - 1;
		
		/* Cut the beginning if needed */
		while (messageview->trail_pos > prefs_common.nav_history_length) {
			g_free(messageview->trail->data);
			messageview->trail = g_list_delete_link(messageview->trail,
						messageview->trail);
			messageview->trail_pos--;
		}
	}
	messageview_set_menu_sensitive(messageview);
}

gboolean messageview_nav_has_prev(MessageView *messageview) {
	return messageview != NULL && messageview->trail != NULL
		&& messageview->trail_pos > 0;
}

gboolean messageview_nav_has_next(MessageView *messageview) {
	if (!messageview || !messageview->trail)
		return FALSE;
	
	return sc_g_list_bigger(messageview->trail, messageview->trail_pos + 1);
}

MsgInfo *messageview_nav_get_prev(MessageView *messageview) {
	GList *item;
	MsgInfo *info;

	cm_return_val_if_fail(messageview, NULL);
	cm_return_val_if_fail(messageview->trail, NULL);

	do {
		if (!messageview_nav_has_prev(messageview))
			return NULL;

		item = g_list_nth(messageview->trail, messageview->trail_pos - 1);
		cm_return_val_if_fail(item != NULL, NULL);

		info = procmsg_get_msginfo_from_identifier((const gchar *)item->data);
		if (info != NULL)
			break;

		g_free(item->data);
		messageview->trail = g_list_delete_link(messageview->trail, item);
		if (messageview->trail_pos > 0)
			messageview->trail_pos--;
	} while (info == NULL);

	return info;
}

MsgInfo *messageview_nav_get_next(MessageView *messageview) {
	GList *item;
	MsgInfo *info;

	cm_return_val_if_fail(messageview, NULL);
	cm_return_val_if_fail(messageview->trail, NULL);

	do {
		if (!messageview_nav_has_next(messageview))
			return NULL;

		item = g_list_nth(messageview->trail, messageview->trail_pos + 1);
		cm_return_val_if_fail(item != NULL, NULL);

		info = procmsg_get_msginfo_from_identifier((const gchar *)item->data);
		if (info != NULL)
			break;

		g_free(item->data);
		messageview->trail = g_list_delete_link(messageview->trail, item);
	} while (info == NULL);
	
	return info;
}

static gboolean messageview_try_select_mimeinfo(MessageView *messageview, MsgInfo *msginfo, MimeInfo *mimeinfo)
{
	if (mimeinfo->type == MIMETYPE_TEXT) {
		if (!strcasecmp(mimeinfo->subtype, "calendar")
				&& mimeview_has_viewer_for_content_type(messageview->mimeview, "text/calendar")) {
			mimeview_select_mimepart_icon(messageview->mimeview, mimeinfo);
			return TRUE;
		} else if (!strcasecmp(mimeinfo->subtype, "html")
				&& mimeinfo->disposition != DISPOSITIONTYPE_ATTACHMENT
				&& ((msginfo->folder && msginfo->folder->prefs->promote_html_part == HTML_PROMOTE_ALWAYS)
					|| ((msginfo->folder && msginfo->folder->prefs->promote_html_part == HTML_PROMOTE_DEFAULT)
						&& prefs_common.promote_html_part))) {
			mimeview_select_mimepart_icon(messageview->mimeview, mimeinfo);
			return TRUE;
		}
	}
	return FALSE;
}

static void messageview_find_part_depth_first(MimeInfoSearch *context, MimeMediaType type, const gchar *subtype)
{
	MimeInfo * mimeinfo = context->current;

	if (!mimeinfo)
		return;

	debug_print("found part %d/%s\n", mimeinfo->type, mimeinfo->subtype);

	if (mimeinfo->type == type
			&& !strcasecmp(mimeinfo->subtype, subtype)) {
		context->found = mimeinfo;
	} else if (mimeinfo->type == MIMETYPE_MULTIPART) {
		if (!strcasecmp(mimeinfo->subtype, "alternative")
				|| !strcasecmp(mimeinfo->subtype, "related")) {
			context->found = procmime_mimeinfo_next(mimeinfo);
			while (context->found && context->found != context->parent) {
				if (context->found->type == type
					&& !strcasecmp(context->found->subtype, subtype))
						break;
				context->found = procmime_mimeinfo_next(context->found);
			}
			if (context->found == context->parent)
				context->found = NULL;
		}
		if (!context->found
			&& (!strcasecmp(mimeinfo->subtype, "related")
				|| !strcasecmp(mimeinfo->subtype, "mixed"))) {
			context->parent = mimeinfo;
			context->current = procmime_mimeinfo_next(mimeinfo);
			messageview_find_part_depth_first(context, type, subtype);
		}
	}
}

gint messageview_show(MessageView *messageview, MsgInfo *msginfo,
		      gboolean all_headers)
{
	gchar *text = NULL;
	gchar *file;
	MimeInfo *mimeinfo, *encinfo, *root;
	gchar *subject = NULL;
	cm_return_val_if_fail(msginfo != NULL, -1);

	if (msginfo != messageview->msginfo)
		messageview->show_full_text = FALSE;

	if (messageview->mimeview->textview &&
	    messageview->mimeview->textview->loading) {
		messageview->mimeview->textview->stop_loading = TRUE;
		return 0;
	}

	if (messageview->toolbar)
		toolbar_set_learn_button
			(messageview->toolbar,
			 MSG_IS_SPAM(msginfo->flags)?LEARN_HAM:LEARN_SPAM);
	else
		toolbar_set_learn_button
			(messageview->mainwin->toolbar,
			 MSG_IS_SPAM(msginfo->flags)?LEARN_HAM:LEARN_SPAM);

	if (messageview->toolbar) {
		if (messageview->toolbar->learn_spam_btn) {
			gboolean can_learn = FALSE;
			if (procmsg_spam_can_learn() &&
			    (msginfo->folder &&
			     msginfo->folder->folder->klass->type != F_UNKNOWN &&
			     msginfo->folder->folder->klass->type != F_NEWS))
				can_learn = TRUE;

			gtk_widget_set_sensitive(
				messageview->toolbar->learn_spam_btn, 
				can_learn);
		}
	}
	
	noticeview_hide(messageview->noticeview);
	mimeview_clear(messageview->mimeview);
	messageview->updating = TRUE;

	if (msginfo->size > 1024*1024)
		statusbar_print_all(_("Fetching message (%s)..."),
			to_human_readable(msginfo->size));
	
	file = procmsg_get_message_file_path(msginfo);

	if (msginfo->size > 1024*1024)
		statusbar_pop_all();

	if (!file) {
		g_warning("can't get message file path");
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}
	
	if (!folder_has_parent_of_type(msginfo->folder, F_QUEUE) &&
	    !folder_has_parent_of_type(msginfo->folder, F_DRAFT))
		mimeinfo = procmime_scan_file(file);
	else
		mimeinfo = procmime_scan_queue_file(file);

	messageview->updating = FALSE;
	
	if (messageview->deferred_destroy) {
		g_free(file);
		messageview_destroy(messageview);
		return 0;
	}

	if (!mimeinfo) {
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}

	while ((encinfo = find_encrypted_part(mimeinfo)) != NULL) {
		debug_print("decrypting message part\n");
		if (privacy_mimeinfo_decrypt(encinfo) < 0) {
			text = g_strdup_printf(_("Couldn't decrypt: %s"),
					       privacy_get_error());
			noticeview_show(messageview->noticeview);
			noticeview_set_icon(messageview->noticeview,
					    STOCK_PIXMAP_NOTICE_WARN);
			noticeview_set_text(messageview->noticeview, text);
			gtk_widget_hide(messageview->noticeview->button);
			gtk_widget_hide(messageview->noticeview->button2);
			g_free(text);
			break;
		}
	}
			
	if (messageview->msginfo != msginfo) {
		procmsg_msginfo_free(&(messageview->msginfo));
		messageview->msginfo = NULL;
		messageview_set_menu_sensitive(messageview);
		messageview->msginfo = 
			procmsg_msginfo_get_full_info_from_file(msginfo, file);
		if (!messageview->msginfo)
			messageview->msginfo = procmsg_msginfo_copy(msginfo);
	} else {
		messageview->msginfo = NULL;
		messageview_set_menu_sensitive(messageview);
		messageview->msginfo = msginfo;
	}
	if (prefs_common.display_header_pane)
		headerview_show(messageview->headerview, messageview->msginfo);

	messageview_register_nav(messageview);
	messageview_set_position(messageview, 0);

	if (messageview->window) {
		gtk_window_set_title(GTK_WINDOW(messageview->window), 
				_("Claws Mail - Message View"));
		GTK_EVENTS_FLUSH();
	}
	mimeview_show_message(messageview->mimeview, mimeinfo, file);
	
#ifndef GENERIC_UMPC
	messageview_set_position(messageview, 0);
#endif

	if (messageview->window && msginfo->subject) {
		subject = g_strdup(msginfo->subject);
		if (!g_utf8_validate(subject, -1, NULL)) {
			g_free(subject);
			subject = g_malloc(strlen(msginfo->subject)*2 +1);
			conv_localetodisp(subject, strlen(msginfo->subject)*2 +1, 
				msginfo->subject);
		}
		if (g_utf8_validate(subject, -1, NULL))
			gtk_window_set_title(GTK_WINDOW(messageview->window), 
				subject);
		g_free(subject);
	}

	if (msginfo->folder) {
		msginfo->folder->last_seen = msginfo->msgnum;	
	}

	main_create_mailing_list_menu(messageview->mainwin, messageview->msginfo);

	if (messageview->msginfo && messageview->msginfo->extradata
	    && messageview->msginfo->extradata->partial_recv
	    && !noticeview_is_visible(messageview->noticeview))
		partial_recv_show(messageview->noticeview, 
				  messageview->msginfo);
	else if (messageview->msginfo && messageview->msginfo->extradata &&
	    (messageview->msginfo->extradata->dispositionnotificationto || 
	     messageview->msginfo->extradata->returnreceiptto) &&
	    !MSG_IS_RETRCPT_SENT(messageview->msginfo->flags) &&
	    !prefs_common.never_send_retrcpt &&
	    !noticeview_is_visible(messageview->noticeview))
		return_receipt_show(messageview->noticeview, 
				    messageview->msginfo);

	if (find_broken_part(mimeinfo) != NULL) {
		noticeview_set_icon(messageview->noticeview,
				    STOCK_PIXMAP_NOTICE_WARN);
		if (!noticeview_is_visible(messageview->noticeview)) {
			noticeview_set_text(messageview->noticeview, _("Message doesn't conform to MIME standard. "
						"It may render wrongly."));
			gtk_widget_hide(messageview->noticeview->button);
			gtk_widget_hide(messageview->noticeview->button2);
		} else {
			gchar *full = g_strconcat(
					gtk_label_get_text(GTK_LABEL(messageview->noticeview->text)), 
					"\n", 
					_("Message doesn't conform to MIME standard. "
					"It may render wrongly."), NULL);
			noticeview_set_text(messageview->noticeview, full);
			g_free(full);
		}
		noticeview_show(messageview->noticeview);
	}
			
	root = mimeinfo;
	mimeinfo = procmime_mimeinfo_next(mimeinfo);
	if (!all_headers && mimeinfo
			&& (mimeinfo->type != MIMETYPE_TEXT
				|| strcasecmp(mimeinfo->subtype, "plain"))
			&& (mimeinfo->type != MIMETYPE_MULTIPART
				|| strcasecmp(mimeinfo->subtype, "signed"))) {
		if (strcasecmp(mimeinfo->subtype, "html")) {
			MimeInfoSearch context = {
				.parent = root,
				.current = mimeinfo,
				.found = NULL
			};
			if (mimeview_has_viewer_for_content_type(messageview->mimeview, "text/calendar")) {
				MimeInfoSearch cal_context = context;
				messageview_find_part_depth_first(&cal_context, MIMETYPE_TEXT, "calendar");
				if (cal_context.found) { /* calendar found */
					mimeinfo = cal_context.found;
					if (messageview_try_select_mimeinfo(messageview, msginfo, mimeinfo))
						goto done;
				}
			}
			messageview_find_part_depth_first(&context, MIMETYPE_TEXT, "html");
			if (context.found &&
			    (msginfo->folder->prefs->promote_html_part == HTML_PROMOTE_ALWAYS ||
			     (msginfo->folder->prefs->promote_html_part == HTML_PROMOTE_DEFAULT &&
			      prefs_common.promote_html_part))) { /* html found */
				mimeinfo = context.found;
				if (messageview_try_select_mimeinfo(messageview, msginfo, mimeinfo))
					goto done;
			} else
				mimeinfo = root; /* nothing found */

			if (!mimeview_show_part(messageview->mimeview, mimeinfo))
				mimeview_select_mimepart_icon(messageview->mimeview, root);
			goto done;
		} else if (prefs_common.invoke_plugin_on_html) {
			mimeview_select_mimepart_icon(messageview->mimeview, mimeinfo);
			goto done;
		}
	}
	if (!all_headers && mimeinfo &&
	    mimeinfo->type == MIMETYPE_MULTIPART &&
	    mimeview_has_viewer_for_content_type(messageview->mimeview, "text/calendar")) {
		/* look for a calendar part or it looks really strange */
		while (mimeinfo) {
			if (mimeinfo->type == MIMETYPE_TEXT &&
			    !strcasecmp(mimeinfo->subtype, "calendar")) {
				mimeview_select_mimepart_icon(messageview->mimeview, mimeinfo);
				goto done;
			}
			mimeinfo = procmime_mimeinfo_next(mimeinfo);
		}
	}

	mimeview_select_mimepart_icon(messageview->mimeview, root);
done:
	messageview_set_menu_sensitive(messageview);
	/* plugins may hook in here to work with the message view */
	hooks_invoke(MESSAGE_VIEW_SHOW_DONE_HOOKLIST, messageview);

	g_free(file);

	return 0;
}

void messageview_reflect_prefs_pixmap_theme(void)
{
	GList *cur;
	MessageView *msgview;

	for (cur = msgview_list; cur != NULL; cur = cur->next) {
		msgview = (MessageView*)cur->data;
		toolbar_update(TOOLBAR_MSGVIEW, msgview);
		mimeview_update(msgview->mimeview);
	}
}

void messageview_clear(MessageView *messageview)
{
	if (!messageview)
		return;
	procmsg_msginfo_free(&(messageview->msginfo));
	messageview->msginfo = NULL;
	messageview->filtered = FALSE;

	if (messageview->window) {
		gtk_window_set_title(GTK_WINDOW(messageview->window), 
				_("Claws Mail - Message View"));
		GTK_EVENTS_FLUSH();
	}

	mimeview_clear(messageview->mimeview);
	headerview_clear(messageview->headerview);
	noticeview_hide(messageview->noticeview);
}

void messageview_destroy(MessageView *messageview)
{
	debug_print("destroy messageview\n");
	messageview_list = g_list_remove(messageview_list, messageview);

	if (messageview->mainwin->summaryview->messageview == messageview) {
		messageview->mainwin->summaryview->displayed = NULL;
		messageview->mainwin->summaryview->messageview = NULL;
	}
	if (messageview->mainwin->summaryview->ext_messageview == messageview) {
		messageview->mainwin->summaryview->displayed = NULL;
		messageview->mainwin->summaryview->ext_messageview = NULL;
	}
	if (!messageview->deferred_destroy)
		hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST,
			      messageview->msginfo_update_callback_id);

	if (messageview->updating) {
		debug_print("uh oh, better not touch that now (fetching)\n");
		messageview->deferred_destroy = TRUE;
		gtk_widget_hide(messageview->window);
		return;
	}
	
	if (messageview->mimeview->textview
	&&  messageview->mimeview->textview->loading) {
		debug_print("uh oh, better not touch that now (loading text)\n");
		messageview->deferred_destroy = TRUE;
		messageview->mimeview->textview->stop_loading = TRUE;
		gtk_widget_hide(messageview->window);
		return;
	}

	headerview_destroy(messageview->headerview);
	mimeview_destroy(messageview->mimeview);
	noticeview_destroy(messageview->noticeview);

	procmsg_msginfo_free(&(messageview->msginfo));
	toolbar_clear_list(TOOLBAR_MSGVIEW);
	if (messageview->toolbar) {
		toolbar_destroy(messageview->toolbar);
		g_free(messageview->toolbar);
	}

	message_search_close(messageview);

	list_free_strings_full(messageview->trail);
	msgview_list = g_list_remove(msgview_list, messageview); 

	if (messageview->window)
		gtk_widget_destroy(messageview->window);
	g_free(messageview);
}

void messageview_delete(MessageView *msgview)
{
	MsgInfo *msginfo = NULL;
	FolderItem *trash = NULL;
	PrefsAccount *ac = NULL;

	if (msgview->msginfo && msgview->mainwin && msgview->mainwin->summaryview)
		msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
	
	/* need a procmsg_msginfo_equal() */
	if (msginfo && msgview->msginfo && 
	    msginfo->msgnum == msgview->msginfo->msgnum && 
	    msginfo->folder == msgview->msginfo->folder) {
		summary_delete_trash(msgview->mainwin->summaryview);
	} else {		
		msginfo = msgview->msginfo;

		cm_return_if_fail(msginfo != NULL);

		/* We will need to access the original message's msginfo
		 * later, so we add our own reference. */
		procmsg_msginfo_new_ref(msginfo);

		/* to get the trash folder, we have to choose either
		 * the folder's or account's trash default - we prefer
		 * the one in the account prefs */
		if (msginfo->folder) {
			if (NULL != (ac = account_find_from_item(msginfo->folder)))
				trash = account_get_special_folder(ac, F_TRASH);
			if (!trash && msginfo->folder->folder)	
				trash = msginfo->folder->folder->trash;
			/* if still not found, use the default */
			if (!trash) 
				trash =	folder_get_default_trash();
		}	

		cm_return_if_fail(trash != NULL);

		if (prefs_common.immediate_exec)
			/* TODO: Delete from trash */
			folder_item_move_msg(trash, msginfo);
		else {
			procmsg_msginfo_set_to_folder(msginfo, trash);
			procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
			/* NOTE: does not update to next message in summaryview */
		}

		procmsg_msginfo_free(&msginfo);
	}
#ifdef GENERIC_UMPC
	if (msgview->window && !prefs_common.always_show_msg) {
		messageview_destroy(msgview);
	}
#endif
}

/* 
 * \brief update messageview with currently selected message in summaryview
 *        leave unchanged if summaryview is empty
 * \param pointer to MessageView
 */	
static void messageview_update(MessageView *msgview, MsgInfo *old_msginfo)
{
	SummaryView *summaryview = (SummaryView*)msgview->mainwin->summaryview;

	cm_return_if_fail(summaryview != NULL);
	
	if (summaryview->selected) {
		MsgInfo *msginfo = summary_get_selected_msg(summaryview);
		if (msginfo == NULL || msginfo == old_msginfo)
			return;

		messageview_show(msgview, msginfo, 
				 msgview->all_headers);
	} 
}

TextView *messageview_get_current_textview(MessageView *messageview)
{
	TextView *text = NULL;

	text = messageview->mimeview->textview;

	return text;
}

MimeInfo *messageview_get_selected_mime_part(MessageView *messageview)
{
	return mimeview_get_selected_part(messageview->mimeview);
}

void messageview_copy_clipboard(MessageView *messageview)
{
	gchar *text = messageview_get_selection(messageview);
	if (text) {
		gtk_clipboard_set_text(
			gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
			text, -1);
	}
	g_free(text);
}

void messageview_select_all(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text) {
		GtkTextView *textview = GTK_TEXT_VIEW(text->text);
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		buffer = gtk_text_view_get_buffer(textview);
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		gtk_text_buffer_select_range(buffer, &start, &end);
	}
}

void messageview_set_position(MessageView *messageview, gint pos)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text)
		textview_set_position(text, pos);
}

gboolean messageview_search_string(MessageView *messageview, const gchar *str,
				   gboolean case_sens)
{
	TextView *text;

	if (messageview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = messageview->mimeview->mimeviewer;
		if (viewer && viewer->text_search) {
			return viewer->text_search(viewer, FALSE, str, case_sens);
		}
	}

	text = messageview_get_current_textview(messageview);
	if (text)
		return textview_search_string(text, str, case_sens);
	return FALSE;
}

gboolean messageview_search_string_backward(MessageView *messageview,
					    const gchar *str,
					    gboolean case_sens)
{
	TextView *text;

	if (messageview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = messageview->mimeview->mimeviewer;
		if (viewer && viewer->text_search) {
			return viewer->text_search(viewer, TRUE, str, case_sens);
		}
	}

	text = messageview_get_current_textview(messageview);
	if (text)	
		return textview_search_string_backward(text,
						       str, case_sens);
	return FALSE;
}

gboolean messageview_is_visible(MessageView *messageview)
{
	if (messageview == NULL)
		return FALSE;
	return messageview->visible;
}

static gint messageview_delete_cb(GtkWidget *widget, GdkEventAny *event,
				  MessageView *messageview)
{
	messageview_destroy(messageview);
	return TRUE;
}

static void messageview_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&prefs_common.msgwin_width, &prefs_common.msgwin_height);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			MessageView *messageview)
{
	if (event && event->keyval == GDK_KEY_Escape && messageview->window) {
		messageview_destroy(messageview);
		return TRUE;
	}

	if (event && (event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
		return FALSE;
	if (event && (event->state & GDK_SHIFT_MASK) && event->keyval != GDK_KEY_space) 
		return FALSE;

	if (event && (event->keyval == GDK_KEY_KP_Enter || event->keyval ==  GDK_KEY_Return) &&
	    messageview->window) {
		MsgInfo *new_msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		messageview_show(messageview, new_msginfo, messageview->all_headers);
		return FALSE;
	}

	return mimeview_pass_key_press_event(messageview->mimeview, event);
}

static void messageview_show_partial_display_cb(NoticeView *noticeview, MessageView *messageview)
{
	messageview->show_full_text = TRUE;
	main_window_cursor_wait(mainwindow_get_mainwindow());
	noticeview_hide(messageview->noticeview);
	messageview->partial_display_shown = FALSE;
	GTK_EVENTS_FLUSH();
	mimeview_handle_cmd(messageview->mimeview, "sc://display_as_text", NULL, NULL);
	main_window_cursor_normal(mainwindow_get_mainwindow());
}

void messageview_show_partial_display(MessageView *messageview, MsgInfo *msginfo,
					     size_t length)
{
	gchar *msg = g_strdup_printf(_("Show all %s."), to_human_readable((goffset)length));
	noticeview_set_icon(messageview->noticeview, STOCK_PIXMAP_NOTICE_WARN);
	noticeview_set_text(messageview->noticeview, _("Only the first megabyte of text is shown."));
	noticeview_set_button_text(messageview->noticeview, msg);
	g_free(msg);
	noticeview_set_button_press_callback(messageview->noticeview,
					     G_CALLBACK(messageview_show_partial_display_cb),
					     (gpointer) messageview);
	noticeview_show(messageview->noticeview);
	messageview->partial_display_shown = TRUE;
}

static void return_receipt_show(NoticeView *noticeview, MsgInfo *msginfo)
{
	gchar *addr = NULL;
	gboolean from_me = FALSE;
	if (msginfo->folder 
		&& (folder_has_parent_of_type(msginfo->folder, F_QUEUE)
		 || folder_has_parent_of_type(msginfo->folder, F_DRAFT)))
		return;

	addr = g_strdup(msginfo->from);
	if (addr) {
		extract_address(addr);
		if (account_find_from_address(addr, FALSE)) {
			from_me = TRUE;
		}
		g_free(addr);
	}

	if (from_me) {
		noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
		if (MSG_IS_RETRCPT_GOT(msginfo->flags)) {
			noticeview_set_text(noticeview, _("You got a return receipt for this message: "
							  "it has been displayed by the recipient."));
		} else {
			noticeview_set_text(noticeview, _("You asked for a return receipt in this message."));
		}
		noticeview_set_button_text(noticeview, NULL);
		noticeview_set_button_press_callback(noticeview, NULL, NULL);
	} else {
		noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
		noticeview_set_text(noticeview, _("This message asks for a return receipt."));
		noticeview_set_button_text(noticeview, _("Send receipt"));
		noticeview_set_button_press_callback(noticeview,
						     G_CALLBACK(return_receipt_send_clicked),
						     (gpointer) msginfo);
	}
	noticeview_show(noticeview);
}

static void return_receipt_send_clicked(NoticeView *noticeview, MsgInfo *msginfo)
{
	MsgInfo *tmpmsginfo;
	gchar *file;

	file = procmsg_get_message_file_path(msginfo);
	if (!file) {
		g_warning("can't get message file path");
		return;
	}

	tmpmsginfo = procheader_parse_file(file, msginfo->flags, TRUE, TRUE);
	tmpmsginfo->folder = msginfo->folder;
	tmpmsginfo->msgnum = msginfo->msgnum;

	if (disposition_notification_send(tmpmsginfo) >= 0) {
		procmsg_msginfo_set_flags(msginfo, MSG_RETRCPT_SENT, 0);
		noticeview_hide(noticeview);
	}		

	procmsg_msginfo_free(&tmpmsginfo);
	g_free(file);
}

static void partial_recv_show(NoticeView *noticeview, MsgInfo *msginfo)
{
	gchar *text = NULL;
	gchar *button1 = NULL;
	gchar *button2 = NULL;
	void  *button1_cb = NULL;
	void  *button2_cb = NULL;

	if (!msginfo->extradata)
		return;
	if (!partial_msg_in_uidl_list(msginfo)) {
		text = g_strdup_printf(_("This message has been partially "
				"retrieved,\nand has been deleted from the "
				"server."));
	} else {
		switch (msginfo->planned_download) {
		case POP3_PARTIAL_DLOAD_UNKN:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s."),
					to_human_readable(
						(goffset)(msginfo->total_size)));
			button1 = _("Mark for download");
			button2 = _("Mark for deletion");
			button1_cb = partial_recv_dload_clicked;
			button2_cb = partial_recv_del_clicked;
			break;
		case POP3_PARTIAL_DLOAD_DLOAD:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s and "
					"will be downloaded."),
					to_human_readable(
						(goffset)(msginfo->total_size)));
			button1 = _("Unmark");
			button1_cb = partial_recv_unmark_clicked;
			button2 = _("Mark for deletion");
			button2_cb = partial_recv_del_clicked;
			break;
		case POP3_PARTIAL_DLOAD_DELE:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s and "
					"will be deleted."),
					to_human_readable(
						(goffset)(msginfo->total_size)));
			button1 = _("Mark for download");
			button1_cb = partial_recv_dload_clicked;
			button2 = _("Unmark");
			button2_cb = partial_recv_unmark_clicked;
			break;
		default:
			return;
		}
	}
	
	noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
	noticeview_set_text(noticeview, text);
	g_free(text);
	noticeview_set_button_text(noticeview, button1);
	noticeview_set_button_press_callback(noticeview,
		     G_CALLBACK(button1_cb), (gpointer) msginfo);

	noticeview_set_2ndbutton_text(noticeview, button2);
	noticeview_set_2ndbutton_press_callback(noticeview,
		     G_CALLBACK(button2_cb), (gpointer) msginfo);

	noticeview_show(noticeview);
}

static void partial_recv_dload_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_mark_for_download(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void partial_recv_del_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_mark_for_delete(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void partial_recv_unmark_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_unmark(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void select_account_cb(GtkWidget *w, gpointer data)
{
	*(gint*)data = combobox_get_active_data(GTK_COMBO_BOX(w));
}

static PrefsAccount *select_account_from_list(GList *ac_list, gboolean has_accounts)
{
	GtkWidget *optmenu;
	gint account_id;
	AlertValue val;

	cm_return_val_if_fail(ac_list != NULL, NULL);
	cm_return_val_if_fail(ac_list->data != NULL, NULL);

	optmenu = gtkut_account_menu_new(ac_list,
			G_CALLBACK(select_account_cb),
			&account_id);
	if (!optmenu)
		return NULL;
	account_id = ((PrefsAccount *) ac_list->data)->account_id;
	if (!has_accounts) {
		gchar *tr;
		gchar *text;
		tr = g_strdup(C_("'%s' stands for 'To' then 'Cc'",
		    "This message is asking for a return receipt notification\n"
		    "but according to its '%s' and '%s' headers it was not\n"
		    "officially addressed to you.\n"
		    "It is advised to not send the return receipt."));
		text = g_strdup_printf(tr,
		  prefs_common_translated_header_name("To"),
		  prefs_common_translated_header_name("Cc"));
		val = alertpanel_with_widget(
				_("Return Receipt Notification"),
				text, NULL, _("_Cancel"), NULL, _("_Send Notification"),
				NULL, NULL, ALERTFOCUS_FIRST, FALSE, optmenu);
		g_free(tr);
		g_free(text);
	} else
		val = alertpanel_with_widget(
				_("Return Receipt Notification"),
				_("More than one of your accounts uses the "
				 "address that this message was sent to.\n"
				 "Please choose which account you want to "
				 "use for sending the receipt notification:"),
				NULL, _("_Cancel"), NULL, _("_Send Notification"),
				NULL, NULL, ALERTFOCUS_FIRST, FALSE, optmenu);

	if (val != G_ALERTALTERNATE)
		return NULL;
	else
		return account_find_from_id(account_id);
}

/* 
 * \brief return selected messageview text, when nothing is 
 * 	  selected and message was filtered, return complete text
 *
 * \param  pointer to Messageview 
 *
 * \return pointer to text (needs to be free'd by calling func)
 */
gchar *messageview_get_selection(MessageView *msgview)
{
	TextView *textview;
	gchar *text = NULL;
	GtkTextView *edit = NULL;
	GtkTextBuffer *textbuf;
	gint body_pos = 0;
	GtkTextIter start_iter, end_iter;
	GtkTextMark *body_start, *body_end;
	
	cm_return_val_if_fail(msgview != NULL, NULL);

	if (msgview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = msgview->mimeview->mimeviewer;
		if (viewer && viewer->get_selection) {
			text = viewer->get_selection(viewer);
			if (text)
				return text;
		}
	}

	textview = messageview_get_current_textview(msgview);
	cm_return_val_if_fail(textview != NULL, NULL);

	edit = GTK_TEXT_VIEW(textview->text);
	cm_return_val_if_fail(edit != NULL, NULL);
	body_pos = textview->body_pos;

	textbuf = gtk_text_view_get_buffer(edit);

	if (gtk_text_buffer_get_selection_bounds(textbuf, NULL, NULL)) {
		return gtkut_text_view_get_selection(edit);
	} else {
		if (msgview->filtered) {
			gtk_text_buffer_get_iter_at_offset(textbuf, &start_iter, body_pos);
			gtk_text_buffer_get_end_iter(textbuf, &end_iter);
		} else {
			body_start = gtk_text_buffer_get_mark(textbuf, "body_start");

			/* If there is no body_start mark, an attachment is likely
			 * selected, and we're looking at instructions on what to do
			 * with it. No point in quoting that, so we'll just return NULL,
			 * so that original message body is quoted instead down the line.
			 */
			if (body_start == NULL) {
				return NULL;
			}

			gtk_text_buffer_get_iter_at_mark(textbuf, &start_iter, body_start);

			body_end = gtk_text_buffer_get_mark(textbuf, "body_end");
			if (body_end != NULL) /* Just in case */
				gtk_text_buffer_get_iter_at_mark(textbuf, &end_iter, body_end);
			else
				gtk_text_buffer_get_end_iter(textbuf, &end_iter);
		}

		return gtk_text_buffer_get_text(textbuf, &start_iter, &end_iter, FALSE);
	}

	return text;
}

static void save_as_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	summary_save_as(messageview->mainwin->summaryview);
}

static void print_mimeview(MimeView *mimeview, gint sel_start, gint sel_end, gint partnum) 
{
	MainWindow *mainwin;
	if (!mimeview 
	||  !mimeview->textview
	||  !mimeview->textview->text)
		alertpanel_warning(_("Cannot print: the message doesn't "
				     "contain text."));
	else {
		gtk_widget_realize(mimeview->textview->text);
		if (partnum > 0) {
			mimeview_select_part_num(mimeview, partnum);
		}
		if (mimeview->type == MIMEVIEW_VIEWER) {
			MimeViewer *viewer = mimeview->mimeviewer;
			if (viewer && viewer->print) {
				viewer->print(viewer);
				return;
			} else {
				/* Force text rendering if possible */
				MimeInfo *mimepart;

				mimepart = mimeview_get_selected_part(mimeview);
				if (mimepart == NULL
				||  (mimepart->type != MIMETYPE_TEXT && mimepart->type != MIMETYPE_MESSAGE)) {
					alertpanel_warning(_("Cannot print: the message doesn't "
							     "contain text."));
					return;
				}
				mimeview_show_part_as_text(mimeview, mimepart);
			}
		}
		if (sel_start != -1 && sel_end != -1) {
			GtkTextIter start, end;
			GtkTextView *text = GTK_TEXT_VIEW(mimeview->textview->text);
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

			gtk_text_buffer_get_iter_at_offset(buffer, &start, sel_start);
			gtk_text_buffer_get_iter_at_offset(buffer, &end, sel_end);
			gtk_text_buffer_select_range(buffer, &start, &end);
		}
		/* TODO: Get the real parent window, not the main window */
		mainwin = mainwindow_get_mainwindow();
		printing_print(GTK_TEXT_VIEW(mimeview->textview->text),
			       mainwin ? GTK_WINDOW(mainwin->window) : NULL,
				sel_start, sel_end,
				(mimeview->textview->image 
					? GTK_IMAGE(mimeview->textview->image)
					: NULL));
	}
}

void messageview_print(MsgInfo *msginfo, gboolean all_headers, 
			gint sel_start, gint sel_end, gint partnum) 
{
	PangoFontDescription *font_desc = NULL;
	MessageView *tmpview = messageview_create_with_new_window_visible(
				mainwindow_get_mainwindow(), FALSE);

	if (prefs_common.use_different_print_font) {
		font_desc = pango_font_description_from_string
						(prefs_common.printfont);
	} else {
		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
	}
	if (font_desc) {
		gtk_widget_override_font(tmpview->mimeview->textview->text, 
			font_desc);
		pango_font_description_free(font_desc);
	}

	tmpview->all_headers = all_headers;
	if (msginfo && messageview_show(tmpview, msginfo, 
		tmpview->all_headers) >= 0) {
			print_mimeview(tmpview->mimeview, 
				sel_start, sel_end, partnum);
	}
	messageview_clear(tmpview);
	messageview_destroy(tmpview);
}

static void page_setup_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	printing_page_setup(messageview ?
			    GTK_WINDOW(messageview->window) : NULL);
}

static void print_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	gint sel_start = -1, sel_end = -1, partnum = 0;

	if (!messageview->msginfo) return;

	partnum = mimeview_get_selected_part_num(messageview->mimeview);
	textview_get_selection_offsets(messageview->mimeview->textview,
		&sel_start, &sel_end);
	messageview_print(messageview->msginfo, messageview->all_headers, 
		sel_start, sel_end, partnum);
}

static void close_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview_destroy(messageview);
}

static void copy_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview_copy_clipboard(messageview);
}

static void allsel_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview_select_all(messageview);
}

static void search_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	message_search(messageview);
}

static void prev_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_prev(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void next_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_next(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void prev_unread_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_prev_unread(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void next_unread_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_next_unread(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void prev_new_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_prev_new(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void next_new_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_next_new(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void prev_marked_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_prev_marked(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void next_marked_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_next_marked(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void prev_labeled_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_prev_labeled(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void next_labeled_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_next_labeled(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void prev_history_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *info = messageview_nav_get_prev(messageview);
	if (info) {
		messageview->updating = TRUE;
		messageview_show(messageview, info, 
					 messageview->all_headers);
		messageview->updating = FALSE;
		procmsg_msginfo_free(&info);
		if (messageview->deferred_destroy) {
			debug_print("messageview got away!\n");
			messageview_destroy(messageview);
			return;
		}
	}
}

static void next_history_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *info = messageview_nav_get_next(messageview);
	if (info) {
		messageview->updating = TRUE;
		messageview_show(messageview, info, 
					 messageview->all_headers);
		messageview->updating = FALSE;
		procmsg_msginfo_free(&info);
		if (messageview->deferred_destroy) {
			debug_print("messageview got away!\n");
			messageview_destroy(messageview);
			return;
		}
	}
}

static void parent_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview->updating = TRUE;
	summary_select_parent(messageview->mainwin->summaryview);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void goto_unread_folder_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	messageview->updating = TRUE;
	folderview_select_next_with_flag(messageview->mainwin->folderview, MSG_UNREAD);
	messageview->updating = FALSE;

	if (messageview->deferred_destroy) {
		debug_print("messageview got away!\n");
		messageview_destroy(messageview);
		return;
	}
	if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
		MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
		if (msginfo)
			messageview_show(messageview, msginfo, 
					 messageview->all_headers);
#endif
	} else {
		gtk_widget_destroy(messageview->window);
	}
}

static void goto_folder_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	FolderItem *to_folder;

	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL, FALSE,
			_("Select folder to go to"));

	if (to_folder) {
		folderview_select(messageview->mainwin->folderview, to_folder);

		if (messageview->deferred_destroy) {
			debug_print("messageview got away!\n");
			messageview_destroy(messageview);
			return;
		}
		if (messageview->mainwin->summaryview->selected) {
#ifndef GENERIC_UMPC
			MsgInfo * msginfo = summary_get_selected_msg(messageview->mainwin->summaryview);
		       
			if (msginfo)
				messageview_show(messageview, msginfo, 
						 messageview->all_headers);
#endif
		} else {
			gtk_widget_destroy(messageview->window);
		}
	}
}

static void scroll_prev_line_cb(GtkAction *action, gpointer data)
{
        MessageView *messageview = (MessageView *)data;
        mimeview_scroll_one_line(messageview->mimeview,TRUE);
}

static void scroll_next_line_cb(GtkAction *action, gpointer data)
{
        MessageView *messageview = (MessageView *)data;
        mimeview_scroll_one_line(messageview->mimeview,FALSE);
}

static void scroll_prev_page_cb(GtkAction *action, gpointer data)
{
        MessageView *messageview = (MessageView *)data;
        mimeview_scroll_page(messageview->mimeview,TRUE);
}

static void scroll_next_page_cb(GtkAction *action, gpointer data)
{
        MessageView *messageview = (MessageView *)data;
        mimeview_scroll_page(messageview->mimeview,FALSE);
}

static void set_charset_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current));
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));
	const gchar *charset;

	if (active) {
		charset = conv_get_charset_str((CharSet)value);
		g_free(messageview->forced_charset);
		messageview->forced_charset = g_strdup(charset);
		procmime_force_charset(charset);
		
		messageview_show(messageview, messageview->msginfo, FALSE);
	}
}

static void set_decode_cb(GtkAction *action, GtkRadioAction *current, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	gboolean active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current));
	gint value = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (current));

	if (active) {
		messageview->forced_encoding = (EncodingType)value;

		messageview_show(messageview, messageview->msginfo, FALSE);
		debug_print("forced encoding: %d\n", value);
	}
}


static void view_source_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	SourceWindow *srcwin;

	if (!messageview->msginfo) return;

	srcwin = source_window_create();
	source_window_show_msg(srcwin, messageview->msginfo);
	source_window_show(srcwin);
}

static void show_all_header_cb(GtkToggleAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo = messageview->msginfo;

	if (messageview->mimeview->textview &&
	    messageview->mimeview->textview->loading) {
		return;
	}
	if (messageview->updating)
		return;

	messageview->all_headers = prefs_common.show_all_headers =
			gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (!msginfo) return;
	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo, messageview->all_headers);
	procmsg_msginfo_free(&msginfo);
	main_window_set_menu_sensitive(messageview->mainwin);
	summary_redisplay_msg(messageview->mainwin->summaryview);
}

static void msg_hide_quotes_cb(GtkToggleAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo = messageview->msginfo;
	static gboolean updating_menu = FALSE;

	if (updating_menu)
		return;
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		const gchar *a_name = gtk_action_get_name(GTK_ACTION(action));
		if (!strcmp(a_name, "View/Quotes/CollapseAll")) prefs_common.hide_quotes = 1;
		else if (!strcmp(a_name, "View/Quotes/Collapse2")) prefs_common.hide_quotes = 2;
		else if (!strcmp(a_name, "View/Quotes/Collapse3")) prefs_common.hide_quotes = 3;
	} else
		prefs_common.hide_quotes = 0;
	
	updating_menu=TRUE;
	
	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/CollapseAll", (prefs_common.hide_quotes == 1));
	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/Collapse2", (prefs_common.hide_quotes == 2));
	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/Collapse3", (prefs_common.hide_quotes == 3));

	updating_menu=FALSE;
	if (!msginfo) return;
	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo,
			 messageview->all_headers);
	procmsg_msginfo_free(&msginfo);
	
	/* update main window */
	main_window_set_menu_sensitive(messageview->mainwin);
	summary_redisplay_msg(messageview->mainwin->summaryview);
}
#undef SET_CHECK_MENU_ACTIVE

static void compose_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	PrefsAccount *ac = NULL;
	FolderItem *item = NULL;

	if (messageview->msginfo)
		item = messageview->msginfo->folder;

	if (item) {
		ac = account_find_from_item(item);
		if (ac && ac->protocol == A_NNTP &&
		    FOLDER_TYPE(item->folder) == F_NEWS) {
			compose_new(ac, item->path, NULL);
			return;
		}
	}

	compose_new(ac, NULL, NULL);
}

#define DO_ACTION(name, act)	{ if (!strcmp(a_name, name)) action = act; }

static void reply_cb(GtkAction *gaction, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	GSList *msginfo_list = NULL;
	gint action = COMPOSE_REPLY;
	const gchar *a_name = gtk_action_get_name(gaction);
	
	cm_return_if_fail(messageview->msginfo);

	DO_ACTION("Message/Reply", COMPOSE_REPLY);
	DO_ACTION("Message/ReplyTo/All", COMPOSE_REPLY_TO_ALL);
	DO_ACTION("Message/ReplyTo/Sender", COMPOSE_REPLY_TO_SENDER);
	DO_ACTION("Message/ReplyTo/List", COMPOSE_REPLY_TO_LIST);
	DO_ACTION("Message/Forward", COMPOSE_FORWARD_INLINE);
	DO_ACTION("Message/ForwardAtt", COMPOSE_FORWARD_AS_ATTACH);
	DO_ACTION("Message/Redirect", COMPOSE_REDIRECT);

	msginfo_list = g_slist_append(msginfo_list, messageview->msginfo);
	compose_reply_from_messageview(messageview, msginfo_list, action);
	g_slist_free(msginfo_list);
}

static void addressbook_open_cb(GtkAction *action, gpointer data)
{
#ifndef USE_ALT_ADDRBOOK
	addressbook_open(NULL);
#else
	GError* error = NULL;
	
	addressbook_dbus_open(FALSE, &error);
	if (error) {
		g_warning("failed to open address book: %s", error->message);
		g_error_free(error);
	}
#endif
}

static void add_address_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo, *full_msginfo;
	gchar *from;
	GdkPixbuf *picture = NULL;
	AvatarRender *avatarr;

	if (!messageview->msginfo || !messageview->msginfo->from) 
		return;

	msginfo = messageview->msginfo;
	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	
	full_msginfo = procmsg_msginfo_get_full_info(msginfo);

	avatarr = avatars_avatarrender_new(full_msginfo);
	hooks_invoke(AVATAR_IMAGE_RENDER_HOOKLIST, avatarr);

	procmsg_msginfo_free(&full_msginfo);

	if (avatarr->image != NULL)
		picture = gtk_image_get_pixbuf(GTK_IMAGE(avatarr->image));

#ifndef USE_ALT_ADDRBOOK
	addressbook_add_contact(msginfo->fromname, from, NULL, picture);
#else
	if (addressadd_selection(msginfo->fromname, from, NULL, picture)) {
		debug_print( "addressbook_add_contact - added\n" );
	}
#endif
	avatars_avatarrender_free(avatarr);
}

static void create_filter_cb(GtkAction *gaction, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	FolderItem * item;
	gint action = -1;
	const gchar *a_name = gtk_action_get_name(gaction);

	if (!messageview->msginfo) return;

	DO_ACTION("Tools/CreateFilterRule/Automatically", FILTER_BY_AUTO);
	DO_ACTION("Tools/CreateFilterRule/ByFrom", FILTER_BY_FROM);
	DO_ACTION("Tools/CreateFilterRule/ByTo", FILTER_BY_TO);
	DO_ACTION("Tools/CreateFilterRule/BySubject", FILTER_BY_SUBJECT);
	DO_ACTION("Tools/CreateFilterRule/BySender", FILTER_BY_SENDER);
	
	item = messageview->msginfo->folder;
	summary_msginfo_filter_open(item,  messageview->msginfo,
				    (PrefsFilterType)action, 0);
}

static void create_processing_cb(GtkAction *gaction, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	FolderItem * item;
	gint action = -1;
	const gchar *a_name = gtk_action_get_name(gaction);
	
	if (!messageview->msginfo) return;
	
	DO_ACTION("Tools/CreateProcessingRule/Automatically", FILTER_BY_AUTO);
	DO_ACTION("Tools/CreateProcessingRule/ByFrom", FILTER_BY_FROM);
	DO_ACTION("Tools/CreateProcessingRule/ByTo", FILTER_BY_TO);
	DO_ACTION("Tools/CreateProcessingRule/BySubject", FILTER_BY_SUBJECT);
	DO_ACTION("Tools/CreateProcessingRule/BySender", FILTER_BY_SENDER);

	item = messageview->msginfo->folder;
	summary_msginfo_filter_open(item,  messageview->msginfo,
				    (PrefsFilterType)action, 1);
}

static void open_urls_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;
	messageview_list_urls(messageview);
}

static void about_cb(GtkAction *gaction, gpointer data)
{
	about_show();
}

static gboolean messageview_update_msg(gpointer source, gpointer data)
{
	MsgInfoUpdate *msginfo_update = (MsgInfoUpdate *) source;
	MessageView *messageview = (MessageView *)data;
	MsgInfo *old_msginfo = messageview->msginfo;

	if (messageview->msginfo != msginfo_update->msginfo)
		return FALSE;

	if ((msginfo_update->flags & MSGINFO_UPDATE_DELETED) ||
	    MSG_IS_DELETED(old_msginfo->flags)) {
		if (messageview->new_window) {
			if (old_msginfo->folder && old_msginfo->folder->total_msgs == 0) {
				messageview_clear(messageview);
				textview_show_info(messageview->mimeview->textview,
					_("\n  There are no messages in this folder"));
				return FALSE;
			}
			
 			if (!OPEN_SELECTED_ON_DELETEMOVE && !OPEN_SELECTED_ON_PREVNEXT) {
 				messageview_clear(messageview);
  				textview_show_info(messageview->mimeview->textview,
  					MSG_IS_DELETED(old_msginfo->flags) ?
  					_("\n  Message has been deleted") :
  					_("\n  Message has been deleted or moved to another folder"));
			} else
				messageview->update_needed = TRUE;
		} else {
			messageview_clear(messageview);
			messageview_update(messageview, old_msginfo);
		}
	}

	return FALSE;
}

void messageview_set_menu_sensitive(MessageView *messageview)
{
	if (!messageview || !messageview->ui_manager)
		return;

	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/CollapseAll", (prefs_common.hide_quotes == 1));
	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/Collapse2", (prefs_common.hide_quotes == 2));
	cm_toggle_menu_set_active_full(messageview->ui_manager, "Menu/View/Quotes/Collapse3", (prefs_common.hide_quotes == 3));
	cm_menu_set_sensitive_full(messageview->ui_manager, "Menu/View/Goto/PrevHistory", messageview_nav_has_prev(messageview));
	cm_menu_set_sensitive_full(messageview->ui_manager, "Menu/View/Goto/NextHistory", messageview_nav_has_next(messageview));

	cm_menu_set_sensitive_full(messageview->ui_manager, "Menu/Message/CheckSignature", messageview->mimeview->signed_part);
}

void messageview_learn (MessageView *msgview, gboolean is_spam)
{
	if (is_spam) {
		if (procmsg_spam_learner_learn(msgview->msginfo, NULL, TRUE) == 0)
			procmsg_msginfo_set_flags(msgview->msginfo, MSG_SPAM, 0);
		else
			log_error(LOG_PROTOCOL, _("An error happened while learning.\n"));
		
	} else {
		if (procmsg_spam_learner_learn(msgview->msginfo, NULL, FALSE) == 0)
			procmsg_msginfo_unset_flags(msgview->msginfo, MSG_SPAM, 0);
		else
			log_error(LOG_PROTOCOL, _("An error happened while learning.\n"));
	}
	if (msgview->toolbar)
		toolbar_set_learn_button
			(msgview->toolbar,
			 MSG_IS_SPAM(msgview->msginfo->flags)?LEARN_HAM:LEARN_SPAM);
	else
		toolbar_set_learn_button
			(msgview->mainwin->toolbar,
			 MSG_IS_SPAM(msgview->msginfo->flags)?LEARN_HAM:LEARN_SPAM);
}

void messageview_list_urls (MessageView	*msgview)
{
	GSList *cur = msgview->mimeview->textview->uri_list;
	GSList *newlist = NULL;
	GHashTable *uri_hashtable;
	gchar *tmp;
	
	uri_hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,
					 (GDestroyNotify) g_free, NULL);
	
	for (; cur; cur = cur->next) {
		ClickableText *uri = (ClickableText *)cur->data;
		if (uri->uri &&
		    (!g_ascii_strncasecmp(uri->uri, "ftp.", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "ftp:", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "ftps:", 5) ||
		     !g_ascii_strncasecmp(uri->uri, "sftp:", 5) ||
		     !g_ascii_strncasecmp(uri->uri, "www.", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "webcal:", 7) ||
		     !g_ascii_strncasecmp(uri->uri, "webcals:", 8) ||
		     !g_ascii_strncasecmp(uri->uri, "http:", 5) ||
		     !g_ascii_strncasecmp(uri->uri, "https:", 6)))
		{
			tmp = g_utf8_strdown(uri->uri, -1);
			
			if (g_hash_table_lookup(uri_hashtable, tmp)) {
				g_free(tmp);
				continue;
			}
			
			newlist = g_slist_prepend(newlist, uri);
			g_hash_table_insert(uri_hashtable, tmp,
					    GUINT_TO_POINTER(g_str_hash(tmp)));
		}
	}
	newlist = g_slist_reverse(newlist);
	uri_opener_open(msgview, newlist);
	g_slist_free(newlist);
	g_hash_table_destroy(uri_hashtable);
}

static void save_part_as_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_save_as(messageview->mimeview);
}

static void view_part_as_text_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_display_as_text(messageview->mimeview);
}

static void open_part_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_launch(messageview->mimeview, NULL);
}
#ifndef G_OS_WIN32
static void open_part_with_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_open_with(messageview->mimeview);
}
#endif
static void check_signature_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_check_signature(messageview->mimeview);
}

static void goto_next_part_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_select_next_part(messageview->mimeview);
}

static void goto_prev_part_cb(GtkAction *action, gpointer data)
{
	MessageView *messageview = (MessageView *)data;

	if (messageview->mimeview)
		mimeview_select_prev_part(messageview->mimeview);
}

