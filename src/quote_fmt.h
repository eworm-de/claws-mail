#ifndef __QUOTE_FMT_H__

#define __QUOTE_FMT_H__

#define quote_fmt_parse	quote_fmtparse

gchar *quote_fmt_get_buffer(void);
void quote_fmt_init(MsgInfo *info, gchar *my_quote_str, const gchar *seltext);
gint quote_fmtparse(void);
void quote_fmt_scan_string(const gchar *str);

#endif /* __QUOTE_FMT_H__ */
