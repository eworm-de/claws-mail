/* gtkspell - a spell-checking addon for GtkText
 * Copyright (c) 2000 Evan Martin.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
/*
    Stuphead: (C) 2000,2001 Grigroy Bakunov, Sergey Pinaev
 */

/*
 * Adapted for Sylpheed (Claws) (c) 2001 by Hiroyuki Yamamoto & 
 * The Sylpheed Claws Team.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "intl.h"

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <prefs_common.h>
#include <utils.h>

#include <dirent.h>

#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>

#include "gtkxtext.h"

#include "gtkspell.h"

/* size of the text buffer used in various word-processing routines. */
#define BUFSIZE 1024

/* number of suggestions to display on each menu. */
#define MENUCOUNT 15

/* because we keep only one copy of the spell program running,
 * all ispell-related variables can be static.
 */
volatile pid_t spell_pid = -1;
static int sp_fd_write[2], sp_fd_read[2];
static int signal_set_up = 0;

static GdkColor highlight = { 0, 255 * 256, 0, 0 };

static void entry_insert_cb(GtkXText *gtktext,
                            gchar *newtext, guint len, guint *ppos, gpointer d);
static void set_up_signal();

int gtkspell_running() 
{
    return (spell_pid > 0);
}

/* functions to interface with pipe */
static void writetext(unsigned char *text_ccc) 
{
    write(sp_fd_write[1], text_ccc, strlen(text_ccc));
}

static int readpipe(unsigned char *buf, int bufsize) 
{
    int len;
    len = read(sp_fd_read[0], buf, bufsize - 1);
    if (len < 0) {
        debug_print(_("*** readpipe: read: %s\n"), strerror(errno));
        return -1;
    } else if (len == 0) {
        debug_print(_("*** readpipe: pipe closed.\n"));
        return -1;
    } else if (len == bufsize - 1) {
        debug_print(_("*** readpipe: buffer overflowed?\n"));
    }

    buf[len] = 0;
    return len;
}

static int readline(unsigned char *buf) 
{
    return readpipe(buf, BUFSIZE);
}

static int readresponse(unsigned char *buf) 
{
    int len;
    len = readpipe(buf, BUFSIZE);

    /* all ispell responses of any reasonable length should end in \n\n.
     * depending on the speed of the spell checker, this may require more
     * reading. */
    if (len >= 2 && (buf[len - 1] != '\n' || buf[len - 2] != '\n')) {
        len += readpipe(buf + len, BUFSIZE - len);
    }

    /* now we can remove all of the the trailing newlines. */
    while (len > 0 && buf[len - 1] == '\n')
        buf[--len] = 0;

    return len;
}

void gtkspell_stop() 
{
    if (gtkspell_running()) {
        kill(spell_pid, SIGTERM);
        debug_print(_("*** Kill pid[%i] returned: %s\n"), spell_pid, strerror(errno));
        while (spell_pid != -1);
    }
}

static int poller(int buffer) 
{
    int len;
    fd_set rfds;
    struct timeval tv;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(buffer, &rfds);
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    return select(buffer + 1, &rfds, NULL, NULL, &tv);
}

