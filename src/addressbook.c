/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
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

#include "main.h"
#include "addressbook.h"
#include "manage_window.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "xml.h"
#include "prefs_gtk.h"
#include "procmime.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "about.h"
#include "addr_compl.h"

#include "mgutils.h"
#include "addressitem.h"
#include "addritem.h"
#include "addrcache.h"
#include "addrbook.h"
#include "addrindex.h"
#include "addressadd.h"
#include "vcard.h"
#include "editvcard.h"
#include "editgroup.h"
#include "editaddress.h"
#include "editbook.h"
#include "importldif.h"
#include "importmutt.h"
#include "importpine.h"

#ifdef USE_JPILOT
#include "jpilot.h"
#include "editjpilot.h"
#endif

#ifdef USE_LDAP
#include <pthread.h>
#include "ldapserver.h"
#include "editldap.h"

#define ADDRESSBOOK_LDAP_BUSYMSG "Busy"
#endif

#include "addrquery.h"
#include "addrselect.h"
#include "addrclip.h"
#include "addrgather.h"
#include "adbookbase.h"
#include "exphtmldlg.h"
#include "expldifdlg.h"
#include "browseldap.h"

typedef enum
{
	COL_NAME	= 0,
	COL_ADDRESS	= 1,
	COL_REMARKS	= 2
} AddressBookColumnPos;

#define N_COLS	3
#define COL_NAME_WIDTH		164
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
static GdkPixmap *interfacexpm;
static GdkBitmap *interfacexpmmask;
static GdkPixmap *bookxpm;
static GdkBitmap *bookxpmmask;
static GdkPixmap *addressxpm;
static GdkBitmap *addressxpmmask;
static GdkPixmap *vcardxpm;
static GdkBitmap *vcardxpmmask;
static GdkPixmap *jpilotxpm;
static GdkBitmap *jpilotxpmmask;
static GdkPixmap *categoryxpm;
static GdkBitmap *categoryxpmmask;
static GdkPixmap *ldapxpm;
static GdkBitmap *ldapxpmmask;
static GdkPixmap *addrsearchxpm;
static GdkPixmap *addrsearchxpmmask;

/* Message buffer */
static gchar addressbook_msgbuf[ ADDRESSBOOK_MSGBUF_SIZE ];

/* Address list selection */
static AddrSelectList *_addressSelect_ = NULL;
static AddressClipboard *_clipBoard_ = NULL;

/* Address index file and interfaces */
static AddressIndex *_addressIndex_ = NULL;
static GList *_addressInterfaceList_ = NULL;
static GList *_addressIFaceSelection_ = NULL;
#define ADDRESSBOOK_IFACE_SELECTION "1/y,3/y,4/y,2/n"

static AddressBook_win addrbook;

static GHashTable *_addressBookTypeHash_ = NULL;
static GList *_addressBookTypeList_ = NULL;

static void addressbook_create			(void);
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

static gboolean addressbook_tree_selected	(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_select_row_tree		(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_list_row_selected	(GtkCTree	*clist,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_list_row_unselected	(GtkCTree	*clist,
						 GtkCTreeNode	*node,
						 gint		 column,
						 gpointer	 data);
static void addressbook_person_expand_node	(GtkCTree	*ctree,
						 GList		*node,
						 gpointer	*data );
static void addressbook_person_collapse_node	(GtkCTree	*ctree,
						 GList		*node,
						 gpointer	*data );
static void addressbook_entry_gotfocus		(GtkWidget	*widget);

#if 0
static void addressbook_entry_changed		(GtkWidget	*widget);
#endif

static gboolean addressbook_list_button_pressed	(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_list_button_released(GtkWidget	*widget,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_tree_button_pressed	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 gpointer	 data);
static gboolean addressbook_tree_button_released(GtkWidget	*ctree,
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
static void addressbook_treenode_edit_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);
static void addressbook_treenode_delete_cb	(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

static void addressbook_change_node_name	(GtkCTreeNode	*node,
						 const gchar	*name);

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
static void addressbook_file_save_cb		(gpointer	 data,
						 guint		 action,
						 GtkWidget	*widget);

/* Data source edit stuff */
static void addressbook_new_book_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
static void addressbook_new_vcard_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
#endif

#ifdef USE_LDAP
static void addressbook_new_ldap_cb		(gpointer	 data,
	       					 guint		 action,
						 GtkWidget	*widget);
#endif

static void addressbook_set_clist		(AddressObject	*obj);

static void addressbook_load_tree		(void);
void addressbook_read_file			(void);

static GtkCTreeNode *addressbook_add_object	(GtkCTreeNode	*node,
						 AddressObject	*obj);
static void addressbook_treenode_remove_item	( void );

static AddressDataSource *addressbook_find_datasource
						(GtkCTreeNode	*node );

static AddressBookFile *addressbook_get_book_file(void);

static GtkCTreeNode *addressbook_node_add_folder
						(GtkCTreeNode	*node,
						AddressDataSource *ds,
						ItemFolder	*itemFolder,
						AddressObjectType otype);
static GtkCTreeNode *addressbook_node_add_group (GtkCTreeNode	*node,
						AddressDataSource *ds,
						ItemGroup	*itemGroup);
static void addressbook_tree_remove_children	(GtkCTree	*ctree,
						GtkCTreeNode	*parent);
static void addressbook_move_nodes_up		(GtkCTree	*ctree,
						GtkCTreeNode	*node);
static GtkCTreeNode *addressbook_find_group_node (GtkCTreeNode	*parent,
						   ItemGroup	*group);
static gboolean key_pressed			(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static gint addressbook_treenode_compare_func	(GtkCList	*clist,
						 gconstpointer	 ptr1,
						 gconstpointer	 ptr2);
static gint addressbook_list_compare_func	(GtkCList	*clist,
						 gconstpointer	 ptr1,
						 gconstpointer	 ptr2);
static void addressbook_folder_load_one_person	(GtkCTree *clist, 
						 ItemPerson *person,  
						 AddressTypeControlItem *atci, 
						 AddressTypeControlItem *atciMail);
static void addressbook_folder_refresh_one_person(GtkCTree *clist, 
						  ItemPerson *person);
static void addressbook_folder_remove_one_person(GtkCTree *clist, 
						 ItemPerson *person);
static void addressbook_folder_remove_node	(GtkCTree *clist, 
						 GtkCTreeNode *node);

#ifdef USE_LDAP
static void addressbook_ldap_show_message	( LdapServer *server );
#endif

/* LUT's and IF stuff */
static void addressbook_free_treenode		( gpointer data );
AddressTypeControlItem *addrbookctl_lookup	(gint		 ot);
AddressTypeControlItem *addrbookctl_lookup_iface(AddressIfType	 ifType);

void addrbookctl_build_map			(GtkWidget	*window);
void addrbookctl_build_iflist			(void);
AdapterInterface *addrbookctl_find_interface	(AddressIfType	 ifType);
void addrbookctl_build_ifselect			(void);

static void addrbookctl_free_interface		(AdapterInterface *adapter);
static void addrbookctl_free_datasource		(AdapterDSource	  *adapter);
static void addrbookctl_free_folder		(AdapterFolder	  *adapter);
static void addrbookctl_free_group		(AdapterGroup	  *adapter);

static void addressbook_list_select_clear	( void );
static void addressbook_list_select_add		( AddrItemObject    *aio,
						  AddressDataSource *ds );
static void addressbook_list_select_remove	( AddrItemObject    *aio );

static void addressbook_import_ldif_cb		( void );
static void addressbook_import_mutt_cb		( void );
static void addressbook_import_pine_cb		( void );
static void addressbook_export_html_cb		( void );
static void addressbook_export_ldif_cb		( void );
static void addressbook_clip_cut_cb		( void );
static void addressbook_clip_copy_cb		( void );
static void addressbook_clip_paste_cb		( void );
static void addressbook_clip_paste_address_cb	( void );
static void addressbook_treenode_cut_cb		( void );
static void addressbook_treenode_copy_cb	( void );
static void addressbook_treenode_paste_cb	( void );

static void addressbook_mail_to_cb		( void );

#ifdef USE_LDAP
static void addressbook_browse_entry_cb		( void );
#endif

static void addressbook_start_drag(GtkWidget *widget, gint button, 
				   GdkEvent *event,
			           void *data);
static void addressbook_drag_data_get(GtkWidget        *widget,
				     GdkDragContext   *drag_context,
				     GtkSelectionData *selection_data,
				     guint             info,
				     guint             time,
				     void	      *data);
static gboolean addressbook_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  void           *data);
static void addressbook_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     void           *data);
static void addressbook_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *drag_context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					void             *pdata);

static GtkTargetEntry addressbook_drag_types[] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};

static GtkTargetList *addressbook_target_list = NULL;


static GtkItemFactoryEntry addressbook_entries[] =
{
	{N_("/_File"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_File/New _Book"),	"<alt>B",	addressbook_new_book_cb,        0, NULL},
	{N_("/_File/New _vCard"),	"<alt>D",	addressbook_new_vcard_cb,       0, NULL},
#ifdef USE_JPILOT
	{N_("/_File/New _JPilot"),	"<alt>J",	addressbook_new_jpilot_cb,      0, NULL},
#endif
#ifdef USE_LDAP
	{N_("/_File/New _Server"),	"<alt>S",	addressbook_new_ldap_cb,        0, NULL},
#endif
	{N_("/_File/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_File/_Edit"),		NULL,		addressbook_treenode_edit_cb,   0, NULL},
	{N_("/_File/_Delete"),		NULL,		addressbook_treenode_delete_cb, 0, NULL},
	{N_("/_File/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_File/_Save"),		"<alt>S",	addressbook_file_save_cb,       0, NULL},
	{N_("/_File/_Close"),		"<alt>W",	close_cb,                       0, NULL},
	{N_("/_Edit"),			NULL,		NULL, 0, "<Branch>"},
	{N_("/_Edit/C_ut"),		"<ctl>X",	addressbook_clip_cut_cb,        0, NULL},
	{N_("/_Edit/_Copy"),		"<ctl>C",	addressbook_clip_copy_cb,       0, NULL},
	{N_("/_Edit/_Paste"),		"<ctl>V",	addressbook_clip_paste_cb,      0, NULL},
	{N_("/_Edit/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Edit/Pa_ste Address"),	NULL,		addressbook_clip_paste_address_cb, 0, NULL},
	{N_("/_Address"),		NULL,		NULL, 0, "<Branch>"},
	{N_("/_Address/New _Address"),	"<alt>N",	addressbook_new_address_cb,     0, NULL},
	{N_("/_Address/New _Group"),	"<alt>G",	addressbook_new_group_cb,       0, NULL},
	{N_("/_Address/New _Folder"),	"<alt>R",	addressbook_new_folder_cb,      0, NULL},
	{N_("/_Address/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Address/_Edit"),		"<alt>Return",	addressbook_edit_address_cb,    0, NULL},
	{N_("/_Address/_Delete"),	NULL,		addressbook_delete_address_cb,  0, NULL},
	{N_("/_Address/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Address/_Mail To"),	NULL,		addressbook_mail_to_cb,         0, NULL},
	{N_("/_Tools/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Tools/Import _LDIF file..."), NULL,	addressbook_import_ldif_cb,	0, NULL},
	{N_("/_Tools/Import M_utt file..."), NULL,	addressbook_import_mutt_cb,	0, NULL},
	{N_("/_Tools/Import _Pine file..."), NULL,	addressbook_import_pine_cb,	0, NULL},
	{N_("/_Tools/---"),		NULL,		NULL, 0, "<Separator>"},
	{N_("/_Tools/Export _HTML..."), NULL,           addressbook_export_html_cb,	0, NULL},
	{N_("/_Tools/Export LDI_F..."), NULL,           addressbook_export_ldif_cb,	0, NULL},
	{N_("/_Help"),			NULL,		NULL, 0, "<LastBranch>"},
	{N_("/_Help/_About"),		NULL,		about_show, 0, NULL}
};

static GtkItemFactoryEntry addressbook_tree_popup_entries[] =
{
	{N_("/_Edit"),		NULL, addressbook_treenode_edit_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_treenode_delete_cb, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/New _Address"),	NULL, addressbook_new_address_cb,     0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,       0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,      0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/C_ut"),		NULL, addressbook_treenode_cut_cb,    0, NULL},
	{N_("/_Copy"),		NULL, addressbook_treenode_copy_cb,   0, NULL},
	{N_("/_Paste"),		NULL, addressbook_treenode_paste_cb,  0, NULL}
};

static GtkItemFactoryEntry addressbook_list_popup_entries[] =
{
	{N_("/_Edit"),		NULL, addressbook_edit_address_cb,   0, NULL},
	{N_("/_Delete"),	NULL, addressbook_delete_address_cb, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/New _Address"),	NULL, addressbook_new_address_cb,    0, NULL},
	{N_("/New _Group"),	NULL, addressbook_new_group_cb,      0, NULL},
	{N_("/New _Folder"),	NULL, addressbook_new_folder_cb,     0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/C_ut"),		NULL, addressbook_clip_cut_cb,       0, NULL},
	{N_("/_Copy"),		NULL, addressbook_clip_copy_cb,      0, NULL},
	{N_("/_Paste"),		NULL, addressbook_clip_paste_cb,     0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/Pa_ste Address"),	NULL, addressbook_clip_paste_address_cb, 0, NULL},
	{N_("/_Mail To"),	NULL, addressbook_mail_to_cb,            0, NULL},
#ifdef USE_LDAP
	{N_("/_Browse Entry"),  NULL, addressbook_browse_entry_cb,	 0, NULL},
#endif	
};

/**
 * Structure of error message table.
 */
typedef struct _ErrMsgTableEntry ErrMsgTableEntry;
struct _ErrMsgTableEntry {
	gint	code;
	gchar	*description;
};

static gchar *_errMsgUnknown_ = N_( "Unknown" );

/**
 * Lookup table of error messages for general errors. Note that a NULL
 * description signifies the end of the table.
 */
static ErrMsgTableEntry _lutErrorsGeneral_[] = {
	{ MGU_SUCCESS,		N_("Success") },
	{ MGU_BAD_ARGS,		N_("Bad arguments") },
	{ MGU_NO_FILE,		N_("File not specified") },
	{ MGU_OPEN_FILE,	N_("Error opening file") },
	{ MGU_ERROR_READ,	N_("Error reading file") },
	{ MGU_EOF,		N_("End of file encountered") },
	{ MGU_OO_MEMORY,	N_("Error allocating memory") },
	{ MGU_BAD_FORMAT,	N_("Bad file format") },
	{ MGU_ERROR_WRITE,	N_("Error writing to file") },
	{ MGU_OPEN_DIRECTORY,	N_("Error opening directory") },
	{ MGU_NO_PATH,      	N_("No path specified") },
	{ 0,			NULL }
};

#ifdef USE_LDAP
/**
 * Lookup table of error messages for LDAP errors.
 */
static ErrMsgTableEntry _lutErrorsLDAP_[] = {
	{ LDAPRC_SUCCESS,	N_("Success") },
	{ LDAPRC_CONNECT,	N_("Error connecting to LDAP server") },
	{ LDAPRC_INIT,		N_("Error initializing LDAP") },
	{ LDAPRC_BIND,		N_("Error binding to LDAP server") },
	{ LDAPRC_SEARCH,	N_("Error searching LDAP database") },
	{ LDAPRC_TIMEOUT,	N_("Timeout performing LDAP operation") },
	{ LDAPRC_CRITERIA,	N_("Error in LDAP search criteria") },
	{ LDAPRC_NOENTRIES,	N_("No LDAP entries found for search criteria") },
	{ LDAPRC_STOP_FLAG,	N_("LDAP search terminated on request") },
	{ LDAPRC_TLS,		N_("Error starting TLS connection") },
	{ 0,			NULL }
};
#endif

/**
 * Lookup message for specified error code.
 * \param lut  Lookup table.
 * \param code Code to lookup.
 * \return Description associated to code.
 */
static gchar *addressbook_err2string( ErrMsgTableEntry lut[], gint code ) {
        gchar *desc = NULL;
        ErrMsgTableEntry entry;
        gint i;

        for( i = 0; ; i++ ) {
                entry = lut[ i ];
                if( entry.description == NULL ) break;
                if( entry.code == code ) {
                        desc = entry.description;
                        break;
                }
        }
        if( ! desc ) {
		desc = _errMsgUnknown_;
        }
        return desc;
}

void addressbook_open(Compose *target)
{
	/* Initialize all static members */
	if( _clipBoard_ == NULL ) {
		_clipBoard_ = addrclip_create();
	}
	if( _addressIndex_ != NULL ) {
		addrclip_set_index( _clipBoard_, _addressIndex_ );
	}
	if( _addressSelect_ == NULL ) {
		_addressSelect_ = addrselect_list_create();
	}
	if (!addrbook.window) {
		addressbook_read_file();
		addressbook_create();
		addressbook_load_tree();
		gtk_ctree_select(GTK_CTREE(addrbook.ctree),
				 GTK_CTREE_NODE(GTK_CLIST(addrbook.ctree)->row_list));
	}
	else {
		gtk_widget_hide(addrbook.window);
	}

	gtk_widget_show_all(addrbook.window);
	addressbook_set_target_compose(target);
}

/**
 * Destroy addressbook.
 */
void addressbook_destroy( void ) {
	/* Free up address stuff */
	if( _addressSelect_ != NULL ) {
		addrselect_list_free( _addressSelect_ );
	}
	if( _clipBoard_ != NULL ) {
		addrclip_free( _clipBoard_ );
	}
	if( _addressIndex_ != NULL ) {
		addrindex_free_index( _addressIndex_ );
		addrindex_teardown();
	}
	_addressSelect_ = NULL;
	_clipBoard_ = NULL;
	_addressIndex_ = NULL;
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

/**
 * Refresh addressbook and save to file(s).
 */
void addressbook_refresh( void )
{
	if (addrbook.window) {
		if (addrbook.treeSelected) {
			gtk_ctree_select(GTK_CTREE(addrbook.ctree),
					 addrbook.treeSelected);
		}
	}
	addressbook_export_to_file();
}

/*
* Create the address book widgets. The address book contains two CTree widgets: the
* address index tree on the left and the address list on the right.
*
* The address index tree displays a hierarchy of interfaces and groups. Each node in
* this tree is linked to an address Adapter. Adapters have been created for interfaces,
* data sources and folder objects.
*
* The address list displays group, person and email objects. These items are linked
* directly to ItemGroup, ItemPerson and ItemEMail objects inside the address book data
* sources.
*
* In the tradition of MVC architecture, the data stores have been separated from the
* GUI components. The addrindex.c file provides the interface to all data stores.
*/
static void addressbook_create(void)
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
	GList *nodeIf;

	gchar *titles[N_COLS];
	gchar *dummy_titles[1];
	gchar *text;
	gint i;

	debug_print("Creating addressbook window...\n");

	titles[COL_NAME]    = _("Name");
	titles[COL_ADDRESS] = _("E-Mail address");
	titles[COL_REMARKS] = _("Remarks");
	dummy_titles[0]     = "";

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Address book"));
	gtk_widget_set_size_request(window, ADDRESSBOOK_WIDTH, ADDRESSBOOK_HEIGHT);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_widget_realize(window);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(addressbook_close), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

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
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(ctree_swin, COL_FOLDER_WIDTH + 40, -1);

	/* Address index */
	ctree = gtk_sctree_new_with_titles(1, 0, dummy_titles);
	gtk_container_add(GTK_CONTAINER(ctree_swin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_width(GTK_CLIST(ctree), 0, COL_FOLDER_WIDTH);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree),
				   addressbook_treenode_compare_func);

	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(addressbook_tree_selected), NULL);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(addressbook_tree_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(addressbook_tree_button_released),
			 NULL);
	/* TEMPORARY */
	g_signal_connect(G_OBJECT(ctree), "select_row",
			 G_CALLBACK(addressbook_select_row_tree), NULL);

	gtk_drag_dest_set(ctree, GTK_DEST_DEFAULT_ALL & ~GTK_DEST_DEFAULT_HIGHLIGHT,
			  addressbook_drag_types, 1,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_DEFAULT);
	g_signal_connect(G_OBJECT(ctree), "drag_motion",
			 G_CALLBACK(addressbook_drag_motion_cb),
			 ctree);
	g_signal_connect(G_OBJECT(ctree), "drag_leave",
			 G_CALLBACK(addressbook_drag_leave_cb),
			 ctree);
	g_signal_connect(G_OBJECT(ctree), "drag_data_received",
			 G_CALLBACK(addressbook_drag_received_cb),
			 ctree);

	clist_vbox = gtk_vbox_new(FALSE, 4);

	clist_swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(clist_swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(clist_vbox), clist_swin, TRUE, TRUE, 0);

	/* Address list */
	clist = gtk_sctree_new_with_titles(N_COLS, 0, titles);
	gtk_container_add(GTK_CONTAINER(clist_swin), clist);
	gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_EXTENDED);
	gtk_ctree_set_line_style(GTK_CTREE(clist), GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style(GTK_CTREE(clist), GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent(GTK_CTREE(clist), CTREE_INDENT);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_NAME,
				   COL_NAME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(clist), COL_ADDRESS,
				   COL_ADDRESS_WIDTH);
	gtk_clist_set_compare_func(GTK_CLIST(clist),
				   addressbook_list_compare_func);

	for (i = 0; i < N_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[i].button,
				       GTK_CAN_FOCUS);

	g_signal_connect(G_OBJECT(clist), "tree_select_row",
			 G_CALLBACK(addressbook_list_row_selected), NULL);
	g_signal_connect(G_OBJECT(clist), "tree_unselect_row",
			 G_CALLBACK(addressbook_list_row_unselected), NULL);
	g_signal_connect(G_OBJECT(clist), "button_press_event",
			 G_CALLBACK(addressbook_list_button_pressed),
			 NULL);
	g_signal_connect(G_OBJECT(clist), "button_release_event",
			 G_CALLBACK(addressbook_list_button_released),
			 NULL);
	g_signal_connect(G_OBJECT(clist), "tree_expand",
			 G_CALLBACK(addressbook_person_expand_node), NULL );
	g_signal_connect(G_OBJECT(clist), "tree_collapse",
			 G_CALLBACK(addressbook_person_collapse_node), NULL );
	g_signal_connect(G_OBJECT(clist), "start_drag",
			 G_CALLBACK(addressbook_start_drag), NULL);
	g_signal_connect(G_OBJECT(clist), "drag_data_get",
			 G_CALLBACK(addressbook_drag_data_get), NULL);	
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(clist_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Name:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

	address_completion_register_entry(GTK_ENTRY(entry));
	g_signal_connect(G_OBJECT(entry), "focus_in_event",
			 G_CALLBACK(addressbook_entry_gotfocus), NULL);

#if 0
	g_signal_connect(G_OBJECT(entry), "changed",
			 G_CALLBACK(addressbook_entry_changed), NULL);
#endif

	paned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox2), paned, TRUE, TRUE, 0);
	gtk_paned_add1(GTK_PANED(paned), ctree_swin);
	gtk_paned_add2(GTK_PANED(paned), clist_vbox);

	/* Status bar */
	hsbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hsbox, FALSE, FALSE, BORDER_WIDTH);
	statusbar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hsbox), statusbar, TRUE, TRUE, BORDER_WIDTH);

	/* Button panel */
	hbbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 2);
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

	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(addressbook_del_clicked), NULL);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(addressbook_reg_clicked), NULL);
	g_signal_connect(G_OBJECT(lup_btn), "clicked",
			 G_CALLBACK(addressbook_lup_clicked), NULL);

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

	g_signal_connect(G_OBJECT(to_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_TO));
	g_signal_connect(G_OBJECT(cc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_CC));
	g_signal_connect(G_OBJECT(bcc_btn), "clicked",
			 G_CALLBACK(addressbook_to_clicked),
			 GINT_TO_POINTER(COMPOSE_BCC));

	/* Build icons for interface */
	stock_pixmap_gdk( window, STOCK_PIXMAP_INTERFACE,
			  &interfacexpm, &interfacexpmmask );

	/* Build control tables */
	addrbookctl_build_map(window);
	addrbookctl_build_iflist();
	addrbookctl_build_ifselect();

	/* Add each interface into the tree as a root level folder */
	nodeIf = _addressInterfaceList_;
	while( nodeIf ) {
		AdapterInterface *adapter = nodeIf->data;
		AddressInterface *iface = adapter->interface;
		nodeIf = g_list_next(nodeIf);

		if(iface->useInterface) {
			AddressTypeControlItem *atci = adapter->atci;
			text = atci->displayName;
			adapter->treeNode =
				gtk_ctree_insert_node( GTK_CTREE(ctree),
					NULL, NULL, &text, FOLDER_SPACING,
					interfacexpm, interfacexpmmask,
					interfacexpm, interfacexpmmask,
					FALSE, FALSE );
			menu_set_sensitive( menu_factory, atci->menuCommand, adapter->haveLibrary );
			gtk_ctree_node_set_row_data_full(
				GTK_CTREE(ctree), adapter->treeNode, adapter,
				addressbook_free_treenode );
		}
	}

	/* Popup menu */
	n_entries = sizeof(addressbook_tree_popup_entries) /
		sizeof(addressbook_tree_popup_entries[0]);
	tree_popup = menu_create_items(addressbook_tree_popup_entries,
				       n_entries,
				       "<AddressBookTree>", &tree_factory,
				       NULL);
	g_signal_connect(G_OBJECT(tree_popup), "selection_done",
			 G_CALLBACK(addressbook_popup_close), NULL);
	n_entries = sizeof(addressbook_list_popup_entries) /
		sizeof(addressbook_list_popup_entries[0]);
	list_popup = menu_create_items(addressbook_list_popup_entries,
				       n_entries,
				       "<AddressBookList>", &list_factory,
				       NULL);

	addrbook.window  = window;
	addrbook.menubar = menubar;
	addrbook.ctree   = ctree;
	addrbook.ctree_swin
			 = ctree_swin;
	addrbook.clist   = clist;
	addrbook.entry   = entry;
	addrbook.statusbar = statusbar;
	addrbook.status_cid = gtk_statusbar_get_context_id(
			GTK_STATUSBAR(statusbar), "Addressbook Window" );

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

	addrbook.listSelected = NULL;
	address_completion_start(window);
	gtk_widget_show_all(window);
	gtk_widget_set_sensitive(addrbook.lup_btn, FALSE);

}

