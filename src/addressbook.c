/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkclist.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <string.h>
#include <setjmp.h>

#include "intl.h"
#include "main.h"
#include "addressbook.h"
#include "manage_window.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "menu.h"
#include "xml.h"
#include "prefs.h"
#include "procmime.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "addr_compl.h"

#include "addressitem.h"
#include "vcard.h"
#include "editvcard.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#include "editjpilot.h"
#endif

#ifdef USE_LDAP
#include <pthread.h>
#include "syldap.h"
#include "editldap.h"

// Interval to check for LDAP search results
// #define ADDRESSBOOK_LDAP_TIMER_INTERVAL	100
#define ADDRESSBOOK_LDAP_BUSYMSG	"Busy"

#endif

#include "pixmaps/dir-close.xpm"
#include "pixmaps/dir-open.xpm"
#include "pixmaps/group.xpm"
#include "pixmaps/vcard.xpm"
#ifdef USE_JPILOT
#include "pixmaps/jpilot.xpm"
#include "pixmaps/category.xpm"
#endif
#ifdef USE_LDAP
#include "pixmaps/ldap.xpm"
#endif

// XML tag names for top level folders
#define ADDRESS_TAG_COMMON    "common_address"
#define ADDRESS_TAG_PERSONAL  "personal_address"
#define ADDRESS_TAG_VCARD     "vcard_list"
#ifdef USE_JPILOT
#define ADDRESS_TAG_JPILOT    "jpilot_list"
#endif
#ifdef USE_LDAP
#define ADDRESS_TAG_LDAP      "ldap_list"
#endif

typedef enum
{
	COL_NAME	= 0,
	COL_ADDRESS	= 1,
	COL_REMARKS	= 2
} AddressBookColumnPos;

#define N_COLS	3
#define COL_NAME_WIDTH		144
#define COL_ADDRESS_WIDTH	156

#define COL_FOLDER_WIDTH	170
#define ADDRESSBOOK_WIDTH	640
#define ADDRESSBOOK_HEIGHT	360

#define ADDRESSBOOK_MSGBUF_SIZE 2048

static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *groupxpm;
static GdkBitmap *groupxpmmask;
static GdkPixmap *vcardxpm;
static GdkBitmap *vcardxpmmask;
#ifdef USE_JPILOT
static GdkPixmap *jpilotxpm;
static GdkBitmap *jpilotxpmmask;
static GdkPixmap *categoryxpm;
static GdkBitmap *categoryxpmmask;
#endif
#ifdef USE_LDAP
static GdkPixmap *ldapxpm;
static GdkBitmap *ldapxpmmask;
#endif

// Pilot library indicator (set at run-time)
static _have_pilot_library_;

// LDAP library indicator (set at run-time)
static _have_ldap_library_;

// Message buffer
static gchar addressbook_msgbuf[ ADDRESSBOOK_MSGBUF_SIZE ];

static AddressBook addrbook;

static struct _AddressEdit
{
	GtkWidget *window;
	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *rem_entry;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
} addredit;

static void addressbook_create			(gboolean show);
static gint addressbook_close			(void);
static void addressbook_button_set_sensitive	(void);

/* callback functions */
static void addressbook_del_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_reg_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_to_clicked		(GtkButton	*button,
						 gpointer	 data);
static void addressbook_lup_clicked		(GtkButton	*button,
						 gpointer	data);

static void addressbook_tree_selected		(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_list_selected		(GtkCList	*clist,
						 gint		 row,
						 gint		 column,
						 GdkEvent	*event,
						 gpointer	 data);
static void addressbook_entry_gotfocus		(GtkWidget	*widget);

#if 0
static void addressbook_entry_changed		(GtkWidget	*widget);
#endif

static void addressbook_list_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void addressbook_list_button_released	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static void addressbook_tree_button_pressed	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 gpointer	 data);
static void addressbook_tree_button_released	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 gpointer	 data);
static void addressbook_popup_close		(GtkMenuShell	*menu_shell,
						 gpointer	 data);

static void addressbook_new_folder_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_new_group_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_edit_folder_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_delete_folder_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

static void addressbook_change_node_name	(GtkCTreeNode	*node,
						 const gchar	*name);
static void addressbook_edit_group		(GtkCTreeNode	*group_node);

static void addressbook_edit_address_create	(gboolean	*cancelled);
static void edit_address_ok			(GtkWidget	*widget,
						 gboolean	*cancelled);
static void edit_address_cancel			(GtkWidget	*widget,
						 gboolean	*cancelled);
static gint edit_address_delete_event		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gboolean	*cancelled);
static void edit_address_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gboolean	*cancelled);
static AddressItem *addressbook_edit_address	(AddressItem	*item);

static void addressbook_new_address_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_edit_address_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_delete_address_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

static void close_cb				(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

// VCard edit stuff
static void addressbook_new_vcard_cb		( gpointer	data,
	       					  guint		action,
						  GtkWidget	*widget );

#ifdef USE_JPILOT
// JPilot edit stuff
static void addressbook_new_jpilot_cb		( gpointer	data,
	       					  guint		action,
						  GtkWidget	*widget );
#endif

#ifdef USE_LDAP
// LDAP edit stuff
static void addressbook_new_ldap_cb		( gpointer	data,
	       					  guint		action,
						  GtkWidget	*widget );
#endif

static AddressItem *addressbook_parse_address	(const gchar	*str);
static void addressbook_append_to_compose_entry	(AddressItem	*item,
						 ComposeEntryType type);

static void addressbook_set_clist		(AddressObject	*obj);

static void addressbook_read_file		(void);
static void addressbook_get_tree		(XMLFile	*file,
						 GtkCTreeNode	*node,
						 const gchar	*folder_tag);
static void addressbook_add_objs		(XMLFile	*file,
						 GtkCTreeNode	*node);

static GtkCTreeNode *addressbook_add_object	(GtkCTreeNode	*node,
						 AddressObject	*obj);
static void addressbook_delete_object		(AddressObject	*obj);
static AddressObject *addressbook_find_object_by_name
						(GtkCTreeNode	*node,
						 const gchar	*name);

static AddressItem *addressbook_parse_item	(XMLFile	*file);
static void addressbook_xml_recursive_write	(GtkCTreeNode	*node,
						 FILE		*fp);
static void addressbook_node_write_begin	(GtkCTreeNode	*node,
						 FILE		*fp);
static void addressbook_node_write_end		(GtkCTreeNode	*node,
						 FILE		*fp);
static void addressbook_write_items		(FILE		*fp,
						 GList		*items,
						 guint		 level);
static void tab_indent_out			(FILE		*fp,
						 guint		 level);

static void key_pressed				(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static gint addressbook_list_compare_func	(GtkCList	*clist,
						 gconstpointer	 ptr1,
						 gconstpointer	 ptr2);
static gint addressbook_obj_name_compare	(gconstpointer	 a,
						 gconstpointer	 b);

static AddressVCard *addressbook_parse_vcard	( XMLFile	*file );
static void addressbook_write_vcard		( FILE		*fp,
						AddressVCard	*vcard,
						guint		level );
static void addressbook_vcard_show_message	( VCardFile *vcf );

#ifdef USE_JPILOT
static AddressJPilot *addressbook_parse_jpilot	( XMLFile	*file );
static void addressbook_write_jpilot		( FILE		*fp,
	       					AddressJPilot	*jpilot,
					       	guint		level );
static void addressbook_jpilot_show_message	( JPilotFile *jpf );
#endif
#ifdef USE_LDAP
static AddressLDAP *addressbook_parse_ldap	( XMLFile	*file );
static void addressbook_write_ldap		( FILE		*fp,
	       					AddressLDAP	*ldapi,
					       	guint		level );
static void addressbook_ldap_show_message	( SyldapServer *server );
#endif

static GtkItemFactoryEntry addressbook_entries[] =
{
	{N_("/_File"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/New _Address"),	"<alt>N", addressbook_new_address_cb, 0, NULL},
	{N_("/_File/New _Group"),	"<alt>G", addressbook_new_group_cb,   0, NULL},
	{N_("/_File/New _Folder"),	"<alt>R", addressbook_new_folder_cb,  0, NULL},
	{N_("/_File/New _V-Card"),	"<alt>D", addressbook_new_vcard_cb,  0, NULL},
#ifdef USE_JPILOT
	{N_("/_File/New _J-Pilot"),	"<alt>J", addressbook_new_jpilot_cb,  0, NULL},
#endif
#ifdef USE_LDAP
	{N_("/_File/New _Server"),	"<alt>S", addressbook_new_ldap_cb,  0, NULL},
#endif
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Edit"),		"<alt>Return", addressbook_edit_address_cb, 0, NULL},
	{N_("/_File/_Delete"),		NULL, addressbook_delete_address_cb, 0, NULL},
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),		"<alt>W", close_cb, 0, NULL},
	{N_("/_Help"),			NULL, NULL, 0, "<LastBranch>"},
	{N_("/_Help/_About"),		NULL, about_show, 0, NULL}
};

static GtkItemFactoryEntry addressbook_tree_popup_entries[] =
{
	{N_("/New _Address"),	NULL, addressbook_new_address_cb, 0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,   0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,  0, NULL},
	{N_("/New _V-Card"),	NULL, addressbook_new_vcard_cb,   0, NULL},
#ifdef USE_JPILOT
	{N_("/New _J-Pilot"),	NULL, addressbook_new_jpilot_cb,  0, NULL},
#endif
#ifdef USE_LDAP
	{N_("/New _Server"),	NULL, addressbook_new_ldap_cb,  0, NULL},
#endif
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit"),		NULL, addressbook_edit_folder_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_delete_folder_cb, 0, NULL}
};

static GtkItemFactoryEntry addressbook_list_popup_entries[] =
{
	{N_("/New _Address"),	NULL, addressbook_new_address_cb,  0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,    0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,   0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit"),		NULL, addressbook_edit_address_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_delete_address_cb, 0, NULL}
};

void addressbook_open(Compose *target)
{
	if (!addrbook.window) {
		addressbook_create(TRUE);
		addressbook_read_file();
		addrbook.open_folder = TRUE;
		gtk_ctree_select(GTK_CTREE(addrbook.ctree),
				 GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list));
	} else
		gtk_widget_hide(addrbook.window);

	gtk_widget_show(addrbook.window);

	addressbook_set_target_compose(target);
}

void addressbook_set_target_compose(Compose *target)
{
	addrbook.target_compose = target;

	addressbook_button_set_sensitive();
}

Compose *addressbook_get_target_compose(void)
{
	return addrbook.target_compose;
}

static void addressbook_create(gboolean show)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *vbox2;
	GtkWidget *ctree_swin;
	GtkWidget *ctree;
	GtkWidget *clist_vbox;
	GtkWidget *clist_swin;
	GtkWidget *clist;
	GtkWidget *paned;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *statusbar;
	GtkWidget *hmbox;
	GtkWidget *hbbox;
	GtkWidget *hsbox;
	GtkWidget *del_btn;
	GtkWidget *reg_btn;
	GtkWidget *lup_btn;
	GtkWidget *to_btn;
	GtkWidget *cc_btn;
	GtkWidget *bcc_btn;
	GtkWidget *tree_popup;
	GtkWidget *list_popup;
	GtkItemFactory *tree_factory;
	GtkItemFactory *list_factory;
	GtkItemFactory *menu_factory;
	gint n_entries;

	gchar *titles[N_COLS] = {_("Name"), _("E-Mail address"), _("Remarks")};
	gchar *text;
	gint i;

	debug_print("Creating addressbook window...\n");

	// Global flag if we have library installed (at run-time)
	_have_pilot_library_ = FALSE;
	_have_ldap_library_ = FALSE;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Address book"));
	gtk_widget_set_usize(window, ADDRESSBOOK_WIDTH, ADDRESSBOOK_HEIGHT);
	//gtk_container_set_border_width(GTK_CONTAINER(window), BORDER_WIDTH);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	gtk_widget_realize(window);

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(addressbook_close), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_entries = sizeof(addressbook_entries) /
		sizeof(addressbook_entries[0]);
	menubar = menubar_create(window, addressbook_entries, n_entries,
				 "<AddressBook>", NULL);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);
	menu_factory = gtk_item_factory_from_widget(menubar);

	vbox2 = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);

	ctree_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctree_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(ctree_swin, COL_FOLDER_WIDTH + 40, -1);

	ctree = gtk_ctree_new(1, 0);
	gtk_container_add(GTK_CONTAINER(ctree_swin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_width(GTK_CLIST(ctree), 0, COL_FOLDER_WIDTH);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree),
				   addressbook_list_compare_func);

	gtk_signal_connect(GTK_OBJECT(ctree), "tree_select_row",
			   GTK_SIGNAL_FUNC(addressbook_tree_selected), NULL);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_press_event",
			   GTK_SIGNAL_FUNC(addressbook_tree_button_pressed),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_release_event",
			   GTK_SIGNAL_FUNC(addressbook_tree_button_released),
			   NULL);

	clist_vbox = gtk_vbox_new(FALSE, 4);

	clist_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(clist_vbox), clist_swin, TRUE, TRUE, 0);

	clist = gtk_clist_new_with_titles(N_COLS, titles);
	gtk_container_add(GTK_CONTAINER(clist_swin), clist);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_EXTENDED);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_NAME,
				   COL_NAME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_ADDRESS,
				   COL_ADDRESS_WIDTH);
	gtk_clist_set_compare_func(GTK_CLIST(clist),
				   addressbook_list_compare_func);

	for (i = 0; i < N_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[i].button,
				       GTK_CAN_FOCUS);

	gtk_signal_connect(GTK_OBJECT(clist), "select_row",
			   GTK_SIGNAL_FUNC(addressbook_list_selected), NULL);
	gtk_signal_connect(GTK_OBJECT(clist), "button_press_event",
			   GTK_SIGNAL_FUNC(addressbook_list_button_pressed),
			   NULL);
	gtk_signal_connect(GTK_OBJECT(clist), "button_release_event",
			   GTK_SIGNAL_FUNC(addressbook_list_button_released),
			   NULL);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(clist_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	address_completion_register_entry(GTK_ENTRY(entry));
	gtk_signal_connect(GTK_OBJECT(entry), "focus_in_event",
			   GTK_SIGNAL_FUNC(addressbook_entry_gotfocus), NULL);

#if 0
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(addressbook_entry_changed), NULL);
#endif

	paned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox2), paned, TRUE, TRUE, 0);
	gtk_paned_add1(GTK_PANED(paned), ctree_swin);
	gtk_paned_add2(GTK_PANED(paned), clist_vbox);

	// Status bar
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	// Button panel
	hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbbox), 2);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	del_btn = gtk_button_new_with_label(_("Delete"));
	GTK_WIDGET_SET_FLAGS(del_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), del_btn, TRUE, TRUE, 0);
	reg_btn = gtk_button_new_with_label(_("Add"));
	GTK_WIDGET_SET_FLAGS(reg_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), reg_btn, TRUE, TRUE, 0);
	lup_btn = gtk_button_new_with_label(_("Lookup"));
	GTK_WIDGET_SET_FLAGS(lup_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), lup_btn, TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(del_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_del_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(reg_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_reg_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(lup_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_lup_clicked), NULL);

	to_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("To:") : "To:");
	GTK_WIDGET_SET_FLAGS(to_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), to_btn, TRUE, TRUE, 0);
	cc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Cc:") : "Cc:");
	GTK_WIDGET_SET_FLAGS(cc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), cc_btn, TRUE, TRUE, 0);
	bcc_btn = gtk_button_new_with_label
		(prefs_common.trans_hdr ? _("Bcc:") : "Bcc:");
	GTK_WIDGET_SET_FLAGS(bcc_btn, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbbox), bcc_btn, TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(to_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_to_clicked),
			   GINT_TO_POINTER(COMPOSE_TO));
	gtk_signal_connect(GTK_OBJECT(cc_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_to_clicked),
			   GINT_TO_POINTER(COMPOSE_CC));
	gtk_signal_connect(GTK_OBJECT(bcc_btn), "clicked",
			   GTK_SIGNAL_FUNC(addressbook_to_clicked),
			   GINT_TO_POINTER(COMPOSE_BCC));

	PIXMAP_CREATE(window, folderxpm, folderxpmmask, DIRECTORY_CLOSE_XPM);
	PIXMAP_CREATE(window, folderopenxpm, folderopenxpmmask,
		      DIRECTORY_OPEN_XPM);
	PIXMAP_CREATE(window, groupxpm, groupxpmmask, group_xpm);
	PIXMAP_CREATE(window, vcardxpm, vcardxpmmask, vcard_xpm);
