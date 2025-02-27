
/*
 * This file is generated by the automatic RPC Parser generator. (Version 0.21)
 *
 * Created on 04/09/97 at 00:00:13.
 */

#ifndef FUNCS_H
#define FUNCS_H

#include "skeleton.h"
#include "database.h"

#define ALIGN(offset, n) (offset) = (((offset) + (n)) & ~(n))

#define WORDAT(address)  ((fIsFlipped)?(XCHG(*(UNALIGNED WORD *)(address))):(*(UNALIGNED WORD *)(address)))

#define DWORDAT(address) ((fIsFlipped)?(DXCHG(*(UNALIGNED DWORD *)(address))):(*(UNALIGNED DWORD *)(address)))

/*
 * Functions defined in funcs.c
 */
extern VOID WINAPIV GenericFormatSummary(LPPROPERTYINST lpPropertyInst);

extern DWORD WINAPI resmon_RmCreateResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmCloseResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmChangeResourceParams_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmOnlineResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmOfflineResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmTerminateResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmArbitrateResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmReleaseResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmNotifyChanges_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmFailResource_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmShutdownProcess_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmResourceControl_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

extern DWORD WINAPI resmon_RmResourceTypeControl_AttachProperties(HFRAME, LPBYTE, DWORD, DWORD, DWORD);

DWORD DllName_Handler_0(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD ResourceType_Handler_1(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD ResourceName_Handler_2(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD Value_Handler_3(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD ResourceId_Handler_4(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD ResourceId_Handler_5(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD InBuffer_Handler_6(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD OutBuffer_Handler_7(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD ResourceTypeName_Handler_8(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD InBuffer_Handler_9(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD OutBuffer_Handler_10(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD InBuffer_Handler_11(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

DWORD InBuffer_Handler_12(HFRAME, LPBYTE, DWORD, DWORD, LPDWORD);

#endif