/**
 * Close address book window and save to file(s).
 */
static gint addressbook_close( void ) {
	gtk_widget_hide(addrbook.window);
	addressbook_export_to_file();
	return TRUE;
}

/**
 * Display message in status line.
 * \param msg Message to display.
 */
static void addressbook_status_show( gchar *msg ) {
	if( addrbook.statusbar != NULL ) {
		gtk_statusbar_pop(
			GTK_STATUSBAR(addrbook.statusbar),
			addrbook.status_cid );
		if( msg ) {
			gtk_statusbar_push(
				GTK_STATUSBAR(addrbook.statusbar),
				addrbook.status_cid, msg );
		}
	}
}

static void addressbook_ds_status_message( AddressDataSource *ds, gchar *msg ) {
	*addressbook_msgbuf = '\0';
	if( ds ) {
		gchar *name;

		name = addrindex_ds_get_name( ds );
		g_snprintf( addressbook_msgbuf, sizeof(addressbook_msgbuf),
			    "%s: %s", name, msg );
	}
	else {
		g_snprintf( addressbook_msgbuf, sizeof(addressbook_msgbuf),
			    "%s", msg );
	}
	addressbook_status_show( addressbook_msgbuf );
}

static void addressbook_ds_show_message( AddressDataSource *ds ) {
	gint retVal;
	gchar *name;
	gchar *desc;
	*addressbook_msgbuf = '\0';
	if( ds ) {
		name = addrindex_ds_get_name( ds );
		retVal = addrindex_ds_get_status_code( ds );
		if( retVal == MGU_SUCCESS ) {
			g_snprintf( addressbook_msgbuf,
				    sizeof(addressbook_msgbuf), "%s", name );
		}
		else {
			desc = addressbook_err2string( _lutErrorsGeneral_, retVal );
			g_snprintf( addressbook_msgbuf, 
			    sizeof(addressbook_msgbuf), "%s: %s", name, desc );
		}
	}
	addressbook_status_show( addressbook_msgbuf );
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
#ifndef CLAWS		
		if (addrbook.target_compose->use_bcc)
#endif			
			bcc_sens = TRUE;
	}

	gtk_widget_set_sensitive(addrbook.to_btn, to_sens);
	gtk_widget_set_sensitive(addrbook.cc_btn, cc_sens);
	gtk_widget_set_sensitive(addrbook.bcc_btn, bcc_sens);
}

/*
* Delete one or more objects from address list.
*/
static void addressbook_del_clicked(GtkButton *button, gpointer data)
{
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *pobj;
	AdapterDSource *ads = NULL;
	GtkCTreeNode *nodeList;
	gboolean procFlag;
	AlertValue aval;
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface;
	AddrItemObject *aio;
	AddrSelectItem *item;
	GList *list, *node;
	gboolean refreshList = FALSE;
	
	pobj = gtk_ctree_node_get_row_data(ctree, addrbook.opened );
	g_return_if_fail(pobj != NULL);

	/* Test whether anything selected for deletion */
	nodeList = addrbook.listSelected;
	aio = gtk_ctree_node_get_row_data( clist, nodeList );
	if( aio == NULL) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	/* Test for read only */
	iface = ds->interface;
	if( iface->readOnly ) {
		alertpanel( _("Delete address(es)"),
			_("This address data is readonly and cannot be deleted."),
			GTK_STOCK_CLOSE, NULL, NULL );
		return;
	}

	/* Test whether Ok to proceed */
	procFlag = FALSE;
	if( pobj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(pobj);
		if( ads->subType == ADDR_BOOK ) procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		procFlag = TRUE;
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		procFlag = TRUE;
	}
	if( ! procFlag ) return;
	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	/* Confirm deletion */
	aval = alertpanel( _("Delete address(es)"),
			_("Really delete the address(es)?"),
			GTK_STOCK_YES, GTK_STOCK_NO, NULL );
	if( aval != G_ALERTDEFAULT ) return;

	/* Process deletions */
	if( pobj->type == ADDR_DATASOURCE || pobj->type == ADDR_ITEM_FOLDER ) {
		/* Items inside folders */
		list = addrselect_get_list( _addressSelect_ );
		node = list;
		while( node ) {
			item = node->data;
			node = g_list_next( node );
			aio = ( AddrItemObject * ) item->addressItem;
			if( aio->type == ADDR_ITEM_GROUP ) {
				ItemGroup *item = ( ItemGroup * ) aio;
				GtkCTreeNode *nd = NULL;

				nd = addressbook_find_group_node( addrbook.opened, item );
				item = addrbook_remove_group( abf, item );
				if( item ) {
					addritem_free_item_group( item );
				}
				/* Remove group from parent node */
				gtk_ctree_remove_node( ctree, nd );
				refreshList = TRUE;
			}
			else if( aio->type == ADDR_ITEM_PERSON ) {
				ItemPerson *item = ( ItemPerson * ) aio;
				addressbook_folder_remove_one_person( clist, item );
				item = addrbook_remove_person( abf, item );
				if( item ) {
					addritem_free_item_person( item );
				}
			}
			else if( aio->type == ADDR_ITEM_EMAIL ) {
				ItemEMail *item = ( ItemEMail * ) aio;
				ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(item);
				item = addrbook_person_remove_email( abf, person, item );
				if( item ) {
					addritem_free_item_email( item );
				}
				addressbook_folder_refresh_one_person( clist, person );
			}
		}
		g_list_free( list );
		addressbook_list_select_clear();
		if( refreshList ) gtk_ctree_select( ctree, addrbook.opened);
		return;
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		/* Items inside groups */
		list = addrselect_get_list( _addressSelect_ );
		node = list;
		while( node ) {
			item = node->data;
			node = g_list_next( node );
			aio = ( AddrItemObject * ) item->addressItem;
			if( aio->type == ADDR_ITEM_EMAIL ) {
				ItemEMail *item = ( ItemEMail * ) aio;
				ItemPerson *person = ( ItemPerson * ) ADDRITEM_PARENT(item);
				item = addrbook_person_remove_email( abf, person, item );
				if( item ) {
					addritem_free_item_email( item );
				}
			}
		}
		g_list_free( list );
		addressbook_list_select_clear();
		gtk_ctree_select( ctree, addrbook.opened);
		return;
	}

	gtk_ctree_node_set_row_data( clist, nodeList, NULL );
	gtk_ctree_remove_node( clist, nodeList );

}

static void addressbook_reg_clicked(GtkButton *button, gpointer data)
{
	addressbook_new_address_cb( NULL, 0, NULL );
}

gchar *addressbook_format_address( AddrItemObject * aio ) {
	gchar *buf = NULL;
	gchar *name = NULL;
	gchar *address = NULL;

	if( aio->type == ADDR_ITEM_EMAIL ) {
		ItemPerson *person = NULL;
		ItemEMail *email = ( ItemEMail * ) aio;

		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		if( email->address ) {
			if( ADDRITEM_NAME(email) ) {
				name = ADDRITEM_NAME(email);
				if( *name == '\0' ) {
					name = ADDRITEM_NAME(person);
				}
			}
			else if( ADDRITEM_NAME(person) ) {
				name = ADDRITEM_NAME(person);
			}
			else {
				buf = g_strdup( email->address );
			}
			address = email->address;
		}
	}
	else if( aio->type == ADDR_ITEM_PERSON ) {
		ItemPerson *person = ( ItemPerson * ) aio;
		GList *node = person->listEMail;

		name = ADDRITEM_NAME(person);
		if( node ) {
			ItemEMail *email = ( ItemEMail * ) node->data;
			address = email->address;
		}
	}
	if( address ) {
		if( name && name[0] != '\0' ) {
			if( strchr_with_skip_quote( name, '"', ',' ) )
				buf = g_strdup_printf( "\"%s\" <%s>", name, address );
			else
				buf = g_strdup_printf( "%s <%s>", name, address );
		}
		else {
			buf = g_strdup( address );
		}
	}

	return buf;
}

static void addressbook_to_clicked(GtkButton *button, gpointer data)
{
	GList *list, *node;
	Compose *compose;
	AddrSelectItem *item;
	AddrItemObject *aio;
	gchar *addr;

	compose = addrbook.target_compose;
	if( ! compose ) return;

	/* Nothing selected, but maybe there is something in text entry */
	addr = (char *)gtk_entry_get_text( GTK_ENTRY( addrbook.entry) );
	if ( addr ) {
		compose_entry_append(
			compose, addr, (ComposeEntryType)data );
	}

	/* Select from address list */
	list = addrselect_get_list( _addressSelect_ );
	node = list;
	while( node ) {
		item = node->data;
		node = g_list_next( node );
		aio = item->addressItem;
		if( aio->type == ADDR_ITEM_PERSON ||
		    aio->type == ADDR_ITEM_EMAIL ) {
			addr = addressbook_format_address( aio );
			compose_entry_append(
				compose, addr, (ComposeEntryType) data );
			g_free( addr );
		}
		else if( aio->type == ADDR_ITEM_GROUP ) {
			ItemGroup *group = ( ItemGroup * ) aio;
			GList *nodeMail = group->listEMail;
			while( nodeMail ) {
				ItemEMail *email = nodeMail->data;

				addr = addressbook_format_address(
						( AddrItemObject * ) email );
				compose_entry_append(
					compose, addr, (ComposeEntryType) data );
				g_free( addr );
				nodeMail = g_list_next( nodeMail );
			}
		}
	}
	g_list_free( list );
}

