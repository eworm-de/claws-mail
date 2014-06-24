/*
 * This file is part of GtkHotkey.
 * Copyright Mikkel Kamstrup Erlandsen, March, 2008
 *
 *   GtkHotkey is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   GtkHotkey is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with GtkHotkey.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__GTK_HOTKEY_H__) && !defined (GTK_HOTKEY_COMPILATION)
#error "Only <gtkhotkey.h> can be included directly."
#endif

#ifndef __gtk_hotkey_marshal_MARSHAL_H__
#define __gtk_hotkey_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT,UINT (src/marshal.list:2) */
extern void gtk_hotkey_marshal_VOID__OBJECT_UINT (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

G_END_DECLS

#endif /* __gtk_hotkey_marshal_MARSHAL_H__ */

