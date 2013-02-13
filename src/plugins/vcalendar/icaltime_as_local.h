#ifndef __ICALTIME_AS_LOCAL_H__
#define __ICALTIME_AS_LOCAL_H__
#include <time.h>
#include <ical.h>
#undef PACKAGE_BUGREPORT        /* Why are they in ical.h? */
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE
#undef VERSION

#if !HAVE_ICALTIME_AS_LOCAL

struct icaltimetype icaltime_as_local(struct icaltimetype tt);

#endif
#endif