static void addressbook_menubar_set_sensitive( gboolean sensitive ) {
	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",   sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete", sensitive );

	menu_set_sensitive( addrbook.menu_factory, "/Edit/Cut",    sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Copy",   sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste",  sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste Address",  sensitive );

	menu_set_sensitive( addrbook.menu_factory, "/Address/New Address", sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Group",   sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Folder",  sensitive );
	menu_set_sensitive( addrbook.menu_factory, "/Address/Mail To",     sensitive );
	gtk_widget_set_sensitive( addrbook.reg_btn, sensitive );
	gtk_widget_set_sensitive( addrbook.del_btn, sensitive );
}

static void addressbook_menuitem_set_sensitive( AddressObject *obj, GtkCTreeNode *node ) {
	gboolean canEdit = FALSE;
	gboolean canAdd = FALSE;
	gboolean canEditTr = TRUE;
	gboolean editAddress = FALSE;
	gboolean canExport = TRUE;
	AddressTypeControlItem *atci = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;

	if( obj == NULL ) return;
	if( obj->type == ADDR_INTERFACE ) {
		AdapterInterface *adapter = ADAPTER_INTERFACE(obj);
		iface = adapter->interface;
		if( iface ) {
			if( iface->haveLibrary ) {
				/* Enable appropriate File / New command */
				atci = adapter->atci;
				menu_set_sensitive( addrbook.menu_factory, atci->menuCommand, TRUE );
			}
		}
		canEditTr = canExport = FALSE;
	}
	else if( obj->type == ADDR_DATASOURCE ) {
		AdapterDSource *ads = ADAPTER_DSOURCE(obj);
		ds = ads->dataSource;
		iface = ds->interface;
		if( ! iface->readOnly ) {
			canAdd = canEdit = editAddress = TRUE;
		}
		if( ! iface->haveLibrary ) {
			canAdd = canEdit = editAddress = canExport = FALSE;
		}
	}
	else if( obj->type == ADDR_ITEM_FOLDER ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		if( ds ) {
			iface = ds->interface;
			if( iface->readOnly ) {
				canEditTr = FALSE;
			}
			else {
				canAdd = editAddress = TRUE;
			}
		}
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		if( ds ) {
			iface = ds->interface;
			if( ! iface->readOnly ) {
				editAddress = TRUE;
			}
		}
	}

	if( addrbook.listSelected == NULL ) canEdit = FALSE;

	/* Enable add */
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Address", editAddress );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Group",   canAdd );
	menu_set_sensitive( addrbook.menu_factory, "/Address/New Folder",  canAdd );
	gtk_widget_set_sensitive( addrbook.reg_btn, editAddress );

	/* Enable edit */
	menu_set_sensitive( addrbook.menu_factory, "/Address/Edit",   canEdit );
	menu_set_sensitive( addrbook.menu_factory, "/Address/Delete", canEdit );
	gtk_widget_set_sensitive( addrbook.del_btn, canEdit );

	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",      canEditTr );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete",    canEditTr );

	/* Export data */
	menu_set_sensitive( addrbook.menu_factory, "/Tools/Export HTML...", canExport );
	menu_set_sensitive( addrbook.menu_factory, "/Tools/Export LDIF...", canExport );
}

/**
 * Address book tree callback function that responds to selection of tree
 * items.
 *
 * \param ctree  Tree widget.
 * \param node   Node that was selected.
 * \param column Column number where selected occurred.
 * \param data   Pointer to user data.
 */
static gboolean addressbook_tree_selected(GtkCTree *ctree, GtkCTreeNode *node,
				      gint column, gpointer data)
{
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;
	ItemFolder *rootFolder = NULL;
	AddressObjectType aot;

	addrbook.treeSelected = node;
	addrbook.listSelected = NULL;
	addressbook_status_show( "" );
	if( addrbook.entry != NULL ) gtk_entry_set_text(GTK_ENTRY(addrbook.entry), "");

	if( addrbook.clist ) gtk_clist_clear( GTK_CLIST(addrbook.clist) );
	if( node ) obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return FALSE;

	addrbook.opened = node;

	if( obj->type == ADDR_DATASOURCE ) {
		/* Read from file */
		static gboolean tVal = TRUE;

		ads = ADAPTER_DSOURCE(obj);
		if( ads == NULL ) return FALSE;
		ds = ads->dataSource;
		if( ds == NULL ) return FALSE;		

		if( addrindex_ds_get_modify_flag( ds ) ) {
			addrindex_ds_read_data( ds );
		}

		if( ! addrindex_ds_get_read_flag( ds ) ) {
			addrindex_ds_read_data( ds );
		}
		addressbook_ds_show_message( ds );

		if( ! addrindex_ds_get_access_flag( ds ) ) {
			/* Remove existing folders and groups */
			gtk_clist_freeze( GTK_CLIST(ctree) );
			addressbook_tree_remove_children( ctree, node );
			gtk_clist_thaw( GTK_CLIST(ctree) );

			/* Load folders into the tree */
			rootFolder = addrindex_ds_get_root_folder( ds );
			if( ds->type == ADDR_IF_JPILOT ) {
				aot = ADDR_CATEGORY;
			}
			else if( ds->type == ADDR_IF_LDAP ) {
				aot = ADDR_LDAP_QUERY;
			}
			else {
				aot = ADDR_ITEM_FOLDER;
			}
			addressbook_node_add_folder( node, ds, rootFolder, aot );
			addrindex_ds_set_access_flag( ds, &tVal );
			gtk_ctree_expand( ctree, node );
		}
	}

	/* Update address list */
	addressbook_set_clist( obj );

	/* Setup main menu selections */
	addressbook_menubar_set_sensitive( FALSE );
	addressbook_menuitem_set_sensitive( obj, node );

	addressbook_list_select_clear();
	
	return FALSE;
}

/**
 * Setup address list popup menu items. Items are enabled or disabled as
 * required.
 */
static void addressbook_list_menu_setup( void ) {
	GtkCTree *clist = NULL;
	AddressObject *pobj = NULL;
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	gboolean canEdit = FALSE;
	gboolean canDelete = FALSE;
	gboolean canCut = FALSE;
	gboolean canCopy = FALSE;
	gboolean canPaste = FALSE;
	gboolean canBrowse = FALSE;

	pobj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), addrbook.treeSelected );
	if( pobj == NULL ) return;

	clist = GTK_CTREE(addrbook.clist);
	obj = gtk_ctree_node_get_row_data( clist, addrbook.listSelected );
	if( obj == NULL ) canEdit = FALSE;

	menu_set_insensitive_all( GTK_MENU_SHELL(addrbook.list_popup) );

	if( pobj->type == ADDR_DATASOURCE ) {
		/* Parent object is a data source */
		ads = ADAPTER_DSOURCE(pobj);
		ds = ads->dataSource;
		iface = ds->interface;
		if( ! iface->readOnly ) {
			menu_set_sensitive( addrbook.list_factory, "/New Address", TRUE );
			menu_set_sensitive( addrbook.list_factory, "/New Folder", TRUE );
			menu_set_sensitive( addrbook.list_factory, "/New Group", TRUE );
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
			if( ! addrclip_is_empty( _clipBoard_ ) ) canPaste = TRUE;
			if( ! addrselect_test_empty( _addressSelect_ ) ) canCut = TRUE;
			if( obj ) canEdit = TRUE;
		}
	}
	else if( pobj->type != ADDR_INTERFACE ) {
		/* Parent object is not an interface */
		ds = addressbook_find_datasource( addrbook.treeSelected );
		iface = ds->interface;
		if( ! iface->readOnly ) {
			/* Folder or group */
			if( pobj->type == ADDR_ITEM_FOLDER || pobj->type == ADDR_ITEM_GROUP ) {
				menu_set_sensitive( addrbook.list_factory, "/New Address", TRUE );
				gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
				if( obj ) canEdit = TRUE;
			}
			/* Folder */
			if( pobj->type == ADDR_ITEM_FOLDER ) {
				menu_set_sensitive( addrbook.list_factory, "/New Folder", TRUE );
				menu_set_sensitive( addrbook.list_factory, "/New Group", TRUE );
				if( obj ) canEdit = TRUE;
			}
			if( ! addrclip_is_empty( _clipBoard_ ) ) canPaste = TRUE;
			if( ! addrselect_test_empty( _addressSelect_ ) ) canCut = TRUE;
		}
		if( iface->type == ADDR_IF_LDAP ) {
			if( obj ) canBrowse = TRUE;
		}
	}
	if( ! addrselect_test_empty( _addressSelect_ ) ) canCopy = TRUE;

	canDelete = canEdit;

	/* Disable edit or browse if more than one row selected */
	if( GTK_CLIST(clist)->selection && GTK_CLIST(clist)->selection->next ) {
		canEdit = FALSE;
		canBrowse = FALSE;
	}

	/* Now go finalize menu items */
	menu_set_sensitive( addrbook.list_factory, "/Edit",   canEdit );
	menu_set_sensitive( addrbook.list_factory, "/Delete", canDelete );

	menu_set_sensitive( addrbook.list_factory, "/Cut",           canCut );
	menu_set_sensitive( addrbook.list_factory, "/Copy",          canCopy );
	menu_set_sensitive( addrbook.list_factory, "/Paste",         canPaste );
	menu_set_sensitive( addrbook.list_factory, "/Paste Address", canPaste );

	menu_set_sensitive( addrbook.list_factory, "/Mail To",       canCopy );

	menu_set_sensitive( addrbook.menu_factory, "/Edit/Cut",           canCut );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Copy",          canCopy );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste",         canPaste );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste Address", canPaste );

	menu_set_sensitive( addrbook.tree_factory, "/Cut",             canCut );
	menu_set_sensitive( addrbook.tree_factory, "/Copy",            canCopy );
	menu_set_sensitive( addrbook.tree_factory, "/Paste",           canPaste );

	menu_set_sensitive( addrbook.menu_factory, "/Address/Edit",    canEdit );
	menu_set_sensitive( addrbook.menu_factory, "/Address/Delete",  canDelete );
	menu_set_sensitive( addrbook.menu_factory, "/Address/Mail To", canCopy );

	gtk_widget_set_sensitive( addrbook.del_btn, canDelete );

#ifdef USE_LDAP
	menu_set_sensitive( addrbook.list_factory, "/Browse Entry",    canBrowse );
#endif
}

static void addressbook_select_row_tree	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 gint		 column,
					 gpointer	 data)
{
}

/**
 * Add list of items into tree node below specified tree node.
 * \param treeNode  Tree node.
 * \param ds        Data source.
 * \param listItems List of items.
 */
static void addressbook_treenode_add_list(
	GtkCTreeNode *treeNode, AddressDataSource *ds, GList *listItems )
{
	GList *node;

	node = listItems;
	while( node ) {
		AddrItemObject *aio;
		GtkCTreeNode *nn;

		aio = node->data;
		if( ADDRESS_OBJECT_TYPE(aio) == ITEMTYPE_GROUP ) {
			ItemGroup *group;

			group = ( ItemGroup * ) aio;
			nn = addressbook_node_add_group( treeNode, ds, group );
		}
		else if( ADDRESS_OBJECT_TYPE(aio) == ITEMTYPE_FOLDER ) {
			ItemFolder *folder;

			folder = ( ItemFolder * ) aio;
			nn = addressbook_node_add_folder(
				treeNode, ds, folder, ADDR_ITEM_FOLDER );
		}
		node = g_list_next( node );
	}
}

/**
 * Cut from address list widget.
 */
static void addressbook_clip_cut_cb( void ) {
	_clipBoard_->cutFlag = TRUE;
	addrclip_clear( _clipBoard_ );
	addrclip_add( _clipBoard_, _addressSelect_ );
	/* addrclip_list_show( _clipBoard_, stdout ); */
}

/**
 * Copy from address list widget.
 */
static void addressbook_clip_copy_cb( void ) {
	_clipBoard_->cutFlag = FALSE;
	addrclip_clear( _clipBoard_ );
	addrclip_add( _clipBoard_, _addressSelect_ );
	/* addrclip_list_show( _clipBoard_, stdout ); */
}

/**
 * Paste clipboard into address list widget.
 */
static void addressbook_clip_paste_cb( void ) {
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	AddressObject *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *folder = NULL;
	GList *folderGroup = NULL;

	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;
	if( addrindex_ds_get_readonly( ds ) ) {
		addressbook_ds_status_message(
			ds, _( "Cannot paste. Target address book is readonly." ) );
		return;
	}

	pobj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( pobj ) {
		if( pobj->type == ADDR_ITEM_FOLDER ) {
			folder = ADAPTER_FOLDER(pobj)->itemFolder;
		}
		else if( pobj->type == ADDR_ITEM_GROUP ) {
			addressbook_ds_status_message(
				ds, _( "Cannot paste into an address group." ) );
			return;
		}
	}

	/* Get an address book */
	abf = addressbook_get_book_file();
	if( abf == NULL ) return;

	if( _clipBoard_->cutFlag ) {
		/* Paste/Cut */
		folderGroup = addrclip_paste_cut( _clipBoard_, abf, folder );

		/* Remove all groups and folders in clipboard from tree node */
		addressbook_treenode_remove_item();

		/* Remove all "cut" items */
		addrclip_delete_item( _clipBoard_ );

		/* Clear clipboard - cut items??? */
		addrclip_clear( _clipBoard_ );
	}
	else {
		/* Paste/Copy */
		folderGroup = addrclip_paste_copy( _clipBoard_, abf, folder );
	}

	/* addrclip_list_show( _clipBoard_, stdout ); */
	if( folderGroup ) {
		/* Update tree by inserting node for each folder or group */
		addressbook_treenode_add_list(
			addrbook.treeSelected, ds, folderGroup );
		gtk_ctree_expand( ctree, addrbook.treeSelected );
		g_list_free( folderGroup );
		folderGroup = NULL;
	}

	/* Display items pasted */
	gtk_ctree_select( ctree, addrbook.opened );

}

/**
 * Paste clipboard email addresses only into address list widget.
 */
static void addressbook_clip_paste_address_cb( void ) {
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	GtkCTree *ctree;
	AddressObject *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *folder = NULL;
	AddrItemObject *aio;
	gint cnt;

	if( addrbook.listSelected == NULL ) return;

       	ctree = GTK_CTREE( addrbook.ctree );
	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;
	if( addrindex_ds_get_readonly( ds ) ) {
		addressbook_ds_status_message(
			ds, _( "Cannot paste. Target address book is readonly." ) );
		return;
	}

	pobj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( pobj ) {
		if( pobj->type == ADDR_ITEM_FOLDER ) {
			folder = ADAPTER_FOLDER(pobj)->itemFolder;
		}
	}

	abf = addressbook_get_book_file();
	if( abf == NULL ) return;

	cnt = 0;
	aio = gtk_ctree_node_get_row_data( clist, addrbook.listSelected );
	if( aio->type == ADDR_ITEM_PERSON ) {
		ItemPerson *person;

		person = ( ItemPerson * ) aio;
		if( _clipBoard_->cutFlag ) {
			/* Paste/Cut */
			cnt = addrclip_paste_person_cut( _clipBoard_, abf, person );

			/* Remove all "cut" items */
			addrclip_delete_address( _clipBoard_ );

			/* Clear clipboard */
			addrclip_clear( _clipBoard_ );
		}
		else {
			/* Paste/Copy */
			cnt = addrclip_paste_person_copy( _clipBoard_, abf, person );
		}
		if( cnt > 0 ) {
			addritem_person_set_opened( person, TRUE );
		}
	}

	/* Display items pasted */
	if( cnt > 0 ) {
		gtk_ctree_select( ctree, addrbook.opened );
	}
}

/**
 * Add current treenode object to clipboard. Note that widget only allows
 * one entry from the tree list to be selected.
 */
static void addressbook_treenode_to_clipboard( void ) {
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddrSelectItem *item;
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	GtkCTreeNode *node;

	node = addrbook.treeSelected;
	if( node == NULL ) return;
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;

	item = NULL;
	if( obj->type == ADDR_ITEM_FOLDER ) {
		AdapterFolder *adapter = ADAPTER_FOLDER(obj);
		ItemFolder *folder = adapter->itemFolder;

		item = addrselect_create_node( obj );
		item->uid = g_strdup( ADDRITEM_ID(folder) );
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		AdapterGroup *adapter = ADAPTER_GROUP(obj);
		ItemGroup *group = adapter->itemGroup;

		item = addrselect_create_node( obj );
		item->uid = g_strdup( ADDRITEM_ID(group) );
	}
	else if( obj->type == ADDR_DATASOURCE ) {
		/* Data source */
		item = addrselect_create_node( obj );
		item->uid = NULL;
	}

	if( item ) {
		/* Clear existing list and add item into list */
		gchar *cacheID;

		addressbook_list_select_clear();
		cacheID = addrindex_get_cache_id( _addressIndex_, ds );
		addrselect_list_add( _addressSelect_, item, cacheID );
		g_free( cacheID );
	}
}

