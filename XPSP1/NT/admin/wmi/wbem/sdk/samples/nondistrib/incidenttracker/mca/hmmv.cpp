// Machine generated IDispatch wrapper class(es) created by Microsoft Visual C++

//

// Copyright (c) 1997-2001 Microsoft Corporation, All Rights Reserved
//
// NOTE: Do not modify the contents of this file.  If this class is regenerated by
//  Microsoft Visual C++, your modifications will be overwritten.


#include "stdafx.h"
#include "hmmv.h"

/////////////////////////////////////////////////////////////////////////////
// CHmmv

IMPLEMENT_DYNCREATE(CHmmv, CWnd)

/////////////////////////////////////////////////////////////////////////////
// CHmmv properties

VARIANT CHmmv::GetObjectPath()
{
	VARIANT result;
	GetProperty(0x2, VT_VARIANT, (void*)&result);
	return result;
}

void CHmmv::SetObjectPath(const VARIANT& propVal)
{
	SetProperty(0x2, VT_VARIANT, &propVal);
}

SCODE CHmmv::GetSc()
{
	SCODE result;
	GetProperty(0x1, VT_ERROR, (void*)&result);
	return result;
}

void CHmmv::SetSc(SCODE propVal)
{
	SetProperty(0x1, VT_ERROR, propVal);
}

CString CHmmv::GetNameSpace()
{
	CString result;
	GetProperty(0x3, VT_BSTR, (void*)&result);
	return result;
}

void CHmmv::SetNameSpace(LPCTSTR propVal)
{
	SetProperty(0x3, VT_BSTR, propVal);
}

long CHmmv::GetReadyState()
{
	long result;
	GetProperty(DISPID_READYSTATE, VT_I4, (void*)&result);
	return result;
}

/////////////////////////////////////////////////////////////////////////////
// CHmmv operations

void CHmmv::ShowInstances(LPCTSTR szTitle, const VARIANT& varPathArray)
{
	static BYTE parms[] =
		VTS_BSTR VTS_VARIANT;
	InvokeHelper(0x5, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 szTitle, &varPathArray);
}

void CHmmv::QueryViewInstances(LPCTSTR pLabel, LPCTSTR pQueryType, LPCTSTR pQuery, LPCTSTR pClass)
{
	static BYTE parms[] =
		VTS_BSTR VTS_BSTR VTS_BSTR VTS_BSTR;
	InvokeHelper(0x7, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 pLabel, pQueryType, pQuery, pClass);
}
