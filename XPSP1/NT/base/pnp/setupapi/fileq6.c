/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    fileq6.c

Abstract:

    Copy list scanning functions.

Author:

    Ted Miller (tedm) 24-Feb-1995

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

//
// Define mask that isolates the action to be performed on the file queue.
//
#define SPQ_ACTION_MASK (SPQ_SCAN_FILE_PRESENCE | SPQ_SCAN_FILE_VALIDITY | SPQ_SCAN_USE_CALLBACK | SPQ_SCAN_USE_CALLBACKEX | SPQ_SCAN_USE_CALLBACK_SIGNERINFO)
#define SPQ_SCAN_USE_CALLBACKEX_PRESENCE (SPQ_SCAN_FILE_PRESENCE|SPQ_SCAN_USE_CALLBACKEX)

BOOL
_SetupMarkFileNodeTargetFlags(
    IN  PSP_FILE_QUEUE      FileQueue,
    IN  LONG                RootID,
    IN  LONG                DirID,
    IN  LONG                FileID,
    IN  DWORD               MaskFlags,
    IN  DWORD               Flags,
    OUT PDWORD              PriorFlags
    )
{
    SP_TARGET_ENT TargetInfo;
    LONG TargetID;
    DWORD rc;
    DWORD OldFlags;

    MYASSERT(Flags == (MaskFlags&Flags));

    rc = pSetupBackupGetTargetByPath((HSPFILEQ)FileQueue,
                                     NULL, // use Queue's string table
                                     NULL,
                                     RootID,
                                     DirID,
                                     FileID,
                                     &TargetID,
                                     &TargetInfo
                                    );

    if (rc != NO_ERROR) {
        SetLastError(rc);
        return FALSE;
    }
    OldFlags = TargetInfo.InternalFlags;
    TargetInfo.InternalFlags = (TargetInfo.InternalFlags&~MaskFlags)|Flags;

    if(OldFlags != TargetInfo.InternalFlags) {
        rc = pSetupBackupSetTargetByID((HSPFILEQ)FileQueue,
                                       TargetID,
                                       &TargetInfo
                                       );
    }
    if(PriorFlags) {
        *PriorFlags = OldFlags;
    }

    if (rc != NO_ERROR) {
        SetLastError(rc);
        return FALSE;
    }
    return TRUE;
}

BOOL
_SetupScanFileQueue(
    IN  HSPFILEQ FileQueue,
    IN  DWORD    Flags,
    IN  HWND     Window,            OPTIONAL
    IN  PVOID    CallbackRoutine,   OPTIONAL
    IN  PVOID    CallbackContext,   OPTIONAL
    OUT PDWORD   Result,
    IN  BOOL     IsNativeCharWidth
    )

/*++

Routine Description:

    Implementation for SetupScanFileQueue, handles ANSI and Unicode
    callback functions.

Arguments:

    Same as SetupScanFileQueue().

    IsNativeCharWidth - supplies flag indicating whether callback routine is
        expecting unicode params. Meaningful only in UNICODE version of DLL.

Return Value:

    Same as SetupScanFileQueue().

--*/

