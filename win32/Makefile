# /sylpheed-claws/win32/Makefile $Revision$ $Date$
# Toplevel Makefile for exported MSVC Makefiles:
## Microsoft Developer Studio Generated NMAKE File, Based on sylpheed.dsp
## Microsoft Developer Studio Generated NMAKE File, Based on sylpheed_debug.dsp
#
# TARGETS :
#   (none)		- build VERSION, BISON, TRANSLATION, SYLPHEED
#   SYLPHEED		- build sylpheed
#   CLEAN		- clean up .obj/.exe
#   VERSION		- create version.h from configure.in using grep/sed
#   BISON		- run bison/flex on quote_fmt and mather_parser
#   TRANSLATION		- translate .po files using msgfmt
# FLAGS :
#   DEBUG=1		- build debug-enabled version
#   ALL=1		- dont check timestamp (build all targets)
# EXAMPLE :
#   nmake DEBUG=1 sylpheed


!IF "$(DEBUG)" == ""
CFG=sylpheed - Win32 Release
MAK=sylpheed.mak
!ELSE
CFG=sylpheed - Win32 Debug
MAK=sylpheed_debug.mak
!ENDIF

!IF "$(ALL)" == ""
ALLFLAG=
!ELSE
ALLFLAG=/A
!ENDIF

ALL : VERSION BISON TRANSLATION SYLPHEED
	
SYLPHEED :
	nmake -f $(MAK) $(ALLFLAG)

CLEAN :
	nmake -f $(MAK) clean

VERSION :
	@cd utils
	mk_version.bat
	@cd ..

BISON :
	@cd utils
	mk_bison.bat
	@cd ..

TRANSLATION :
	@cd utils
	mk_translation.bat
	@cd ..


