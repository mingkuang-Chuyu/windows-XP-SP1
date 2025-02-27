// NonCOMEvent.cpp : Implementation of WinMain

#include "PreComp.h"
#include "resource.h"

#include <initguid.h>

#include "NonCOMEvent.h"
#include "..\NonCOMEventPS\NonCOMEvent_i.c"

#include "NonCOMEventModule.h"
#include "..\NonCOMEventPS\NonCOMEventModule_i.c"

// debuging features
#ifndef	_INC_CRTDBG
#include <crtdbg.h>
#endif	_INC_CRTDBG

// new stores file/line info
#ifdef _DEBUG
#ifndef	NEW
#define NEW new( _NORMAL_BLOCK, __FILE__, __LINE__ )
#define new NEW
#endif	NEW
#endif	_DEBUG

// declarations
#include "_Module.h"
#include "_App.h"

// debug behaviour
#ifdef	_DEBUG
#endif	_DEBUG

/////////////////////////////////////////////////////////////////////////////
// VARIABLES
/////////////////////////////////////////////////////////////////////////////

// app
MyApp		_App ( IDS_PROJNAME );

// com module
MyModule	_Module;

/////////////////////////////////////////////////////////////////////////////
// dialogs
/////////////////////////////////////////////////////////////////////////////

#include "_Dlg.h"
#include "_DlgImpl.h"

#include "NonCOMEventAboutDlg.h"
#include "NonCOMEventMainDlg.h"

#include "module.h"

/////////////////////////////////////////////////////////////////////////////
// OBJECT MAP
/////////////////////////////////////////////////////////////////////////////

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_ModuleScalar, CModuleScalar)
OBJECT_ENTRY(CLSID_ModuleArray, CModuleArray)
OBJECT_ENTRY(CLSID_ModuleGeneric, CModuleGeneric)
END_OBJECT_MAP()

////////////////////////////////////////////////////////////////////////////
// ATL stuff
////////////////////////////////////////////////////////////////////////////

// need atl wrappers
#ifndef	__ATLBASE_H__
#include <atlbase.h>
#endif	__ATLBASE_H__

// need registry
#ifdef _ATL_STATIC_REGISTRY
#include <statreg.h>
#include <statreg.cpp>
#endif

#include <atlimpl.cpp>

/////////////////////////////////////////////////////////////////////////////
//
// WIN MAIN
//
/////////////////////////////////////////////////////////////////////////////

extern "C" HRESULT	WINAPI _tWinRun		( int nCmdShow = SW_SHOWDEFAULT, BOOL bGUI = FALSE );
extern "C" int		WINAPI _tWinMain	( HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow )
{
	return WinMain ( hInstance, NULL, NULL, nCmdShow );
}

