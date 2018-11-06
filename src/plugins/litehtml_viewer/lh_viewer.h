#include <mimeview.h>

#include "lh_widget_wrapped.h"

MimeViewer *lh_viewer_create();

typedef struct _LHViewer LHViewer;
struct _LHViewer {
	MimeViewer mimeviewer;
	lh_widget_wrapped *widget;
	GtkWidget *vbox;
};
