// Machine generated IDispatch wrapper class(es) created by Microsoft Visual C++

// NOTE: Do not modify the contents of this file.  If this class is regenerated by
//  Microsoft Visual C++, your modifications will be overwritten.


#include "stdafx.h"
#include "security.h"
#include "nsentry.h"
#include "NSETest.h"
#include "NSETestDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CNSEntry

IMPLEMENT_DYNCREATE(CNSEntry, CWnd)

/////////////////////////////////////////////////////////////////////////////
// CNSEntry properties

BEGIN_EVENTSINK_MAP(CNSEntry, CWnd)
    //{{AFX_EVENTSINK_MAP(CNSEntry)
	//}}AFX_EVENTSINK_MAP
	ON_EVENT_REFLECT(CNSEntry,3,OnGetIWbemServices,VTS_BSTR VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT)
END_EVENTSINK_MAP()

CString CNSEntry::GetNameSpace()
{
	CString result;
	GetProperty(0x1, VT_BSTR, (void*)&result);
	return result;
}

void CNSEntry::SetNameSpace(LPCTSTR propVal)
{
	SetProperty(0x1, VT_BSTR, propVal);
}

/////////////////////////////////////////////////////////////////////////////
// CNSEntry operations

long CNSEntry::OpenNamespace(LPCTSTR bstrNamespace, long longNoFireEvent)
{
	long result;
	static BYTE parms[] =
		VTS_BSTR VTS_I4;
	InvokeHelper(0x2, DISPATCH_METHOD, VT_I4, (void*)&result, parms,
		bstrNamespace, longNoFireEvent);
	return result;
}

void CNSEntry::SetNamespaceText(LPCTSTR lpctstrNamespace)
{
	static BYTE parms[] =
		VTS_BSTR;
	InvokeHelper(0x3, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 lpctstrNamespace);
}

CString CNSEntry::GetNamespaceText()
{
	CString result;
	InvokeHelper(0x4, DISPATCH_METHOD, VT_BSTR, (void*)&result, NULL);
	return result;
}

long CNSEntry::IsTextValid()
{
	long result;
	InvokeHelper(0x5, DISPATCH_METHOD, VT_I4, (void*)&result, NULL);
	return result;
}

void CNSEntry::ClearOnLoseFocus(long lClearOnLoseFocus)
{
	static BYTE parms[] =
		VTS_I4;
	InvokeHelper(0x6, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 lClearOnLoseFocus);
}

void CNSEntry::ClearNamespaceText(LPCTSTR lpctstrNamespace)
{
	static BYTE parms[] =
		VTS_BSTR;
	InvokeHelper(0x7, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 lpctstrNamespace);
}

void CNSEntry::AboutBox()
{
	InvokeHelper(0xfffffdd8, DISPATCH_METHOD, VT_EMPTY, NULL, NULL);
}

void CNSEntry::OnGetIWbemServices
(LPCTSTR lpctstrNamespace, VARIANT FAR* pvarUpdatePointer, VARIANT FAR* pvarServices, VARIANT FAR* pvarSC, VARIANT FAR* pvarUserCancel) 
{
	m_pcsecurityLoginControl -> GetIWbemServices
		(lpctstrNamespace, 
		pvarUpdatePointer, 
		pvarServices, 
		pvarSC, 
		pvarUserCancel);
}