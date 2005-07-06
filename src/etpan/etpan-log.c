#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_LIBETPAN

#include "etpan-log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <libetpan/libetpan.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOG_LINE 1024
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static char log_line[MAX_LOG_LINE];
static chash * log_filter = NULL;

static void etpan_str_log(void)
{
  fprintf(stderr, "%s\n", log_line);
}

void etpan_log_init(void)
{
  pthread_mutex_lock(&log_lock);
  if (log_filter == NULL) {
    char * env_value;
    
    env_value = getenv("ETPAN_LOG");
    if (env_value != NULL) {
      strncpy(log_line, env_value, sizeof(log_line));
      log_line[sizeof(log_line) - 1] = '\0';
    }
    else {
      * log_line = '\0';
    }
    env_value = log_line;
    
    log_filter = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
    if (log_filter != NULL) {
      chashdatum key;
      chashdatum value;
      
      key.data = "LOG";
      key.len = strlen("LOG");
      value.data = NULL;
      value.len = 0;
      chash_set(log_filter, &key, &value, NULL);
      
      while (env_value != NULL) {
        char * p;
        
        p = strchr(env_value, ' ');
        if (p != NULL) {
          * p = '\0';
          key.data = env_value;
          key.len = strlen(env_value);
          value.data = NULL;
          value.len = 0;
          chash_set(log_filter, &key, &value, NULL);
          
          env_value = p + 1;
        }
        else {
          key.data = env_value;
          key.len = strlen(env_value);
          value.data = NULL;
          value.len = 0;
          chash_set(log_filter, &key, &value, NULL);
          
          env_value = p;
        }
      }
    }
  }
  pthread_mutex_unlock(&log_lock);
}

void etpan_log_done(void)
{
  pthread_mutex_lock(&log_lock);
  if (log_filter != NULL) {
    chash_free(log_filter);
    log_filter = NULL;
  }
  pthread_mutex_unlock(&log_lock);
}

void etpan_log(char * log_id, char * format, ...)
{
  va_list argp;
  struct timeval time_info;
  int r;
  chashdatum key;
  chashdatum value;
  
  etpan_log_init();
  
  key.data = log_id;
  key.len = strlen(log_id);
  r = chash_get(log_filter, &key, &value);
  if (r < 0)
    return;
  
  r = gettimeofday(&time_info, NULL);
  if (r == 0) {
    fprintf(stderr, "%4lu.%03u [%s] ", time_info.tv_sec % 3600,
        (unsigned int) (time_info.tv_usec / 1000), log_id);
  }
  
  va_start(argp, format);
  pthread_mutex_lock(&log_lock);
  vsnprintf(log_line, sizeof(log_line), format, argp);
  etpan_str_log();
  pthread_mutex_unlock(&log_lock);
  va_end(argp);
}

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/vm_types.h>

extern void thread_stack_pcs(vm_address_t *buffer,
    unsigned max, unsigned *num);

void etpan_log_stack(void)
{
  unsigned buffer[256];
  int num_frames;
  int i;
  char output[1024];
  char * current_output;
  size_t remaining;
  
  thread_stack_pcs(buffer, sizeof(buffer) / sizeof(buffer[0]), &num_frames);
  remaining = sizeof(output);
  current_output = output;
  for(i = 0 ; i < num_frames ; i ++) {
    size_t len;
    
    snprintf(current_output, remaining, "0x%x ", buffer[i]);
    len = strlen(current_output);
    remaining -= len;
    current_output += len;
    if (remaining == 0)
      break;
  }
  ETPAN_STACK_LOG(output);
}
#else
#if defined(__linux__)
#include <execinfo.h>

extern char **backtrace_symbols (void *__const *__array, int __size) __THROW;

void etpan_log_stack(void)
{
  void * buffer[256];
  char ** table;
  int num_frames;
  int i;
  char output[1024];
  char * current_output;
  size_t remaining;
  
  num_frames = backtrace(buffer, sizeof(buffer) / sizeof(buffer[0]));
  remaining = sizeof(output);
  current_output = output;
  for(i = 0 ; i < num_frames ; i ++) {
    size_t len;
    
    snprintf(current_output, remaining, "%p ", buffer[i]);
    len = strlen(current_output);
    remaining -= len;
    current_output += len;
    if (remaining == 0)
      break;
  }
  ETPAN_STACK_LOG(output);
}

#else

void etpan_log_stack(void)
{
  ETPAN_STACK_LOG("this feature not available");
}

#endif
#endif
#endif