/**
 * Cut from tree widget.
 */
static void addressbook_treenode_cut_cb( void ) {
	_clipBoard_->cutFlag = TRUE;
	addressbook_treenode_to_clipboard();
	addrclip_clear( _clipBoard_ );
	addrclip_add( _clipBoard_, _addressSelect_ );
	/* addrclip_list_show( _clipBoard_, stdout ); */
}

/**
 * Copy from tree widget.
 */
static void addressbook_treenode_copy_cb( void ) {
	_clipBoard_->cutFlag = FALSE;
	addressbook_treenode_to_clipboard();
	addrclip_clear( _clipBoard_ );
	addrclip_add( _clipBoard_, _addressSelect_ );
	/* addrclip_list_show( _clipBoard_, stdout ); */
}

/**
 * Paste clipboard into address tree widget.
 */
static void addressbook_treenode_paste_cb( void ) {
	addressbook_clip_paste_cb();
}

/**
 * Clear selected entries in clipboard.
 */
static void addressbook_list_select_clear( void ) {
	addrselect_list_clear( _addressSelect_ );
}

/**
 * Add specified address item to selected address list.
 * \param aio Address item object.
 * \param ds  Datasource.
 */
static void addressbook_list_select_add( AddrItemObject *aio, AddressDataSource *ds ) {
	gchar *cacheID;

	if( ds == NULL ) return;
	cacheID = addrindex_get_cache_id( _addressIndex_, ds );
	addrselect_list_add_obj( _addressSelect_, aio, cacheID );
	g_free( cacheID );
}

/**
 * Remove specified address item from selected address list.
 * \param aio Address item object.
 */
static void addressbook_list_select_remove( AddrItemObject *aio ) {
	addrselect_list_remove( _addressSelect_, aio );
}

/**
 * Invoke EMail compose window with addresses in selected address list.
 */
static void addressbook_mail_to_cb( void ) {
	GList *listAddress;

	if( ! addrselect_test_empty( _addressSelect_ ) ) {
		listAddress = addrselect_build_list( _addressSelect_ );
		compose_new_with_list( NULL, listAddress );
		mgu_free_dlist( listAddress );
		listAddress = NULL;
	}
}

static void addressbook_list_row_selected( GtkCTree *clist,
					   GtkCTreeNode *node,
					   gint column,
					   gpointer data )
{
	GtkEntry *entry = GTK_ENTRY(addrbook.entry);
	AddrItemObject *aio = NULL;
	AddressObject *pobj = NULL;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;

	gtk_entry_set_text( entry, "" );
	addrbook.listSelected = node;

	pobj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), addrbook.treeSelected );
	if( pobj == NULL ) return;

	if( pobj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(pobj);
		ds = ads->dataSource;
	}
	else if( pobj->type != ADDR_INTERFACE ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
	}

	aio = gtk_ctree_node_get_row_data( clist, node );
	if( aio ) {
		/* printf( "list select: %d : '%s'\n", aio->type, aio->name ); */
		addressbook_list_select_add( aio, ds );
	}

	addressbook_list_menu_setup();
}

static void addressbook_list_row_unselected( GtkCTree *ctree,
					     GtkCTreeNode *node,
					     gint column,
					     gpointer data )
{
	AddrItemObject *aio;

	aio = gtk_ctree_node_get_row_data( ctree, node );
	if( aio != NULL ) {
		/* printf( "list unselect: %d : '%s'\n", aio->type, aio->name ); */
		addressbook_list_select_remove( aio );
	}
}

static void addressbook_entry_gotfocus( GtkWidget *widget ) {
	gtk_editable_select_region( GTK_EDITABLE(addrbook.entry), 0, -1 );
}

/* from gdkevents.c */
#define DOUBLE_CLICK_TIME 250

static gboolean addressbook_list_button_pressed(GtkWidget *widget,
						GdkEventButton *event,
						gpointer data)
{
	static guint32 lasttime = 0;
	if( ! event ) return FALSE;

	addressbook_list_menu_setup();

	if( event->button == 3 ) {
		gtk_menu_popup( GTK_MENU(addrbook.list_popup), NULL, NULL, NULL, NULL,
		       event->button, event->time );
	} else if (event->button == 1) {
		if (event->time - lasttime < DOUBLE_CLICK_TIME) {
			if (prefs_common.add_address_by_click &&
			    addrbook.target_compose)
				addressbook_to_clicked(NULL, GINT_TO_POINTER(COMPOSE_TO));
			else
				addressbook_edit_address_cb(NULL, 0, NULL);

			lasttime = 0;
		} else
			lasttime = event->time;
	}

	return FALSE;
}

static gboolean addressbook_list_button_released(GtkWidget *widget,
						 GdkEventButton *event,
						 gpointer data)
{
	return FALSE;
}

static gboolean addressbook_tree_button_pressed(GtkWidget *ctree,
						GdkEventButton *event,
						gpointer data)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row, column;
	AddressObject *obj = NULL;
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	gboolean canEdit = FALSE;
	gboolean canDelete = FALSE;
	gboolean canCut = FALSE;
	gboolean canCopy = FALSE;
	gboolean canPaste = FALSE;
	gboolean canTreeCut = FALSE;
	gboolean canTreeCopy = FALSE;
	gboolean canTreePaste = FALSE;
	gboolean canLookup = FALSE;

	if( ! event ) return FALSE;
	addressbook_menubar_set_sensitive( FALSE );

	if( gtk_clist_get_selection_info( clist, event->x, event->y, &row, &column ) ) {
		gtk_clist_select_row( clist, row, column );
		gtkut_clist_set_focus_row(clist, row);
		obj = gtk_clist_get_row_data( clist, row );
	}

	menu_set_insensitive_all(GTK_MENU_SHELL(addrbook.tree_popup));
	gtk_widget_set_sensitive( addrbook.lup_btn, FALSE );

	if( obj == NULL ) return FALSE;

	if( ! addrclip_is_empty( _clipBoard_ ) ) {
		canTreePaste = TRUE;
	}

	if (obj->type == ADDR_DATASOURCE) {
		ads = ADAPTER_DSOURCE(obj);
		ds = ads->dataSource;
		iface = ds->interface;
		canEdit = TRUE;
		canDelete = TRUE;
		if( iface->readOnly ) {
			canTreePaste = FALSE;
		}
		else {
			menu_set_sensitive( addrbook.tree_factory, "/New Address", TRUE );
			menu_set_sensitive( addrbook.tree_factory, "/New Folder", TRUE );
			menu_set_sensitive( addrbook.tree_factory, "/New Group", TRUE );
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
		}
		canTreeCopy = TRUE;
		if( iface->externalQuery ) canLookup = TRUE;
	}
	else if (obj->type == ADDR_ITEM_FOLDER) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		iface = ds->interface;
		if( iface->readOnly ) {
			canTreePaste = FALSE;
		}
		else {
			canEdit = TRUE;
			canDelete = TRUE;
			canTreeCut = TRUE;
			menu_set_sensitive( addrbook.tree_factory, "/New Address", TRUE );
			menu_set_sensitive( addrbook.tree_factory, "/New Folder", TRUE );
			menu_set_sensitive( addrbook.tree_factory, "/New Group", TRUE );
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
		}
		canTreeCopy = TRUE;
		iface = ds->interface;
		if( iface->externalQuery ) {
			/* Enable deletion of LDAP folder */
			canLookup = TRUE;
			canDelete = TRUE;
		}
	}
	else if (obj->type == ADDR_ITEM_GROUP) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		iface = ds->interface;
		if( ! iface->readOnly ) {
			canEdit = TRUE;
			canDelete = TRUE;
			menu_set_sensitive( addrbook.tree_factory, "/New Address", TRUE );
			gtk_widget_set_sensitive( addrbook.reg_btn, TRUE );
		}
	}
	else if (obj->type == ADDR_INTERFACE) {
		canTreePaste = FALSE;
	}

	if( canEdit ) {
		if( ! addrselect_test_empty( _addressSelect_ ) ) canCut = TRUE;
	}
	if( ! addrselect_test_empty( _addressSelect_ ) ) canCopy = TRUE;
	if( ! addrclip_is_empty( _clipBoard_ ) ) canPaste = TRUE;

	/* Enable edit */
	menu_set_sensitive( addrbook.tree_factory, "/Edit",   canEdit );
	menu_set_sensitive( addrbook.tree_factory, "/Delete", canDelete );
	menu_set_sensitive( addrbook.tree_factory, "/Cut",    canTreeCut );
	menu_set_sensitive( addrbook.tree_factory, "/Copy",   canTreeCopy );
	menu_set_sensitive( addrbook.tree_factory, "/Paste",  canTreePaste );

	menu_set_sensitive( addrbook.menu_factory, "/File/Edit",          canEdit );
	menu_set_sensitive( addrbook.menu_factory, "/File/Delete",        canEdit );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Cut",           canCut );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Copy",          canCopy );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste",         canPaste );
	menu_set_sensitive( addrbook.menu_factory, "/Edit/Paste Address", canPaste );

	gtk_widget_set_sensitive( addrbook.lup_btn, canLookup );

	if( event->button == 3 ) {
		gtk_menu_popup(GTK_MENU(addrbook.tree_popup), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}

	return FALSE;
}

static gboolean addressbook_tree_button_released(GtkWidget *ctree,
						 GdkEventButton *event,
						 gpointer data)
{
	gtk_ctree_select(GTK_CTREE(addrbook.ctree), addrbook.opened);
	gtkut_ctree_set_focus_row(GTK_CTREE(addrbook.ctree), addrbook.opened);
	return FALSE;
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
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemFolder *folder = NULL;

	if( ! addrbook.treeSelected ) return;
	obj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( obj == NULL ) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK ) return;
	}
	else if( obj->type == ADDR_ITEM_FOLDER ) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	}
	else {
		return;
	}

	abf = ds->rawDataSource;
	if( abf == NULL ) return;
	folder = addressbook_edit_folder( abf, parentFolder, NULL );
	if( folder ) {
		GtkCTreeNode *nn;
		nn = addressbook_node_add_folder(
			addrbook.treeSelected, ds, folder, ADDR_ITEM_FOLDER );
		gtk_ctree_expand( ctree, addrbook.treeSelected );
		if( addrbook.treeSelected == addrbook.opened )
			addressbook_set_clist(obj);
	}

}

static void addressbook_new_group_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *parentFolder = NULL;
	ItemGroup *group = NULL;

	if( ! addrbook.treeSelected ) return;
	obj = gtk_ctree_node_get_row_data(ctree, addrbook.treeSelected);
	if( obj == NULL ) return;
	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(obj)->subType != ADDR_BOOK ) return;
	}
	else if( obj->type == ADDR_ITEM_FOLDER ) {
		parentFolder = ADAPTER_FOLDER(obj)->itemFolder;
	}
	else {
		return;
	}

	abf = ds->rawDataSource;
	if( abf == NULL ) return;
	group = addressbook_edit_group( abf, parentFolder, NULL );
	if( group ) {
		GtkCTreeNode *nn;
		nn = addressbook_node_add_group( addrbook.treeSelected, ds, group );
		gtk_ctree_expand( ctree, addrbook.treeSelected );
		if( addrbook.treeSelected == addrbook.opened )
			addressbook_set_clist(obj);
	}

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

/**
 * Edit data source.
 * \param obj  Address object to edit.
 * \param node Node in tree.
 * \return New name of data source.
 */
static gchar *addressbook_edit_datasource( AddressObject *obj, GtkCTreeNode *node ) {
	gchar *newName = NULL;
	AddressDataSource *ds = NULL;
	AddressInterface *iface = NULL;
	AdapterDSource *ads = NULL;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return NULL;
	iface = ds->interface;
	if( ! iface->haveLibrary ) return NULL;

	/* Read data from data source */
	if( addrindex_ds_get_modify_flag( ds ) ) {
		addrindex_ds_read_data( ds );
	}

	if( ! addrindex_ds_get_read_flag( ds ) ) {
		addrindex_ds_read_data( ds );
	}

	/* Handle edit */
	ads = ADAPTER_DSOURCE(obj);
	if( ads->subType == ADDR_BOOK ) {
                if( addressbook_edit_book( _addressIndex_, ads ) == NULL ) return NULL;
	}
	else if( ads->subType == ADDR_VCARD ) {
       	        if( addressbook_edit_vcard( _addressIndex_, ads ) == NULL ) return NULL;
	}
#ifdef USE_JPILOT
	else if( ads->subType == ADDR_JPILOT ) {
                if( addressbook_edit_jpilot( _addressIndex_, ads ) == NULL ) return NULL;
	}
#endif
#ifdef USE_LDAP
	else if( ads->subType == ADDR_LDAP ) {
		if( addressbook_edit_ldap( _addressIndex_, ads ) == NULL ) return NULL;
	}
#endif
	else {
		return NULL;
	}
	newName = obj->name;
	return newName;
}

/*
* Edit an object that is in the address tree area.
*/
static void addressbook_treenode_edit_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;
	GtkCTreeNode *node = NULL, *parentNode = NULL;
	gchar *name = NULL;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;
	parentNode = GTK_CTREE_ROW(node)->parent;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;

	if( obj->type == ADDR_DATASOURCE ) {
		name = addressbook_edit_datasource( obj, node );
		if( name == NULL ) return;
	}
	else {
		abf = ds->rawDataSource;
		if( abf == NULL ) return;
		if( obj->type == ADDR_ITEM_FOLDER ) {
			AdapterFolder *adapter = ADAPTER_FOLDER(obj);
			ItemFolder *item = adapter->itemFolder;
			ItemFolder *parentFolder = NULL;
			parentFolder = ( ItemFolder * ) ADDRITEM_PARENT(item);
			if( addressbook_edit_folder( abf, parentFolder, item ) == NULL ) return;
			name = ADDRITEM_NAME(item);
		}
		else if( obj->type == ADDR_ITEM_GROUP ) {
			AdapterGroup *adapter = ADAPTER_GROUP(obj);
			ItemGroup *item = adapter->itemGroup;
			ItemFolder *parentFolder = NULL;
			parentFolder = ( ItemFolder * ) ADDRITEM_PARENT(item);
			if( addressbook_edit_group( abf, parentFolder, item ) == NULL ) return;
			name = ADDRITEM_NAME(item);
		}
	}
	if( name && parentNode ) {
		/* Update node in tree view */
		addressbook_change_node_name( node, name );
		gtk_sctree_sort_node(ctree, parentNode);
		gtk_ctree_expand( ctree, node );
		gtk_ctree_select( ctree, node );
	}
}

typedef enum {
	ADDRTREE_DEL_NONE,
	ADDRTREE_DEL_DATA,
	ADDRTREE_DEL_FOLDER_ONLY,
	ADDRTREE_DEL_FOLDER_ADDR
} TreeItemDelType ;

/**
 * Delete an item from the tree widget.
 * \param data   Data passed in.
 * \param action Action.
 * \param widget Widget issuing callback.
 */
static void addressbook_treenode_delete_cb(
		gpointer data, guint action, GtkWidget *widget )
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *node = NULL;
	AddressObject *obj;
	gchar *message;
	AlertValue aval;
	AddrBookBase *adbase;
	AddressCache *cache;
	AdapterDSource *ads = NULL;
	AddressInterface *iface = NULL;
	AddressDataSource *ds = NULL;
	gboolean remFlag = FALSE;
	TreeItemDelType delType;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;

	obj = gtk_ctree_node_get_row_data( ctree, node );
	g_return_if_fail(obj != NULL);

	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
		if( ads == NULL ) return;
		ds = ads->dataSource;
		if( ds == NULL ) return;
	}
	else {
		/* Must be folder or something else */
		ds = addressbook_find_datasource( node );
		if( ds == NULL ) return;

		/* Only allow deletion from non-readOnly */
		iface = ds->interface;
		if( iface->readOnly ) {
			/* Allow deletion of query results */
			if( ! iface->externalQuery ) return;
		}
	}

	/* Confirm deletion */
	delType = ADDRTREE_DEL_NONE;
	if( obj->type == ADDR_ITEM_FOLDER ) {
		if( iface->externalQuery ) {
			message = g_strdup_printf( _(
				"Do you want to delete the query " \
				"results and addresses in `%s' ?" ),
				obj->name );
			aval = alertpanel( _("Delete"), message,
				GTK_STOCK_YES, GTK_STOCK_NO, NULL );
			g_free(message);
			if( aval == G_ALERTDEFAULT ) {
				delType = ADDRTREE_DEL_FOLDER_ADDR;
			}
		}
		else {
			message = g_strdup_printf
				( _( "Do you want to delete the folder AND all addresses in `%s' ?\n"
			    	     "If deleting the folder only, addresses will be moved into parent folder." ),
			 	 obj->name );
			aval = alertpanel( _("Delete folder"), message,
				_("_Folder only"), _("Folder and _addresses"),
				GTK_STOCK_CANCEL );
			g_free(message);
			if( aval == G_ALERTDEFAULT ) {
				delType = ADDRTREE_DEL_FOLDER_ONLY;
			}
			else if( aval == G_ALERTALTERNATE ) {
				delType = ADDRTREE_DEL_FOLDER_ADDR;
			}
		}
	}
	else {
		message = g_strdup_printf(_("Really delete `%s' ?"), obj->name);
		aval = alertpanel(_("Delete"), message, GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		g_free(message);
		if( aval == G_ALERTDEFAULT ) delType = ADDRTREE_DEL_DATA;
	}
	if( delType == ADDRTREE_DEL_NONE ) return;

	/* Proceed with deletion */
	if( obj->type == ADDR_DATASOURCE ) {
		/* Remove node from tree */
		gtk_ctree_remove_node( ctree, node );
	
		/* Remove data source. */
		if( addrindex_index_remove_datasource( _addressIndex_, ds ) ) {
			addrindex_free_datasource( ds );
		}
		return;
	}

	/* Get reference to cache */
	adbase = ( AddrBookBase * ) ds->rawDataSource;
	if( adbase == NULL ) return;
	cache = adbase->addressCache;

	/* Remove query results folder */
	if( iface->externalQuery ) {
		AdapterFolder *adapter = ADAPTER_FOLDER(obj);
		ItemFolder *folder = adapter->itemFolder;

		adapter->itemFolder = NULL;
		/*
		printf( "remove folder for ::%s::\n", obj->name );
		printf( "      folder name ::%s::\n", ADDRITEM_NAME(folder) );
		printf( "-------------- remove results\n" );
		*/
		addrindex_remove_results( ds, folder );
		/* printf( "-------------- remove node\n" ); */
		gtk_ctree_remove_node( ctree, node );
		return;
	}

	/* Code below is valid for regular address book deletion */
	if( obj->type == ADDR_ITEM_FOLDER ) {
		AdapterFolder *adapter = ADAPTER_FOLDER(obj);
		ItemFolder *item = adapter->itemFolder;

		if( delType == ADDRTREE_DEL_FOLDER_ONLY ) {
			/* Remove folder only */
			item = addrcache_remove_folder( cache, item );
			if( item ) {
				addritem_free_item_folder( item );
				addressbook_move_nodes_up( ctree, node );
				remFlag = TRUE;
			}
		}
		else if( delType == ADDRTREE_DEL_FOLDER_ADDR ) {
			/* Remove folder and addresses */
			item = addrcache_remove_folder_delete( cache, item );
			if( item ) {
				addritem_free_item_folder( item );
				remFlag = TRUE;
			}
		}
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		AdapterGroup *adapter = ADAPTER_GROUP(obj);
		ItemGroup *item = adapter->itemGroup;

		item = addrcache_remove_group( cache, item );
		if( item ) {
			addritem_free_item_group( item );
			remFlag = TRUE;
		}
	}

	if( remFlag ) {
		/* Remove node. */
		gtk_ctree_remove_node(ctree, node );
	}
}

