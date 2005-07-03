#ifndef ETPAN_LOG_H

#define ETPAN_LOG_H

#define ENABLE_LOG

#ifdef ENABLE_LOG

/*
  an environment variable ETPAN_LOG must be set
  and will contains the log identifier separated with spaces.
*/

#define ETPAN_GTK_TREE_MODEL_LOG(...) ETPAN_LOG_ID("GTKTREEMODEL", __VA_ARGS__)
#define ETPAN_TABBED_LOG(...) ETPAN_LOG_ID("TABBED", __VA_ARGS__)
#define ETPAN_MSGVIEW_LOG(...) ETPAN_LOG_ID("MSGVIEW", __VA_ARGS__)
#define ETPAN_MSGLIST_LOG(...) ETPAN_LOG_ID("MSGLIST", __VA_ARGS__)
#define ETPAN_FOLDERLIST_LOG(...) ETPAN_LOG_ID("FOLDERLIST", __VA_ARGS__)
#define ETPAN_MAINWINDOW_LOG(...) ETPAN_LOG_ID("MAINWINDOW", __VA_ARGS__)
#define ETPAN_SIGNAL_LOG(...) ETPAN_LOG_ID("SIGNAL", __VA_ARGS__)
#define ETPAN_MEM_LOG(...) ETPAN_LOG_ID("MEM", __VA_ARGS__)
#define ETPAN_UI_LOG(...) ETPAN_LOG_ID("UI", __VA_ARGS__)
#define ETPAN_PROGRESS_LOG(...) ETPAN_LOG_ID("PROGRESS", __VA_ARGS__)
#define ETPAN_DEBUG_LOG(...) ETPAN_LOG_ID("DEBUG", __VA_ARGS__)
#define ETPAN_STACK_LOG(...) ETPAN_LOG_ID("STACK", __VA_ARGS__)
#define ETPAN_LOG(...) ETPAN_LOG_ID("LOG", __VA_ARGS__)

#else

#define ETPAN_GTK_TREE_MODEL_LOG(...)
#define ETPAN_TABBED_LOG(...)
#define ETPAN_MSGVIEW_LOG(...)
#define ETPAN_MSGLIST_LOG(...)
#define ETPAN_FOLDERLIST_LOG(...)
#define ETPAN_MAINWINDOW_LOG(...)
#define ETPAN_SIGNAL_LOG(...)
#define ETPAN_MEM_LOG(...)
#define ETPAN_UI_LOG(...)
#define ETPAN_PROGRESS_LOG(...)
#define ETPAN_DEBUG_LOG(...)
#define ETPAN_STACK_LOG(...)
#define ETPAN_LOG(...) ETPAN_LOG_ID("LOG", __VA_ARGS__)

#endif

#define ETPAN_LOG_ID(...) etpan_log(__VA_ARGS__)

void etpan_log_init(void);
void etpan_log_done(void);
void etpan_log(char * log_id, char * format, ...);

void etpan_log_stack(void);

#endif
