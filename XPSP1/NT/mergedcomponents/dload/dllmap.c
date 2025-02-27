//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1997.
//
//  File:       P R O C M A P . C
//
//  Contents:   Procedure maps for dload.c
//
//  Notes:
//
//  Author:     shaunco   19 May 1998
//
//----------------------------------------------------------------------------

#include "pch.h"
#pragma hdrstop

//
// All of the dll's that kernel32.dll supports delay-load failure handlers for
// (both by procedure and by ordinal) need both a DECLARE_XXXXXX_MAP below and
// a DLDENTRYX entry in the g_DllEntries list.
//

// alphabetical order (hint hint)
DECLARE_ORDINAL_MAP(activeds)
DECLARE_PROCNAME_MAP(advapi32)
DECLARE_PROCNAME_MAP(advpack)
DECLARE_PROCNAME_MAP(apphelp)
DECLARE_PROCNAME_MAP(authz)
DECLARE_ORDINAL_MAP(browseui)
DECLARE_ORDINAL_MAP(cabinet)
DECLARE_PROCNAME_MAP(cdfview)
DECLARE_ORDINAL_MAP(certcli)
DECLARE_PROCNAME_MAP(certcli)
DECLARE_ORDINAL_MAP(comctl32)
DECLARE_PROCNAME_MAP(comctl32)
DECLARE_PROCNAME_MAP(comdlg32)
DECLARE_PROCNAME_MAP(credui)
DECLARE_PROCNAME_MAP(crypt32)
DECLARE_PROCNAME_MAP(cryptui)
DECLARE_ORDINAL_MAP(cscdll)
DECLARE_PROCNAME_MAP(ddraw)
DECLARE_ORDINAL_MAP(devmgr)
DECLARE_PROCNAME_MAP(dhcpcsvc)
DECLARE_PROCNAME_MAP(dnsapi)
DECLARE_PROCNAME_MAP(duser)
DECLARE_PROCNAME_MAP(efsadu)
DECLARE_PROCNAME_MAP(esent)
DECLARE_PROCNAME_MAP(gdi32)
DECLARE_PROCNAME_MAP(gdiplus)
DECLARE_PROCNAME_MAP(imagehlp)
DECLARE_PROCNAME_MAP(imgutil)
DECLARE_PROCNAME_MAP(imm32)
DECLARE_PROCNAME_MAP(inetcomm)
DECLARE_PROCNAME_MAP(iphlpapi)
DECLARE_PROCNAME_MAP(kdcsvc)
DECLARE_PROCNAME_MAP(keymgr)
DECLARE_PROCNAME_MAP(linkinfo)
DECLARE_PROCNAME_MAP(lz32)
DECLARE_ORDINAL_MAP(mlang)
DECLARE_PROCNAME_MAP(mobsync)
DECLARE_PROCNAME_MAP(mpr)
DECLARE_PROCNAME_MAP(mprapi)
DECLARE_PROCNAME_MAP(mprui)
DECLARE_PROCNAME_MAP(msacm32)
DECLARE_PROCNAME_MAP(mscat32)
DECLARE_ORDINAL_MAP(msgina)
DECLARE_PROCNAME_MAP(mshtml)
DECLARE_ORDINAL_MAP(msi)
DECLARE_PROCNAME_MAP(msimg32)
DECLARE_PROCNAME_MAP(msrating)
DECLARE_PROCNAME_MAP(mssign32)
DECLARE_PROCNAME_MAP(mswsock)
DECLARE_PROCNAME_MAP(netapi32)
DECLARE_PROCNAME_MAP(netcfgx)
DECLARE_PROCNAME_MAP(netplwiz)
DECLARE_PROCNAME_MAP(netrap)
DECLARE_PROCNAME_MAP(netshell)
DECLARE_PROCNAME_MAP(ntdsa)
DECLARE_PROCNAME_MAP(ntdsapi)
DECLARE_PROCNAME_MAP(ntlanman)
DECLARE_PROCNAME_MAP(ntlsapi)
DECLARE_PROCNAME_MAP(ntmarta)
DECLARE_PROCNAME_MAP(ocmanage)
DECLARE_ORDINAL_MAP(odbc32)
DECLARE_PROCNAME_MAP(ole32)
DECLARE_PROCNAME_MAP(oleacc)
DECLARE_ORDINAL_MAP(oleaut32)
DECLARE_PROCNAME_MAP(pautoenr)
DECLARE_ORDINAL_MAP(pidgen)
DECLARE_PROCNAME_MAP(powrprof)
DECLARE_PROCNAME_MAP(printui)
DECLARE_PROCNAME_MAP(pstorec)
DECLARE_PROCNAME_MAP(query)
DECLARE_PROCNAME_MAP(rasapi32)
DECLARE_PROCNAME_MAP(rasdlg)
DECLARE_PROCNAME_MAP(rasman)
DECLARE_PROCNAME_MAP(regapi)
DECLARE_PROCNAME_MAP(rpcrt4)
DECLARE_PROCNAME_MAP(rtutils)
DECLARE_PROCNAME_MAP(samlib)
DECLARE_PROCNAME_MAP(scecli)
DECLARE_PROCNAME_MAP(secur32)
DECLARE_PROCNAME_MAP(setupapi)
DECLARE_ORDINAL_MAP(sfc)
DECLARE_PROCNAME_MAP(sfc)
DECLARE_ORDINAL_MAP(shdocvw)
DECLARE_PROCNAME_MAP(shdocvw)
DECLARE_ORDINAL_MAP(shell32)
DECLARE_PROCNAME_MAP(shell32)
DECLARE_ORDINAL_MAP(shlwapi)
DECLARE_PROCNAME_MAP(shlwapi)
DECLARE_ORDINAL_MAP(shsvcs)
DECLARE_PROCNAME_MAP(sti)
DECLARE_PROCNAME_MAP(syssetup)
DECLARE_PROCNAME_MAP(urlmon)
DECLARE_PROCNAME_MAP(user32)
DECLARE_ORDINAL_MAP(userenv)
DECLARE_PROCNAME_MAP(userenv)
DECLARE_PROCNAME_MAP(utildll)
DECLARE_ORDINAL_MAP(uxtheme)
DECLARE_PROCNAME_MAP(uxtheme)
DECLARE_PROCNAME_MAP(version)
DECLARE_ORDINAL_MAP(wininet)
DECLARE_PROCNAME_MAP(wininet)
DECLARE_PROCNAME_MAP(winmm)
DECLARE_PROCNAME_MAP(winscard)
DECLARE_ORDINAL_MAP(winspool)
DECLARE_PROCNAME_MAP(winspool)
DECLARE_PROCNAME_MAP(winsta)
DECLARE_PROCNAME_MAP(wintrust)
DECLARE_ORDINAL_MAP(wldap32)
DECLARE_PROCNAME_MAP(wmi)
DECLARE_PROCNAME_MAP(wmvcore)
DECLARE_ORDINAL_MAP(ws2_32)
DECLARE_PROCNAME_MAP(ws2_32)
DECLARE_PROCNAME_MAP(wtsapi32)
DECLARE_PROCNAME_MAP(wzcdlg)
DECLARE_ORDINAL_MAP(wzcsapi)
DECLARE_PROCNAME_MAP(wzcsapi)