{
    DWORD Action;
    PSP_FILE_QUEUE Queue;
    PSP_FILE_QUEUE_NODE QueueNode, TempNode, NextNode;
    PSP_FILE_QUEUE_NODE CheckNode;
    PSOURCE_MEDIA_INFO SourceMedia;
    BOOL Continue;
    TCHAR TargetPath[MAX_PATH];
    BOOL Err;
    int i;
    PTSTR Message;
    DWORD flags;
    DWORD Win32Error;
    SetupapiVerifyProblem Problem;
    TCHAR TempCharBuffer[MAX_PATH];
    TCHAR SourcePath[MAX_PATH];
    TCHAR DigitalSigner[MAX_PATH];
    TCHAR SignerVersion[MAX_PATH];
    TCHAR CatalogFile[MAX_PATH];
    DWORD rc;
    UINT Notification;
    UINT_PTR CallbackParam1;
    FILEPATHS FilePaths;
    FILEPATHS_SIGNERINFO FilePathsSignerInfo;
    BOOL DoPruning, PruneNode;
    PSPQ_CATALOG_INFO CatalogNode;
    PSETUP_LOG_CONTEXT lc = NULL;
    DWORD slot_fileop = 0;

    Queue = (PSP_FILE_QUEUE)FileQueue;

    rc = NO_ERROR;
    try {
        if (Queue->Signature != SP_FILE_QUEUE_SIG) {
            rc = ERROR_INVALID_HANDLE;
        }
    } except(EXCEPTION_EXECUTE_HANDLER) {
       rc = ERROR_INVALID_HANDLE;
    }

    if(rc != NO_ERROR) {
        SetLastError(rc);
        return(FALSE);
    }

    lc = Queue->LogContext;

    //
    // Validate arguments. Exactly one action flag must be specified.
    //
    if(Result) {
        *Result = 0;
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }
    Action = (Flags & SPQ_ACTION_MASK);
    switch(Action) {
        case SPQ_SCAN_FILE_PRESENCE:
        case SPQ_SCAN_FILE_VALIDITY:
            break;
        case SPQ_SCAN_USE_CALLBACK:
        case SPQ_SCAN_USE_CALLBACKEX:
        case SPQ_SCAN_USE_CALLBACKEX_PRESENCE:
        case SPQ_SCAN_USE_CALLBACK_SIGNERINFO:
            if(CallbackRoutine) {
                break;
            }
            // else fall through to invalid arg case
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return(FALSE);
    }

    DoPruning = Flags & SPQ_SCAN_PRUNE_COPY_QUEUE;
    if(DoPruning) {
        if(Queue->Flags & FQF_QUEUE_ALREADY_COMMITTED) {
            //
            // If we've been asked to prune the copy queue, then make sure the queue
            // hasn't been committed yet.
            //
            SetLastError(ERROR_NO_MORE_ITEMS);
            return FALSE;
        }

        if((Action == SPQ_SCAN_USE_CALLBACK)
           || (Action == SPQ_SCAN_USE_CALLBACKEX)
           || (Action == SPQ_SCAN_USE_CALLBACKEX_PRESENCE)
           || (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)
           || (Flags & SPQ_SCAN_INFORM_USER)) {
            //
            // Presently, pruning the queue is not supported when using a callback.
            // Also, SPQ_SCAN_INFORM_USER and SPQ_SCAN_PRUNE_COPY_QUEUE don't play well
            // together...
            //
            SetLastError(ERROR_INVALID_FLAGS);
            return FALSE;
        }
    }

    //
    // If the caller asked for UI, make sure we're running interactively.
    //
    if((Flags & SPQ_SCAN_INFORM_USER) && (GlobalSetupFlags & (PSPGF_NONINTERACTIVE|PSPGF_UNATTENDED_SETUP))) {
        SetLastError(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION);
        return FALSE;
    }

    //
    // If we're verifying the digital signatures of the files, then first make
    // sure that we've processed the catalog nodes for this queue.  However, we
    // don't want any copying to take place if there are OEM INFs in the list.
    //
    if((Action == SPQ_SCAN_FILE_VALIDITY) ||
       (Action == SPQ_SCAN_USE_CALLBACKEX) ||
       (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)) {

        WriteLogEntry(
            lc,
            SETUP_LOG_TIME,
            MSG_LOG_BEGIN_VERIFY4_CAT_TIME,
            NULL);       // text message

        rc = _SetupVerifyQueuedCatalogs(Window,
                                        Queue,
                                        VERCAT_NO_PROMPT_ON_ERROR,
                                        NULL,
                                        NULL
                                       );

        WriteLogEntry(
            lc,
            SETUP_LOG_TIME,
            MSG_LOG_END_VERIFY4_CAT_TIME,
            NULL);       // text message

        if((Action == SPQ_SCAN_FILE_VALIDITY) && (rc != NO_ERROR)) {
            WriteLogEntry(
                lc,
                (rc == ERROR_CANNOT_COPY ? SETUP_LOG_VVERBOSE : SETUP_LOG_WARNING) | SETUP_LOG_BUFFER,
                MSG_LOG_SCANQUEUE_VERIFY_FAILED,
                NULL);
            WriteLogError(
                lc,
                (rc == ERROR_CANNOT_COPY ? SETUP_LOG_VVERBOSE : SETUP_LOG_WARNING),
                rc);

            //
            // Result output param has already been initialized to zero
            // above.
            //
            return TRUE;
        }
    } else {
        rc = NO_ERROR;
    }

    //
    // Regardless of whether or not the catalog validation succeeded
    // above, we still want to call the callback for each file.  The
    // failed catalog verifications will be reflected in the failed
    // file verifications that the caller gets informed of via the
    // Win32Error field of the FILEPATHS struct we give the callback.
    //
    // Initialize the static fields of that structure here so we don't
    // have to each pass through the loop below.
    //
    FilePaths.Target = TargetPath;
    FilePaths.Source = SourcePath;
    FilePathsSignerInfo.Target = TargetPath;
    FilePathsSignerInfo.Source = SourcePath;


    //
    // markup delete/rename usage
    // we don't need to do this if we have SPQ_SCAN_PRUNE_DELREN set
    //
    if(!(Flags & SPQ_SCAN_PRUNE_DELREN)) {
        for(TempNode=Queue->DeleteQueue; TempNode; TempNode=TempNode->Next) {
            if(!_SetupMarkFileNodeTargetFlags(Queue,
                                              TempNode->TargetDirectory,
                                              -1,
                                              TempNode->TargetFilename,
                                              SP_TEFLG_PRUNE_DEL,SP_TEFLG_PRUNE_DEL,
                                              NULL)) {
                //
                // this would indicate an out-of-memory condition
                // last error is set
                //
                return FALSE;
            }
        }
        for(TempNode=Queue->RenameQueue; TempNode; TempNode=TempNode->Next) {
            if(!_SetupMarkFileNodeTargetFlags(Queue,
                                              TempNode->SourcePath,
                                              -1,
                                              TempNode->SourceFilename,
                                              SP_TEFLG_PRUNE_RENSRC,SP_TEFLG_PRUNE_RENSRC,
                                              NULL)) {
                //
                // this would indicate an out-of-memory condition
                // last error is set
                //
                return FALSE;
            }
            if(!_SetupMarkFileNodeTargetFlags(Queue,
                                              TempNode->TargetDirectory == -1 ? TempNode->SourcePath : TempNode->TargetDirectory,
                                              -1,
                                              TempNode->TargetFilename,
                                              SP_TEFLG_PRUNE_RENTARG,SP_TEFLG_PRUNE_RENTARG,
                                              NULL)) {
                //
                // this would indicate an out-of-memory condition
                // last error is set
                //
                return FALSE;
            }
        }
    }

    //
    // Process all nodes in the copy queue.
    //
    Err = FALSE;
    Continue = TRUE;
    for(SourceMedia=Queue->SourceMediaList; Continue && SourceMedia; SourceMedia=SourceMedia->Next) {

        TempNode = NULL;
        QueueNode = SourceMedia->CopyQueue;

        while(Continue && QueueNode) {
            DWORD PrevNodeFlags;
            //
            // markup copy usage
            //
            if(!_SetupMarkFileNodeTargetFlags(Queue,
                                              QueueNode->TargetDirectory,
                                              -1,
                                              QueueNode->TargetFilename,
                                              SP_TEFLG_PRUNE_COPY,SP_TEFLG_PRUNE_COPY,
                                              &PrevNodeFlags)) {
                //
                // this would indicate an out-of-memory condition
                // last error is set
                //
                rc = GetLastError();
                Err = TRUE;
                Continue = FALSE;
                break;
            }

            //
            // Form target path.
            //
            lstrcpyn(
                TargetPath,
                pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetDirectory),
                MAX_PATH
                );

            pSetupConcatenatePaths(
                TargetPath,
                pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetFilename),
                MAX_PATH,
                NULL
                );


            if((PrevNodeFlags & (SP_TEFLG_PRUNE_DEL|SP_TEFLG_PRUNE_RENSRC|SP_TEFLG_PRUNE_RENTARG))
               && !((Flags & SPQ_SCAN_PRUNE_DELREN) || (QueueNode->StyleFlags & SP_COPY_NOPRUNE))) {
                DWORD msg;
                //
                // this file is touched in source/target
                // but the INF author forgot to mark it as SP_COPY_NOPRUNE
                // this typically indicates a mistake on the author's part
                // so we try and handle it best as we can
                // if SPQ_SCAN_PRUNE_DELREN is set, we'll kill the delete/rename entries
                // if it's not set, we enforce the SP_COPY_NOPRUNE
                //
                QueueNode->StyleFlags |= SP_COPY_NOPRUNE;

                if(PrevNodeFlags & SP_TEFLG_PRUNE_DEL) {
                    msg = MSG_LOG_CHANGEPRUNE_DEL;
                } else if(PrevNodeFlags & SP_TEFLG_PRUNE_RENSRC) {
                    msg = MSG_LOG_CHANGEPRUNE_RENSRC;
                } else {
                    msg = MSG_LOG_CHANGEPRUNE_RENTARG;
                }

                WriteLogEntry(
                    lc,
                    SETUP_LOG_WARNING,
                    msg,
                    NULL,
                    TargetPath);
            }

            //
            // Perform check on file.
            //
            PruneNode = FALSE;
            switch(Action) {

            case SPQ_SCAN_FILE_PRESENCE:

                Continue = FileExists(TargetPath,NULL);
                if(DoPruning) {
                    //
                    // File's presence should result in this copy node's removal
                    // from the queue--it's absence should be ignored.
                    //
                    if(Continue) {
                        PruneNode = TRUE;
                    } else {
                        //
                        // Leave copy node alone.
                        //
                        PruneNode = FALSE;
                        Continue = TRUE;
                    }
                } else {
                    if (Continue) {
                        //
                        // we should not continue if the copy node is marked as a "no prune" node
                        //
                        if (QueueNode->StyleFlags & SP_COPY_NOPRUNE) {
                            Continue = FALSE;
                        }
                    }
                }
                break;

            case SPQ_SCAN_FILE_VALIDITY:

                //
                // If we are going to prune the copy queue then:
                //
                // (a) only validate the file against the system catalogs (not
                //     against any OEM catalogs), and
                // (b) don't validate self-signed files.
                //
                // (Note: we will never consider an INF in %windir%\Inf to be
                // signed, since it is invalid to copy an INF there via a file
                // queue.  SetupCopyOEMInf must be used instead.)
                //
                if(GlobalSetupFlags & PSPGF_MINIMAL_EMBEDDED) {
                    //
                    // If we're pruning, then we don't want to consider any
                    // existing files as valid, otherwise we'd never copy
                    // anything!
                    //
                    Continue = !DoPruning;

                    //
                    // We want to prune the queue node if the target exists and the source doesn't
                    //
                    if ( DoPruning && FileExists(TargetPath, NULL) ) {
                        //
                        // The target exists!  Now build the path the source file...
                        //
                        lstrcpyn(SourcePath,
                                 pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourceRootPath),
                                 SIZECHARS(SourcePath)
                                );

                        if (QueueNode->SourcePath != -1) {
                            pSetupConcatenatePaths(SourcePath,
                                                   pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourcePath),
                                                   SIZECHARS(SourcePath),
                                                   NULL
                                                  );
                        }

                        pSetupConcatenatePaths(SourcePath,
                                               pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourceFilename),
                                               SIZECHARS(SourcePath),
                                               NULL
                                              );

                        //
                        // If there's no source file to copy, prune this node
                        //
                        Continue = !FileExists(SourcePath, NULL);
                    }

                } else {

                    Continue = (NO_ERROR == VerifySourceFile(lc,
                                                             Queue,
                                                             QueueNode,
                                                             pSetupGetFileTitle(TargetPath),
                                                             TargetPath,
                                                             NULL,
                                                             ((Queue->Flags & FQF_USE_ALT_PLATFORM)
                                                                 ? &(Queue->AltPlatformInfo)
                                                                 : Queue->ValidationPlatform),
                                                             (DoPruning
                                                                ? (VERIFY_FILE_IGNORE_SELFSIGNED | VERIFY_FILE_FAIL_COPIED_INFS)
                                                                : (VERIFY_FILE_USE_OEM_CATALOGS  | VERIFY_FILE_FAIL_COPIED_INFS)),
                                                             &Problem,
                                                             TempCharBuffer,
                                                             NULL,
                                                             NULL,
                                                             NULL
                                                            ));
                }

                if(DoPruning) {
                    //
                    // File's validity should result in this copy node's removal
                    // from the queue--it's invalidity should be ignored.
                    //
                    if(Continue) {
                        PruneNode = TRUE;
                    } else {
                        //
                        // Leave copy node alone.
                        //
                        PruneNode = FALSE;
                        Continue = TRUE;
                    }
                } else {
                    if (Continue) {
                        //
                        // we should not continue if the copy node is marked as a "no prune" node
                        //
                        if (QueueNode->StyleFlags & SP_COPY_NOPRUNE) {
                            Continue = FALSE;
                        }
                    }
                }
                break;

            case SPQ_SCAN_USE_CALLBACK:
            case SPQ_SCAN_USE_CALLBACKEX:
            case SPQ_SCAN_USE_CALLBACKEX_PRESENCE:
            case SPQ_SCAN_USE_CALLBACK_SIGNERINFO:

                flags = (QueueNode->InternalFlags & (INUSE_INF_WANTS_REBOOT | INUSE_IN_USE))
                      ? SPQ_DELAYED_COPY
                      : 0;

                if((Action == SPQ_SCAN_USE_CALLBACKEX) ||
                   (Action == SPQ_SCAN_USE_CALLBACKEX_PRESENCE) ||
                   (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)) {

                    //
                    // The caller requested the extended version of the queue
                    // scan callback--we need to build the source file path.
                    //
                    lstrcpyn(SourcePath,
                             pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourceRootPath),
                             SIZECHARS(SourcePath)
                            );

                    if(QueueNode->SourcePath != -1) {

                        pSetupConcatenatePaths(SourcePath,
                                         pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourcePath),
                                         SIZECHARS(SourcePath),
                                         NULL
                                        );
                    }

                    pSetupConcatenatePaths(SourcePath,
                                     pSetupStringTableStringFromId(Queue->StringTable, QueueNode->SourceFilename),
                                     SIZECHARS(SourcePath),
                                     NULL
                                    );

                    if((Action == SPQ_SCAN_USE_CALLBACKEX_PRESENCE)) {
                        Win32Error = NO_ERROR;
                    } else {

                        CatalogFile[0] = TEXT('\0');
                        DigitalSigner[0] = TEXT('\0');
                        SignerVersion[0] = TEXT('\0');

                        if(GlobalSetupFlags & PSPGF_MINIMAL_EMBEDDED) {
                            //
                            // We can't call our internal VerifySourceFile
                            // routine, because it doesn't expect to be asked
                            // for signer info when in "minimal embedded" mode
                            // (plus, it doesn't make sense to ask for such
                            // info anyway, since we have no idea who signed
                            // the file, or even if it was signed).
                            //
                            Win32Error = NO_ERROR;

                        } else {

                            Win32Error = VerifySourceFile(lc,
                                                          Queue,
                                                          QueueNode,
                                                          pSetupGetFileTitle(TargetPath),
                                                          TargetPath,
                                                          NULL,
                                                          ((Queue->Flags & FQF_USE_ALT_PLATFORM)
                                                                ? &(Queue->AltPlatformInfo)
                                                                : Queue->ValidationPlatform),
                                                          VERIFY_FILE_USE_OEM_CATALOGS | VERIFY_FILE_FAIL_COPIED_INFS,
                                                          &Problem,
                                                          TempCharBuffer,
                                                          (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)
                                                              ? CatalogFile
                                                              : NULL,
                                                          (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)
                                                              ? DigitalSigner
                                                              : NULL,
                                                          (Action == SPQ_SCAN_USE_CALLBACK_SIGNERINFO)
                                                              ? SignerVersion
                                                              : NULL
                                                         );
                        }
                    }

                    if ((Action == SPQ_SCAN_USE_CALLBACKEX) ||
                        (Action == SPQ_SCAN_USE_CALLBACKEX_PRESENCE)) {
                        FilePaths.Win32Error = Win32Error;
                        FilePaths.Flags  = QueueNode->StyleFlags;
                        CallbackParam1 = (UINT_PTR)(&FilePaths);
                        Notification = SPFILENOTIFY_QUEUESCAN_EX;
                    } else {
                        FilePathsSignerInfo.Win32Error = Win32Error;
                        FilePathsSignerInfo.Flags  = QueueNode->StyleFlags;
                        FilePathsSignerInfo.DigitalSigner = (DigitalSigner[0] != TEXT('\0'))
                                                                ? DigitalSigner
                                                                : NULL;
                        FilePathsSignerInfo.Version = (SignerVersion[0] != TEXT('\0'))
                                                                ? SignerVersion
                                                                : NULL;
                        FilePathsSignerInfo.CatalogFile = (CatalogFile[0] != TEXT('\0'))
                                                                ? CatalogFile
                                                                : NULL;
                        CallbackParam1 = (UINT_PTR)(&FilePathsSignerInfo);
                        Notification = SPFILENOTIFY_QUEUESCAN_SIGNERINFO;
                    }
                } else {
                    CallbackParam1 = (UINT_PTR)TargetPath;
                    Notification = SPFILENOTIFY_QUEUESCAN;
                }

                rc = (DWORD)pSetupCallMsgHandler(
                                    lc,
                                    CallbackRoutine,
                                    IsNativeCharWidth,
                                    CallbackContext,
                                    Notification,
                                    CallbackParam1,
                                    flags
                                    );
                *Result = rc;
                Err = (rc != NO_ERROR);
                Continue = !Err;
                break;
            }

            if(DoPruning && PruneNode) {
                BOOL ReallyPrune = TRUE;
                MYASSERT(Continue == TRUE);

                //
                // before we remove the item from the queue, we must check if the copy item
                // also exists in the rename or delete queues.  if it does, then we cannot
                // prune the item from the copy queue
                //
                if (QueueNode->StyleFlags & SP_COPY_NOPRUNE) {
                    ReallyPrune = FALSE;
                    TempNode = QueueNode;
                    QueueNode = QueueNode->Next;
                }

                if (ReallyPrune) {

                    WriteLogEntry(
                        lc,
                        SETUP_LOG_VERBOSE,
                        MSG_LOG_PRUNE,
                        NULL,
                        TargetPath);

                    NextNode = QueueNode->Next;
                    if(TempNode) {
                        TempNode->Next = NextNode;
                    } else {
                        SourceMedia->CopyQueue = NextNode;
                    }
                    MyFree(QueueNode);
                    QueueNode = NextNode;

                    //
                    // Adjust the queue node counts.
                    //
                    Queue->CopyNodeCount--;
                    SourceMedia->CopyNodeCount--;
                }


            } else {
                TempNode = QueueNode;
                QueueNode = QueueNode->Next;
            }
        }
    }

    if(Flags & SPQ_SCAN_PRUNE_DELREN) {
        //
        // flag tells us to rip out of Delete/Rename queues
        // files that are (or was) in the copy queue.
        // we'll only be asked to do this in limited cases
        // (eg, GUI setup)
        //
        TempNode = NULL;
        QueueNode = Queue->DeleteQueue;
        while(QueueNode) {
            DWORD PrevFlags;
            NextNode = QueueNode->Next;

            if(_SetupMarkFileNodeTargetFlags(Queue,
                                              QueueNode->TargetDirectory,
                                              -1,
                                              QueueNode->TargetFilename,
                                              0,0,
                                              &PrevFlags)) {
                if(PrevFlags & SP_TEFLG_PRUNE_COPY) {
                    //
                    // warn about this
                    //
                    lstrcpyn(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetDirectory),
                        MAX_PATH
                        );
                    pSetupConcatenatePaths(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetFilename),
                        MAX_PATH,
                        NULL
                        );
                    WriteLogEntry(
                        lc,
                        SETUP_LOG_WARNING,
                        MSG_LOG_PRUNE_DEL,
                        NULL,
                        TargetPath);

                    if(TempNode) {
                        TempNode->Next = NextNode;
                    } else {
                        Queue->DeleteQueue = NextNode;
                    }
                    MyFree(QueueNode);
                    Queue->DeleteNodeCount--;
                    QueueNode = NextNode;
                    continue;
                }
            }
            TempNode = QueueNode;
            QueueNode = NextNode;
        }
        TempNode = NULL;
        QueueNode = Queue->RenameQueue;
        while(QueueNode) {
            DWORD PrevFlags;
            NextNode = QueueNode->Next;

            if(_SetupMarkFileNodeTargetFlags(Queue,
                                              QueueNode->SourcePath,
                                              -1,
                                              QueueNode->SourceFilename,
                                              0,0,
                                              &PrevFlags)) {
                if(PrevFlags & SP_TEFLG_PRUNE_COPY) {
                    //
                    // warn about this
                    //
                    lstrcpyn(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->SourcePath),
                        MAX_PATH
                        );
                    pSetupConcatenatePaths(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->SourceFilename),
                        MAX_PATH,
                        NULL
                        );
                    WriteLogEntry(
                        lc,
                        SETUP_LOG_WARNING,
                        MSG_LOG_PRUNE_RENSRC,
                        NULL,
                        TargetPath);

                    if(TempNode) {
                        TempNode->Next = NextNode;
                    } else {
                        Queue->RenameQueue = NextNode;
                    }
                    MyFree(QueueNode);
                    Queue->RenameNodeCount--;
                    QueueNode = NextNode;
                    continue;
                }
            }
            if(_SetupMarkFileNodeTargetFlags(Queue,
                                              QueueNode->TargetDirectory == -1 ? QueueNode->SourcePath : QueueNode->TargetDirectory,
                                              -1,
                                              QueueNode->TargetFilename,
                                              0,0,
                                              &PrevFlags)) {
                if(PrevFlags & SP_TEFLG_PRUNE_COPY) {
                    //
                    // warn about this
                    //
                    lstrcpyn(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetDirectory == -1 ? QueueNode->SourcePath : QueueNode->TargetDirectory),
                        MAX_PATH
                        );
                    pSetupConcatenatePaths(
                        TargetPath,
                        pSetupStringTableStringFromId(Queue->StringTable,QueueNode->TargetFilename),
                        MAX_PATH,
                        NULL
                        );
                    WriteLogEntry(
                        lc,
                        SETUP_LOG_WARNING,
                        MSG_LOG_PRUNE_RENTARG,
                        NULL,
                        TargetPath);

                    if(TempNode) {
                        TempNode->Next = NextNode;
                    } else {
                        Queue->RenameQueue = NextNode;
                    }
                    MyFree(QueueNode);
                    Queue->RenameNodeCount--;
                    QueueNode = NextNode;
                    continue;
                }
            }
            TempNode = QueueNode;
            QueueNode = NextNode;
        }
    }
    //
    // If the case of SPQ_SCAN_USE_CALLBACK(EX), *Result is already set up
    // when we get here. If Continue is TRUE then we visited all nodes
    // and the presence/validity check passed on all of them. Note that
    // if Continue is TRUE then Err must be FALSE.
    //
    if((Action == SPQ_SCAN_FILE_PRESENCE) || (Action == SPQ_SCAN_FILE_VALIDITY)) {

        if(DoPruning) {
            //
            // Set result based on whether any of the queues have nodes in them.
            //
            if(Queue->CopyNodeCount) {
                *Result = 0;
            } else {
                *Result = (Queue->DeleteNodeCount || Queue->RenameNodeCount || Queue->BackupNodeCount) ? 2 : 1;
            }
        } else {
            //
            // If we weren't doing pruning, then we know that the Continue
            // variable indicates whether or not we bailed partway through.
            //
            if(Continue) {
                //
                // Need to set up Result.
                //
                if((Flags & SPQ_SCAN_INFORM_USER) && Queue->CopyNodeCount
                && (Message = RetreiveAndFormatMessage(MSG_NO_NEED_TO_COPY))) {

                    //
                    // Overload TargetPath for use as the caption string.
                    //
                    GetWindowText(Window,TargetPath,sizeof(TargetPath)/sizeof(TargetPath[0]));

                    i = MessageBox(
                            Window,
                            Message,
                            TargetPath,
                            MB_APPLMODAL | MB_YESNO | MB_ICONINFORMATION
                            );

                    MyFree(Message);

                    if(i == IDYES) {
                        //
                        // User wants to skip copying.
                        //
                        *Result = (Queue->DeleteNodeCount || Queue->RenameNodeCount || Queue->BackupNodeCount) ? 2 : 1;
                    } else {
                        //
                        // User wants to perform copy.
                        //
                        *Result = 0;
                    }
                } else {
                    //
                    // Don't want to ask user. Set up Result based on whether
                    // there are items in the delete, rename or backup queues.
                    //
                    *Result = (Queue->DeleteNodeCount || Queue->RenameNodeCount || Queue->BackupNodeCount) ? 2 : 1;
                }
            } else {
                //
                // Presence/validity check failed.
                //
                *Result = 0;
            }

            //
            // Empty the copy queue if necessary.
            //
            if(*Result) {
                for(SourceMedia=Queue->SourceMediaList; Continue && SourceMedia; SourceMedia=SourceMedia->Next) {
                    for(QueueNode=SourceMedia->CopyQueue; QueueNode; QueueNode=TempNode) {
                        TempNode = QueueNode->Next;
                        MyFree(QueueNode);
                    }
                    Queue->CopyNodeCount -= SourceMedia->CopyNodeCount;
                    SourceMedia->CopyQueue = NULL;
                    SourceMedia->CopyNodeCount = 0;
                }
                //
                // We think we just removed all files in all copy queues.
                // The 2 counts we maintain should be in sync -- meaning that
                // the total copy node count should now be 0.
                //
                MYASSERT(Queue->CopyNodeCount == 0);
            }
        }
    }

    SetLastError(rc);
    return(!Err);
}