int gtkspell_start(unsigned char *path, char * args[]) 
{
    int fd_error[2];
    FILE *sav_stdin, *sav_stdout, *sav_stderr;
    char buf[BUFSIZE];
    int retncode;

    if (gtkspell_running()) {
        debug_print(_("*** gtkspell_start called while already running.\n"));
        gtkspell_stop();
    }

    if (!signal_set_up) {
        set_up_signal();
        signal_set_up = 1;
    }

    pipe(sp_fd_write);
    pipe(sp_fd_read);
    pipe(fd_error);

    spell_pid = fork();
    if (spell_pid < 0) {
        debug_print(_("*** fork: %s\n"), strerror(errno));
        return -1;
    } else if (spell_pid == 0) {
        sav_stdin = fdopen(dup(fileno(stdin)), "r");
        sav_stdout = fdopen(dup(fileno(stdout)), "w");
        sav_stderr = fdopen(dup(fileno(stderr)), "w");
        dup2(sp_fd_write[0], 0);
        dup2(sp_fd_read[1], 1);
        dup2(fd_error[1], 2);
        close(sp_fd_read[0]);
        close(fd_error[0]);
        close(sp_fd_write[1]);

        if (path == NULL) {
            if (execvp(args[0], args) < 0)
                //DONT call debug_print here, because stdout is closed at this moment
                fprintf(sav_stderr, _("*** execvp('%s'): %s\n"), args[0], strerror(errno));
        } else {
            if (execv(path, args) < 0)
                //DONT call debug_print here, because stdout is closed at this moment
                fprintf(sav_stderr, _("*** execv('%s'): %s\n"), path, strerror(errno));
        }
        /* if we get here, we failed.
         * send some text on the pipe to indicate status.
         */
        write(sp_fd_read[1], "!", 1);

        _exit(0);
    } else {
        retncode = poller(sp_fd_read[1]);
        if (retncode < 0) {
            debug_print(_("*** Spell comand failed: %s.\n"), strerror(errno));
            gtkspell_stop();
            return -1;
        }
        readline(buf);
        /* ispell should print something like this:
         * @(#) International Ispell Version 3.1.20 10/10/95
         * if it doesn't, it's an error. */
        if (buf[0] != '@') {
            debug_print(_("*** ispell didnt print '@'\n"));
            gtkspell_stop();
            return -1;
        }
    }

    /* put ispell into terse mode.
     * this makes it not respond on correctly spelled words. */
    sprintf(buf, "!\n");
    writetext(buf);
    return 0;
}

static GList* misspelled_suggest(unsigned char *word) 
{
    unsigned char buf[BUFSIZE];
    unsigned char *newword;
    GList *l = NULL;
    int count;
    sprintf(buf, "^%s\n", word);  /* guard against ispell control chars */
    writetext(buf);
    readresponse(buf);
    switch (buf[0]) { /* first char is ispell command. */
    case 0:  /* no response: word is ok. */
        return NULL;
    case 10:  /* just enter word is ok.  */
        return NULL;
    case '&':  /* misspelled, with suggestions */
        /* & <orig> <count> <ofs>: <miss>, <miss>, <guess>, ... */
        strtok(buf, " ");  /* & */
        newword = strtok(NULL, " ");  /* orig */
        l = g_list_append(l, g_strdup(newword));
        newword = strtok(NULL, " ");  /* count */
        count = atoi(newword);
        strtok(NULL, " ");  /* ofs: */

        while ((newword = strtok(NULL, ",")) != NULL) {
            int len = strlen(newword);
            if (newword[len - 1] == ' ' || newword[len - 1] == '\n')
                newword[len - 1] = 0;
            if (count == 0) {
                g_list_append(l, NULL);  /* signal the "suggestions" */
            }
            /* add it to the list, skipping the initial space. */
            l = g_list_append(l,
                              g_strdup(newword[0] == ' ' ? newword + 1 : newword));

            count--;
        }
        return l;
    case '?':  /* ispell is guessing. */
    case '#':  /* misspelled, no suggestions */
        /* # <orig> <ofs> */
        strtok(buf, " ");  /* & */
        newword = strtok(NULL, " ");  /* orig */
        l = g_list_append(l, g_strdup(newword));
        return l;
    default:
        debug_print(_("*** Unsupported spell command '%c'.\n"), buf[0]);
    }
    return NULL;
}

static int misspelled_test(unsigned char *word) 
{
    unsigned char buf[BUFSIZE];
    sprintf(buf, "^%s\n", word);  /* guard against ispell control chars */
    writetext(buf);
    readresponse(buf);

    if (buf[0] == 0) {
        return 0;
    } else if (buf[0] == '&' || buf[0] == '#' || buf[0] == '?') {
        return 1;
    }

    debug_print(_("*** Unsupported spell command '%c'.\n"), buf[0]);

    return -1;
}

static gboolean iswordsep(unsigned char c) 
{
    return !isalpha(c) && c != '\'';
}

static guchar get_text_index_whar(GtkXText *gtktext, int pos) 
{
    guchar a;
    gchar *text;
    text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), pos, pos + 1);
    if (text == NULL) return 0;
    a = (guchar) * text;
    g_free(text);
    return a;
}

