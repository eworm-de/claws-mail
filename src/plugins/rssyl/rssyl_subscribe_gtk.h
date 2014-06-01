#ifndef __RSSYL_SUBSCRIBE_GTK_H
#define __RSSYL_SUBSCRIBE_GTK_H

#include "libfeed/feed.h"

struct _RSubCtx {
	Feed *feed;
	GtkWidget *title;
};

typedef struct _RSubCtx RSubCtx;

void rssyl_subscribe_dialog(RSubCtx *ctx);

#endif /* __RSSYL_SUBSCRIBE_GTK_H */
