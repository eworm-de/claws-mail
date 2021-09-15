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

#include <Python.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <errno.h>

#include "common/hooks.h"
#include "common/plugin.h"
#include "common/version.h"
#include "common/utils.h"
#include "gtk/menu.h"
#include "main.h"
#include "mainwindow.h"
#include "prefs_toolbar.h"

#include "python-shell.h"
#include "python-hooks.h"
#include "clawsmailmodule.h"
#include "file-utils.h"
#include "python_prefs.h"

#define PYTHON_SCRIPTS_BASE_DIR "python-scripts"
#define PYTHON_SCRIPTS_MAIN_DIR "main"
#define PYTHON_SCRIPTS_COMPOSE_DIR "compose"
#define PYTHON_SCRIPTS_AUTO_DIR "auto"
#define PYTHON_SCRIPTS_AUTO_STARTUP "startup"
#define PYTHON_SCRIPTS_AUTO_SHUTDOWN "shutdown"
#define PYTHON_SCRIPTS_AUTO_COMPOSE "compose_any"
#define PYTHON_SCRIPTS_ACTION_PREFIX "Tools/PythonScripts/"

static GSList *menu_id_list = NULL;
static GSList *python_mainwin_scripts_id_list = NULL;
static GSList *python_mainwin_scripts_names = NULL;
static GSList *python_compose_scripts_names = NULL;

static GtkWidget *python_console = NULL;

static gulong hook_compose_create = 0;

static gboolean python_console_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  MainWindow *mainwin;
  GtkToggleAction *action;

  mainwin =  mainwindow_get_mainwindow();
  action = GTK_TOGGLE_ACTION(gtk_action_group_get_action(mainwin->action_group, "Tools/ShowPythonConsole"));
  gtk_toggle_action_set_active(action, FALSE);
  return TRUE;
}

static void size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&python_config.console_win_width, &python_config.console_win_height);
}

static void setup_python_console(void)
{
  GtkWidget *vbox;
  GtkWidget *console;
  static GdkGeometry geometry;

  python_console = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT(python_console), "size_allocate",
		   G_CALLBACK (size_allocate_cb), NULL);
  if (!geometry.min_height) {
	  geometry.min_width = 600;
	  geometry.min_height = 400;
  }

  gtk_window_set_geometry_hints(GTK_WINDOW(python_console), NULL, &geometry,
				    GDK_HINT_MIN_SIZE);
  gtk_window_set_default_size(GTK_WINDOW(python_console),
				  python_config.console_win_width,
				  python_config.console_win_height);

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(python_console), vbox);

  console = parasite_python_shell_new();
  gtk_box_pack_start(GTK_BOX(vbox), console, TRUE, TRUE, 0);

  g_signal_connect(python_console, "delete-event", G_CALLBACK(python_console_delete_event), NULL);

  gtk_widget_show_all(python_console);

  parasite_python_shell_focus(PARASITE_PYTHON_SHELL(console));
}

static void show_hide_python_console(GtkToggleAction *action, gpointer callback_data)
{
  if(gtk_toggle_action_get_active(action)) {
    if(!python_console)
      setup_python_console();
    gtk_widget_show(python_console);
  }
  else {
    gtk_widget_hide(python_console);
  }
}

static void remove_python_scripts_menus(void)
{
  GSList *walk;
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();

  /* toolbar */
  for(walk = python_mainwin_scripts_names; walk; walk = walk->next)
    prefs_toolbar_unregister_plugin_item(TOOLBAR_MAIN, "Python", walk->data);

  /* ui */
  for(walk = python_mainwin_scripts_id_list; walk; walk = walk->next)
      gtk_ui_manager_remove_ui(mainwin->ui_manager, GPOINTER_TO_UINT(walk->data));
  g_slist_free(python_mainwin_scripts_id_list);
  python_mainwin_scripts_id_list = NULL;

  /* actions */
  for(walk = python_mainwin_scripts_names; walk; walk = walk->next) {
    GtkAction *action;
    gchar *entry;
    entry = g_strconcat(PYTHON_SCRIPTS_ACTION_PREFIX, walk->data, NULL);
    action = gtk_action_group_get_action(mainwin->action_group, entry);
    g_free(entry);
    if(action)
      gtk_action_group_remove_action(mainwin->action_group, action);
    g_free(walk->data);
  }
  g_slist_free(python_mainwin_scripts_names);
  python_mainwin_scripts_names = NULL;

  /* compose scripts */
  for(walk = python_compose_scripts_names; walk; walk = walk->next) {
    prefs_toolbar_unregister_plugin_item(TOOLBAR_COMPOSE, "Python", walk->data);
    g_free(walk->data);
  }
  g_slist_free(python_compose_scripts_names);
  python_compose_scripts_names = NULL;
}