#ifdef USE_JPILOT
	PIXMAP_CREATE(window, jpilotxpm, jpilotxpmmask, jpilot_xpm);
	PIXMAP_CREATE(window, categoryxpm, categoryxpmmask, category_xpm);
#endif
#ifdef USE_LDAP
	PIXMAP_CREATE(window, ldapxpm, ldapxpmmask, ldap_xpm);
#endif

	text = _("Common address");
	addrbook.common =
		gtk_ctree_insert_node(GTK_CTREE(ctree),
				      NULL, NULL, &text, FOLDER_SPACING,
				      folderxpm, folderxpmmask,
				      folderopenxpm, folderopenxpmmask,
				      FALSE, FALSE);
	text = _("Personal address");
	addrbook.personal =
		gtk_ctree_insert_node(GTK_CTREE(ctree),
				      NULL, NULL, &text, FOLDER_SPACING,
				      folderxpm, folderxpmmask,
				      folderopenxpm, folderopenxpmmask,
				      FALSE, FALSE);

	text = _("V-Card");
	addrbook.vcard =
		gtk_ctree_insert_node(GTK_CTREE(ctree),
				      NULL, NULL, &text, FOLDER_SPACING,
				      folderxpm, folderxpmmask,
				      folderopenxpm, folderopenxpmmask,
				      FALSE, FALSE);

#ifdef USE_JPILOT
	text = _("J-Pllot");
	addrbook.jpilot =
		gtk_ctree_insert_node(GTK_CTREE(ctree),
				      NULL, NULL, &text, FOLDER_SPACING,
				      folderxpm, folderxpmmask,
				      folderopenxpm, folderopenxpmmask,
				      FALSE, FALSE);
	if( jpilot_test_pilot_lib() ) {
		_have_pilot_library_ = TRUE;
		menu_set_sensitive( menu_factory, "/File/New J-Pilot", TRUE );
	}
	else {
		menu_set_sensitive( menu_factory, "/File/New J-Pilot", FALSE );
	}
#endif

#ifdef USE_LDAP
	text = _("Directory");
	addrbook.ldap =
		gtk_ctree_insert_node(GTK_CTREE(ctree),
				      NULL, NULL, &text, FOLDER_SPACING,
				      folderxpm, folderxpmmask,
				      folderopenxpm, folderopenxpmmask,
				      FALSE, FALSE);
	if( syldap_test_ldap_lib() ) {
		_have_ldap_library_ = TRUE;
		menu_set_sensitive( menu_factory, "/File/New Server", TRUE );
	}
	else {
		menu_set_sensitive( menu_factory, "/File/New Server", FALSE );
	}
#endif

	/* popup menu */
	n_entries = sizeof(addressbook_tree_popup_entries) /
		sizeof(addressbook_tree_popup_entries[0]);
	tree_popup = menu_create_items(addressbook_tree_popup_entries,
				       n_entries,
				       "<AddressBookTree>", &tree_factory,
				       NULL);
	gtk_signal_connect(GTK_OBJECT(tree_popup), "selection_done",
			   GTK_SIGNAL_FUNC(addressbook_popup_close), NULL);
	n_entries = sizeof(addressbook_list_popup_entries) /
		sizeof(addressbook_list_popup_entries[0]);
	list_popup = menu_create_items(addressbook_list_popup_entries,
				       n_entries,
				       "<AddressBookList>", &list_factory,
				       NULL);

	addrbook.window  = window;
	addrbook.menubar = menubar;
	addrbook.ctree   = ctree;
	addrbook.clist   = clist;
	addrbook.entry   = entry;
	addrbook.statusbar = statusbar;
	addrbook.status_cid = gtk_statusbar_get_context_id( GTK_STATUSBAR(statusbar), "Addressbook Window" );

	addrbook.del_btn = del_btn;
	addrbook.reg_btn = reg_btn;
	addrbook.lup_btn = lup_btn;
	addrbook.to_btn  = to_btn;
	addrbook.cc_btn  = cc_btn;
	addrbook.bcc_btn = bcc_btn;

	addrbook.tree_popup   = tree_popup;
	addrbook.list_popup   = list_popup;
	addrbook.tree_factory = tree_factory;
	addrbook.list_factory = list_factory;
	addrbook.menu_factory = menu_factory;

	address_completion_start(window);

	if (show) 
		gtk_widget_show_all(window);
}

static gint addressbook_close(void)
{
	gtk_widget_hide(addrbook.window);
	addressbook_export_to_file();
	/* tell addr_compl that there's a new addressbook file */
	invalidate_address_completion();
	return TRUE;
}

static void addressbook_status_show( gchar *msg ) {
	if( addrbook.statusbar != NULL ) {
		gtk_statusbar_pop( GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid );
		if( msg ) {
			gtk_statusbar_push( GTK_STATUSBAR(addrbook.statusbar), addrbook.status_cid, msg );
		}
	}
}

static void addressbook_button_set_sensitive(void)
{
	gboolean to_sens  = FALSE;
	gboolean cc_sens  = FALSE;
	gboolean bcc_sens = FALSE;

	if (!addrbook.window) return;

	if (addrbook.target_compose) {
		to_sens = TRUE;
		cc_sens = TRUE;
		if (addrbook.target_compose->use_bcc)
			bcc_sens = TRUE;
	}

	gtk_widget_set_sensitive(addrbook.to_btn, to_sens);
	gtk_widget_set_sensitive(addrbook.cc_btn, cc_sens);
	gtk_widget_set_sensitive(addrbook.bcc_btn, bcc_sens);
}

static void addressbook_del_clicked(GtkButton *button, gpointer data)
{
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *pobj, *obj;
	GList *cur, *next;
	gint row;
	gboolean remFlag;

	if (!clist->selection) {
		addressbook_delete_folder_cb(NULL, 0, NULL);
		return;
	}

	pobj = gtk_ctree_node_get_row_data(ctree, addrbook.opened);
	g_return_if_fail(pobj != NULL);

	if (alertpanel(_("Delete address(es)"),
		       _("Really delete the address(es)?"),
		       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	for (cur = clist->selection; cur != NULL; cur = next) {
		next = cur->next;
		row = GPOINTER_TO_INT(cur->data);
		remFlag = FALSE;

		obj = gtk_clist_get_row_data(clist, row);
		if (!obj) continue;

		if (pobj->type == ADDR_GROUP) {
			AddressGroup *group = ADDRESS_GROUP(pobj);
			group->items = g_list_remove(group->items, obj);
		} else if (pobj->type == ADDR_FOLDER) {
			AddressFolder *folder = ADDRESS_FOLDER(pobj);

			folder->items = g_list_remove(folder->items, obj);
			if (obj->type == ADDR_GROUP) {
				remFlag = TRUE;
			}
			else if (obj->type == ADDR_VCARD) {
				remFlag = TRUE;
			}
			else if (obj->type == ADDR_JPILOT) {
				remFlag = TRUE;
			}
			else if (obj->type == ADDR_LDAP) {
				remFlag = TRUE;
			}

			if( remFlag ) {
				GtkCTreeNode *node;
				node = gtk_ctree_find_by_row_data
					(ctree, addrbook.opened, obj);
				if (node) gtk_ctree_remove_node(ctree, node);
			}
		} else
			continue;

		addressbook_delete_object(obj);

		gtk_clist_remove(clist, row);
	}
}

static void addressbook_reg_clicked(GtkButton *button, gpointer data)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkEntry *entry = GTK_ENTRY(addrbook.entry);
	AddressObject *obj;
	AddressItem *item;
	gchar *str;

	if (*gtk_entry_get_text(entry) == '\0') {
		addressbook_new_address_cb(NULL, 0, NULL);
		return;
	}
	if (!addrbook.opened) return;

	obj = gtk_ctree_node_get_row_data(ctree, addrbook.opened);
	if (!obj) return;

	g_return_if_fail(obj->type == ADDR_GROUP || obj->type == ADDR_FOLDER);

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	item = addressbook_parse_address(str);
	g_free(str);
	if (item) {
		if (addressbook_find_object_by_name
			(addrbook.opened, item->name) != NULL) {
			addressbook_delete_object(ADDRESS_OBJECT(item));
			item = NULL;
		} else if (addressbook_edit_address(item) == NULL) {
			addressbook_delete_object(ADDRESS_OBJECT(item));
			return;
		}
	}

	if (!item) {
		item = addressbook_edit_address(NULL);
		if (!item) return;
	}

	if (addressbook_find_object_by_name(addrbook.opened, item->name)) {
		addressbook_delete_object(ADDRESS_OBJECT(item));
		return;
	}

	addressbook_add_object(addrbook.opened, ADDRESS_OBJECT(item));
	addrbook.open_folder = TRUE;
	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
}

static AddressItem *addressbook_parse_address(const gchar *str)
{
	gchar *name    = NULL;
	gchar *address = NULL;
	AddressItem *item;
	gchar *buf;
	gchar *start, *end;

	Xalloca(buf, strlen(str) + 1, return NULL);

	strcpy(buf, str);
	g_strstrip(buf);
	if (*buf == '\0') return NULL;

	if ((start = strchr(buf, '<'))) {
		if (start > buf) {
			*start = '\0';
			g_strstrip(buf);
			if (*buf != '\0')
				name = g_strdup(buf);
		}
		start++;
		if ((end = strchr(start, '>'))) {
			*end = '\0';
			g_strstrip(start);
			if (*start != '\0')
				address = g_strdup(start);
		}
	} else
		name = g_strdup(buf);

	if (!name && !address) return NULL;

	item = mgu_create_address();
	ADDRESS_OBJECT_TYPE(item) = ADDR_ITEM;
	item->name    = name;
	item->address = address;
	item->remarks = NULL;

	return item;
}

static void addressbook_to_clicked(GtkButton *button, gpointer data)
{
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	GList *cur;

	if (!addrbook.target_compose) return;

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		AddressObject *obj;

		obj = gtk_clist_get_row_data(clist,
					     GPOINTER_TO_INT(cur->data));
		if (!obj) return;

		if (obj->type == ADDR_ITEM) {
			addressbook_append_to_compose_entry
				(ADDRESS_ITEM(obj), (ComposeEntryType)data);
		} else if (obj->type == ADDR_GROUP) {
			AddressGroup *group;
			GList *cur_item;

			group = ADDRESS_GROUP(obj);
			for (cur_item = group->items; cur_item != NULL;
			     cur_item = cur_item->next) {
				if (ADDRESS_OBJECT(cur_item->data)->type
				    != ADDR_ITEM)
					continue;
				addressbook_append_to_compose_entry
					(ADDRESS_ITEM(cur_item->data),
					 (ComposeEntryType)data);
			}
		}
	}
}

static void addressbook_append_to_compose_entry(AddressItem *item,
						ComposeEntryType type)
{
	Compose *compose = addrbook.target_compose;

	if (item->name && item->address) {
		gchar *buf;

		buf = g_strdup_printf
			("%s <%s>", item->name, item->address);
		compose_entry_append(compose, buf, type);
		g_free(buf);
	} else if (item->address)
		compose_entry_append(compose, item->address, type);
}

