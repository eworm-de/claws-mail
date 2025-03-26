/* Python plugin for Claws Mail
 * Copyright (C) 2009 Holger Berndt
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
#  include "claws-features.h"
#endif

#include "composewindowtype.h"
#include "accounttype.h"

#include "clawsmailmodule.h"
#include "foldertype.h"
#include "messageinfotype.h"

#include "mainwindow.h"
#include "account.h"
#include "summaryview.h"
#include "gtk/combobox.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <structmember.h>

#include <string.h>

typedef struct {
    PyObject_HEAD
    PyObject *ui_manager;
    PyObject *text;
    PyObject *replyinfo;
    PyObject *fwdinfo;
    Compose *compose;
} clawsmail_ComposeWindowObject;

static void ComposeWindow_dealloc(clawsmail_ComposeWindowObject* self)
{
  Py_XDECREF(self->ui_manager);
  Py_XDECREF(self->text);
  Py_XDECREF(self->replyinfo);
  Py_XDECREF(self->fwdinfo);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static void flush_gtk_queue(void)
{
  while(gtk_events_pending())
    gtk_main_iteration();
}

static void store_py_object(PyObject **target, PyObject *obj)
{
  Py_XDECREF(*target);
  if(obj)
  {
    Py_INCREF(obj);
    *target = obj;
  }
  else {
    Py_INCREF(Py_None);
    *target = Py_None;
  }
}

static void composewindow_set_compose(clawsmail_ComposeWindowObject *self, Compose *compose)
{
  self->compose = compose;

  store_py_object(&(self->ui_manager), get_gobj_from_address(compose->ui_manager));
  store_py_object(&(self->text), get_gobj_from_address(compose->text));

  store_py_object(&(self->replyinfo), clawsmail_messageinfo_new(compose->replyinfo));
  store_py_object(&(self->fwdinfo), clawsmail_messageinfo_new(compose->fwdinfo));
}

static int ComposeWindow_init(clawsmail_ComposeWindowObject *self, PyObject *args, PyObject *kwds)
{
  MainWindow *mainwin;
  PrefsAccount *ac = NULL;
  FolderItem *item;
  GList* list;
  GList* cur;
  gboolean did_find_compose;
  Compose *compose = NULL;
  const char *ss;
  unsigned char open_window;
  /* if __open_window is set to 0/False,
   * composewindow_set_compose must be called before this object is valid */
  static char *kwlist[] = {"address", "__open_window", NULL};

  ss = NULL;
  open_window = 1;
  PyArg_ParseTupleAndKeywords(args, kwds, "|sb", kwlist, &ss, &open_window);

  if(open_window) {
    mainwin = mainwindow_get_mainwindow();
    item = mainwin->summaryview->folder_item;
    did_find_compose = FALSE;

    if(ss) {
      ac = account_find_from_address(ss, FALSE);
      if (ac && ac->protocol != A_NNTP) {
        compose = compose_new_with_folderitem(ac, item, NULL);
        did_find_compose = TRUE;
      }
    }
    if(!did_find_compose) {
      if (item) {
        ac = account_find_from_item(item);
        if (ac && ac->protocol != A_NNTP) {
          compose = compose_new_with_folderitem(ac, item, NULL);
          did_find_compose = TRUE;
        }
      }

      /* use current account */
      if (!did_find_compose && cur_account && (cur_account->protocol != A_NNTP)) {
        compose = compose_new_with_folderitem(cur_account, item, NULL);
        did_find_compose = TRUE;
      }

      if(!did_find_compose) {
        /* just get the first one */
        list = account_get_list();
        for (cur = list ; cur != NULL ; cur = g_list_next(cur)) {
          ac = (PrefsAccount *) cur->data;
          if (ac->protocol != A_NNTP) {
            compose = compose_new_with_folderitem(ac, item, NULL);
            did_find_compose = TRUE;
          }
        }
      }
    }

    if(!did_find_compose)
      return -1;

    composewindow_set_compose(self, compose);
    gtk_widget_show_all(compose->window);
    flush_gtk_queue();
  }
  return 0;
}

/* this is here because wrapping GTK_EDITABLEs in PyGTK is buggy */
static PyObject* get_python_object_from_gtk_entry(GtkWidget *entry)
{
  return Py_BuildValue("s", gtk_entry_get_text(GTK_ENTRY(entry)));
}

