/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ki.h

Abstract:

    This module contains the private (internal) header file for the
    kernel.

Author:

    David N. Cutler (davec) 28-Feb-1989

Revision History:

--*/

#ifndef _KI_
#define _KI_
#include "ntos.h"
#include "stdio.h"
#include "stdlib.h"
#include "zwapi.h"

//
// Private (internal) constant definitions.
//
// Priority increment value definitions
//

#define ALERT_INCREMENT 2           // Alerted unwait priority increment
#define BALANCE_INCREMENT 10        // Balance set priority increment
#define RESUME_INCREMENT 0          // Resume thread priority increment
#define TIMER_EXPIRE_INCREMENT 0    // Timer expiration priority increment

//
// Define time critical priority class base.
//

#define TIME_CRITICAL_PRIORITY_BOUND 14

//
// Define NIL pointer value.
//

#define NIL (PVOID)NULL             // Null pointer to void

//
// Define macros which are used in the kernel only
//
// Clear member in set
//

#define ClearMember(Member, Set) \
    Set = Set & (~(1 << (Member)))

//
// Set member in set
//

#define SetMember(Member, Set) \
    Set = Set | (1 << (Member))

//
// Lock and unlock context swap lock.
//

#define KiLockContextSwap(OldIrql) \
    *(OldIrql) = KeAcquireQueuedSpinLockRaiseToSynch(LockQueueContextSwapLock)

#define KiUnlockContextSwap(OldIrql) \
    KeReleaseQueuedSpinLock(LockQueueContextSwapLock, OldIrql)

VOID
FASTCALL
KiUnlockDispatcherDatabase (
    IN KIRQL OldIrql
    );


// VOID
// KiBoostPriorityThread (
//    IN PKTHREAD Thread,
//    IN KPRIORITY Increment
//    )
//
//*++
//
// Routine Description:
//
//    This function boosts the priority of the specified thread using
//    the same algorithm used when a thread gets a boost from a wait
//    operation.
//
// Arguments:
//
//    Thread  - Supplies a pointer to a dispatcher object of type thread.
//
//    Increment - Supplies the priority increment that is to be applied to
//        the thread's priority.
//
// Return Value:
//
//    None.
//
//--*

#define KiBoostPriorityThread(Thread, Increment) {              \
    KPRIORITY NewPriority;                                      \
    PKPROCESS Process;                                          \
                                                                \
    if ((Thread)->Priority < LOW_REALTIME_PRIORITY) {           \
        if ((Thread)->PriorityDecrement == 0) {                 \
            NewPriority = (Thread)->BasePriority + (Increment); \
            if (NewPriority > (Thread)->Priority) {             \
                if (NewPriority >= LOW_REALTIME_PRIORITY) {     \
                    NewPriority = LOW_REALTIME_PRIORITY - 1;    \
                }                                               \
                                                                \
                Process = (Thread)->ApcState.Process;           \
                (Thread)->Quantum = Process->ThreadQuantum;     \
                KiSetPriorityThread((Thread), NewPriority);     \
            }                                                   \
        }                                                       \
    }                                                           \
}

FORCEINLINE
LOGICAL
KiIsKernelStackSwappable (
    IN KPROCESSOR_MODE WaitMode,
    IN PKTHREAD Thread
    )

/*++

Routine Description:

    This function determines whether the kernel stack is swappabel for the
    the specified thread in a wait operation.

Arguments:

    WaitMode - Supplies the processor mode of the wait operation.

    Thread - Supplies a pointer to a dispatcher object of type thread.

Return Value:

    If the kernel stack for the specified thread is swappable, then TRUE is
    returned. Otherwise, FALSE is returned.

--*/

{

    return ((WaitMode != KernelMode) &&                         
            (Thread->EnableStackSwap != FALSE) &&               
            (Thread->Priority < (LOW_REALTIME_PRIORITY + 9)));
}

//
// Private (internal) structure definitions.
//
// APC Parameter structure.
//

typedef struct _KAPC_RECORD {
    PKNORMAL_ROUTINE NormalRoutine;
    PVOID NormalContext;
    PVOID SystemArgument1;
    PVOID SystemArgument2;
} KAPC_RECORD, *PKAPC_RECORD;