#ifdef UNICODE
//
// ANSI version for UNICODE
//
BOOL
SetupScanFileQueueA(
    IN  HSPFILEQ            FileQueue,
    IN  DWORD               Flags,
    IN  HWND                Window,            OPTIONAL
    IN  PSP_FILE_CALLBACK_A CallbackRoutine,   OPTIONAL
    IN  PVOID               CallbackContext,   OPTIONAL
    OUT PDWORD              Result
    )
{
    BOOL b;

    try {
        b = _SetupScanFileQueue(
                FileQueue,
                Flags,
                Window,
                CallbackRoutine,
                CallbackContext,
                Result,
                FALSE
                );
    } except(EXCEPTION_EXECUTE_HANDLER) {
       b = FALSE;
       SetLastError(ERROR_INVALID_DATA);
    }

    return(b);
}

#else
//
// UNICODE version for Win9x
//
BOOL
SetupScanFileQueueW(
    IN  HSPFILEQ            FileQueue,
    IN  DWORD               Flags,
    IN  HWND                Window,            OPTIONAL
    IN  PSP_FILE_CALLBACK_W CallbackRoutine,   OPTIONAL
    IN  PVOID               CallbackContext,   OPTIONAL
    OUT PDWORD              Result
    )
{
    UNREFERENCED_PARAMETER(FileQueue);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(Window);
    UNREFERENCED_PARAMETER(CallbackRoutine);
    UNREFERENCED_PARAMETER(CallbackContext);
    UNREFERENCED_PARAMETER(Result);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return(FALSE);
}