static PyObject* set_gtk_entry_from_python_object(GtkWidget *entry, PyObject *args)
{
  const char *ss;

  if(!PyArg_ParseTuple(args, "s", &ss))
    return NULL;

  gtk_entry_set_text(GTK_ENTRY(entry), ss);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_get_subject(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  return get_python_object_from_gtk_entry(self->compose->subject_entry);
}

static PyObject* ComposeWindow_set_subject(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  PyObject *ret;
  ret = set_gtk_entry_from_python_object(self->compose->subject_entry, args);
  flush_gtk_queue();
  return ret;
}

static PyObject* ComposeWindow_get_from(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  return get_python_object_from_gtk_entry(self->compose->from_name);
}

static PyObject* ComposeWindow_set_from(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  PyObject *ret;
  ret = set_gtk_entry_from_python_object(self->compose->from_name, args);
  flush_gtk_queue();
  return ret;
}

static PyObject* ComposeWindow_add_To(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  const char *ss;

  if(!PyArg_ParseTuple(args, "s", &ss))
    return NULL;

  compose_entry_append(self->compose, ss, COMPOSE_TO, PREF_NONE);

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_add_Cc(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  const char *ss;

  if(!PyArg_ParseTuple(args, "s", &ss))
    return NULL;

  compose_entry_append(self->compose, ss, COMPOSE_CC, PREF_NONE);

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_add_Bcc(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  const char *ss;

  if(!PyArg_ParseTuple(args, "s", &ss))
    return NULL;

  compose_entry_append(self->compose, ss, COMPOSE_BCC, PREF_NONE);

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_attach(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  PyObject *olist;
  Py_ssize_t size, iEl;
  GList *list = NULL;

  if(!PyArg_ParseTuple(args, "O!", &PyList_Type, &olist))
    return NULL;

  size = PyList_Size(olist);
  for(iEl = 0; iEl < size; iEl++) {
    char *ss;
    PyObject *element = PyList_GET_ITEM(olist, iEl);

    if(!element)
      continue;

    Py_INCREF(element);
    if(!PyArg_Parse(element, "s", &ss)) {
      Py_DECREF(element);
      if(list)
        g_list_free(list);
      return NULL;
    }
    list = g_list_prepend(list, ss);
    Py_DECREF(element);
  }

  compose_attach_from_list(self->compose, list, FALSE);
  g_list_free(list);

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_get_header_list(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  GSList *walk;
  PyObject *retval;

  retval = Py_BuildValue("[]");
  for(walk = self->compose->header_list; walk; walk = walk->next) {
    ComposeHeaderEntry *headerentry = walk->data;
    const gchar *header;
    const gchar *text;

    header = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(headerentry->combo))), 0, -1);
    text = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));

    if(text && strcmp("", text)) {
      PyObject *ee;
      int ok;

      ee = Py_BuildValue("(ss)", header, text);
      ok = PyList_Append(retval, ee);
      Py_DECREF(ee);
      if(ok == -1) {
        Py_DECREF(retval);
        return NULL;
      }
    }
  }
  return retval;
}

