/*
 * This program is based on gtkflist.c
 */

#include <stdlib.h>

#include "gtksctree.h"
#include "sylpheed-marshal.h"
#include "stock_pixmap.h"

#define CLIST_UNFROZEN(clist)     (((GtkCList*) (clist))->freeze_count == 0)
#define CLIST_REFRESH(clist)    G_STMT_START { \
  if (CLIST_UNFROZEN (clist)) \
    GTK_CLIST_GET_CLASS (clist)->refresh ((GtkCList*) (clist)); \
} G_STMT_END
#define CELL_SPACING               1
#define CLIST_OPTIMUM_SIZE         64
#define COLUMN_INSET               3

enum {
	ROW_POPUP_MENU,
	EMPTY_POPUP_MENU,
	OPEN_ROW,
	START_DRAG,
	LAST_SIGNAL
};

static GdkPixmap *emptyxpm = NULL;
static GdkBitmap *emptyxpmmask = NULL;

static void gtk_sctree_class_init (GtkSCTreeClass *class);
static void gtk_sctree_init (GtkSCTree *sctree);

static gint gtk_sctree_button_press (GtkWidget *widget, GdkEventButton *event);
static gint gtk_sctree_button_release (GtkWidget *widget, GdkEventButton *event);
static gint gtk_sctree_motion (GtkWidget *widget, GdkEventMotion *event);
static void gtk_sctree_drag_begin (GtkWidget *widget, GdkDragContext *context);
static void gtk_sctree_drag_end (GtkWidget *widget, GdkDragContext *context);
static void gtk_sctree_drag_data_get (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *data, guint info, guint time);
static void gtk_sctree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time);
static gboolean gtk_sctree_drag_motion (GtkWidget *widget, GdkDragContext *context,
				       gint x, gint y, guint time);
static gboolean gtk_sctree_drag_drop (GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, guint time);
static void gtk_sctree_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					  gint x, gint y, GtkSelectionData *data,
					  guint info, guint time);

static void gtk_sctree_clear (GtkCList *clist);
static void gtk_sctree_collapse (GtkCTree *ctree, GtkCTreeNode *node);
       
static void stree_sort (GtkCTree *ctree, GtkCTreeNode  *node, gpointer data);
void gtk_sctree_sort_node (GtkCTree *ctree, GtkCTreeNode *node);
void gtk_sctree_sort_recursive (GtkCTree *ctree, GtkCTreeNode *node);

static void gtk_sctree_link (GtkCTree *ctree,
			GtkCTreeNode  *node,
			GtkCTreeNode  *parent,
			GtkCTreeNode  *sibling,
			gboolean       update_focus_row);

static void gtk_sctree_unlink (GtkCTree      *ctree, 
			GtkCTreeNode  *node,
			gboolean       update_focus_row);

static void stree_update_level (GtkCTree      *ctree, 
			GtkCTreeNode  *node, 
			gpointer       data);

static GtkCTreeNode * gtk_sctree_last_visible (GtkCTree     *ctree,
					      GtkCTreeNode *node);
static void gtk_sctree_real_tree_expand            (GtkCTree      *ctree,
						 GtkCTreeNode  *node);
static void
sreal_tree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent, 
		GtkCTreeNode *new_sibling);

static GtkCTreeClass *parent_class;

static guint sctree_signals[LAST_SIGNAL];


/**
 * gtk_sctree_get_type:
 * @void: 
 * 
 * Creates the GtkSCTree class and its type information
 * 
 * Return value: The type ID for GtkSCTreeClass
 **/
GType
gtk_sctree_get_type (void)
{
	static GType sctree_type = 0;

	if (!sctree_type) {
		GTypeInfo sctree_info = {
			sizeof (GtkSCTreeClass),

			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,

			(GClassInitFunc) gtk_sctree_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,	/* class_data */

			sizeof (GtkSCTree),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_sctree_init,
		};

		sctree_type = g_type_register_static (GTK_TYPE_CTREE, "GtkSCTree", &sctree_info, (GTypeFlags)0);
	}

	return sctree_type;
}

/* Standard class initialization function */
static void
gtk_sctree_class_init (GtkSCTreeClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;
	GtkCTreeClass *ctree_class;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	clist_class = (GtkCListClass *) klass;
	ctree_class = (GtkCTreeClass *) klass;

	parent_class = gtk_type_class (gtk_ctree_get_type ());

	sctree_signals[ROW_POPUP_MENU] =
		g_signal_new ("row_popup_menu",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkSCTreeClass, row_popup_menu),
			      NULL, NULL,
			      sylpheed_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      GDK_TYPE_EVENT);
	sctree_signals[EMPTY_POPUP_MENU] =
		g_signal_new ("empty_popup_menu",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkSCTreeClass, empty_popup_menu),
			      NULL, NULL,
			      sylpheed_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      GDK_TYPE_EVENT);
	sctree_signals[OPEN_ROW] =
		g_signal_new ("open_row",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkSCTreeClass, open_row),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	sctree_signals[START_DRAG] =
		g_signal_new ("start_drag",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkSCTreeClass, start_drag),
			      NULL, NULL,
			      sylpheed_marshal_VOID__INT_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT,
			      GDK_TYPE_EVENT);

	/* gtk_object_class_add_signals (object_class, sctree_signals, LAST_SIGNAL); */

	clist_class->clear = gtk_sctree_clear;
        ctree_class->tree_collapse = gtk_sctree_collapse;
	ctree_class->tree_expand = gtk_sctree_real_tree_expand;
	ctree_class->tree_move = sreal_tree_move;
	
	widget_class->button_press_event = gtk_sctree_button_press;
	widget_class->button_release_event = gtk_sctree_button_release;
	widget_class->motion_notify_event = gtk_sctree_motion;
	widget_class->drag_begin = gtk_sctree_drag_begin;
	widget_class->drag_end = gtk_sctree_drag_end;
	widget_class->drag_data_get = gtk_sctree_drag_data_get;
	widget_class->drag_leave = gtk_sctree_drag_leave;
	widget_class->drag_motion = gtk_sctree_drag_motion;
	widget_class->drag_drop = gtk_sctree_drag_drop;
	widget_class->drag_data_received = gtk_sctree_drag_data_received;
}

