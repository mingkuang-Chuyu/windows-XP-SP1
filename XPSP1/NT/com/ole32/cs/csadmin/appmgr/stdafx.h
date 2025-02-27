// This is a part of the Microsoft Management Console.
// Copyright (C) 1995-1996 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Management Console and related
// electronic documentation provided with the interfaces.

// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#include <afxwin.h>
#include <afxdisp.h>

#include <atlbase.h>

//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>

#pragma comment(lib, "mmc")
#include <mmc.h>
#include "afxtempl.h"

const long UNINITIALIZED = -1;

// Sample folder types
enum FOLDER_TYPES
{
    STATIC = 0x8000,
};

/////////////////////////////////////////////////////////////////////////////
// Helper functions

template<class TYPE>
inline void SAFE_RELEASE(TYPE*& pObj)
{
    if (pObj != NULL)
    {
        pObj->Release();
        pObj = NULL;
    }
    else
    {
        TRACE(_T("Release called on NULL interface ptr\n"));
    }
}

extern const CLSID CLSID_Snapin;    // In-Proc server GUID
extern const GUID cNodeType;        // Main NodeType GUID on numeric format
extern const wchar_t*  cszNodeType; // Main NodeType GUID on string format

// New Clipboard format that has the Type and Cookie
extern const wchar_t* SNAPIN_INTERNAL;

struct INTERNAL
{
    INTERNAL() { m_type = CCT_UNINITIALIZED; m_cookie = -1;};
    ~INTERNAL() {}

    DATA_OBJECT_TYPES   m_type;     // What context is the data object.
    long                m_cookie;   // What object the cookie represents
    CString             m_string;

    INTERNAL & operator=(const INTERNAL& rhs)
    {
        if (&rhs == this)
            return *this;

        m_type = rhs.m_type;
        m_cookie = rhs.m_cookie;
        m_string = rhs.m_string;

        return *this;
    }

    BOOL operator==(const INTERNAL& rhs)
    {
        return rhs.m_string == m_string;
    }
};


// Debug instance counter
#ifdef _DEBUG

inline void DbgInstanceRemaining(char * pszClassName, int cInstRem)
{
    char buf[100];
    wsprintfA(buf, "%s has %d instances left over.", pszClassName, cInstRem);
    ::MessageBoxA(NULL, buf, "Memory Leak!!!", MB_OK);
}
    #define DEBUG_DECLARE_INSTANCE_COUNTER(cls)      extern int s_cInst_##cls = 0;
    #define DEBUG_INCREMENT_INSTANCE_COUNTER(cls)    ++(s_cInst_##cls);
    #define DEBUG_DECREMENT_INSTANCE_COUNTER(cls)    --(s_cInst_##cls);
    #define DEBUG_VERIFY_INSTANCE_COUNT(cls)    \
        extern int s_cInst_##cls; \
        if (s_cInst_##cls) DbgInstanceRemaining(#cls, s_cInst_##cls);
#else
    #define DEBUG_DECLARE_INSTANCE_COUNTER(cls)
    #define DEBUG_INCREMENT_INSTANCE_COUNTER(cls)
    #define DEBUG_DECREMENT_INSTANCE_COUNTER(cls)
    #define DEBUG_VERIFY_INSTANCE_COUNT(cls)
#endif
