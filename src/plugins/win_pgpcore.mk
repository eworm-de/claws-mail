# Copyright 1999-2022 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if OS_WIN32
plugin_extra_deps += ../pgpcore/.libs/pgpcore.dll.a
plugin_ldflags += -Wl,-L../pgpcore/.libs -Wl,-lpgpcore
endif

if CYGWIN
plugin_libadd += ../pgpcore/pgpcore.la
endif


