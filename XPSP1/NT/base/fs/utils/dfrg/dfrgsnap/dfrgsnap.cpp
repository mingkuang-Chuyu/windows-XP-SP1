// DfrgSnap.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//		To build a separate proxy/stub DLL,
//		run nmake -f DfrgSnapps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#include "initguid.h"
#include "DfrgSnap.h"

#include "DfrgSnap_i.c"
#include "DfrgSnapin.h"

#include "DfrgUI_i.c"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
	OBJECT_ENTRY(CLSID_DfrgSnapin, CDfrgSnapin)
	OBJECT_ENTRY(CLSID_DfrgSnapinAbout, CDfrgSnapinAbout)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
        SHFusionInitialize(NULL);
		_Module.Init(ObjectMap, hInstance);
		CSnapInItem::Init();
		DisableThreadLibraryCalls(hInstance);
	}
    else if (dwReason == DLL_PROCESS_DETACH) {
		_Module.Term();
        SHFusionUninitialize();
    }
	return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
	return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
	// registers object, typelib and all interfaces in typelib
	return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
	_Module.UnregisterServer();
	return S_OK;
}