/* Standard object initialization function */
static void
gtk_sctree_init (GtkSCTree *sctree)
{
	sctree->anchor_row = NULL;

	/* GtkCTree does not specify pointer motion by default */
	gtk_widget_add_events (GTK_WIDGET (sctree), GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events (GTK_WIDGET (sctree), GDK_POINTER_MOTION_MASK);
}

/* Get information the specified row is selected. */

static gboolean
row_is_selected(GtkSCTree *sctree, gint row)
{
	GtkCListRow *clist_row;
	clist_row =  g_list_nth (GTK_CLIST(sctree)->row_list, row)->data;
	return clist_row ? clist_row->state == GTK_STATE_SELECTED : FALSE;
}

/* Selects the rows between the anchor to the specified row, inclusive.  */
static void
select_range (GtkSCTree *sctree, gint row)
{
	gint prev_row;
	gint min, max;
	gint i;
	GList *node;
	if (sctree->anchor_row == NULL) {
		prev_row = row;
		sctree->anchor_row = gtk_ctree_node_nth(GTK_CTREE(sctree), row);
	} else
		prev_row = g_list_position(GTK_CLIST(sctree)->row_list,
					   (GList *)sctree->anchor_row);

	if (row < prev_row) {
		min = row;
		max = prev_row;
		GTK_CLIST(sctree)->focus_row = max;
	} else {
		min = prev_row;
		max = row;
	}
	sctree->selecting_range = TRUE;
	
	if (max < min) {
		int t = min;
		min = max;
		max = t;
	}
	node = g_list_nth((GTK_CLIST(sctree))->row_list, min);
	for (i = min; i < max; i++) {
		if (node && GTK_CTREE_ROW (node)->row.selectable) {
			g_signal_emit_by_name(G_OBJECT(sctree), "tree_select_row",
				node, -1);
		}
		node = node->next;
	}

	sctree->selecting_range = FALSE;
	gtk_clist_select_row (GTK_CLIST (sctree), max, -1);
}

/* Handles row selection according to the specified modifier state */
/* in certain cases, we arrive here from a function knowing the GtkCTreeNode, and having
 * already slowly found row using g_list_position. In which case, _node will be non-NULL
 * to avoid this function having to slowly find it with g_list_nth. */
static void
select_row (GtkSCTree *sctree, gint row, gint col, guint state, GtkCTreeNode *_node)
{
	gboolean range, additive;
	g_return_if_fail (sctree != NULL);
	g_return_if_fail (GTK_IS_SCTREE (sctree));
    
	range = ((state & GDK_SHIFT_MASK) != 0) &&
		(GTK_CLIST(sctree)->selection_mode != GTK_SELECTION_SINGLE) &&
		(GTK_CLIST(sctree)->selection_mode != GTK_SELECTION_BROWSE);
	additive = ((state & GDK_CONTROL_MASK) != 0) &&
		   (GTK_CLIST(sctree)->selection_mode != GTK_SELECTION_SINGLE) &&
		   (GTK_CLIST(sctree)->selection_mode != GTK_SELECTION_BROWSE);

	/* heavy GUI updates will be done only if we're selecting a range.
	 * additive selection is ctrl-click, where the user is the slow factor.
	 * So we only freeze the list if selecting a range (potentially thousands 
	 * of lines. */
	if (range)
		gtk_clist_freeze (GTK_CLIST (sctree));

	GTK_CLIST(sctree)->focus_row = row;

	if (!additive) {
		/* if this selection isn't additive, we have to unselect what
		 * is selected. Here, heavy GUI updates can occur if we have 
		 * a big selection. See if more than one line is selected, in
		 * which case, freeze, else don't. */

		gboolean should_freeze = FALSE;
		if (GTK_CLIST(sctree)->selection
		 && GTK_CLIST(sctree)->selection->next) {
		 	should_freeze = TRUE;
			sctree->selecting_range = TRUE;
			gtk_clist_freeze (GTK_CLIST (sctree));
		}

		gtk_clist_unselect_all (GTK_CLIST (sctree));

		if (should_freeze) {
			gtk_clist_thaw (GTK_CLIST (sctree));
			sctree->selecting_range = FALSE;
		}
	}

	if (!range) {
		GtkCTreeNode *node;

		node = _node ? _node : gtk_ctree_node_nth (GTK_CTREE(sctree), row);

		/*No need to manage overlapped list*/
		if (additive) {
			if (row_is_selected(sctree, row))
				gtk_clist_unselect_row (GTK_CLIST (sctree), row, col);
			else
				g_signal_emit_by_name
					(G_OBJECT (sctree),
					 "tree_select_row", node, col);
		} else {
			g_signal_emit_by_name
				(G_OBJECT (sctree),
				 "tree_select_row", node, col);
		}
		sctree->anchor_row = node;
	} else
		select_range (sctree, row);
	
	if (range)
		gtk_clist_thaw (GTK_CLIST (sctree));
}

/* Our handler for button_press events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_sctree_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkSCTree *sctree;
	GtkCList *clist;
	gboolean on_row;
	gint row;
	gint col;
	gint retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SCTREE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	sctree = GTK_SCTREE (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	if (on_row && !GTK_WIDGET_HAS_FOCUS(widget))
		gtk_widget_grab_focus (widget);

	if (gtk_ctree_is_hot_spot (GTK_CTREE(sctree), event->x, event->y)) {
		gtk_ctree_toggle_expansion
			(GTK_CTREE(sctree), 
			 gtk_ctree_node_nth(GTK_CTREE(sctree), row));
		return TRUE;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1 || event->button == 2) {
			if (event->button == 2)
				event->state &= ~(GDK_SHIFT_MASK | GDK_CONTROL_MASK);
			if (on_row) {
				/* Save the mouse info for DnD */
				sctree->dnd_press_button = event->button;
				sctree->dnd_press_x = event->x;
				sctree->dnd_press_y = event->y;

				/* Handle selection */
				if ((row_is_selected (sctree, row)
				     && !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				    || ((event->state & GDK_CONTROL_MASK)
					&& !(event->state & GDK_SHIFT_MASK))) {
					sctree->dnd_select_pending = TRUE;
					sctree->dnd_select_pending_state = event->state;
					sctree->dnd_select_pending_row = row;
				} else {
					select_row (sctree, row, col, event->state, NULL);
				}
			} else {
				sctree->selecting_range = TRUE;
				gtk_clist_unselect_all (clist);
				sctree->selecting_range = FALSE;
			}

			retval = TRUE;
		} else if (event->button == 3) {
			/* Emit *_popup_menu signal*/
			if (on_row) {
				if (!row_is_selected(sctree,row))
					select_row (sctree, row, col, 0, NULL);
				g_signal_emit (G_OBJECT (sctree),
						 sctree_signals[ROW_POPUP_MENU],
						 0, event);
			} else {
				sctree->selecting_range = TRUE;
				gtk_clist_unselect_all(clist);
				sctree->selecting_range = FALSE;
				g_signal_emit (G_OBJECT (sctree),
						 sctree_signals[EMPTY_POPUP_MENU],
						 0, event);
			}
			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button != 1)
			break;

		sctree->dnd_select_pending = FALSE;
		sctree->dnd_select_pending_state = 0;

		if (on_row)
			g_signal_emit (G_OBJECT (sctree),
				       sctree_signals[OPEN_ROW], 0);

		retval = TRUE;
		break;

	default:
		break;
	}

	return retval;
}

/* Our handler for button_release events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_sctree_button_release (GtkWidget *widget, GdkEventButton *event)
{
	GtkSCTree *sctree;
	GtkCList *clist;
	gint on_row;
	gint row, col;
	gint retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SCTREE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	sctree = GTK_SCTREE (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->button_release_event) (widget, event);

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	if (!(event->button == 1 || event->button == 2))
		return FALSE;

	sctree->dnd_press_button = 0;
	sctree->dnd_press_x = 0;
	sctree->dnd_press_y = 0;

	if (on_row) {
		if (sctree->dnd_select_pending) {
			select_row (sctree, row, col, sctree->dnd_select_pending_state, NULL);
			sctree->dnd_select_pending = FALSE;
			sctree->dnd_select_pending_state = 0;
		}

		retval = TRUE;
	}

	return retval;
}

/* Our handler for motion_notify events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_sctree_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GtkSCTree *sctree;
	GtkCList *clist;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SCTREE (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	sctree = GTK_SCTREE (widget);
	clist = GTK_CLIST (widget);

	if (event->window != clist->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	if (!((sctree->dnd_press_button == 1 && (event->state & GDK_BUTTON1_MASK))
	      || (sctree->dnd_press_button == 2 && (event->state & GDK_BUTTON2_MASK))))
		return FALSE;

	/* This is the same threshold value that is used in gtkdnd.c */

	if (MAX (ABS (sctree->dnd_press_x - event->x),
		 ABS (sctree->dnd_press_y - event->y)) <= 3)
		return FALSE;

	/* Handle any pending selections */

	if (sctree->dnd_select_pending) {
		if (!row_is_selected(sctree,sctree->dnd_select_pending_row))
			select_row (sctree,
				    sctree->dnd_select_pending_row,
				    -1,
				    sctree->dnd_select_pending_state,
				    NULL);

		sctree->dnd_select_pending = FALSE;
		sctree->dnd_select_pending_state = 0;
	}

	g_signal_emit (G_OBJECT (sctree),
		       sctree_signals[START_DRAG],
		       0,
		       sctree->dnd_press_button,
		       event);
	return TRUE;
}