static gchar* extract_filename(const gchar *str)
{
  gchar *filename;

  filename = g_strrstr(str, "/");
  if(!filename || *(filename+1) == '\0') {
    debug_print("Error: Could not extract filename from %s\n", str);
    return NULL;
  }
  filename++;
  return filename;
}

static void run_script_file(const gchar *filename, Compose *compose)
{
  FILE *fp;
  fp = claws_fopen(filename, "r");
  if(!fp) {
    debug_print("Error: Could not open file '%s'\n", filename);
    return;
  }
  put_composewindow_into_module(compose);
  if(PyRun_SimpleFile(fp, filename) == 0)
    debug_print("Problem running script file '%s'\n", filename);
  claws_fclose(fp);
}

static void run_auto_script_file_if_it_exists(const gchar *autofilename, Compose *compose)
{
  gchar *auto_filepath;

  /* execute auto/autofilename, if it exists */
  auto_filepath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_AUTO_DIR, G_DIR_SEPARATOR_S, autofilename, NULL);
  if(file_exist(auto_filepath, FALSE))
    run_script_file(auto_filepath, compose);
  g_free(auto_filepath);
}

static void python_mainwin_script_callback(GtkAction *action, gpointer data)
{
  char *filename;

  filename = extract_filename(data);
  if(!filename)
    return;
  filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_MAIN_DIR, G_DIR_SEPARATOR_S, filename, NULL);
  run_script_file(filename, NULL);
  g_free(filename);
}

typedef struct _ComposeActionData ComposeActionData;
struct _ComposeActionData {
  gchar *name;
  Compose *compose;
};

static void python_compose_script_callback(GtkAction *action, gpointer data)
{
  char *filename;
  ComposeActionData *dat = (ComposeActionData*)data;

  filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_COMPOSE_DIR, G_DIR_SEPARATOR_S, dat->name, NULL);
  run_script_file(filename, dat->compose);

  g_free(filename);
}

static void mainwin_toolbar_callback(gpointer parent, const gchar *item_name, gpointer data)
{
	gchar *script;
	script = g_strconcat(PYTHON_SCRIPTS_ACTION_PREFIX, item_name, NULL);
	python_mainwin_script_callback(NULL, script);
	g_free(script);
}

static void compose_toolbar_callback(gpointer parent, const gchar *item_name, gpointer data)
{
  gchar *filename;

  filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_COMPOSE_DIR, G_DIR_SEPARATOR_S,
      item_name, NULL);
  run_script_file(filename, (Compose*)parent);
  g_free(filename);
}

static char* make_sure_script_directory_exists(const gchar *subdir)
{
  char *dir;
  char *retval = NULL;
  dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S, subdir, NULL);
  if(!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
    if(g_mkdir(dir, 0777) != 0)
      retval = g_strdup_printf("Could not create directory '%s': %s", dir, g_strerror(errno));
  }
  g_free(dir);
  return retval;
}

static int make_sure_directories_exist(char **error)
{
  const char* dirs[] = {
      ""
      , PYTHON_SCRIPTS_MAIN_DIR
      , PYTHON_SCRIPTS_COMPOSE_DIR
      , PYTHON_SCRIPTS_AUTO_DIR
      , NULL
  };
  const char **dir = dirs;

  *error = NULL;

  while(*dir) {
    *error = make_sure_script_directory_exists(*dir);
    if(*error)
      break;
    dir++;
  }

  return (*error == NULL);
}