static gboolean get_word_from_pos(GtkXText *gtktext, int pos, unsigned char* buf,
                                  int *pstart, int *pend) 
{
    gint start, end;
    if (iswordsep(get_text_index_whar(gtktext, pos))) return FALSE;

    for (start = pos; start >= 0; --start) {
        if (iswordsep(get_text_index_whar(gtktext, start))) break;
    }
    start++;

    for (end = pos; end < gtk_xtext_get_length(gtktext); end++) {
        if (iswordsep(get_text_index_whar(gtktext, end))) break;
    }

    if (buf) {
        for (pos = start; pos < end; pos++) buf[pos - start] = get_text_index_whar(gtktext, pos);
        buf[pos - start] = 0;
    }

    if (pstart) *pstart = start;
    if (pend) *pend = end;

    return TRUE;
}

static gboolean get_curword(GtkXText *gtktext, unsigned char* buf,
                            int *pstart, int *pend) 
{
    int pos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    return get_word_from_pos(gtktext, pos, buf, pstart, pend);
}

static void change_color(GtkXText *gtktext, int start, int end, GdkColor *color) 
{
    char *newtext;
    if (start >= end) {
        return ;
    };
    gtk_xtext_freeze(gtktext);
    newtext = gtk_editable_get_chars(GTK_EDITABLE(gtktext), start, end);
//    if (prefs_common.auto_makeispell) {
        gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
                                         GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
//    }
    gtk_xtext_set_point(gtktext, start);
    gtk_xtext_forward_delete(gtktext, end - start);

    if (newtext && end - start > 0)
        gtk_xtext_insert(gtktext, NULL, color, NULL, newtext, end - start);
  //  if (prefs_common.auto_makeispell) {
        gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
                                           GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
   // }
    gtk_xtext_thaw(gtktext);
}

static gboolean check_at(GtkXText *gtktext, int from_pos) 
{
    int start, end;
    unsigned char buf[BUFSIZE];
    if (from_pos < 0) return FALSE;
    if (!get_word_from_pos(gtktext, from_pos, buf, &start, &end)) {
        return FALSE;
    };
    if (misspelled_test(buf)) {
        if (highlight.pixel == 0) {
            /* add an entry for the highlight in the color map. */
            GdkColormap *gc = gtk_widget_get_colormap(GTK_WIDGET(gtktext));
            gdk_colormap_alloc_color(gc, &highlight, FALSE, TRUE);
            ;
        }
        change_color(gtktext, start, end, &highlight);
        return TRUE;
    } else {
        change_color(gtktext, start, end,
                     &(GTK_WIDGET(gtktext)->style->fg[0]));
        return FALSE;
    }
}

void gtkspell_check_all(GtkXText *gtktext) 
{
    guint origpos;
    guint pos = 0;
    guint len;
    float adj_value;

    if (!gtkspell_running()) return ;

    len = gtk_xtext_get_length(gtktext);

    adj_value = gtktext->vadj->value;
    gtk_xtext_freeze(gtktext);
    origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    while (pos < len) {
        while (pos < len && iswordsep(get_text_index_whar(gtktext, pos)))
            pos++;
        while (pos < len && !iswordsep(get_text_index_whar(gtktext, pos)))
            pos++;
        if (pos > 0)
            check_at(gtktext, pos - 1);
    }
    gtk_xtext_thaw(gtktext);
    gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
    gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);

}

static void entry_insert_cb(GtkXText *gtktext,
                            gchar *newtext, guint len, guint *ppos, gpointer d) 
{
    int origpos;
    if (!gtkspell_running()) return ;

    gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
                                     GTK_SIGNAL_FUNC(entry_insert_cb),
                                     NULL);
    gtk_xtext_insert(GTK_XTEXT(gtktext), NULL,
                    &(GTK_WIDGET(gtktext)->style->fg[0]), NULL, newtext, len);

    gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
                                       GTK_SIGNAL_FUNC(entry_insert_cb),
                                       NULL);
    gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "insert-text");
    *ppos += len;
    origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));

    if (iswordsep(newtext[0])) {
        /* did we just end a word? */
        if (*ppos >= 2) check_at(gtktext, *ppos - 2);

        /* did we just split a word? */
        if (*ppos < gtk_xtext_get_length(gtktext))
            check_at(gtktext, *ppos + 1);
    } else {
        /* check as they type, *except* if they're typing at the end (the most
         * common case.
         */
        if (*ppos < gtk_xtext_get_length(gtktext) &&
                !iswordsep(get_text_index_whar(gtktext, *ppos)))
            check_at(gtktext, *ppos - 1);
    }

    gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
}