static void addressbook_menubar_set_sensitive( gboolean sensitive ) {
	menu_set_sensitive( addrbook.menu_factory, "/File/New Address", sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/New Group",   sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/New Folder",  sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/New V-Card",  sensitive );
#ifdef USE_JPILOT
	menu_set_sensitive( addrbook.menu_factory, "/File/New J-Pilot", sensitive );
#endif
#ifdef USE_LDAP
	menu_set_sensitive( addrbook.menu_factory, "/File/New Server",  sensitive );
#endif
	gtk_widget_set_sensitive( addrbook.reg_btn, sensitive );
	gtk_widget_set_sensitive( addrbook.del_btn, sensitive );
}

static void addressbook_menuitem_set_sensitive( AddressObject *obj, GtkCTreeNode *node ) {
	gboolean canEdit = TRUE;
	if( obj->type == ADDR_FOLDER ) {
		if( node == addrbook.common ) {
			canEdit = FALSE;
		}
		if( node == addrbook.personal ) {
			canEdit = FALSE;
		}
		if( node == addrbook.vcard ) {
			canEdit = FALSE;
			menu_set_sensitive( addrbook.menu_factory, "/File/New V-Card", TRUE );
		}
#ifdef USE_JPILOT
		else if( node == addrbook.jpilot ) {
			canEdit = FALSE;
			if( _have_pilot_library_ ) {
				menu_set_sensitive( addrbook.menu_factory, "/File/New J-Pilot", TRUE );
			}
		}
#endif
#ifdef USE_LDAP
		else if( node == addrbook.ldap ) {
			canEdit = FALSE;
			if( _have_ldap_library_ ) {
				menu_set_sensitive( addrbook.menu_factory, "/File/New Server", TRUE );
			}
		}
#endif
		else {
			menu_set_sensitive( addrbook.menu_factory, "/File/New Address", TRUE );
			menu_set_sensitive( addrbook.menu_factory, "/File/New Group",   TRUE );
			menu_set_sensitive( addrbook.menu_factory, "/File/New Folder",  TRUE );
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
			gtk_widget_set_sensitive( addrbook.del_btn, TRUE );
		}
	}
	else if( obj->type == ADDR_GROUP ) {
		menu_set_sensitive( addrbook.menu_factory, "/File/New Address", TRUE );
		gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
		gtk_widget_set_sensitive( addrbook.del_btn, TRUE );
	}
#ifdef USE_JPILOT
	else if( obj->type == ADDR_JPILOT ) {
		if( ! _have_pilot_library_ ) canEdit = FALSE;
	}
	else if( obj->type == ADDR_CATEGORY ) {
		canEdit = FALSE;
	}
#endif
#ifdef USE_LDAP
	else if( obj->type == ADDR_LDAP ) {
		if( ! _have_ldap_library_ ) canEdit = FALSE;
	}
#endif
	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",    canEdit );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete",  canEdit );
}

static void addressbook_tree_selected(GtkCTree *ctree, GtkCTreeNode *node,
				      gint column, gpointer data)
{
	AddressObject *obj;

	addrbook.selected = node;
	addrbook.open_folder = FALSE;
	addressbook_status_show( "" );
	if( addrbook.entry != NULL ) {
		gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");
	}

	obj = gtk_ctree_node_get_row_data(ctree, node);
	if( obj == NULL ) return;

	addrbook.opened = node;

	if(	obj->type == ADDR_GROUP || obj->type == ADDR_FOLDER ||
		obj->type == ADDR_VCARD || obj->type == ADDR_JPILOT ||
	        obj->type == ADDR_CATEGORY || obj->type == ADDR_LDAP ) {
		addressbook_set_clist(obj);
	}

	if( obj->type == ADDR_VCARD ) {
		// Read from file
		VCardFile *vcf;
		vcf = ADDRESS_VCARD(obj)->cardFile;
		vcard_read_data( vcf );
		addressbook_vcard_show_message( vcf );
		ADDRESS_VCARD(obj)->items = vcard_get_address_list( vcf );
		addressbook_set_clist( obj );
	}
#ifdef USE_JPILOT
	else if( obj->type == ADDR_JPILOT ) {
		if( _have_pilot_library_ ) {
			// Read from file
			JPilotFile *jpf;
			GList *catList, *catNode;
			AddressCategory *acat;
			GtkCTreeNode *childNode, *nextNode;
			GtkCTreeRow *currRow;

			jpf = ADDRESS_JPILOT(obj)->pilotFile;
			addressbook_jpilot_show_message( jpf );
			if( jpilot_get_modified( jpf ) ) {
				jpilot_read_data( jpf );
				catList = jpilot_get_category_items( jpf );

				// Remove existing categories
				currRow = GTK_CTREE_ROW( node );
				if( currRow ) {
					while( nextNode = currRow->children ) {
						gtk_ctree_remove_node( ctree, nextNode );
					}
				}

				// Load new categories into the tree.
				catNode = catList;
				while( catNode ) {
					AddressItem *item = catNode->data;
					acat = g_new(AddressCategory, 1);
					ADDRESS_OBJECT_TYPE(acat) = ADDR_CATEGORY;
					acat->name = g_strdup( item->name );
					acat->items = NULL;
					acat->pilotFile = jpf;
					acat->category = item;
					catNode = g_list_next( catNode );
					addressbook_add_object(node, ADDRESS_OBJECT(acat));
				}

				ADDRESS_JPILOT(obj)->items = catList;
			}
			addressbook_set_clist( obj );
		}
	}
	else if( obj->type == ADDR_CATEGORY ) {
		if( _have_pilot_library_ ) {
			// Read from file
			JPilotFile *jpf;

			jpf = ADDRESS_JPILOT(obj)->pilotFile;
			if( jpilot_get_modified( jpf ) ) {
				// Force parent to be reloaded
				gtk_ctree_select( GTK_CTREE(addrbook.ctree), GTK_CTREE_ROW(node)->parent);
				gtk_ctree_expand( GTK_CTREE(addrbook.ctree), GTK_CTREE_ROW(node)->parent);
			}
			else {
				AddressItem *item = NULL;
				AddressCategory *acat = ADDRESS_CATEGORY(obj);
				if( acat ) item = acat->category;
				if( item ) {
					ADDRESS_CATEGORY(obj)->items =
						jpilot_get_address_list_cat( jpf, item->categoryID );
				}
				addressbook_set_clist( obj );
			}
		}
	}
#endif
#ifdef USE_LDAP
	else if( obj->type == ADDR_LDAP ) {
		if( _have_ldap_library_ ) {
			// Read from cache
			SyldapServer *server;
			server = ADDRESS_LDAP(obj)->ldapServer;
			addressbook_ldap_show_message( server );
			if( ! server->busyFlag ) {
				ADDRESS_LDAP(obj)->items = syldap_get_address_list( server );
				addressbook_set_clist( obj );
			}
		}
	}
#endif

	// Setup main menu selections
	addressbook_menubar_set_sensitive( FALSE );
	addressbook_menuitem_set_sensitive( obj, node );
}

static void addressbook_list_selected(GtkCList *clist, gint row, gint column,
				      GdkEvent *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(addrbook.entry);
	AddressObject *obj;
	GList *cur;

	if (event && event->type == GDK_2BUTTON_PRESS) {
		if (prefs_common.add_address_by_click &&
		    addrbook.target_compose)
			addressbook_to_clicked(NULL, NULL);
		else
			addressbook_edit_address_cb(NULL, 0, NULL);
		return;
	}

#if 0
	gtk_signal_handler_block_by_func
		(GTK_OBJECT(entry),
		 GTK_SIGNAL_FUNC(addressbook_entry_changed), NULL);
#endif		 

	gtk_entry_set_text(entry, "");

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		obj = gtk_clist_get_row_data(clist,
					     GPOINTER_TO_INT(cur->data));
		g_return_if_fail(obj != NULL);

		if (obj->type == ADDR_ITEM) {
			AddressItem *item;

			item = ADDRESS_ITEM(obj);
			if (item->name && item->address) {
				gchar *buf;

				buf = g_strdup_printf
					("%s <%s>", item->name, item->address);
				if (*gtk_entry_get_text(entry) != '\0')
					gtk_entry_append_text(entry, ", ");
				gtk_entry_append_text(entry, buf);
				g_free(buf);
			} else if (item->address) {
				if (*gtk_entry_get_text(entry) != '\0')
					gtk_entry_append_text(entry, ", ");
				gtk_entry_append_text(entry, item->address);
			}
		}
	}

#if 0
	gtk_signal_handler_unblock_by_func
		(GTK_OBJECT(entry),
		 GTK_SIGNAL_FUNC(addressbook_entry_changed), NULL);
#endif		 
}

#if 0
static void addressbook_entry_changed(GtkWidget *widget)
{
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	GtkEntry *entry = GTK_ENTRY(addrbook.entry);
	const gchar *str;
	gint len;
	gint row;

	//if (clist->selection && clist->selection->next) return;

	str = gtk_entry_get_text(entry);
	if (*str == '\0') {
		gtk_clist_unselect_all(clist);
		return;
	}
	len = strlen(str);

	for (row = 0; row < clist->rows; row++) {
		AddressObject *obj;
		const gchar *name;

		obj = ADDRESS_OBJECT(gtk_clist_get_row_data(clist, row));
		if (!obj) continue;
		if (obj->type == ADDR_ITEM)
			name = ADDRESS_ITEM(obj)->name;
		else if (obj->type == ADDR_GROUP)
			name = ADDRESS_GROUP(obj)->name;
		else
			continue;

		if (name && !strncasecmp(name, str, len)) {
			gtk_clist_unselect_all(clist);
			gtk_clist_select_row(clist, row, -1);
			return;
		}
	}

	gtk_clist_unselect_all(clist);
}
#endif

static void addressbook_entry_gotfocus( GtkWidget *widget ) {
	gtk_editable_select_region( GTK_EDITABLE(addrbook.entry), 0, -1 );
}

static void addressbook_list_button_pressed(GtkWidget *widget,
					    GdkEventButton *event,
					    gpointer data)
{
	GtkCList *clist = GTK_CLIST(widget);
	gint row, column;
	gint tRow, tCol;
	AddressObject *obj, *pobj;

	if (!event) return;

	obj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree),
					  addrbook.opened);
	g_return_if_fail(obj != NULL);

	pobj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), addrbook.selected);
	if( pobj ) {
		if(	pobj->type == ADDR_VCARD ||
			pobj->type == ADDR_JPILOT ||
			pobj->type == ADDR_CATEGORY ||
			pobj->type == ADDR_LDAP ) {
			menu_set_sensitive(addrbook.menu_factory, "/File/Edit", FALSE);
			menu_set_sensitive(addrbook.menu_factory, "/File/Delete", FALSE);
		}
	}

	if (event->button != 3) return;
	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.list_popup));

	if (gtk_clist_get_selection_info
		(clist, event->x, event->y, &row, &column)) {
		GtkCListRow *clist_row;

		clist_row = g_list_nth(clist->row_list, row)->data;
		if (clist_row->state != GTK_STATE_SELECTED) {
			gtk_clist_unselect_all(clist);
			gtk_clist_select_row(clist, row, column);
		}
		gtkut_clist_set_focus_row(clist, row);

		if(	obj->type != ADDR_VCARD &&
			obj->type != ADDR_JPILOT &&
			obj->type != ADDR_CATEGORY &&
			obj->type != ADDR_LDAP ) {
			menu_set_sensitive(addrbook.list_factory, "/Edit", TRUE);
			menu_set_sensitive(addrbook.list_factory, "/Delete", TRUE);
		}
	}

	if( !(	addrbook.opened == addrbook.vcard ||
		addrbook.opened == addrbook.jpilot ||
		addrbook.opened == addrbook.ldap ) ) {

		if( obj->type == ADDR_FOLDER || obj->type == ADDR_GROUP ) {
			menu_set_sensitive(addrbook.list_factory, "/New Address", TRUE);
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
			gtk_widget_set_sensitive( addrbook.del_btn, TRUE );
		}
		if (obj->type == ADDR_FOLDER) {
			menu_set_sensitive(addrbook.list_factory, "/New Folder", TRUE);
			menu_set_sensitive(addrbook.list_factory, "/New Group", TRUE);
		}
	}
	gtk_menu_popup(GTK_MENU(addrbook.list_popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);
}

static void addressbook_list_button_released(GtkWidget *widget,
					     GdkEventButton *event,
					     gpointer data)
{
}

