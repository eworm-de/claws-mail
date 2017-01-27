#ifndef __ICALTIME_AS_LOCAL_H__
#define __ICALTIME_AS_LOCAL_H__
#include <time.h>
#include <libical/ical.h>

#if !HAVE_ICALTIME_AS_LOCAL

struct icaltimetype icaltime_as_local(struct icaltimetype tt);

#endif
#endif