/* We override the drag_begin signal to do nothing */
static void
gtk_sctree_drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_end signal to do nothing */
static void
gtk_sctree_drag_end (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_data_get signal to do nothing */
static void
gtk_sctree_drag_data_get (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *data, guint info, guint time)
{
	/* nothing */
}

/* We override the drag_leave signal to do nothing */
static void
gtk_sctree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	/* nothing */
}

/* We override the drag_motion signal to do nothing */
static gboolean
gtk_sctree_drag_motion (GtkWidget *widget, GdkDragContext *context,
				   gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_drop signal to do nothing */
static gboolean
gtk_sctree_drag_drop (GtkWidget *widget, GdkDragContext *context,
				 gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_data_received signal to do nothing */
static void
gtk_sctree_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					  gint x, gint y, GtkSelectionData *data,
					  guint info, guint time)
{
	/* nothing */
}

/* Our handler for the clear signal of the clist.  We have to reset the anchor
 * to null.
 */
static void
gtk_sctree_clear (GtkCList *clist)
{
	GtkSCTree *sctree;

	g_return_if_fail (clist != NULL);
	g_return_if_fail (GTK_IS_SCTREE (clist));

	sctree = GTK_SCTREE (clist);
	sctree->anchor_row = NULL;

	if (((GtkCListClass *)parent_class)->clear)
		(* ((GtkCListClass *)parent_class)->clear) (clist);
}

/* Our handler for the change_focus_row_expansion signal of the ctree.  
 We have to set the anchor to parent visible node.
 */
static void 
gtk_sctree_collapse (GtkCTree *ctree, GtkCTreeNode *node)
{
	g_return_if_fail (ctree != NULL);
	g_return_if_fail (GTK_IS_SCTREE (ctree));

        (* parent_class->tree_collapse) (ctree, node);
	GTK_SCTREE(ctree)->anchor_row =
		gtk_ctree_node_nth(ctree, GTK_CLIST(ctree)->focus_row);
}

GtkWidget *gtk_sctree_new_with_titles (gint columns, gint tree_column, 
				       gchar *titles[])
{
	GtkWidget *widget;
                                                                                                            
	g_return_val_if_fail (columns > 0, NULL);
	g_return_val_if_fail (tree_column >= 0, NULL);
                                                                                                            
	widget = gtk_widget_new (TYPE_GTK_SCTREE,
				 "n_columns", columns,
				 "tree_column", tree_column,
				 NULL);
	if (titles) {
		GtkCList *clist = GTK_CLIST (widget);
		guint i;

		for (i = 0; i < columns; i++)
			gtk_clist_set_column_title (clist, i, titles[i]);
		gtk_clist_column_titles_show (clist);
	}

	return widget;
}

void gtk_sctree_select (GtkSCTree *sctree, GtkCTreeNode *node)
{
	select_row(sctree, 
		   g_list_position(GTK_CLIST(sctree)->row_list, (GList *)node),
		   -1, 0, node);
}

void gtk_sctree_select_with_state (GtkSCTree *sctree, GtkCTreeNode *node, int state)
{
	select_row(sctree, 
		   g_list_position(GTK_CLIST(sctree)->row_list, (GList *)node),
		   -1, state, node);
}

void gtk_sctree_unselect_all (GtkSCTree *sctree)
{
	sctree->selecting_range = TRUE;
	gtk_clist_freeze(GTK_CLIST(sctree));
	gtk_clist_unselect_all(GTK_CLIST(sctree));
	gtk_clist_thaw(GTK_CLIST(sctree));
	sctree->selecting_range = FALSE;
	sctree->anchor_row = NULL;
}

void gtk_sctree_set_anchor_row (GtkSCTree *sctree, GtkCTreeNode *node)
{
	sctree->anchor_row = node;
}

void gtk_sctree_remove_node (GtkSCTree *sctree, GtkCTreeNode *node)
{
	if (sctree->anchor_row == node)
		sctree->anchor_row = NULL;
	gtk_ctree_remove_node(GTK_CTREE(sctree), node);
}

/***********************************************************
 *             Tree sorting functions                      *
 ***********************************************************/

static void sink(GtkCList *clist, GPtrArray *numbers, gint root, gint bottom)
{
	gint j, k ;
	GtkCTreeNode *temp;

	j = 2 * root;
	k = j + 1;

	/* find the maximum element of numbers[root],
	   numbers[2*root] and numbers[2*root+1] */
	if (j <= bottom) {
		if (clist->compare( clist, GTK_CTREE_ROW (g_ptr_array_index(numbers, root)),
				    GTK_CTREE_ROW(g_ptr_array_index( numbers, j))) >= 0)
			j = root;
		if (k <= bottom)
			if (clist->compare( clist, GTK_CTREE_ROW (g_ptr_array_index(numbers, k)),
					    GTK_CTREE_ROW (g_ptr_array_index( numbers, j))) > 0)
				j = k;
		/* if numbers[root] wasn't the maximum element then
		   sink again */
		if (root != j) {
			temp = g_ptr_array_index( numbers,root);
			g_ptr_array_index( numbers, root) = g_ptr_array_index( numbers, j);
			g_ptr_array_index( numbers, j) = temp;
			sink( clist, numbers, j, bottom);
		}
	}
}

static void heap_sort(GtkCList *clist, GPtrArray *numbers, gint array_size)
{
	gint i;
	GtkCTreeNode *temp;
	
	/* build the Heap */
	for (i = (array_size / 2); i >= 1; i--)
		sink( clist, numbers, i, array_size);
	/* output the Heap */
	for (i = array_size; i >= 2; i--) {
		temp = g_ptr_array_index( numbers, 1);
		g_ptr_array_index( numbers, 1) = g_ptr_array_index( numbers, i);
		g_ptr_array_index( numbers, i) = temp;
		sink( clist, numbers, 1, i-1);
	}
}

static void
stree_sort (GtkCTree    *ctree,
	   GtkCTreeNode *node,
	   gpointer      data)
{
	GtkCTreeNode *list_start, *work, *next;
	GPtrArray *row_array, *viewable_array;
	GtkCList *clist;
	gint i;

	clist = GTK_CLIST (ctree);

	if (node)
		work = GTK_CTREE_ROW (node)->children;
	else
		work = GTK_CTREE_NODE (clist->row_list);

	row_array = g_ptr_array_new();
	viewable_array = g_ptr_array_new();

	if (work) {
		g_ptr_array_add( row_array, NULL);
		while (work) {
			/* add all rows to row_array */
			g_ptr_array_add( row_array, work);
			if (GTK_CTREE_ROW (work)->parent && gtk_ctree_is_viewable( ctree, work))
				g_ptr_array_add( viewable_array, GTK_CTREE_ROW (work)->parent);
			next = GTK_CTREE_ROW (work)->sibling;
			gtk_sctree_unlink( ctree, work, FALSE);
			work = next;
		}

		heap_sort( clist, row_array, (row_array->len)-1);

		if (node)
			list_start = GTK_CTREE_ROW (node)->children;
		else
			list_start = GTK_CTREE_NODE (clist->row_list);

		if (clist->sort_type == GTK_SORT_ASCENDING) {
			for (i=(row_array->len)-1; i>=1; i--) {
				work = g_ptr_array_index( row_array, i);
				gtk_sctree_link( ctree, work, node, list_start, FALSE);
				list_start = work;
				/* insert work at the beginning of the list */
			}
		} else {
			for (i=1; i<row_array->len; i++) {
				work = g_ptr_array_index( row_array, i);
				gtk_sctree_link( ctree, work, node, list_start, FALSE);
				list_start = work;
				/* insert work at the beginning of the list */
			}
		}

		for (i=0; i<viewable_array->len; i++) {
			gtk_ctree_expand( ctree, g_ptr_array_index( viewable_array, i));
		}
		
	}
	g_ptr_array_free( row_array, TRUE);
	g_ptr_array_free( viewable_array, TRUE);
}

void
gtk_sctree_sort_recursive (GtkCTree     *ctree, 
			  GtkCTreeNode *node)
{
	GtkCList *clist;
	GtkCTreeNode *focus_node = NULL;

	g_return_if_fail (ctree != NULL);
	g_return_if_fail (GTK_IS_CTREE (ctree));

	clist = GTK_CLIST (ctree);

	gtk_clist_freeze (clist);

	if (clist->selection_mode == GTK_SELECTION_EXTENDED) {
		GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
		g_list_free (clist->undo_selection);
		g_list_free (clist->undo_unselection);
		clist->undo_selection = NULL;
		clist->undo_unselection = NULL;
	}

	if (!node || (node && gtk_ctree_is_viewable (ctree, node)))
		focus_node = GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
	GTK_SCTREE(ctree)->sorting = TRUE;

	gtk_ctree_post_recursive (ctree, node, GTK_CTREE_FUNC (stree_sort), NULL);

	if (!node)
		stree_sort (ctree, NULL, NULL);

	GTK_SCTREE(ctree)->sorting = FALSE;

	if (focus_node) {
		clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
		clist->undo_anchor = clist->focus_row;
	}

	gtk_clist_thaw (clist);
}

void
gtk_sctree_sort_node (GtkCTree     *ctree, 
		     GtkCTreeNode *node)
{
	GtkCList *clist;
	GtkCTreeNode *focus_node = NULL;

	g_return_if_fail (ctree != NULL);
	g_return_if_fail (GTK_IS_CTREE (ctree));

	clist = GTK_CLIST (ctree);

	gtk_clist_freeze (clist);

	if (clist->selection_mode == GTK_SELECTION_EXTENDED) {
		GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);

		g_list_free (clist->undo_selection);
		g_list_free (clist->undo_unselection);
		clist->undo_selection = NULL;
		clist->undo_unselection = NULL;
	}

	if (!node || (node && gtk_ctree_is_viewable (ctree, node)))
		focus_node = GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));

	GTK_SCTREE(ctree)->sorting = TRUE;

	stree_sort (ctree, node, NULL);

	GTK_SCTREE(ctree)->sorting = FALSE;

	if (focus_node) {
		clist->focus_row = g_list_position (clist->row_list,(GList *)focus_node);
		clist->undo_anchor = clist->focus_row;
	}

	gtk_clist_thaw (clist);
}