static void addressbook_tree_button_pressed(GtkWidget *ctree,
					    GdkEventButton *event,
					    gpointer data)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row, column;
	AddressObject *obj;
	GtkCTreeNode *node;

	if (!event) return;
	if (event->button == 1) {
		addrbook.open_folder = TRUE;
		return;
	}
	if (event->button != 3) return;

	if (!gtk_clist_get_selection_info
		(clist, event->x, event->y, &row, &column)) return;
	gtk_clist_select_row(clist, row, column);

	obj = gtk_clist_get_row_data(clist, row);
	g_return_if_fail(obj != NULL);

	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.tree_popup));

	if (obj->type == ADDR_FOLDER) {
		node = gtk_ctree_node_nth(GTK_CTREE(ctree), row);
		if( node == addrbook.vcard ) {
			menu_set_sensitive(addrbook.tree_factory, "/New V-Card", TRUE);
		}
#ifdef USE_JPILOT
		else if( node == addrbook.jpilot ) {
			if( _have_pilot_library_ ) {
				menu_set_sensitive(addrbook.tree_factory, "/New J-Pilot", TRUE);
			}
		}
#endif
#ifdef USE_LDAP
		else if( node == addrbook.ldap ) {
			if( _have_ldap_library_ ) {
				menu_set_sensitive(addrbook.tree_factory, "/New Server", TRUE);
			}
		}
#endif
		else {
			menu_set_sensitive(addrbook.tree_factory, "/New Address", TRUE);
			menu_set_sensitive(addrbook.tree_factory, "/New Folder", TRUE);
			menu_set_sensitive(addrbook.tree_factory, "/New Group", TRUE);
			if (node && GTK_CTREE_ROW(node)->level >= 2) {
				menu_set_sensitive(addrbook.tree_factory, "/Edit", TRUE);
				menu_set_sensitive(addrbook.tree_factory, "/Delete", TRUE);
			}
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
			gtk_widget_set_sensitive( addrbook.del_btn, TRUE );
		}
	}
	else if (obj->type == ADDR_GROUP) {
		menu_set_sensitive(addrbook.tree_factory, "/New Address", TRUE);
		menu_set_sensitive(addrbook.tree_factory, "/Edit", TRUE);
		menu_set_sensitive(addrbook.tree_factory, "/Delete", TRUE);
		gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
		gtk_widget_set_sensitive( addrbook.del_btn, TRUE );
	}
	else if (obj->type == ADDR_VCARD) {
		menu_set_sensitive(addrbook.tree_factory, "/Edit", TRUE);
		menu_set_sensitive(addrbook.tree_factory, "/Delete", TRUE);
	}
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		if( _have_pilot_library_ ) {
			menu_set_sensitive(addrbook.tree_factory, "/Edit", TRUE);
			menu_set_sensitive(addrbook.tree_factory, "/Delete", TRUE);
		}
	}
	else if (obj->type == ADDR_CATEGORY) {
		if( _have_pilot_library_ ) {
			menu_set_sensitive(addrbook.tree_factory, "/Edit", FALSE);
			menu_set_sensitive(addrbook.tree_factory, "/Delete", FALSE);
		}
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		if( _have_ldap_library_ ) {
			menu_set_sensitive(addrbook.tree_factory, "/Edit", TRUE);
			menu_set_sensitive(addrbook.tree_factory, "/Delete", TRUE);
		}
	}
#endif
	else {
		return;
	}
	gtk_menu_popup(GTK_MENU(addrbook.tree_popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);
}

static void addressbook_tree_button_released(GtkWidget *ctree,
					     GdkEventButton *event,
					     gpointer data)
{
	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(addrbook.ctree), addrbook.opened);
}

static void addressbook_popup_close(GtkMenuShell *menu_shell, gpointer data)
{
	if (!addrbook.opened) return;

	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(addrbook.ctree),
				  addrbook.opened);
}

static void addressbook_new_folder_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressFolder *folder;
	gchar *new_folder;

	if (!addrbook.selected) return;

	obj = gtk_ctree_node_get_row_data(ctree, addrbook.selected);
	g_return_if_fail(obj != NULL);
	if (obj->type != ADDR_FOLDER) return;

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	g_strstrip(new_folder);
	if (*new_folder == '\0') {
		g_free(new_folder);
		return;
	}

	if (gtk_ctree_find_by_row_data_custom(ctree, addrbook.selected,
					      new_folder,
					      addressbook_obj_name_compare)) {
		alertpanel_error(_("The name already exists."));
		g_free(new_folder);
		return;
	}

	folder = g_new(AddressFolder, 1);
	ADDRESS_OBJECT_TYPE(folder) = ADDR_FOLDER;
	folder->name = g_strdup(new_folder);
	folder->items = NULL;

	addressbook_add_object(addrbook.selected, ADDRESS_OBJECT(folder));

	g_free(new_folder);

	if (addrbook.selected == addrbook.opened)
		addressbook_set_clist(obj);
}

static void addressbook_new_group_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressGroup *group;
	gchar *new_group;

	if (!addrbook.selected) return;

	obj = gtk_ctree_node_get_row_data(ctree, addrbook.selected);
	g_return_if_fail(obj != NULL);
	if (obj->type != ADDR_FOLDER) return;

	new_group = input_dialog(_("New group"),
				 _("Input the name of new group:"),
				  _("NewGroup"));
	if (!new_group) return;
	g_strstrip(new_group);
	if (*new_group == '\0') {
		g_free(new_group);
		return;
	}

	if (gtk_ctree_find_by_row_data_custom(ctree, addrbook.selected,
					      new_group,
					      addressbook_obj_name_compare)) {
		alertpanel_error(_("The name already exists."));
		g_free(new_group);
		return;
	}

	group = g_new(AddressGroup, 1);
	ADDRESS_OBJECT_TYPE(group) = ADDR_GROUP;
	group->name = g_strdup(new_group);
	group->items = NULL;

	addressbook_add_object(addrbook.selected, ADDRESS_OBJECT(group));

	g_free(new_group);

	if (addrbook.selected == addrbook.opened)
		addressbook_set_clist(obj);
}

static void addressbook_change_node_name(GtkCTreeNode *node, const gchar *name)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	gchar *text[1];
	guint8 spacing;
	GdkPixmap *pix_cl, *pix_op;
	GdkBitmap *mask_cl, *mask_op;
	gboolean is_leaf, expanded;

	gtk_ctree_get_node_info(ctree, node, text, &spacing,
				&pix_cl, &mask_cl, &pix_op, &mask_op,
				&is_leaf, &expanded);
	gtk_ctree_set_node_info(ctree, node, name, spacing,
				pix_cl, mask_cl, pix_op, mask_op,
				is_leaf, expanded);
}

static void addressbook_edit_group(GtkCTreeNode *group_node)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	AddressObject *obj;
	AddressGroup *group;
	gchar *new_name;
	GtkCTreeNode *node;

	if (!group_node && clist->selection) {
		obj = gtk_clist_get_row_data(clist,
					     GPOINTER_TO_INT(clist->selection->data));
		g_return_if_fail(obj != NULL);
		if (obj->type != ADDR_GROUP) return;
		node = gtk_ctree_find_by_row_data
			(ctree, addrbook.selected, obj);
		if (!node) return;
	} else {
		if (group_node)
			node = group_node;
		else
			node = addrbook.selected;
		obj = gtk_ctree_node_get_row_data(ctree, node);
		g_return_if_fail(obj != NULL);
		if (obj->type != ADDR_GROUP) return;
	}

	group = ADDRESS_GROUP(obj);

	new_name = input_dialog(_("Edit group"),
				_("Input the new name of group:"),
				group->name);
	if (!new_name) return;
	g_strstrip(new_name);
	if (*new_name == '\0') {
		g_free(new_name);
		return;
	}

	if (gtk_ctree_find_by_row_data_custom(ctree, addrbook.selected,
					      new_name,
					      addressbook_obj_name_compare)) {
		alertpanel_error(_("The name already exists."));
		g_free(new_name);
		return;
	}

	g_free(group->name);
	group->name = g_strdup(new_name);

	addressbook_change_node_name(node, new_name);
	gtk_ctree_sort_node(ctree, GTK_CTREE_ROW(node)->parent);

	g_free(new_name);

	addrbook.open_folder = TRUE;
	gtk_ctree_select(ctree, addrbook.opened);
}

static void addressbook_edit_folder_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressFolder *folder;
	gchar *new_name = NULL;
	GtkCTreeNode *node = NULL, *parentNode = NULL;

	if (!addrbook.selected) return;
	if (GTK_CTREE_ROW(addrbook.selected)->level == 1) return;

	obj = gtk_ctree_node_get_row_data(ctree, addrbook.selected);
	g_return_if_fail(obj != NULL);
	g_return_if_fail(obj->type == ADDR_FOLDER || obj->type == ADDR_GROUP ||
			obj->type == ADDR_VCARD || obj->type == ADDR_JPILOT ||
			obj->type == ADDR_CATEGORY || obj->type == ADDR_LDAP );

	if (obj->type == ADDR_GROUP) {
		addressbook_edit_group(addrbook.selected);
		return;
	}

	if( obj->type == ADDR_VCARD ) {
                AddressVCard *vcard = ADDRESS_VCARD(obj);
                if( addressbook_edit_vcard( vcard ) == NULL ) return;
		new_name = vcard->name;
		parentNode = addrbook.vcard;
	}
#ifdef USE_JPILOT
	else if( obj->type == ADDR_JPILOT ) {
                AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		if( ! _have_pilot_library_ ) return;
                if( addressbook_edit_jpilot( jpilot ) == NULL ) return;
		new_name = jpilot->name;
		parentNode = addrbook.jpilot;
	}
#endif
#ifdef USE_LDAP
	else if( obj->type == ADDR_LDAP ) {
                AddressLDAP *ldapi = ADDRESS_LDAP(obj);
		if( ! _have_ldap_library_ ) return;
                if( addressbook_edit_ldap( ldapi ) == NULL ) return;
		new_name = ldapi->name;
		parentNode = addrbook.ldap;
	}
#endif

	if( new_name && parentNode) {
		// Update node in tree view
		node = gtk_ctree_find_by_row_data( ctree, addrbook.selected, obj );
		if( ! node ) return;
		addressbook_change_node_name( node, new_name );
		gtk_ctree_sort_node(ctree, parentNode);
		addrbook.open_folder = TRUE;
		gtk_ctree_select( GTK_CTREE(addrbook.ctree), node );
		return;
	}

	folder = ADDRESS_FOLDER(obj);
	new_name = input_dialog(_("Edit folder"),
				_("Input the new name of folder:"),
				folder->name);

	if (!new_name) return;
	g_strstrip(new_name);
	if (*new_name == '\0') {
		g_free(new_name);
		return;
	}

	if (gtk_ctree_find_by_row_data_custom(ctree, addrbook.selected,
					      new_name,
					      addressbook_obj_name_compare)) {
		alertpanel_error(_("The name already exists."));
		g_free(new_name);
		return;
	}

	g_free(folder->name);
	folder->name = g_strdup(new_name);

	addressbook_change_node_name(addrbook.selected, new_name);
	gtk_ctree_sort_node(ctree, GTK_CTREE_ROW(addrbook.selected)->parent);

	g_free(new_name);
}

static void addressbook_delete_folder_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj, *pobj;
	gchar *name;
	gchar *message;
	AlertValue aval;

	if (!addrbook.selected) return;
	if (GTK_CTREE_ROW(addrbook.selected)->level == 1) return;

	obj = gtk_ctree_node_get_row_data(ctree, addrbook.selected);
	g_return_if_fail(obj != NULL);

	if (obj->type == ADDR_GROUP)
		name = ADDRESS_GROUP(obj)->name;
	else if (obj->type == ADDR_FOLDER)
		name = ADDRESS_FOLDER(obj)->name;
	else if (obj->type == ADDR_VCARD)
		name = ADDRESS_VCARD(obj)->name;
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		if( ! _have_pilot_library_ ) return;
		name = ADDRESS_JPILOT(obj)->name;
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		if( ! _have_ldap_library_ ) return;
		name = ADDRESS_LDAP(obj)->name;
	}
#endif
	else
		return;

	message = g_strdup_printf(_("Really delete `%s' ?"), name);
	aval = alertpanel(_("Delete"), message, _("Yes"), _("No"), NULL);
	g_free(message);
	if (aval != G_ALERTDEFAULT) return;

	pobj = gtk_ctree_node_get_row_data
		(ctree, GTK_CTREE_ROW(addrbook.selected)->parent);
	if (!pobj) return;
	g_return_if_fail(pobj->type == ADDR_FOLDER);
	ADDRESS_FOLDER(pobj)->items =
		g_list_remove(ADDRESS_FOLDER(pobj)->items, obj);

	addressbook_delete_object(obj);
	addrbook.open_folder = TRUE;
	gtk_ctree_remove_node(ctree, addrbook.selected);
	addrbook.open_folder = FALSE;
}

#define SET_LABEL_AND_ENTRY(str, entry, top) \
{ \
	label = gtk_label_new(str); \
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), \
			 GTK_FILL, 0, 0, 0); \
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5); \
 \
	entry = gtk_entry_new(); \
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, top, (top + 1), \
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0); \
}

static void addressbook_edit_address_create(gboolean *cancelled)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *rem_entry;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	debug_print("Creating edit_address window...\n");

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(window, 400, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Edit address"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);	
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(edit_address_delete_event),
			   cancelled);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(edit_address_key_pressed),
			   cancelled);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	table = gtk_table_new(3, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	SET_LABEL_AND_ENTRY(_("Name"),    name_entry, 0);
	SET_LABEL_AND_ENTRY(_("Address"), addr_entry, 1);
	SET_LABEL_AND_ENTRY(_("Remarks"), rem_entry,  2);

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(edit_address_ok), cancelled);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(edit_address_cancel), cancelled);

	gtk_widget_show_all(vbox);

	addredit.window     = window;
	addredit.name_entry = name_entry;
	addredit.addr_entry = addr_entry;
	addredit.rem_entry  = rem_entry;
	addredit.ok_btn     = ok_btn;
	addredit.cancel_btn = cancel_btn;
}

static void edit_address_ok(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = FALSE;
	gtk_main_quit();
}

static void edit_address_cancel(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint edit_address_delete_event(GtkWidget *widget, GdkEventAny *event,
				      gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();

	return TRUE;
}

static void edit_address_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gboolean *cancelled)
{
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
}

