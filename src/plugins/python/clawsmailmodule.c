/* Python plugin for Claws-Mail
 * Copyright (C) 2009-2012 Holger Berndt
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

#include "clawsmailmodule.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "nodetype.h"
#include "composewindowtype.h"
#include "foldertype.h"
#include "messageinfotype.h"

#include <pygobject.h>
#include <pygtk/pygtk.h>

#include "main.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "quicksearch.h"
#include "toolbar.h"
#include "prefs_common.h"
#include "common/tags.h"

#include <glib.h>

static PyObject *cm_module = NULL;

PyObject* get_gobj_from_address(gpointer addr)
{
  GObject *obj;

  if (!G_IS_OBJECT(addr))
      return NULL;

  obj = G_OBJECT(addr);

  if (!obj)
      return NULL;

  return pygobject_new(obj);
}

static PyObject* private_wrap_gobj(PyObject *self, PyObject *args)
{
    void *addr;

    if (!PyArg_ParseTuple(args, "l", &addr))
        return NULL;

    return get_gobj_from_address(addr);
}

static PyObject *get_mainwindow_action_group(PyObject *self, PyObject *args)
{
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();
  if(mainwin)
    return get_gobj_from_address(mainwin->action_group);
  else
    return NULL;
}

static PyObject *get_mainwindow_ui_manager(PyObject *self, PyObject *args)
{
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();
  if(mainwin)
    return get_gobj_from_address(mainwin->ui_manager);
  else
    return NULL;
}

static PyObject *get_folderview_selected_folder(PyObject *self, PyObject *args)
{
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();
  if(mainwin && mainwin->folderview) {
    FolderItem *item;
    item = folderview_get_selected_item(mainwin->folderview);
    if(item)
      return clawsmail_folder_new(item);
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *folderview_select_folder(PyObject *self, PyObject *args)
{
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();
  if(mainwin && mainwin->folderview) {
    FolderItem *item;
    PyObject *folder;
    folder = PyTuple_GetItem(args, 0);
    if(!folder)
      return NULL;
    Py_INCREF(folder);
    item = clawsmail_folder_get_item(folder);
    Py_DECREF(folder);
    if(item)
      folderview_select(mainwin->folderview, item);
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static gboolean setup_folderitem_node(GNode *item_node, GNode *item_parent, PyObject **pyparent)
{
  PyObject *pynode, *children;
  int retval, n_children, i_child;
  PyObject *folder;

  /* create a python node for the folderitem node */
  pynode = clawsmail_node_new(cm_module);
  if(!pynode)
    return FALSE;

  /* store Folder in pynode */
  folder = clawsmail_folder_new(item_node->data);
  retval = PyObject_SetAttrString(pynode, "data", folder);
  Py_DECREF(folder);
  if(retval == -1) {
    Py_DECREF(pynode);
    return FALSE;
  }

  if(pyparent && *pyparent) {
    /* add this node to the parent's childs */
    children = PyObject_GetAttrString(*pyparent, "children");
    retval = PyList_Append(children, pynode);
    Py_DECREF(children);

    if(retval == -1) {
      Py_DECREF(pynode);
      return FALSE;
    }
  }
  else if(pyparent) {
    *pyparent = pynode;
    Py_INCREF(pynode);
  }

  /* call this function recursively for all children of the new node */
  n_children = g_node_n_children(item_node);
  for(i_child = 0; i_child < n_children; i_child++) {
    if(!setup_folderitem_node(g_node_nth_child(item_node, i_child), item_node, &pynode)) {
      Py_DECREF(pynode);
      return FALSE;
    }
  }

  Py_DECREF(pynode);
  return TRUE;
}

static PyObject* get_folder_tree_from_account_name(const char *str)
{
  PyObject *result;
  GList *walk;

  result = Py_BuildValue("[]");
  if(!result)
    return NULL;

  for(walk = folder_get_list(); walk; walk = walk->next) {
    Folder *folder = walk->data;
    if((!str || !g_strcmp0(str, folder->name)) && folder->node) {
      PyObject *root;
      int n_children, i_child, retval;

      /* create root nodes */
      root = clawsmail_node_new(cm_module);
      if(!root) {
        Py_DECREF(result);
        return NULL;
      }

      n_children = g_node_n_children(folder->node);
      for(i_child = 0; i_child < n_children; i_child++) {
        if(!setup_folderitem_node(g_node_nth_child(folder->node, i_child), folder->node, &root)) {
          Py_DECREF(root);
          Py_DECREF(result);
          return NULL;
        }
      }
      retval = PyList_Append(result, root);
      Py_DECREF(root);
      if(retval == -1) {
        Py_DECREF(result);
        return NULL;
      }
    }
  }
  return result;
}

