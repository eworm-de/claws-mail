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
#include "claws-features.h"
#endif

#include "foldertype.h"
#include "folderpropertiestype.h"
#include "messageinfotype.h"
#include "mailboxtype.h"

#include <glib/gi18n.h>

#include <structmember.h>


typedef struct {
    PyObject_HEAD
    PyObject *properties;
    FolderItem *folderitem;
} clawsmail_FolderObject;


static void Folder_dealloc(clawsmail_FolderObject* self)
{
  Py_XDECREF(self->properties);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static int Folder_init(clawsmail_FolderObject *self, PyObject *args, PyObject *kwds)
{
  const char *ss = NULL;
  FolderItem *folderitem = NULL;
  char create = 0;

  /* optional constructor argument: folderitem id string */
  if(!PyArg_ParseTuple(args, "|sb", &ss, &create))
    return -1;

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

  self->folderitem = folderitem;
  if(folderitem)
    self->properties = clawsmail_folderproperties_new(folderitem->prefs);
  else {
    Py_INCREF(Py_None);
    self->properties = Py_None;
  }

  return 0;
}

static PyObject* Folder_str(clawsmail_FolderObject *self)
{
  return PyUnicode_FromFormat("Folder: %s", self->folderitem->name);
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

static PyObject* get_name(clawsmail_FolderObject *self, void *closure)
{
  if(self->folderitem && self->folderitem->name)
    return PyUnicode_FromString(self->folderitem->name);
  Py_RETURN_NONE;
}

static PyObject* get_mailbox_name(clawsmail_FolderObject *self, void *closure)
{
  if(self->folderitem && self->folderitem->folder && self->folderitem->folder->name)
    return PyUnicode_FromString(self->folderitem->folder->name);
  Py_RETURN_NONE;
}

static PyObject* get_mailbox(clawsmail_FolderObject *self, void *closure)
{
  if(self->folderitem && self->folderitem->folder)
    return clawsmail_mailbox_new(self->folderitem->folder);
  Py_RETURN_NONE;
}


static PyObject* get_identifier(clawsmail_FolderObject *self, void *closure)
{
  if(self->folderitem) {
    gchar *id;
    id = folder_item_get_identifier(self->folderitem);
    if(id) {
      PyObject *retval;
      retval = PyUnicode_FromString(id);
      g_free(id);
      return retval;
    }
  }
  Py_RETURN_NONE;
}

static PyObject* get_path(clawsmail_FolderObject *self, void *closure)
{
  if(self->folderitem) {
    gchar *path;
    path = folder_item_get_path(self->folderitem);
    if(path) {
      PyObject *retval;
      retval = PyUnicode_FromString(path);
      g_free(path);
      return retval;
    }
  }
  Py_RETURN_NONE;
}


static PyObject* get_properties(clawsmail_FolderObject *self, void *closure)
{
  Py_INCREF(self->properties);
  return self->properties;
}

static PyObject* get_num_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->total_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_new_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->new_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_unread_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->unread_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_marked_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->marked_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_locked_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->locked_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_unread_marked_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->unreadmarked_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_ignored_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->ignored_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_watched_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->watched_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_replied_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->replied_msgs);
  Py_RETURN_NONE;
}

static PyObject* get_num_forwarded_messages(clawsmail_FolderObject *self, void *closure)
{
  if(self && self->folderitem)
    return PyLong_FromLong(self->folderitem->forwarded_msgs);
  Py_RETURN_NONE;
}

static PyMethodDef Folder_methods[] = {
    {"get_identifier", (PyCFunction)Folder_get_identifier, METH_NOARGS,
     "get_identifier() - get identifier\n"
     "\n"
     "Get identifier for folder as a string (e.g. #mh/foo/bar).\n\n"
     "DEPRECATED: Use identifier property instead."},

    {"get_messages", (PyCFunction)Folder_get_messages, METH_NOARGS,
     "get_messages() - get a tuple of messages in folder\n"
     "\n"
     "Get a tuple of MessageInfos for the folder."},

     {NULL}
};

static PyGetSetDef Folder_getset[] = {
    {"name", (getter)get_name, (setter)NULL,
     "name - name of folder", NULL},

    {"path", (getter)get_path, (setter)NULL,
     "path - path of folder", NULL},

    {"identifier", (getter)get_identifier, (setter)NULL,
     "identifier - identifier of folder", NULL},

    {"mailbox", (getter)get_mailbox, (setter)NULL,
     "mailbox - corresponding mailbox", NULL},

    {"mailbox_name", (getter)get_mailbox_name, (setter)NULL,
     "mailbox_name - name of the corresponding mailbox\n\n"
     "DEPRECATED: Use folder.mailbox.name instead", NULL},

    {"properties", (getter)get_properties, (setter)NULL,
     "properties - folder properties object", NULL},

    {"num_messages", (getter)get_num_messages, (setter)NULL,
     "num_messages - total number of messages in folder", NULL},

    {"num_new_messages", (getter)get_num_new_messages, (setter)NULL,
     "num_new_messages - number of new messages in folder", NULL},

    {"num_unread_messages", (getter)get_num_unread_messages, (setter)NULL,
     "num_unread_messages - number of unread messages in folder", NULL},

    {"num_marked_messages", (getter)get_num_marked_messages, (setter)NULL,
     "num_marked_messages - number of marked messages in folder", NULL},

    {"num_locked_messages", (getter)get_num_locked_messages, (setter)NULL,
     "num_locked_messages - number of locked messages in folder", NULL},

    {"num_unread_marked_messages", (getter)get_num_unread_marked_messages, (setter)NULL,
     "num_unread_marked_messages - number of unread marked messages in folder", NULL},

    {"num_ignored_messages", (getter)get_num_ignored_messages, (setter)NULL,
     "num_ignored_messages - number of ignored messages in folder", NULL},

    {"num_watched_messages", (getter)get_num_watched_messages, (setter)NULL,
     "num_watched_messages - number of watched messages in folder", NULL},

    {"num_replied_messages", (getter)get_num_replied_messages, (setter)NULL,
     "num_replied_messages - number of replied messages in folder", NULL},

    {"num_forwarded_messages", (getter)get_num_forwarded_messages, (setter)NULL,
     "num_forwarded_messages - number of forwarded messages in folder", NULL},

    {NULL}
};


static PyTypeObject clawsmail_FolderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
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
    (reprfunc)Folder_str,      /* tp_str*/
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
    0,                         /* tp_members */
    Folder_getset,             /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Folder_init,     /* tp_init */
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

gboolean cmpy_add_folder(PyObject *module)
{
  clawsmail_FolderType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_FolderType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_FolderType);
  return (PyModule_AddObject(module, "Folder", (PyObject*)&clawsmail_FolderType) == 0);
}

PyObject* clawsmail_folder_new(FolderItem *folderitem)
{
  clawsmail_FolderObject *ff;
  PyObject *arglist;
  gchar *id;

  if(!folderitem)
    return NULL;

  id = folder_item_get_identifier(folderitem);
  if(id) {
    arglist = Py_BuildValue("(s)", id);
    g_free(id);
    ff = (clawsmail_FolderObject*) PyObject_CallObject((PyObject*) &clawsmail_FolderType, arglist);
    Py_DECREF(arglist);
    return (PyObject*)ff;
  }
  Py_RETURN_NONE;
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
