//////////////////////////////////////////////////////////////////////////////

//

// Module:      thunk32.c

//

// Part of:     thunk32.dll

//

// Description:

//

//   This module contains the entry and exit functions for windows DLL

//   initialization.

//

//////////////////////////////////////////////////////////////////////////////

//

// History:

//

//  jennymc     8/2/95      Initial version

//  jennymc     1/8/98      Modified for Wbem

//

// Copyright (c) 1995-2001 Microsoft Corporation, All Rights Reserved
//
//////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN 1
#include <pshpack1.h>
#include <windows.h>
#include <win32thk.h>


///////////////////////////////////////////////////////////////////////////
// thunk startup handshake (code in .asm module generated by thunk compiler)
///////////////////////////////////////////////////////////////////////////
BOOL WINAPI win32thk_ThunkConnect32(LPSTR pszDll16,   LPSTR pszDll32,
                				 HINSTANCE  hInst, DWORD dwReason);

//////////////////////////////////////////////////////////////////////////
//
// Function:    DllMain
//
///////////////////////////////////////////////////////////////////////////
//
// Description: The main entry point for the DLL.  This is where we
//              set up the thunk connection.  Nothing else special is
//              done at this point.
//
///////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(HANDLE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    DWORD dwOffset;
    DWORD dwData;

    if (!(win32thk_ThunkConnect32(CIM16NET_DLL, CIM32NET_DLL, hDLL, dwReason))){
        #ifdef DEBUG
    	    MessageBox( NULL, "Thunk Error","ERROR",MB_OK);
        #endif
	    return FALSE;
    }

    switch( dwReason ) {
    	case DLL_PROCESS_ATTACH:
//            dwOffset = *((&thk_ThunkData32) + 7);
//            dwData = *(DWORD *)((char *)(&thk_ThunkData32) + dwOffset);
//            if (dwData == 0xCCCCCCCC)
//            {
//                return FALSE;
//            }

	        break;

	    case DLL_PROCESS_DETACH:
	        break;
    }

    return TRUE;
}

//***********************************************************************
//***********************************************************************
ULONG WINAPI CIM16GetUserInfo1(LPSTR Name, LPSTR Comment, LPSTR HomeDirectory,
			                  LPSTR ScriptPath, unsigned long * PasswordAge,
			                  unsigned short * Privilges, unsigned short * Flags);

ULONG WINAPI CIM16GetUserInfo2(LPSTR Name, LPSTR FullName,
		       LPSTR UserComment, LPSTR Parameters,
		       LPSTR Workstations, LPSTR LogonServer,
               LPLOGONDETAILS LogonDetails );

ULONG WINAPI CIM16GetUserInfo1Ex(LPSTR Domain, LPSTR Name, DWORD fGetDC, LPSTR Comment, LPSTR HomeDirectory,
			                  LPSTR ScriptPath, unsigned long * PasswordAge,
			                  unsigned short * Privilges, unsigned short * Flags);

ULONG WINAPI CIM16GetUserInfo2Ex(LPSTR Domain, LPSTR Name, DWORD fGetDC, LPSTR FullName,
		       LPSTR UserComment, LPSTR Parameters,
		       LPSTR Workstations, LPSTR LogonServer,
               LPLOGONDETAILS LogonDetails );

ULONG WINAPI CIM16GetConfigManagerStatus(LPSTR HardwareKey);

BYTE WINAPI CIM16GetDrivePartitions(BYTE cDrive, pMasterBootSector MBR);
BYTE WINAPI CIM16GetDriveParams(BYTE cDrive, pInt13DriveParams pParams);
WORD WINAPI CIM16GetBiosUnit(LPSTR);

ULONG WINAPI CIM16GetUseInfo1(
    LPSTR Name,
    LPSTR Local,
    LPSTR Remote,
    LPSTR Password,
    LPULONG pdwStatus,
    LPULONG pdwType,
    LPULONG pdwRefCount,
    LPULONG pdwUseCount);