static AddressItem *addressbook_edit_address(AddressItem *item)
{
	static gboolean cancelled;
	const gchar *str;

	if (!addredit.window)
		addressbook_edit_address_create(&cancelled);
	gtk_widget_grab_focus(addredit.ok_btn);
	gtk_widget_grab_focus(addredit.name_entry);
	gtk_widget_show(addredit.window);
	manage_window_set_transient(GTK_WINDOW(addredit.window));

	gtk_entry_set_text(GTK_ENTRY(addredit.name_entry), "");
	gtk_entry_set_text(GTK_ENTRY(addredit.addr_entry), "");
	gtk_entry_set_text(GTK_ENTRY(addredit.rem_entry),  "");

	if (item) {
		if (item->name)
			gtk_entry_set_text(GTK_ENTRY(addredit.name_entry),
					   item->name);
		if (item->address)
			gtk_entry_set_text(GTK_ENTRY(addredit.addr_entry),
					   item->address);
		if (item->remarks)
			gtk_entry_set_text(GTK_ENTRY(addredit.rem_entry),
					   item->remarks);
	}

	gtk_main();
	gtk_widget_hide(addredit.window);
	if (cancelled == TRUE) return NULL;

	str = gtk_entry_get_text(GTK_ENTRY(addredit.name_entry));
	if (*str == '\0') return NULL;

	if (!item) {
		item = mgu_create_address();
		ADDRESS_OBJECT_TYPE(item) = ADDR_ITEM;
	}

	g_free(item->name);
	item->name = g_strdup(str);

	str = gtk_entry_get_text(GTK_ENTRY(addredit.addr_entry));
	g_free(item->address);
	if (*str == '\0')
		item->address = NULL;
	else
		item->address = g_strdup(str);

	str = gtk_entry_get_text(GTK_ENTRY(addredit.rem_entry));
	g_free(item->remarks);
	if (*str == '\0')
		item->remarks = NULL;
	else
		item->remarks = g_strdup(str);

	return item;
}

static void addressbook_new_address_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	AddressItem *item;

	item = addressbook_edit_address(NULL);

	if (item) {
		addressbook_add_object(addrbook.selected,
				       ADDRESS_OBJECT(item));
		if (addrbook.selected == addrbook.opened) {
			addrbook.open_folder = TRUE;
			gtk_ctree_select(GTK_CTREE(addrbook.ctree),
					 addrbook.opened);
		}
	}
}

static void addressbook_edit_address_cb(gpointer data, guint action,
					GtkWidget *widget)
{
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	GtkCTree *ctree;
	AddressObject *obj, *pobj;
	GtkCTreeNode *node = NULL, *parentNode = NULL;
	gchar *nodeName;

	if (!clist->selection) {
		addressbook_edit_folder_cb(NULL, 0, NULL);
		return;
	}

	obj = gtk_clist_get_row_data(clist, GPOINTER_TO_INT(clist->selection->data));
	g_return_if_fail(obj != NULL);

	pobj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), addrbook.selected);

	if (obj->type == ADDR_ITEM) {
		AddressItem *item = ADDRESS_ITEM(obj);

		if( pobj ) {
			// Prevent edit of readonly items
			if(	pobj->type == ADDR_VCARD ||
				pobj->type == ADDR_JPILOT ||
				pobj->type == ADDR_CATEGORY ||
				pobj->type == ADDR_LDAP ) return;
		}

		if (addressbook_edit_address(item) == NULL) return;

		addrbook.open_folder = TRUE;
		gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
		return;
	}
	else if (obj->type == ADDR_GROUP) {
		addressbook_edit_group(NULL);
		return;
	}
	else if( obj->type == ADDR_VCARD ) {
		AddressVCard *vcard = ADDRESS_VCARD(obj);
		if( addressbook_edit_vcard( vcard ) == NULL ) return;
		nodeName = vcard->name;
		parentNode = addrbook.vcard;
	}
#ifdef USE_JPILOT
	else if( obj->type == ADDR_JPILOT ) {
		AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		if( addressbook_edit_jpilot( jpilot ) == NULL ) return;
		nodeName = jpilot->name;
		parentNode = addrbook.jpilot;
	}
#endif
#ifdef USE_LDAP
       	else if( obj->type == ADDR_LDAP ) {
		AddressLDAP *ldapi = ADDRESS_LDAP(obj);
		if( addressbook_edit_ldap( ldapi ) == NULL ) return;
		nodeName = ldapi->name;
		parentNode = addrbook.ldap;
	}
#endif
	else {
		return;
	}

	// Update tree node with node name
       	ctree = GTK_CTREE( addrbook.ctree );
	node = gtk_ctree_find_by_row_data( ctree, addrbook.selected, obj );
	if( ! node ) return;
	addressbook_change_node_name( node, nodeName );
	gtk_ctree_sort_node(ctree, parentNode );
	addrbook.open_folder = TRUE;
	gtk_ctree_select( ctree, addrbook.opened );
}

static void addressbook_delete_address_cb(gpointer data, guint action,
					  GtkWidget *widget)
{
	addressbook_del_clicked(NULL, NULL);
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_close();
}

static void addressbook_set_clist(AddressObject *obj)
{
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	GList *items;
	gchar *text[N_COLS];

	if (!obj) {
		gtk_clist_clear(clist);
		return;
	}

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	if (obj->type == ADDR_GROUP)
		items = ADDRESS_GROUP(obj)->items;
	else if (obj->type == ADDR_FOLDER) {
		items = ADDRESS_FOLDER(obj)->items;
	}
	else if (obj->type == ADDR_VCARD) {
		items = ADDRESS_VCARD(obj)->items;
	}
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		items = ADDRESS_JPILOT(obj)->items;
	}
	else if (obj->type == ADDR_CATEGORY) {
		items = ADDRESS_CATEGORY(obj)->items;
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		items = ADDRESS_LDAP(obj)->items;
	}
#endif
	else {
		gtk_clist_thaw(clist);
		return;
	}

	for (; items != NULL; items = items->next) {
		AddressObject *iobj;
		gint row;
		iobj = ADDRESS_OBJECT(items->data);
		if( iobj == NULL ) continue;

		if (iobj->type == ADDR_GROUP) {
			AddressGroup *group;

			group = ADDRESS_GROUP(iobj);
			text[COL_NAME]    = group->name;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_pixtext(clist, row, COL_NAME,
					      group->name, 4,
					      groupxpm, groupxpmmask);
			gtk_clist_set_row_data(clist, row, iobj);
		} if (iobj->type == ADDR_VCARD) {
			AddressVCard *vcard;

			vcard = ADDRESS_VCARD(iobj);
			text[COL_NAME]    = vcard->name;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_pixtext(clist, row, COL_NAME,
					      vcard->name, 4,
					      vcardxpm, vcardxpmmask);
			gtk_clist_set_row_data(clist, row, iobj);
#ifdef USE_JPILOT
		} if (iobj->type == ADDR_JPILOT) {
			AddressJPilot *jpilot;

			jpilot = ADDRESS_JPILOT(iobj);
			text[COL_NAME]    = jpilot->name;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_pixtext(clist, row, COL_NAME,
					      jpilot->name, 4,
					      jpilotxpm, jpilotxpmmask);
			gtk_clist_set_row_data(clist, row, iobj);
		} if (iobj->type == ADDR_CATEGORY) {
			AddressCategory *category;

			category = ADDRESS_CATEGORY(iobj);
			text[COL_NAME]    = category->name;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_pixtext(clist, row, COL_NAME,
					      category->name, 4,
					      categoryxpm, categoryxpmmask);
			gtk_clist_set_row_data(clist, row, iobj);
#endif
#ifdef USE_LDAP
		} if (iobj->type == ADDR_LDAP) {
			AddressLDAP *ldapi;

			ldapi = ADDRESS_LDAP(iobj);
			text[COL_NAME]    = ldapi->name;
			text[COL_ADDRESS] = NULL;
			text[COL_REMARKS] = NULL;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_pixtext(clist, row, COL_NAME,
					      ldapi->name, 4,
					      ldapxpm, ldapxpmmask);
			gtk_clist_set_row_data(clist, row, iobj);
#endif
		} else if (iobj->type == ADDR_ITEM) {
			AddressItem *item;

			item = ADDRESS_ITEM(iobj);
			text[COL_NAME]    = item->name;
			text[COL_ADDRESS] = item->address;
			text[COL_REMARKS] = item->remarks;
			row = gtk_clist_append(clist, text);
			gtk_clist_set_row_data(clist, row, iobj);
		}
	}

	gtk_clist_sort(clist);
	gtk_clist_thaw(clist);
}

static void addressbook_read_file(void)
{
	XMLFile *file;
	gchar *path;

	debug_print(_("Reading addressbook file..."));

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ADDRESS_BOOK, NULL);
	if ((file = xml_open_file(path)) == NULL) {
		debug_print(_("%s doesn't exist.\n"), path);
		g_free(path);
		addressbook_get_tree(NULL, addrbook.common, ADDRESS_TAG_COMMON);
		addressbook_get_tree(NULL, addrbook.personal, ADDRESS_TAG_PERSONAL);
		addressbook_get_tree(NULL, addrbook.vcard, ADDRESS_TAG_VCARD);
#ifdef USE_JPILOT
		addressbook_get_tree(NULL, addrbook.jpilot, ADDRESS_TAG_JPILOT);
#endif
#ifdef USE_LDAP
		addressbook_get_tree(NULL, addrbook.ldap, ADDRESS_TAG_LDAP);
#endif
		return;
	}
	g_free(path);

	xml_get_dtd(file);

	if (xml_parse_next_tag(file) < 0 ||
	    xml_compare_tag(file, "addressbook") == FALSE) {
		g_warning("Invalid addressbook data\n");
		xml_close_file(file);
		return;
	}

	addressbook_get_tree(file, addrbook.common, ADDRESS_TAG_COMMON);
	addressbook_get_tree(file, addrbook.personal, ADDRESS_TAG_PERSONAL);
	addressbook_get_tree(file, addrbook.vcard, ADDRESS_TAG_VCARD);
#ifdef USE_JPILOT
	addressbook_get_tree(file, addrbook.jpilot, ADDRESS_TAG_JPILOT);
#endif
#ifdef USE_LDAP
	addressbook_get_tree(file, addrbook.ldap, ADDRESS_TAG_LDAP);
#endif

	xml_close_file(file);

	debug_print(_("done.\n"));
}

static void addressbook_get_tree(XMLFile *file, GtkCTreeNode *node,
				 const gchar *folder_tag)
{
	AddressFolder *folder;

	g_return_if_fail(node != NULL);

	folder = g_new(AddressFolder, 1);
	ADDRESS_OBJECT(folder)->type = ADDR_FOLDER;
	folder->name = g_strdup(folder_tag);
	folder->items = NULL;
	gtk_ctree_node_set_row_data(GTK_CTREE(addrbook.ctree), node, folder);

	if (file) {
		if (xml_parse_next_tag(file) < 0 ||
		    xml_compare_tag(file, folder_tag) == FALSE) {
			g_warning("Invalid addressbook data\n");
			return;
		}
	}

	if (file) addressbook_add_objs(file, node);
}

static void addressbook_add_objs(XMLFile *file, GtkCTreeNode *node)
{
	GList *attr;
	guint prev_level;
	GtkCTreeNode *new_node;

	for (;;) {
		prev_level = file->level;
		if (xml_parse_next_tag(file) < 0) return;
		if (file->level < prev_level) return;

		if (xml_compare_tag(file, "group")) {
			AddressGroup *group;

			group = g_new(AddressGroup, 1);
			ADDRESS_OBJECT_TYPE(group) = ADDR_GROUP;
			attr = xml_get_current_tag_attr(file);
			if (attr)
				group->name = g_strdup(((XMLAttr *)attr->data)->value);
			else
				group->name = NULL;
			group->items = NULL;

			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(group));

			addressbook_add_objs(file, new_node);
		} else if (xml_compare_tag(file, "folder")) {
			AddressFolder *folder;

			folder = g_new(AddressFolder, 1);
			ADDRESS_OBJECT_TYPE(folder) = ADDR_FOLDER;
			attr = xml_get_current_tag_attr(file);
			if (attr)
				folder->name = g_strdup(((XMLAttr *)attr->data)->value);
			else
				folder->name = NULL;
			folder->items = NULL;

			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(folder));

			addressbook_add_objs(file, new_node);
		}
		else if( xml_compare_tag( file, "vcard" ) ) {
			AddressVCard *vcard;
			vcard = addressbook_parse_vcard( file );
			if( ! vcard ) return;
			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(vcard));
		}
#ifdef USE_JPILOT
		else if( xml_compare_tag( file, "jpilot" ) ) {
			AddressJPilot *jpilot;
			jpilot = addressbook_parse_jpilot( file );
			if( ! jpilot ) return;
			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(jpilot));
		}
#endif
#ifdef USE_LDAP
		else if( xml_compare_tag( file, "server" ) ) {
			AddressLDAP *ldapi;
			ldapi = addressbook_parse_ldap( file );
			if( ! ldapi ) return;
			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(ldapi));
		}
#endif
		else if (xml_compare_tag(file, "item")) {
			AddressItem *item;

			item = addressbook_parse_item(file);
			if (!item) return;
			new_node = addressbook_add_object
				(node, ADDRESS_OBJECT(item));
		} else {
			g_warning("Invalid tag\n");
			return;
		}
	}
}

