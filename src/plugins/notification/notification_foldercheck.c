
/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2022 Holger Berndt and the Claws Mail Team
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

/* This code is based on foldersel.c in Claws Mail.
 * Some functions are only slightly modified, almost 1:1 copies from there. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

/* Basic definitions first */
#include "common/defs.h"

/* System includes */
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

/* Claws Mail includes */
#include "manage_window.h"
#include "folder.h"
#include "stock_pixmap.h"
#include "gtk/gtkutils.h"
#include "common/utils.h"
#include "common/prefs.h"
#include "common/xml.h"
#include "common/hooks.h"
#include "prefs_common.h"

/* local includes */
#include "notification_foldercheck.h"

/* enums and structures */
enum {
  FOLDERCHECK_FOLDERNAME,
  FOLDERCHECK_FOLDERITEM,
  FOLDERCHECK_PIXBUF,
  FOLDERCHECK_PIXBUF_OPEN,
  FOLDERCHECK_CHECK,
  N_FOLDERCHECK_COLUMNS
};

typedef struct {
  /* Data */
  gchar        *name;
  GSList       *list;
  GtkTreeStore *tree_store;
  /* Dialog box*/
  GtkWidget *window;
  GtkWidget *treeview;
  gboolean cancelled;
  gboolean finished;
  gboolean recursive;
} SpecificFolderArrayEntry;

/* variables with file scope */ 
static GdkPixbuf *folder_pixbuf;
static GdkPixbuf *folderopen_pixbuf;
static GdkPixbuf *foldernoselect_pixbuf;
static GdkPixbuf *foldernoselectopen_pixbuf;

static GArray *specific_folder_array;
static guint   specific_folder_array_size;

static gulong hook_folder_update;


/* defines */
#define FOLDERCHECK_ARRAY "notification_foldercheck.xml"
#define foldercheck_get_entry_from_id(id) \
  ((id) < specific_folder_array_size) ? \
  g_array_index(specific_folder_array,SpecificFolderArrayEntry*,(id)) : NULL

/* function prototypes */
static void folder_checked(guint);
static void foldercheck_create_window(SpecificFolderArrayEntry*);
static void foldercheck_destroy_window(SpecificFolderArrayEntry*);
static gint foldercheck_folder_name_compare(GtkTreeModel*, GtkTreeIter*,
					    GtkTreeIter*, gpointer);
static gboolean foldercheck_selected(GtkTreeSelection*,
				     GtkTreeModel*, GtkTreePath*,
				     gboolean, gpointer);

static gint delete_event(GtkWidget*, GdkEventAny*, gpointer);
static void foldercheck_ok(GtkButton*, gpointer);
static void foldercheck_cancel(GtkButton*, gpointer);
static void foldercheck_set_tree(SpecificFolderArrayEntry*);
static void foldercheck_insert_gnode_in_store(GtkTreeStore*, GNode*,
					      GtkTreeIter*);
static void foldercheck_append_item(GtkTreeStore*, FolderItem*,
				    GtkTreeIter*, GtkTreeIter*);
static void foldercheck_recursive_cb(GtkToggleButton*, gpointer);
static void folder_toggle_cb(GtkCellRendererToggle*, gchar*, gpointer);
static void folder_toggle_recurse_tree(GtkTreeStore*, GtkTreeIter*, gint,
				       gboolean);
static gboolean foldercheck_foreach_check(GtkTreeModel*, GtkTreePath*,
					  GtkTreeIter*, gpointer);
static gboolean foldercheck_foreach_update_to_list(GtkTreeModel*, GtkTreePath*,
						   GtkTreeIter*, gpointer);
static gchar *foldercheck_get_array_path(void);
static gboolean my_folder_update_hook(gpointer, gpointer);
static gboolean key_pressed(GtkWidget*, GdkEventKey*,gpointer);


/* Creates an entry in the specific_folder_array, and fills it with a new
 * SpecificFolderArrayEntry*. If specific_folder_array already has an entry
 * with the same name, return its ID. (The ID is the index in the array.) */
