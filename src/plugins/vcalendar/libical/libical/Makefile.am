#======================================================================
#  FILE: Makefile.am
#  CREATOR: eric 
#  
#  $Id$
#
#
# (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either: 
#
#    The LGPL as published by the Free Software Foundation, version
#    2.1, available at: http://www.fsf.org/copyleft/lesser.html
#
#  Or:
#
#    The Mozilla Public License Version 1.0. You may obtain a copy of
#    the License at http://www.mozilla.org/MPL/
#
#  The original code is icalcomponent.c
#
#======================================================================


DESIGNDATA = $(top_srcdir)/src/plugins/vcalendar/libical/design-data
ICALSCRIPTS = $(top_srcdir)/src/plugins/vcalendar/libical/scripts

if BUILD_VCALENDAR_PLUGIN
noinst_LTLIBRARIES = libical.la
endif

YFLAGS = -d -v -t -pical_yy
LFLAGS = -Pical_yy
LEX_OUTPUT_ROOT = lex.ical_yy

INCLUDES =			\
	-I$(top_builddir)	\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(srcdir)		\
	-I$(top_srcdir)/src/plugins/vcalendar/libical \
	-I$(top_srcdir)/src/plugins/vcalendar/libical/libical

libical_la_LDFLAGS = -version-info 0:0:0

libical_la_SOURCES =		\
	icalattendee.h		\
	icalattendee.c		\
	icalcomponent.c		\
	icalcomponent.h		\
	icalenums.c		\
	icalenums.h		\
	icalerror.c		\
	icalerror.h		\
	icallexer.l		\
	icalmemory.c		\
	icalmemory.h		\
	icalmime.c		\
	icalmime.h		\
	icalparameter.c		\
	icalparameter.h		\
	icalparameterimpl.h	\
	icalderivedparameter.c	\
	icalderivedparameter.h	\
	icalparser.c		\
	icalparser.h		\
	icalderivedproperty.c	\
	icalderivedproperty.h	\
	icalproperty.c		\
	icalproperty.h		\
	icalrecur.c		\
	icalrecur.h		\
	icalrestriction.c	\
	icalrestriction.h	\
	icaltime.c		\
	icaltime.h		\
	icalduration.h		\
	icalduration.c		\
	icalperiod.h		\
	icalperiod.c		\
	icaltypes.c		\
	icaltypes.h		\
	icalvalue.c		\
	icalvalue.h		\
	icalvalueimpl.h		\
	icalderivedvalue.c	\
	icalderivedvalue.h	\
	icalyacc.h		\
	icalyacc.y		\
	pvl.c			\
	pvl.h			\
	sspm.c			\
	sspm.h			\
	vsnprintf.c		\
	icallangbind.h		

libicalincludedir = $(includedir)/claws-mail/plugins/@PACKAGE@

libicalinclude_HEADERS =  ical.h 

# ORDERING OF HEADERS IS SIGNIFICANT. Don't change this ordering. It
# is required to make the combined header ical.h properly
COMBINEDHEADERS =					\
	$(top_builddir)/src/plugins/vcalendar/libical/libical/icalversion.h	\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icaltime.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalduration.h	\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalperiod.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalenums.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icaltypes.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalrecur.h		\
	icalderivedvalue.h				\
	icalderivedparameter.h				\
	icalvalue.h					\
	icalparameter.h					\
	icalderivedproperty.h				\
	icalproperty.h					\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalattendee.h	\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/pvl.h			\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalcomponent.h	\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalparser.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalmemory.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalerror.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalrestriction.h	\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/sspm.h		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icalmime.h 		\
	$(top_srcdir)/src/plugins/vcalendar/libical/libical/icallangbind.h	

BUILT_SOURCES =			\
	ical.h			\
	icalderivedparameter.c	\
	icalderivedparameter.h	\
	icalderivedproperty.c	\
	icalderivedproperty.h	\
	icalrestriction.c	\
	icalderivedvalue.c	\
	icalderivedvalue.h

ical.h: $(COMBINEDHEADERS)
	cat $(COMBINEDHEADERS)				\
	| egrep -v "#include.*\"ical"			\
	| egrep -v "#include.*\"pvl\.h\"" > ical.h

icallexer.c : icalyacc.h


# parameters

PARAMETERDEPS =	\
	$(ICALSCRIPTS)/mkderivedparameters.pl \
	$(DESIGNDATA)/parameters.csv	\
	icalderivedparameter.c.in \
	icalderivedparameter.h.in

icalderivedparameter.h: $(PARAMETERDEPS) 
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.h.in -h $(DESIGNDATA)/parameters.csv > icalderivedparameter.h 

icalderivedparameter.c: $(PARAMETERDEPS) icalparameter.h
	$(PERL) -I $(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedparameters.pl -i $(srcdir)/icalderivedparameter.c.in -c $(DESIGNDATA)/parameters.csv > icalderivedparameter.c


# properties

PROPERTYDEPS =					\
	$(ICALSCRIPTS)/mkderivedproperties.pl	\
	$(DESIGNDATA)/properties.csv		\
	$(DESIGNDATA)/value-types.csv		\
	icalderivedproperty.c.in		\
	icalderivedproperty.h.in


icalderivedproperty.h: $(PROPERTYDEPS)
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.h.in -h $(DESIGNDATA)/properties.csv\
		${DESIGNDATA}/value-types.csv > icalderivedproperty.h 

icalderivedproperty.c: $(PROPERTYDEPS) icalproperty.h 
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedproperties.pl \
		-i $(srcdir)/icalderivedproperty.c.in -c $(DESIGNDATA)/properties.csv \
		${DESIGNDATA}/value-types.csv > icalderivedproperty.c

# restrictions

RESTRICTIONDEPS =				\
	$(ICALSCRIPTS)/mkrestrictiontable.pl	\
	$(DESIGNDATA)/restrictions.csv		\
	icalrestriction.c.in

icalrestriction.c: $(RESTRICTIONDEPS)
	$(PERL) $(ICALSCRIPTS)/mkrestrictiontable.pl  -i $(srcdir)/icalrestriction.c.in \
		$(DESIGNDATA)/restrictions.csv > icalrestriction.c

# values

VALUEDEPS =					\
	$(ICALSCRIPTS)/mkderivedvalues.pl  	\
	$(DESIGNDATA)/value-types.csv		\
	icalderivedvalue.c.in				\
	icalderivedvalue.h.in

icalderivedvalue.h: $(VALUEDEPS)
	$(PERL) -I$(ICALSCRIPTS)  $(ICALSCRIPTS)/mkderivedvalues.pl \
	         -i $(srcdir)/icalderivedvalue.h.in -h $(DESIGNDATA)/value-types.csv > icalderivedvalue.h

icalderivedvalue.c: $(VALUEDEPS) icalderivedvalue.h
	$(PERL)  -I$(ICALSCRIPTS) $(ICALSCRIPTS)/mkderivedvalues.pl \
		-i $(srcdir)/icalderivedvalue.c.in -c $(DESIGNDATA)/value-types.csv > icalderivedvalue.c



# housekeeping
CONFIG_CLEAN_FILES = y.output

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =			\
	icalderivedparameter.c.in \
	icalderivedparameter.h.in \
	icalderivedproperty.c.in \
	icalderivedproperty.h.in \
	icalrestriction.c.in	\
	icalderivedvalue.c.in	\
	icalderivedvalue.h.in	\
	icalversion.h.in	\
	icallexer.c		\
	icalyacc.c

AM_CPPFLAGS = -DNDEBUG $(GLIB_CFLAGS) 
