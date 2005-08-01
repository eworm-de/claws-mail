#ifndef ETPAN_THREAD_MANAGER_TYPES_H

#define ETPAN_THREAD_MANAGER_TYPES_H

#include <pthread.h>
#include <libetpan/libetpan.h>

struct etpan_thread_manager {
  /* thread pool */
  carray * thread_pool;
  carray * thread_pending;
  int can_create_thread;
  
  int unbound_count;
  
  int notify_fds[2];
};

struct etpan_thread {
  struct etpan_thread_manager * manager;
  
  pthread_t th_id;
  
  pthread_mutex_t lock;
  carray * op_list;
  carray * op_done_list;
  
  int bound_count;
  int terminate_state;
  
  struct mailsem * start_sem;
  struct mailsem * stop_sem;
  struct mailsem * op_sem;
};

struct etpan_thread_op {
  struct etpan_thread * thread;
  
  void (* run)(struct etpan_thread_op * op);
  
  void (* callback)(int cancelled, void * result, void * callback_data);
  void * callback_data;
  
  void (* cleanup)(struct etpan_thread_op * op);
  
  pthread_mutex_t lock;
  int callback_called;
  int cancellable;
  int cancelled;
  void * param;
  void * result;
};

#endif