guint notification_register_folder_specific_list(gchar *node_name)
{
  SpecificFolderArrayEntry *entry;
  gint ii = 0;

  /* If array does not yet exist, create it. */
  if(!specific_folder_array) {
    specific_folder_array = g_array_new(FALSE, FALSE,
					sizeof(SpecificFolderArrayEntry*));
    specific_folder_array_size = 0;

    /* Register hook for folder update */
    /* "The hook is registered" is bound to "the array is allocated" */
    hook_folder_update = hooks_register_hook(FOLDER_UPDATE_HOOKLIST,
					     my_folder_update_hook, NULL);
    if(hook_folder_update == 0) {
      debug_print("Warning: Failed to register hook to folder update "
		  "hooklist. "
		  "Strange things can occur when deleting folders.\n");
    }
  }

  /* Check if we already have such a name. If so, return its id. */
  while(ii < specific_folder_array_size) {
    entry = g_array_index(specific_folder_array,SpecificFolderArrayEntry*,ii);
    if(entry) {
      if(!g_strcmp0(entry->name,node_name))
	return ii;
    }
    ii++;
  }

  /* Create an entry with the corresponding node name. */
  entry = g_new(SpecificFolderArrayEntry, 1);
  entry->name = g_strdup(node_name);
  entry->list = NULL;
  entry->window = NULL;
  entry->treeview = NULL;
  entry->cancelled = FALSE;
  entry->finished  = FALSE;
  entry->recursive = FALSE;
  entry->tree_store = gtk_tree_store_new(N_FOLDERCHECK_COLUMNS,
					 G_TYPE_STRING,
					 G_TYPE_POINTER,
					 GDK_TYPE_PIXBUF,
					 GDK_TYPE_PIXBUF,
					 G_TYPE_BOOLEAN);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(entry->tree_store),
				  FOLDERCHECK_FOLDERNAME,
				  foldercheck_folder_name_compare,
				  NULL, NULL);
  specific_folder_array = g_array_append_val(specific_folder_array, entry);
  return specific_folder_array_size++;
}

/* This function is called in plugin_done. It frees the whole
 * folder_specific_array with all its entries. */
void notification_free_folder_specific_array(void)
{
  guint ii;
  SpecificFolderArrayEntry *entry;

  for(ii = 0; ii < specific_folder_array_size; ii++) {
    entry = g_array_index(specific_folder_array,SpecificFolderArrayEntry*,ii);
    if(entry) {
      g_free(entry->name);
      if(entry->list)
	g_slist_free(entry->list);
      if(entry->tree_store)
	g_object_unref(G_OBJECT(entry->tree_store));
      g_free(entry);
    }
  }
  if(specific_folder_array) {
    /* Free array */
    g_array_free(specific_folder_array, TRUE);

    /* Unregister hook */
    hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, hook_folder_update);
  }
  specific_folder_array = NULL;
  specific_folder_array_size = 0;
}

/* Returns the list of the entry with the corresponding ID, or NULL if
 * no such element exists. */
GSList* notification_foldercheck_get_list(guint id)
{
  SpecificFolderArrayEntry *entry;
  
  entry = foldercheck_get_entry_from_id(id);
  if(entry) {
    return entry->list;
  }
  else
    return NULL;
}

/* Save selections in a common xml-file. Called when unloading the plugin.
 * This is analog to folder.h::folder_write_list. */