/************************************************************************/

static void
gtk_sctree_unlink (GtkCTree     *ctree, 
		  GtkCTreeNode *node,
                  gboolean      update_focus_row)
{
	GtkCList *clist;
	gint rows;
	gint level;
	gint visible;
	GtkCTreeNode *work;
	GtkCTreeNode *parent;
	GList *list;

	g_return_if_fail (ctree != NULL);
	g_return_if_fail (GTK_IS_CTREE (ctree));
	g_return_if_fail (node != NULL);

	clist = GTK_CLIST (ctree);
  
	if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED) {
		GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);

		g_list_free (clist->undo_selection);
		g_list_free (clist->undo_unselection);
		clist->undo_selection = NULL;
		clist->undo_unselection = NULL;
	}

	visible = gtk_ctree_is_viewable (ctree, node);

	/* clist->row_list_end unlinked ? */
	if (visible && (GTK_CTREE_NODE_NEXT (node) == NULL ||
	   (GTK_CTREE_ROW (node)->children && gtk_ctree_is_ancestor (ctree, node,
	    GTK_CTREE_NODE (clist->row_list_end)))))
		clist->row_list_end = (GList *) (GTK_CTREE_NODE_PREV (node));

	/* update list */
	rows = 0;
	level = GTK_CTREE_ROW (node)->level;
	work = GTK_CTREE_NODE_NEXT (node);
	while (work && GTK_CTREE_ROW (work)->level > level) {
		work = GTK_CTREE_NODE_NEXT (work);
		rows++;
	}

	if (visible) {
		clist->rows -= (rows + 1);

		if (update_focus_row) {
			gint pos;
			pos = g_list_position (clist->row_list, (GList *)node);
			if (pos + rows < clist->focus_row)
				clist->focus_row -= (rows + 1);
			else if (pos <= clist->focus_row) {
				if (!GTK_CTREE_ROW (node)->sibling)
					clist->focus_row = MAX (pos - 1, 0);
				else
					clist->focus_row = pos;
	      
				clist->focus_row = MIN (clist->focus_row, clist->rows - 1);
			}
			clist->undo_anchor = clist->focus_row;
		}
	}

	if (work) {
		list = (GList *)GTK_CTREE_NODE_PREV (work);
		list->next = NULL;
		list = (GList *)work;
		list->prev = (GList *)GTK_CTREE_NODE_PREV (node);
	}

	if (GTK_CTREE_NODE_PREV (node) &&
	    GTK_CTREE_NODE_NEXT (GTK_CTREE_NODE_PREV (node)) == node) {
		list = (GList *)GTK_CTREE_NODE_PREV (node);
		list->next = (GList *)work;
	}

	/* update tree */
	parent = GTK_CTREE_ROW (node)->parent;
	if (parent) {
		if (GTK_CTREE_ROW (parent)->children == node) {
			GTK_CTREE_ROW (parent)->children = GTK_CTREE_ROW (node)->sibling;
		}
		else {
			GtkCTreeNode *sibling;

			sibling = GTK_CTREE_ROW (parent)->children;
			while (GTK_CTREE_ROW (sibling)->sibling != node)
				sibling = GTK_CTREE_ROW (sibling)->sibling;
			GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
		}
	}
	else {
		if (clist->row_list == (GList *)node)
			clist->row_list = (GList *) (GTK_CTREE_ROW (node)->sibling);
		else {
			GtkCTreeNode *sibling;

			sibling = GTK_CTREE_NODE (clist->row_list);
			while (GTK_CTREE_ROW (sibling)->sibling != node)
				sibling = GTK_CTREE_ROW (sibling)->sibling;
			GTK_CTREE_ROW (sibling)->sibling = GTK_CTREE_ROW (node)->sibling;
		}
	}
}