static void migrate_scripts_out_of_base_dir(void)
{
  char *base_dir;
  GDir *dir;
  const char *filename;
  gchar *dest_dir;

  base_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, NULL);
  dir = g_dir_open(base_dir, 0, NULL);
  g_free(base_dir);
  if(!dir)
    return;

  dest_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S,
      PYTHON_SCRIPTS_MAIN_DIR, NULL);
  if(!g_file_test(dest_dir, G_FILE_TEST_IS_DIR)) {
    if(g_mkdir(dest_dir, 0777) != 0) {
      g_free(dest_dir);
      g_dir_close(dir);
      return;
    }
  }

  while((filename = g_dir_read_name(dir)) != NULL) {
    gchar *filepath;
    filepath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S, filename, NULL);
    if(g_file_test(filepath, G_FILE_TEST_IS_REGULAR)) {
      gchar *dest_file;
      dest_file = g_strconcat(dest_dir, G_DIR_SEPARATOR_S, filename, NULL);
      if(move_file(filepath, dest_file, FALSE) == 0)
        debug_print("Python plugin: Moved file '%s' to %s subdir\n", filename, PYTHON_SCRIPTS_MAIN_DIR);
      else
        debug_print("Python plugin: Warning: Could not move file '%s' to %s subdir\n", filename, PYTHON_SCRIPTS_MAIN_DIR);
      g_free(dest_file);
    }
    g_free(filepath);
  }
  g_dir_close(dir);
  g_free(dest_dir);
}


static void create_mainwindow_menus_and_items(GSList *filenames, gint num_entries)
{
  MainWindow *mainwin;
  gint ii;
  GSList *walk;
  GtkActionEntry *entries;

  /* create menu items */
  entries = g_new0(GtkActionEntry, num_entries);
  ii = 0;
  mainwin =  mainwindow_get_mainwindow();
  for(walk = filenames; walk; walk = walk->next) {
    entries[ii].name = g_strconcat(PYTHON_SCRIPTS_ACTION_PREFIX, walk->data, NULL);
    entries[ii].label = walk->data;
    entries[ii].callback = G_CALLBACK(python_mainwin_script_callback);
    gtk_action_group_add_actions(mainwin->action_group, &(entries[ii]), 1, (gpointer)entries[ii].name);
    ii++;
  }
  for(ii = 0; ii < num_entries; ii++) {
    guint id;

    python_mainwin_scripts_names = g_slist_prepend(python_mainwin_scripts_names, g_strdup(entries[ii].label));
    MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/" PYTHON_SCRIPTS_ACTION_PREFIX, entries[ii].label,
        entries[ii].name, GTK_UI_MANAGER_MENUITEM, id)
    python_mainwin_scripts_id_list = g_slist_prepend(python_mainwin_scripts_id_list, GUINT_TO_POINTER(id));

    prefs_toolbar_register_plugin_item(TOOLBAR_MAIN, "Python", entries[ii].label, mainwin_toolbar_callback, NULL);
  }

  g_free(entries);
}


/* this function doesn't really create menu items, but prepares a list that can be used
 * in the compose create hook. It does however register the scripts for the toolbar editor */
static void create_compose_menus_and_items(GSList *filenames)
{
  GSList *walk;
  for(walk = filenames; walk; walk = walk->next) {
    python_compose_scripts_names = g_slist_prepend(python_compose_scripts_names, g_strdup((gchar*)walk->data));
    prefs_toolbar_register_plugin_item(TOOLBAR_COMPOSE, "Python", (gchar*)walk->data, compose_toolbar_callback, NULL);
  }
}

static GtkActionEntry compose_tools_python_actions[] = {
    {"Tools/PythonScripts", NULL, N_("Python scripts"), NULL, NULL, NULL },
};

static void ComposeActionData_destroy_cb(gpointer data)
{
  ComposeActionData *dat = (ComposeActionData*)data;
  g_free(dat->name);
  g_free(dat);
}

