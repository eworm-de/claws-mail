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

#include "nodetype.h"

#include <glib/gi18n.h>

#include <structmember.h>

/* returns true on success, false if an exception was thrown */
gboolean cmpy_add_node(PyObject *module)
{
  gboolean retval;
  PyObject *dict;
  PyObject *res;
  const char *cmd =
      "class Node(object):\n"
      "    \"\"\"A general purpose tree container type\"\"\"\n"
      "\n"
      "    def __init__(self):\n"
      "        self.data = None\n"
      "        self.children = []\n"
      "\n"
      "    def __str__(self):\n"
      "        return '\\n'.join(self.get_str_list(0))\n"
      "\n"
      "    def get_str_list(self, level):\n"
      "        \"\"\"get_str_list(level) - get a list of string-representations of the tree data\n"
      "        \n"
      "        The nesting of the tree elements is represented by various levels of indentation.\"\"\"\n"
      "        str = []\n"
      "        indent = '  '*level\n"
      "        if self.data:\n"
      "            str.append(indent + self.data.__str__())\n"
      "        else:\n"
      "            str.append(indent + 'None')\n"
      "        for child in self.children:\n"
      "            str.extend(child.get_str_list(level+1))\n"
      "        return str\n"
      "\n"
      "    def traverse(self, callback, arg=None):\n"
      "        \"\"\"traverse(callback [, arg=None]) - traverse the tree\n"
      "        \n"
      "        Traverse the tree, calling the callback function for each node element,\n"
      "        with optional arg as user-data. The expected callback function signature is\n"
      "        callback(node_data [, arg]).\"\"\"\n"
      "        if self.data:\n"
      "            if arg is not None:\n"
      "                callback(self.data, arg)\n"
      "            else:\n"
      "                callback(self.data)\n"
      "        for child in self.children:\n"
      "            child.traverse(callback, arg)\n"
      "\n"
      "    def flat_list(self):\n"
      "        \"\"\"flat_list() - get a flat list of the tree\n"
      "        \n"
      "        Returns a flat list of the tree, disregarding the nesting structure.\"\"\"\n"
      "        flat_list = []\n"
      "        self.traverse(lambda data,list: list.append(data), flat_list)\n"
      "        return flat_list\n"
      "\n";

  dict = PyModule_GetDict(module);

  if(PyDict_GetItemString(dict, "__builtins__") == NULL)
    PyDict_SetItemString(dict, "__builtins__", PyEval_GetBuiltins());

  res = PyRun_String(cmd, Py_file_input, dict, dict);

  retval = (res != NULL);
  Py_XDECREF(res);
  return retval;
}


PyObject* clawsmail_node_new(PyObject *module)
{
  PyObject *class, *dict;

  dict = PyModule_GetDict(module);
  class = PyDict_GetItemString(dict, "Node");
  return PyObject_CallObject(class, NULL);
}
