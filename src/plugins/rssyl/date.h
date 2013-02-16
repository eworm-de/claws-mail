#ifndef __DATE_H
#define __DATE_H

#include <time.h>
#include <glib.h>

time_t parseISO8601Date(gchar *date);
gchar *createRFC822Date(const time_t *time);

#endif /* __DATE_H */