void notification_foldercheck_write_array(void)
{
  gchar *path;
  XMLTag *tag;
  XMLNode *xmlnode;
  GNode *rootnode;
  gint ii;
  PrefFile *pfile;

  /* Do nothing if foldercheck is not in use */
  if(specific_folder_array_size == 0)
    return;

  path = foldercheck_get_array_path();
  if((pfile = prefs_write_open(path)) == NULL) {
    debug_print("Notification plugin error: cannot open "
		"file " FOLDERCHECK_ARRAY " for writing\n");
    return;
  }

  /* XML declarations */
  xml_file_put_xml_decl(pfile->fp);

  /* Build up XML tree */
  
  /* root node */
  tag = xml_tag_new("foldercheckarray");
  xmlnode = xml_node_new(tag, NULL);
  rootnode = g_node_new(xmlnode);

  /* branch nodes */
  for(ii = 0; ii < specific_folder_array_size; ii++) {
    GNode *branchnode;
    GSList *walk;
    SpecificFolderArrayEntry *entry;
  
    entry = foldercheck_get_entry_from_id(ii);
    
    tag = xml_tag_new("branch");
    xml_tag_add_attr(tag, xml_attr_new("name",entry->name));
    xmlnode = xml_node_new(tag, NULL);
    branchnode = g_node_new(xmlnode);
    g_node_append(rootnode, branchnode);

    /* Write out the list as leaf nodes */
    for(walk = entry->list; walk != NULL; walk = g_slist_next(walk)) {
      gchar *identifier;
      GNode *node;
      FolderItem *item = (FolderItem*) walk->data;

      identifier = folder_item_get_identifier(item);

      tag = xml_tag_new("folderitem");
      xml_tag_add_attr(tag, xml_attr_new("identifier", identifier));
      g_free(identifier);
      xmlnode = xml_node_new(tag, NULL);
      node = g_node_new(xmlnode);
      g_node_append(branchnode, node);
    } /* for all list elements in branch node */

  } /* for all branch nodes */

  /* Actual writing and cleanup */
  xml_write_tree(rootnode, pfile->fp);

  if(prefs_file_close(pfile) < 0) {
    debug_print("Notification plugin error: failed to write "
		"file " FOLDERCHECK_ARRAY "\n");
  }

  /* Free XML tree */
  xml_free_tree(rootnode);
}

/* Read selections from a common xml-file. Called when loading the plugin.
 * Returns TRUE if data has been read, FALSE if no data is available
 * or an error occurred.
 * This is analog to folder.h::folder_read_list. */
gboolean notification_foldercheck_read_array(void)
{
  gchar *path;
  GNode *rootnode, *node, *branchnode;
  XMLNode *xmlnode;
  gboolean success = FALSE;

  path = foldercheck_get_array_path();
  if(!is_file_exist(path)) {
    path = NULL;
    return FALSE;
  }

  /* We don't do merging, so if the file existed, clear what we
     have stored in memory right now.. */
  notification_free_folder_specific_array();

  /* .. and evaluate the file */
  rootnode = xml_parse_file(path);
  path = NULL;
  if(!rootnode)
    return FALSE;

  xmlnode = rootnode->data;

  /* Check that root entry is "foldercheckarray" */
  if(g_strcmp0(xmlnode->tag->tag, "foldercheckarray") != 0) {
    g_warning("wrong foldercheck array file");
    xml_free_tree(rootnode);
    return FALSE;
  }

  /* Process branch entries */
  for(branchnode = rootnode->children; branchnode != NULL;
      branchnode = branchnode->next) {
    GList *list;
    guint id;
    SpecificFolderArrayEntry *entry = NULL;

    xmlnode = branchnode->data;
    if(g_strcmp0(xmlnode->tag->tag, "branch") != 0) {
      g_warning("tag name != \"branch\"");
      return FALSE;
    }

    /* Attributes of the branch nodes */
    list = xmlnode->tag->attr;
    for(; list != NULL; list = list->next) {
      XMLAttr *attr = list->data;

      if(attr && attr->name && attr->value &&  !g_strcmp0(attr->name, "name")) {
	id = notification_register_folder_specific_list(attr->value);
	entry = foldercheck_get_entry_from_id(id);
	/* We have found something */
	success = TRUE;
	break;
      }
    }
    if((list == NULL) || (entry == NULL)) {
      g_warning("did not find attribute \"name\" in tag \"branch\"");
      continue; /* with next branch */
    }

    /* Now descent into the children of the brach, which are the folderitems */
    for(node = branchnode->children; node != NULL; node = node->next) {
      FolderItem *item = NULL;

      /* These should all be leaves. */
      if(!G_NODE_IS_LEAF(node))
	g_warning("subnodes in \"branch\" nodes should all be leaves, "
		  "ignoring deeper subnodes");

      /* Check if tag is "folderitem" */
      xmlnode = node->data;
      if(g_strcmp0(xmlnode->tag->tag, "folderitem") != 0) {
	g_warning("tag name != \"folderitem\"");
	continue; /* to next node in branch */
      }

      /* Attributes of the leaf nodes */
      list = xmlnode->tag->attr;
      for(; list != NULL; list = list->next) {
	XMLAttr *attr = list->data;

	if(attr && attr->name && attr->value &&
	   !g_strcmp0(attr->name, "identifier")) {
	  item = folder_find_item_from_identifier(attr->value);
	  break;
	}
      }
      if((list == NULL) || (item == NULL)) {
	g_warning("did not find attribute \"identifier\" in tag "
		  "\"folderitem\"");
	continue; /* with next leaf node */
      }
      
      /* Store all FolderItems in the list */
      /* We started with a cleared array, so we don't need to check if
	 it's already in there. */
      entry->list = g_slist_prepend(entry->list, item);

    } /* for all subnodes in branch */

  } /* for all branches */
  return success;
}

