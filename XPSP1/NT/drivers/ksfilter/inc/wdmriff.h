
/*	-	-	-	-	-	-	-	-	*/
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (C) Microsoft Corporation, 1995 - 1996  All Rights Reserved.
//
/*	-	-	-	-	-	-	-	-	*/

#define	STATIC_AVRIFF	0x0F9FFCA0L, 0x912E, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
DEFINE_GUIDEX(AVRIFF);

#define	STATIC_AVLIST	0x2064C880L, 0x912F, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
DEFINE_GUIDEX(AVLIST);

typedef struct {
	GUID		guidChunk;
#if defined(_NTDDK_)
	ULONGLONG	cbLength;
#else
	DWORDLONG	cbLength;
#endif
} AVRIFFCHUNK, *PAVRIFFCHUNK;

typedef struct {
	AVRIFFCHUNK	RiffChunk;
	GUID		guidForm;
} AVRIFFFORM, *PAVRIFFFORM

/*	-	-	-	-	-	-	-	-	*/