#endif


BOOL
SetupScanFileQueue(
    IN  HSPFILEQ          FileQueue,
    IN  DWORD             Flags,
    IN  HWND              Window,            OPTIONAL
    IN  PSP_FILE_CALLBACK CallbackRoutine,   OPTIONAL
    IN  PVOID             CallbackContext,   OPTIONAL
    OUT PDWORD            Result
    )

/*++

Routine Description:

    This routine scans a setup file queue, performing an operation on each
    node in its copy list. The operation is specified by a set of flags.

    A caller can use this API to determine whether all files that have been
    enqueued for copy already exist on the target, and if so, to inform the
    user, who may elect to skip the file copying. This can spare the user from
    having to furnish Setup media in many cases.

Arguments:

    FileQueue - supplies handle to Setup file queue whose copy list is to
        be scanned/iterated.

    Flags - supplies a set of values that control operation of the API. A
        combination of the following values:

        SPQ_SCAN_FILE_PRESENCE - determine whether all target files in the
            copy queue are already present on the target.

        SPQ_SCAN_FILE_VALIDITY - determine whether all target files in the
            copy queue are already present on the target, and verify their
            digital signatures.

        SPQ_SCAN_USE_CALLBACK - for each node in the queue, call the
            callback routine with SPFILENOTIFY_QUEUESCAN.  If the callback
            routine returns non-0 then queue processing is stopped and this
            routine returns FALSE immediately.

        SPQ_SCAN_USE_CALLBACKEX - same as SPQ_SCAN_USE_CALLBACK except that
            SPFILENOTIFY_QUEUESCAN_EX is used instead.  This supplies a pointer
            to a FILEPATHS structure in Param1, thus you get both source and
            destination info.  You also get the results of the file presence
            check (and if present, of its digital signature verification) in
            the Win32Error field, and the CopyStyle flags in effect for that
            copy queue node in the Flags field.

        SPQ_SCAN_USE_CALLBACK_SIGNERINFO - same as SPQ_SCAN_USE_CALLBACK except
            that SPFILENOTIFY_QUEUESCAN_SIGNERINFO is used instead.  This
            supplies a pointer to a FILEPATHS_SIGNERINFO structure in Param1,
            thus you get both source and destination info.  You also get the
            results of the file presence check (and if present, of its digital
            signature verification) in the Win32Error field, and the CopyStyle
            flags in effect for that copy queue node in the Flags field. In
            addition you get who digitaly signed the file in the DigitalSigner
            field and the SHA1 key in the SignerId field.

        Exactly one of SPQ_SCAN_FILE_PRESENCE, SPQ_SCAN_FILE_VALIDITY,
        SPQ_SCAN_USE_CALLBACK, SPQ_SCAN_USE_CALLBACKEX, or
        SPQ_SCAN_USE_CALLBACK_SIGNERINFO must be specified.

        SPQ_SCAN_INFORM_USER - if specified and all files in the queue
            pass the presence/validity check, then this routine will inform
            the user that the operation he is attempting requires files but
            that we believe all files are already present. Ignored if
            SPQ_SCAN_FILE_PRESENCE or SPQ_SCAN_FILE_VALIDITY is not specified.
            Not valid if specified in combination with SPQ_SCAN_PRUNE_COPY_QUEUE.

        SPQ_SCAN_PRUNE_COPY_QUEUE - if specified, the copy queue will be pruned
            of any nodes that are deemed unnecessary.  This determination is
            made based on the type of queue scan being performed:

            If SPQ_SCAN_FILE_PRESENCE, then the presence of a file having the
            destination filename is sufficient to consider this copy operation
            unnecessary.

            If SPQ_SCAN_FILE_VALIDITY, then the destination file must not only
            be present, but also valid in order for the copy operation to be
            considered unnecessary.

            If SPQ_SCAN_USE_CALLBACK, SPQ_SCAN_USE_CALLBACKEX or
            SPQ_SCAN_USE_CALLBACK_SIGNERINFO, then the queue callback routine
            should return zero to mark the copy node as unnecessary, or non-zero
            to leave the node in the copy queue.

            NOTE: This flag may only be specified _before_ the queue has been
            committed.  This means that the flags contained in Param2 will
            always be zero.  If SetupScanFileQueue is called with
            SPQ_SCAN_PRUNE_COPY_QUEUE after committing the queue, the API will
            fail and GetLastError() will return
            ERROR_NO_MORE_ITEMS.

            This flag is not valid if specified in combination with
            SPQ_SCAN_INFORM_USER.

    Window - specifies the window to own any dialogs, etc, that may be
        presented. Unused if Flags does not contain one of
        SPQ_SCAN_FILE_PRESENCE or SPQ_SCAN_FILE_VALIDITY, or if Flags does not
        contain SPQ_SCAN_INFORM_USER.

    CallbackRoutine - required if Flags includes SPQ_SCAN_USE_CALLBACK.
        Specifies a callback function to be called on each node in
        the copy queue. The notification code passed to the callback is
        SPFILENOTIFY_QUEUESCAN.

    CallbackContext - caller-defined data to be passed to CallbackRoutine.

    Result - receives result of routine. See below.

Return Value:

    If FALSE, then an error occurred or the callback function returned non-0.
    Check Result -- if it is non-0, then it is the value returned by
    the callback function which stopped queue processing.
    If Result is 0, then extended error information is available from
    GetLastError().

    If TRUE, then all nodes were processed. Result is 0 if a callback was
    specified. If SPQ_SCAN_USE_CALLBACK(EX) or SPQ_SCAN_USE_CALLBACK_SIGNERINFO
    was not specified, then Result indicates whether the queue passed the
    presence/validity check:

        Result = 0: queue failed the check, or the queue passed the
        check but SPQ_SCAN_INFORM_USER was specified and the user indicated
        that he wants new copies of the files.  There are still nodes in the
        copy queue, although if SPQ_SCAN_PRUNE_COPY_QUEUE is specified, then
        any nodes that were validated have been pruned.

        Result = 1: queue passed the check, and, if SPQ_SCAN_INFORM_USER was
        specified, the user indicated that no copying is required. If Result is
        1, the copy queue has been emptied, and there are no elements on the
        delete or rename queues, so the caller may skip queue commit.

        Result = 2: queue passed the check, and, if SPQ_SCAN_INFORM_USER was
        specified, the user indicated that no copying is required. In this case,
        the copy queue has been emptied, however there are elements on the
        delete or rename queues, so the caller may not skip queue commit.

--*/