static void
gtk_sctree_link (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *parent,
		GtkCTreeNode *sibling,
		gboolean      update_focus_row)
{
	GtkCList *clist;
	GList *list_end;
	GList *list;
	GList *work;
	gboolean visible = FALSE;
	gint rows = 0;
  
	if (sibling)
		g_return_if_fail (GTK_CTREE_ROW (sibling)->parent == parent);
	g_return_if_fail (node != NULL);
	g_return_if_fail (node != sibling);
	g_return_if_fail (node != parent);

	clist = GTK_CLIST (ctree);

	if (update_focus_row && clist->selection_mode == GTK_SELECTION_EXTENDED) {
		GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);

		g_list_free (clist->undo_selection);
		g_list_free (clist->undo_unselection);
		clist->undo_selection = NULL;
		clist->undo_unselection = NULL;
	}

	for (rows = 1, list_end = (GList *)node; list_end->next;
	     list_end = list_end->next)
		rows++;

	GTK_CTREE_ROW (node)->parent = parent;
	GTK_CTREE_ROW (node)->sibling = sibling;

	if (!parent || (parent && (gtk_ctree_is_viewable (ctree, parent) &&
	    GTK_CTREE_ROW (parent)->expanded))) {
		visible = TRUE;
		clist->rows += rows;
	}

	if (parent)
		work = (GList *)(GTK_CTREE_ROW (parent)->children);
	else
		work = clist->row_list;

	if (sibling) {
		if (work != (GList *)sibling) {
			while (GTK_CTREE_ROW (work)->sibling != sibling)
				work = (GList *)(GTK_CTREE_ROW (work)->sibling);
			GTK_CTREE_ROW (work)->sibling = node;
		}

		if (sibling == GTK_CTREE_NODE (clist->row_list))
		clist->row_list = (GList *) node;
		if (GTK_CTREE_NODE_PREV (sibling) &&
		    GTK_CTREE_NODE_NEXT (GTK_CTREE_NODE_PREV (sibling)) == sibling) {
			list = (GList *)GTK_CTREE_NODE_PREV (sibling);
			list->next = (GList *)node;
		}

		list = (GList *)node;
		list->prev = (GList *)GTK_CTREE_NODE_PREV (sibling);
		list_end->next = (GList *)sibling;
		list = (GList *)sibling;
		list->prev = list_end;
		if (parent && GTK_CTREE_ROW (parent)->children == sibling)
			GTK_CTREE_ROW (parent)->children = node;
	}
	else {
		if (work) {
			/* find sibling */
			while (GTK_CTREE_ROW (work)->sibling)
			work = (GList *)(GTK_CTREE_ROW (work)->sibling);
			GTK_CTREE_ROW (work)->sibling = node;

			/* find last visible child of sibling */
			work = (GList *) gtk_sctree_last_visible (ctree,
			       GTK_CTREE_NODE (work));

			list_end->next = work->next;
			if (work->next)
				list = work->next->prev = list_end;
			work->next = (GList *)node;
			list = (GList *)node;
			list->prev = work;
		}
		else {
			if (parent) {
				GTK_CTREE_ROW (parent)->children = node;
				list = (GList *)node;
				list->prev = (GList *)parent;
				if (GTK_CTREE_ROW (parent)->expanded) {
					list_end->next = (GList *)GTK_CTREE_NODE_NEXT (parent);
					if (GTK_CTREE_NODE_NEXT(parent)) {
						list = (GList *)GTK_CTREE_NODE_NEXT (parent);
						list->prev = list_end;
					}
					list = (GList *)parent;
					list->next = (GList *)node;
				}
				else
					list_end->next = NULL;
			}
			else {
				clist->row_list = (GList *)node;
				list = (GList *)node;
				list->prev = NULL;
				list_end->next = NULL;
			}
		}
	}

	gtk_ctree_pre_recursive (ctree, node, stree_update_level, NULL); 

	if (clist->row_list_end == NULL ||
	    clist->row_list_end->next == (GList *)node)
		clist->row_list_end = list_end;

	if (visible && update_focus_row) {
		gint pos;
		pos = g_list_position (clist->row_list, (GList *)node);
  
		if (pos <= clist->focus_row) {
			clist->focus_row += rows;
			clist->undo_anchor = clist->focus_row;
		}
	}
}

static void
stree_update_level (GtkCTree     *ctree, 
		   GtkCTreeNode *node, 
		   gpointer      data)
{
	if (!node)
		return;

	if (GTK_CTREE_ROW (node)->parent)
		GTK_CTREE_ROW (node)->level = 
		GTK_CTREE_ROW (GTK_CTREE_ROW (node)->parent)->level + 1;
	else
		GTK_CTREE_ROW (node)->level = 1;
}

static GtkCTreeNode *
gtk_sctree_last_visible (GtkCTree     *ctree,
			GtkCTreeNode *node)
{
	GtkCTreeNode *work;
  
	if (!node)
		return NULL;

	work = GTK_CTREE_ROW (node)->children;

	if (!work || !GTK_CTREE_ROW (node)->expanded)
		return node;

	while (GTK_CTREE_ROW (work)->sibling)
		work = GTK_CTREE_ROW (work)->sibling;

	return gtk_sctree_last_visible (ctree, work);
}

/* this wrapper simply replaces NULL pixmaps 
 * with a transparent, 1x1 pixmap. This works
 * around a memory problem deep inside gtk, 
 * revealed by valgrind. 
 */
/*GtkCTreeNode* gtk_sctree_insert_node        (GtkCTree *ctree,
                                             GtkCTreeNode *parent,
                                             GtkCTreeNode *sibling,
                                             gchar *text[],
                                             guint8 spacing,
                                             GdkPixmap *pixmap_closed,
                                             GdkBitmap *mask_closed,
                                             GdkPixmap *pixmap_opened,
                                             GdkBitmap *mask_opened,
                                             gboolean is_leaf,
                                             gboolean expanded)
{
	if (!emptyxpm) {
		stock_pixmap_gdk(GTK_WIDGET(ctree), STOCK_PIXMAP_EMPTY,
			 &emptyxpm, &emptyxpmmask);
	}
	if (!pixmap_closed) {
		pixmap_closed = emptyxpm;
		mask_closed = emptyxpmmask;
	}
	if (!pixmap_opened) {
		pixmap_opened = emptyxpm;
		mask_opened = emptyxpmmask;
	}
	return gtk_ctree_insert_node(ctree, parent, sibling, text,spacing,
		pixmap_closed, mask_closed, pixmap_opened, mask_opened,
		is_leaf, expanded);
}*/

