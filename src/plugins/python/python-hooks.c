/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef ENABLE_PYTHON
#include <Python.h>
#include <pygobject.h>
#endif // ENABLE_PYTHON

#include <glib.h>
#include <glib/gi18n.h>

#include <dlfcn.h>

#include <signal.h>

#include "python-hooks.h"


static gboolean python_enabled = FALSE;
static void *python_dlhandle = NULL;

#ifdef ENABLE_PYTHON
static GString *captured_stdout = NULL;
static GString *captured_stderr = NULL;


static PyObject *
capture_stdout(PyObject *self, PyObject *args)
{
    char *str = NULL;

    if (!PyArg_ParseTuple(args, "s", &str))
        return NULL;

    g_string_append(captured_stdout, str);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
capture_stderr(PyObject *self, PyObject *args)
{
    char *str = NULL;

    if (!PyArg_ParseTuple(args, "s", &str))
        return NULL;

    g_string_append(captured_stderr, str);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
capture_stdin(PyObject *self, PyObject *args)
{
    /* Return an empty string.
     * This is what read() returns when hitting EOF. */
    return PyUnicode_FromString("");
}

static PyObject *
wrap_gobj(PyObject *self, PyObject *args)
{
    void *addr;
    GObject *obj;

    if (!PyArg_ParseTuple(args, "l", &addr))
        return NULL;

    if (!G_IS_OBJECT(addr))
        return NULL; // XXX

    obj = G_OBJECT(addr);

    if (!obj)
        return NULL; // XXX

    return pygobject_new(obj);
}

static PyMethodDef parasite_python_methods[] = {
    {"capture_stdout", capture_stdout, METH_VARARGS, "Captures stdout"},
    {"capture_stderr", capture_stderr, METH_VARARGS, "Captures stderr"},
    {"capture_stdin", capture_stdin, METH_VARARGS, "Captures stdin"},
    {"gobj", wrap_gobj, METH_VARARGS, "Wraps a C GObject"},
    {NULL, NULL, 0, NULL}
};


static gboolean
is_blacklisted(void)
{
    const char *prgname = g_get_prgname();

    return (!strcmp(prgname, "gimp"));
}
#endif // ENABLE_PYTHON

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "parasite",
    NULL,
    -1,
    parasite_python_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
parasite_python_module_init(void)
{
#ifdef ENABLE_PYTHON
    return PyModule_Create(&moduledef);
#endif // ENABLE_PYTHON
    return NULL;
}

int
parasite_python_init(char **error)
{
#ifdef ENABLE_PYTHON
    struct sigaction old_sigint;
    PyObject *pygtk;

    if (is_blacklisted()) {
      *error = g_strdup("Application is blacklisted");
      return 0;
    }

    /* This prevents errors such as "undefined symbol: PyExc_ImportError" */
    python_dlhandle = dlopen(PYTHON_SHARED_LIB, RTLD_NOW | RTLD_GLOBAL);
    if (python_dlhandle == NULL)
    {
        *error = g_strdup_printf("Parasite: Error on dlopen(): %s\n", dlerror());
        return 0;
    }

    captured_stdout = g_string_new("");
    captured_stderr = g_string_new("");

    /* Back up and later restore SIGINT so Python doesn't steal it from us. */
    sigaction(SIGINT, NULL, &old_sigint);

    if (!Py_IsInitialized())
        Py_Initialize();

    sigaction(SIGINT, &old_sigint, NULL);

    if(PyRun_SimpleString(
        "import parasite\n"
        "import sys\n"
        "\n"
        "class StdoutCatcher:\n"
        "    def write(self, str):\n"
        "        parasite.capture_stdout(str)\n"
        "    def flush(self):\n"
        "        pass\n"
        "\n"
        "class StderrCatcher:\n"
        "    def write(self, str):\n"
        "        parasite.capture_stderr(str)\n"
        "    def flush(self):\n"
        "        pass\n"
        "\n"
        "class StdinCatcher:\n"
        "    def readline(self, size=-1):\n"
        "        return parasite.capture_stdin(size)\n"
        "    def read(self, size=-1):\n"
        "        return parasite.capture_stdin(size)\n"
        "    def flush(self):\n"
        "        pass\n"
        "\n"
    ) == -1) {
      dlclose(python_dlhandle);
      python_dlhandle = NULL;
      return 0;
    }

    pygtk = PyImport_ImportModule("gi");

    if (pygtk == NULL)
    {
        *error = g_strdup("Parasite: Could not import gi");
        dlclose(python_dlhandle);
        python_dlhandle = NULL;
        return 0;
    }

    python_enabled = TRUE;
#endif // ENABLE_PYTHON
    return !0;
}

void
parasite_python_done(void)
{
#ifdef ENABLE_PYTHON
    if(python_dlhandle != NULL) {
	dlclose(python_dlhandle);
	python_dlhandle = NULL;
    }
#endif
}

void
parasite_python_run(const char *command,
                    ParasitePythonLogger stdout_logger,
                    ParasitePythonLogger stderr_logger,
                    gpointer user_data)
{
#ifdef ENABLE_PYTHON
    PyGILState_STATE gstate;
    PyObject *module;
    PyObject *dict;
    PyObject *obj;
    const char *cp;

    /* empty string as command is a noop */
    if(!strcmp(command, ""))
      return;

    /* if first non-whitespace character is '#', command is also a noop */
    cp = command;
    while(cp && (*cp != '\0') && g_ascii_isspace(*cp))
      cp++;
    if(cp && *cp == '#')
      return;

    gstate = PyGILState_Ensure();

    module = PyImport_AddModule("__main__");
    dict = PyModule_GetDict(module);

    PyRun_SimpleString("old_stdout = sys.stdout\n"
                       "old_stderr = sys.stderr\n"
                       "old_stdin  = sys.stdin\n"
                       "sys.stdout = StdoutCatcher()\n"
                       "sys.stderr = StderrCatcher()\n"
                       "sys.stdin  = StdinCatcher()\n");

    obj = PyRun_String(command, Py_single_input, dict, dict);
    if(PyErr_Occurred())
      PyErr_Print();
    PyRun_SimpleString("sys.stdout = old_stdout\n"
                       "sys.stderr = old_stderr\n"
                       "sys.stdin = old_stdin\n");

    if (stdout_logger != NULL)
        stdout_logger(captured_stdout->str, user_data);

    if (stderr_logger != NULL)
        stderr_logger(captured_stderr->str, user_data);

    // Print any returned object
    if (obj != NULL && obj != Py_None) {
       PyObject *repr = PyObject_Repr(obj);
       if (repr != NULL) {
           char *string = PyBytes_AsString(repr);

           if (stdout_logger != NULL) {
               stdout_logger(string, user_data);
               stdout_logger("\n", user_data);
           }
        }

        Py_XDECREF(repr);
    }
    Py_XDECREF(obj);

    PyGILState_Release(gstate);
    g_string_erase(captured_stdout, 0, -1);
    g_string_erase(captured_stderr, 0, -1);
#endif // ENABLE_PYTHON
}

gboolean
parasite_python_is_enabled(void)
{
    return python_enabled;
}

// vim: set et sw=4 ts=4:
