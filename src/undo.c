/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* code ported from gedit */

#include <config.h>
#include <string.h> /* For strlen */

#include "prefs_common.h"
#include "undo.h"
#include "utils.h"
#include "compose.h"
#include "gtkstext.h"

typedef struct _UndoInfo UndoInfo;

struct _UndoInfo
{
        UndoAction action;
        gchar *text;
        gint start_pos;
        gint end_pos;
        gfloat window_position;
        gint mergeable;
};


/**
 * compose_undo_free_list:
 * @list_pointer: list to be freed
 *
 * frees and undo structure list
 **/
void compose_undo_free_list (GList ** list_pointer)
{
	gint n;
	UndoInfo *nth_redo;
        GList *list = * list_pointer;

	if (list==NULL)
	{
		return;
	}

	debug_print("length of list: %d\n", g_list_length (list));

	/* DINH V. Hoa */
	/* a better list traversing should be implemented, here
	   it is O(n^2), it can be O(n) */
	for (n=0; n < g_list_length (list); n++)
	{
		nth_redo = g_list_nth_data (list, n);
		if (nth_redo==NULL){
			g_warning ("nth_redo==NULL");
		}
		g_free (nth_redo->text);
		g_free (nth_redo);
	}

        g_list_free (list);
	*list_pointer = NULL;
}

/**
 * compose_set_undo:
 *
 * Change the sensivity of the menuentries undo and redo
 **/
static /* DINH V.Hoa */
void compose_set_undo (Compose *compose, gint undo_state, gint redo_state)
{
	GtkItemFactory *ifactory;

        debug_print ("\nSet_undo. view:0x%x  UNDO:%i  REDO:%i\n",
                 (gint) compose,
                 undo_state,
                 redo_state);

	g_return_if_fail (compose != NULL);

	ifactory = gtk_item_factory_from_widget(compose->menubar);

        /* Set undo */
        switch (undo_state)
        {
        case UNDO_STATE_TRUE:
                if (!compose->undo_state)
                {
                        compose->undo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Undo", TRUE);
                }
                break;
        case UNDO_STATE_FALSE:
                if (compose->undo_state)
                {
                        compose->undo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
                }
                break;
        case UNDO_STATE_UNCHANGED:
                break;
        case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Undo", compose->undo_state);
                break;
        default:
                g_warning ("Undo state not recognized");
        }

        /* Set redo*/
        switch (redo_state)
        {
        case UNDO_STATE_TRUE:
                if (!compose->redo_state)
                {
                        compose->redo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Redo", TRUE);
                }
                break;
        case UNDO_STATE_FALSE:
                if (compose->redo_state)
                {
                        compose->redo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);
                }
                break;
        case UNDO_STATE_UNCHANGED:
                break;
        case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Redo", compose->redo_state);
                break;
        default:
                g_warning ("Redo state not recognized");
        }
}

/**
 * compose_undo_check_size:
 * @compose: document to check
 *
 * Checks that the size of compose->undo does not excede settings->undo_levels and
 * frees any undo level above sett->undo_level.
 *
 **/

/* DINH V. Hoa */
/* remove only the first-inserted elements if necessary
   whenever we add a new element
   remove before inserting a new element */

static void compose_undo_check_size (Compose *compose)
{
        gint n;
        UndoInfo *nth_undo;
	gint undo_levels = 100;

        if (undo_levels < 1)
                return;

        /* No need to check for the redo list size since the undo
           list gets freed on any call to compose_undo_add */
        if (g_list_length (compose->undo) > undo_levels && undo_levels > 0)
        {
                gint start;
                gint end;

                start = g_list_length (compose->undo);
                end   = undo_levels;
                for (n = start; n >= end;  n--)
                {
                        nth_undo = g_list_nth_data (compose->undo, n - 1);
                        compose->undo = g_list_remove (compose->undo, nth_undo);
                        g_free (nth_undo->text);
                        g_free (nth_undo);
                }
        }

}

/**
 * compose_undo_merge:
 * @last_undo:
 * @start_pos:
 * @end_pos:
 * @action:
 *
 * This function tries to merge the undo object at the top of
 * the stack with a new set of data. So when we undo for example
 * typing, we can undo the whole word and not each letter by itself
 *
 * Return Value: TRUE is merge was sucessful, FALSE otherwise
 **/
static gint compose_undo_merge (GList *list, guint start_pos, guint end_pos, gint action, const guchar* text)
{
        guchar * temp_string;
        UndoInfo * last_undo;

        /* This are the cases in which we will NOT merge :
           1. if (last_undo->mergeable == FALSE)
           [mergeable = FALSE when the size of the undo data was not 1.
           or if the data was size = 1 but = '\n' or if the undo object
           has been "undone" already ]
           2. The size of text is not 1
           3. If the new merging data is a '\n'
           4. If the last char of the undo_last data is a space/tab
           and the new char is not a space/tab ( so that we undo
           words and not chars )
           5. If the type (action) of undo is different from the last one
        Chema */

        if (list==NULL)
                return FALSE;

	/* DINH V. Hoa */
	/* g_list_first or just list->data */

        last_undo = g_list_nth_data (list, 0);

        if (!last_undo->mergeable)
        {
                return FALSE;
        }

        if (end_pos-start_pos != 1)
        {
                last_undo->mergeable = FALSE;
                return FALSE;
        }

	/* DINH V. Hoa */
	/* goto are bad coding */

        if (text[0]=='\n')
                goto compose_undo_do_not_merge;

        if (action != last_undo->action)
                goto compose_undo_do_not_merge;

        if (action == UNDO_ACTION_DELETE)
        {
                if (last_undo->start_pos!=end_pos && last_undo->start_pos != start_pos)
                        goto compose_undo_do_not_merge;

                if (last_undo->start_pos == start_pos)
                {
                        /* Deleted with the delete key */
                        if ( text[0]!=' ' && text[0]!='\t' &&
                             (last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] ==' '
                              || last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] == '\t'))
                                goto compose_undo_do_not_merge;

                        temp_string = g_strdup_printf ("%s%s", last_undo->text, text);
                        g_free (last_undo->text);
                        last_undo->end_pos += 1;
                        last_undo->text = temp_string;
                }
                else
                {
                        /* Deleted with the backspace key */
                        if ( text[0]!=' ' && text[0]!='\t' &&
                             (last_undo->text [0] == ' '
                              || last_undo->text [0] == '\t'))
                                goto compose_undo_do_not_merge;

                        temp_string = g_strdup_printf ("%s%s", text, last_undo->text);
                        g_free (last_undo->text);
                        last_undo->start_pos = start_pos;
                        last_undo->text = temp_string;
                }
        }
        else if (action == UNDO_ACTION_INSERT)
        {
                if (last_undo->end_pos != start_pos)
                        goto compose_undo_do_not_merge;

/*                if ( text[0]!=' ' && text[0]!='\t' &&
                     (last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] ==' '
                      || last_undo->text [last_undo->end_pos-last_undo->start_pos - 1] == '\t'))
                        goto compose_undo_do_not_merge;
*/
                temp_string = g_strdup_printf ("%s%s", last_undo->text, text);
                g_free (last_undo->text);
                last_undo->end_pos = end_pos;
                last_undo->text = temp_string;
        }
        else
                debug_print ("Unknown action [%i] inside undo merge encountered", action);

        debug_print ("Merged: %s\n", text);
        return TRUE;

compose_undo_do_not_merge:
        last_undo->mergeable = FALSE;
        return FALSE;
}

/**
 * compose_undo_add:
 * @text:
 * @start_pos:
 * @end_pos:
 * @action: either UNDO_ACTION_INSERT or UNDO_ACTION_DELETE
 * @compose:
 * @view: The view so that we save the scroll bar position.
 *
 * Adds text to the undo stack. It also performs test to limit the number
 * of undo levels and deltes the redo list
 **/

void compose_undo_add (const gchar *text, gint start_pos, gint end_pos,
                UndoAction action, Compose *compose)
{
        UndoInfo *undo;

        debug_print ("undo_add(%i)*%s*\n", strlen (text), text);

	g_return_if_fail (text != NULL);
        g_return_if_fail (end_pos >= start_pos);

        compose_undo_free_list (&compose->redo);

        /* Set the redo sensitivity */
        compose_set_undo (compose, UNDO_STATE_UNCHANGED, UNDO_STATE_FALSE);

        if (compose_undo_merge (compose->undo, start_pos, end_pos, action, text))
                return;

	debug_print ("New: %s\n", text);

	/* DINH V. Hoa */
	/* build undo objects allocator and destructor functions */

	undo = g_new (UndoInfo, 1);
        undo->text      = g_strdup (text);
        undo->start_pos = start_pos;
        undo->end_pos   = end_pos;
        undo->action    = action;

	undo->window_position = GTK_ADJUSTMENT(GTK_STEXT(compose->text)->vadj)->value;

	if (end_pos-start_pos!=1 || text[0]=='\n')
                undo->mergeable = FALSE;
        else
                undo->mergeable = TRUE;

	compose->undo = g_list_prepend (compose->undo, undo);

	/* DINH V. Hoa */
	/* removal of elements should be done first */

        compose_undo_check_size (compose);

        compose_set_undo (compose, UNDO_STATE_TRUE, UNDO_STATE_UNCHANGED);
}

/**
 * compose_undo_undo:
 * @w: not used
 * @data: not used
 *
 * Executes an undo request on the current document
 **/
void compose_undo_undo (Compose *compose, gpointer data)
{
	UndoInfo *undo;
        guint start_pos, end_pos;

	if (compose->undo == NULL)
		return;

	g_return_if_fail (compose!=NULL);


	/* The undo data we need is always at the top op the
	   stack. So, therefore, the first one */
	undo = g_list_nth_data (compose->undo, 0);
	g_return_if_fail (undo!=NULL);
	undo->mergeable = FALSE;
	compose->redo = g_list_prepend (compose->redo, undo);
	compose->undo = g_list_remove (compose->undo, undo);

	/* Check if there is a selection active */
        start_pos = GTK_EDITABLE(compose->text)->selection_start_pos;
        end_pos   = GTK_EDITABLE(compose->text)->selection_end_pos;
	if ((start_pos > 0 || end_pos > 0) && (start_pos != end_pos))
		gtk_editable_select_region (GTK_EDITABLE(compose->text), 0, 0);

	/* Move the view (scrollbars) to the correct position */
	gtk_adjustment_set_value (GTK_ADJUSTMENT(GTK_STEXT(compose->text)->vadj), undo->window_position);

	switch (undo->action){
	case UNDO_ACTION_DELETE:
		gtk_stext_set_point(GTK_STEXT(compose->text), undo->start_pos);
		gtk_stext_insert (GTK_STEXT(compose->text), NULL, NULL, NULL, undo->text, -1);
		debug_print("UNDO_ACTION_DELETE %s\n",undo->text);
		break;
	case UNDO_ACTION_INSERT:
		gtk_stext_set_point(GTK_STEXT(compose->text), undo->end_pos);
		gtk_stext_backward_delete (GTK_STEXT(compose->text), undo->end_pos-undo->start_pos);
		debug_print("UNDO_ACTION_INSERT %d\n",undo->end_pos-undo->start_pos);
		break;
	default:
		g_assert_not_reached ();
	}

	compose_set_undo (compose, UNDO_STATE_UNCHANGED, UNDO_STATE_TRUE);
	if (g_list_length (compose->undo) == 0)
		compose_set_undo (compose, UNDO_STATE_FALSE, UNDO_STATE_UNCHANGED);
}

/**
 * compose_undo_redo:
 * @w: not used
 * @data: not used
 *
 * executes a redo request on the current document
 **/
void compose_undo_redo (Compose *compose, gpointer data)
{
	UndoInfo *redo;
        guint start_pos, end_pos;

	if (compose->redo == NULL)
		return;

	if (compose==NULL)
		return;

	redo = g_list_nth_data (compose->redo, 0);
	g_return_if_fail (redo!=NULL);
	compose->undo = g_list_prepend (compose->undo, redo);
	compose->redo = g_list_remove (compose->redo, redo);

	/* Check if there is a selection active */
        start_pos = GTK_EDITABLE(compose->text)->selection_start_pos;
        end_pos   = GTK_EDITABLE(compose->text)->selection_end_pos;
	if ((start_pos > 0 || end_pos > 0) && (start_pos != end_pos))
		gtk_editable_select_region (GTK_EDITABLE(compose->text), 0, 0);

	/* Move the view to the right position. */
	gtk_adjustment_set_value (GTK_ADJUSTMENT(GTK_STEXT(compose->text)->vadj), redo->window_position);

	switch (redo->action){
	case UNDO_ACTION_INSERT:
		gtk_stext_set_point(GTK_STEXT(compose->text), redo->start_pos);
		gtk_stext_insert (GTK_STEXT(compose->text), NULL, NULL, NULL, redo->text, -1);
		debug_print("UNDO_ACTION_DELETE %s\n",redo->text);
		break;
	case UNDO_ACTION_DELETE:
		gtk_stext_set_point(GTK_STEXT(compose->text), redo->end_pos);
		gtk_stext_backward_delete (GTK_STEXT(compose->text), redo->end_pos-redo->start_pos);
		debug_print("UNDO_ACTION_INSERT %d\n",redo->end_pos-redo->start_pos);
		break;
	default:
		g_assert_not_reached ();
	}

	compose_set_undo (compose, UNDO_STATE_TRUE, UNDO_STATE_UNCHANGED);
	if (g_list_length (compose->redo) == 0)
		compose_set_undo (compose, UNDO_STATE_UNCHANGED, UNDO_STATE_FALSE);
}