static void addressbook_new_address_cb( gpointer data, guint action, GtkWidget *widget ) {
	AddressObject *pobj = NULL;
	AddressDataSource *ds = NULL;
	AddressBookFile *abf = NULL;

	pobj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), addrbook.treeSelected);
	if( pobj == NULL ) return;
	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;

	abf = ds->rawDataSource;
	if( abf == NULL ) return;

	if( pobj->type == ADDR_DATASOURCE ) {
		if( ADAPTER_DSOURCE(pobj)->subType == ADDR_BOOK ) {
			/* New address */
			ItemPerson *person = addressbook_edit_person( abf, NULL, NULL, FALSE );
			if( person && addrbook.treeSelected == addrbook.opened ) {
				gtk_clist_unselect_all( GTK_CLIST(addrbook.clist) );
				addressbook_folder_refresh_one_person(
					GTK_CTREE(addrbook.clist), person );
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_FOLDER ) {
		/* New address */
		ItemFolder *folder = ADAPTER_FOLDER(pobj)->itemFolder;
		ItemPerson *person = addressbook_edit_person( abf, folder, NULL, FALSE );
		if( person ) {
			if (addrbook.treeSelected == addrbook.opened) {
				gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
			}
		}
	}
	else if( pobj->type == ADDR_ITEM_GROUP ) {
		/* New address in group */
		ItemGroup *group = ADAPTER_GROUP(pobj)->itemGroup;
		if( addressbook_edit_group( abf, NULL, group ) == NULL ) return;
		if (addrbook.treeSelected == addrbook.opened) {
			/* Change node name in tree. */
			addressbook_change_node_name( addrbook.treeSelected, ADDRITEM_NAME(group) );
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened );
		}
	}
}

/**
 * Search for specified child group node in address index tree.
 * \param parent Parent node.
 * \param group  Group to find.
 */
static GtkCTreeNode *addressbook_find_group_node( GtkCTreeNode *parent, ItemGroup *group ) {
	GtkCTreeNode *node = NULL;
	GtkCTreeRow *currRow;

	currRow = GTK_CTREE_ROW( parent );
	if( currRow ) {
		node = currRow->children;
		while( node ) {
			AddressObject *obj;

			obj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), node );
			if( obj->type == ADDR_ITEM_GROUP ) {
				ItemGroup *g = ADAPTER_GROUP(obj)->itemGroup;
				if( g == group ) return node;
			}
			currRow = GTK_CTREE_ROW(node);
			node = currRow->sibling;
		}
	}
	return NULL;
}

static AddressBookFile *addressbook_get_book_file() {
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	ds = addressbook_find_datasource( addrbook.treeSelected );
	if( ds == NULL ) return NULL;
	if( ds->type == ADDR_IF_BOOK ) abf = ds->rawDataSource;
	return abf;
}

static AddressBookFile *addressbook_get_book_file_for_node(GtkCTreeNode *node) {
	AddressBookFile *abf = NULL;
	AddressDataSource *ds = NULL;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return NULL;
	if( ds->type == ADDR_IF_BOOK ) abf = ds->rawDataSource;
	return abf;
}

static void addressbook_tree_remove_children( GtkCTree *ctree, GtkCTreeNode *parent ) {
	GtkCTreeNode *node;
	GtkCTreeRow *row;

	/* Remove existing folders and groups */
	row = GTK_CTREE_ROW( parent );
	if( row ) {
		while( (node = row->children) ) {
			gtk_ctree_remove_node( ctree, node );
		}
	}
}

static void addressbook_move_nodes_up( GtkCTree *ctree, GtkCTreeNode *node ) {
	GtkCTreeNode *parent, *child;
	GtkCTreeRow *currRow;
	currRow = GTK_CTREE_ROW( node );
	if( currRow ) {
		parent = currRow->parent;
		while( (child = currRow->children) ) {
			gtk_ctree_move( ctree, child, parent, node );
		}
		gtk_sctree_sort_node( ctree, parent );
	}
}

static void addressbook_edit_address_cb( gpointer data, guint action, GtkWidget *widget ) {
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	GtkCTree *ctree;
	AddressObject *obj = NULL, *pobj = NULL;
	AddressDataSource *ds = NULL;
	GtkCTreeNode *node = NULL, *parentNode = NULL;
	gchar *name = NULL;
	AddressBookFile *abf = NULL;

	if( addrbook.listSelected == NULL ) return;
	obj = gtk_ctree_node_get_row_data( clist, addrbook.listSelected );
	g_return_if_fail(obj != NULL);

       	ctree = GTK_CTREE( addrbook.ctree );
	pobj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	node = gtk_ctree_find_by_row_data( ctree, addrbook.treeSelected, obj );

	ds = addressbook_find_datasource( GTK_CTREE_NODE(addrbook.treeSelected) );
	if( ds == NULL ) return;

	abf = addressbook_get_book_file();
	if( abf == NULL ) return;
	if( obj->type == ADDR_ITEM_EMAIL ) {
		ItemEMail *email = ( ItemEMail * ) obj;
		if( email == NULL ) return;
		if( pobj && pobj->type == ADDR_ITEM_GROUP ) {
			/* Edit parent group */
			AdapterGroup *adapter = ADAPTER_GROUP(pobj);
			ItemGroup *itemGrp = adapter->itemGroup;
			if( addressbook_edit_group( abf, NULL, itemGrp ) == NULL ) return;
			name = ADDRITEM_NAME(itemGrp);
			node = addrbook.treeSelected;
			parentNode = GTK_CTREE_ROW(node)->parent;
		}
		else {
			/* Edit person - email page */
			ItemPerson *person;
			person = ( ItemPerson * ) ADDRITEM_PARENT(email);
			if( addressbook_edit_person( abf, NULL, person, TRUE ) == NULL ) return;
			addressbook_folder_refresh_one_person( clist, person );
			invalidate_address_completion();
			return;
		}
	}
	else if( obj->type == ADDR_ITEM_PERSON ) {
		/* Edit person - basic page */
		ItemPerson *person = ( ItemPerson * ) obj;
		if( addressbook_edit_person( abf, NULL, person, FALSE ) == NULL ) return;
		invalidate_address_completion();
		addressbook_folder_refresh_one_person( clist, person );
		return;
	}
	else if( obj->type == ADDR_ITEM_GROUP ) {
		ItemGroup *itemGrp = ( ItemGroup * ) obj;
		if( addressbook_edit_group( abf, NULL, itemGrp ) == NULL ) return;
		parentNode = addrbook.treeSelected;
		node = addressbook_find_group_node( parentNode, itemGrp );
		name = ADDRITEM_NAME(itemGrp);
	}
	else {
		return;
	}

	/* Update tree node with node name */
	if( node == NULL ) return;
	addressbook_change_node_name( node, name );
	gtk_sctree_sort_node( ctree, parentNode );
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

static void addressbook_file_save_cb( gpointer data, guint action, GtkWidget *widget ) {
	addressbook_export_to_file();
}

static void addressbook_person_expand_node( GtkCTree *ctree, GList *node, gpointer *data ) {
	if( node ) {
		ItemPerson *person = gtk_ctree_node_get_row_data( ctree, GTK_CTREE_NODE(node) );
		if( person ) addritem_person_set_opened( person, TRUE );
	}
}

static void addressbook_person_collapse_node( GtkCTree *ctree, GList *node, gpointer *data ) {
	if( node ) {
		ItemPerson *person = gtk_ctree_node_get_row_data( ctree, GTK_CTREE_NODE(node) );
		if( person ) addritem_person_set_opened( person, FALSE );
	}
}

static gchar *addressbook_format_item_clist( ItemPerson *person, ItemEMail *email ) {
	gchar *str = NULL;
	gchar *eMailAlias = ADDRITEM_NAME(email);
	if( eMailAlias && *eMailAlias != '\0' ) {
		if( person ) {
			str = g_strdup_printf( "%s - %s", ADDRITEM_NAME(person), eMailAlias );
		}
		else {
			str = g_strdup( eMailAlias );
		}
	}
	return str;
}

static void addressbook_load_group( GtkCTree *clist, ItemGroup *itemGroup ) {
	GList *items = itemGroup->listEMail;
	AddressTypeControlItem *atci = addrbookctl_lookup( ADDR_ITEM_EMAIL );
	for( ; items != NULL; items = g_list_next( items ) ) {
		GtkCTreeNode *nodeEMail = NULL;
		gchar *text[N_COLS];
		ItemEMail *email = items->data;
		ItemPerson *person;
		gchar *str = NULL;

		if( ! email ) continue;

		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		str = addressbook_format_item_clist( person, email );
		if( str ) {
			text[COL_NAME] = str;
		}
		else {
			text[COL_NAME] = ADDRITEM_NAME(person);
		}
		text[COL_ADDRESS] = email->address;
		text[COL_REMARKS] = email->remarks;
		nodeEMail = gtk_ctree_insert_node(
				clist, NULL, NULL,
				text, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm,
				atci->iconXpmOpen, atci->maskXpmOpen,
				FALSE, FALSE );
		gtk_ctree_node_set_row_data( clist, nodeEMail, email );
		g_free( str );
		str = NULL;
	}
}

static void addressbook_folder_load_one_person(
		GtkCTree *clist, ItemPerson *person,
		AddressTypeControlItem *atci,
		AddressTypeControlItem *atciMail )
{
	GtkCTreeNode *nodePerson = NULL;
	GtkCTreeNode *nodeEMail = NULL;
	gchar *text[N_COLS];
	gboolean flgFirst = TRUE, haveAddr = FALSE;
	GList *node;

	if( person == NULL ) return;

	text[COL_NAME] = NULL;
	node = person->listEMail;
	while( node ) {
		ItemEMail *email = node->data;
		gchar *eMailAddr = NULL;
		node = g_list_next( node );

		text[COL_ADDRESS] = email->address;
		text[COL_REMARKS] = email->remarks;
		eMailAddr = ADDRITEM_NAME(email);
		if( eMailAddr && *eMailAddr == '\0' ) eMailAddr = NULL;
		if( flgFirst ) {
			/* First email belongs with person */
			gchar *str = addressbook_format_item_clist( person, email );
			if( str ) {
				text[COL_NAME] = str;
			}
			else {
				text[COL_NAME] = ADDRITEM_NAME(person);
			}
			nodePerson = gtk_ctree_insert_node(
					clist, NULL, NULL,
					text, FOLDER_SPACING,
					atci->iconXpm, atci->maskXpm,
					atci->iconXpmOpen, atci->maskXpmOpen,
					FALSE, person->isOpened );
			g_free( str );
			str = NULL;
			gtk_ctree_node_set_row_data(clist, nodePerson, person );
		}
		else {
			/* Subsequent email is a child node of person */
			text[COL_NAME] = ADDRITEM_NAME(email);
			nodeEMail = gtk_ctree_insert_node(
					clist, nodePerson, NULL,
					text, FOLDER_SPACING,
					atciMail->iconXpm, atciMail->maskXpm,
					atciMail->iconXpmOpen, atciMail->maskXpmOpen,
					FALSE, TRUE );
			gtk_ctree_node_set_row_data(clist, nodeEMail, email );
		}
		flgFirst = FALSE;
		haveAddr = TRUE;
	}
	if( ! haveAddr ) {
		/* Have name without EMail */
		text[COL_NAME] = ADDRITEM_NAME(person);
		text[COL_ADDRESS] = NULL;
		text[COL_REMARKS] = NULL;
		nodePerson = gtk_ctree_insert_node(
				clist, NULL, NULL,
				text, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm,
				atci->iconXpmOpen, atci->maskXpmOpen,
				FALSE, person->isOpened );
		gtk_ctree_node_set_row_data(clist, nodePerson, person );
	}
	return;
}

static void addressbook_folder_load_person( GtkCTree *clist, ItemFolder *itemFolder ) {
	GList *items;
	AddressTypeControlItem *atci = addrbookctl_lookup( ADDR_ITEM_PERSON );
	AddressTypeControlItem *atciMail = addrbookctl_lookup( ADDR_ITEM_EMAIL );

	if( atci == NULL ) return;
	if( atciMail == NULL ) return;

	/* Load email addresses */
	items = addritem_folder_get_person_list( itemFolder );
	for( ; items != NULL; items = g_list_next( items ) ) {
		addressbook_folder_load_one_person( clist, items->data, atci, atciMail );
	}
	/* Free up the list */
	mgu_clear_list( items );
	g_list_free( items );
}

static void addressbook_folder_remove_node( GtkCTree *clist, GtkCTreeNode *node ) { 
	addrbook.listSelected = NULL;
	gtk_ctree_remove_node( clist, node );
	addressbook_menubar_set_sensitive( FALSE );
	addressbook_menuitem_set_sensitive(
		gtk_ctree_node_get_row_data(
			GTK_CTREE(clist), addrbook.treeSelected ),
		addrbook.treeSelected );
}

static void addressbook_folder_refresh_one_person( GtkCTree *clist, ItemPerson *person ) {
	AddressTypeControlItem *atci = addrbookctl_lookup( ADDR_ITEM_PERSON );
	AddressTypeControlItem *atciMail = addrbookctl_lookup( ADDR_ITEM_EMAIL );
	GtkCTreeNode *node;
	if( atci == NULL ) return;
	if( atciMail == NULL ) return;
	if( person == NULL ) return;
	/* unload the person */
	
	node = gtk_ctree_find_by_row_data( clist, NULL, person );
	if( node )
		addressbook_folder_remove_node( clist, node );
	addressbook_folder_load_one_person( clist, person, atci, atciMail );
	node = gtk_ctree_find_by_row_data( clist, NULL, person );
	if( node ) {
		gtk_ctree_select( clist, node );
		if (!gtk_ctree_node_is_visible( clist, node ) ) 
			gtk_ctree_node_moveto( clist, node, 0, 0, 0 );
	}
}

static void addressbook_folder_remove_one_person( GtkCTree *clist, ItemPerson *person ) {
	GtkCTreeNode *node;
	gint row;
	
	if( person == NULL ) return;
	node = gtk_ctree_find_by_row_data( clist, NULL, person );
	row  = gtk_clist_find_row_from_data( GTK_CLIST(clist), person );
	if( node ) {
		addressbook_folder_remove_node( clist, node );
	}
}

static void addressbook_folder_load_group( GtkCTree *clist, ItemFolder *itemFolder ) {
	GList *items;
	AddressTypeControlItem *atci =  addrbookctl_lookup( ADDR_ITEM_GROUP );

	/* Load any groups */
	if( ! atci ) return;
	items = addritem_folder_get_group_list( itemFolder );
	for( ; items != NULL; items = g_list_next( items ) ) {
		GtkCTreeNode *nodeGroup = NULL;
		gchar *text[N_COLS];
		ItemGroup *group = items->data;
		if( group == NULL ) continue;
		text[COL_NAME] = ADDRITEM_NAME(group);
		text[COL_ADDRESS] = NULL;
		text[COL_REMARKS] = NULL;
		nodeGroup = gtk_ctree_insert_node(clist, NULL, NULL,
				      text, FOLDER_SPACING,
				      atci->iconXpm, atci->maskXpm,
				      atci->iconXpmOpen, atci->maskXpmOpen,
				      FALSE, FALSE);
		gtk_ctree_node_set_row_data(clist, nodeGroup, group );
		gtk_sctree_sort_node(clist, NULL);
	}
	/* Free up the list */
	mgu_clear_list( items );
	g_list_free( items );
}

/**
 * Search ctree widget callback function.
 * \param  pA Pointer to node.
 * \param  pB Pointer to data item being sought.
 * \return Zero (0) if group found.
 */
static int addressbook_treenode_find_group_cb( gconstpointer pA, gconstpointer pB ) {
	AddressObject *aoA;

	aoA = ( AddressObject * ) pA;
	if( aoA->type == ADDR_ITEM_GROUP ) {
		ItemGroup *group, *grp;

		grp = ADAPTER_GROUP(aoA)->itemGroup;
		group = ( ItemGroup * ) pB;
		if( grp == group ) return 0;	/* Found group */
	}
	return 1;
}

/**
 * Search ctree widget callback function.
 * \param  pA Pointer to node.
 * \param  pB Pointer to data item being sought.
 * \return Zero (0) if folder found.
 */
static int addressbook_treenode_find_folder_cb( gconstpointer pA, gconstpointer pB ) {
	AddressObject *aoA;

	aoA = ( AddressObject * ) pA;
	if( aoA->type == ADDR_ITEM_FOLDER ) {
		ItemFolder *folder, *fld;

		fld = ADAPTER_FOLDER(aoA)->itemFolder;
		folder = ( ItemFolder * ) pB;
		if( fld == folder ) return 0;	/* Found folder */
	}
	return 1;
}

/*
* Remove folder and group nodes from tree widget for items contained ("cut")
* in clipboard.
*/
static void addressbook_treenode_remove_item( void ) {
	GList *node;
	AddrSelectItem *cutItem;
	AddressCache *cache;
	AddrItemObject *aio;
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	GtkCTreeNode *tn;

	node = _clipBoard_->objectList;
	while( node ) {
		cutItem = node->data;
		node = g_list_next( node );
		cache = addrindex_get_cache(
			_clipBoard_->addressIndex, cutItem->cacheID );
		if( cache == NULL ) continue;
		aio = addrcache_get_object( cache, cutItem->uid );
		if( aio ) {
			tn = NULL;
			if( ADDRITEM_TYPE(aio) == ITEMTYPE_FOLDER ) {
				ItemFolder *folder;

				folder = ( ItemFolder * ) aio;
				tn = gtk_ctree_find_by_row_data_custom(
					ctree, NULL, folder,
					addressbook_treenode_find_folder_cb );
			}
			else if( ADDRITEM_TYPE(aio) == ITEMTYPE_GROUP ) {
				ItemGroup *group;

				group = ( ItemGroup * ) aio;
				tn = gtk_ctree_find_by_row_data_custom(
					ctree, NULL, group,
					addressbook_treenode_find_group_cb );
			}

			if( tn ) {
				/* Free up adapter and remove node. */
				gtk_ctree_remove_node( ctree, tn );
			}
		}
	}
}

/**
 * Find parent datasource for specified tree node.
 * \param  node Node to test.
 * \return Data source, or NULL if not found.
 */
static AddressDataSource *addressbook_find_datasource( GtkCTreeNode *node ) {
	AddressDataSource *ds = NULL;
	AddressObject *ao;

	g_return_val_if_fail(addrbook.ctree != NULL, NULL);

	while( node ) {
		if( GTK_CTREE_ROW(node)->level < 2 ) return NULL;
		ao = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), node );
		if( ao ) {
			/* printf( "ao->type = %d\n", ao->type ); */
			if( ao->type == ADDR_DATASOURCE ) {
				AdapterDSource *ads = ADAPTER_DSOURCE(ao);
				/* printf( "found it\n" ); */
				ds = ads->dataSource;
				break;
			}
		}
		node = GTK_CTREE_ROW(node)->parent;
	}
	return ds;
}

