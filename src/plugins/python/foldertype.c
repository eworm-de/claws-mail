/* Python plugin for Claws-Mail
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "foldertype.h"
#include "messageinfotype.h"

#include <structmember.h>


typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *path;
    PyObject *mailbox_name;
    FolderItem *folderitem;
} clawsmail_FolderObject;


static void Folder_dealloc(clawsmail_FolderObject* self)
{
  Py_XDECREF(self->name);
  Py_XDECREF(self->path);
  Py_XDECREF(self->mailbox_name);
  self->ob_type->tp_free((PyObject*)self);
}

#define FOLDERITEM_STRING_TO_PYTHON_FOLDER_MEMBER(self,fis, pms)    \
  do {                                                              \
    if(fis) {                                                       \
      PyObject *str;                                                \
      str = PyString_FromString(fis);                               \
      if(str) {                                                     \
        int retval;                                                 \
        retval = PyObject_SetAttrString((PyObject*)self, pms, str); \
        Py_DECREF(str);                                             \
        if(retval == -1)                                            \
          goto err;                                                 \
      }                                                             \
    }                                                               \
  } while(0)

static int Folder_init(clawsmail_FolderObject *self, PyObject *args, PyObject *kwds)
{
  const char *ss = NULL;
  FolderItem *folderitem = NULL;
  char create = 0;

  /* optional constructor argument: folderitem id string */
  if(!PyArg_ParseTuple(args, "|sb", &ss, &create))
    return -1;
  
  Py_INCREF(Py_None);
  self->name = Py_None;
  
  Py_INCREF(Py_None);
  self->path = Py_None;

  Py_INCREF(Py_None);
  self->mailbox_name = Py_None;

  if(ss) {
    if(create == 0) {
      folderitem = folder_find_item_from_identifier(ss);
      if(!folderitem) {
        PyErr_SetString(PyExc_ValueError, "A folder with that path does not exist, and the create parameter was False.");
        return -1;
      }
    }
    else {
      folderitem = folder_get_item_from_identifier(ss);
      if(!folderitem) {
        PyErr_SetString(PyExc_IOError, "A folder with that path does not exist, and could not be created.");
        return -1;
      }
    }
  }

  if(folderitem) {
    FOLDERITEM_STRING_TO_PYTHON_FOLDER_MEMBER(self, folderitem->name, "name");
    FOLDERITEM_STRING_TO_PYTHON_FOLDER_MEMBER(self, folderitem->path, "path");
    FOLDERITEM_STRING_TO_PYTHON_FOLDER_MEMBER(self, folderitem->folder->name, "mailbox_name");
    self->folderitem = folderitem;
  }

  return 0;

 err:
  return -1;
}

static PyObject* Folder_str(PyObject *self)
{
  PyObject *str;
  str = PyString_FromString("Folder: ");
  PyString_ConcatAndDel(&str, PyObject_GetAttrString(self, "name"));
  return str;
}

static PyObject* Folder_get_identifier(clawsmail_FolderObject *self, PyObject *args)
{
  PyObject *obj;
  gchar *id;
  if(!self->folderitem)
    return NULL;
  id = folder_item_get_identifier(self->folderitem);
  obj = Py_BuildValue("s", id);
  g_free(id);
  return obj;
}

static PyObject* Folder_get_messages(clawsmail_FolderObject *self, PyObject *args)
{
  GSList *msglist, *walk;
  PyObject *retval;
  Py_ssize_t pos;

  if(!self->folderitem)
    return NULL;

  msglist = folder_item_get_msg_list(self->folderitem);
  retval = PyTuple_New(g_slist_length(msglist));
  if(!retval) {
    procmsg_msg_list_free(msglist);
    Py_INCREF(Py_None);
    return Py_None;
  }
  
  for(pos = 0, walk = msglist; walk; walk = walk->next, ++pos) {
    PyObject *msg;
    msg = clawsmail_messageinfo_new(walk->data);
    PyTuple_SET_ITEM(retval, pos, msg);
  }
  procmsg_msg_list_free(msglist);
  
  return retval;
}

static PyMethodDef Folder_methods[] = {
    {"get_identifier", (PyCFunction)Folder_get_identifier, METH_NOARGS,
     "get_identifier() - get identifier\n"
     "\n"
     "Get identifier for folder as a string (e.g. #mh/foo/bar)."},
    {"get_messages", (PyCFunction)Folder_get_messages, METH_NOARGS,
     "get_messages() - get a tuple of messages in folder\n"
     "\n"
     "Get a tuple of MessageInfos for the folder."},
    {NULL}
};

static PyMemberDef Folder_members[] = {
  {"name", T_OBJECT_EX, offsetof(clawsmail_FolderObject, name), 0,
   "name - name of folder"},
  
  {"path", T_OBJECT_EX, offsetof(clawsmail_FolderObject, path), 0,
   "path - path of folder"},
  
  {"mailbox_name", T_OBJECT_EX, offsetof(clawsmail_FolderObject, mailbox_name), 0,
   "mailbox_name - name of the corresponding mailbox"},

  {NULL}
};

static PyTypeObject clawsmail_FolderType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size*/
    "clawsmail.Folder",        /* tp_name*/
    sizeof(clawsmail_FolderObject), /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)Folder_dealloc, /* tp_dealloc*/
    0,                         /* tp_print*/
    0,                         /* tp_getattr*/
    0,                         /* tp_setattr*/
    0,                         /* tp_compare*/
    0,                         /* tp_repr*/
    0,                         /* tp_as_number*/
    0,                         /* tp_as_sequence*/
    0,                         /* tp_as_mapping*/
    0,                         /* tp_hash */
    0,                         /* tp_call*/
    Folder_str,                /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "Folder objects.\n\n"      /* tp_doc */
    "The __init__ function takes two optional arguments:\n"
    "folder = Folder(identifier, [create_if_not_existing=False])\n"
    "The identifier is an id string (e.g. '#mh/Mail/foo/bar'),"
    "create_if_not_existing is a boolean expression.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Folder_methods,            /* tp_methods */
    Folder_members,            /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Folder_init,     /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

PyMODINIT_FUNC initfolder(PyObject *module)
{
    clawsmail_FolderType.tp_new = PyType_GenericNew;
    if(PyType_Ready(&clawsmail_FolderType) < 0)
        return;

    Py_INCREF(&clawsmail_FolderType);
    PyModule_AddObject(module, "Folder", (PyObject*)&clawsmail_FolderType);
}

PyObject* clawsmail_folder_new(FolderItem *folderitem)
{
  clawsmail_FolderObject *ff;
  PyObject *arglist;
  gchar *id;

  if(!folderitem)
    return NULL;

  id = folder_item_get_identifier(folderitem);
  arglist = Py_BuildValue("(s)", id);
  g_free(id);
  ff = (clawsmail_FolderObject*) PyObject_CallObject((PyObject*) &clawsmail_FolderType, arglist);
  Py_DECREF(arglist);
  return (PyObject*)ff;
}

FolderItem* clawsmail_folder_get_item(PyObject *self)
{
  return ((clawsmail_FolderObject*)self)->folderitem;
}

PyTypeObject* clawsmail_folder_get_type_object()
{
  return &clawsmail_FolderType;
}

gboolean clawsmail_folder_check(PyObject *self)
{
  return (PyObject_TypeCheck(self, &clawsmail_FolderType) != 0);
}
