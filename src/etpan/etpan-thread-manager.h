/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2007 DINH Viet Hoa and the Claws Mail team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef ETPAN_THREAD_MANAGER_H

#define ETPAN_THREAD_MANAGER_H

#include "etpan-thread-manager-types.h"

/* ** object creation ** */

struct etpan_thread_manager * etpan_thread_manager_new(void);
void etpan_thread_manager_free(struct etpan_thread_manager * manager);


struct etpan_thread_op * etpan_thread_op_new(void);
void etpan_thread_op_free(struct etpan_thread_op * op);

/* ** thread creation ** */

int etpan_thread_start(struct etpan_thread * thread);

int etpan_thread_is_stopped(struct etpan_thread * thread);

struct etpan_thread *
etpan_thread_manager_get_thread(struct etpan_thread_manager * manager);

void etpan_thread_unbind(struct etpan_thread * thread);

/* ** op schedule ** */

int etpan_thread_op_schedule(struct etpan_thread * thread,
                             struct etpan_thread_op * op);



/* ** manager main loop ** */


void etpan_thread_manager_stop(struct etpan_thread_manager * manager);
void etpan_thread_manager_join(struct etpan_thread_manager * manager);

int etpan_thread_manager_get_fd(struct etpan_thread_manager * manager);
void etpan_thread_manager_loop(struct etpan_thread_manager * manager);

#endif
