#ifndef QUOTE_FMT_H

#define QUOTE_FMT_H

gchar * quote_fmt_get_buffer();
void quote_fmt_init(MsgInfo * info, gchar * my_quote_str);
int quote_fmtparse(void);

#endif