static PyObject* ComposeWindow_set_header_list(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  gint num;
  GSList *walk;
  PyObject *headerlist;
  Py_ssize_t headerlistsize;
  Py_ssize_t iEl;

  if(!PyArg_ParseTuple(args, "O!", &PyList_Type, &headerlist))
    return NULL;

  headerlistsize = PyList_Size(headerlist);
  num = g_slist_length(self->compose->header_list);

  /* check correctness of argument before deleting old content */
  for(iEl = 0; iEl < headerlistsize; iEl++) {
    PyObject *element;
    PyObject *headerfield;
    PyObject *headercontent;

    /* check that we got a list of tuples with two elements */
    element = PyList_GET_ITEM(headerlist, iEl);
    if(!element || !PyObject_TypeCheck(element, &PyTuple_Type) || (PyTuple_Size(element) != 2)) {
      PyErr_SetString(PyExc_LookupError, "Argument to set_header_list() must be a list of tuples with two bytestrings");
      return NULL;
    }

    /* check that the two tuple elements are strings */
    headerfield = PyTuple_GetItem(element, 0);
    headercontent = PyTuple_GetItem(element, 1);
    if(!headerfield || !headercontent
        || !PyObject_TypeCheck(headerfield, &PyBytes_Type) || !PyObject_TypeCheck(headercontent, &PyBytes_Type)) {
      PyErr_SetString(PyExc_LookupError, "Argument to set_header_list() must be a list of tuples with two bytestrings");
      return NULL;
    }
  }

  /* delete old headers */
  for(walk = self->compose->header_list; walk; walk = walk->next) {
    ComposeHeaderEntry *headerentry = walk->data;
    gtk_entry_set_text(GTK_ENTRY(headerentry->entry), "");
  }

  /* if given header list is bigger than current header list, add dummy values */
  while(num < headerlistsize) {
    compose_entry_append(self->compose, "dummy1dummy2dummy3", COMPOSE_TO, PREF_NONE);
    num++;
  }

  /* set headers to new values */
  for(iEl = 0; iEl < headerlistsize; iEl++) {
    PyObject *element;
    PyObject *headerfield;
    PyObject *headercontent;
    ComposeHeaderEntry *headerentry;
    GtkEditable *editable;
    gint pos;

    element = PyList_GET_ITEM(headerlist, iEl);
    headerfield = PyTuple_GetItem(element, 0);
    headercontent = PyTuple_GetItem(element, 1);

    headerentry = g_slist_nth_data(self->compose->header_list, iEl);

    /* set header field */
    editable = GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(headerentry->combo)));
    gtk_editable_delete_text(editable, 0, -1);
    gtk_editable_insert_text(editable, PyBytes_AsString(headerfield), -1, &pos);

    /* set header content */
    gtk_entry_set_text(GTK_ENTRY(headerentry->entry), PyBytes_AsString(headercontent));
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_add_header(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  const char *header;
  const char *text;
  gint num;

  if(!PyArg_ParseTuple(args, "ss", &header, &text))
    return NULL;

  /* add a dummy, and modify it then */
  compose_entry_append(self->compose, "dummy1dummy2dummy3", COMPOSE_TO, PREF_NONE);
  num = g_slist_length(self->compose->header_list);
  if(num > 1) {
    ComposeHeaderEntry *headerentry;
    headerentry = g_slist_nth_data(self->compose->header_list, num-2);
    if(headerentry) {
      GtkEditable *editable;
      gint pos;
      gtk_entry_set_text(GTK_ENTRY(headerentry->entry), text);
      editable = GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(headerentry->combo)));
      gtk_editable_delete_text(editable, 0, -1);
      gtk_editable_insert_text(editable, header, -1, &pos);
    }
  }

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_get_account_selection(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  if(GTK_IS_COMBO_BOX(self->compose->account_combo))
    return get_gobj_from_address(self->compose->account_combo);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_save_message_to(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  PyObject *arg;

  if(!PyArg_ParseTuple(args, "O", &arg))
    return NULL;

  if(PyBytes_Check(arg)) {
    GtkEditable *editable;
    gint pos;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->compose->savemsg_checkbtn), TRUE);

    editable = GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(self->compose->savemsg_combo)));
    gtk_editable_delete_text(editable, 0, -1);
    gtk_editable_insert_text(editable, PyBytes_AsString(arg), -1, &pos);
  }
  else if(clawsmail_folder_check(arg)) {
    GtkEditable *editable;
    gint pos;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->compose->savemsg_checkbtn), TRUE);

    editable = GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(self->compose->savemsg_combo)));
    gtk_editable_delete_text(editable, 0, -1);
    gtk_editable_insert_text(editable, folder_item_get_identifier(clawsmail_folder_get_item(arg)), -1, &pos);
  }
  else if (arg == Py_None){
    /* turn off checkbutton */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->compose->savemsg_checkbtn), FALSE);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "function takes exactly one argument which may be a folder object, a string, or None");
    return NULL;
  }

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* ComposeWindow_set_modified(clawsmail_ComposeWindowObject *self, PyObject *args)
{
  char modified = 0;
  gboolean old_modified;

  if(!PyArg_ParseTuple(args, "b", &modified))
    return NULL;

  old_modified = self->compose->modified;

  self->compose->modified = (modified != 0);

  /* If the modified state changed, rewrite window title.
   * This partly duplicates functionality in compose.c::compose_set_title().
   * While it's nice to not have to modify Claws Mail for this to work,
   * it would be cleaner to export that function in Claws Mail. */
  if((strcmp(gtk_window_get_title(GTK_WINDOW(self->compose->window)), _("Compose message")) != 0) &&
      (old_modified != self->compose->modified)) {
      gchar *str;
      gchar *edited;
      gchar *subject;

      edited = self->compose->modified  ? _(" [Edited]") : "";
      subject = gtk_editable_get_chars(GTK_EDITABLE(self->compose->subject_entry), 0, -1);
      if(subject && strlen(subject))
        str = g_strdup_printf(_("%s - Compose message%s"),
            subject, edited);
      else
        str = g_strdup_printf(_("[no subject] - Compose message%s"), edited);
      gtk_window_set_title(GTK_WINDOW(self->compose->window), str);
      g_free(str);
      g_free(subject);
  }

  flush_gtk_queue();

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* get_account(clawsmail_ComposeWindowObject *self, void *closure)
{
  if(self->compose->account) {
    return clawsmail_account_new(self->compose->account);
  }
  Py_RETURN_NONE;
}

static int set_account(clawsmail_ComposeWindowObject *self, PyObject *value, void *closure)
{
  PrefsAccount *target_account;

  if(value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Cannot delete 'account' attribute");
    return -1;
  }

  if(!clawsmail_account_check(value)) {
    PyErr_SetString(PyExc_TypeError, "ComposeWindow.account: Can only assign an account");
    return -1;
  }


  target_account = clawsmail_account_get_account(value);
  if(!target_account) {
    PyErr_SetString(PyExc_TypeError, "Account value broken");
    return -1;
  }

  if(!self->compose || !self->compose->account_combo) {
    PyErr_SetString(PyExc_RuntimeError, "ComposeWindow: Cannot access account");
    return -1;
  }

  combobox_select_by_data(GTK_COMBO_BOX(self->compose->account_combo), target_account->account_id);

  return 0;
}



static PyMethodDef ComposeWindow_methods[] = {
    {"set_subject", (PyCFunction)ComposeWindow_set_subject, METH_VARARGS,
     "set_subject(text) - set subject to text\n"
     "\n"
     "Set the subject to text. text must be a string."},

    {"get_subject", (PyCFunction)ComposeWindow_get_subject, METH_NOARGS,
     "get_subject() - get subject\n"
     "\n"
     "Get a string of the current subject entry."},

    {"set_from", (PyCFunction)ComposeWindow_set_from, METH_VARARGS,
     "set_from(text) - set From header entry to text\n"
     "\n"
     "Set the From header entry to text. text must be a string.\n"
     "Beware: No sanity checking is performed."},

    {"get_from", (PyCFunction)ComposeWindow_get_from, METH_NOARGS,
     "get_from - get From header entry\n"
     "\n"
     "Get a string of the current From header entry."},

    {"add_To",  (PyCFunction)ComposeWindow_add_To,  METH_VARARGS,
     "add_To(text) - append another To header with text\n"
     "\n"
     "Add another header line with the combo box set to To:, and the\n"
     "content set to text."},

    {"add_Cc",  (PyCFunction)ComposeWindow_add_Cc,  METH_VARARGS,
     "add_Cc(text) - append another Cc header with text\n"
     "\n"
     "Add another header line with the combo box set to Cc:, and the\n"
     "content set to text."},

    {"add_Bcc", (PyCFunction)ComposeWindow_add_Bcc, METH_VARARGS,
     "add_Bcc(text) - append another Bcc header with text\n"
     "\n"
     "Add another header line with the combo box set to Bcc:, and the\n"
     "content set to text."},

    {"add_header", (PyCFunction)ComposeWindow_add_header, METH_VARARGS,
     "add_header(headername, text) - add a custom header\n"
     "\n"
     "Adds a custom header with the header set to headername, and the\n"
     "contents set to text."},

    {"get_header_list", (PyCFunction)ComposeWindow_get_header_list, METH_NOARGS,
     "get_header_list() - get list of headers\n"
     "\n"
     "Gets a list of headers that are currently defined in the compose window.\n"
     "The return value is a list of tuples, where the first tuple element is\n"
     "the header name (entry in the combo box) and the second element is the contents."},

    {"set_header_list", (PyCFunction)ComposeWindow_set_header_list, METH_VARARGS,
     "set_header_list(list_of_header_value_pairs) - set list of headers\n"
     "\n"
     "Sets the list of headers that are currently defined in the compose window.\n"
     "This function overwrites the current setting.\n\n"
     "The parameter is expected to be a list of header name and value tuples,\n"
     "analogous to the return value of the get_header_list() function."},

    {"attach",  (PyCFunction)ComposeWindow_attach, METH_VARARGS,
     "attach(filenames) - attach a list of files\n"
     "\n"
     "Attach files to the mail. The filenames argument is a list of\n"
     "string of the filenames that are being attached."},

    {"get_account_selection", (PyCFunction)ComposeWindow_get_account_selection, METH_NOARGS,
     "get_account_selection() - get account selection widget\n"
     "\n"
     "Returns the account selection combo box as a gtk.ComboBox"},

    {"save_message_to", (PyCFunction)ComposeWindow_save_message_to, METH_VARARGS,
     "save_message_to(folder) - save message to folder id\n"
     "\n"
     "Set the folder where the sent message will be saved to. folder may be\n"
     "a Folder, a string of the folder identifier (e.g. #mh/foo/bar), or\n"
     "None is which case the message will not be saved at all."},

     {"set_modified", (PyCFunction)ComposeWindow_set_modified, METH_VARARGS,
     "set_modified(bool) - set or unset modification marker of compose window\n"
     "\n"
     "Set or unset the modification marker of the compose window. This marker determines\n"
     "for example whether you get a confirmation dialog when closing the compose window\n"
     "or not.\n"
     "In the usual case, Claws Mail keeps track of the modification status itself.\n"
     "However, there are cases when it might be desirable to overwrite the marker,\n"
     "for example because a compose_any script modifies the body or subject which\n"
     "can be regarded compose window preprocessing and should not trigger a confirmation\n"
     "dialog on close like a manual edit."},

    {NULL}
};

static PyMemberDef ComposeWindow_members[] = {
    {"ui_manager", T_OBJECT_EX, offsetof(clawsmail_ComposeWindowObject, ui_manager), 0,
     "ui_manager - the gtk.UIManager of the compose window"},

    {"text", T_OBJECT_EX, offsetof(clawsmail_ComposeWindowObject, text), 0,
     "text - the gtk.TextView widget of the message body"},

    {"replyinfo", T_OBJECT_EX, offsetof(clawsmail_ComposeWindowObject, replyinfo), 0,
     "replyinfo - The MessageInfo object of the message that is being replied to, or None"},

    {"forwardinfo", T_OBJECT_EX, offsetof(clawsmail_ComposeWindowObject, fwdinfo), 0,
     "forwardinfo - The MessageInfo object of the message that is being forwarded, or None"},

    {NULL}
};

static PyGetSetDef ComposeWindow_getset[] = {
    {"account", (getter)get_account, (setter)set_account,
      "account - the account corresponding to this compose window", NULL},

    {NULL}
};

static PyTypeObject clawsmail_ComposeWindowType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "clawsmail.ComposeWindow", /*tp_name*/
    sizeof(clawsmail_ComposeWindowObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)ComposeWindow_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    /* tp_doc */
    "ComposeWindow objects. Optional argument to constructor: sender account address. ",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    ComposeWindow_methods,     /* tp_methods */
    ComposeWindow_members,     /* tp_members */
    ComposeWindow_getset,      /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)ComposeWindow_init, /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
    0,                         /* tp_free */
	0,                         /* tp_is_gc */
	0,                         /* tp_bases */
	0,                         /* tp_mro */
	0,                         /* tp_cache */
	0,                         /* tp_subclasses */
	0,                         /* tp_weaklist */
	0,                         /* tp_del */
#if ((PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION >= 6) || \
     (PY_MAJOR_VERSION == 3))
    0,                         /* tp_version_tag */
#endif
#if (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4)
    0,                         /* tp_finalize */
#endif
};

gboolean cmpy_add_composewindow(PyObject *module)
{
  clawsmail_ComposeWindowType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_ComposeWindowType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_ComposeWindowType);
  return (PyModule_AddObject(module, "ComposeWindow", (PyObject*)&clawsmail_ComposeWindowType) == 0);
}

PyObject* clawsmail_compose_new(PyObject *module, Compose *compose)
{
  PyObject *class, *dict;
  PyObject *self, *args, *kw;

  if(!compose) {
    Py_INCREF(Py_None);
    return Py_None;
  }

  dict = PyModule_GetDict(module);
  class = PyDict_GetItemString(dict, "ComposeWindow");
  args = Py_BuildValue("()");
  kw = Py_BuildValue("{s:b}", "__open_window", 0);
  self = PyObject_Call(class, args, kw);
  Py_DECREF(args);
  Py_DECREF(kw);
  composewindow_set_compose((clawsmail_ComposeWindowObject*)self, compose);
  return self;
}