static gboolean my_compose_create_hook(gpointer cw, gpointer data)
{
  gint ii;
  GSList *walk;
  GtkActionEntry *entries;
  GtkActionGroup *action_group;
  Compose *compose = (Compose*)cw;
  guint num_entries = g_slist_length(python_compose_scripts_names);

  action_group = gtk_action_group_new("PythonPlugin");
  gtk_action_group_add_actions(action_group, compose_tools_python_actions, 1, NULL);
  entries = g_new0(GtkActionEntry, num_entries);
  ii = 0;
  for(walk = python_compose_scripts_names; walk; walk = walk->next) {
    ComposeActionData *dat;

    entries[ii].name = walk->data;
    entries[ii].label = walk->data;
    entries[ii].callback = G_CALLBACK(python_compose_script_callback);

    dat = g_new0(ComposeActionData, 1);
    dat->name = g_strdup(walk->data);
    dat->compose = compose;

    gtk_action_group_add_actions_full(action_group, &(entries[ii]), 1, dat, ComposeActionData_destroy_cb);
    ii++;
  }
  gtk_ui_manager_insert_action_group(compose->ui_manager, action_group, 0);

  MENUITEM_ADDUI_MANAGER(compose->ui_manager, "/Menu/Tools", "PythonScripts",
      "Tools/PythonScripts", GTK_UI_MANAGER_MENU)

  for(ii = 0; ii < num_entries; ii++) {
    MENUITEM_ADDUI_MANAGER(compose->ui_manager, "/Menu/" PYTHON_SCRIPTS_ACTION_PREFIX, entries[ii].label,
        entries[ii].name, GTK_UI_MANAGER_MENUITEM)
  }

  g_free(entries);

  run_auto_script_file_if_it_exists(PYTHON_SCRIPTS_AUTO_COMPOSE, compose);

  return FALSE;
}


static void refresh_scripts_in_dir(const gchar *subdir, ToolbarType toolbar_type)
{
  char *scripts_dir;
  GDir *dir;
  GError *error = NULL;
  const char *filename;
  GSList *filenames = NULL;
  GSList *walk;
  gint num_entries;

  scripts_dir = g_strconcat(get_rc_dir(),
      G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR,
      G_DIR_SEPARATOR_S, subdir,
      NULL);
  debug_print("Refreshing: %s\n", scripts_dir);

  dir = g_dir_open(scripts_dir, 0, &error);
  g_free(scripts_dir);

  if(!dir) {
    debug_print("Could not open directory '%s': %s\n", subdir, error->message);
    g_error_free(error);
    return;
  }

  /* get filenames */
  num_entries = 0;
  while((filename = g_dir_read_name(dir)) != NULL) {
    char *fn;

    fn = g_strdup(filename);
    filenames = g_slist_prepend(filenames, fn);
    num_entries++;
  }
  g_dir_close(dir);

  if(toolbar_type == TOOLBAR_MAIN)
    create_mainwindow_menus_and_items(filenames, num_entries);
  else if(toolbar_type == TOOLBAR_COMPOSE)
    create_compose_menus_and_items(filenames);

  /* cleanup */
  for(walk = filenames; walk; walk = walk->next)
    g_free(walk->data);
  g_slist_free(filenames);
}

static void browse_python_scripts_dir(GtkAction *action, gpointer data)
{
  gchar *uri;
  GdkAppLaunchContext *launch_context;
  GError *error = NULL;
  MainWindow *mainwin;

  mainwin =  mainwindow_get_mainwindow();
  if(!mainwin) {
      debug_print("Browse Python scripts: Problems getting the mainwindow\n");
      return;
  }
  launch_context = gdk_app_launch_context_new();
  gdk_app_launch_context_set_screen(launch_context, gtk_widget_get_screen(mainwin->window));
  uri = g_strconcat("file://", get_rc_dir(), G_DIR_SEPARATOR_S, PYTHON_SCRIPTS_BASE_DIR, G_DIR_SEPARATOR_S, NULL);
  g_app_info_launch_default_for_uri(uri, G_APP_LAUNCH_CONTEXT(launch_context), &error);

  if(error) {
      debug_print("Could not open scripts dir browser: '%s'\n", error->message);
      g_error_free(error);
  }

  g_object_unref(launch_context);
  g_free(uri);
}

static void refresh_python_scripts_menus(GtkAction *action, gpointer data)
{
  remove_python_scripts_menus();

  migrate_scripts_out_of_base_dir();

  refresh_scripts_in_dir(PYTHON_SCRIPTS_MAIN_DIR, TOOLBAR_MAIN);
  refresh_scripts_in_dir(PYTHON_SCRIPTS_COMPOSE_DIR, TOOLBAR_COMPOSE);
}

static GtkToggleActionEntry mainwindow_tools_python_toggle[] = {
    {"Tools/ShowPythonConsole", NULL, N_("Show Python console..."),
        NULL, NULL, G_CALLBACK(show_hide_python_console), FALSE},
};

