// Copyright (C) 1993-1998  Microsoft Corporation.  All Rights Reserved.
//
//  MODULE: nmmgr.h
//
//  AUTHOR: claus giloi
//
//  COMMENTS: Based on Service sample in NT SDK
//            Code will have the following command line interface
//
//  <service exe> -?                to display this list
//  <service exe> -install          to install the service
//  <service exe> -remove           to remove the service
//  <service exe> -debug <params>   to run as a console app for debugging
//
//  Note: This code also implements Ctrl+C and Ctrl+Break handlers
//        when using the debug option.  These console events cause
//        your ServiceStop routine to be called
//
//        Also, this code only handles the OWN_SERVICE service type
//        running in the LOCAL_SYSTEM security context.
//
//        To control your service ( start, stop, etc ) you may use the
//        Services control panel applet or the NET.EXE program.
//
//        To aid in writing/debugging service, the
//        SDK contains a utility (MSTOOLS\BIN\SC.EXE) that
//        can be used to control, configure, or obtain service status.
//        SC displays complete status for any service/driver
//        in the service database, and allows any of the configuration
//        parameters to be easily changed at the command line.
//        For more information on SC.EXE, type SC at the command line.
//

#ifndef __NTSRVC_H__
#define __NTSRVC_H__


#ifdef __cplusplus
extern "C" {
#endif

extern SERVICE_STATUS          g_ssStatus;       // current status of the service 
extern SERVICE_STATUS_HANDLE   g_sshStatusHandle;


//////////////////////////////////////////////////////////////////////////////
////       The service should use ReportStatusToSCMgr to indicate
////       progress.  This routine must also be used by StartService()
////       to report to the SCM when the service is running.
////
////       If a ServiceStop procedure is going to take longer than
////       3 seconds to execute, it should spawn a thread to
////       execute the stop code, and return.  Otherwise, the
////       ServiceControlManager will believe that the service has
////       stopped responding
////
VOID MNMServiceStart(DWORD dwArgc, LPTSTR *lpszArgv);
BOOL MNMServiceActivate();
BOOL MNMServiceDeActivate();
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//// The following are procedures which
//// may be useful to call within the above procedures,
//// but require no implementation by the user.
//// They are implemented in service.c

//
//  FUNCTION: ReportStatusToSCMgr()
//
//  PURPOSE: Sets the current status of the service and
//           reports it to the Service Control Manager
//
//  PARAMETERS:
//    dwCurrentState - the state of the service
//    dwWin32ExitCode - error code to report
//    dwWaitHint - worst case estimate to next checkpoint
//
//  RETURN VALUE:
//    TRUE  - success 
//    FALSE - failure
//
BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);


#ifdef __cplusplus
}
#endif

#endif __NTSRVC_H__