static void entry_delete_cb(GtkXText *gtktext,
                            gint start, gint end, gpointer d) 
{
    int origpos;
    if (!gtkspell_running()) return ;

    origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    check_at(gtktext, start - 1);
    gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
    gtk_editable_select_region(GTK_EDITABLE(gtktext), origpos, origpos);
    /* this is to *UNDO* the selection, in case they were holding shift
     * while hitting backspace. */
}

static void replace_word(GtkWidget *w, gpointer d) 
{
    int start, end;
    unsigned char *newword;
    unsigned char buf[BUFSIZE];
    gtk_xtext_freeze(GTK_XTEXT(d));

    gtk_label_get(GTK_LABEL(GTK_BIN(w)->child), (gchar**) &newword);
    get_curword(GTK_XTEXT(d), buf, &start, &end);

    gtk_xtext_set_point(GTK_XTEXT(d), end);
    gtk_xtext_backward_delete(GTK_XTEXT(d), end - start);
    gtk_xtext_insert(GTK_XTEXT(d), NULL, NULL, NULL, newword, strlen(newword));

    gtk_xtext_thaw(GTK_XTEXT(d));
}

static GtkMenu *make_menu(GList *l, GtkXText *gtktext) 
{
    GtkWidget *menu, *item;
    unsigned char *caption;
    menu = gtk_menu_new(); 
    
        caption = g_strdup_printf(_("Not in dictionary: %s"), (unsigned char*)l->data);
        item = gtk_menu_item_new_with_label(caption);
        /* I'd like to make it so this item is never selectable, like
         * the menu titles in the GNOME panel... unfortunately, the GNOME
         * panel creates their own custom widget to do this! */
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

        item = gtk_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_append(GTK_MENU(menu), item);

        l = l->next;
        if (l == NULL) {
            item = gtk_menu_item_new_with_label(_("(no suggestions)"));
            gtk_widget_show(item);
            gtk_menu_append(GTK_MENU(menu), item);
        } else {
            GtkWidget *curmenu = menu;
            int count = 0;
            do {
                if (l->data == NULL && l->next != NULL) {
                    count = 0;
                    curmenu = gtk_menu_new();
                    item = gtk_menu_item_new_with_label(_("Others..."));
                    gtk_widget_show(item);
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
                    gtk_menu_append(GTK_MENU(curmenu), item);
                    l = l->next;
                } else if (count > MENUCOUNT) {
                    count -= MENUCOUNT;
                    item = gtk_menu_item_new_with_label(_("More..."));
                    gtk_widget_show(item);
                    gtk_menu_append(GTK_MENU(curmenu), item);
                    curmenu = gtk_menu_new();
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
                }
                item = gtk_menu_item_new_with_label((unsigned char*)l->data);
                gtk_signal_connect(GTK_OBJECT(item), "activate",
                                   GTK_SIGNAL_FUNC(replace_word), gtktext);
                gtk_widget_show(item);
                gtk_menu_append(GTK_MENU(curmenu), item);
                count++;
            } while ((l = l->next) != NULL);
        }
    return GTK_MENU(menu);
}

static void popup_menu(GtkXText *gtktext, GdkEventButton *eb) 
{
    unsigned char buf[BUFSIZE];
    GList *list, *l;
    if (!get_curword(gtktext, buf, NULL, NULL)) return ;
    if (buf == NULL) return ;
    list = misspelled_suggest(buf);
    if (list != NULL) {
        gtk_menu_popup(make_menu(list, gtktext), NULL, NULL, NULL, NULL,
                       eb->button, eb->time);
        for (l = list; l != NULL; l = l->next)
            g_free(l->data);
        g_list_free(list);
    }
}

