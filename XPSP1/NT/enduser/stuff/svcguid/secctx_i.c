
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 6.00.0344 */
/* Compiler settings for secctx.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#if !defined(_M_IA64) && !defined(_M_AXP64)

#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ISecurityCallCtxPrivate,0x29012F73,0x8C30,0x11d1,0x8E,0x04,0x00,0x80,0xC7,0xF8,0xC2,0xBF);


MIDL_DEFINE_GUID(IID, IID_ISecurityCertificateColl,0xCAFC823B,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityIdentityColl,0xCAFC823C,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityCallersColl,0xCAFC823D,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityCallContext,0xCAFC823E,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_IGetSecurityCallContext,0xCAFC823F,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



#endif /* !defined(_M_IA64) && !defined(_M_AXP64)*/


#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the IIDs and CLSIDs */

/* link this file in with the server and any clients */


 /* File created by MIDL compiler version 6.00.0344 */
/* Compiler settings for secctx.idl:
    Oicf, W1, Zp8, env=Win64 (32b run,appending)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#if defined(_M_IA64) || defined(_M_AXP64)

#ifdef __cplusplus
extern "C"{
#endif 


#include <rpc.h>
#include <rpcndr.h>

#ifdef _MIDL_USE_GUIDDEF_

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        DEFINE_GUID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

#else // !_MIDL_USE_GUIDDEF_

#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}

#endif !_MIDL_USE_GUIDDEF_

MIDL_DEFINE_GUID(IID, IID_ISecurityCallCtxPrivate,0x29012F73,0x8C30,0x11d1,0x8E,0x04,0x00,0x80,0xC7,0xF8,0xC2,0xBF);


MIDL_DEFINE_GUID(IID, IID_ISecurityCertificateColl,0xCAFC823B,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityIdentityColl,0xCAFC823C,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityCallersColl,0xCAFC823D,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_ISecurityCallContext,0xCAFC823E,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);


MIDL_DEFINE_GUID(IID, IID_IGetSecurityCallContext,0xCAFC823F,0xB441,0x11d1,0xB8,0x2B,0x00,0x00,0xF8,0x75,0x7E,0x2A);

#undef MIDL_DEFINE_GUID

#ifdef __cplusplus
}
#endif



#endif /* defined(_M_IA64) || defined(_M_AXP64)*/