extern "C" int WINAPI   WinMain		( HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow )
{
	HRESULT hRes		= S_OK;

	////////////////////////////////////////////////////////////////////////
	// initialization
	////////////////////////////////////////////////////////////////////////
    _Module.Init	( ObjectMap, hInstance, &LIBID_NONCOMEVENTLib );

	////////////////////////////////////////////////////////////////////////
	// variables
	////////////////////////////////////////////////////////////////////////
    WCHAR	szTokens[]	= L"-/";

	////////////////////////////////////////////////////////////////////////
	// command line
	////////////////////////////////////////////////////////////////////////
    LPWSTR lpCmdLine = GetCommandLine();

	////////////////////////////////////////////////////////////////////////
	// find behaviour
	////////////////////////////////////////////////////////////////////////
	LPCWSTR lpszToken	= MyApp::FindOneOf(lpCmdLine, szTokens);
	LPCWSTR wsz			= NULL;

	try
	{
		while (lpszToken != NULL)
		{
			if (lstrcmpiW(lpszToken, L"UnregServer")==0)
			{
				////////////////////////////////////////////////////////////////
				// unregister local server
				////////////////////////////////////////////////////////////////
				_Module.UpdateRegistryFromResource(IDR_NonCOMEvent, FALSE);

				#ifdef	__SUPPORT_TYPELIB
				hRes = _Module.UnregisterServer(TRUE);
				#else	! __SUPPORT_TYPELIB
				hRes = _Module.UnregisterServer(FALSE);
				#endif	__SUPPORT_TYPELIB

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				return HRESULT_TO_WIN32 ( hRes );
			}
			if (lstrcmpiW(lpszToken, L"RegServer")==0)
			{
				////////////////////////////////////////////////////////////////
				// register local server
				////////////////////////////////////////////////////////////////
				_Module.UpdateRegistryFromResource(IDR_NonCOMEvent, TRUE);

				#ifdef	__SUPPORT_TYPELIB
				hRes = _Module.RegisterServer(TRUE);
				#else	! __SUPPORT_TYPELIB
				hRes = _Module.RegisterServer(FALSE);
				#endif	__SUPPORT_TYPELIB

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				return HRESULT_TO_WIN32 ( hRes );
			}

			if (lstrcmpiW(lpszToken, L"GUI")==0)
			{
				////////////////////////////////////////////////////////////////
				// previous instance
				////////////////////////////////////////////////////////////////
				if ( _App.Exists() )
				{
					////////////////////////////////////////////////////////////
					// termination
					////////////////////////////////////////////////////////////
					return ERROR_ALREADY_EXISTS;
				}

				////////////////////////////////////////////////////////////////
				// run instance
				////////////////////////////////////////////////////////////////

				hRes = _tWinRun ( nCmdShow, TRUE );

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				return HRESULT_TO_WIN32 ( hRes );
			}

			wsz = MyApp::_cstrstr( CharUpperW ( const_cast < LPWSTR > ( lpszToken ) ), L"CMD_SCALAR");
			if (wsz)
			{
				// move string pointer
				wsz = &wsz [ 11 ];

				////////////////////////////////////////////////////////////////
				// previous instance
				////////////////////////////////////////////////////////////////
				if ( _App.Exists() )
				{
					////////////////////////////////////////////////////////////
					// termination
					////////////////////////////////////////////////////////////
					return ERROR_ALREADY_EXISTS;
				}

				hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
				_ASSERTE(SUCCEEDED(hRes));

				////////////////////////////////////////////////////////////////
				// run command line application
				////////////////////////////////////////////////////////////////

				CComObject < CModuleScalar > * pModule = NULL;
				CComObject < CModuleScalar > :: CreateInstance ( &pModule );

				CComPtr < ICimModule > pCimModule;

				if SUCCEEDED ( hRes = pModule->QueryInterface ( __uuidof ( ICimModule ), ( void** ) & pCimModule ) )
				{
					// 1st argument
					CComVariant var ( wsz );

					if SUCCEEDED ( hRes = pCimModule->Start ( &var, NULL ) )
					{
						// wait till test is done
					}
				}

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				::CoUninitialize ();

				if FAILED ( hRes )
				{
					return HRESULT_TO_WIN32 ( hRes );
				}
			}

			wsz = MyApp::_cstrstr( CharUpperW ( const_cast < LPWSTR > ( lpszToken ) ), L"CMD_ARRAY");
			if (wsz)
			{
				// move string pointer
				wsz = &wsz [ 10 ];

				////////////////////////////////////////////////////////////////
				// previous instance
				////////////////////////////////////////////////////////////////
				if ( _App.Exists() )
				{
					////////////////////////////////////////////////////////////
					// termination
					////////////////////////////////////////////////////////////
					return ERROR_ALREADY_EXISTS;
				}

				hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
				_ASSERTE(SUCCEEDED(hRes));

				////////////////////////////////////////////////////////////////
				// run command line application
				////////////////////////////////////////////////////////////////

				CComObject < CModuleArray > * pModule = NULL;
				CComObject < CModuleArray > :: CreateInstance ( &pModule );

				CComPtr < ICimModule > pCimModule;

				if SUCCEEDED ( hRes = pModule->QueryInterface ( __uuidof ( ICimModule ), ( void** ) & pCimModule ) )
				{
					// 1st argument
					CComVariant var ( wsz );

					if SUCCEEDED ( hRes = pCimModule->Start ( &var, NULL ) )
					{
						// wait till test is done
					}
				}

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				::CoUninitialize ();

				if FAILED ( hRes )
				{
					return HRESULT_TO_WIN32 ( hRes );
				}
			}

			wsz = MyApp::_cstrstr( CharUpperW ( const_cast < LPWSTR > ( lpszToken ) ), L"CMD_GENERIC");
			if (wsz)
			{
				// move string pointer
				wsz = &wsz [ 12 ];

				////////////////////////////////////////////////////////////////
				// previous instance
				////////////////////////////////////////////////////////////////
				if ( _App.Exists() )
				{
					////////////////////////////////////////////////////////////
					// termination
					////////////////////////////////////////////////////////////
					return ERROR_ALREADY_EXISTS;
				}

				hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
				_ASSERTE(SUCCEEDED(hRes));

				////////////////////////////////////////////////////////////////
				// run command line application
				////////////////////////////////////////////////////////////////

				CComObject < CModuleGeneric > * pModule = NULL;
				CComObject < CModuleGeneric > :: CreateInstance ( &pModule );

				CComPtr < ICimModule > pCimModule;

				if SUCCEEDED ( hRes = pModule->QueryInterface ( __uuidof ( ICimModule ), ( void** ) & pCimModule ) )
				{
					// 1st argument
					CComVariant var ( wsz );

					if SUCCEEDED ( hRes = pCimModule->Start ( &var, NULL ) )
					{
						// wait till test is done
					}
				}

				////////////////////////////////////////////////////////////////
				// termination
				////////////////////////////////////////////////////////////////
				_Module.Term();

				::CoUninitialize ();

				if FAILED ( hRes )
				{
					return HRESULT_TO_WIN32 ( hRes );
				}
			}

			lpszToken = MyApp::FindOneOf(lpszToken, szTokens);
		}
	}
	catch ( ... )
	{
		////////////////////////////////////////////////////////////////////////
		// termination
		////////////////////////////////////////////////////////////////////////
		_Module.Term();

		return HRESULT_TO_WIN32 ( E_FAIL );
	}

	////////////////////////////////////////////////////////////////////////
	// previous instance
	////////////////////////////////////////////////////////////////////////
	if ( _App.Exists() )
	{
		////////////////////////////////////////////////////////////////////
		// termination
		////////////////////////////////////////////////////////////////////
		return ERROR_ALREADY_EXISTS;
	}

	////////////////////////////////////////////////////////////////////////
	// run instance
	////////////////////////////////////////////////////////////////////////

	int nRet = _tWinRun ( nCmdShow );

	////////////////////////////////////////////////////////////////////////
	// termination
	////////////////////////////////////////////////////////////////////////
	_Module.Term();

	return HRESULT_TO_WIN32 ( nRet );
}