static GtkActionEntry mainwindow_tools_python_actions[] = {
    {"Tools/PythonScripts", NULL, N_("Python scripts"), NULL, NULL, NULL },
    {"Tools/PythonScripts/Refresh", NULL, N_("Refresh"),
        NULL, NULL, G_CALLBACK(refresh_python_scripts_menus) },
    {"Tools/PythonScripts/Browse", NULL, N_("Browse"),
        NULL, NULL, G_CALLBACK(browse_python_scripts_dir) },
    {"Tools/PythonScripts/---", NULL, "---", NULL, NULL, NULL },
};

static int python_menu_init(char **error)
{
  MainWindow *mainwin;
  guint id;

  mainwin =  mainwindow_get_mainwindow();
  if(!mainwin) {
    *error = g_strdup("Could not get main window");
    return 0;
  }

  gtk_action_group_add_toggle_actions(mainwin->action_group, mainwindow_tools_python_toggle, 1, mainwin);
  gtk_action_group_add_actions(mainwin->action_group, mainwindow_tools_python_actions, 3, mainwin);

  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools", "ShowPythonConsole",
      "Tools/ShowPythonConsole", GTK_UI_MANAGER_MENUITEM, id)
  menu_id_list = g_slist_prepend(menu_id_list, GUINT_TO_POINTER(id));

  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools", "PythonScripts",
      "Tools/PythonScripts", GTK_UI_MANAGER_MENU, id)
  menu_id_list = g_slist_prepend(menu_id_list, GUINT_TO_POINTER(id));

  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools/PythonScripts", "Refresh",
      "Tools/PythonScripts/Refresh", GTK_UI_MANAGER_MENUITEM, id)
  menu_id_list = g_slist_prepend(menu_id_list, GUINT_TO_POINTER(id));

  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools/PythonScripts", "Browse",
      "Tools/PythonScripts/Browse", GTK_UI_MANAGER_MENUITEM, id)
  menu_id_list = g_slist_prepend(menu_id_list, GUINT_TO_POINTER(id));

  MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Tools/PythonScripts", "Separator1",
      "Tools/PythonScripts/---", GTK_UI_MANAGER_SEPARATOR, id)
  menu_id_list = g_slist_prepend(menu_id_list, GUINT_TO_POINTER(id));

  refresh_python_scripts_menus(NULL, NULL);

  return !0;
}

static void python_menu_done(void)
{
  MainWindow *mainwin;

  mainwin = mainwindow_get_mainwindow();

  if(mainwin && !claws_is_exiting()) {
    GSList *walk;

    remove_python_scripts_menus();

    for(walk = menu_id_list; walk; walk = walk->next)
      gtk_ui_manager_remove_ui(mainwin->ui_manager, GPOINTER_TO_UINT(walk->data));
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager, mainwin->action_group, "Tools/ShowPythonConsole", 0);
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager, mainwin->action_group, "Tools/PythonScripts", 0);
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager, mainwin->action_group, "Tools/PythonScripts/Refresh", 0);
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager, mainwin->action_group, "Tools/PythonScripts/Browse", 0);
    MENUITEM_REMUI_MANAGER(mainwin->ui_manager, mainwin->action_group, "Tools/PythonScripts/---", 0);
  }
}


static PyObject *get_StringIO_instance(void)
{
  PyObject *module_StringIO = NULL;
  PyObject *class_StringIO = NULL;
  PyObject *inst_StringIO = NULL;

  module_StringIO = PyImport_ImportModule("io");
  if(!module_StringIO) {
    debug_print("Error getting traceback: Could not import module io\n");
    goto done;
  }

  class_StringIO = PyObject_GetAttrString(module_StringIO, "StringIO");
  if(!class_StringIO) {
    debug_print("Error getting traceback: Could not get StringIO class\n");
    goto done;
  }

  inst_StringIO = PyObject_CallObject(class_StringIO, NULL);
  if(!inst_StringIO) {
    debug_print("Error getting traceback: Could not create an instance of the StringIO class\n");
    goto done;
  }

done:
  Py_XDECREF(module_StringIO);
  Py_XDECREF(class_StringIO);

  return inst_StringIO;
}

static void log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
}