/**
 * Load address list widget with children of specified object.
 * \param obj Parent object to be loaded.
 */
static void addressbook_set_clist( AddressObject *obj ) {
	GtkCTree *ctreelist = GTK_CTREE(addrbook.clist);
	GtkCList *clist = GTK_CLIST(addrbook.clist);
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;

	if( obj == NULL ) {
		gtk_clist_clear(clist);
		return;
	}

	if( obj->type == ADDR_INTERFACE ) {
		/* printf( "set_clist: loading datasource...\n" ); */
		/* addressbook_node_load_datasource( GTK_CTREE(clist), obj ); */
		return;
	}

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	if( obj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(obj);
		ds = ADAPTER_DSOURCE(obj)->dataSource;
		if( ds ) {
			/* Load root folder */
			ItemFolder *rootFolder = NULL;
			rootFolder = addrindex_ds_get_root_folder( ds );
			addressbook_folder_load_person(
				ctreelist, addrindex_ds_get_root_folder( ds ) );
			addressbook_folder_load_group(
				ctreelist, addrindex_ds_get_root_folder( ds ) );
		}
	}
	else {
		if( obj->type == ADDR_ITEM_GROUP ) {
			/* Load groups */
			ItemGroup *itemGroup = ADAPTER_GROUP(obj)->itemGroup;
			addressbook_load_group( ctreelist, itemGroup );
		}
		else if( obj->type == ADDR_ITEM_FOLDER ) {
			/* Load folders */
			ItemFolder *itemFolder = ADAPTER_FOLDER(obj)->itemFolder;
			addressbook_folder_load_person( ctreelist, itemFolder );
			addressbook_folder_load_group( ctreelist, itemFolder );
		}
	}
	gtk_sctree_sort_node(GTK_CTREE(clist), NULL);
	gtk_clist_thaw(clist);
}

/**
 * Call back function to free adaptor. Call back is setup by function
 * gtk_ctree_node_set_row_data_full() when node is populated. This function is
 * called when the address book tree widget node is removed by calling
 * function gtk_ctree_remove_node().
 * 
 * \param data Tree node's row data.
 */
static void addressbook_free_treenode( gpointer data ) {
	AddressObject *ao;

	ao = ( AddressObject * ) data;
	if( ao == NULL ) return;
	if( ao->type == ADDR_INTERFACE ) {
		AdapterInterface *ai = ADAPTER_INTERFACE(ao);
		addrbookctl_free_interface( ai );
	}
	else if( ao->type == ADDR_DATASOURCE ) {
		AdapterDSource *ads = ADAPTER_DSOURCE(ao);
		addrbookctl_free_datasource( ads );
	}
	else if( ao->type == ADDR_ITEM_FOLDER ) {
		AdapterFolder *af = ADAPTER_FOLDER(ao);
		addrbookctl_free_folder( af );
	}
	else if( ao->type == ADDR_ITEM_GROUP ) {
		AdapterGroup *ag = ADAPTER_GROUP(ao);
		addrbookctl_free_group( ag );
	}
}

/*
* Create new adaptor for specified data source.
*/
AdapterDSource *addressbook_create_ds_adapter( AddressDataSource *ds,
				AddressObjectType otype, gchar *name )
{
	AdapterDSource *adapter = g_new0( AdapterDSource, 1 );
	ADDRESS_OBJECT(adapter)->type = ADDR_DATASOURCE;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup( name );
	adapter->dataSource = ds;
	adapter->subType = otype;
	return adapter;
}

void addressbook_ads_set_name( AdapterDSource *adapter, gchar *value ) {
	ADDRESS_OBJECT_NAME(adapter) =
		mgu_replace_string( ADDRESS_OBJECT_NAME(adapter), value );
}

/*
 * Load tree from address index with the initial data.
 */
static void addressbook_load_tree( void ) {
	GtkCTree *ctree = GTK_CTREE( addrbook.ctree );
	GList *nodeIf, *nodeDS;
	AdapterInterface *adapter;
	AddressInterface *iface;
	AddressTypeControlItem *atci;
	AddressDataSource *ds;
	AdapterDSource *ads;
	GtkCTreeNode *node, *newNode;
	gchar *name;

	nodeIf = _addressInterfaceList_;
	while( nodeIf ) {
		adapter = nodeIf->data;
		node = adapter->treeNode;
		iface = adapter->interface;
		atci = adapter->atci;
		if( iface ) {
			if( iface->useInterface ) {
				/* Load data sources below interface node */
				nodeDS = iface->listSource;
				while( nodeDS ) {
					ds = nodeDS->data;
					newNode = NULL;
					name = addrindex_ds_get_name( ds );
					ads = addressbook_create_ds_adapter(
							ds, atci->objectType, name );
					newNode = addressbook_add_object(
							node, ADDRESS_OBJECT(ads) );
					nodeDS = g_list_next( nodeDS );
				}
				gtk_ctree_expand( ctree, node );
			}
		}
		nodeIf = g_list_next( nodeIf );
	}
}

/*
 * Convert the old address book to new format.
 */
static gboolean addressbook_convert( AddressIndex *addrIndex ) {
	gboolean retVal = FALSE;
	gboolean errFlag = TRUE;
	gchar *msg = NULL;

	/* Read old address book, performing conversion */
	debug_print( "Reading and converting old address book...\n" );
	addrindex_set_file_name( addrIndex, ADDRESSBOOK_OLD_FILE );
	addrindex_read_data( addrIndex );
	if( addrIndex->retVal == MGU_NO_FILE ) {
		/* We do not have a file - new user */
		debug_print( "New user... create new books...\n" );
		addrindex_create_new_books( addrIndex );
		if( addrIndex->retVal == MGU_SUCCESS ) {
			/* Save index file */
			addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
			addrindex_save_data( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				retVal = TRUE;
				errFlag = FALSE;
			}
			else {
				msg = _( "New user, could not save index file." );
			}
		}
		else {
			msg = _( "New user, could not save address book files." );
		}
	}
	else {
		/* We have an old file */
		if( addrIndex->wasConverted ) {
			/* Converted successfully - save address index */
			addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
			addrindex_save_data( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				msg = _( "Old address book converted successfully." );
				retVal = TRUE;
				errFlag = FALSE;
			}
			else {
				msg = _("Old address book converted,\n"
					"could not save new address index file" );
			}
		}
		else {
			/* File conversion failed - just create new books */
			debug_print( "File conversion failed... just create new books...\n" );
			addrindex_create_new_books( addrIndex );
			if( addrIndex->retVal == MGU_SUCCESS ) {
				/* Save index */
				addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
				addrindex_save_data( addrIndex );
				if( addrIndex->retVal == MGU_SUCCESS ) {
					msg = _("Could not convert address book,\n"
						"but created empty new address book files." );
					retVal = TRUE;
					errFlag = FALSE;
				}
				else {
					msg = _("Could not convert address book,\n"
						"could not create new address book files." );
				}
			}
			else {
				msg = _("Could not convert address book\n"
					"and could not create new address book files." );
			}
		}
	}
	if( errFlag ) {
		debug_print( "Error\n%s\n", msg );
		alertpanel_with_type( _( "Addressbook conversion error" ), msg, GTK_STOCK_CLOSE, 
				      NULL, NULL, NULL, ALERT_ERROR );
	}
	else if( msg ) {
		debug_print( "Warning\n%s\n", msg );
		alertpanel_with_type( _( "Addressbook conversion" ), msg, GTK_STOCK_CLOSE, 
				      NULL, NULL, NULL, ALERT_WARNING );
	}

	return retVal;
}

void addressbook_read_file( void ) {
	AddressIndex *addrIndex = NULL;

	debug_print( "Reading address index...\n" );
	if( _addressIndex_ ) {
		debug_print( "address book already read!!!\n" );
		return;
	}

	addrIndex = addrindex_create_index();
	addrindex_initialize();

	/* Use new address book index. */
	addrindex_set_file_path( addrIndex, get_rc_dir() );
	addrindex_set_file_name( addrIndex, ADDRESSBOOK_INDEX_FILE );
	addrindex_read_data( addrIndex );
	if( addrIndex->retVal == MGU_NO_FILE ) {
		/* Conversion required */
		debug_print( "Converting...\n" );
		if( addressbook_convert( addrIndex ) ) {
			_addressIndex_ = addrIndex;
		}
	}
	else if( addrIndex->retVal == MGU_SUCCESS ) {
		_addressIndex_ = addrIndex;
	}
	else {
		/* Error reading address book */
		debug_print( "Could not read address index.\n" );
		addrindex_print_index( addrIndex, stdout );
		alertpanel_with_type( _( "Addressbook Error" ),
			    _( "Could not read address index" ),
			    GTK_STOCK_CLOSE, NULL, NULL, NULL,
			    ALERT_ERROR);
	}
	debug_print( "done.\n" );
}

/*
* Add object into the address index tree widget.
* Enter: node	Parent node.
*        obj	Object to add.
* Return: Node that was added, or NULL if object not added.
*/
static GtkCTreeNode *addressbook_add_object(GtkCTreeNode *node,
					    AddressObject *obj)
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *added;
	AddressObject *pobj;
	AddressObjectType otype;
	AddressTypeControlItem *atci = NULL;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(obj  != NULL, NULL);

	pobj = gtk_ctree_node_get_row_data(ctree, node);
	g_return_val_if_fail(pobj != NULL, NULL);

	/* Determine object type to be displayed */
	if( obj->type == ADDR_DATASOURCE ) {
		otype = ADAPTER_DSOURCE(obj)->subType;
	}
	else {
		otype = obj->type;
	}

	/* Handle any special conditions. */
	added = node;
	atci = addrbookctl_lookup( otype );
	if( atci ) {
		if( atci->showInTree ) {
			/* Add object to tree */
			gchar **name;
			name = &obj->name;
			added = gtk_ctree_insert_node( ctree, node, NULL, name, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm, atci->iconXpmOpen, atci->maskXpmOpen,
				atci->treeLeaf, atci->treeExpand );
			gtk_ctree_node_set_row_data_full( ctree, added, obj,
				addressbook_free_treenode );
		}
	}

	gtk_sctree_sort_node(ctree, node);

	return added;
}

/**
 * Add group into the address index tree.
 * \param  node      Parent node.
 * \param  ds        Data source.
 * \param  itemGroup Group to add.
 * \return Inserted node.
 */
static GtkCTreeNode *addressbook_node_add_group(
		GtkCTreeNode *node, AddressDataSource *ds,
		ItemGroup *itemGroup )
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *newNode;
	AdapterGroup *adapter;
	AddressTypeControlItem *atci = NULL;
	gchar **name;

	if( ds == NULL ) return NULL;
	if( node == NULL || itemGroup == NULL ) return NULL;

	name = &itemGroup->obj.name;

	atci = addrbookctl_lookup( ADDR_ITEM_GROUP );

	adapter = g_new0( AdapterGroup, 1 );
	ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_GROUP;
	ADDRESS_OBJECT_NAME(adapter) = g_strdup( ADDRITEM_NAME(itemGroup) );
	adapter->itemGroup = itemGroup;

	newNode = gtk_ctree_insert_node( ctree, node, NULL, name, FOLDER_SPACING,
			atci->iconXpm, atci->maskXpm, atci->iconXpm, atci->maskXpm,
			atci->treeLeaf, atci->treeExpand );
	gtk_ctree_node_set_row_data_full( ctree, newNode, adapter,
		addressbook_free_treenode );
	gtk_sctree_sort_node( ctree, node );
	return newNode;
}

/**
 * Add folder into the address index tree. Only visible folders are loaded into
 * the address index tree. Note that the root folder is not inserted into the
 * tree.
 *
 * \param  node	      Parent node.
 * \param  ds         Data source.
 * \param  itemFolder Folder to add.
 * \param  otype      Object type to display.
 * \return Inserted node for the folder.
*/
static GtkCTreeNode *addressbook_node_add_folder(
		GtkCTreeNode *node, AddressDataSource *ds,
		ItemFolder *itemFolder, AddressObjectType otype )
{
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	GtkCTreeNode *newNode = NULL;
	AdapterFolder *adapter;
	AddressTypeControlItem *atci = NULL;
	GList *listItems = NULL;
	gchar *name;
	ItemFolder *rootFolder;

	/* Only visible folders */
	if( itemFolder->isHidden ) return NULL;

	if( ds == NULL ) return NULL;
	if( node == NULL || itemFolder == NULL ) return NULL;

	/* Determine object type */
	atci = addrbookctl_lookup( otype );
	if( atci == NULL ) return NULL;

	rootFolder = addrindex_ds_get_root_folder( ds );
	if( itemFolder == rootFolder ) {
		newNode = node;
	}
	else {
		adapter = g_new0( AdapterFolder, 1 );
		ADDRESS_OBJECT_TYPE(adapter) = ADDR_ITEM_FOLDER;
		ADDRESS_OBJECT_NAME(adapter) = g_strdup( ADDRITEM_NAME(itemFolder) );
		adapter->itemFolder = itemFolder;

		name = ADDRITEM_NAME(itemFolder);
		newNode = gtk_ctree_insert_node( ctree, node, NULL, &name, FOLDER_SPACING,
				atci->iconXpm, atci->maskXpm, atci->iconXpm, atci->maskXpm,
				atci->treeLeaf, atci->treeExpand );
		if( newNode ) {
			gtk_ctree_node_set_row_data_full( ctree, newNode, adapter,
				addressbook_free_treenode );
		}
	}

	listItems = itemFolder->listFolder;
	while( listItems ) {
		ItemFolder *item = listItems->data;
		addressbook_node_add_folder( newNode, ds, item, otype );
		listItems = g_list_next( listItems );
	}
	listItems = itemFolder->listGroup;
	while( listItems ) {
		ItemGroup *item = listItems->data;
		addressbook_node_add_group( newNode, ds, item );
		listItems = g_list_next( listItems );
	}
	gtk_sctree_sort_node( ctree, node );
	return newNode;
}