static PyObject* get_folder_tree_from_folderitem(FolderItem *item)
{
  PyObject *result;
  GList *walk;

  for(walk = folder_get_list(); walk; walk = walk->next) {
    Folder *folder = walk->data;
    if(folder->node) {
      GNode *root_node;

      root_node = g_node_find(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
      if(!root_node)
        continue;

      result = NULL;
      if(!setup_folderitem_node(root_node, NULL, &result))
        return NULL;
      else
        return result;
    }
  }

  PyErr_SetString(PyExc_LookupError, "Folder not found");
  return NULL;
}

static PyObject* get_folder_tree(PyObject *self, PyObject *args)
{
  PyObject *arg;
  PyObject *result;
  int retval;

  Py_INCREF(Py_None);
  arg = Py_None;
  retval = PyArg_ParseTuple(args, "|O", &arg);
  Py_DECREF(Py_None);
  if(!retval)
    return NULL;

  /* calling possibilities:
   * nothing, the mailbox name in a string, a Folder object */

  /* no arguments: build up a list of folder trees */
  if(PyTuple_Size(args) == 0) {
    result = get_folder_tree_from_account_name(NULL);
  }
  else if(PyString_Check(arg)){
    const char *str;
    str = PyString_AsString(arg);
    if(!str)
      return NULL;

    result = get_folder_tree_from_account_name(str);
  }
  else if(PyObject_TypeCheck(arg, clawsmail_folder_get_type_object())) {
    result = get_folder_tree_from_folderitem(clawsmail_folder_get_item(arg));
  }
  else {
    PyErr_SetString(PyExc_TypeError, "Parameter must be nothing, a mailbox string or a Folder object.");
    return NULL;
  }

  return result;
}

static PyObject* quicksearch_search(PyObject *self, PyObject *args)
{
  const char *string;
  int searchtype;
  QuickSearch *qs;
  MainWindow *mainwin;

  /* must be given exactly one argument, which is a string */
  searchtype = prefs_common.summary_quicksearch_type;
  if(!PyArg_ParseTuple(args, "s|i", &string, &searchtype))
    return NULL;

  mainwin = mainwindow_get_mainwindow();
  if(!mainwin || !mainwin->summaryview || !mainwin->summaryview->quicksearch) {
    PyErr_SetString(PyExc_LookupError, "Quicksearch not found");
    return NULL;
  }

  qs = mainwin->summaryview->quicksearch;
  quicksearch_set(qs, searchtype, string);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* quicksearch_clear(PyObject *self, PyObject *args)
{
  QuickSearch *qs;
  MainWindow *mainwin;

  mainwin = mainwindow_get_mainwindow();
  if(!mainwin || !mainwin->summaryview || !mainwin->summaryview->quicksearch) {
    PyErr_SetString(PyExc_LookupError, "Quicksearch not found");
    return NULL;
  }

  qs = mainwin->summaryview->quicksearch;
  quicksearch_set(qs, prefs_common.summary_quicksearch_type, "");

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* summaryview_select_messages(PyObject *self, PyObject *args)
{
  PyObject *olist;
  MainWindow *mainwin;
  Py_ssize_t size, iEl;
  GSList *msginfos;

  mainwin = mainwindow_get_mainwindow();
  if(!mainwin || !mainwin->summaryview) {
    PyErr_SetString(PyExc_LookupError, "SummaryView not found");
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "O!", &PyList_Type, &olist)) {
    PyErr_SetString(PyExc_LookupError, "Argument must be a list of MessageInfo objects.");
    return NULL;
  }

  msginfos = NULL;
  size = PyList_Size(olist);
  for(iEl = 0; iEl < size; iEl++) {
    PyObject *element = PyList_GET_ITEM(olist, iEl);

    if(!element || !PyObject_TypeCheck(element, clawsmail_messageinfo_get_type_object())) {
      PyErr_SetString(PyExc_LookupError, "Argument must be a list of MessageInfo objects.");
      return NULL;
    }

    msginfos = g_slist_prepend(msginfos, clawsmail_messageinfo_get_msginfo(element));
  }

  summary_unselect_all(mainwin->summaryview);
  summary_select_by_msg_list(mainwin->summaryview, msginfos);
  g_slist_free(msginfos);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* get_summaryview_selected_message_list(PyObject *self, PyObject *args)
{
  MainWindow *mainwin;
  GSList *list, *walk;
  PyObject *result;

  mainwin = mainwindow_get_mainwindow();
  if(!mainwin || !mainwin->summaryview) {
    PyErr_SetString(PyExc_LookupError, "SummaryView not found");
    return NULL;
  }

  result = Py_BuildValue("[]");
  if(!result)
    return NULL;

  list = summary_get_selected_msg_list(mainwin->summaryview);
  for(walk = list; walk; walk = walk->next) {
    PyObject *msg;
    msg = clawsmail_messageinfo_new(walk->data);
    if(PyList_Append(result, msg) == -1) {
      Py_DECREF(result);
      return NULL;
    }
  }
  g_slist_free(list);

  return result;
}

static PyObject* is_exiting(PyObject *self, PyObject *args)
{
  if(claws_is_exiting())
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

static PyObject* get_tags(PyObject *self, PyObject *args)
{
  Py_ssize_t num_tags;
  PyObject *tags_tuple;
  GSList *tags_list;

  tags_list = tags_get_list();
  num_tags = g_slist_length(tags_list);

  tags_tuple = PyTuple_New(num_tags);
  if(tags_tuple != NULL) {
    Py_ssize_t iTag;
    PyObject *tag_object;
    GSList *walk;

    iTag = 0;
    for(walk = tags_list; walk; walk = walk->next) {
      tag_object = Py_BuildValue("s", tags_get_tag(GPOINTER_TO_INT(walk->data)));
      if(tag_object == NULL) {
        Py_DECREF(tags_tuple);
        return NULL;
      }
      PyTuple_SET_ITEM(tags_tuple, iTag++, tag_object);
    }
  }

  g_slist_free(tags_list);

  return tags_tuple;
}

static PyObject* make_sure_tag_exists(PyObject *self, PyObject *args)
{
  int retval;
  const char *tag_str;

  retval = PyArg_ParseTuple(args, "s", &tag_str);
  if(!retval)
    return NULL;

  if(IS_NOT_RESERVED_TAG(tag_str) == FALSE) {
    /* tag name is reserved, raise KeyError */
    PyErr_SetString(PyExc_ValueError, "Tag name is reserved");
    return NULL;
  }

  tags_add_tag(tag_str);

  Py_RETURN_NONE;
}

static PyObject* delete_tag(PyObject *self, PyObject *args)
{
  int retval;
  const char *tag_str;
  gint tag_id;
  MainWindow *mainwin;

  retval = PyArg_ParseTuple(args, "s", &tag_str);
  if(!retval)
    return NULL;

  tag_id = tags_get_id_for_str(tag_str);
  if(tag_id == -1) {
    PyErr_SetString(PyExc_KeyError, "Tag does not exist");
    return NULL;
  }

  tags_remove_tag(tag_id);

  /* update display */
  mainwin = mainwindow_get_mainwindow();
  if(mainwin)
    summary_redisplay_msg(mainwin->summaryview);

  Py_RETURN_NONE;
}


static PyObject* rename_tag(PyObject *self, PyObject *args)
{
  int retval;
  const char *old_tag_str;
  const char *new_tag_str;
  gint tag_id;
  MainWindow *mainwin;

  retval = PyArg_ParseTuple(args, "ss", &old_tag_str, &new_tag_str);
  if(!retval)
    return NULL;

  if((IS_NOT_RESERVED_TAG(new_tag_str) == FALSE) ||(IS_NOT_RESERVED_TAG(old_tag_str) == FALSE)) {
    PyErr_SetString(PyExc_ValueError, "Tag name is reserved");
    return NULL;
  }

  tag_id = tags_get_id_for_str(old_tag_str);
  if(tag_id == -1) {
    PyErr_SetString(PyExc_KeyError, "Tag does not exist");
    return NULL;
  }

  tags_update_tag(tag_id, new_tag_str);

  /* update display */
  mainwin = mainwindow_get_mainwindow();
  if(mainwin)
    summary_redisplay_msg(mainwin->summaryview);

  Py_RETURN_NONE;
}

static gboolean get_message_list_for_move_or_copy(PyObject *messagelist, PyObject *folder, GSList **list)
{
  Py_ssize_t size, iEl;
  FolderItem *folderitem;
  
  *list = NULL;

  folderitem = clawsmail_folder_get_item(folder);
  if(!folderitem) {
    PyErr_SetString(PyExc_LookupError, "Brokern Folder object.");
    return FALSE;
  }

  size = PyList_Size(messagelist);
  for(iEl = 0; iEl < size; iEl++) {
    PyObject *element = PyList_GET_ITEM(messagelist, iEl);
    MsgInfo *msginfo;

    if(!element || !PyObject_TypeCheck(element, clawsmail_messageinfo_get_type_object())) {
      PyErr_SetString(PyExc_TypeError, "Argument must be a list of MessageInfo objects.");
      return FALSE;
    }
    
    msginfo = clawsmail_messageinfo_get_msginfo(element);
    if(!msginfo) {
      PyErr_SetString(PyExc_LookupError, "Broken MessageInfo object.");
      return FALSE;
    }
   
    procmsg_msginfo_set_to_folder(msginfo, folderitem);
    *list = g_slist_prepend(*list, msginfo);
  }
 
  return TRUE;
}

static PyObject* move_or_copy_messages(PyObject *self, PyObject *args, gboolean move)
{
  PyObject *messagelist;
  PyObject *folder;
  int retval;
  GSList *list = NULL;
  
  retval = PyArg_ParseTuple(args, "O!O!",
    &PyList_Type, &messagelist,
    clawsmail_folder_get_type_object(), &folder);
  if(!retval )
    return NULL;  

  folder_item_update_freeze();
  
  if(!get_message_list_for_move_or_copy(messagelist, folder, &list))
    goto err;
  
  if(move)   
    procmsg_move_messages(list);
  else
    procmsg_copy_messages(list);
      
  folder_item_update_thaw();
  g_slist_free(list);
  Py_RETURN_NONE;

err:
  folder_item_update_thaw();
  g_slist_free(list);
  return NULL;
}

static PyObject* move_messages(PyObject *self, PyObject *args)
{
  return move_or_copy_messages(self, args, TRUE);
}


static PyObject* copy_messages(PyObject *self, PyObject *args)
{
  return move_or_copy_messages(self, args, FALSE);
}

static PyMethodDef ClawsMailMethods[] = {
    /* public */
    {"get_mainwindow_action_group",  get_mainwindow_action_group, METH_NOARGS,
     "get_mainwindow_action_group() - get action group of main window menu\n"
     "\n"
     "Returns the gtk.ActionGroup for the main window."},

    {"get_mainwindow_ui_manager",  get_mainwindow_ui_manager, METH_NOARGS, 
     "get_mainwindow_ui_manager() - get ui manager of main window\n"
     "\n"
     "Returns the gtk.UIManager for the main window."},

    {"get_folder_tree",  get_folder_tree, METH_VARARGS,
     "get_folder_tree([root]) - get a folder tree\n"
     "\n"
     "Without arguments, get a list of folder trees for all mailboxes.\n"
     "\n"
     "If the optional root argument is a string, it is supposed to be a\n"
     "mailbox name. The function then returns a tree of folders of that mailbox.\n"
     "\n"
     "If the optional root argument is a clawsmail.Folder, the function\n"
     "returns a tree of subfolders with the given folder as root element.\n"
     "\n"
     "In any case, a tree consists of elements of the type clawsmail.Node."},

    {"get_folderview_selected_folder",  get_folderview_selected_folder, METH_NOARGS,
     "get_folderview_selected_folder() - get selected folder in folderview\n"
     "\n"
     "Returns the currently selected folder as a clawsmail.Folder."},
    {"folderview_select_folder",  folderview_select_folder, METH_VARARGS,
     "folderview_select_folder(folder) - select folder in folderview\n"
     "\n"
     "Takes an argument of type clawsmail.Folder, and selects the corresponding folder."},

    {"quicksearch_search", quicksearch_search, METH_VARARGS,
     "quicksearch_search(string [, type]) - perform a quicksearch\n"
     "\n"
     "Perform a quicksearch of the given string. The optional type argument can be\n"
     "one of clawsmail.QUICK_SEARCH_SUBJECT, clawsmail.QUICK_SEARCH_FROM, clawsmail.QUICK_SEARCH_TO,\n"
     "clawsmail.QUICK_SEARCH_EXTENDED, clawsmail.QUICK_SEARCH_MIXED, or clawsmail.QUICK_SEARCH_TAG.\n"
     "If it is omitted, the current selection is used. The string argument has to be a valid search\n"
     "string corresponding to the type."},

    {"quicksearch_clear", quicksearch_clear, METH_NOARGS,
     "quicksearch_clear() - clear the quicksearch"},

    {"get_summaryview_selected_message_list", get_summaryview_selected_message_list, METH_NOARGS,
     "get_summaryview_selected_message_list() - get selected message list\n"
     "\n"
     "Get a list of clawsmail.MessageInfo objects of the current selection."},

    {"summaryview_select_messages", summaryview_select_messages, METH_VARARGS,
     "summaryview_select_messages(message_list) - select a list of messages in the summary view\n"
     "\n"
     "Select a list of clawsmail.MessageInfo objects in the summary view."},

    {"is_exiting", is_exiting, METH_NOARGS,
     "is_exiting() - test whether Claws Mail is currently exiting\n"
     "\n"
     "Returns True if Claws Mail is currently exiting. The most common usage for this is to skip\n"
     "unnecessary cleanup tasks in a shutdown script when Claws Mail is exiting anyways. If the Python\n"
     "plugin is explicitly unloaded, the shutdown script will still be called, but this function will\n"
     "return False."},

    {"move_messages", move_messages, METH_VARARGS,
     "move_messages(message_list, target_folder) - move a list of messages to a target folder\n"
     "\n"
     "Move a list of clawsmail.MessageInfo objects to a target folder.\n"
     "The target_folder argument has to be a clawsmail.Folder object."},

    {"copy_messages", copy_messages, METH_VARARGS,
     "copy_messages(message_list, target_folder) - copy a list of messages to a target folder\n"
     "\n"
     "Copy a list of clawsmail.MessageInfo objects to a target folder.\n"
     "The target_folder argument has to be a clawsmail.Folder object."},

    {"get_tags", get_tags, METH_NOARGS,
     "get_tags() - get a tuple of all tags that Claws Mail knows about\n"
     "\n"
     "Get a tuple of strings representing all tags that are defined in Claws Mail."},

    {"make_sure_tag_exists", make_sure_tag_exists, METH_VARARGS,
     "make_sure_tag_exists(tag) - make sure that the specified tag exists\n"
     "\n"
     "This function creates the given tag if it does not exist yet.\n"
     "It is not an error if the tag already exists. In this case, this function does nothing.\n"
     "However, if a reserved tag name is chosen, a ValueError exception is raised."},

    {"delete_tag", delete_tag, METH_VARARGS,
     "delete_tag(tag) - delete a tag\n"
     "\n"
     "This function deletes an existing tag.\n"
     "Raises a KeyError exception if the tag does not exist."},

    {"rename_tag", rename_tag, METH_VARARGS,
     "rename_tag(old_tag, new_tag) - rename tag old_tag to new_tag\n"
     "\n"
     "This function renames an existing tag.\n"
     "Raises a KeyError exception if the tag does not exist.\n"
     "Raises a ValueError exception if the old or new tag name is a reserved name."},

     /* private */
    {"__gobj", private_wrap_gobj, METH_VARARGS,
     "__gobj(ptr) - transforms a C GObject pointer into a PyGObject\n"
     "\n"
     "For internal usage only."},

    {NULL, NULL, 0, NULL}
};

static void initmiscstuff(PyObject *module)
{
  PyObject *dict;
  PyObject *res;
  const char *cmd =
      "QUICK_SEARCH_SUBJECT = 0\n"
      "QUICK_SEARCH_FROM = 1\n"
      "QUICK_SEARCH_TO = 2\n"
      "QUICK_SEARCH_EXTENDED = 3\n"
      "QUICK_SEARCH_MIXED = 4\n"
      "QUICK_SEARCH_TAG = 5\n"
      "\n";
  dict = PyModule_GetDict(module);
  res = PyRun_String(cmd, Py_file_input, dict, dict);
  Py_XDECREF(res);
}


void claws_mail_python_init(void)
{
  if (!Py_IsInitialized())
      Py_Initialize();

  /* create module */
  cm_module = Py_InitModule3("clawsmail", ClawsMailMethods,
      "This module can be used to access some of Claws Mail's data structures\n"
      "in order to extend or modify the user interface or automate repetitive tasks.\n"
      "\n"
      "Whenever possible, the interface works with standard GTK+ widgets\n"
      "via the PyGTK bindings, so you can refer to the GTK+ / PyGTK documentation\n"
      "to find out about all possible options.\n"
      "\n"
      "The interface to Claws Mail in this module is extended on a 'as-needed' basis.\n"
      "If you're missing something specific, try contacting the author.");

  /* initialize classes */
  initnode(cm_module);
  initcomposewindow(cm_module);
  initfolder(cm_module);
  initmessageinfo(cm_module);

  /* initialize misc things */
  initmiscstuff(cm_module);

  PyRun_SimpleString("import clawsmail\n");
  PyRun_SimpleString("clawsmail.compose_window = None\n");
}


void put_composewindow_into_module(Compose *compose)
{
  PyObject *pycompose;

  pycompose = clawsmail_compose_new(cm_module, compose);
  PyObject_SetAttrString(cm_module, "compose_window", pycompose);
  Py_DECREF(pycompose);
}