static void 
sset_node_info (GtkCTree     *ctree,
	       GtkCTreeNode *node,
	       const gchar  *text,
	       guint8        spacing,
	       GdkPixmap    *pixmap_closed,
	       GdkBitmap    *mask_closed,
	       GdkPixmap    *pixmap_opened,
	       GdkBitmap    *mask_opened,
	       gboolean      is_leaf,
	       gboolean      expanded)
{
  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      gdk_pixmap_unref (GTK_CTREE_ROW (node)->pixmap_opened);
      if (GTK_CTREE_ROW (node)->mask_opened) 
	gdk_bitmap_unref (GTK_CTREE_ROW (node)->mask_opened);
    }
  if (GTK_CTREE_ROW (node)->pixmap_closed)
    {
      gdk_pixmap_unref (GTK_CTREE_ROW (node)->pixmap_closed);
      if (GTK_CTREE_ROW (node)->mask_closed) 
	gdk_bitmap_unref (GTK_CTREE_ROW (node)->mask_closed);
    }

  GTK_CTREE_ROW (node)->pixmap_opened = NULL;
  GTK_CTREE_ROW (node)->mask_opened   = NULL;
  GTK_CTREE_ROW (node)->pixmap_closed = NULL;
  GTK_CTREE_ROW (node)->mask_closed   = NULL;

  if (pixmap_closed)
    {
      GTK_CTREE_ROW (node)->pixmap_closed = gdk_pixmap_ref (pixmap_closed);
      if (mask_closed) 
	GTK_CTREE_ROW (node)->mask_closed = gdk_bitmap_ref (mask_closed);
    }
  if (pixmap_opened)
    {
      GTK_CTREE_ROW (node)->pixmap_opened = gdk_pixmap_ref (pixmap_opened);
      if (mask_opened) 
	GTK_CTREE_ROW (node)->mask_opened = gdk_bitmap_ref (mask_opened);
    }

  GTK_CTREE_ROW (node)->is_leaf  = is_leaf;
  GTK_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;

  if (GTK_CTREE_ROW (node)->expanded)
    gtk_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_opened, mask_opened);
  else 
    gtk_ctree_node_set_pixtext (ctree, node, ctree->tree_column,
				text, spacing, pixmap_closed, mask_closed);
}

static void
stree_draw_node (GtkCTree     *ctree, 
	        GtkCTreeNode *node)
{
  GtkCList *clist;
  
  clist = GTK_CLIST (ctree);

  if (CLIST_UNFROZEN (clist) && gtk_ctree_is_viewable (ctree, node))
    {
      GtkCTreeNode *work;
      gint num = 0;
      
      work = GTK_CTREE_NODE (clist->row_list);
      while (work && work != node)
	{
	  work = GTK_CTREE_NODE_NEXT (work);
	  num++;
	}
      if (work && gtk_clist_row_is_visible (clist, num) != GTK_VISIBILITY_NONE)
	GTK_CLIST_GET_CLASS (clist)->draw_row
	  (clist, NULL, num, GTK_CLIST_ROW ((GList *) node));
    }
}

/* this wrapper simply replaces NULL pixmaps 
 * with a transparent, 1x1 pixmap. This works
 * around a memory problem deep inside gtk, 
 * revealed by valgrind. 
 */
void        gtk_sctree_set_node_info        (GtkCTree *ctree,
                                             GtkCTreeNode *node,
                                             const gchar *text,
                                             guint8 spacing,
                                             GdkPixmap *pixmap_closed,
                                             GdkBitmap *mask_closed,
                                             GdkPixmap *pixmap_opened,
                                             GdkBitmap *mask_opened,
                                             gboolean is_leaf,
                                             gboolean expanded)
{
  gboolean old_leaf;
  gboolean old_expanded;
  GtkCTreeNode *work;
 
  if (!emptyxpm) {
	  stock_pixmap_gdk(GTK_WIDGET(ctree), STOCK_PIXMAP_EMPTY,
		   &emptyxpm, &emptyxpmmask);
  }
  if (!pixmap_closed) {
	  pixmap_closed = emptyxpm;
	  mask_closed = emptyxpmmask;
  }
  if (!pixmap_opened) {
	  pixmap_opened = emptyxpm;
	  mask_opened = emptyxpmmask;
  }

  if (!GTK_IS_CTREE (ctree) || !node) return;

  old_leaf = GTK_CTREE_ROW (node)->is_leaf;
  old_expanded = GTK_CTREE_ROW (node)->expanded;

  if (is_leaf && (work = GTK_CTREE_ROW (node)->children) != NULL)
    {
      GtkCTreeNode *ptr;
      
      while (work)
	{
	  ptr = work;
	  work = GTK_CTREE_ROW (work)->sibling;
	  gtk_ctree_remove_node (ctree, ptr);
	}
    }

  sset_node_info (ctree, node, text, spacing, pixmap_closed, mask_closed,
		 pixmap_opened, mask_opened, is_leaf, expanded);

  if (!is_leaf && !old_leaf)
    {
      GTK_CTREE_ROW (node)->expanded = old_expanded;
      if (expanded && !old_expanded)
	gtk_ctree_expand (ctree, node);
      else if (!expanded && old_expanded)
	gtk_ctree_collapse (ctree, node);
    }

  GTK_CTREE_ROW (node)->expanded = (is_leaf) ? FALSE : expanded;
  
  stree_draw_node (ctree, node);
}

static GtkCTreeRow *
srow_new (GtkCTree *ctree)
{
  GtkCList *clist;
  GtkCTreeRow *ctree_row;
  int i;

  clist = GTK_CLIST (ctree);
#if GTK_CHECK_VERSION(2,9,0)
  ctree_row = g_slice_new (GtkCTreeRow);
  ctree_row->row.cell = g_slice_alloc (sizeof (GtkCell) * clist->columns);
#else
  ctree_row = g_chunk_new (GtkCTreeRow, (GMemChunk *)clist->row_mem_chunk);
  ctree_row->row.cell = g_chunk_new (GtkCell, (GMemChunk *)clist->cell_mem_chunk);
#endif
  for (i = 0; i < clist->columns; i++)
    {
      ctree_row->row.cell[i].type = GTK_CELL_EMPTY;
      ctree_row->row.cell[i].vertical = 0;
      ctree_row->row.cell[i].horizontal = 0;
      ctree_row->row.cell[i].style = NULL;
    }

  GTK_CELL_PIXTEXT (ctree_row->row.cell[ctree->tree_column])->text = NULL;

  ctree_row->row.fg_set     = FALSE;
  ctree_row->row.bg_set     = FALSE;
  ctree_row->row.style      = NULL;
  ctree_row->row.selectable = TRUE;
  ctree_row->row.state      = GTK_STATE_NORMAL;
  ctree_row->row.data       = NULL;
  ctree_row->row.destroy    = NULL;

  ctree_row->level         = 0;
  ctree_row->expanded      = FALSE;
  ctree_row->parent        = NULL;
  ctree_row->sibling       = NULL;
  ctree_row->children      = NULL;
  ctree_row->pixmap_closed = NULL;
  ctree_row->mask_closed   = NULL;
  ctree_row->pixmap_opened = NULL;
  ctree_row->mask_opened   = NULL;
  
  return ctree_row;
}

static void
srow_delete (GtkCTree    *ctree,
	    GtkCTreeRow *ctree_row)
{
  GtkCList *clist;
  gint i;

  clist = GTK_CLIST (ctree);

  for (i = 0; i < clist->columns; i++)
    {
      GTK_CLIST_GET_CLASS (clist)->set_cell_contents
	(clist, &(ctree_row->row), i, GTK_CELL_EMPTY, NULL, 0, NULL, NULL);
      if (ctree_row->row.cell[i].style)
	{
	  if (GTK_WIDGET_REALIZED (ctree))
	    gtk_style_detach (ctree_row->row.cell[i].style);
	  g_object_unref (ctree_row->row.cell[i].style);
	}
    }

  if (ctree_row->row.style)
    {
      if (GTK_WIDGET_REALIZED (ctree))
	gtk_style_detach (ctree_row->row.style);
      g_object_unref (ctree_row->row.style);
    }

  if (ctree_row->pixmap_closed)
    {
      gdk_pixmap_unref (ctree_row->pixmap_closed);
      if (ctree_row->mask_closed)
	gdk_bitmap_unref (ctree_row->mask_closed);
    }

  if (ctree_row->pixmap_opened)
    {
      gdk_pixmap_unref (ctree_row->pixmap_opened);
      if (ctree_row->mask_opened)
	gdk_bitmap_unref (ctree_row->mask_opened);
    }

  if (ctree_row->row.destroy)
    {
      GtkDestroyNotify dnotify = ctree_row->row.destroy;
      gpointer ddata = ctree_row->row.data;

      ctree_row->row.destroy = NULL;
      ctree_row->row.data = NULL;

      dnotify (ddata);
    }

#if GTK_CHECK_VERSION(2,9,0)  
  g_slice_free1 (sizeof (GtkCell) * clist->columns, ctree_row->row.cell);
  g_slice_free (GtkCTreeRow, ctree_row);
#else
  g_mem_chunk_free ((GMemChunk *)clist->cell_mem_chunk, ctree_row->row.cell);
  g_mem_chunk_free ((GMemChunk *)clist->row_mem_chunk, ctree_row);
#endif
}

