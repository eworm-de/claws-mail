#ifndef __LH_WIDGET_WRAPPED_H
#define __LH_WIDGET_WRAPPED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lh_widget lh_widget_wrapped;

lh_widget_wrapped *lh_widget_new();
GtkWidget *lh_widget_get_widget(lh_widget_wrapped *w);
void lh_widget_open_html(lh_widget_wrapped *w, const gchar *path);
void lh_widget_clear(lh_widget_wrapped *w);
void lh_widget_destroy(lh_widget_wrapped *w);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __LH_WIDGET_WRAPPED_H */