/* Stolen from folder.c. Return value should NOT be freed. */
static gchar *foldercheck_get_array_path(void)
{
  static gchar *filename = NULL;

  if(!filename)
    filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   FOLDERCHECK_ARRAY, NULL);
  return filename;
}

/* Callback for selecting folders. If no selection dialog exists yet, create
 * one and initialize the selection. The recurse through the whole model, and
 * add all selected items to the list. */
static void folder_checked(guint id)
{
  SpecificFolderArrayEntry *entry;
  GSList *checked_list = NULL;

  entry = foldercheck_get_entry_from_id(id);

  /* Create window */
  foldercheck_create_window(entry);
  gtk_widget_show(entry->window);
  manage_window_set_transient(GTK_WINDOW(entry->window));

  entry->cancelled = entry->finished = FALSE;
  while(entry->finished == FALSE)
    gtk_main_iteration();
  
  foldercheck_destroy_window(entry);

  if(!entry->cancelled) {
    /* recurse through the whole model, add all selected items to the list */
    gtk_tree_model_foreach(GTK_TREE_MODEL(entry->tree_store),
			   foldercheck_foreach_check, &checked_list);

    if(entry->list) {
      g_slist_free(entry->list);
      entry->list = NULL;
    }
    entry->list = g_slist_copy(checked_list);
    g_slist_free(checked_list);
  }

  gtk_tree_store_clear(entry->tree_store);

  entry->cancelled = FALSE;
  entry->finished  = FALSE;
}