/* ok, this is pretty wacky:
 * we need to let the right-mouse-click go through, so it moves the cursor,
 * but we *can't* let it go through, because GtkText interprets rightclicks as
 * weird selection modifiers.
 *
 * so what do we do?  forge rightclicks as leftclicks, then popup the menu.
 * HACK HACK HACK.
 */
static gint button_press_intercept_cb(GtkXText *gtktext, GdkEvent *e, gpointer d) 
{
    GdkEventButton *eb;
    gboolean retval;

    if (!gtkspell_running()) return FALSE;

    if (e->type != GDK_BUTTON_PRESS) return FALSE;
    eb = (GdkEventButton*) e;

    if (eb->button != 3) return FALSE;

    /* forge the leftclick */
    eb->button = 1;

    gtk_signal_handler_block_by_func(GTK_OBJECT(gtktext),
                                     GTK_SIGNAL_FUNC(button_press_intercept_cb), d);
    gtk_signal_emit_by_name(GTK_OBJECT(gtktext), "button-press-event",
                            e, &retval);
    gtk_signal_handler_unblock_by_func(GTK_OBJECT(gtktext),
                                       GTK_SIGNAL_FUNC(button_press_intercept_cb), d);
    gtk_signal_emit_stop_by_name(GTK_OBJECT(gtktext), "button-press-event");

    /* now do the menu wackiness */
    popup_menu(gtktext, eb);
    return TRUE;
}

void gtkspell_uncheck_all(GtkXText *gtktext) 
{
    int origpos;
    unsigned char *text;
    float adj_value;
    adj_value = gtktext->vadj->value;
    gtk_xtext_freeze(gtktext);
    origpos = gtk_editable_get_position(GTK_EDITABLE(gtktext));
    text = gtk_editable_get_chars(GTK_EDITABLE(gtktext), 0, -1);
    gtk_xtext_set_point(gtktext, 0);
    gtk_xtext_forward_delete(gtktext, gtk_xtext_get_length(gtktext));
    gtk_xtext_insert(gtktext, NULL, NULL, NULL, text, strlen(text));
    gtk_xtext_thaw(gtktext);

    gtk_editable_set_position(GTK_EDITABLE(gtktext), origpos);
    gtk_adjustment_set_value(gtktext->vadj, adj_value);
}

void gtkspell_attach(GtkXText *gtktext) 
{
//    if (prefs_common.auto_makeispell) {
        gtk_signal_connect(GTK_OBJECT(gtktext), "insert-text",
                           GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
        gtk_signal_connect_after(GTK_OBJECT(gtktext), "delete-text",
                                 GTK_SIGNAL_FUNC(entry_delete_cb), NULL);
//    };
    gtk_signal_connect(GTK_OBJECT(gtktext), "button-press-event",
                       GTK_SIGNAL_FUNC(button_press_intercept_cb), NULL);

}

void gtkspell_detach(GtkXText *gtktext) 
{
//    if (prefs_common.auto_makeispell) {
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_insert_cb), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                      GTK_SIGNAL_FUNC(entry_delete_cb), NULL);
//    };
    gtk_signal_disconnect_by_func(GTK_OBJECT(gtktext),
                                  GTK_SIGNAL_FUNC(button_press_intercept_cb), NULL);

    gtkspell_uncheck_all(gtktext);
}

static void sigchld(int param) 
{
    int retstat, retval;

    debug_print(_("*** SIGCHLD called. (ispell pid: %i)\n"), spell_pid);
    while ((retval = waitpid( -1, &retstat, WNOHANG)) > 0) {
        if (retval == spell_pid) {
            debug_print(_("*** SIGCHLD called for ispell. (pid: %i)\n"), spell_pid);
            spell_pid = -1;
        }
    }
}

static void set_up_signal() 
{
    /* RETSIGTYPE is found in autoconf's config.h */
#ifdef RETSIGTYPE
    typedef RETSIGTYPE (*sighandler)(int);
    signal(SIGCHLD, (sighandler)sigchld);
#else
    /* otherwise, just hope it works */
    signal(SIGCHLD, sigchld);
#endif
}

/*** Sylpheed (Claws) ***/