static GtkCTreeNode *addressbook_add_object(GtkCTreeNode *node,
					    AddressObject *obj)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *added;
	AddressObject *pobj;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(obj  != NULL, NULL);

	pobj = gtk_ctree_node_get_row_data(ctree, node);
	g_return_val_if_fail(pobj != NULL, NULL);

	if (pobj->type == ADDR_ITEM) {
		g_warning("Parent object mustn't be an item.\n");
		return NULL;
	}
	if (pobj->type == ADDR_FOLDER &&
	    (obj->type == ADDR_GROUP || obj->type == ADDR_FOLDER))
		gtk_ctree_expand(ctree, node);

	if (pobj->type == ADDR_FOLDER && obj->type == ADDR_VCARD )
		gtk_ctree_expand(ctree, node);

	if (pobj->type == ADDR_FOLDER && obj->type == ADDR_JPILOT )
		gtk_ctree_expand(ctree, node);

	if (pobj->type == ADDR_FOLDER && obj->type == ADDR_LDAP )
		gtk_ctree_expand(ctree, node);

	if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);

		if (pobj->type != ADDR_FOLDER) {
			g_warning("Group can't be added in another group.\n");
			return NULL;
		}

		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &group->name, FOLDER_SPACING,
					      groupxpm, groupxpmmask,
					      groupxpm, groupxpmmask,
					      TRUE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);
	} else if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);

		if (pobj->type != ADDR_FOLDER) {
			g_warning("Group can't contain folder.\n");
			return NULL;
		}

		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &folder->name, FOLDER_SPACING,
					      folderxpm, folderxpmmask,
					      folderopenxpm, folderopenxpmmask,
					      FALSE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);

	}
	else if (obj->type == ADDR_VCARD) {
		AddressVCard *vcard = ADDRESS_VCARD(obj);
		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &vcard->name, FOLDER_SPACING,
					      vcardxpm, vcardxpmmask,
					      vcardxpm, vcardxpmmask,
					      TRUE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);
	}
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &jpilot->name, FOLDER_SPACING,
					      jpilotxpm, jpilotxpmmask,
					      jpilotxpm, jpilotxpmmask,
					      FALSE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);
	}
	else if (obj->type == ADDR_CATEGORY) {
		AddressCategory *category = ADDRESS_CATEGORY(obj);
		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &category->name, FOLDER_SPACING,
					      categoryxpm, categoryxpmmask,
					      categoryxpm, categoryxpmmask,
					      TRUE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		AddressLDAP *server = ADDRESS_LDAP(obj);
		added = gtk_ctree_insert_node(ctree, node, NULL,
					      &server->name, FOLDER_SPACING,
					      ldapxpm, ldapxpmmask,
					      ldapxpm, ldapxpmmask,
					      TRUE, FALSE);
		gtk_ctree_node_set_row_data(ctree, added, obj);
	}
#endif
       	else {
		added = node;
	}

	if (obj->type == ADDR_GROUP || obj->type == ADDR_ITEM) {
		if (pobj->type == ADDR_GROUP) {
			AddressGroup *group = ADDRESS_GROUP(pobj);

			group->items = g_list_append(group->items, obj);
		} else if (pobj->type == ADDR_FOLDER) {
			AddressFolder *folder = ADDRESS_FOLDER(pobj);

			folder->items = g_list_append(folder->items, obj);
		}
	}

	if (pobj->type == ADDR_FOLDER) {
		if (obj->type == ADDR_VCARD || obj->type == ADDR_JPILOT || obj->type == ADDR_LDAP) {
			AddressFolder *folder = ADDRESS_FOLDER(pobj);
			folder->items = g_list_append(folder->items, obj);
		}
	}

	gtk_ctree_sort_node(ctree, node);

	return added;
}

static void addressbook_delete_object(AddressObject *obj)
{
	if (!obj) return;

	if (obj->type == ADDR_ITEM) {
		AddressItem *item = ADDRESS_ITEM(obj);

		mgu_free_address( item );
	} else if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);

		g_free(group->name);
		while (group->items != NULL) {
			addressbook_delete_object
				(ADDRESS_OBJECT(group->items->data));
			group->items = g_list_remove(group->items,
						     group->items->data);
		}
		g_free(group);
	} else if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);

		g_free(folder->name);
		while (folder->items != NULL) {
			addressbook_delete_object
				(ADDRESS_OBJECT(folder->items->data));
			folder->items = g_list_remove(folder->items,
						      folder->items->data);
		}
		g_free(folder);
	}
	else if( obj->type == ADDR_VCARD ) {
		AddressVCard *vcard = ADDRESS_VCARD(obj);
		g_free( vcard->name );
		vcard_free( vcard->cardFile );
		vcard->cardFile = NULL;
		vcard->items = NULL;
		g_free( vcard );
	}
#ifdef USE_JPILOT
	else if( obj->type == ADDR_JPILOT ) {
		AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		g_free( jpilot->name );
		jpilot_free( jpilot->pilotFile );
		jpilot->pilotFile = NULL;
		jpilot->items = NULL;
		g_free( jpilot );
	}
#endif
#ifdef USE_LDAP
	else if( obj->type == ADDR_LDAP ) {
		AddressLDAP *ldapi = ADDRESS_LDAP(obj);
		g_free( ldapi->name );
		syldap_free( ldapi->ldapServer );
		ldapi->ldapServer = NULL;
		ldapi->items = NULL;
		g_free( ldapi );
	}
#endif
}

static AddressObject *addressbook_find_object_by_name(GtkCTreeNode *node,
						      const gchar *name)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	GList *found;

	g_return_val_if_fail(node != NULL, NULL);

	obj = gtk_ctree_node_get_row_data(ctree, node);
	g_return_val_if_fail(obj != NULL, NULL);

	if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);

		found = g_list_find_custom(group->items, (gpointer)name,
					   addressbook_obj_name_compare);
		if (found) return ADDRESS_OBJECT(found->data);
	} else if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);

		found = g_list_find_custom(folder->items, (gpointer)name,
					   addressbook_obj_name_compare);
		if (found) return ADDRESS_OBJECT(found->data);
	} else if (obj->type == ADDR_ITEM) {
		if (!addressbook_obj_name_compare(obj, name)) return obj;
	}

	return NULL;
}

static AddressItem *addressbook_parse_item(XMLFile *file)
{
	gchar *element;
	AddressItem *item;
	guint level;

	item = mgu_create_address();
	ADDRESS_OBJECT(item)->type = ADDR_ITEM;

	level = file->level;

	while (xml_parse_next_tag(file) == 0) {
		if (file->level < level) return item;
		if (file->level == level) break;

		element = xml_get_element(file);

		if (xml_compare_tag(file, "name")) {
			item->name = element;
		} else if (xml_compare_tag(file, "address")) {
			item->address = element;
		} else if (xml_compare_tag(file, "remarks")) {
			item->remarks = element;
		}

		if (xml_parse_next_tag(file) < 0) break;
		if (file->level != level) break;
	}

	g_warning("addressbook_parse_item(): Parse error\n");
	mgu_free_address( item );
}

void addressbook_export_to_file(void)
{
	PrefFile *pfile;
	gchar *path;

	if (!addrbook.ctree) return;

	debug_print(_("Exporting addressbook to file..."));

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ADDRESS_BOOK, NULL);
	if ((pfile = prefs_write_open(path)) == NULL) {
		g_free(path);
		return;
	}
	g_free(path);

	fprintf(pfile->fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n",
		conv_get_current_charset_str());
	fputs("<addressbook>\n\n", pfile->fp);

	addressbook_xml_recursive_write(NULL, pfile->fp);

	fputs("</addressbook>\n", pfile->fp);

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write addressbook data.\n"));
		return;
	}

	debug_print(_("done.\n"));
}

/* Most part of this function was taken from gtk_ctree_pre_recursive() and
   gtk_ctree_post_recursive(). */
static void addressbook_xml_recursive_write(GtkCTreeNode *node, FILE *fp)
{
	GtkCTreeNode *work;
	GtkCTreeNode *tmp;

	if (node) {
		work = GTK_CTREE_ROW(node)->children;
		addressbook_node_write_begin(node, fp);
	} else
		work = GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list);

	while (work) {
		tmp = GTK_CTREE_ROW(work)->sibling;
		addressbook_xml_recursive_write(work, fp);
		work = tmp;
	}

	if (node)
		addressbook_node_write_end(node, fp);
}

static void addressbook_node_write_begin(GtkCTreeNode *node, FILE *fp)
{
	AddressObject *obj;

	obj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), node);
	g_return_if_fail(obj != NULL);

	if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);

		if (GTK_CTREE_ROW(node)->level == 1) {
			fprintf(fp, "<%s>\n", folder->name);
		} else {
			tab_indent_out(fp, GTK_CTREE_ROW(node)->level - 1);
			fputs("<folder name=\"", fp);
			xml_file_put_escape_str(fp, folder->name);
			fputs("\">\n", fp);
		}
	} else if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);

		tab_indent_out(fp, GTK_CTREE_ROW(node)->level - 1);
		fputs("<group name=\"", fp);
		xml_file_put_escape_str(fp, group->name);
		fputs("\">\n", fp);
	}
}

static void addressbook_node_write_end(GtkCTreeNode *node, FILE *fp)
{
	AddressObject *obj;

	obj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), node);
	g_return_if_fail(obj != NULL);

	if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);

		addressbook_write_items(fp, folder->items,
					GTK_CTREE_ROW(node)->level);

		if (GTK_CTREE_ROW(node)->level == 1) {
			fprintf(fp, "</%s>\n\n", folder->name);
		} else {
			tab_indent_out(fp, GTK_CTREE_ROW(node)->level - 1);
			fputs("</folder>\n", fp);
		}
	} else if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);

		addressbook_write_items(fp, group->items,
					GTK_CTREE_ROW(node)->level);

		tab_indent_out(fp, GTK_CTREE_ROW(node)->level - 1);
		fputs("</group>\n", fp);
	}
	else if (obj->type == ADDR_VCARD) {
		AddressVCard *vcard = ADDRESS_VCARD(obj);
		addressbook_write_vcard( fp, vcard, GTK_CTREE_ROW(node)->level);
	}
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		addressbook_write_jpilot( fp, jpilot, GTK_CTREE_ROW(node)->level);
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		AddressLDAP *ldap = ADDRESS_LDAP(obj);
		addressbook_write_ldap( fp, ldap, GTK_CTREE_ROW(node)->level);
	}
#endif
}

static void addressbook_write_items(FILE *fp, GList *items, guint level)
{
	AddressItem *item;

	for (; items != NULL; items = items->next) {
		if (ADDRESS_OBJECT_TYPE(items->data) == ADDR_ITEM) {
			item = ADDRESS_ITEM(items->data);

			tab_indent_out(fp, level);
			fputs("<item>\n", fp);

			tab_indent_out(fp, level + 1);
			fputs("<name>", fp);
			xml_file_put_escape_str(fp, item->name);
			fputs("</name>\n", fp);

			tab_indent_out(fp, level + 1);
			fputs("<address>", fp);
			xml_file_put_escape_str(fp, item->address);
			fputs("</address>\n", fp);

			tab_indent_out(fp, level + 1);
			fputs("<remarks>", fp);
			xml_file_put_escape_str(fp, item->remarks);
			fputs("</remarks>\n", fp);

			tab_indent_out(fp, level);
			fputs("</item>\n", fp);
		}
	}
}

static void tab_indent_out(FILE *fp, guint level)
{
	gint i;

	for (i = 0; i < level; i++)
		fputs("    ", fp);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		addressbook_close();
}

static gint addressbook_list_compare_func(GtkCList *clist,
					  gconstpointer ptr1,
					  gconstpointer ptr2)
{
	AddressObject *obj1 = ((GtkCListRow *)ptr1)->data;
	AddressObject *obj2 = ((GtkCListRow *)ptr2)->data;
	gchar *name1, *name2;

	if (obj1) {
		if (obj1->type == ADDR_ITEM)
			name1 = ADDRESS_ITEM(obj1)->name;
		else if (obj1->type == ADDR_GROUP)
			name1 = ADDRESS_GROUP(obj1)->name;
		else if (obj1->type == ADDR_FOLDER)
			name1 = ADDRESS_FOLDER(obj1)->name;
		else if (obj1->type == ADDR_VCARD)
			name1 = ADDRESS_VCARD(obj1)->name;
#ifdef USE_JPILOT
		else if (obj1->type == ADDR_JPILOT)
			name1 = ADDRESS_JPILOT(obj1)->name;
		else if (obj1->type == ADDR_CATEGORY)
			name1 = ADDRESS_CATEGORY(obj1)->name;
#endif
#ifdef USE_LDAP
		else if (obj1->type == ADDR_LDAP)
			name1 = ADDRESS_LDAP(obj1)->name;
#endif
		else
			name1 = NULL;
	} else
		name1 = NULL;

	if (obj2) {
		if (obj2->type == ADDR_ITEM)
			name2 = ADDRESS_ITEM(obj2)->name;
		else if (obj2->type == ADDR_GROUP)
			name2 = ADDRESS_GROUP(obj2)->name;
		else if (obj2->type == ADDR_FOLDER)
			name2 = ADDRESS_FOLDER(obj2)->name;
		else if (obj2->type == ADDR_VCARD)
			name2 = ADDRESS_VCARD(obj2)->name;
#ifdef USE_JPILOT
		else if (obj2->type == ADDR_JPILOT)
			name2 = ADDRESS_JPILOT(obj2)->name;
		else if (obj2->type == ADDR_CATEGORY)
			name2 = ADDRESS_CATEGORY(obj2)->name;
#endif
#ifdef USE_LDAP
		else if (obj2->type == ADDR_LDAP)
			name2 = ADDRESS_LDAP(obj2)->name;
#endif
		else
			name2 = NULL;
	} else
		name2 = NULL;

	if (!name1)
		return (name2 != NULL);
	if (!name2)
		return -1;

	return strcasecmp(name1, name2);
}

