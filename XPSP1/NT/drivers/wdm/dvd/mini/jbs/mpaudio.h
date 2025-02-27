/*******************************************************************
*
*				 MPAUDIO.H
*
*				 Copyright (C) 1995 SGS-THOMSON Microelectronics.
*
*
*				 Prototypes for NPAUDIO.C
*
*******************************************************************/

#ifndef __MPAUDIO_H__
#define __MPAUDIO_H__
VOID miniPortAudioGetProperty(PHW_STREAM_REQUEST_BLOCK pSrb);
VOID miniPortAudioSetState(PHW_STREAM_REQUEST_BLOCK pSrb);

ULONG miniPortAudioStop (PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioSetStc(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioReset(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioSetAttribute(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioQueryInfo (PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioPlay(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioPause(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
VOID miniPortAudioPacket(PHW_STREAM_REQUEST_BLOCK pSrb);
ULONG miniPortAudioGetStc(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioGetAttribute(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioEndOfStream(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioDisable(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortAudioEnable(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
ULONG miniPortCancelAudio(PHW_STREAM_REQUEST_BLOCK pMrb, PHW_DEVICE_EXTENSION);
#endif //__MPAUDIO_H__