/* Create the window for selecting folders with checkboxes */
static void foldercheck_create_window(SpecificFolderArrayEntry *entry)
{
  GtkWidget *vbox;
  GtkWidget *scrolledwin;
  GtkWidget *confirm_area;
  GtkWidget *checkbox;
  GtkWidget *cancel_button;
  GtkWidget *ok_button;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  static GdkGeometry geometry;

  /* Create window */
  entry->window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "notification_foldercheck");
  gtk_window_set_title(GTK_WINDOW(entry->window), _("Select folder(s)"));
  gtk_container_set_border_width(GTK_CONTAINER(entry->window), 4);
  gtk_window_set_position(GTK_WINDOW(entry->window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal(GTK_WINDOW(entry->window), TRUE);
  gtk_window_set_resizable(GTK_WINDOW(entry->window), TRUE);
  g_signal_connect(G_OBJECT(entry->window), "delete_event",
		   G_CALLBACK(delete_event), entry);
  g_signal_connect(G_OBJECT(entry->window), "key_press_event",
      G_CALLBACK(key_pressed), entry);
  MANAGE_WINDOW_SIGNALS_CONNECT(entry->window);

  /* vbox */
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add(GTK_CONTAINER(entry->window), vbox);

  /* scrolled window */
  scrolledwin = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
				      GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

  /* pixbufs */
  if(!folder_pixbuf)
    stock_pixbuf_gdk(STOCK_PIXMAP_DIR_CLOSE,
		     &folder_pixbuf);
  if(!folderopen_pixbuf)
    stock_pixbuf_gdk(STOCK_PIXMAP_DIR_OPEN,
		     &folderopen_pixbuf);
  if(!foldernoselect_pixbuf)
    stock_pixbuf_gdk(STOCK_PIXMAP_DIR_NOSELECT_CLOSE,
		     &foldernoselect_pixbuf);
  if(!foldernoselectopen_pixbuf)
    stock_pixbuf_gdk(STOCK_PIXMAP_DIR_NOSELECT_OPEN,
		     &foldernoselectopen_pixbuf);

  /* Tree store */
  foldercheck_set_tree(entry);
  gtk_tree_model_foreach(GTK_TREE_MODEL(entry->tree_store),
			 foldercheck_foreach_update_to_list, entry);


  /* tree view */
  entry->treeview =
    gtk_tree_view_new_with_model(GTK_TREE_MODEL(entry->tree_store));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(entry->treeview), FALSE);
  gtk_tree_view_set_search_column(GTK_TREE_VIEW(entry->treeview),
				  FOLDERCHECK_FOLDERNAME);
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(entry->treeview),
				  prefs_common_get_prefs()->use_stripes_everywhere);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(entry->treeview), FALSE);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(entry->treeview));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
  gtk_tree_selection_set_select_function(selection, foldercheck_selected,
					 NULL, NULL);

  gtk_container_add(GTK_CONTAINER(scrolledwin), entry->treeview);

  /* --- column 1 --- */
  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column, "sel");
  gtk_tree_view_column_set_spacing(column, 2);
  
  /* checkbox */
  renderer = gtk_cell_renderer_toggle_new();
  g_object_set(renderer, "xalign", 0.0, NULL);
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  g_signal_connect(renderer, "toggled", G_CALLBACK(folder_toggle_cb),entry);
  gtk_tree_view_column_set_attributes(column, renderer,
				      "active", FOLDERCHECK_CHECK,NULL);

  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(entry->treeview), column);

  /* --- column 2 --- */
  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column, "Folder");
  gtk_tree_view_column_set_spacing(column, 2);

  /* pixbuf */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_set_attributes
    (column, renderer,
     "pixbuf", FOLDERCHECK_PIXBUF,
     "pixbuf-expander-open", FOLDERCHECK_PIXBUF_OPEN,
     "pixbuf-expander-closed", FOLDERCHECK_PIXBUF,
     NULL);

  /* text */
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_set_attributes(column, renderer,
				      "text", FOLDERCHECK_FOLDERNAME,
				      NULL);

  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(entry->treeview), column);

  /* recursive */
  checkbox = gtk_check_button_new_with_label( _("select recursively"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), FALSE);
  g_signal_connect(G_OBJECT(checkbox), "toggled",
		   G_CALLBACK(foldercheck_recursive_cb), entry);
  gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 10);

  gtkut_stock_button_set_create(&confirm_area,
				&cancel_button, NULL, _("_Cancel"),
				&ok_button, NULL, _("_OK"),
				NULL, NULL, NULL);
  gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default(ok_button);

  g_signal_connect(G_OBJECT(ok_button), "clicked",
		   G_CALLBACK(foldercheck_ok), entry);
  g_signal_connect(G_OBJECT(cancel_button), "clicked",
		   G_CALLBACK(foldercheck_cancel), entry);

  if(!geometry.min_height) {
    geometry.min_width = 360;
    geometry.min_height = 360;
  }

  gtk_window_set_geometry_hints(GTK_WINDOW(entry->window), NULL, &geometry,
				GDK_HINT_MIN_SIZE);

  gtk_tree_view_expand_all(GTK_TREE_VIEW(entry->treeview));

  gtk_widget_show_all(vbox);
}

static void foldercheck_destroy_window(SpecificFolderArrayEntry *entry)
{
  gtk_widget_destroy(entry->window);
  entry->window = NULL;
  entry->treeview = NULL;
  entry->recursive = FALSE;
}

/* Handler for the delete event of the windows for selecting folders */
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
  foldercheck_cancel(NULL, data);
  return TRUE;
}