/////////////////////////////////////////////////////////////////////////////
//
// RUN :))
//
/////////////////////////////////////////////////////////////////////////////

extern "C" HRESULT WINAPI _tWinRun( int, BOOL bGUI )
{
	////////////////////////////////////////////////////////////////////////
	// INITIALIZATION
	////////////////////////////////////////////////////////////////////////

	HRESULT hRes		= S_OK;

	// kill event
	try
	{
		if ( (	_App.m_hKill =
				::CreateEvent ( NULL,
								TRUE,
								FALSE,
								NULL ) 
			 ) == NULL )
		{
			return HRESULT_FROM_WIN32 ( ::GetLastError() );
		}

		if ( !bGUI && (	_App.m_hUse =
						::CreateSemaphore ( NULL,
											1L,
											100L,
											L"NonCOMEvent_StressSemaphore" ) 
					  ) == NULL )
		{
			return HRESULT_FROM_WIN32 ( ::GetLastError() );
		}
	}
	catch ( ... )
	{
		return ERROR_NOT_READY;
	}

	////////////////////////////////////////////////////////////////////////
	// COM INITIALIZATION
	////////////////////////////////////////////////////////////////////////

	if ( bGUI )
	hRes = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	else
	hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	_ASSERTE(SUCCEEDED(hRes));

	////////////////////////////////////////////////////////////////////////
	// This provides a NULL DACL which will allow access to everyone.
	////////////////////////////////////////////////////////////////////////

	hRes = CoInitializeSecurity(NULL,
								-1,
								NULL,
								NULL,
								RPC_C_AUTHN_LEVEL_NONE,
								RPC_C_IMP_LEVEL_IMPERSONATE,
								NULL,
								EOAC_NONE,
								NULL);
	_ASSERTE(SUCCEEDED(hRes));

	hRes = _Module.RegisterClassObjects( CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE );
	_ASSERTE(SUCCEEDED(hRes));

	// called from local server ( unlock will be called )
	if ( ! _Module.MonitorShutdownStart() )
	{
		// revoke class object
		hRes = _Module.RevokeClassObjects();
		_ASSERTE(SUCCEEDED(hRes));

		////////////////////////////////////////////////////////////////
		// COM UNINITIALIZATION
		////////////////////////////////////////////////////////////////
		::CoUninitialize();

		return E_FAIL;
	}

//	////////////////////////////////////////////////////////////////////////
//	message queve is forgiven
//	////////////////////////////////////////////////////////////////////////
//
//	MSG msg;
//	while ( GetMessage( &msg, 0, 0, 0 ) )
//	{
//		DispatchMessage( &msg );
//	}

	// init WBEM
	_App.m_event.ObjectLocator ();

	if ( bGUI )
	{
		////////////////////////////////////////////////////////////////////
		// COMMON CONTROLS
		////////////////////////////////////////////////////////////////////

		#if (_WIN32_IE >= 0x0300)
		INITCOMMONCONTROLSEX iccx;
		iccx.dwSize = sizeof(iccx);
		iccx.dwICC = ICC_BAR_CLASSES;	// change to support other controls
		::InitCommonControlsEx(&iccx);
		#else
		::InitCommonControls();
		#endif

		MyDlg < CNonCOMEventMainDlg> dlg;

		if SUCCEEDED ( dlg.Init() )
		{
			dlg.Run ( SW_SHOWDEFAULT );
		}
	}

	::WaitForSingleObject ( _App.m_hKill, INFINITE );

	////////////////////////////////////////////////////////////////////////
	// do real finishing stuff ( synchronize etc )
	////////////////////////////////////////////////////////////////////////

	// uninit WBEM
	_App.m_event.ObjectLocator ( FALSE );

	// revoke class object
	_Module.RevokeClassObjects();

	////////////////////////////////////////////////////////////////////////
	// COM UNINITIALIZATION
	////////////////////////////////////////////////////////////////////////
	::CoUninitialize();

	return hRes;
}