void addressbook_export_to_file( void ) {
	if( _addressIndex_ ) {
		/* Save all new address book data */
		debug_print( "Saving address books...\n" );
		addrindex_save_all_books( _addressIndex_ );

		debug_print( "Exporting addressbook to file...\n" );
		addrindex_save_data( _addressIndex_ );
		if( _addressIndex_->retVal != MGU_SUCCESS ) {
			addrindex_print_index( _addressIndex_, stdout );
		}

		/* Notify address completion of new data */
		invalidate_address_completion();
	}
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		addressbook_close();
	return FALSE;
}

/*
* Comparison using cell contents (text in first column). Used for sort
* address index widget.
*/
static gint addressbook_treenode_compare_func(
	GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2 )
{
	GtkCell *cell1 = ((GtkCListRow *)ptr1)->cell;
	GtkCell *cell2 = ((GtkCListRow *)ptr2)->cell;
	gchar *name1 = NULL, *name2 = NULL;
	if( cell1 ) name1 = cell1->u.text;
	if( cell2 ) name2 = cell2->u.text;
	if( ! name1 ) return ( name2 != NULL );
	if( ! name2 ) return -1;
	return g_utf8_collate( name1, name2 );
}

/*
* Comparison using object names and types.
*/
static gint addressbook_list_compare_func(
	GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2 )
{
	AddrItemObject *aio1 = ((GtkCListRow *)ptr1)->data;
	AddrItemObject *aio2 = ((GtkCListRow *)ptr2)->data;
	gchar *name1 = NULL, *name2 = NULL;

	/* Order by cell contents */
	name1 = ADDRITEM_NAME( aio1 );
	name2 = ADDRITEM_NAME( aio2 );

	if( aio1->type == aio2->type ) {
		/* Order by name */
		if( ! name1 ) return ( name2 != NULL );
		if( ! name2 ) return -1;
		return g_utf8_collate( name1, name2 );
	}
	else {
		/* Order groups before person */
		if( aio1->type == ITEMTYPE_GROUP ) {
			return -1;
		}
		else if( aio2->type == ITEMTYPE_GROUP ) {
			return 1;
		}
		return 0;
	}
}

static void addressbook_new_book_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if( adapter == NULL ) return;
	ads = addressbook_edit_book( _addressIndex_, NULL );
	if( ads ) {
		newNode = addressbook_add_object( adapter->treeNode, ADDRESS_OBJECT(ads) );
		if( newNode ) {
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), newNode );
			addrbook.treeSelected = newNode;
		}
	}
}

static void addressbook_new_vcard_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_VCARD );
	if( adapter == NULL ) return;
	ads = addressbook_edit_vcard( _addressIndex_, NULL );
	if( ads ) {
		newNode = addressbook_add_object( adapter->treeNode, ADDRESS_OBJECT(ads) );
		if( newNode ) {
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), newNode );
			addrbook.treeSelected = newNode;
		}
	}
}

#ifdef USE_JPILOT
static void addressbook_new_jpilot_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_JPILOT );
	if( adapter == NULL ) return;
	iface = adapter->interface;
	if( ! iface->haveLibrary ) return;
	ads = addressbook_edit_jpilot( _addressIndex_, NULL );
	if( ads ) {
		newNode = addressbook_add_object( adapter->treeNode, ADDRESS_OBJECT(ads) );
		if( newNode ) {
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), newNode );
			addrbook.treeSelected = newNode;
		}
	}
}
#endif

#ifdef USE_LDAP
static void addressbook_new_ldap_cb( gpointer data, guint action, GtkWidget *widget ) {
	AdapterDSource *ads;
	AdapterInterface *adapter;
	AddressInterface *iface;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_LDAP );
	if( adapter == NULL ) return;
	iface = adapter->interface;
	if( ! iface->haveLibrary ) return;
	ads = addressbook_edit_ldap( _addressIndex_, NULL );
	if( ads ) {
		newNode = addressbook_add_object( adapter->treeNode, ADDRESS_OBJECT(ads) );
		if( newNode ) {
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), newNode );
			addrbook.treeSelected = newNode;
		}
	}
}
#endif

/**
 * Display address search status message.
 * \param queryType Query type.
 * \param status    Status/Error code.
 */
static void addressbook_search_message( gint queryType, gint sts ) {
	gchar *desc = NULL;
	*addressbook_msgbuf = '\0';

	if( sts != MGU_SUCCESS ) {
		if( queryType == ADDRQUERY_LDAP ) {
#ifdef USE_LDAP			
			desc = addressbook_err2string( _lutErrorsLDAP_, sts );
#endif
		}
	}
	if( desc ) {
		g_snprintf( addressbook_msgbuf,
			sizeof(addressbook_msgbuf), "%s", desc );
		addressbook_status_show( addressbook_msgbuf );
	}
	else {
		addressbook_status_show( "" );
	}
}

/**
 * Refresh addressbook by forcing refresh of current selected object in
 * tree.
 */
static void addressbook_refresh_current( void ) {
	AddressObject *obj;
	GtkCTree *ctree;

	ctree = GTK_CTREE(addrbook.ctree);
	obj = gtk_ctree_node_get_row_data( ctree, addrbook.treeSelected );
	if( obj == NULL ) return;
	addressbook_set_clist( obj );
}

/**
 * Message that is displayed whilst a query is executing in a background
 * thread.
 */
static gchar *_tempMessage_ = N_( "Busy searching..." );

/**
 * Address search idle function. This function is called during UI idle time
 * while a search is in progress.
 *
 * \param data Idler data.
 */
static void addressbook_search_idle( gpointer data ) {
	/*
	gint queryID;

	queryID = GPOINTER_TO_INT( data );
	printf( "addressbook_ldap_idle... queryID=%d\n", queryID );
	*/
}

/**
 * Search completion callback function. This removes the query from the idle
 * list.
 *
 * \param queryID Query ID of search request.
 */
void addressbook_clear_idler( gint queryID ) {
	gpointer ptrQID;

	/* Remove idler function */
	/* printf( "addressbook_clear_idler::%d::\n", queryID ); */
	ptrQID = GINT_TO_POINTER( queryID );
	if( ptrQID ) {
		gtk_idle_remove_by_data( ptrQID );
	}
}

/**
 * Search completion callback function. This removes the query from the idle
 * list.
 *
 * \param sender  Sender of query.
 * \param queryID Query ID of search request.
 * \param status  Search status.
 * \param data    Query data.
 */
static void addressbook_search_callback_end(
		gpointer sender, gint queryID, gint status, gpointer data )
{
	gpointer ptrQID;
	QueryRequest *req;
	AddrQueryObject *aqo;

	/* Remove idler function */
	ptrQID = GINT_TO_POINTER( queryID );
	if( ptrQID ) {
		gtk_idle_remove_by_data( ptrQID );
	}

	/* Refresh addressbook contents */
	addressbook_refresh_current();
	req = qrymgr_find_request( queryID );
	if( req != NULL ) {
		aqo = ( AddrQueryObject * ) req->queryList->data;
		addressbook_search_message( aqo->queryType, status );
	}

	/* Stop the search */
	addrindex_stop_search( queryID );
}

/**
 * Label (a format string) that is used to name each folder.
 */
static gchar *_queryFolderLabel_ = N_( "Search '%s'" );

/**
 * Perform search.
 *
 * \param ds         Data source to search.
 * \param searchTerm String to lookup.
 * \param pNode      Parent data source node.
 */
static void addressbook_perform_search(
		AddressDataSource *ds, gchar *searchTerm,
		GtkCTreeNode *pNode )
{
	AddrBookBase *adbase;
	AddressCache *cache;
	ItemFolder *folder;
	GtkCTree *ctree;
	GtkCTreeNode *nNode;
	gchar *name;
	gint queryID;
	guint idleID;
	AddressObjectType aoType = ADDR_NONE;

	/* Setup a query */
	if( *searchTerm == '\0' || strlen( searchTerm ) < 1 ) return;

	if( ds->type == ADDR_IF_LDAP ) {
#if USE_LDAP
		aoType = ADDR_LDAP_QUERY;
#endif
	}
	else {
		return;
	}

	/* Get reference to address cache */	
	adbase = ( AddrBookBase * ) ds->rawDataSource;
	cache = adbase->addressCache;

	/* Create a folder for the search results */
	folder = addrcache_add_new_folder( cache, NULL );
	name = g_strdup_printf( _queryFolderLabel_, searchTerm );
	addritem_folder_set_name( folder, name );
	addritem_folder_set_remarks( folder, "" );
	g_free( name );

	/* Now let's see the folder */
	ctree = GTK_CTREE(addrbook.ctree);
	nNode = addressbook_node_add_folder( pNode, ds, folder, aoType );
	gtk_ctree_expand( ctree, pNode );
	if( nNode ) {
		gtk_ctree_select( ctree, nNode );
		addrbook.treeSelected = nNode;
	}

	/* Setup the search */
	queryID = addrindex_setup_explicit_search(
		ds, searchTerm, folder, addressbook_search_callback_end, NULL );
	if( queryID == 0 ) return;

	/* Set up idler function */
	idleID = gtk_idle_add(
			( GtkFunction ) addressbook_search_idle,
			GINT_TO_POINTER( queryID ) );

	/* Start search, sit back and wait for something to happen */
	addrindex_start_search( queryID );

	addressbook_status_show( _tempMessage_ );
}

/**
 * Lookup button handler. Address search is only performed against
 * address interfaces for external queries.
 *
 * \param button Lookup button widget.
 * \param data   Data object.
 */
static void addressbook_lup_clicked( GtkButton *button, gpointer data ) {
	GtkCTree *ctree;
	AddressObject *obj;
	AddressDataSource *ds;
	AddressInterface *iface;
	gchar *searchTerm;
	GtkCTreeNode *node, *parentNode;

	node = addrbook.treeSelected;
	if( ! node ) return;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;

	ctree = GTK_CTREE(addrbook.ctree);
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;

	/* We must have a datasource that is an external interface */
	iface = ds->interface;
	if( ! iface->haveLibrary ) return;
	if( ! iface->externalQuery ) return;

	searchTerm =
		gtk_editable_get_chars( GTK_EDITABLE(addrbook.entry), 0, -1 );
	g_strchomp( searchTerm );

	if( obj->type == ADDR_ITEM_FOLDER ) {
		parentNode = GTK_CTREE_ROW(node)->parent;
	}
	else {
		parentNode = node;
	}
	addressbook_perform_search( ds, searchTerm, parentNode );
	gtk_widget_grab_focus( addrbook.entry );

	g_free( searchTerm );
}

#ifdef USE_LDAP
/**
 * Browse address entry for highlighted entry.
 */
static void addressbook_browse_entry_cb(void)
{
	GtkCTree *clist = GTK_CTREE(addrbook.clist);
	AddressObject *obj;
	AddressDataSource *ds;
	AddressInterface *iface;
	ItemPerson *person;
	ItemEMail *email;

	if(addrbook.listSelected == NULL)
		return;

	obj = gtk_ctree_node_get_row_data(clist, addrbook.listSelected);
	if (obj == NULL)
		return;

	ds = addressbook_find_datasource(GTK_CTREE_NODE(addrbook.treeSelected));
	if(ds == NULL)
		return;

	iface = ds->interface;
	if(! iface->haveLibrary )
		return;

	person = NULL;
	if (obj->type == ADDR_ITEM_EMAIL) {
		email = ( ItemEMail * ) obj;
		if (email == NULL)
			return;
		
		person = (ItemPerson *) ADDRITEM_PARENT(email);
	}
	else if (obj->type == ADDR_ITEM_PERSON) {
		person = (ItemPerson *) obj;
	}
	else {
		/* None of these */
		return;
	}

	if( iface->type == ADDR_IF_LDAP ) {
		browseldap_entry(ds, person->externalID);
	}
}
#endif

/* **********************************************************************
* Build lookup tables.
* ***********************************************************************
*/

/*
 * Remap object types.
 * Enter:  abType AddressObjectType (used in tree node).
 * Return: ItemObjectType (used in address cache data).
 */
ItemObjectType addressbook_type2item( AddressObjectType abType ) {
	ItemObjectType ioType;

	switch( abType ) {
		case ADDR_ITEM_PERSON: ioType = ITEMTYPE_PERSON;     break;
		case ADDR_ITEM_EMAIL:  ioType = ITEMTYPE_EMAIL;      break;
		case ADDR_ITEM_FOLDER: ioType = ITEMTYPE_FOLDER;     break;
		case ADDR_ITEM_GROUP:  ioType = ITEMTYPE_GROUP;      break;
		case ADDR_DATASOURCE:  ioType = ITEMTYPE_DATASOURCE; break;
		default:               ioType = ITEMTYPE_NONE;       break;
	}
	return ioType;
}

/*
* Build table that controls the rendering of object types.
*/
void addrbookctl_build_map( GtkWidget *window ) {
	AddressTypeControlItem *atci;

	/* Build icons */
	stock_pixmap_gdk(window, STOCK_PIXMAP_DIR_CLOSE, &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_DIR_OPEN, &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_GROUP, &groupxpm, &groupxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_VCARD, &vcardxpm, &vcardxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_BOOK, &bookxpm, &bookxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_ADDRESS, &addressxpm, &addressxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_JPILOT, &jpilotxpm, &jpilotxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_CATEGORY, &categoryxpm, &categoryxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_LDAP, &ldapxpm, &ldapxpmmask);
	stock_pixmap_gdk(window, STOCK_PIXMAP_ADDRESS_SEARCH, &addrsearchxpm, &addrsearchxpmmask);

	_addressBookTypeHash_ = g_hash_table_new( g_int_hash, g_int_equal );
	_addressBookTypeList_ = NULL;

	/* Interface */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_INTERFACE;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Interface" );
	atci->iconXpm = folderxpm;
	atci->maskXpm = folderxpmmask;
	atci->iconXpmOpen = folderopenxpm;
	atci->maskXpmOpen = folderopenxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Address book */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_BOOK;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Address Book" );
	atci->iconXpm = bookxpm;
	atci->maskXpm = bookxpmmask;
	atci->iconXpmOpen = bookxpm;
	atci->maskXpmOpen = bookxpmmask;
	atci->menuCommand = "/File/New Book";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item person */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_PERSON;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Person" );
	atci->iconXpm = NULL;
	atci->maskXpm = NULL;
	atci->iconXpmOpen = NULL;
	atci->maskXpmOpen = NULL;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item email */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_EMAIL;
	atci->interfaceType = ADDR_IF_NONE;
	atci->showInTree = FALSE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "EMail Address" );
	atci->iconXpm = addressxpm;
	atci->maskXpm = addressxpmmask;
	atci->iconXpmOpen = addressxpm;
	atci->maskXpmOpen = addressxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item group */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_GROUP;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Group" );
	atci->iconXpm = groupxpm;
	atci->maskXpm = groupxpmmask;
	atci->iconXpmOpen = groupxpm;
	atci->maskXpmOpen = groupxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Item folder */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_ITEM_FOLDER;
	atci->interfaceType = ADDR_IF_BOOK;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "Folder" );
	atci->iconXpm = folderxpm;
	atci->maskXpm = folderxpmmask;
	atci->iconXpmOpen = folderopenxpm;
	atci->maskXpmOpen = folderopenxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* vCard */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_VCARD;
	atci->interfaceType = ADDR_IF_VCARD;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "vCard" );
	atci->iconXpm = vcardxpm;
	atci->maskXpm = vcardxpmmask;
	atci->iconXpmOpen = vcardxpm;
	atci->maskXpmOpen = vcardxpmmask;
	atci->menuCommand = "/File/New vCard";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* J-Pilot */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_JPILOT;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "JPilot" );
	atci->iconXpm = jpilotxpm;
	atci->maskXpm = jpilotxpmmask;
	atci->iconXpmOpen = jpilotxpm;
	atci->maskXpmOpen = jpilotxpmmask;
	atci->menuCommand = "/File/New JPilot";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* Category */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_CATEGORY;
	atci->interfaceType = ADDR_IF_JPILOT;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "JPilot" );
	atci->iconXpm = categoryxpm;
	atci->maskXpm = categoryxpmmask;
	atci->iconXpmOpen = categoryxpm;
	atci->maskXpmOpen = categoryxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* LDAP Server */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_LDAP;
	atci->interfaceType = ADDR_IF_LDAP;
	atci->showInTree = TRUE;
	atci->treeExpand = TRUE;
	atci->treeLeaf = FALSE;
	atci->displayName = _( "LDAP Server" );
	atci->iconXpm = ldapxpm;
	atci->maskXpm = ldapxpmmask;
	atci->iconXpmOpen = ldapxpm;
	atci->maskXpmOpen = ldapxpmmask;
	atci->menuCommand = "/File/New Server";
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

	/* LDAP Query  */
	atci = g_new0( AddressTypeControlItem, 1 );
	atci->objectType = ADDR_LDAP_QUERY;
	atci->interfaceType = ADDR_IF_LDAP;
	atci->showInTree = TRUE;
	atci->treeExpand = FALSE;
	atci->treeLeaf = TRUE;
	atci->displayName = _( "LDAP Query" );
	atci->iconXpm = addrsearchxpm;
	atci->maskXpm = addrsearchxpmmask;
	atci->iconXpmOpen = addrsearchxpm;
	atci->maskXpmOpen = addrsearchxpmmask;
	atci->menuCommand = NULL;
	g_hash_table_insert( _addressBookTypeHash_, &atci->objectType, atci );
	_addressBookTypeList_ = g_list_append( _addressBookTypeList_, atci );

}