static gint addressbook_obj_name_compare(gconstpointer a, gconstpointer b)
{
	const AddressObject *obj = a;
	const gchar *name = b;

	if (!obj || !name) return -1;

	if (obj->type == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(obj);
		if (!group->name)
			return -1;
		else
			return strcasecmp(group->name, name);
	} else if (obj->type == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(obj);
		if (!folder->name)
			return -1;
		else
			return strcasecmp(folder->name, name);
	}
	else if (obj->type == ADDR_VCARD) {
		AddressVCard *vcard = ADDRESS_VCARD(obj);
		if (!vcard->name)
			return -1;
		else
			return strcasecmp(vcard->name, name);
	}
#ifdef USE_JPILOT
	else if (obj->type == ADDR_JPILOT) {
		AddressJPilot *jpilot = ADDRESS_JPILOT(obj);
		if (!jpilot->name)
			return -1;
		else
			return strcasecmp(jpilot->name, name);
	}
	else if (obj->type == ADDR_CATEGORY) {
		AddressCategory *category = ADDRESS_CATEGORY(obj);
		if (!category->name)
			return -1;
		else
			return strcasecmp(category->name, name);
	}
#endif
#ifdef USE_LDAP
	else if (obj->type == ADDR_LDAP) {
		AddressLDAP *server = ADDRESS_LDAP(obj);
		if (!server->name)
			return -1;
		else
			return strcasecmp(server->name, name);
	}
#endif
	else if (obj->type == ADDR_ITEM) {
		AddressItem *item = ADDRESS_ITEM(obj);
		if (!item->name)
			return -1;
		else
			return strcasecmp(item->name, name);
	} else
		return -1;
}

static AddressVCard *addressbook_parse_vcard(XMLFile *file) {
	AddressVCard *item = NULL;
	VCardFile *vcf;
	GList *attr;
	gchar *name, *value;

	vcf = vcard_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, "name" ) == 0 ) {
			vcard_set_name( vcf, value );
		}
		else if( strcmp( name, "file" ) == 0) {
			vcard_set_file( vcf, value );
		}
		attr = g_list_next( attr );
	}

	// Move to next tag
	if( xml_parse_next_tag( file ) >= 0 ) {
		if( vcard_validate( vcf ) ) {
			item = g_new( AddressVCard, 1 );
			ADDRESS_OBJECT(item)->type = ADDR_VCARD;
			item->name = g_strdup( vcf->name );
			item->cardFile = vcf;
			item->items = NULL;
			return item;
		}
	}

	// Must be an invalid tag or data.
	g_warning( "addressbook_parse_vcard(): Parse error\n");
	vcard_free( vcf );
	vcf = NULL;
	item = NULL;
	return NULL;
}

static void addressbook_write_vcard( FILE *fp, AddressVCard *vcard, guint level ) {
	VCardFile *cardFile = vcard->cardFile;
	if( cardFile ) {
		tab_indent_out(fp, 1);
		fputs("<vcard ", fp);
		fputs("name=\"", fp);
		xml_file_put_escape_str(fp, cardFile->name);
		fputs("\"", fp);
		fputs(" file=\"", fp);
		xml_file_put_escape_str(fp, cardFile->path);
		fputs("\"", fp);
		fputs(" />\n", fp);
	}
}

