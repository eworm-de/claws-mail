/* Python plugin for Claws Mail
 * Copyright (C) 2009-2014 Holger Berndt
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

#include "messageinfotype.h"
#include "foldertype.h"

#include "common/tags.h"
#include "common/defs.h"
#include "mainwindow.h"
#include "summaryview.h"
#include "procheader.h"

#include <glib/gi18n.h>

#include <structmember.h>

#include <string.h>

typedef struct {
    PyObject_HEAD
    MsgInfo *msginfo;
} clawsmail_MessageInfoObject;


static void MessageInfo_dealloc(clawsmail_MessageInfoObject* self)
{
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static int MessageInfo_init(clawsmail_MessageInfoObject *self, PyObject *args, PyObject *kwds)
{
  return 0;
}

static PyObject* MessageInfo_str(clawsmail_MessageInfoObject *self)
{
  if(self->msginfo) {
    gchar *From;
    gchar *Subject;
    From = self->msginfo->from ? self->msginfo->from : "";
    Subject = self->msginfo->subject ? self->msginfo->subject : "";
    return PyUnicode_FromFormat("MessageInfo: %s / %s", From, Subject);
  }
  Py_RETURN_NONE;
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

static PyObject* get_header(PyObject *self, PyObject *args)
{
  int retval;
  char *header_str;
  char *header_str_dup;
  MsgInfo *msginfo;
  gchar *header_content = NULL;

  retval = PyArg_ParseTuple(args, "s", &header_str);
  if(!retval)
    return NULL;

  msginfo = ((clawsmail_MessageInfoObject*)self)->msginfo;

  header_str_dup = g_strdup(header_str);
  retval = procheader_get_header_from_msginfo(msginfo, &header_content, header_str);
  g_free(header_str_dup);
  if(retval == 0) {
    PyObject *header_content_object;
    gchar *content_start;

    /* the string is now Header: Value. Strip the Header: part */
    content_start = strstr(header_content, ":");
    if(content_start == NULL)
      content_start = header_content;
    else
      content_start++;
    /* strip leading spaces */
    while(*content_start == ' ')
      content_start++;
    header_content_object = Py_BuildValue("s", content_start);
    g_free(header_content);
    return header_content_object;
  }
  else {
    g_free(header_content);
    Py_RETURN_NONE;
  }
}

static PyObject* get_From(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->from)
    return PyUnicode_FromString(self->msginfo->from);
  Py_RETURN_NONE;
}

static PyObject* get_To(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->to)
    return PyUnicode_FromString(self->msginfo->to);
  Py_RETURN_NONE;
}

static PyObject* get_Cc(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->cc)
    return PyUnicode_FromString(self->msginfo->cc);
  Py_RETURN_NONE;
}

static PyObject* get_Subject(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->subject)
    return PyUnicode_FromString(self->msginfo->subject);
  Py_RETURN_NONE;
}

static PyObject* get_MessageID(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->msgid)
    return PyUnicode_FromString(self->msginfo->msgid);
  Py_RETURN_NONE;
}

static PyObject* get_FilePath(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo) {
    gchar *filepath;
    filepath = procmsg_get_message_file_path(self->msginfo);
    if(filepath) {
      PyObject *retval;
      retval = PyUnicode_FromString(filepath);
      g_free(filepath);
      return retval;
    }
  }
  Py_RETURN_NONE;
}

static PyObject* get_flag(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo) {
    int flag = GPOINTER_TO_INT(closure);
    return py_boolean_return_value(((self->msginfo->flags.perm_flags & flag) != 0));
  }
  Py_RETURN_NONE;
}

static int set_flag(clawsmail_MessageInfoObject *self, PyObject *value, void *closure)
{
  int flag = GPOINTER_TO_INT(closure);

  if(value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Cannot delete flag attribute");
    return -1;
  }

  if(!self->msginfo) {
    PyErr_SetString(PyExc_RuntimeError, "MessageInfo object broken");
    return -1;
  }

  if(PyObject_IsTrue(value))
    procmsg_msginfo_set_flags(self->msginfo, flag, 0);
  else
    procmsg_msginfo_unset_flags(self->msginfo, flag, 0);

  return 0;
}


static PyObject* get_Folder(clawsmail_MessageInfoObject *self, void *closure)
{
  if(self->msginfo && self->msginfo->folder) {
    return clawsmail_folder_new(self->msginfo->folder);
  }
  Py_RETURN_NONE;
}


