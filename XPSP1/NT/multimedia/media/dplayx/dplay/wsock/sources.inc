MAJORCOMP=windows
MINORCOMP=GDI
UMTYPE=windows

TARGETNAME=dpwsockx
TARGETTYPE=DYNLINK

INCLUDES=..\..\..\inc; \
	 ..\..\..\misc; \
	..\; \
	 ..\..\dplay; \
	 ..\..\dplaysvr; \
	 ..\..\common; \
	 ..\..\nmvoice; \
	 ..\..\..\dplobby\dplobby;

TARGETPATH=obj

TARGETLIBS=$(SDK_LIB_PATH)\libc.lib    \
	   $(SDK_LIB_PATH)\user32.lib    \
	   $(SDK_LIB_PATH)\kernel32.lib  \
	   $(SDK_LIB_PATH)\wsock32.lib \
	   $(SDK_LIB_PATH)\dplayx.lib \
	   $(SDK_LIB_PATH)\ole32.lib \
	   $(SDK_LIB_PATH)\oleaut32.lib \
	   $(SDK_LIB_PATH)\uuid.lib \
	   $(SDK_LIB_PATH)\winmm.lib

!IFNDEF MSC_WARNING_LEVEL
MSC_WARNING_LEVEL=/W3
!ENDIF
MSC_WARNING_LEVEL=$(MSC_WARNING_LEVEL) /WX

!IFDEF PENTIUMCOUNTER
C_DEFINES=$(C_DEFINES) -DPENTIUMCOUNTER
ASM_DEFINES=$(ASM_DEFINES) -DPENTIUMCOUNTER
!endif

C_DEFINES=$(C_DEFINES) -DUSE_CRITSECTS -DWIN16_SEPARATE -DNEW_DPF

LINKER_FLAGS=-SECTION:share,RWS

#LINKLIBS= \

SOURCES= \
	..\bilink.c \
	..\fpm.c \
	..\wsock2.c \
	..\leverage.c \
	..\dpsp.c \
	..\winsock.c \
	..\dllmain.c \
	..\helpcli.c \
	..\handler.c \
	..\dpwsock.rc \
	memalloc.c \
	newdpf.c \
	dputils.c

DLLENTRY=_DllMainCRTStartup
DLLDEF=..\dpwsock.def
NTTARGETFILE0=MISC
