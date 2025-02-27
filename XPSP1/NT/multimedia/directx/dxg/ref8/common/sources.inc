TARGETNAME = refcomm
TARGETPATH = obj
TARGETTYPE = LIBRARY

INCLUDES = ..;$(INCLUDES);..\..\drv

SOURCES = \
    ..\refdev.cpp    \
    ..\refdevi.cpp   \
    ..\debugmon.cpp  \
    ..\rddmon.cpp    \
    ..\dxtn.cpp      \
    ..\maplegcy.cpp  \
    ..\pixref.cpp    \
    ..\rdutil.cpp    \
    ..\rtarget.cpp   \
    ..\rdsurf.cpp

PRECOMPILED_INCLUDE = ..\pch.cpp
