/***********************************************************************
 *  INTEL Corporation Proprietary Information                          *
 *                                                                     *
 *  This listing is supplied under the terms of a license agreement    *
 *  with INTEL Corporation and may not be copied nor disclosed except  *
 *  in accordance with the terms of that agreement.                    *
 *                                                                     *
 *      Copyright (c) 1997 Intel Corporation. All rights reserved.     *
 ***********************************************************************
 *																	   *
 *	$Archive:   S:\sturgeon\src\include\vcs\coder.hpv  $
 *
 *	$Revision:   1.4  $
 *	$Date:   16 Jan 1997 15:25:06  $
 *
 *	$Author:   BPOLING  $
 *
 *	$Log:   S:\sturgeon\src\include\vcs\coder.hpv  $
// 
//    Rev 1.4   16 Jan 1997 15:25:06   BPOLING
// changed copyrights to 1997
// 
//    Rev 1.3   18 Dec 1996 21:49:58   BPOLING
// builds with msdev and for windows only
// 
//    Rev 1.2   09 Dec 1996 14:13:54   EHOWARDX
// Updated copyright notice.
 * 
 *    Rev 1.1   15 Nov 1996 16:16:14   BPOLING
 * vcs header added.
 *                                                                     *
 ***********************************************************************
 *																	   *
 *	coder.hpp														   *
 *																	   * 
 *	PURPOSE:	Encode/Decode ANS.1 Gatekeeper PDU using the OSS ANS.1 *
 *				compiler functions.									   *
 *																	   *
 *	FUNCTIONS:														   *
 *																	   *
 *	COMMENTS: 														   *
 *																	   *
 ***********************************************************************/

/************************************************************************
 * 						Include Files		           				   	*
 ***********************************************************************/

/************************************************************************
 * 						Manifest Constants								*
 ***********************************************************************/
																			
/************************************************************************
*						GLOBAL VARIABLES								*
************************************************************************/

/************************************************************************
 * 						Class Definitions								*
 ***********************************************************************/

#ifndef _CODER_HPP
#define _CODER_HPP

//  DEBUG OPTIONS
#define CODER_DEBUG		  0x00000001
#define CODER_SUPER_DEBUG 0x00000002

// Coder Error Messages

#define CODER_NOERROR 0
#define CODER_ERROR   1

// Protocol version information
#define ITU_T			0
#define RECOMMENDATION	0
#define SERIES			8     // H225.0
#define RECOMM_NUMBER   2250
#define VERSION         0
#define ADDITIONAL      1


typedef struct _percallInfo
{
    unsigned char   bit_mask;
    NonStandardParameter prCllInf_nnStndrdDt;  /* optional */
    CallReferenceValue callReferenceValue;
    ConferenceIdentifier conferenceID;
    ossBoolean      originator;  /* optional */
    struct _seqofP17 {
        struct _seqofP17 *next;
        RTPSession      value;
    } *audio;  /* optional */
    struct _seqofP18 {
        struct _seqofP18 *next;
        RTPSession      value;
    } *video;  /* optional */
    struct _seqofP19 {
        struct _seqofP19 *next;
        TransportChannelInfo value;
    } *data;  /* optional */
    TransportChannelInfo h245;
    TransportChannelInfo callSignaling;
    CallType        callType;
    BandWidth       bandWidth;
    CallModel       callModel;
} PERCALLINFO, *PPERCALLINFO;

typedef struct _seqpercallInfo{
    struct _seqpercallInfo *next;
	struct _percallInfo		value;
} SEQPERCALLINFO, *PSEQPERCALLINFO;

typedef struct _seqtransadds{
    struct _seqtransadds  	*next;
    TransportAddress 		value;
} SEQTRANSADDS, *PSEQTRANSADDS;

typedef struct _seqaliasadds {
    struct _seqaliasadds  	*next;
    AliasAddress    		value;
} SEQALIASADDS, *PSEQALIASADDS;

class Coder{

private:
	OssGlobal 			m_World;
	CRITICAL_SECTION	m_critSec;

public:
	struct ObjectID_ m_protocolIdentifier1,m_protocolIdentifier2,m_protocolIdentifier3,
		             m_protocolIdentifier4,m_protocolIdentifier5,m_protocolIdentifier6;

public:
	Coder();
	~Coder();

#ifdef _CONSOLE
	// Print a H323 structure
	int Print(RasMessage *pData);
	// Print an encoded H323 structure
	void Print(OssBuf *pOssBuf);
#else
	int InitCoder();
#endif
	// Creates an OssBuf
	int Encode(RasMessage *pInputData, OssBuf *pOutputOssBuf);
	// Create memory to hold decoded OssBuf
	// For H323 this is a rasStruct
	int Decode(OssBuf *pInputOssBuf, RasMessage **pOutputData);
	// Used to free buffer created by decode
	int Free(RasMessage *pData);
	// Used to free buffer created by encode
	void Free(OssBuf ossBuf);
	int Copy(RasMessage **pDestination,RasMessage *pSource);
	// Compare rasStruct
	// return of 1 means structures are not the same
	// return of 0 means structures are the same
	int Compare(RasMessage *pOriginal, RasMessage *pCopied);
	// Returns the sequence number out of any RasMessage
	// Returns zero on error
	RequestSeqNum GetSequenceNumber(RasMessage *prasStruct);
	RequestSeqNum SetSequenceNumber(RasMessage &rasStruct,RequestSeqNum reqNum);
	// Returns a pointer EndpointIdentifier for any RasMessage
	// NULL for RasMessage that have no EndpointIdentifier
	EndpointIdentifier *GetEndpointID(RasMessage *prasStruct);
	// Returns a pointer to a valid H323 protocolIdentifier linked list 
	// when a valid Rasmessage is passed in and sets the protocol Identifier
	// item in RasMessages that have an protocol Identifier
	// If in an RasMessage that doesn't have a protocol identifer is passed in,
	// NULL is returned.
	ProtocolIdentifier SetProtocolIdentifier(RasMessage &rasStruct);
	// Returns TRUE if protocols match, FALSE - otherwise
	BOOL VerifyProtocolIdentifier(RasMessage &rasStruct);
	// finds the requested protocol rasAddress and copies it
	DWORD CopyRasAddress(TransportAddress *pDestAddress, RasMessage *prasStruct, unsigned short choice);
	DWORD CopyRasAddress(TransportAddress *pDestAddress, PSEQTRANSADDS pSrcSeqRasAddress, unsigned short choice);
	// finds the requested protocol callSignalAddress and copies it
	DWORD CopyCallSignal(TransportAddress *pCallSignalAddress, RasMessage *prasStruct, unsigned short choice);
	DWORD CopyCallSignal(TransportAddress *pDestCallSignalAddress, PSEQTRANSADDS pSrcSeqCSAAddress, unsigned short choice);
};

#endif