/*
* Search for specified object type.
*/
AddressTypeControlItem *addrbookctl_lookup( gint ot ) {
	gint objType = ot;
	return ( AddressTypeControlItem * ) g_hash_table_lookup( _addressBookTypeHash_, &objType );
}

/*
* Search for specified interface type.
*/
AddressTypeControlItem *addrbookctl_lookup_iface( AddressIfType ifType ) {
	GList *node = _addressBookTypeList_;
	while( node ) {
		AddressTypeControlItem *atci = node->data;
		if( atci->interfaceType == ifType ) return atci;
		node = g_list_next( node );
	}
	return NULL;
}

static void addrbookctl_free_address( AddressObject *obj ) {
	g_free( obj->name );
	obj->type = ADDR_NONE;
	obj->name = NULL;
}

static void addrbookctl_free_interface( AdapterInterface *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->interface = NULL;
	adapter->interfaceType = ADDR_IF_NONE;
	adapter->atci = NULL;
	adapter->enabled = FALSE;
	adapter->haveLibrary = FALSE;
	adapter->treeNode = NULL;
	g_free( adapter );
}

static void addrbookctl_free_datasource( AdapterDSource *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->dataSource = NULL;
	adapter->subType = ADDR_NONE;
	g_free( adapter );
}

static void addrbookctl_free_folder( AdapterFolder *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->itemFolder = NULL;
	g_free( adapter );
}

static void addrbookctl_free_group( AdapterGroup *adapter ) {
	addrbookctl_free_address( ADDRESS_OBJECT(adapter) );
	adapter->itemGroup = NULL;
	g_free( adapter );
}

/**
 * Build GUI interface list.
 */
void addrbookctl_build_iflist( void ) {
	AddressTypeControlItem *atci;
	AdapterInterface *adapter;
	GList *list = NULL;

	if( _addressIndex_ == NULL ) {
		_addressIndex_ = addrindex_create_index();
		if( _clipBoard_ == NULL ) {
			_clipBoard_ = addrclip_create();
		}
		addrclip_set_index( _clipBoard_, _addressIndex_ );
	}
	_addressInterfaceList_ = NULL;
	list = addrindex_get_interface_list( _addressIndex_ );
	while( list ) {
		AddressInterface *interface = list->data;
		atci = addrbookctl_lookup_iface( interface->type );
		if( atci ) {
			adapter = g_new0( AdapterInterface, 1 );
			adapter->interfaceType = interface->type;
			adapter->atci = atci;
			adapter->interface = interface;
			adapter->treeNode = NULL;
			adapter->enabled = TRUE;
			adapter->haveLibrary = interface->haveLibrary;
			ADDRESS_OBJECT(adapter)->type = ADDR_INTERFACE;
			ADDRESS_OBJECT_NAME(adapter) = g_strdup( atci->displayName );
			_addressInterfaceList_ =
				g_list_append( _addressInterfaceList_, adapter );
		}
		list = g_list_next( list );
	}
}

#if 0
void addrbookctl_free_selection( GList *list ) {
	GList *node = list;
	while( node ) {
		AdapterInterface *adapter = node->data;
		adapter = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}
#endif

/**
 * Find GUI interface type specified interface type.
 * \param  ifType Interface type.
 * \return Interface item, or NULL if not found.
 */
AdapterInterface *addrbookctl_find_interface( AddressIfType ifType ) {
	GList *node = _addressInterfaceList_;
	while( node ) {
		AdapterInterface *adapter = node->data;
		if( adapter->interfaceType == ifType ) return adapter;
		node = g_list_next( node );
	}
	return NULL;
}

/**
 * Build interface list selection.
 */
void addrbookctl_build_ifselect( void ) {
	GList *newList = NULL;
	gchar *selectStr;
	gchar **splitStr;
	gint ifType;
	gint i;
	gchar *endptr = NULL;
	gboolean enabled;
	AdapterInterface *adapter;

	selectStr = g_strdup( ADDRESSBOOK_IFACE_SELECTION );

	/* Parse string */
	splitStr = g_strsplit( selectStr, ",", -1 );
	for( i = 0; i < ADDRESSBOOK_MAX_IFACE; i++ ) {
		if( splitStr[i] ) {
			/* printf( "%d : %s\n", i, splitStr[i] ); */
			ifType = strtol( splitStr[i], &endptr, 10 );
			enabled = TRUE;
			if( *endptr ) {
				if( strcmp( endptr, "/n" ) == 0 ) {
					enabled = FALSE;
				}
			}
			/* printf( "\t%d : %s\n", ifType, enabled ? "yes" : "no" ); */
			adapter = addrbookctl_find_interface( ifType );
			if( adapter ) {
				newList = g_list_append( newList, adapter );
			}
		}
		else {
			break;
		}
	}
	/* printf( "i=%d\n", i ); */
	g_strfreev( splitStr );
	g_free( selectStr );

	/* Replace existing list */
	mgu_clear_list( _addressIFaceSelection_ );
	g_list_free( _addressIFaceSelection_ );
	_addressIFaceSelection_ = newList;
	newList = NULL;
}

/* ***********************************************************************
 * Add sender to address book.
 * ***********************************************************************
 */

/*
 * This function is used by the Add sender to address book function.
 */
gboolean addressbook_add_contact(
		const gchar *name, const gchar *address, const gchar *remarks )
{
	debug_print( "addressbook_add_contact: name/address: %s - %s\n", name, address );
	if( addressadd_selection( _addressIndex_, name, address, remarks ) ) {
		debug_print( "addressbook_add_contact - added\n" );
		addressbook_refresh();
	}
	return TRUE;
}

/* **********************************************************************
 * Address Import.
 * ***********************************************************************
 */

/**
 * Import LDIF file.
 */
static void addressbook_import_ldif_cb( void ) {
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if( adapter ) {
		if( adapter->treeNode ) {
			abf = addressbook_imp_ldif( _addressIndex_ );
			if( abf ) {
				ds = addrindex_index_add_datasource(
					_addressIndex_, ADDR_IF_BOOK, abf );
				ads = addressbook_create_ds_adapter(
					ds, ADDR_BOOK, NULL );
				addressbook_ads_set_name(
					ads, addrbook_get_name( abf ) );
				newNode = addressbook_add_object(
					adapter->treeNode,
					ADDRESS_OBJECT(ads) );
				if( newNode ) {
					gtk_ctree_select(
						GTK_CTREE(addrbook.ctree),
						newNode );
					addrbook.treeSelected = newNode;
				}

				/* Notify address completion */
				invalidate_address_completion();
			}
		}
	}
}

/**
 * Import MUTT file.
 */
static void addressbook_import_mutt_cb( void ) {
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if( adapter ) {
		if( adapter->treeNode ) {
			abf = addressbook_imp_mutt( _addressIndex_ );
			if( abf ) {
				ds = addrindex_index_add_datasource(
					_addressIndex_, ADDR_IF_BOOK, abf );
				ads = addressbook_create_ds_adapter(
					ds, ADDR_BOOK, NULL );
				addressbook_ads_set_name(
					ads, addrbook_get_name( abf ) );
				newNode = addressbook_add_object(
					adapter->treeNode,
					ADDRESS_OBJECT(ads) );
				if( newNode ) {
					gtk_ctree_select(
						GTK_CTREE(addrbook.ctree),
						newNode );
					addrbook.treeSelected = newNode;
				}

				/* Notify address completion */
				invalidate_address_completion();
			}
		}
	}
}

/**
 * Import Pine file.
 */
static void addressbook_import_pine_cb( void ) {
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
	if( adapter ) {
		if( adapter->treeNode ) {
			abf = addressbook_imp_pine( _addressIndex_ );
			if( abf ) {
				ds = addrindex_index_add_datasource(
					_addressIndex_, ADDR_IF_BOOK, abf );
				ads = addressbook_create_ds_adapter(
					ds, ADDR_BOOK, NULL );
				addressbook_ads_set_name(
					ads, addrbook_get_name( abf ) );
				newNode = addressbook_add_object(
					adapter->treeNode,
					ADDRESS_OBJECT(ads) );
				if( newNode ) {
					gtk_ctree_select(
						GTK_CTREE(addrbook.ctree),
						newNode );
					addrbook.treeSelected = newNode;
				}

				/* Notify address completion */
				invalidate_address_completion();
			}
		}
	}
}

/**
 * Harvest addresses.
 * \param folderItem Folder to import.
 * \param sourceInd  Source indicator: FALSE - Folder, TRUE - Messages.
 * \param msgList    List of message numbers, or NULL to process folder.
 */
void addressbook_harvest(
	FolderItem *folderItem, gboolean sourceInd, GList *msgList )
{
	AddressDataSource *ds = NULL;
	AdapterDSource *ads = NULL;
	AddressBookFile *abf = NULL;
	AdapterInterface *adapter;
	GtkCTreeNode *newNode;

	abf = addrgather_dlg_execute(
		folderItem, _addressIndex_, sourceInd, msgList );
	if( abf ) {
		ds = addrindex_index_add_datasource(
			_addressIndex_, ADDR_IF_BOOK, abf );

		adapter = addrbookctl_find_interface( ADDR_IF_BOOK );
		if( adapter ) {
			if( adapter->treeNode ) {
				ads = addressbook_create_ds_adapter(
					ds, ADDR_BOOK, addrbook_get_name( abf ) );
				newNode = addressbook_add_object(
						adapter->treeNode,
						ADDRESS_OBJECT(ads) );
			}
		}

		/* Notify address completion */
		invalidate_address_completion();
	}
}

/**
 * Export HTML file.
 */
static void addressbook_export_html_cb( void ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressDataSource *ds = NULL;
	AddrBookBase *adbase;
	AddressCache *cache;
	GtkCTreeNode *node = NULL;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;
	adbase = ( AddrBookBase * ) ds->rawDataSource;
	cache = adbase->addressCache;
	addressbook_exp_html( cache );
}

/**
 * Export LDIF file.
 */
static void addressbook_export_ldif_cb( void ) {
	GtkCTree *ctree = GTK_CTREE(addrbook.ctree);
	AddressObject *obj;
	AddressDataSource *ds = NULL;
	AddrBookBase *adbase;
	AddressCache *cache;
	GtkCTreeNode *node = NULL;

	if( ! addrbook.treeSelected ) return;
	node = addrbook.treeSelected;
	if( GTK_CTREE_ROW(node)->level == 1 ) return;
	obj = gtk_ctree_node_get_row_data( ctree, node );
	if( obj == NULL ) return;

	ds = addressbook_find_datasource( node );
	if( ds == NULL ) return;
	adbase = ( AddrBookBase * ) ds->rawDataSource;
	cache = adbase->addressCache;
	addressbook_exp_ldif( cache );
}

static void addressbook_start_drag(GtkWidget *widget, gint button, 
				   GdkEvent *event,
			           void *data)
{
	GdkDragContext *context;
	if (addressbook_target_list == NULL)
		addressbook_target_list = gtk_target_list_new(
				addressbook_drag_types, 1);
	context = gtk_drag_begin(widget, addressbook_target_list,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT, button, event);
	gtk_drag_set_icon_default(context);
}

static ItemPerson *dragged_person = NULL;
static ItemFolder *dragged_folder = NULL;
static AddressBookFile *dragged_ab = NULL;

static void addressbook_drag_data_get(GtkWidget        *widget,
				     GdkDragContext   *drag_context,
				     GtkSelectionData *selection_data,
				     guint             info,
				     guint             time,
				     void	      *data)
{
	AddrItemObject *aio = NULL;
	AddressObject *pobj = NULL;
	AdapterDSource *ads = NULL;
	AddressDataSource *ds = NULL;

	pobj = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.ctree), addrbook.treeSelected );
	if( pobj == NULL ) return;

	if( pobj->type == ADDR_DATASOURCE ) {
		ads = ADAPTER_DSOURCE(pobj);
		ds = ads->dataSource;
	} else if (pobj->type == ADDR_ITEM_GROUP) {
		return;
	}
	
	else if( pobj->type != ADDR_INTERFACE ) {
		ds = addressbook_find_datasource( addrbook.treeSelected );
		if (!ds)
			return;
	}
	if (addrbook.listSelected)
		aio = gtk_ctree_node_get_row_data( GTK_CTREE(addrbook.clist), 
				addrbook.listSelected );
	
	while (aio && aio->type != ADDR_ITEM_PERSON) {
		aio = aio->parent;
	}

	if (aio && aio->type == ADDR_ITEM_PERSON) {
		dragged_person = (ItemPerson *)aio;
		dragged_folder = (ItemFolder *)ADAPTER_FOLDER(pobj)->itemFolder;
		dragged_ab = addressbook_get_book_file();

		gtk_selection_data_set(selection_data,
				       selection_data->target, 8,
				       "Dummy_addr", 11);
	       	drag_context->actions = GDK_ACTION_MOVE;
		
		if (pobj->type == ADDR_DATASOURCE) {
			if( ds != NULL) {
				dragged_folder = addrindex_ds_get_root_folder( ds );
				if (ds->type != ADDR_IF_JPILOT ||
				    ds->type != ADDR_IF_LDAP)
				    drag_context->action = GDK_ACTION_COPY;
			} else {
				dragged_folder = NULL;
				dragged_person = NULL;
				dragged_ab = NULL;
			}
		}
	} else {
		dragged_folder = NULL;
		dragged_person = NULL;
		dragged_ab = NULL;
	}
}

static gboolean addressbook_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  void            *data)
{
	gint row, column;
	GtkCTreeNode *node = NULL;
	gboolean acceptable = FALSE;
	gint height = addrbook.ctree->allocation.height;
	gint total_height = addrbook.ctree->requisition.height;
	GtkAdjustment *pos = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(addrbook.ctree_swin));
	gfloat vpos = pos->value;
	
	if (gtk_clist_get_selection_info
		(GTK_CLIST(widget), x - 24, y - 24, &row, &column)) {

		if (y > height - 24 && height + vpos < total_height)
			gtk_adjustment_set_value(pos, (vpos+5 > height ? height : vpos+5));

		if (y < 24 && y > 0)
			gtk_adjustment_set_value(pos, (vpos-5 < 0 ? 0 : vpos-5));

		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);

		if (node != NULL) {
			AddressObject *obj = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node );
			if( obj->type == ADDR_ITEM_FOLDER 
			|| obj->type == ADDR_ITEM_GROUP)
				acceptable = TRUE;
			else {
				AdapterDSource *ads = NULL;
				AddressDataSource *ds = NULL;
				ads = ADAPTER_DSOURCE(obj);
				if (ads == NULL ) return FALSE;
				ds = ads->dataSource;
				if (ds == NULL ) return FALSE;
				if (obj->type == ADDR_DATASOURCE
				&&  !ds->interface->externalQuery)
					acceptable = TRUE;

			}
		}
	}

	
	if (acceptable) {
		g_signal_handlers_block_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(addressbook_tree_selected), NULL);
		gtk_ctree_select(GTK_CTREE(widget), node);
		g_signal_handlers_unblock_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(addressbook_tree_selected), NULL);
		gdk_drag_status(context, 
					(context->actions == GDK_ACTION_COPY ?
					GDK_ACTION_COPY : GDK_ACTION_MOVE) , time);
	} else {
		gdk_drag_status(context, 0, time);
	}

	return acceptable;
}

static void addressbook_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     void           *data)
{
	if (addrbook.treeSelected) {
		g_signal_handlers_block_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(addressbook_tree_selected), NULL);
		gtk_ctree_select(GTK_CTREE(widget), addrbook.treeSelected);
		g_signal_handlers_unblock_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(addressbook_tree_selected), NULL);
	}
}

static void addressbook_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *drag_context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					void             *pdata)
{
	gint row, column;
	GtkCTreeNode *node;
	ItemFolder *afolder = NULL;
	ItemFolder *ofolder = NULL;
	ItemPerson *person = NULL;
	

	if (!strcmp(data->data, "Dummy_addr")) {
		AddressObject *obj = NULL;
		AdapterDSource *ads = NULL;
		AddressDataSource *ds = NULL;
		
		if (gtk_clist_get_selection_info
			(GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0) {
			return;
		}
	
		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		if( node ) 
			obj = gtk_ctree_node_get_row_data(GTK_CTREE(addrbook.ctree), node );
		if( obj == NULL ) 
			return;
			
		
		
		person = ( ItemPerson * )dragged_person;
		
		if (obj->type == ADDR_ITEM_FOLDER) {
			afolder = ADAPTER_FOLDER(obj)->itemFolder;

		} else if (obj->type == ADDR_DATASOURCE) {
			ads = ADAPTER_DSOURCE(obj);
			if( ads == NULL ) 
				return;
			ds = ads->dataSource;
			if( ds == NULL ||
			    ds->type == ADDR_IF_JPILOT || 
			    ds->type == ADDR_IF_LDAP) 
			    	return;		
			afolder = addrindex_ds_get_root_folder( ds );
			
		} else {
			return;
		}

		ofolder = dragged_folder;
		
		if (afolder && ofolder) {
			AddressBookFile *obook = dragged_ab;
			AddressBookFile *abook = addressbook_get_book_file_for_node(node);
			addritem_folder_remove_person(ofolder, person);
			addritem_folder_add_person(afolder, person);
			addressbook_list_select_clear();
			gtk_ctree_select( GTK_CTREE(addrbook.ctree), addrbook.opened);
							
			if (abook) {
				addrbook_set_dirty(abook, TRUE);
			}
			if (obook) {
				addrbook_set_dirty(obook, TRUE);
			}
			
			addressbook_export_to_file();
		}

		gtk_drag_finish(drag_context, TRUE, TRUE, time);
	}
}

/*
* End of Source.
*/

