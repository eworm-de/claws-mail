#ifndef ETPAN_THREAD_MANAGER_H

#define ETPAN_THREAD_MANAGER_H

#include "etpan-thread-manager-types.h"

/* ** object creation ** */

struct etpan_thread_manager * etpan_thread_manager_new(void);
void etpan_thread_manager_free(struct etpan_thread_manager * manager);

struct etpan_thread * etpan_thread_new(void);
void etpan_thread_free(struct etpan_thread * thread);

struct etpan_thread_op * etpan_thread_op_new(void);
void etpan_thread_op_free(struct etpan_thread_op * op);

/* ** thread creation ** */

int etpan_thread_start(struct etpan_thread * thread);

void etpan_thread_stop(struct etpan_thread * thread);
int etpan_thread_is_stopped(struct etpan_thread * thread);
void etpan_thread_join(struct etpan_thread * thread);

struct etpan_thread *
etpan_thread_manager_get_thread(struct etpan_thread_manager * manager);

unsigned int etpan_thread_get_load(struct etpan_thread * thread);
void etpan_thread_bind(struct etpan_thread * thread);
void etpan_thread_unbind(struct etpan_thread * thread);
int etpan_thread_is_bound(struct etpan_thread * thread);

/* ** op schedule ** */

int etpan_thread_op_schedule(struct etpan_thread * thread,
                             struct etpan_thread_op * op);

int etpan_thread_op_cancelled(struct etpan_thread_op * op);
void etpan_thread_op_cancel(struct etpan_thread_op * op);
void etpan_thread_op_lock(struct etpan_thread_op * op);
void etpan_thread_op_unlock(struct etpan_thread_op * op);

int etpan_thread_manager_op_schedule(struct etpan_thread_manager * manager,
    struct etpan_thread_op * op);

/* ** manager main loop ** */

void etpan_thread_manager_start(struct etpan_thread_manager * manager);

void etpan_thread_manager_stop(struct etpan_thread_manager * manager);
int etpan_thread_manager_is_stopped(struct etpan_thread_manager * manager);
void etpan_thread_manager_join(struct etpan_thread_manager * manager);

int etpan_thread_manager_get_fd(struct etpan_thread_manager * manager);
void etpan_thread_manager_loop(struct etpan_thread_manager * manager);

#endif