static GSList *create_empty_dictionary_list(void)
{
	GSList *list = NULL;
	Dictionary *dict;
	
	dict = g_new0(Dictionary, 1);
	dict->name = g_strdup(_("None"));
	dict->path = NULL;
	return g_slist_append(list, dict);
}

/* gtkspell_get_dictionary_list() - returns list of dictionary names and the full 
 * path of file names. */
GSList *gtkspell_get_dictionary_list(const gchar *ispell_path)
{
	GSList *list;
	gchar *dict_path, *tmp, *prevdir;
	GSList *walk;
	Dictionary *dict;
	DIR *dir;
	struct dirent *ent;

	list = NULL;

	/* ASSUME: ispell_path is full path */
	dict_path = strstr(ispell_path + 1, G_DIR_SEPARATOR_S);
	tmp = g_strndup(ispell_path, dict_path - ispell_path);
	
	/* ASSUME: ispell dictionaries in PREFIX/lib/ispell */
	dict_path = g_strconcat(tmp, G_DIR_SEPARATOR_S, "lib", G_DIR_SEPARATOR_S, "ispell", NULL);
	g_free(tmp);

#ifdef USE_THREADS
#warning TODO: no directory change
#endif
	
	prevdir = g_get_current_dir();
	if (chdir(dict_path) <0) {
		FILE_OP_ERROR(dict_path, "chdir");
		g_free(prevdir);
		g_free(dict_path);
		return create_empty_dictionary_list();
	}

	debug_print(_("Checking for dictionaries in %s\n"), dict_path);

	if (NULL != (dir = opendir("."))) {
		while (NULL != (ent = readdir(dir))) {
			/* search for hash table */
			if (NULL != (tmp = strstr(ent->d_name, ".hash"))) {
				dict = g_new0(Dictionary, 1);
				dict->name = g_strndup(ent->d_name, tmp - ent->d_name);
				dict->path = g_strconcat(dict_path, G_DIR_SEPARATOR_S, ent->d_name, NULL);
				debug_print(_("Found dictionary %s\n"), dict->path);
				list = g_slist_append(list, dict);
			}
		}			
		closedir(dir);
	}
	else {
		FILE_OP_ERROR(dict_path, "opendir");
		debug_print(_("No dictionary found\n"));
		list = create_empty_dictionary_list();
	}
	chdir(prevdir);
	g_free(dict_path);
	g_free(prevdir);
	return list;
}

void gtkspell_free_dictionary_list(GSList *list)
{
	Dictionary *dict;
	GSList *walk;
	for (walk = list; walk != NULL; walk = g_slist_next(walk))
		if (walk->data) {
			dict = (Dictionary *) walk->data;
			if (dict->name)
				g_free(dict->name);
			if (dict->path)
				g_free(dict->path);
			g_free(dict);
		}				
	g_slist_free(list);
}

static void dictionary_option_menu_item_data_destroy(gpointer data)
{
	gchar *str = (gchar *) data;

	if (str)
		g_free(str);
}

GtkWidget *gtkspell_dictionary_option_menu_new(const gchar *ispell_path)
{
	GSList *dict_list, *tmp;
	GtkWidget *item;
	GtkWidget *menu;
	Dictionary *dict;

	dict_list = gtkspell_get_dictionary_list(ispell_path);
	g_return_val_if_fail(dict_list, NULL);

	menu = gtk_menu_new();
	
	for (tmp = dict_list; tmp != NULL; tmp = g_slist_next(tmp)) {
		dict = (Dictionary *) tmp->data;
		item = gtk_menu_item_new_with_label(dict->name);
		if (dict->path)
			gtk_object_set_data_full(GTK_OBJECT(item), "full_path",
					 g_strdup(dict->path), 
					 dictionary_option_menu_item_data_destroy);
		gtk_menu_append(GTK_MENU(menu), item);					 
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	gtkspell_free_dictionary_list(dict_list);

	return menu;
}

gchar *gtkspell_get_dictionary_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	gchar *result;

	g_return_val_if_fail(GTK_IS_MENU(menu), NULL);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	
	result = gtk_object_get_data(GTK_OBJECT(menuitem), "full_path");
	g_return_val_if_fail(result, NULL);

	return g_strdup(result);
}

