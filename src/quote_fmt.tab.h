#ifndef QUOTE_FMT

#define QUOTE_FMT

#include "procmsg.h"

int quote_fmtparse(void);
void quote_fmt_init(MsgInfo * info, gchar * quote_str);
gchar * quote_fmt_get_buffer();

#endif