static void
stree_delete_row (GtkCTree     *ctree, 
		 GtkCTreeNode *node, 
		 gpointer      data)
{
  srow_delete (ctree, GTK_CTREE_ROW (node));
  g_list_free_1 ((GList *)node);
}

static void
gtk_sctree_column_auto_resize (GtkCList    *clist,
		    GtkCListRow *clist_row,
		    gint         column,
		    gint         old_width)
{
  /* resize column if needed for auto_resize */
  GtkRequisition requisition;

  if (!clist->column[column].auto_resize ||
      GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    return;

  if (clist_row)
    GTK_CLIST_GET_CLASS (clist)->cell_size_request (clist, clist_row,
						   column, &requisition);
  else
    requisition.width = 0;

  if (requisition.width > clist->column[column].width)
    gtk_clist_set_column_width (clist, column, requisition.width);
  else if (requisition.width < old_width &&
	   old_width == clist->column[column].width)
    {
      GList *list;
      gint new_width;

      /* run a "gtk_clist_optimal_column_width" but break, if
       * the column doesn't shrink */
      if (GTK_CLIST_SHOW_TITLES (clist) && clist->column[column].button)
	new_width = (clist->column[column].button->requisition.width -
		     (CELL_SPACING + (2 * COLUMN_INSET)));
      else
	new_width = 0;

      for (list = clist->row_list; list; list = list->next)
	{
	  GTK_CLIST_GET_CLASS (clist)->cell_size_request
	    (clist, GTK_CLIST_ROW (list), column, &requisition);
	  new_width = MAX (new_width, requisition.width);
	  if (new_width == clist->column[column].width)
	    break;
	}
      if (new_width < clist->column[column].width)
	gtk_clist_set_column_width (clist, column, new_width);
    }
}


static void 
gtk_sctree_real_tree_expand (GtkCTree     *ctree,
		  GtkCTreeNode *node)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  GtkRequisition requisition;
  gboolean visible;
  gint level;

  g_return_if_fail (GTK_IS_CTREE (ctree));

  if (!node || GTK_CTREE_ROW (node)->expanded || GTK_CTREE_ROW (node)->is_leaf)
    return;

  clist = GTK_CLIST (ctree);
  
  GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);

  GTK_CTREE_ROW (node)->expanded = TRUE;
  level = GTK_CTREE_ROW (node)->level;

  visible = gtk_ctree_is_viewable (ctree, node);
  /* get cell width if tree_column is auto resized */
  if (visible && clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
    GTK_CLIST_GET_CLASS (clist)->cell_size_request
      (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column, &requisition);

  /* unref/unset closed pixmap */
  if (GTK_CELL_PIXTEXT 
      (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap)
    {
      gdk_pixmap_unref
	(GTK_CELL_PIXTEXT
	 (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap);
      
      GTK_CELL_PIXTEXT
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = NULL;
      
      if (GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask)
	{
	  gdk_pixmap_unref
	    (GTK_CELL_PIXTEXT 
	     (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask);
	  GTK_CELL_PIXTEXT 
	    (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = NULL;
	}
    }

  /* set/ref opened pixmap */
  if (GTK_CTREE_ROW (node)->pixmap_opened)
    {
      GTK_CELL_PIXTEXT 
	(GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->pixmap = 
	gdk_pixmap_ref (GTK_CTREE_ROW (node)->pixmap_opened);

      if (GTK_CTREE_ROW (node)->mask_opened) 
	GTK_CELL_PIXTEXT 
	  (GTK_CTREE_ROW (node)->row.cell[ctree->tree_column])->mask = 
	  gdk_pixmap_ref (GTK_CTREE_ROW (node)->mask_opened);
    }


  work = GTK_CTREE_ROW (node)->children;
  if (work)
    {
      GList *list = (GList *)work;
      gint *cell_width = NULL;
      gint tmp = 0;
      gint row;
      gint i;
      
      if (visible && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist))
	{
	  cell_width = g_new0 (gint, clist->columns);
	  if (clist->column[ctree->tree_column].auto_resize)
	      cell_width[ctree->tree_column] = requisition.width;

	  while (work)
	    {
	      /* search maximum cell widths of auto_resize columns */
	      for (i = 0; i < clist->columns; i++)
		if (clist->column[i].auto_resize)
		  {
		    GTK_CLIST_GET_CLASS (clist)->cell_size_request
		      (clist, &GTK_CTREE_ROW (work)->row, i, &requisition);
		    cell_width[i] = MAX (requisition.width, cell_width[i]);
		  }

	      list = (GList *)work;
	      work = GTK_CTREE_NODE_NEXT (work);
	      tmp++;
	    }
	}
      else
	while (work)
	  {
	    list = (GList *)work;
	    work = GTK_CTREE_NODE_NEXT (work);
	    tmp++;
	  }

      list->next = (GList *)GTK_CTREE_NODE_NEXT (node);

      if (GTK_CTREE_NODE_NEXT (node))
	{
	  GList *tmp_list;

	  tmp_list = (GList *)GTK_CTREE_NODE_NEXT (node);
	  tmp_list->prev = list;
	}
      else
	clist->row_list_end = list;

      list = (GList *)node;
      list->next = (GList *)(GTK_CTREE_ROW (node)->children);

      if (visible)
	{
	  /* resize auto_resize columns if needed */
	  for (i = 0; i < clist->columns; i++)
	    if (clist->column[i].auto_resize &&
		cell_width[i] > clist->column[i].width)
	      gtk_clist_set_column_width (clist, i, cell_width[i]);
	  g_free (cell_width);
	
	  if (!GTK_SCTREE(ctree)->sorting) {
		  /* update focus_row position */
		  row = g_list_position (clist->row_list, (GList *)node);
		  if (row < clist->focus_row)
		    clist->focus_row += tmp;
	  }
	  clist->rows += tmp;
	  CLIST_REFRESH (clist);
	}
    }
  else if (visible && clist->column[ctree->tree_column].auto_resize)
    /* resize tree_column if needed */
    gtk_sctree_column_auto_resize (clist, &GTK_CTREE_ROW (node)->row, ctree->tree_column,
			requisition.width);
}

GtkCTreeNode * 
gtk_sctree_insert_node (GtkCTree     *ctree,
		       GtkCTreeNode *parent, 
		       GtkCTreeNode *sibling,
		       gchar        *text[],
		       guint8        spacing,
		       GdkPixmap    *pixmap_closed,
		       GdkBitmap    *mask_closed,
		       GdkPixmap    *pixmap_opened,
		       GdkBitmap    *mask_opened,
		       gboolean      is_leaf,
		       gboolean      expanded)
{
  GtkCList *clist;
  GtkCTreeRow *new_row;
  GtkCTreeNode *node;
  GList *list;
  gint i;

  if (!emptyxpm) {
	  stock_pixmap_gdk(GTK_WIDGET(ctree), STOCK_PIXMAP_EMPTY,
		   &emptyxpm, &emptyxpmmask);
  }
  if (!pixmap_closed) {
	  pixmap_closed = emptyxpm;
	  mask_closed = emptyxpmmask;
  }
  if (!pixmap_opened) {
	  pixmap_opened = emptyxpm;
	  mask_opened = emptyxpmmask;
  }
  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  if (sibling)
    g_return_val_if_fail (GTK_CTREE_ROW (sibling)->parent == parent, NULL);

  if (parent && GTK_CTREE_ROW (parent)->is_leaf)
    return NULL;

  clist = GTK_CLIST (ctree);

  /* create the row */
  new_row = srow_new (ctree);
  list = g_list_alloc ();
  list->data = new_row;
  node = GTK_CTREE_NODE (list);

  if (text)
    for (i = 0; i < clist->columns; i++)
      if (text[i] && i != ctree->tree_column)
	GTK_CLIST_GET_CLASS (clist)->set_cell_contents
	  (clist, &(new_row->row), i, GTK_CELL_TEXT, text[i], 0, NULL, NULL);

  sset_node_info (ctree, node, text ?
		 text[ctree->tree_column] : NULL, spacing, pixmap_closed,
		 mask_closed, pixmap_opened, mask_opened, is_leaf, expanded);

  /* sorted insertion */
  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = GTK_CTREE_ROW (parent)->children;
      else
	sibling = GTK_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, GTK_CTREE_ROW (node), GTK_CTREE_ROW (sibling)) > 0)
	sibling = GTK_CTREE_ROW (sibling)->sibling;
    }

  gtk_sctree_link (ctree, node, parent, sibling, FALSE);

  if (text && !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      gtk_ctree_is_viewable (ctree, node))
    {
      for (i = 0; i < clist->columns; i++)
	if (clist->column[i].auto_resize)
	  gtk_sctree_column_auto_resize (clist, &(new_row->row), i, 0);
    }

  if (clist->rows == 1)
    {
      clist->focus_row = 0;
      if (clist->selection_mode == GTK_SELECTION_BROWSE)
	gtk_sctree_select (GTK_SCTREE(ctree), node);
    }


  CLIST_REFRESH (clist);

  return node;
}

GtkCTreeNode *
gtk_sctree_insert_gnode (GtkCTree          *ctree,
			GtkCTreeNode      *parent,
			GtkCTreeNode      *sibling,
			GNode             *gnode,
			GtkCTreeGNodeFunc  func,
			gpointer           data)
{
  GtkCList *clist;
  GtkCTreeNode *cnode = NULL;
  GtkCTreeNode *child = NULL;
  GtkCTreeNode *new_child;
  GList *list;
  GNode *work;
  guint depth = 1;

  g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);
  g_return_val_if_fail (gnode != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  if (sibling)
    g_return_val_if_fail (GTK_CTREE_ROW (sibling)->parent == parent, NULL);
  
  clist = GTK_CLIST (ctree);

  if (parent)
    depth = GTK_CTREE_ROW (parent)->level + 1;

  list = g_list_alloc ();
  list->data = srow_new (ctree);
  cnode = GTK_CTREE_NODE (list);

  gtk_clist_freeze (clist);

  sset_node_info (ctree, cnode, "", 0, NULL, NULL, NULL, NULL, TRUE, FALSE);

  if (!func (ctree, depth, gnode, cnode, data))
    {
      stree_delete_row (ctree, cnode, NULL);
      gtk_clist_thaw (clist);
      return NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (parent)
	sibling = GTK_CTREE_ROW (parent)->children;
      else
	sibling = GTK_CTREE_NODE (clist->row_list);

      while (sibling && clist->compare
	     (clist, GTK_CTREE_ROW (cnode), GTK_CTREE_ROW (sibling)) > 0)
	sibling = GTK_CTREE_ROW (sibling)->sibling;
    }

  gtk_sctree_link (ctree, cnode, parent, sibling, FALSE);

  for (work = g_node_last_child (gnode); work; work = work->prev)
    {
      new_child = gtk_sctree_insert_gnode (ctree, cnode, child,
					  work, func, data);
      if (new_child)
	child = new_child;
    }	
  
  gtk_clist_thaw (clist);

  return cnode;
}

static void
sreal_tree_move (GtkCTree     *ctree,
		GtkCTreeNode *node,
		GtkCTreeNode *new_parent, 
		GtkCTreeNode *new_sibling)
{
  GtkCList *clist;
  GtkCTreeNode *work;
  gboolean visible = FALSE;

  g_return_if_fail (ctree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (!new_sibling || 
		    GTK_CTREE_ROW (new_sibling)->parent == new_parent);

  if (new_parent && GTK_CTREE_ROW (new_parent)->is_leaf)
    return;

  /* new_parent != child of child */
  for (work = new_parent; work; work = GTK_CTREE_ROW (work)->parent)
    if (work == node)
      return;

  clist = GTK_CLIST (ctree);

  visible = gtk_ctree_is_viewable (ctree, node);

  if (clist->selection_mode == GTK_SELECTION_MULTIPLE)
    {
      GTK_CLIST_GET_CLASS (clist)->resync_selection (clist, NULL);
      
      g_list_free (clist->undo_selection);
      g_list_free (clist->undo_unselection);
      clist->undo_selection = NULL;
      clist->undo_unselection = NULL;
    }

  if (GTK_CLIST_AUTO_SORT (clist))
    {
      if (new_parent == GTK_CTREE_ROW (node)->parent)
	return;
      
      if (new_parent)
	new_sibling = GTK_CTREE_ROW (new_parent)->children;
      else
	new_sibling = GTK_CTREE_NODE (clist->row_list);

      while (new_sibling && clist->compare
	     (clist, GTK_CTREE_ROW (node), GTK_CTREE_ROW (new_sibling)) > 0)
	new_sibling = GTK_CTREE_ROW (new_sibling)->sibling;
    }

  if (new_parent == GTK_CTREE_ROW (node)->parent && 
      new_sibling == GTK_CTREE_ROW (node)->sibling)
    return;

  gtk_clist_freeze (clist);

  work = NULL;

  if (!GTK_SCTREE(ctree)->sorting && gtk_ctree_is_viewable (ctree, node))
    work = GTK_CTREE_NODE (g_list_nth (clist->row_list, clist->focus_row));
      
  gtk_sctree_unlink (ctree, node, FALSE);
  gtk_sctree_link (ctree, node, new_parent, new_sibling, FALSE);
  
  if (!GTK_SCTREE(ctree)->sorting && work)
    {
      while (work &&  !gtk_ctree_is_viewable (ctree, work))
	work = GTK_CTREE_ROW (work)->parent;
      clist->focus_row = g_list_position (clist->row_list, (GList *)work);
      clist->undo_anchor = clist->focus_row;
    }

  if (clist->column[ctree->tree_column].auto_resize &&
      !GTK_CLIST_AUTO_RESIZE_BLOCKED (clist) &&
      (visible || gtk_ctree_is_viewable (ctree, node)))
    gtk_clist_set_column_width
      (clist, ctree->tree_column,
       gtk_clist_optimal_column_width (clist, ctree->tree_column));

  gtk_clist_thaw (clist);
}

