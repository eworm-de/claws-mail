/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#ifndef GTKXTEXT_H__
#define GTKXTEXT_H__

#include "config.h"

#ifdef CLAWS

#include "gtkstext.h"

typedef GtkSText GtkXText;

#define GTK_IS_XTEXT(obj)	GTK_IS_STEXT(obj)
#define GTK_XTEXT(obj)		GTK_STEXT(obj)

#define gtk_xtext_get_type	gtk_stext_get_type
#define gtk_xtext_new		gtk_stext_new
#define gtk_xtext_set_editable	gtk_stext_set_editable
#define gtk_xtext_set_word_wrap	gtk_stext_set_word_wrap
#define gtk_xtext_set_line_wrap	gtk_stext_set_line_wrap
#define gtk_xtext_set_adjustments	gtk_stext_set_adjustments
#define gtk_xtext_set_point	gtk_stext_set_point
#define gtk_xtext_get_point	gtk_stext_get_point
#define gtk_xtext_get_length	gtk_stext_get_length
#define gtk_xtext_freeze	gtk_stext_freeze
#define gtk_xtext_thaw		gtk_stext_thaw
#define gtk_xtext_insert	gtk_stext_insert
#define gtk_xtext_backward_delete	gtk_stext_backward_delete
#define gtk_xtext_forward_delete	gtk_stext_forward_delete

#define GTK_XTEXT_INDEX		GTK_STEXT_INDEX

#else

#include <gtk/gtktext.h>

typedef GtkText  GtkXText;

#define GTK_IS_XTEXT(obj)	GTK_IS_TEXT(obj)
#define GTK_XTEXT(obj)		GTK_TEXT(obj)

#define gtk_xtext_get_type	gtk_text_get_type
#define gtk_xtext_new		gtk_text_new
#define gtk_xtext_set_editable	gtk_text_set_editable
#define gtk_xtext_set_word_wrap	gtk_text_set_word_wrap
#define gtk_xtext_set_line_wrap	gtk_text_set_line_wrap
#define gtk_xtext_set_adjustments	gtk_text_set_adjustments
#define gtk_xtext_set_point	gtk_text_set_point
#define gtk_xtext_get_point	gtk_text_get_point
#define gtk_xtext_get_length	gtk_text_get_length
#define gtk_xtext_freeze	gtk_text_freeze
#define gtk_xtext_thaw		gtk_text_thaw
#define gtk_xtext_insert	gtk_text_insert
#define gtk_xtext_backward_delete	gtk_text_backward_delete
#define gtk_xtext_forward_delete	gtk_text_forward_delete

#define GTK_XTEXT_INDEX		GTK_TEXT_INDEX

#endif /* !CLAWS */


#endif /* GTKXTEXT_H__ */
 