gint plugin_init(gchar **error)
{
  guint log_handler;
  int parasite_retval;
  PyObject *inst_StringIO = NULL;

  /* Version check */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,7,6,9), VERSION_NUMERIC, _("Python"), error))
    return -1;

  /* init/load prefs */
  python_prefs_init();

  /* load hooks */
  hook_compose_create = hooks_register_hook(COMPOSE_CREATED_HOOKLIST, my_compose_create_hook, NULL);
  if(hook_compose_create == 0) {
    *error = g_strdup(_("Failed to register \"compose create hook\" in the Python plugin"));
    return -1;
  }

  /* script directories */
  if(!make_sure_directories_exist(error))
    goto err;

  /* register moduke */
  PyImport_AppendInittab("clawsmail", initclawsmail);
  PyImport_AppendInittab("parasite", parasite_python_module_init);

  /* initialize python interpreter */
  Py_Initialize();

  /* The Python C API only offers to print an exception to sys.stderr. In order to catch it
   * in a string, a StringIO object is created, to which sys.stderr can be redirected in case
   * an error occurred. */
  inst_StringIO = get_StringIO_instance();

  if(PyRun_SimpleString("import clawsmail") == -1) {
    *error = g_strdup("Error importing the clawsmail module");
    goto err;
  }

  /* initialize python interactive shell */
  log_handler = g_log_set_handler(NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO, log_func, NULL);
  parasite_retval = parasite_python_init(error);
  g_log_remove_handler(NULL, log_handler);
  if(!parasite_retval) {
    goto err;
  }

  /* load menu options */
  if(!python_menu_init(error)) {
    goto err;
  }

  /* problems here are not fatal */
  run_auto_script_file_if_it_exists(PYTHON_SCRIPTS_AUTO_STARTUP, NULL);

  debug_print("Python plugin loaded\n");

  return 0;

err:
  hooks_unregister_hook(COMPOSE_CREATED_HOOKLIST, hook_compose_create);
  Py_XDECREF(inst_StringIO);
  return -1;
}

gboolean plugin_done(void)
{
  hooks_unregister_hook(COMPOSE_CREATED_HOOKLIST, hook_compose_create);

  run_auto_script_file_if_it_exists(PYTHON_SCRIPTS_AUTO_SHUTDOWN, NULL);

  python_menu_done();

  if(python_console) {
    gtk_widget_destroy(python_console);
    python_console = NULL;
  }

  /* finialize python interpreter */
  Py_Finalize();

  parasite_python_done();

  /* save prefs */
  python_prefs_done();

  debug_print("Python plugin done and unloaded.\n");
  return FALSE;
}

const gchar *plugin_name(void)
{
  return _("Python");
}

const gchar *plugin_desc(void)
{
  return _("This plugin provides Python integration features.\n"
      "Python code can be entered interactively into an embedded Python console, "
      "under Tools -> Show Python console, or stored in scripts.\n\n"
      "These scripts are then available via the menu. You can assign "
      "keyboard shortcuts to them just like it is done with other menu items. "
      "You can also put buttons for script invocation into the toolbars "
      "using Claws Mail's builtin toolbar editor.\n\n"
      "You can provide scripts working on the main window by placing files "
      "into ~/.claws-mail/python-scripts/main.\n\n"
      "You can also provide scripts working on an open compose window "
      "by placing files into ~/.claws-mail/python-scripts/compose.\n\n"
      "The folder ~/.claws-mail/python-scripts/auto/ may contain some "
      "scripts that are automatically executed when certain events "
      "occur. Currently, the following files in this directory "
      "are recognised:\n\n"
      "compose_any\n"
      "Gets executed whenever a compose window is opened, no matter "
      "if that opening happened as a result of composing a new message, "
      "replying or forwarding a message.\n\n"
      "startup\n"
      "Executed at plugin load\n\n"
      "shutdown\n"
      "Executed at plugin unload\n\n"
      "\nFor the most up-to-date API documentation, type\n"
      "\n help(clawsmail)\n"
      "\nin the interactive Python console.\n"
      "\nThe source distribution of this plugin comes with various example scripts "
      "in the \"examples\" subdirectory. If you wrote a script that you would be "
      "interested in sharing, feel free to send it to me to have it considered "
      "for inclusion in the examples.\n"
      "\nFeedback to <berndth@gmx.de> is welcome.");
}

const gchar *plugin_type(void)
{
  return "GTK3";
}

const gchar *plugin_licence(void)
{
  return "GPL3+";
}

const gchar *plugin_version(void)
{
  return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
  static struct PluginFeature features[] =
    { {PLUGIN_UTILITY, N_("Python integration")},
      {PLUGIN_NOTHING, NULL}};
  return features;
}