{
    BOOL b;

    try {
        b = _SetupScanFileQueue(
                FileQueue,
                Flags,
                Window,
                CallbackRoutine,
                CallbackContext,
                Result,
                TRUE
                );
    } except(EXCEPTION_EXECUTE_HANDLER) {
       b = FALSE;
       SetLastError(ERROR_INVALID_DATA);
    }

    return(b);
}


INT
SetupPromptReboot(
    IN HSPFILEQ FileQueue,  OPTIONAL
    IN HWND     Owner,
    IN BOOL     ScanOnly
    )

/*++

Routine Description:

    This routine asks the user whether he wants to reboot the system,
    optionally dependent on whether any files in a committed file queue
    were in use (and are thus now pending operations via MoveFileEx()).
    A reboot is also required if any files were installed as boot files
    (e.g., marked as COPYFLG_REPLACE_BOOT_FILE in the INF).

    If the user answers yes to the prompt, shutdown is initiated
    before this routine returns.

Arguments:

    FileQueue - if specified, supplies a file queue upon which
        to base the decision about whether shutdown is necessary.
        If not specified, then this routine assumes shutdown is
        necessary and asks the user what to do.

    Owner - supplies window handle for parent window to own windows
        created by this routine.

    ScanOnly - if TRUE, then the user is never asked whether he wants
        to reboot and no shutdown is initiated. In this case FileQueue
        must be specified. If FALSE then this routine functions as
        described above.

        This flags is used when the caller wants to determine whether
        shutdown is necessary separately from actually performing
        the shutdown.

Return Value:

    A combination of the following flags or -1 if an error occured:

    SPFILEQ_FILE_IN_USE: at least one file was in use and thus there are
        delayed file operations pending. This flag will never be set
        when FileQueue is not specified.

    SPFILEQ_REBOOT_RECOMMENDED: it is recommended that the system
        be rebooted. Depending on other flags and user response to
        the shutdown query, shutdown may already be happening.

    SPFILEQ_REBOOT_IN_PROGRESS: shutdown is in progress.

--*/