#ifdef USE_JPILOT
static AddressJPilot *addressbook_parse_jpilot(XMLFile *file) {
	AddressJPilot *item = NULL;
	JPilotFile *jpf;
	GList *attr;
	gchar *name, *value;

	jpf = jpilot_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		if( strcmp( name, "name" ) == 0 ) {
			jpilot_set_name( jpf, value );
		}
		else if( strcmp( name, "file" ) == 0 ) {
			jpilot_set_file( jpf, value );
		}
		else if( strcmp( name, "custom-1" ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, "custom-2" ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, "custom-3" ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		else if( strcmp( name, "custom-4" ) == 0 ) {
			jpilot_add_custom_label( jpf, value );
		}
		attr = g_list_next( attr );
	}

	// Move to next tag
	if( xml_parse_next_tag( file ) >= 0 ) {
		if( jpilot_validate( jpf ) ) {
			item = g_new( AddressJPilot, 1 );
			ADDRESS_OBJECT(item)->type = ADDR_JPILOT;
			item->name = g_strdup( jpf->name );
			item->pilotFile = jpf;
			item->items = NULL;
			return item;
		}
	}

	// Must be an invalid tag or data.
	g_warning( "addressbook_parse_jpilot(): Parse error\n");
	jpilot_free( jpf );
	jpf = NULL;
	item = NULL;
	return NULL;
}

static void addressbook_write_jpilot( FILE *fp, AddressJPilot *jpilot, guint level ) {
	JPilotFile *pilotFile = jpilot->pilotFile;
	if( pilotFile ) {
		gint ind;
		GList *node;
		GList *customLbl = jpilot_get_custom_labels( pilotFile );
		tab_indent_out(fp, 1);
		fputs("<jpilot ", fp);
		fputs("name=\"", fp);
		xml_file_put_escape_str(fp, pilotFile->name);
		fputs("\" file=\"", fp);
		xml_file_put_escape_str(fp, pilotFile->path);

		fputs( "\" ", fp );
		node = customLbl;
		ind = 1;
		while( node ) {
			fprintf( fp, "custom-%d=\"", ind );
			xml_file_put_escape_str( fp, node->data );
			fputs( "\" ", fp );
			ind++;
			node = g_list_next( node );
		}
		fputs("/>\n", fp);
	}
}
#endif

static void addressbook_new_vcard_cb( gpointer data, guint action, GtkWidget *widget ) {
	AddressVCard *vcard;

	if( addrbook.selected != addrbook.vcard ) return;
	vcard = addressbook_edit_vcard( NULL );
	if( vcard ) {
		addressbook_add_object( addrbook.selected, ADDRESS_OBJECT(vcard) );
		if( addrbook.selected == addrbook.opened ) {
			addrbook.open_folder = TRUE;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

static void addressbook_vcard_show_message( VCardFile *vcf ) {
	*addressbook_msgbuf = '\0';
	if( vcf ) {
		if( vcf->retVal == MGU_SUCCESS ) {
			sprintf( addressbook_msgbuf, "%s", vcf->name );
		}
		else {
			sprintf( addressbook_msgbuf, "%s: %s", vcf->name, mgu_error2string( vcf->retVal ) );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb( gpointer data, guint action, GtkWidget *widget ) {
	AddressJPilot *jpilot;

	if( addrbook.selected != addrbook.jpilot ) return;
	if( ! _have_pilot_library_ ) return;
	jpilot = addressbook_edit_jpilot( NULL );
	if( jpilot ) {
		addressbook_add_object( addrbook.selected, ADDRESS_OBJECT(jpilot) );
		if( addrbook.selected == addrbook.opened ) {
			addrbook.open_folder = TRUE;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

static void addressbook_jpilot_show_message( JPilotFile *jpf ) {
	*addressbook_msgbuf = '\0';
	if( jpf ) {
		if( jpf->retVal == MGU_SUCCESS ) {
			sprintf( addressbook_msgbuf, "%s", jpf->name );
		}
		else {
			sprintf( addressbook_msgbuf, "%s: %s", jpf->name, mgu_error2string( jpf->retVal ) );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}

#endif

#ifdef USE_LDAP
static void addressbook_new_ldap_cb( gpointer data, guint action, GtkWidget *widget ) {
	AddressLDAP *ldapi;

	if( addrbook.selected != addrbook.ldap ) return;
	if( ! _have_ldap_library_ ) return;
	ldapi = addressbook_edit_ldap( NULL );
	if( ldapi ) {
		addressbook_add_object( addrbook.selected, ADDRESS_OBJECT(ldapi) );
		if( addrbook.selected == addrbook.opened ) {
			addrbook.open_folder = TRUE;
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

static AddressLDAP *addressbook_parse_ldap(XMLFile *file) {
	AddressLDAP *item = NULL;
	SyldapServer *server;
	GList *attr;
	gchar *name, *value;
	gint ivalue;

	server = syldap_create();
	attr = xml_get_current_tag_attr( file );
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		ivalue = atoi( value );
		if( strcmp( name, "name" ) == 0 ) {
			syldap_set_name( server, value );
		}
		else if( strcmp( name, "host" ) == 0 ) {
			syldap_set_host( server, value );
		}
		else if( strcmp( name, "port" ) == 0 ) {
			syldap_set_port( server, ivalue );
		}
		else if( strcmp( name, "base-dn" ) == 0 ) {
			syldap_set_base_dn( server, value );
		}
		else if( strcmp( name, "bind-dn" ) == 0 ) {
			syldap_set_bind_dn( server, value );
		}
		else if( strcmp( name, "bind-pass" ) == 0 ) {
			syldap_set_bind_password( server, value );
		}
		else if( strcmp( name, "criteria" ) == 0 ) {
			syldap_set_search_criteria( server, value );
		}
		else if( strcmp( name, "max-entry" ) == 0 ) {
			syldap_set_max_entries( server, ivalue );
		}
		else if( strcmp( name, "timeout" ) == 0 ) {
			syldap_set_timeout( server, ivalue );
		}
		attr = g_list_next( attr );
	}

	// Move to next tag
	if( xml_parse_next_tag( file ) >= 0 ) {
		item = g_new( AddressLDAP, 1 );
		ADDRESS_OBJECT(item)->type = ADDR_LDAP;
		item->name = g_strdup( server->name );
		item->ldapServer = server;
		item->items = NULL;
		return item;
	}

	// Must be an invalid tag or data.
	g_warning( "addressbook_parse_ldap(): Parse error\n");
	syldap_free( server );
	server = NULL;
	item = NULL;
	return NULL;
}

static void addressbook_write_ldap( FILE *fp, AddressLDAP *ldapi, guint level ) {
	SyldapServer *server = ldapi->ldapServer;
	if( server ) {
		tab_indent_out(fp, 1);
		fputs("<server ", fp);
		fputs("name=\"", fp);
		xml_file_put_escape_str(fp, server->name);
		fputs("\" host=\"", fp);
		xml_file_put_escape_str(fp, server->hostName);
		fprintf( fp, "\" port=\"%d", server->port);
		fputs("\" base-dn=\"", fp);
		xml_file_put_escape_str(fp, server->baseDN);
		fputs("\" bind-dn=\"", fp);
		xml_file_put_escape_str(fp, server->bindDN);
		fputs("\" bind-pass=\"", fp);
		xml_file_put_escape_str(fp, server->bindPass);
		fputs("\" criteria=\"", fp);
		xml_file_put_escape_str(fp, server->searchCriteria);
		fprintf( fp, "\" max-entry=\"%d", server->maxEntries);
		fprintf( fp, "\" timeout=\"%d", server->timeOut);
		fputs("\" />\n", fp);
	}
}

static void addressbook_ldap_show_message( SyldapServer *svr ) {
	*addressbook_msgbuf = '\0';
	if( svr ) {
		if( svr->busyFlag ) {
			sprintf( addressbook_msgbuf, "%s: %s", svr->name, ADDRESSBOOK_LDAP_BUSYMSG );
		}
		else {
			if( svr->retVal == MGU_SUCCESS ) {
				sprintf( addressbook_msgbuf, "%s", svr->name );
			}
			else {
				sprintf( addressbook_msgbuf, "%s: %s", svr->name, mgu_error2string( svr->retVal ) );
			}
		}
	}
	addressbook_status_show( addressbook_msgbuf );
}

static gint ldapsearch_callback( SyldapServer *sls ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;

	if( sls == NULL ) return;
	if( ! addrbook.selected ) return;
	if( GTK_CTREE_ROW( addrbook.selected )->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, addrbook.selected );
	if( obj == NULL ) return;
	if( obj->type == ADDR_LDAP ) {
                AddressLDAP *ldapi = ADDRESS_LDAP(obj);
		SyldapServer *server = ldapi->ldapServer;
		if( server == sls ) {
			if( ! _have_ldap_library_ ) return;
			// Read from cache
			gtk_widget_show_all(addrbook.window);
			ADDRESS_LDAP(obj)->items = syldap_get_address_list( sls );
			addressbook_set_clist( obj );
			addressbook_ldap_show_message( sls );
			gtk_widget_show_all(addrbook.window);
		}
	}
}
#endif

/*
 * Lookup button handler.
 */
static void addressbook_lup_clicked( GtkButton *button, gpointer data ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	gchar *sLookup;
#ifdef USE_LDAP
	AddressLDAP *ldapi;
	SyldapServer *server;
#endif

	sLookup = gtk_editable_get_chars( GTK_EDITABLE(addrbook.entry), 0, -1 );
	g_strchomp( sLookup );

	if( ! addrbook.selected ) return;
	if( GTK_CTREE_ROW( addrbook.selected )->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, addrbook.selected );
	if( obj == NULL ) return;

#ifdef USE_LDAP
	if( obj->type == ADDR_LDAP ) {
		ldapi = ADDRESS_LDAP(obj);
		server = ldapi->ldapServer;
		if( server ) {
  			if( ! _have_ldap_library_ ) return;
			syldap_cancel_read( server );
			if( *sLookup == '\0' || strlen( sLookup ) < 1 ) return;
			syldap_set_search_value( server, sLookup );
			syldap_set_callback( server, ldapsearch_callback );
			syldap_read_data_th( server );
			addressbook_ldap_show_message( server );
		}
	}
#endif

}

/***/

typedef struct {
	gboolean		init;			/* if FALSE should init jump buffer */
	GtkCTreeNode   *node_found;		/* match (can be used to backtrack folders)  */
	AddressObject  *addr_found;		/* match */
	int				level;			/* current recursion level (0 is root level) */
	jmp_buf			jumper;			/* jump buffer */
} FindObject;

typedef struct {
	FindObject		ancestor;
	const gchar	   *groupname;
} FindGroup;

typedef struct {
	FindObject		ancestor;
	const gchar    *name;
	const gchar    *address;
} FindAddress;

typedef struct {
	FindObject		ancestor;
	GList		   *grouplist;
} FindAllGroups;

typedef gboolean (*ADDRESSBOOK_TRAVERSE_FUNC)(AddressObject *node, gpointer data);

/***/

static gboolean traverse_find_group_by_name(AddressObject *ao, FindGroup *find)
{
	AddressFolder *folder;
	AddressGroup  *group;

	/* a group or folder: both are groups */
	if (ADDRESS_OBJECT_TYPE(ao) == ADDR_GROUP) {
		group = ADDRESS_GROUP(ao);
		if (0 == g_strcasecmp(group->name, find->groupname)) {
			return TRUE;
		}
	}
	else if (ADDRESS_OBJECT_TYPE(ao) == ADDR_FOLDER) {
		folder = ADDRESS_FOLDER(ao);
		if (0 == g_strcasecmp(folder->name, find->groupname)) {
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean traverse_find_name_email(AddressObject *ao, FindAddress *find)
{
	AddressItem *item;
	if (ADDRESS_OBJECT_TYPE(ao) == ADDR_ITEM) {
		gboolean nmatch = FALSE, amatch = FALSE;
		item = ADDRESS_ITEM(ao);
		/* conditions:
		 * o only match at the first characters in item strings 
		 * o match either name or address */
		if (find->name && item->name) {
			nmatch = item->name == strcasestr(item->name, find->name);
		}
		if (find->address && item->address) {
			amatch = item->address == strcasestr(item->address, find->address);
		}
		return nmatch || amatch;
	}
	return FALSE;
}

static gboolean traverse_find_all_groups(AddressObject *ao, FindAllGroups *find)
{
	/* NOTE: added strings come from the address book. should perhaps 
	 * strdup() them, especially if the address book is invalidated */
	if (ADDRESS_OBJECT_TYPE(ao) == ADDR_FOLDER) {
		AddressFolder *folder = ADDRESS_FOLDER(ao);
		find->grouplist = g_list_insert_sorted(find->grouplist, (gpointer) folder->name, (GCompareFunc) g_strcasecmp);
	}
	else if (ADDRESS_OBJECT_TYPE(ao) == ADDR_GROUP) {
		AddressGroup *group = ADDRESS_GROUP(ao);
		find->grouplist = g_list_insert_sorted(find->grouplist, (gpointer) group->name, (GCompareFunc) g_strcasecmp);
	}
	return FALSE;
}

/* addressbook_traverse() - traverses all address objects stored in the address book. 
 * for some reason gtkctree's recursive tree functions don't allow a premature return, 
 * which is what we need if we need to enumerate the tree and check for a condition 
 * and then skipping other nodes. */ 
static AddressObject *addressbook_traverse(GtkCTreeNode *node, ADDRESSBOOK_TRAVERSE_FUNC func, FindObject *data, int level)
{
	GtkCTreeNode  *current, *tmp;
	AddressObject *ao;

	if (data->init == FALSE) {
		/* initialize non-local exit */
		data->init  = TRUE;
		data->level = 0;
		/* HANDLE NON-LOCAL EXIT */
		if (setjmp(data->jumper)) {
			return data->addr_found;
		}
	}

	/* actual recursive code */
	if (!node) {
		current = GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list);
	}
	else {
		current = node;
	}

	while (current) {
		tmp = GTK_CTREE_ROW(current)->sibling;
		ao = (AddressObject *) gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), current);
		if (ao) {
			GList *next;

			next = (ADDRESS_OBJECT_TYPE(ao) == ADDR_FOLDER) ? 
				   g_list_first(((ADDRESS_FOLDER(ao))->items)) :
				   (ADDRESS_OBJECT_TYPE(ao)  == ADDR_GROUP) ?
				   g_list_first(((ADDRESS_GROUP(ao))->items))  : NULL;

			while (ao) {
				/* NOTE: first iteration of the root calls callback for the tree 
				 * node, other iterations call callback for the address book items */
				if (func(ao, data)) {
					/* unwind */
					data->node_found = current;
					data->addr_found = ao;
					longjmp(data->jumper, 1);
				}
				/* ctree node only stores folders and groups. now descend into
				 * address object data, searching for address items. */
				for ( ; next && ADDRESS_OBJECT_TYPE((next->data)) != ADDR_ITEM
					  ; next = g_list_next(next))
					;			
				ao   = next ? (AddressObject *) next->data : NULL;
				next = next ? g_list_next(next) : NULL;
			}				
		}
		/* check the children (if level permits) */
		if (level == -1 || data->level < level) {
			current = GTK_CTREE_ROW(current)->children;
			if (current) {
				data->level++;
				addressbook_traverse(current, func, data, level);
				data->level--;
			}			
		}			
		/* check the siblings */
		current = tmp;
	}
	return NULL;
}

static GtkCTreeNode *addressbook_get_group_node(const gchar *name)
{
	FindGroup fg = { { FALSE, NULL, NULL }, NULL };
	fg.groupname = name;
	addressbook_traverse(NULL, (void *)traverse_find_group_by_name, (FindObject *)&fg, -1);
	return fg.ancestor.node_found;
}

static void addressbook_free_item(AddressItem *item)
{
	if (item) {
		if (item->name) g_free(item->name);
		if (item->address) g_free(item->address);
		if (item->remarks) g_free(item->remarks);
		g_free(item);
	}
}

static AddressItem *addressbook_alloc_item(const gchar *name, const gchar *address, const gchar *remarks)
{
	AddressItem *item = g_new0(AddressItem, 1);
	
	if (item) {
		item->obj.type = ADDR_ITEM;
		if (item->name = g_strdup(name))
			if (item->address = g_strdup(address)) {
				if (remarks) {
					item->remarks = g_strdup(remarks);
				}
				return item;
			}
	}
	addressbook_free_item(item);
	return NULL;
}

/***/

/* public provisional API */

/* addressbook_access() - should be called before using any of the following apis. it
 * reloads the address book. */
void addressbook_access(void)
{
	log_message("accessing address book\n");
	if (!addrbook.window) {
		addressbook_create(FALSE);
		addressbook_read_file();
		addrbook.open_folder = TRUE;
		gtk_ctree_select(GTK_CTREE(addrbook.ctree), GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list));
	} 
}

/* addressbook_unaccess() - should only be called after changing the address book's
 * contents */
void addressbook_unaccess(void)
{
	log_message("unaccessing address book\n");
	addressbook_export_to_file();
	invalidate_address_completion();
}

const gchar *addressbook_get_personal_folder_name(void)
{
	return _("Personal addresses"); /* human readable */
}

const gchar *addressbook_get_common_folder_name(void)
{
	return _("Common addresses"); /* human readable */
}

/* addressbook_find_group_by_name() - finds a group (folder or group) by
 * its name */
AddressObject *addressbook_find_group_by_name(const gchar *name)
{
	FindGroup	   fg = { { FALSE, NULL, NULL } };
	AddressObject *ao;

	/* initialize obj members */
	fg.groupname = name;
	ao = addressbook_traverse(NULL, 
							  (ADDRESSBOOK_TRAVERSE_FUNC)traverse_find_group_by_name, 
							  (FindObject *)&fg, -1);
	return ao;
}

/* addressbook_find_contact() - finds an address item by either name or address
 * or both. the comparison is done on the first few characters of the strings */
AddressObject *addressbook_find_contact(const gchar *name, const gchar *address)
{
	FindAddress   fa = { { FALSE, NULL, NULL } };
	AddressObject *ao;

	fa.name = name;
	fa.address = address;
	ao = addressbook_traverse(NULL, (ADDRESSBOOK_TRAVERSE_FUNC)traverse_find_name_email,
							  (FindObject *)&fa, -1);
	return ao;							  
}

/* addressbook_get_group_list() - returns a list of strings with group names (both
 * groups and folders). free the list using g_list_free(). note that another
 * call may invalidate the returned list */
GList *addressbook_get_group_list(void)
{
	FindAllGroups fag = { { FALSE, NULL, NULL }, NULL };
	addressbook_traverse(NULL, (ADDRESSBOOK_TRAVERSE_FUNC)traverse_find_all_groups,
						 (FindObject *)&fag, -1);
	return fag.grouplist;
}

/* addressbook_add_contact() - adds a contact to the address book. returns 1
 * if succesful else error */
gint addressbook_add_contact(const gchar *group, const gchar *name, const gchar *address,
							 const gchar *remarks) 
{
	GtkCTreeNode *node;
	AddressItem *item;
	FindAddress  fa = { { FALSE, NULL, NULL } };

	/* a healthy mix of hiro's and my code */
	if (name == NULL || strlen(name) == 0
	||  address == NULL || strlen(address) == 0
	||  group == NULL || strlen(group) == 0) {
		return __LINE__;
	}
	node = addressbook_get_group_node(group);
	if (!node) {
		return __LINE__;
	}

	/* check if it's already in this group */
	fa.name = name;
	fa.address = address;

	if (addressbook_traverse(node, (gpointer)traverse_find_name_email, (gpointer)&fa, 0)) {
		log_message("address <%s> already in %s\n", address, group);
		return __LINE__;
	}

	item = addressbook_alloc_item(name, address, remarks);
	if (!item) {
		return __LINE__;
	}

	if (!addressbook_add_object(node, (AddressObject *)item)) {
		addressbook_free_item(item);
		return __LINE__;
	}

	/* make sure it's updated if selected */
	log_message("updating addressbook widgets\n");
	addrbook.open_folder = TRUE;
	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);

	/* not saved yet. only after unaccessing the address book */
	return 0;
}

static void group_object_data_destroy(gchar *group)
{
	if (group) {
		g_free(group);
	}		
}

/***/

typedef struct {
	gchar *name;
	gchar *address;
	gchar *remarks;
} ContactInfo;

static void addressbook_destroy_contact(ContactInfo *ci)
{
	g_return_if_fail(ci != NULL);
	if (ci->name) g_free(ci->name);
	if (ci->address) g_free(ci->address);
	if (ci->remarks) g_free(ci->remarks);
	g_free(ci);
}

static ContactInfo *addressbook_new_contact(const gchar *name, const gchar *address, const gchar *remarks)
{
	ContactInfo *ci = g_new0(ContactInfo, 1);
	
	g_return_val_if_fail(ci != NULL, NULL);
	g_return_val_if_fail(address != NULL, NULL); /* address should be valid */
	ci->name    = name ? g_strdup(name) : NULL;
	ci->address = g_strdup(address);
	ci->remarks = remarks ? g_strdup(remarks) : NULL;
	if (NULL == ci->address) {
		addressbook_destroy_contact(ci);
		ci = NULL;
	}
	return ci;
}

static void addressbook_group_menu_selected(GtkMenuItem *menuitem,
											ContactInfo *data)
{
	const gchar *group_name = (const gchar *) gtk_object_get_data(GTK_OBJECT(menuitem),
																  "group_name");
													   
	if (!group_name) {
		g_warning("%s(%d) - invalid group name\n", __FILE__, __LINE__);
		return ;
	}
	g_return_if_fail(group_name != NULL); 

	g_message("selected group %s from menu\n", group_name);
	g_message("selected %s <%s>\n", data->name ? data->name : data->address, data->address);

	addressbook_access();
	addressbook_add_contact(group_name, data->name ? data->name : data->address, 
							data->address, data->remarks ? data->remarks : data->address);
	addressbook_unaccess();

	g_free(data);
}

/* addressbook_add_contact_by_meny() - launches menu with group items. submenu may be
 * the menu item in the parent menu, or NULL for a normal right-click context menu */
gboolean addressbook_add_contact_by_menu(GtkWidget   *submenu,
										 const gchar *name, 
										 const gchar *address, 
										 const gchar *remarks)
{
	GtkWidget	*menu, *menuitem;
	GList		*groups, *tmp;
	ContactInfo *ci;

	ci = addressbook_new_contact(name, address, remarks);
	g_return_val_if_fail(ci != NULL, FALSE);

	addressbook_access();
	groups = addressbook_get_group_list();
	g_return_val_if_fail(groups != NULL, (addressbook_destroy_contact(ci), FALSE));
	
	menu = gtk_menu_new();
	g_return_val_if_fail(menu != NULL, (g_list_free(groups), addressbook_destroy_contact(ci), FALSE));

	/* add groups to menu */
	for (tmp = g_list_first(groups); tmp != NULL; tmp = g_list_next(tmp)) {
		const gchar *display_name;
		gchar *original_name = (gchar *) tmp->data;
		gboolean addItem = TRUE;

		if (!g_strcasecmp(original_name, ADDRESS_TAG_PERSONAL)) {
			display_name = addressbook_get_personal_folder_name();
		}
		else if (!g_strcasecmp(original_name, ADDRESS_TAG_COMMON)) {
			display_name = addressbook_get_common_folder_name();
		}
		else if( ! g_strcasecmp( original_name, ADDRESS_TAG_VCARD ) ) {
			addItem = FALSE;
		}
#ifdef USE_JPILOT
		else if( ! g_strcasecmp( original_name, ADDRESS_TAG_JPILOT ) ) {
			addItem = FALSE;
		}
#endif
#ifdef USE_LDAP
		else if( ! g_strcasecmp( original_name, ADDRESS_TAG_LDAP ) ) {
			addItem = FALSE;
		}
#endif
		else {
			display_name = original_name;
		}

		if( addItem ) {
			original_name = g_strdup(original_name);
			menuitem = gtk_menu_item_new_with_label(display_name);
			/* register the duplicated string pointer as object data,
			 * so we get the opportunity to free it */
			gtk_object_set_data_full(GTK_OBJECT(menuitem), "group_name", 
								 original_name, 
								 (GtkDestroyNotify) group_object_data_destroy);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
						   GTK_SIGNAL_FUNC(addressbook_group_menu_selected),
						   (gpointer)(ci));
			gtk_menu_append(GTK_MENU(menu), menuitem);
			gtk_widget_show(menuitem);
		}
	}

	gtk_widget_show(menu);

	if (submenu) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menu);
		gtk_widget_set_sensitive(GTK_WIDGET(submenu), TRUE);
	} 
	else {
		gtk_widget_grab_focus(GTK_WIDGET(menu));
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 1, GDK_CURRENT_TIME);
	}

	if (groups) g_list_free(groups);
	return TRUE;
}

/*
* End of Source.
*/