static PyMethodDef MessageInfo_methods[] = {
  {"is_new",  is_new, METH_NOARGS,
   "is_new() - checks if the message is new\n"
   "\n"
   "Returns True if the new flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'new' attribute."},

  {"is_unread",  is_unread, METH_NOARGS,
   "is_unread() - checks if the message is unread\n"
   "\n"
   "Returns True if the unread flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'unread' attribute."},

  {"is_marked",  is_marked, METH_NOARGS,
   "is_marked() - checks if the message is marked\n"
   "\n"
   "Returns True if the marked flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'marked' attribute."},

  {"is_replied",  is_replied, METH_NOARGS,
   "is_replied() - checks if the message has been replied to\n"
   "\n"
   "Returns True if the replied flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'replied' attribute."},

  {"is_locked",  is_locked, METH_NOARGS,
   "is_locked() - checks if the message has been locked\n"
   "\n"
   "Returns True if the locked flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'locked' attribute."},

  {"is_forwarded",  is_forwarded, METH_NOARGS,
   "is_forwarded() - checks if the message has been forwarded\n"
   "\n"
   "Returns True if the forwarded flag of the message is set."
   "\n\nThis function is deprecated in favor of the 'forwarded' attribute."},

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

   {"get_header",  get_header, METH_VARARGS,
    "get_header(name) - get a message header with a given name\n"
    "\n"
    "Get a message header content with a given name. If the header does not exist,\n"
    "the value 'None' is returned. If multiple headers with the same name exist,\n"
    "the first one is returned."},

  {NULL}
};

static PyGetSetDef MessageInfo_getset[] = {
    { "From", (getter)get_From, (setter)NULL,
      "From - the From header of the message", NULL},

    { "To", (getter)get_To, (setter)NULL,
      "To - the To header of the message", NULL },

    { "Cc", (getter)get_Cc, (setter)NULL,
      "Cc - the Cc header of the message", NULL },

    {"Subject", (getter)get_Subject, (setter)NULL,
     "Subject - the subject header of the message", NULL},

    {"MessageID", (getter)get_MessageID, (setter)NULL,
     "MessageID - the Message-ID header of the message", NULL},

    {"FilePath", (getter)get_FilePath, (setter)NULL,
     "FilePath - path and filename of the message", NULL},

    {"unread", (getter)get_flag, (setter)set_flag,
     "unread - Unread-flag of the message", GINT_TO_POINTER(MSG_UNREAD)},

    {"locked", (getter)get_flag, (setter)set_flag,
     "locked - Locked-flag of the message", GINT_TO_POINTER(MSG_LOCKED)},

    {"marked", (getter)get_flag, (setter)set_flag,
     "marked - Marked-flag of the message", GINT_TO_POINTER(MSG_MARKED)},

    {"new", (getter)get_flag, (setter)NULL,
     "new - new-flag of the message", GINT_TO_POINTER(MSG_NEW)},

    {"replied", (getter)get_flag, (setter)NULL,
     "replied - Replied-flag of the message", GINT_TO_POINTER(MSG_REPLIED)},

    {"forwarded", (getter)get_flag, (setter)NULL,
     "forwarded - Forwarded-flag of the message", GINT_TO_POINTER(MSG_FORWARDED)},

    {"Folder", (getter)get_Folder, (setter)NULL,
     "Folder - Folder in which the message is contained", NULL},

    {NULL}
};


static PyTypeObject clawsmail_MessageInfoType = {
    PyVarObject_HEAD_INIT(NULL, 0)
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
    (reprfunc)MessageInfo_str, /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "A MessageInfo represents" /* tp_doc */
    " a single message.\n\n"
    "Do not construct objects of this type yourself.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    MessageInfo_methods,       /* tp_methods */
    0,                         /* tp_members */
    MessageInfo_getset,        /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)MessageInfo_init,/* tp_init */
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

gboolean cmpy_add_messageinfo(PyObject *module)
{
  clawsmail_MessageInfoType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_MessageInfoType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_MessageInfoType);
  return (PyModule_AddObject(module, "MessageInfo", (PyObject*)&clawsmail_MessageInfoType) == 0);
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
  return (PyObject*)ff;
}

PyTypeObject* clawsmail_messageinfo_get_type_object()
{
  return &clawsmail_MessageInfoType;
}

MsgInfo* clawsmail_messageinfo_get_msginfo(PyObject *self)
{
  return ((clawsmail_MessageInfoObject*)self)->msginfo;
}
