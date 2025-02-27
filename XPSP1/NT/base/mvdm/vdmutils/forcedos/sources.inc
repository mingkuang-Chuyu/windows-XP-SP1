!IF 0

Copyright (c) 1989  Microsoft Corporation

Module Name:

    sources.

Abstract:

    This file specifies the target component being built and the list of
    sources files needed to build that component.  Also specifies optional
    compiler switches and libraries that are unique for the component being
    built.


Author:

    Steve Wood (stevewo) 12-Apr-1990

NOTE:   Commented description of this file is in \nt\bak\bin\sources.tpl

!ENDIF

TARGETNAME=forcedos
TARGETPATH=obj
TARGETTYPE=PROGRAM

SOURCES=..\forcedos.c ..\forcedos.rc

C_DEFINES=/DUNICODE=1 /DNOMINMAX

USE_MSVCRT=1

UMLIBS=$(SDK_LIB_PATH)\user32.lib \
       $(SDK_LIB_PATH)\ntdll.lib

MSC_WARNING_LEVEL=/W3 /WX

UMTYPE=console
SOURCES_USED=..\sources.inc