ULONG WINAPI CIM16NetUseEnum(
    LPCSTR pszServer,
    short sLevel,
    LPSTR pbBuffer,
    use_info_1Out *pBuffer2,
    unsigned short cbBuffer,
    unsigned short far *pcEntriesRead,
    unsigned short far *pcTotalAvail
);

// Config manager exports
WORD WINAPI CIM16_CM_Locate_DevNode( PDEVNODE pdn, LPSTR HardwareKey, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Parent( PDEVNODE pdn, DEVNODE dnChild, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Child( PDEVNODE pdn, DEVNODE dnParent, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Sibling( PDEVNODE pdn, DEVNODE dnParent, ULONG ulFlags );
WORD WINAPI CIM16_CM_Read_Registry_Value( DEVNODE dnDevNode, LPSTR pszSubKey, LPSTR pszValueName, ULONG ulExpectedType, LPVOID Buffer, LPULONG pulLength, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_DevNode_Status( LPULONG pulStatus, LPULONG pulProblemNumber, DEVNODE dnDevNode, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Device_ID( DEVNODE dnDevNode, LPVOID Buffer, ULONG BufferLen, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Device_ID_Size( LPULONG pulLen, DEVNODE dnDevNode, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_First_Log_Conf( PLOG_CONF plcLogConf, DEVNODE dnDevNode, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Next_Res_Des( PRES_DES prdResDes, RES_DES rdResDes, RESOURCEID ForResource, PRESOURCEID pResourceID, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Res_Des_Data_Size( LPULONG pulSize, RES_DES rdResDes, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Res_Des_Data( RES_DES rdResDes, LPVOID Buffer, ULONG BufferLen, ULONG ulFlags );
WORD WINAPI CIM16_CM_Get_Bus_Info(DEVNODE dnDevNode, PCMBUSTYPE pbtBusType, LPULONG pulSizeOfInfo, LPVOID pInfo, ULONG ulFlags);

WORD WINAPI CIM16_CM_Query_Arbitrator_Free_Data(LPVOID pData, ULONG DataLen, DEVNODE dnDevInst, RESOURCEID ResourceID, ULONG ulFlags);
WORD WINAPI CIM16_CM_Delete_Range(ULONG ulStartValue, ULONG ulEndValue, RANGE_LIST rlh, ULONG ulFlags);
WORD WINAPI CIM16_CM_First_Range(RANGE_LIST rlh, LPULONG pulStart, LPULONG pulEnd, PRANGE_ELEMENT preElement, ULONG ulFlags);
WORD WINAPI CIM16_CM_Next_Range(PRANGE_ELEMENT preElement, LPULONG pulStart, LPULONG pullEnd, ULONG ulFlags);
WORD WINAPI CIM16_CM_Free_Range_List(RANGE_LIST rlh, ULONG ulFlags);
DWORD WINAPI CIM16GetFreeSpace ( UINT option ) ;

//////////////////////////////////////////////////////////////////////////////
// Function:
//
//   gGetAllUserGroups
//
// Comments:
//   Calls the gGetLoggedOnUsersGroups in the 16 bit dll via thunk
//////////////////////////////////////////////////////////////////////////////
ULONG WINAPI GetWin9XUserInfo1(LPSTR Name,	LPSTR HomeDirectory,
					   LPSTR Comment, LPSTR ScriptPath, LPULONG PasswordAge,
					   LPUSHORT Privileges, LPUSHORT Flags )
{
    return(CIM16GetUserInfo1( Name,HomeDirectory,Comment,ScriptPath,
                              PasswordAge, Privileges, Flags));
}

//////////////////////////////////////////////////////////////////////
ULONG WINAPI GetWin9XUserInfo2(LPSTR Name, LPSTR FullName,
	LPSTR UserComment, LPSTR Parameters, LPSTR Workstations,
	LPSTR LogonServer, LPLOGONDETAILS LogonDetails )

{
    return(CIM16GetUserInfo2(Name, FullName,UserComment, Parameters,
                Workstations, LogonServer, LogonDetails ));
}

//////////////////////////////////////////////////////////////////////
ULONG WINAPI GetWin9XUserInfo1Ex(LPSTR Domain, LPSTR Name, DWORD fGetDC, LPSTR HomeDirectory,
					   LPSTR Comment, LPSTR ScriptPath, LPULONG PasswordAge,
					   LPUSHORT Privileges, LPUSHORT Flags )
{
    return(CIM16GetUserInfo1Ex( Domain, Name, fGetDC, HomeDirectory, Comment, ScriptPath,
                              PasswordAge, Privileges, Flags));
}

//////////////////////////////////////////////////////////////////////
ULONG WINAPI GetWin9XUserInfo2Ex(LPSTR Domain, LPSTR Name, DWORD fGetDC, LPSTR FullName,
	LPSTR UserComment, LPSTR Parameters, LPSTR Workstations,
	LPSTR LogonServer, LPLOGONDETAILS LogonDetails )

{
    return(CIM16GetUserInfo2Ex(Domain, Name, fGetDC, FullName,UserComment, Parameters,
                Workstations, LogonServer, LogonDetails ));
}

//////////////////////////////////////////////////////////////////////
ULONG WINAPI GetWin9XConfigManagerStatus(LPSTR HardwareKey)
{
    return(CIM16GetConfigManagerStatus(HardwareKey));
}

ULONG WINAPI GetWin9XUseInfo1(
    LPSTR Name,
    LPSTR Local,
    LPSTR Remote,
    LPSTR Password,
    LPULONG pdwStatus,
    LPULONG pdwType,
    LPULONG pdwRefCount,
    LPULONG pdwUseCount)
{
    return
        CIM16GetUseInfo1(
            Name,
            Local,
            Remote,
            Password,
            pdwStatus,
            pdwType,
            pdwRefCount,
            pdwUseCount);
}

ULONG WINAPI GetWin9XNetUseEnum(
    LPCSTR pszServer,
    short sLevel,
    LPSTR pbBuffer,
    use_info_1Out *pBuffer2,
    unsigned short cbBuffer,
    unsigned short *pcEntriesRead,
    unsigned short *pcTotalAvail
)
{
    return
        CIM16NetUseEnum(
            pszServer,
            sLevel,
            pbBuffer,
            pBuffer2,
            cbBuffer,
            pcEntriesRead,
            pcTotalAvail);

}

// Config Manager API call-thrus
DWORD WINAPI CIM32THK_CM_Locate_DevNode( PDEVNODE pdn, LPSTR HardwareKey, ULONG ulFlags )
{
	return( CIM16_CM_Locate_DevNode( pdn, HardwareKey, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Parent( PDEVNODE pdn, DEVNODE dnChild, ULONG ulFlags )
{
	return( CIM16_CM_Get_Parent( pdn, dnChild, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Child( PDEVNODE pdn, DEVNODE dnParent, ULONG ulFlags )
{
	return( CIM16_CM_Get_Child( pdn, dnParent, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Sibling( PDEVNODE pdn, DEVNODE dnParent, ULONG ulFlags )
{
	return( CIM16_CM_Get_Sibling( pdn, dnParent, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Read_Registry_Value( DEVNODE dnDevNode, LPSTR pszSubKey, LPSTR pszValueName, ULONG ulExpectedType, LPVOID Buffer, LPULONG pulLength, ULONG ulFlags )
{
	return( CIM16_CM_Read_Registry_Value( dnDevNode, pszSubKey, pszValueName, ulExpectedType, Buffer, pulLength, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_DevNode_Status( LPULONG pulStatus, LPULONG pulProblemNumber, DEVNODE dnDevNode, ULONG ulFlags )
{
	return( CIM16_CM_Get_DevNode_Status( pulStatus, pulProblemNumber, dnDevNode, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Device_ID( DEVNODE dnDevNode, LPVOID Buffer, ULONG BufferLen, ULONG ulFlags )
{
	return( CIM16_CM_Get_Device_ID( dnDevNode, Buffer, BufferLen, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Device_ID_Size( LPULONG pulLen, DEVNODE dnDevNode, ULONG ulFlags )
{
	return( CIM16_CM_Get_Device_ID_Size( pulLen, dnDevNode, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_First_Log_Conf( PLOG_CONF plcLogConf, DEVNODE dnDevNode, ULONG ulFlags )
{
	return( CIM16_CM_Get_First_Log_Conf( plcLogConf, dnDevNode, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Next_Res_Des( PRES_DES prdResDes, RES_DES rdResDes, RESOURCEID ForResource, PRESOURCEID pResourceID, ULONG ulFlags )
{
	return( CIM16_CM_Get_Next_Res_Des( prdResDes, rdResDes, ForResource, pResourceID, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Res_Des_Data_Size( LPULONG pulSize, RES_DES rdResDes, ULONG ulFlags )
{
	return( CIM16_CM_Get_Res_Des_Data_Size( pulSize, rdResDes, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Res_Des_Data( RES_DES rdResDes, LPVOID Buffer, ULONG BufferLen, ULONG ulFlags )
{
	return( CIM16_CM_Get_Res_Des_Data( rdResDes, Buffer, BufferLen, ulFlags ) );
}

DWORD WINAPI CIM32THK_CM_Get_Bus_Info(DEVNODE dnDevNode, PCMBUSTYPE pbtBusType, LPULONG pulSizeOfInfo, LPVOID pInfo, ULONG ulFlags)
{
	return( CIM16_CM_Get_Bus_Info( dnDevNode, pbtBusType, pulSizeOfInfo, pInfo, ulFlags ) );
}

WORD WINAPI CIM32THK_CM_Query_Arbitrator_Free_Data(LPVOID pData, ULONG DataLen, DEVNODE dnDevInst, RESOURCEID ResourceID, ULONG ulFlags)
{
    return CIM16_CM_Query_Arbitrator_Free_Data(pData, DataLen, dnDevInst, ResourceID, ulFlags);
}

WORD WINAPI CIM32THK_CM_Delete_Range(ULONG ulStartValue, ULONG ulEndValue, RANGE_LIST rlh, ULONG ulFlags)
{
    return CIM16_CM_Delete_Range(ulStartValue, ulEndValue, rlh, ulFlags);
}

WORD WINAPI CIM32THK_CM_First_Range(RANGE_LIST rlh, LPULONG pulStart, LPULONG pulEnd, PRANGE_ELEMENT preElement, ULONG ulFlags)
{
    return CIM16_CM_First_Range(rlh, pulStart, pulEnd, preElement, ulFlags);
}

WORD WINAPI CIM32THK_CM_Next_Range(PRANGE_ELEMENT preElement, LPULONG pulStart, LPULONG pullEnd, ULONG ulFlags)
{
    return CIM16_CM_Next_Range(preElement, pulStart, pullEnd, ulFlags);
}

WORD WINAPI CIM32THK_CM_Free_Range_List(RANGE_LIST rlh, ULONG ulFlags)
{
    return CIM16_CM_Free_Range_List(rlh, ulFlags);
}


BYTE WINAPI GetWin9XPartitionTable(BYTE cDrive, pMasterBootSector pMBR)
{
    return(CIM16GetDrivePartitions(cDrive, pMBR));
}

BYTE WINAPI GetWin9XDriveParams(BYTE cDrive, pInt13DriveParams pParams)
{
    return(CIM16GetDriveParams(cDrive, pParams));
}

WORD WINAPI GetWin9XBiosUnit(LPSTR lpDeviceID)
{
    return(CIM16GetBiosUnit(lpDeviceID));
}

DWORD WINAPI GetWin9XFreeSpace ( DWORD option )
{
	return CIM16GetFreeSpace ( option ) ;
}

