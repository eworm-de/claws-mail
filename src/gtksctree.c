/*
 * This program is based on gtkflist.c
 */

#include "gtksctree.h"


enum {
	ROW_POPUP_MENU,
	EMPTY_POPUP_MENU,
	OPEN_ROW,
	START_DRAG,
	LAST_SIGNAL
};


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
GtkType
gtk_sctree_get_type (void)
{
	static GtkType sctree_type = 0;

	if (!sctree_type) {
		GtkTypeInfo sctree_info = {
			"GtkSCTree",
			sizeof (GtkSCTree),
			sizeof (GtkSCTreeClass),
			(GtkClassInitFunc) gtk_sctree_class_init,
			(GtkObjectInitFunc) gtk_sctree_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		sctree_type = gtk_type_unique (gtk_ctree_get_type (), &sctree_info);
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
		gtk_signal_new ("row_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkSCTreeClass, row_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
	sctree_signals[EMPTY_POPUP_MENU] =
		gtk_signal_new ("empty_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkSCTreeClass, empty_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
	sctree_signals[OPEN_ROW] =
		gtk_signal_new ("open_row",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkSCTreeClass, open_row),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	sctree_signals[START_DRAG] =
		gtk_signal_new ("start_drag",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkSCTreeClass, start_drag),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_EVENT);

	gtk_object_class_add_signals (object_class, sctree_signals, LAST_SIGNAL);

	clist_class->clear = gtk_sctree_clear;
        ctree_class->tree_collapse = gtk_sctree_collapse;
	
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

	if (sctree->anchor_row == NULL) {
		prev_row = row;
		sctree->anchor_row = gtk_ctree_node_nth(GTK_CTREE(sctree), row);
	} else
		prev_row = g_list_position(GTK_CLIST(sctree)->row_list,
					   (GList *)sctree->anchor_row);

	if (row < prev_row) {
		min = row;
		max = prev_row;
	} else {
		min = prev_row;
		max = row;
	}
	for (i = min; i <= max; i++)
		gtk_clist_select_row (GTK_CLIST (sctree), i, -1);
}

/* Handles row selection according to the specified modifier state */
static void
select_row (GtkSCTree *sctree, gint row, gint col, guint state)
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

	gtk_clist_freeze (GTK_CLIST (sctree));
	GTK_CLIST(sctree)->focus_row = row;
	GTK_CLIST_CLASS(GTK_OBJECT(sctree)->klass)->refresh(GTK_CLIST(sctree));
	if (!additive)
		gtk_clist_unselect_all (GTK_CLIST (sctree));

	if (!range) {
		GtkCTreeNode *node;

		node = gtk_ctree_node_nth (GTK_CTREE(sctree), row);

		/*No need to manage overlapped list*/
		if (additive) {
			if (row_is_selected(sctree, row))
				gtk_clist_unselect_row (GTK_CLIST (sctree), row, col);
			else
				gtk_signal_emit_by_name
					(GTK_OBJECT (sctree),
					 "tree_select_row", node, col);
		} else {
			gtk_signal_emit_by_name
				(GTK_OBJECT (sctree),
				 "tree_select_row", node, col);
		}
		sctree->anchor_row = node;
	} else
		select_range (sctree, row);
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
				} else
					select_row (sctree, row, col, event->state);
			} else
				gtk_clist_unselect_all (clist);

			retval = TRUE;
		} else if (event->button == 3) {
			/* Emit *_popup_menu signal*/
			if (on_row) {
				if (!row_is_selected(sctree,row))
					select_row (sctree, row, col, 0);
				gtk_signal_emit (GTK_OBJECT (sctree),
						 sctree_signals[ROW_POPUP_MENU],
						 event);
			} else {
				gtk_clist_unselect_all(clist);
				gtk_signal_emit (GTK_OBJECT (sctree),
						 sctree_signals[EMPTY_POPUP_MENU],
						 event);
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
			gtk_signal_emit (GTK_OBJECT (sctree),
					 sctree_signals[OPEN_ROW]);

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
			select_row (sctree, row, col, sctree->dnd_select_pending_state);
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

	if (MAX (abs (sctree->dnd_press_x - event->x),
		 abs (sctree->dnd_press_y - event->y)) <= 3)
		return FALSE;

	/* Handle any pending selections */

	if (sctree->dnd_select_pending) {
		if (!row_is_selected(sctree,sctree->dnd_select_pending_row))
			select_row (sctree,
				    sctree->dnd_select_pending_row,
				    -1,
				    sctree->dnd_select_pending_state);

		sctree->dnd_select_pending = FALSE;
		sctree->dnd_select_pending_state = 0;
	}

	gtk_signal_emit (GTK_OBJECT (sctree),
			 sctree_signals[START_DRAG],
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

GtkWidget *gtk_sctree_new_with_titles (gint columns, 
				       gint tree_column, 
				       gchar *titles[])
{
	GtkSCTree* sctree;

	sctree = gtk_type_new (gtk_sctree_get_type ());
	gtk_ctree_construct (GTK_CTREE (sctree), columns, tree_column, titles);
	gtk_clist_set_selection_mode(GTK_CLIST(sctree), GTK_SELECTION_EXTENDED);

	return GTK_WIDGET (sctree);
}

void  gtk_sctree_select (GtkSCTree *sctree,
			 GtkCTreeNode *node)
{
	select_row(sctree, 
		   g_list_position(GTK_CLIST(sctree)->row_list, (GList *)node),
		   -1, 0);
}

void  gtk_sctree_unselect_all (GtkSCTree *sctree)
{
	gtk_clist_unselect_all(GTK_CLIST(sctree));
	sctree->anchor_row = NULL;
}

