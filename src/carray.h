
/*
 * libEtPan! -- a mail stuff library
 *
 * carray - Implements simple dynamic pointer arrays
 *
 * Copyright (c) 1999-2000, Gaël Roualland <gael.roualland@iname.com>
 * interface changes - 2002 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CARRAY_H
#define CARRAY_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef GPtrArray carray;

/* Creates a new array of pointers, with initsize preallocated cells */
static inline carray *carray_new(guint initsize)
{
	carray *res = g_ptr_array_new();
	gint n = (gint) initsize;
	g_assert(n >= 0);
	g_ptr_array_set_size(res, n);
	return res;
}

static inline gint carray_add(carray *array, void *data, guint *index)
{
	g_ptr_array_add(array, data);
	*index = array->len - 1;
	return 0;
}

static inline gint carray_set_size(carray *array, guint new_size)
{
	gint n = (gint) new_size;
	g_assert(n >= 0);
	g_ptr_array_set_size(array, n);
	return 0;
}

static inline gint carray_delete(carray *array, guint indx)
{
	gint n = (gint) indx;
	if (n < 0) return -1;
	if (n >= array->len) return -1;
	g_ptr_array_remove_index_fast(array, n);
	return 0;
}

static inline gint carray_delete_slow(carray *array, guint indx)
{
	gint n = (gint) indx;
	if (n < 0) return -1;
	return g_ptr_array_remove_index(array, n) ? 0 : -1;
}

static inline gint carray_delete_fast(carray *array, guint indx)
{
	gint n = (gint) indx;
	if (n < 0) return -1;
	g_ptr_array_index(array, n) = NULL;
	return 0;
}

static inline gint carray_free(carray *array)
{
	g_ptr_array_free(array, TRUE);
}

static inline gpointer carray_get(carray *array, guint index)
{
	gint n = (gint) index;
	if (n < 0) return NULL;
	return g_ptr_array_index(array, n);
}

static inline carray_set(carray *array, guint index, gpointer value)
{
	gint n = (gint) index;
	g_assert(n >= 0);
	g_ptr_array_index(array, n) = value;
}

static guint carray_count(carray *array)
{
	/* NOTE: We're hosed when len < 0, but that won't occur */
	return (guint) array->len;
}

#ifdef __cplusplus
}
#endif

#endif
