//+-----------------------------------------------------------------------
//
//  Microsoft
//  Copyright (C) Microsoft Corporation, 1999
//
//  File:      src\time\src\externuuids.cpp
//
//  Contents:  UUID definitions for music center
//
//------------------------------------------------------------------------
#include "headers.h"
#include <initguid.h>
#include "mixerocx.h"
#include <ExternUUIDs.h>
#include <inc\qnetwork.h>

//
// originally in \mm\inc\manager.h
//
const CLSID    CLSID_MCMANAGER          = {0xcd103bcf,0x4846,0x11d3,{0xa2,0x0a,0x00,0xc0,0x4f,0xa3,0xb6,0x0c}};
const IID      IID_IMCManager           = {0x901B7025,0x4846,0x11d3,{0xa2,0x0a,0x00,0xc0,0x4f,0xa3,0xb6,0x0c}};

//
// originally in \mm\inc\dplayer.h
//
const CLSID    CLSID_DLXPLAY            = {0xcf94dff3,0x38ea,0x4343,{0x96,0x3e,0x41,0x0d,0xb6,0x0d,0xd9,0xb8}};
const IID      IID_IDLXPLAY             = {0x89301af7,0xeb8d,0x41f8,{0xbb,0x3d,0x6b,0xc2,0x25,0xda,0x31,0xc2}};
const IID      IID_IDLXPLAYEVENTSINK    = {0x89301af8,0xeb8d,0x41f8,{0xbb,0x3d,0x6b,0xc2,0x25,0xda,0x31,0xc2}};

//
// originally in \mm\shplay\shplay.h
//
const IID      IID_IMCPList             = {0xEBC54B0C,0x4091,0x11D3,{0xA2,0x08,0x00,0xC0,0x4F,0xA3,0xB6,0x0C}};

#include "mixerocx_i.c"
