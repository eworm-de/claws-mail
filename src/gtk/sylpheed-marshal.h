
#ifndef __sylpheed_marshal_MARSHAL_H__
#define __sylpheed_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* NONE:POINTER (sylpheed-marshal.list:1) */
#define sylpheed_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER
#define sylpheed_marshal_NONE__POINTER	sylpheed_marshal_VOID__POINTER

/* NONE:INT,POINTER (sylpheed-marshal.list:2) */
extern void sylpheed_marshal_VOID__INT_POINTER (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define sylpheed_marshal_NONE__INT_POINTER	sylpheed_marshal_VOID__INT_POINTER

G_END_DECLS

#endif /* __sylpheed_marshal_MARSHAL_H__ */

