############################################################################
#
#   Copyright (C) 1999, Microsoft Corporation.
#
#   All rights reserved.
#
############################################################################

WMIUNICODE=1
WMIPRECOMPSHARED=1
!include $(PROJECT_ROOT)\wmi\wbem\common\makefile.cmn
SOURCES_USED=$(PROJECT_ROOT)\wmi\wbem\common\makefile.cmn

TARGETNAME=Provider
TARGETPATH=obj

!if "$(WMIASLOCAL)" == "true"
TARGETTYPE=PROGRAM
UMENTRY=winmain
UMTYPE=windows
!else
TARGETTYPE=DYNLINK
!endif

# The TARGETLIBS macro specifies additional libraries to link against your target
# image. Each library path specification should contain an asterisk (*)
# where the machine-specific subdirectory name should go.
LINKLIBS= \
	$(WBEMPRV_COMN_LIB)
TARGETLIBS= \
	$(WMIIDL_LIB) \
	$(WBEMINT_LIB) \
	$(WMIIDL_LIB)  \
	$(STDLIBRARY_LIB) \
	$(COREPROX_LIB) \
	$(SDK_LIB_PATH)\ntdll.lib \
	$(SDK_LIB_PATH)\kernel32.lib \
	$(SDK_LIB_PATH)\advapi32.lib \
	$(SDK_LIB_PATH)\user32.lib \
	$(SDK_LIB_PATH)\oleaut32.lib \
	$(SDK_LIB_PATH)\ole32.lib \
	$(SDK_LIB_PATH)\uuid.lib \
	$(SDK_LIB_PATH)\msvcirt.lib \
	$(SDK_LIB_PATH)\msvcrt.lib \
	$(SDK_LIB_PATH)\vccomsup.lib

# The INCLUDES variable specifies any include paths that are specific to
# this source directory. Separate multiple paths with single
# semicolons. Relative path specifications are okay.
INCLUDES=$(INCLUDES); \
	$(WBEMINT_INC);\
	$(WBEMPSSPROV_INC); \
	$(CONTAINER_INC); \
	$(CONTAINER_SRC); \
	$(WBEMPRV_COMN_INC);\

# The developer defines the SOURCES macro. It contains a list of all the
# source files for this component. Specify each source file on a separate
# line using the line-continuation character. This minimizes merge
# conflicts if two developers are adding source files to the same component.

!if "$(WMIASLOCAL)" == "true"
XSOURCES= \
	..\Main.cpp
!endif

SOURCES=$(XSOURCES) \
	..\Provider.rc \
	..\Globals.cpp \
	..\Maindll.cpp \
	..\Service.cpp \
	..\ClassService.cpp \
	..\Task.cpp \


# Add the following to add DB verbose logging: '/DREP_VERBOSE_LOGGING'

!if "$(WMIASLOCAL)" == "true"
C_DEFINES=$(C_DEFINES) /DWMIASLOCAL
!endif

!if "$(WMIASSTA)" == "true"
C_DEFINES=$(C_DEFINES) /DWMIASSTA
!endif

C_DEFINES=$(C_DEFINES) /D_WIN32_DCOM
MSC_OPTIMIZATION=/Od 
USE_NATIVE_EH=ASYNC
USE_RTTI=1
USE_MSVCRT=1

!if "$(WMIASLOCAL)" != "true"
DLLENTRY=_DllMainCRTStartup
!endif

!if "$(WMIASLOCAL)" != "true"
DLLDEF=$(O)\Provider.def
!else
EXEDEF=$(O)\Provider.def
!endif
