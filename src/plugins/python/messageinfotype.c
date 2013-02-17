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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "messageinfotype.h"

#include "common/tags.h"
#include "mainwindow.h"
#include "summaryview.h"

#include <structmember.h>


typedef struct {
    PyObject_HEAD
    PyObject *from;
    PyObject *to;
    PyObject *subject;
    PyObject *msgid;
    PyObject *filepath;
    MsgInfo *msginfo;
} clawsmail_MessageInfoObject;


static void MessageInfo_dealloc(clawsmail_MessageInfoObject* self)
{
  Py_XDECREF(self->from);
  Py_XDECREF(self->to);
  Py_XDECREF(self->subject);
  Py_XDECREF(self->msgid);
  self->ob_type->tp_free((PyObject*)self);
}

static int MessageInfo_init(clawsmail_MessageInfoObject *self, PyObject *args, PyObject *kwds)
{
  Py_INCREF(Py_None);
  self->from = Py_None;

  Py_INCREF(Py_None);
  self->to = Py_None;

  Py_INCREF(Py_None);
  self->subject = Py_None;

  Py_INCREF(Py_None);
  self->msgid = Py_None;

  return 0;
}

static PyObject* MessageInfo_str(PyObject *self)
{
  PyObject *str;
  str = PyString_FromString("MessageInfo: ");
  PyString_ConcatAndDel(&str, PyObject_GetAttrString(self, "From"));
  PyString_ConcatAndDel(&str, PyString_FromString(" / "));
  PyString_ConcatAndDel(&str, PyObject_GetAttrString(self, "Subject"));
  return str;
}

static PyObject *py_boolean_return_value(gboolean val)
{
  if(val) {
    Py_INCREF(Py_True);
    return Py_True;
  }
  else {
    Py_INCREF(Py_False);
    return Py_False;
  }
}