/* sortable_set_sort_func */
static gint foldercheck_folder_name_compare(GtkTreeModel *model,
					    GtkTreeIter *a, GtkTreeIter *b,
					    gpointer context)
{
  gchar *str_a = NULL, *str_b = NULL;
  gint val = 0;
  FolderItem *item_a = NULL, *item_b = NULL;
  GtkTreeIter parent;

  gtk_tree_model_get(model, a, FOLDERCHECK_FOLDERITEM, &item_a, -1);
  gtk_tree_model_get(model, b, FOLDERCHECK_FOLDERITEM, &item_b, -1);

  /* no sort for root folder */
  if (!gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), &parent, a))
    return 0;

  /* if both a and b are special folders, sort them according to
   * their types (which is in-order). Note that this assumes that
   * there are no multiple folders of a special type. */
  if (item_a->stype != F_NORMAL && item_b->stype != F_NORMAL)
    return item_a->stype - item_b->stype;

  /* if b is normal folder, and a is not, b is smaller (ends up
   * lower in the list) */
  if (item_a->stype != F_NORMAL && item_b->stype == F_NORMAL)
    return item_b->stype - item_a->stype;

  /* if b is special folder, and a is not, b is larger (ends up
   * higher in the list) */
  if (item_a->stype == F_NORMAL && item_b->stype != F_NORMAL)
    return item_b->stype - item_a->stype;

  /* XXX g_utf8_collate_key() comparisons may speed things
   * up when having large lists of folders */
  gtk_tree_model_get(model, a, FOLDERCHECK_FOLDERNAME, &str_a, -1);
  gtk_tree_model_get(model, b, FOLDERCHECK_FOLDERNAME, &str_b, -1);

  /* otherwise just compare the folder names */
  val = g_utf8_collate(str_a, str_b);
  
  g_free(str_a);
  g_free(str_b);

  return val;
}

/* select_function of the gtk tree selection */
static gboolean foldercheck_selected(GtkTreeSelection *selection,
				     GtkTreeModel *model, GtkTreePath *path,
				     gboolean currently_selected,gpointer data)
{
  GtkTreeIter iter;
  FolderItem *item = NULL;

  if (currently_selected)
    return TRUE;

  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
    return TRUE;

  gtk_tree_model_get(model, &iter, FOLDERCHECK_FOLDERITEM, &item, -1);

  return TRUE;
}

/* Callback for the OK-button of the folderselection dialog */
static void foldercheck_ok(GtkButton *button, gpointer data)
{
  SpecificFolderArrayEntry *entry = (SpecificFolderArrayEntry*) data;

  entry->finished = TRUE;
}

/* Callback for the Cancel-button of the folderselection dialog. Gets also
 * called on a delete-event of the folderselection window. */
static void foldercheck_cancel(GtkButton *button, gpointer data)
{
  SpecificFolderArrayEntry *entry = (SpecificFolderArrayEntry*) data;

  entry->cancelled = TRUE;
  entry->finished  = TRUE;
}

/* Set tree of the tree-store. This includes getting the folder tree and
 * storing it */
static void foldercheck_set_tree(SpecificFolderArrayEntry *entry)
{
  Folder *folder;
  GList *list;

  for(list = folder_get_list(); list != NULL; list = list->next) {
    folder = FOLDER(list->data);

    if(folder == NULL) {
      debug_print("Notification plugin::foldercheck_set_tree(): Found a NULL folder.\n");
      continue;
    }

    /* Only regard built-in folders, because folders from plugins (such as RSS, calendar,
     * or plugin-provided mailbox storage systems like Maildir or MBox) may vanish
     * without letting us know. */
    switch(folder->klass->type) {
    case F_MH:
    case F_IMAP:
    case F_NEWS:
      foldercheck_insert_gnode_in_store(entry->tree_store, folder->node, NULL);
      break;
    default:
      break;
    }
  }

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(entry->tree_store),
				       FOLDERCHECK_FOLDERNAME,
				       GTK_SORT_ASCENDING);

  if(GTK_IS_TREE_VIEW(entry->treeview))
    gtk_tree_view_expand_all(GTK_TREE_VIEW(entry->treeview));
}

/* Helper function for foldercheck_set_tree */
static void foldercheck_insert_gnode_in_store(GtkTreeStore *store, GNode *node,
					      GtkTreeIter *parent)
{
  FolderItem *item;
  GtkTreeIter child;
  GNode *iter;

  g_return_if_fail(node != NULL);
  g_return_if_fail(node->data != NULL);
  g_return_if_fail(store != NULL);

  item = FOLDER_ITEM(node->data);
  foldercheck_append_item(store, item, &child, parent);

  /* insert its children (this node as parent) */
  for(iter = node->children; iter != NULL; iter = iter->next)
    foldercheck_insert_gnode_in_store(store, iter, &child);
}