//
// Executive initialization.
//

VOID
ExpInitializeExecutive (
    IN ULONG Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Interprocessor interrupt function definitions.
//
// Define immediate interprocessor commands.
//

#define IPI_APC 1                       // APC interrupt request
#define IPI_DPC 2                       // DPC interrupt request
#define IPI_FREEZE 4                    // freeze execution request
#define IPI_PACKET_READY 8              // packet ready request
#define IPI_SYNCH_REQUEST 16            // reverse stall packet request

//
// Define interprocess interrupt types.
//

typedef ULONG KIPI_REQUEST;

typedef
ULONG_PTR
(*PKIPI_BROADCAST_WORKER)(
    IN ULONG_PTR Argument
    );

#if NT_INST

#define IPI_INSTRUMENT_COUNT(a,b) KiIpiCounts[a].b++;

#else

#define IPI_INSTRUMENT_COUNT(a,b)

#endif

//
// Define interprocessor interrupt function prototypes.
//

ULONG_PTR
KiIpiGenericCall (
    IN PKIPI_BROADCAST_WORKER BroadcastFunction,
    IN ULONG_PTR Context
    );

#if defined(_AMD64_) || defined(_IA64_)

ULONG
KiIpiProcessRequests (
    VOID
    );

#endif // defined(_AMD64_) || defined(_IA64_)

VOID
FASTCALL
KiIpiSend (
    IN KAFFINITY TargetProcessors,
    IN KIPI_REQUEST Request
    );

VOID
KiIpiSendPacket (
    IN KAFFINITY TargetProcessors,
    IN PKIPI_WORKER WorkerFunction,
    IN PVOID Parameter1,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    );

VOID
FASTCALL
KiIpiSignalPacketDone (
    IN PKIPI_CONTEXT SignalDone
    );

VOID
KiIpiStallOnPacketTargets (
    KAFFINITY TargetSet
    );

//
// Private (internal) function definitions.
//

VOID
FASTCALL
KiActivateWaiterQueue (
    IN PKQUEUE Queue
    );

BOOLEAN
KiAdjustInterruptTime (
    IN LONGLONG TimeDelta
    );

VOID
KiAllProcessorsStarted (
    VOID
    );

VOID
KiApcInterrupt (
    VOID
    );

NTSTATUS
KiCallUserMode (
    IN PVOID *OutputBuffer,
    IN PULONG OutputLength
    );

typedef struct {
    ULONGLONG               Adjustment;
    LARGE_INTEGER           NewCount;
    volatile LONG           KiNumber;
    volatile LONG           HalNumber;
    volatile LONG           Barrier;
} ADJUST_INTERRUPT_TIME_CONTEXT, *PADJUST_INTERRUPT_TIME_CONTEXT;

VOID
KiCalibrateTimeAdjustment (
    PADJUST_INTERRUPT_TIME_CONTEXT Adjust
    );

VOID
KiChainedDispatch (
    VOID
    );

#if DBG

VOID
KiCheckTimerTable (
    IN ULARGE_INTEGER SystemTime
    );

#endif

LARGE_INTEGER
KiComputeReciprocal (
    IN LONG Divisor,
    OUT PCCHAR Shift
    );

ULONG
KiComputeTimerTableIndex (
    IN LARGE_INTEGER Interval,
    IN LARGE_INTEGER CurrentCount,
    IN PKTIMER Timer
    );

PLARGE_INTEGER
FASTCALL
KiComputeWaitInterval (
    IN PLARGE_INTEGER OriginalTime,
    IN PLARGE_INTEGER DueTime,
    IN OUT PLARGE_INTEGER NewTime
    );

NTSTATUS
KiContinue (
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

VOID
KiDeliverApc (
    IN KPROCESSOR_MODE PreviousMode,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

VOID
KiDispatchException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance
    );

KCONTINUE_STATUS
KiSetDebugProcessor (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN KPROCESSOR_MODE PreviousMode
    );

ULONG
KiCopyInformation (
    IN OUT PEXCEPTION_RECORD ExceptionRecord1,
    IN PEXCEPTION_RECORD ExceptionRecord2
    );

VOID
KiDispatchInterrupt (
    VOID
    );

PKTHREAD
FASTCALL
KiFindReadyThread (
    IN ULONG Processor,
    KPRIORITY LowPriority
    );

VOID
KiFloatingDispatch (
    VOID
    );

#if !defined(_IA64_) && !defined(_AMD64_)

VOID
FASTCALL
KiFlushSingleTb (
    IN BOOLEAN Invalid,
    IN PVOID Virtual
    );

#endif // !_IA64_ && !_AMD64_

VOID
KiFlushMultipleTb (
    IN BOOLEAN Invalid,
    IN PVOID *Virtual,
    IN ULONG Count
    );

//
// VOID
// KiSetTbFlushTimeStampBusy (
//    VOID
//    )
//
//*++
//
// Routine Description:
//
//    This function sets the TB flush time stamp busy by setting the high
//    order bit of the TB flush time stamp. All readers of the time stamp
//    value will spin until the bit is cleared.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--*

__inline
VOID
KiSetTbFlushTimeStampBusy (
   VOID
   )

{

    LONG Value;

    //
    // While the TB flush time stamp counter is being updated the high
    // order bit of the time stamp value is set. Otherwise, the bit is
    // clear.
    //

    do {
        do {
        } while ((Value = KiTbFlushTimeStamp) < 0);

        //
        // Attempt to set the high order bit.
        //

    } while (InterlockedCompareExchange((PLONG)&KiTbFlushTimeStamp,
                                        Value | 0x80000000,
                                        Value) != Value);

    return;
}

// VOID
// KiClearTbFlushTimeStampBusy (
//    VOID
//    )
//
//*++
//
// Routine Description:
//
//    This function ckears the TB flush time stamp busy by clearing the high
//    order bit of the TB flush time stamp and incrementing the low 32-bit
//    value.
//
//    N.B. It is assumed that the high order bit of the time stamp value
//         is set on entry to this routine.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--*

__inline
VOID
KiClearTbFlushTimeStampBusy (
   VOID
   )

{

    LONG Value;

    //
    // Get the current TB flush time stamp value, compute the next value,
    // and store the result clearing the busy bit.
    //

    Value = (KiTbFlushTimeStamp + 1) & 0x7fffffff;
    InterlockedExchange((PLONG)&KiTbFlushTimeStamp, Value);
    return;
}

PULONG
KiGetUserModeStackAddress (
    VOID
    );

VOID
KiInitializeContextThread (
    IN PKTHREAD Thread,
    IN PKSYSTEM_ROUTINE SystemRoutine,
    IN PKSTART_ROUTINE StartRoutine OPTIONAL,
    IN PVOID StartContext OPTIONAL,
    IN PCONTEXT ContextFrame OPTIONAL
    );

VOID
KiInitializeKernel (
    IN PKPROCESS Process,
    IN PKTHREAD Thread,
    IN PVOID IdleStack,
    IN PKPRCB Prcb,
    IN CCHAR Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
KiInitQueuedSpinLocks (
    PKPRCB Prcb,
    ULONG Number
    );

VOID
KiInitSystem (
    VOID
    );

BOOLEAN
KiInitMachineDependent (
    VOID
    );

VOID
KiInitializeUserApc (
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PKNORMAL_ROUTINE NormalRoutine,
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

LONG
FASTCALL
KiInsertQueue (
    IN PKQUEUE Queue,
    IN PLIST_ENTRY Entry,
    IN BOOLEAN Head
    );

BOOLEAN
FASTCALL
KiInsertQueueApc (
    IN PKAPC Apc,
    IN KPRIORITY Increment
    );

LOGICAL
FASTCALL
KiInsertTreeTimer (
    IN PKTIMER Timer,
    IN LARGE_INTEGER Interval
    );

VOID
KiInterruptDispatch (
    VOID
    );

VOID
KiInterruptDispatchRaise (
    IN PKINTERRUPT Interrupt
    );

VOID
KiInterruptDispatchSame (
    IN PKINTERRUPT Interrupt
    );

VOID
KiPassiveRelease (
    VOID
    );

PKTHREAD
KiQuantumEnd (
    VOID
    );

NTSTATUS
KiRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN BOOLEAN FirstChance
    );

VOID
FASTCALL
KiReadyThread (
    IN PKTHREAD Thread
    );

LOGICAL
FASTCALL
KiReinsertTreeTimer (
    IN PKTIMER Timer,
    IN ULARGE_INTEGER DueTime
    );

#if DBG

#define KiRemoveTreeTimer(Timer)               \
    (Timer)->Header.Inserted = FALSE;          \
    RemoveEntryList(&(Timer)->TimerListEntry); \
    (Timer)->TimerListEntry.Flink = NULL;      \
    (Timer)->TimerListEntry.Blink = NULL

#else

#define KiRemoveTreeTimer(Timer)               \
    (Timer)->Header.Inserted = FALSE;          \
    RemoveEntryList(&(Timer)->TimerListEntry)

#endif

#if defined(NT_UP)

#define KiRequestApcInterrupt(Processor) KiRequestSoftwareInterrupt(APC_LEVEL)

#else

#define KiRequestApcInterrupt(Processor)                  \
    if (KeGetCurrentPrcb()->Number == (CCHAR)Processor) { \
        KiRequestSoftwareInterrupt(APC_LEVEL);            \
    } else {                                              \
        KiIpiSend(AFFINITY_MASK(Processor), IPI_APC);     \
    }

#endif

#if defined(NT_UP)

#define KiRequestDispatchInterrupt(Processor)

#else

#define KiRequestDispatchInterrupt(Processor)             \
    if (KeGetCurrentPrcb()->Number != (CCHAR)Processor) { \
        KiIpiSend(AFFINITY_MASK(Processor), IPI_DPC);     \
    }

#endif

PKTHREAD
FASTCALL
KiSelectNextThread (
    IN ULONG Processor
    );

KAFFINITY
FASTCALL
KiSetAffinityThread (
    IN PKTHREAD Thread,
    IN KAFFINITY Affinity
    );

VOID
KiSetSystemTime (
    IN PLARGE_INTEGER NewTime,
    OUT PLARGE_INTEGER OldTime
    );

VOID
KiSuspendNop (
    IN struct _KAPC *Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
    );

VOID
KiSuspendRundown (
    IN PKAPC Apc
    );

VOID
KiSuspendThread (
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

BOOLEAN
KiSwapProcess (
    IN PKPROCESS NewProcess,
    IN PKPROCESS OldProcess
    );

LONG_PTR
FASTCALL
KiSwapThread (
    VOID
    );

VOID
KiThreadStartup (
    IN PVOID StartContext
    );

VOID
KiTimerExpiration (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
FASTCALL
KiTimerListExpire (
    IN PLIST_ENTRY ExpiredListHead,
    IN KIRQL OldIrql
    );

VOID
KiUnexpectedInterrupt (
    VOID
    );

VOID
FASTCALL
KiUnlinkThread (
    IN PKTHREAD Thread,
    IN LONG_PTR WaitStatus
    );

VOID
FASTCALL
KiUnwaitThread (
    IN PKTHREAD Thread,
    IN LONG_PTR WaitStatus,
    IN KPRIORITY Increment,
    IN PLIST_ENTRY ThreadList OPTIONAL
    );

VOID
KiUserApcDispatcher (
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2,
    IN PKNORMAL_ROUTINE NormalRoutine
    );

VOID
KiUserExceptionDispatcher (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextFrame
    );

BOOLEAN
FASTCALL
KiSwapContext (
    IN PKTHREAD Thread
    );

VOID
FASTCALL
KiWaitSatisfyAll (
    IN PKWAIT_BLOCK WaitBlock
    );

//
// VOID
// FASTCALL
// KiWaitSatisfyAny (
//    IN PKMUTANT Object,
//    IN PKTHREAD Thread
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for any type of object and performs
//    any side effects that are necessary.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
//    Thread - Supplies a pointer to a dispatcher object of type thread.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyAny(_Object_, _Thread_) {                               \
    if (((_Object_)->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == EventSynchronizationObject) { \
        (_Object_)->Header.SignalState = 0;                                  \
                                                                             \
    } else if ((_Object_)->Header.Type == SemaphoreObject) {                 \
        (_Object_)->Header.SignalState -= 1;                                 \
                                                                             \
    } else if ((_Object_)->Header.Type == MutantObject) {                    \
        (_Object_)->Header.SignalState -= 1;                                 \
        if ((_Object_)->Header.SignalState == 0) {                           \
            (_Thread_)->KernelApcDisable -= (_Object_)->ApcDisable;          \
            (_Object_)->OwnerThread = (_Thread_);                            \
            if ((_Object_)->Abandoned == TRUE) {                             \
                (_Object_)->Abandoned = FALSE;                               \
                (_Thread_)->WaitStatus = STATUS_ABANDONED;                   \
            }                                                                \
                                                                             \
            InsertHeadList((_Thread_)->MutantListHead.Blink,                 \
                           &(_Object_)->MutantListEntry);                    \
        }                                                                    \
    }                                                                        \
}

//
// VOID
// FASTCALL
// KiWaitSatisfyMutant (
//    IN PKMUTANT Object,
//    IN PKTHREAD Thread
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for a mutant object.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
//    Thread - Supplies a pointer to a dispatcher object of type thread.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyMutant(_Object_, _Thread_) {                            \
    (_Object_)->Header.SignalState -= 1;                                     \
    if ((_Object_)->Header.SignalState == 0) {                               \
        (_Thread_)->KernelApcDisable -= (_Object_)->ApcDisable;              \
        (_Object_)->OwnerThread = (_Thread_);                                \
        if ((_Object_)->Abandoned == TRUE) {                                 \
            (_Object_)->Abandoned = FALSE;                                   \
            (_Thread_)->WaitStatus = STATUS_ABANDONED;                       \
        }                                                                    \
                                                                             \
        InsertHeadList((_Thread_)->MutantListHead.Blink,                     \
                       &(_Object_)->MutantListEntry);                        \
    }                                                                        \
}

//
// VOID
// FASTCALL
// KiWaitSatisfyOther (
//    IN PKMUTANT Object
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for any type of object except a mutant
//    and performs any side effects that are necessary.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyOther(_Object_) {                                       \
    if (((_Object_)->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == EventSynchronizationObject) { \
        (_Object_)->Header.SignalState = 0;                                  \
                                                                             \
    } else if ((_Object_)->Header.Type == SemaphoreObject) {                 \
        (_Object_)->Header.SignalState -= 1;                                 \
                                                                             \
    }                                                                        \
}

VOID
FASTCALL
KiWaitTest (
    IN PVOID Object,
    IN KPRIORITY Increment
    );

VOID
KiFreezeTargetExecution (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiPollFreezeExecution (
    VOID
    );

VOID
KiSaveProcessorState (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiSaveProcessorControlState (
    IN PKPROCESSOR_STATE ProcessorState
    );

VOID
KiRestoreProcessorState (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiRestoreProcessorControlState (
    IN PKPROCESSOR_STATE ProcessorState
    );

#define KiEnableAlignmentExceptions()
#define KiDisableAlignmentExceptions()

BOOLEAN
KiHandleAlignmentFault(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance,
    OUT BOOLEAN *ExceptionForwarded
    );

//
// External references to private kernel data structures
//

extern PMESSAGE_RESOURCE_DATA  KiBugCodeMessages;
extern ULONG KiDmaIoCoherency;
extern ULONG KiMaximumDpcQueueDepth;
extern ULONG KiMinimumDpcRate;
extern ULONG KiAdjustDpcThreshold;
extern PKDEBUG_ROUTINE KiDebugRoutine;
extern PKDEBUG_SWITCH_ROUTINE KiDebugSwitchRoutine;
extern LIST_ENTRY KiDispatcherReadyListHead[MAXIMUM_PRIORITY];
extern const CCHAR KiFindFirstSetLeft[256];
extern CALL_PERFORMANCE_DATA KiFlushSingleCallData;
extern ULONG_PTR KiHardwareTrigger;
extern KAFFINITY KiIdleSummary;
extern KAFFINITY KiIdleSMTSummary;
extern KEVENT KiSwapEvent;
extern PKTHREAD KiSwappingThread;
extern KNODE KiNode0;
extern KNODE KiNodeInit[];
extern SINGLE_LIST_ENTRY KiProcessInSwapListHead;
extern SINGLE_LIST_ENTRY KiProcessOutSwapListHead;
extern SINGLE_LIST_ENTRY KiStackInSwapListHead;
extern LIST_ENTRY KiProfileSourceListHead;
extern BOOLEAN KiProfileAlignmentFixup;
extern ULONG KiProfileAlignmentFixupInterval;
extern ULONG KiProfileAlignmentFixupCount;
#if defined(_IA64_)
// KiProfileInterval value should be replaced by a call:
// HalQuerySystemInformation(HalProfileSourceInformation)
#else  // !_IA64_
extern ULONG KiProfileInterval;
#endif // !_IA64_
extern LIST_ENTRY KiProfileListHead;
extern KSPIN_LOCK KiProfileLock;
extern ULONG KiReadySummary;
extern UCHAR KiArgumentTable[];
extern ULONG KiServiceLimit;
extern ULONG_PTR KiServiceTable[];
extern CALL_PERFORMANCE_DATA KiSetEventCallData;
extern ULONG KiTickOffset;
extern LARGE_INTEGER KiTimeIncrementReciprocal;
extern CCHAR KiTimeIncrementShiftCount;
extern LIST_ENTRY KiTimerTableListHead[TIMER_TABLE_SIZE];
extern KAFFINITY KiTimeProcessor;
extern KDPC KiTimerExpireDpc;
extern KSPIN_LOCK KiFreezeExecutionLock;
extern BOOLEAN KiSlavesStartExecution;
extern PTIME_UPDATE_NOTIFY_ROUTINE KiTimeUpdateNotifyRoutine;
extern LIST_ENTRY KiWaitListHead;
extern CALL_PERFORMANCE_DATA KiWaitSingleCallData;
extern ULONG KiEnableTimerWatchdog;

#if defined(_IA64_)

extern ULONG KiMasterRid;
extern ULONGLONG KiMasterSequence;
extern ULONG KiIdealDpcRate;

#if !defined(UP_NT)

extern KSPIN_LOCK KiMasterRidLock;

#endif

VOID
KiSaveEmDebugContext (
    IN OUT PCONTEXT Context
    );

VOID
KiLoadEmDebugContext (
    IN PCONTEXT Context
    );

VOID
KiFlushRse (
    VOID
    );

VOID
KiInvalidateStackedRegisters (
    VOID
    );

NTSTATUS
Ki386CheckDivideByZeroTrap(
    IN PKTRAP_FRAME Frame
    );

#endif // defined(_IA64_)

#if defined(_IA64_)

extern KINTERRUPT KxUnexpectedInterrupt;

#endif

#if NT_INST

extern KIPI_COUNTS KiIpiCounts[MAXIMUM_PROCESSORS];

#endif

extern KSPIN_LOCK KiFreezeLockBackup;
extern ULONG KiFreezeFlag;
extern volatile ULONG KiSuspendState;

#if DBG

extern ULONG KiMaximumSearchCount;

#endif

// VOID
// KiSetSwapEvent (
//    VOID
//    )
//
//*++
//
// Routine Description:
//
//    This function sets the swap event or unwaits the swap thread.
//
//    N.B. The dispatcher lock must be held to call this routine.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--*

__inline
VOID
KiSetSwapEvent (
    VOID
    )

{

    PLIST_ENTRY WaitEntry;

    //
    // If the swap event wait queue is not empty, then unwait the swap
    // thread (there is only one swap thread). Otherwise, set the swap
    // event.
    //

    WaitEntry = KiSwapEvent.Header.WaitListHead.Flink;
    if (WaitEntry != &KiSwapEvent.Header.WaitListHead) {
        KiUnwaitThread(KiSwappingThread, 0, BALANCE_INCREMENT, NULL);

    } else {
        KiSwapEvent.Header.SignalState = 1;
    }

    return;
}

//
// Include platform specific internal kernel header file.
//

#if defined(_AMD64_)

#include "amd64\kiamd64.h"

#elif defined(_X86_)

#include "i386\kix86.h"

#endif // defined(_AMD64_)

#endif // defined(_KI_)