const DLOAD_DLL_ENTRY g_DllEntries [] =
{
    // alphabetical order (hint hint)
    DLDENTRYO(activeds)
    DLDENTRYP(advapi32)
    DLDENTRYP(advpack)
    DLDENTRYP(apphelp)
    DLDENTRYP(authz)
    DLDENTRYO(browseui)
    DLDENTRYO(cabinet)
    DLDENTRYP(cdfview)
    DLDENTRYB(certcli)
    DLDENTRYB(comctl32)
    DLDENTRYP(comdlg32)
    DLDENTRYP(credui)
    DLDENTRYP(crypt32)
    DLDENTRYP(cryptui)
    DLDENTRYO(cscdll)
    DLDENTRYP(ddraw)
    DLDENTRYO(devmgr)
    DLDENTRYP(dhcpcsvc)
    DLDENTRYP(dnsapi)
    DLDENTRYP(duser)
    DLDENTRYP(efsadu)
    DLDENTRYP(esent)
    DLDENTRYP(gdi32)
    DLDENTRYP(gdiplus)
    DLDENTRYP(imagehlp)
    DLDENTRYP(imgutil)
    DLDENTRYP(imm32)
    DLDENTRYP(inetcomm)
    DLDENTRYP(iphlpapi)
    DLDENTRYP(kdcsvc)
    DLDENTRYP(keymgr)
    DLDENTRYP(linkinfo)
    DLDENTRYP(lz32)
    DLDENTRYO(mlang)
    DLDENTRYP(mobsync)
    DLDENTRYP(mpr)
    DLDENTRYP(mprapi)
    DLDENTRYP(mprui)
    DLDENTRYP(msacm32)
    DLDENTRYP(mscat32)
    DLDENTRYO(msgina)
    DLDENTRYP(mshtml)
    DLDENTRYO(msi)
    DLDENTRYP(msimg32)
    DLDENTRYP(msrating)
    DLDENTRYP(mssign32)
    DLDENTRYP(mswsock)
    DLDENTRYP(netapi32)
    DLDENTRYP(netcfgx)
    DLDENTRYP(netplwiz)
    DLDENTRYP(netrap)
    DLDENTRYP(netshell)
    DLDENTRYP(ntdsa)
    DLDENTRYP(ntdsapi)
    DLDENTRYP(ntlanman)
    DLDENTRYP(ntlsapi)
    DLDENTRYP(ntmarta)
    DLDENTRYP(ocmanage)
    DLDENTRYO(odbc32)
    DLDENTRYP(ole32)
    DLDENTRYP(oleacc)
    DLDENTRYO(oleaut32)
    DLDENTRYP(pautoenr)
    DLDENTRYO(pidgen)
    DLDENTRYP(powrprof)
    DLDENTRYP(printui)
    DLDENTRYP(pstorec)
    DLDENTRYP(query)
    DLDENTRYP(rasapi32)
    DLDENTRYP(rasdlg)
    DLDENTRYP(rasman)
    DLDENTRYP(regapi)
    DLDENTRYP(rpcrt4)
    DLDENTRYP(rtutils)
    DLDENTRYP(samlib)
    DLDENTRYP(scecli)
    DLDENTRYP(secur32)
    DLDENTRYP(setupapi)
    DLDENTRYB(sfc)
    DLDENTRYB(shdocvw)
    DLDENTRYB(shell32)
    DLDENTRYB(shlwapi)
    DLDENTRYO(shsvcs)
    DLDENTRYP(sti)
    DLDENTRYP(syssetup)
    DLDENTRYP(urlmon)
    DLDENTRYP(user32)
    DLDENTRYB(userenv)
    DLDENTRYP(utildll)
    DLDENTRYB(uxtheme)
    DLDENTRYP(version)
    DLDENTRYB(wininet)
    DLDENTRYP(winmm)
    DLDENTRYP(winscard)
    DLDENTRYB_DRV(winspool)
    DLDENTRYP(winsta)
    DLDENTRYP(wintrust)
    DLDENTRYO(wldap32)
    DLDENTRYP(wmi)
    DLDENTRYP(wmvcore)
    DLDENTRYB(ws2_32)
    DLDENTRYP(wtsapi32)
    DLDENTRYP(wzcdlg)
    DLDENTRYB(wzcsapi)
};

const DLOAD_DLL_MAP g_DllMap =
{
    celems(g_DllEntries),
    g_DllEntries
};