/* Helper function for foldercheck_insert_gnode_in_store */
static void foldercheck_append_item(GtkTreeStore *store, FolderItem *item,
				    GtkTreeIter *iter, GtkTreeIter *parent)
{
  gchar *name, *tmpname;
  GdkPixbuf *pixbuf, *pixbuf_open;

  name = tmpname = folder_item_get_name(item);

  if (item->stype != F_NORMAL && FOLDER_IS_LOCAL(item->folder)) {
    switch (item->stype) {
    case F_INBOX:
      if (!g_strcmp0(item->name, INBOX_DIR))
	name = "Inbox";
      break;
    case F_OUTBOX:
      if (!g_strcmp0(item->name, OUTBOX_DIR))
	name = "Sent";
      break;
    case F_QUEUE:
      if (!g_strcmp0(item->name, QUEUE_DIR))
	name = "Queue";
      break;
    case F_TRASH:
      if (!g_strcmp0(item->name, TRASH_DIR))
	name = "Trash";
      break;
    case F_DRAFT:
      if (!g_strcmp0(item->name, DRAFT_DIR))
	name = "Drafts";
      break;
    default:
      break;
    }
  }

  if (folder_has_parent_of_type(item, F_QUEUE) && item->total_msgs > 0) {
    name = g_strdup_printf("%s (%d)", name, item->total_msgs);
  } else if (item->unread_msgs > 0) {
    name = g_strdup_printf("%s (%d)", name, item->unread_msgs);
  } else
    name = g_strdup(name);
  
  pixbuf = item->no_select ? foldernoselect_pixbuf : folder_pixbuf;
  pixbuf_open =
    item->no_select ? foldernoselectopen_pixbuf : folderopen_pixbuf;
  
  /* insert this node */
  gtk_tree_store_append(store, iter, parent);
  gtk_tree_store_set(store, iter,
		     FOLDERCHECK_FOLDERNAME, name,
		     FOLDERCHECK_FOLDERITEM, item,
		     FOLDERCHECK_PIXBUF, pixbuf,
		     FOLDERCHECK_PIXBUF_OPEN, pixbuf_open,
		     -1);

  g_free(tmpname);
}

/* Callback of the recursive-checkbox */
static void foldercheck_recursive_cb(GtkToggleButton *button, gpointer data)
{
  SpecificFolderArrayEntry *entry = (SpecificFolderArrayEntry*) data;

  entry->recursive = gtk_toggle_button_get_active(button);
}

/* Callback of the checkboxes corresponding to the folders. Obeys
 * the "recursive" selection. */
static void folder_toggle_cb(GtkCellRendererToggle *cell_renderer,
			     gchar *path_str, gpointer data)
{
  gboolean toggle_item;
  GtkTreeIter iter;
  SpecificFolderArrayEntry *entry = (SpecificFolderArrayEntry*) data;
  GtkTreePath *path = gtk_tree_path_new_from_string(path_str);

  gtk_tree_model_get_iter(GTK_TREE_MODEL(entry->tree_store), &iter, path);
  gtk_tree_path_free(path);
  gtk_tree_model_get(GTK_TREE_MODEL(entry->tree_store), &iter,
		     FOLDERCHECK_CHECK, &toggle_item, -1);
  toggle_item = !toggle_item;

  if(!entry->recursive)
    gtk_tree_store_set(entry->tree_store, &iter,
		       FOLDERCHECK_CHECK, toggle_item, -1);
  else {
    GtkTreeIter child;
    gtk_tree_store_set(entry->tree_store, &iter,
		       FOLDERCHECK_CHECK, toggle_item, -1);
    if(gtk_tree_model_iter_children(GTK_TREE_MODEL(entry->tree_store),
				    &child, &iter))
      folder_toggle_recurse_tree(entry->tree_store,&child,
				 FOLDERCHECK_CHECK,toggle_item);
  }

  while(gtk_events_pending())
    gtk_main_iteration();
}

