/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    rtp.h

Abstract:

    RT DLL private internal functions

Author:

    Erez Haba (erezh) 24-Dec-95

--*/

#ifndef __RTP_H
#define __RTP_H

#include "rtpsec.h"
#include "_mqrpc.h"

#define SEND_PARSE  1
#define RECV_PARSE  2

#define QUEUE_CREATE    1
#define QUEUE_SET_PROPS 2
#define QUEUE_GET_PROPS 3

#define CPP_EXCEPTION_CODE 0xe06d7363

extern DWORD g_dwThreadEventIndex;
extern LPWSTR g_lpwcsComputerName;
extern DWORD g_dwComputerNameLen;
extern LPWSTR  g_lpwcsLocalComputerName;
extern BOOL   g_fDependentClient ;
extern WCHAR g_wszRemoteQMName[] ;
extern DWORD  g_dwPlatformId ;



HRESULT
RTpConvertToMQCode(
    HRESULT hr,
    DWORD dwObjectType =MQDS_QUEUE
    );

//
// The CMQHResult class is used in order to automatically convert the various
// error codes to Falcon error codes. This is done by defining the assignment
// operator of this class so it converts whatever error code that is assigned
// to objects of this class to a Falcon error code. The casting operator
// from this class to HRESULT, returns the converted error code.
//
class CMQHResult
{
public:
    CMQHResult(DWORD =MQDS_QUEUE); // Default constructor.
    CMQHResult(const CMQHResult &); // Copy constructor
    CMQHResult& operator =(HRESULT); // Assignment operator.
    operator HRESULT(); // Casting operator to HRESULT type.
    HRESULT GetReal(); // A method that returns the real error code.

private:
    HRESULT m_hr; // The converted error code.
    HRESULT m_real; // The real error code.
    DWORD m_dwObjectType; // The type of object (can be only queue, or machine).
};

//---------- CMQHResult implementation ----------------------------------

inline CMQHResult::CMQHResult(DWORD dwObjectType)
{
    ASSERT((dwObjectType == MQDS_QUEUE) || (dwObjectType == MQDS_MACHINE));
    m_dwObjectType = dwObjectType;
}

inline CMQHResult::CMQHResult(const CMQHResult &hr)
{
    m_hr = hr.m_hr;
    m_real = hr.m_real;
    m_dwObjectType = hr.m_dwObjectType;
}

inline CMQHResult& CMQHResult::operator =(HRESULT hr)
{
    m_hr = RTpConvertToMQCode(hr, m_dwObjectType);
    m_real = hr;

    return *this;
}

inline CMQHResult::operator HRESULT()
{
    return m_hr;
}

inline HRESULT CMQHResult::GetReal()
{
    return m_real;
}

//---------- CMQHResult implementation end ------------------------------

//---------- Function declarations --------------------------------------

HRESULT
RTpParseMessageProperties(
    LONG                     op,
    CACTransferBufferV2       *ptb,
    ULONG                    cProps,
    PROPID                   aPropID[],
    PROPVARIANT              aPropVar[],
    HRESULT                  aResult[],
    OUT PMQSECURITY_CONTEXT *ppSecCtx,
    OUT LPWSTR*              ppwcsResponseStringToFree,
    OUT LPWSTR*              ppwcsAdminStringToFree
    );

LPWSTR
RTpGetQueuePathNamePropVar(
    MQQUEUEPROPS *pqp
    );

GUID*
RTpGetQueueGuidPropVar(
    MQQUEUEPROPS *pqp
    );

enum QUEUE_PATH_TYPE{
    ILLEGAL_QUEUE_PATH_TYPE = 0,
    PRIVATE_QUEUE_PATH_TYPE,
    PUBLIC_QUEUE_PATH_TYPE,
    SYSTEM_QUEUE_PATH_TYPE
};

QUEUE_PATH_TYPE
RTpValidateAndExpandQueuePath(
    LPCWSTR pwcsPathName,
    LPCWSTR* ppwcsExpandedPathName,
    LPWSTR* ppStringToFree
    );

BOOL
RTpIsLocalPublicQueue(LPCWSTR lpwcsExpandedPathName) ;

BOOL
RTpFormatNameToQueueFormat(
    LPCWSTR lpwcsFormatName,
    QUEUE_FORMAT* pQueueFormat,
    LPWSTR* ppStringToFree
    );

HRESULT
RTpQueueFormatToFormatName(
    QUEUE_FORMAT* pQueueFormat,
    LPWSTR lpwcsFormatName,
    DWORD dwBufferLength,
    LPDWORD lpdwFormatNameLength
    );

HRESULT
RTpMakeSelfRelativeSDAndGetSize(
    PSECURITY_DESCRIPTOR *pSecurityDescriptor,
    PSECURITY_DESCRIPTOR *pSelfRelativeSecurityDescriptor,
    DWORD *pSDSize
    );

HRESULT
RTpCheckColumnsParameter(
    IN MQCOLUMNSET* pColumns
    );

HRESULT
RTpCheckQueueProps(
    IN  MQQUEUEPROPS* pqp,
    IN  DWORD         dwOp,
    IN  BOOL          fPrivateQueue,
    OUT MQQUEUEPROPS **ppGoodQP,
    OUT char **ppTmpBuff
    );

HRESULT
RTpCheckQMProps(
    IN  MQQMPROPS * pQMProps,
    IN OUT HRESULT* aStatus,
    OUT MQQMPROPS **ppGoodQMP,
    OUT char      **ppTmpBuff
    );

HRESULT
RTpCheckRestrictionParameter(
    IN MQRESTRICTION* pRestriction
    );

HRESULT
RTpCheckSortParameter(
    IN MQSORTSET* pSort
    );

HRESULT
RTpCheckLocateNextParameter(
    IN DWORD		cPropsRead,
    IN PROPVARIANT  aPropVar[]
	);

HRESULT
RTpCheckComputerProps(
    IN      MQPRIVATEPROPS * pPrivateProps,
    IN OUT  HRESULT*    aStatus
	);


HRESULT
RTpProvideTransactionEnlist(
    ITransaction *pTrans,
    XACTUOW *pUow
    );

VOID
RTpInitXactRingBuf(
    VOID
    );

HANDLE
GetThreadEvent(
    VOID
    );

HRESULT 
DeppOneTimeInit();


HRESULT
QMSendMessage(
    IN handle_t hBind,
    IN QUEUEHANDLE  hQueue,
    IN struct CACTransferBufferV2 *pCacTB
    );

HRESULT RTpGetSupportServerInfo(BOOL *pfRemote) ;

//
//  cursor information
//
struct CCursorInfo {
    HANDLE hQueue;
    HACCursor32 hCursor;
};
  

//
//  CCursorInfo to cursor handle
//
inline HACCursor32 CI2CH(HANDLE hCursor)
{
    return ((CCursorInfo*)hCursor)->hCursor;
}


//
//  CCursorInfo to queue handle
//
inline HANDLE CI2QH(HANDLE hCursor)
{
    return ((CCursorInfo*)hCursor)->hQueue;
}


//
//  Macro for extracting the explicit rpc binding handle from tls
//
#define UNINIT_TLSINDEX_VALUE   0xffffffff

extern DWORD  g_hBindIndex ;
#define tls_hBindRpc  ((handle_t) TlsGetValue( g_hBindIndex ))

#endif // __RTP_H