static PyObject *is_new(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_NEW(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject *is_unread(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_UNREAD(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject *is_marked(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_MARKED(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject *is_replied(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_REPLIED(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject *is_locked(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_LOCKED(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject *is_forwarded(PyObject *self, PyObject *args)
{
  return py_boolean_return_value(MSG_IS_FORWARDED(((clawsmail_MessageInfoObject*)self)->msginfo->flags));
}

static PyObject* get_tags(PyObject *self, PyObject *args)
{
  GSList *tags_list;
  Py_ssize_t num_tags;
  PyObject *tags_tuple;

  tags_list = ((clawsmail_MessageInfoObject*)self)->msginfo->tags;
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

  return tags_tuple;
}


static PyObject* add_or_remove_tag(PyObject *self, PyObject *args, gboolean add)
{
  int retval;
  const char *tag_str;
  gint tag_id;
  MsgInfo *msginfo;
  MainWindow *mainwin;

  retval = PyArg_ParseTuple(args, "s", &tag_str);
  if(!retval)
    return NULL;

  tag_id = tags_get_id_for_str(tag_str);
  if(tag_id == -1) {
    PyErr_SetString(PyExc_ValueError, "Tag does not exist");
    return NULL;
  }

  msginfo = ((clawsmail_MessageInfoObject*)self)->msginfo;

  if(!add) {
    /* raise KeyError if tag is not set */
    if(!g_slist_find(msginfo->tags, GINT_TO_POINTER(tag_id))) {
      PyErr_SetString(PyExc_KeyError, "Tag is not set on this message");
      return NULL;
    }
  }

  procmsg_msginfo_update_tags(msginfo, add, tag_id);

  /* update display */
  mainwin = mainwindow_get_mainwindow();
  if(mainwin)
    summary_redisplay_msg(mainwin->summaryview);

  Py_RETURN_NONE;
}



static PyObject* add_tag(PyObject *self, PyObject *args)
{
  return add_or_remove_tag(self, args, TRUE);
}


static PyObject* remove_tag(PyObject *self, PyObject *args)
{
  return add_or_remove_tag(self, args, FALSE);
}


static PyMethodDef MessageInfo_methods[] = {
  {"is_new",  is_new, METH_NOARGS,
   "is_new() - checks if the message is new\n"
   "\n"
   "Returns True if the new flag of the message is set."},

  {"is_unread",  is_unread, METH_NOARGS, 
   "is_unread() - checks if the message is unread\n"
   "\n"
   "Returns True if the unread flag of the message is set."},

  {"is_marked",  is_marked, METH_NOARGS,
   "is_marked() - checks if the message is marked\n"
   "\n"
   "Returns True if the marked flag of the message is set."},

  {"is_replied",  is_replied, METH_NOARGS,
   "is_replied() - checks if the message has been replied to\n"
   "\n"
   "Returns True if the replied flag of the message is set."},

  {"is_locked",  is_locked, METH_NOARGS,
   "is_locked() - checks if the message has been locked\n"
   "\n"
   "Returns True if the locked flag of the message is set."},

  {"is_forwarded",  is_forwarded, METH_NOARGS,
   "is_forwarded() - checks if the message has been forwarded\n"
   "\n"
   "Returns True if the forwarded flag of the message is set."},

  {"get_tags",  get_tags, METH_NOARGS,
   "get_tags() - get message tags\n"
   "\n"
   "Returns a tuple of tags that apply to this message."},

  {"add_tag",  add_tag, METH_VARARGS,
   "add_tag(tag) - add a tag to this message\n"
   "\n"
   "Add a tag to this message. If the tag is already set, nothing is done.\n"
   "If the tag does not exist, a ValueError exception is raised."},

  {"remove_tag",  remove_tag, METH_VARARGS,
   "remove_tag(tag) - remove a tag from this message\n"
   "\n"
   "Remove a tag from this message. If the tag is not set, a KeyError exception is raised.\n"
   "If the tag does not exist, a ValueError exception is raised."},

  {NULL}
};

static PyMemberDef MessageInfo_members[] = {
    {"From", T_OBJECT_EX, offsetof(clawsmail_MessageInfoObject, from), 0,
     "From - the From header of the message"},

    {"To", T_OBJECT_EX, offsetof(clawsmail_MessageInfoObject, to), 0,
     "To - the To header of the message"},

    {"Subject", T_OBJECT_EX, offsetof(clawsmail_MessageInfoObject, subject), 0,
     "Subject - the subject header of the message"},

    {"MessageID", T_OBJECT_EX, offsetof(clawsmail_MessageInfoObject, msgid), 0,
     "MessageID - the Message-ID header of the message"},

    {"FilePath", T_OBJECT_EX, offsetof(clawsmail_MessageInfoObject, filepath), 0,
     "FilePath - path and filename of the message"},

    {NULL}
};

static PyTypeObject clawsmail_MessageInfoType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size*/
    "clawsmail.MessageInfo",   /* tp_name*/
    sizeof(clawsmail_MessageInfoObject), /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)MessageInfo_dealloc, /* tp_dealloc*/
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
    MessageInfo_str,           /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "A MessageInfo represents" /* tp_doc */
    "a single message.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    MessageInfo_methods,       /* tp_methods */
    MessageInfo_members,       /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)MessageInfo_init,/* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

PyMODINIT_FUNC initmessageinfo(PyObject *module)
{
    clawsmail_MessageInfoType.tp_new = PyType_GenericNew;
    if(PyType_Ready(&clawsmail_MessageInfoType) < 0)
        return;

    Py_INCREF(&clawsmail_MessageInfoType);
    PyModule_AddObject(module, "MessageInfo", (PyObject*)&clawsmail_MessageInfoType);
}

#define MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(fis, pms)     \
  do {                                                            \
    if(fis) {                                                     \
      PyObject *str;                                              \
      str = PyString_FromString(fis);                             \
      if(str) {                                                   \
        int retval;                                               \
        retval = PyObject_SetAttrString((PyObject*)ff, pms, str); \
        Py_DECREF(str);                                           \
        if(retval == -1)                                          \
          goto err;                                               \
      }                                                           \
    }                                                             \
  } while(0)

static gboolean update_members(clawsmail_MessageInfoObject *ff, MsgInfo *msginfo)
{
  gchar *filepath;

  MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(msginfo->from, "From");
  MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(msginfo->to, "To");
  MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(msginfo->subject, "Subject");
  MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(msginfo->msgid, "MessageID");

  filepath = procmsg_get_message_file_path(msginfo);
  if(filepath) {
    MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER(filepath, "FilePath");
    g_free(filepath);
  }
  else {
    MSGINFO_STRING_TO_PYTHON_MESSAGEINFO_MEMBER("", "FilePath");
  }

  return TRUE;
err:
  return FALSE;
}

PyObject* clawsmail_messageinfo_new(MsgInfo *msginfo)
{
  clawsmail_MessageInfoObject *ff;

  if(!msginfo)
    return NULL;

  ff = (clawsmail_MessageInfoObject*) PyObject_CallObject((PyObject*) &clawsmail_MessageInfoType, NULL);
  if(!ff)
    return NULL;

  ff->msginfo = msginfo;

  if(update_members(ff, msginfo))
    return (PyObject*)ff;
  else {
    Py_XDECREF(ff);
    return NULL;
  }
}

PyTypeObject* clawsmail_messageinfo_get_type_object()
{
  return &clawsmail_MessageInfoType;
}

MsgInfo* clawsmail_messageinfo_get_msginfo(PyObject *self)
{
  return ((clawsmail_MessageInfoObject*)self)->msginfo;
}