/* Helper function for folder_toggle_cb */
/* This function calls itself recurively */
static void folder_toggle_recurse_tree(GtkTreeStore *tree_store,
				       GtkTreeIter *iterp, gint column,
				       gboolean toggle_item)
{
  GtkTreeIter iter = *iterp;
  GtkTreeIter next;

  /* set the value of this iter */
  gtk_tree_store_set(tree_store, &iter, column, toggle_item, -1);

  /* do the same for the first child */
  if(gtk_tree_model_iter_children(GTK_TREE_MODEL(tree_store),&next, &iter))
    folder_toggle_recurse_tree(tree_store,&next,
			       FOLDERCHECK_CHECK, toggle_item);

  /* do the same for the next sibling */
  if(gtk_tree_model_iter_next(GTK_TREE_MODEL(tree_store), &iter))
    folder_toggle_recurse_tree(tree_store, &iter,
			       FOLDERCHECK_CHECK, toggle_item);
}

/* Helper function to be used with a foreach statement of the model. Checks
 * if a node is checked, and adds it to a list if it is. data us a (GSList**)
 * where the result it to be stored */
static gboolean foldercheck_foreach_check(GtkTreeModel *model,
					  GtkTreePath *path,
					  GtkTreeIter *iter, gpointer data)
{
  gboolean toggled_item;
  GSList **list = (GSList**) data;

  gtk_tree_model_get(model, iter, FOLDERCHECK_CHECK, &toggled_item, -1);

  if(toggled_item) {
    FolderItem *item;
    gtk_tree_model_get(model, iter, FOLDERCHECK_FOLDERITEM, &item, -1);
    *list = g_slist_prepend(*list, item);
  }

  return FALSE;
}

/* Helper function to be used with a foreach statement of the model. Checks
 * if a node is checked, and adds it to a list if it is. data us a (GSList**)
 * where the result it to be stored */
static gboolean foldercheck_foreach_update_to_list(GtkTreeModel *model,
						   GtkTreePath *path,
						   GtkTreeIter *iter,
						   gpointer data)
{
  gchar *ident_tree, *ident_list;
  FolderItem *item;
  GSList *walk;
  gboolean toggle_item = FALSE;
  SpecificFolderArrayEntry *entry = (SpecificFolderArrayEntry*) data;

  gtk_tree_model_get(model, iter, FOLDERCHECK_FOLDERITEM, &item, -1);

  if(item->path != NULL)
    ident_tree = folder_item_get_identifier(item);
  else
    return FALSE;

  for(walk = entry->list; walk != NULL; walk = g_slist_next(walk)) {
    FolderItem *list_item = (FolderItem*) walk->data;
    ident_list = folder_item_get_identifier(list_item);
    if(!g_strcmp0(ident_list,ident_tree)) {
      toggle_item = TRUE;
      g_free(ident_list);
      break;
    }
    g_free(ident_list);
  }
  g_free(ident_tree);

  gtk_tree_store_set(entry->tree_store, iter, FOLDERCHECK_CHECK,
		     toggle_item, -1);

  return FALSE;
}


/* Callback for the folder selection dialog. Basically a wrapper around 
 * folder_checked that first resolves the name to an ID first. */
void notification_foldercheck_sel_folders_cb(GtkButton *button, gpointer data)
{
  guint id;
  gchar *name = (gchar*) data;

  id = notification_register_folder_specific_list(name);

  folder_checked(id);
}

static gboolean my_folder_update_hook(gpointer source, gpointer data)
{
  FolderUpdateData *hookdata = (FolderUpdateData*) source;

  if(hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM) {
    gint ii;
    SpecificFolderArrayEntry *entry;
    FolderItem *item = hookdata->item;

    /* If that folder is in anywhere in the array, cut it out. */
    for(ii = 0; ii < specific_folder_array_size; ii++) {
      entry = foldercheck_get_entry_from_id(ii);
      entry->list = g_slist_remove(entry->list, item);
    } /* for all entries in the array */
  } /* A FolderItem was deleted */

  return FALSE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  if(event && (event->keyval == GDK_KEY_Escape)) {
    foldercheck_cancel(NULL, data);
    return TRUE;
  }
  return FALSE;
}