{
    PSP_FILE_QUEUE Queue;
    PSP_FILE_QUEUE_NODE QueueNode;
    PSOURCE_MEDIA_INFO SourceMedia;
    INT Flags;
    int i;
    DWORD Reason = REASON_PLANNED_FLAG;

    //
    // If only scanning, there must be a FileQueue to scan!
    //
    if(ScanOnly && !FileQueue) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    //
    // If we're not just scanning (i.e., we're potentially going to popup UI,
    // then we'd better be interactive.
    //
    if(!ScanOnly && (GlobalSetupFlags & (PSPGF_NONINTERACTIVE|PSPGF_UNATTENDED_SETUP))) {
        SetLastError(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION);
        return -1;
    }

    Queue = (PSP_FILE_QUEUE)FileQueue;
    Flags = 0;

    //
    // Scan file queue if the caller so desires.
    //
    if(FileQueue) {
        try {
            if(Queue->Flags & FQF_DEVICE_INSTALL) {
                Reason |= REASON_HWINSTALL;
            }
            //
            // Check delete queue for in-use files.
            //
            for(QueueNode=Queue->DeleteQueue; QueueNode; QueueNode=QueueNode->Next) {

                if(QueueNode->InternalFlags & INUSE_INF_WANTS_REBOOT) {
                    Flags |= SPFILEQ_REBOOT_RECOMMENDED;
                }

                if(QueueNode->InternalFlags & INUSE_IN_USE) {
                    Flags |= SPFILEQ_FILE_IN_USE;
                }
            }

            //
            // Check copy queues for in-use files.
            //
            for(SourceMedia=Queue->SourceMediaList; SourceMedia; SourceMedia=SourceMedia->Next) {
                for(QueueNode=SourceMedia->CopyQueue; QueueNode; QueueNode=QueueNode->Next) {

                    if(QueueNode->InternalFlags & INUSE_INF_WANTS_REBOOT) {
                        Flags |= SPFILEQ_REBOOT_RECOMMENDED;
                    }

                    if(QueueNode->InternalFlags & INUSE_IN_USE) {
                        Flags |= SPFILEQ_FILE_IN_USE;
                    }
                }
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            SetLastError(ERROR_INVALID_PARAMETER);
            Flags = -1;
        }
    } else {
        Flags = SPFILEQ_REBOOT_RECOMMENDED;
    }

    //
    // Ask the user if he wants to shut down, if necessary.
    //
    if(!ScanOnly &&
       (Flags & (SPFILEQ_REBOOT_RECOMMENDED | SPFILEQ_FILE_IN_USE)) &&
       (Flags != -1)) {
        if(RestartDialogEx(Owner,NULL,EWX_REBOOT,Reason) == IDYES) {
            Flags |= SPFILEQ_REBOOT_IN_PROGRESS;
        }
    }

    return(Flags);
}

