/*++

Copyright (c) 1994-2000  Microsoft Corporation

Module Name:

    heappage.c

Abstract:

    Implementation of NT RtlHeap family of APIs for debugging
    applications with heap usage bugs.  Each allocation returned to
    the calling app is placed at the end of a virtual page such that
    the following virtual page is protected (ie, NO_ACCESS).
    So, when the errant app attempts to reference or modify memory
    beyond the allocated portion of a heap block, an access violation
    is immediately caused.  This facilitates debugging the app
    because the access violation occurs at the exact point in the
    app where the heap corruption or abuse would occur.  Note that
    significantly more memory (pagefile) is required to run an app
    using this heap implementation as opposed to the retail heap
    manager.

Author:

    Tom McGuire (TomMcg) 06-Jan-1995
    Silviu Calinoiu (SilviuC) 22-Feb-2000

Revision History:

--*/

#include "ntrtlp.h"
#include "heappage.h"       // external interface (hooks) to debug heap manager
#include "heappagi.h"
#include "heappriv.h"

int __cdecl sprintf(char *, const char *, ...);

//
//  Remainder of entire file is wrapped with #ifdef DEBUG_PAGE_HEAP so that
//  it will compile away to nothing if DEBUG_PAGE_HEAP is not defined in
//  heappage.h
//

#ifdef DEBUG_PAGE_HEAP

//
// Page size
//

#if defined(_X86_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x1000
    #endif
    #define USER_ALIGNMENT 8

#elif defined(_IA64_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x2000
    #endif
    #define USER_ALIGNMENT 16

#elif defined(_AMD64_)
    #ifndef PAGE_SIZE
    #define PAGE_SIZE   0x1000
    #endif
    #define USER_ALIGNMENT 16

#else
    #error  // platform not defined
#endif

//
// Few constants
//

#define DPH_HEAP_SIGNATURE       0xFFEEDDCC
#define FILL_BYTE                0xEE
#define HEAD_FILL_SIZE           0x10
#define RESERVE_SIZE             0x100000
#define VM_UNIT_SIZE             0x10000
#define POOL_SIZE                0x4000
#define INLINE                   __inline
#define MIN_FREE_LIST_LENGTH     8

//
// Few macros
//

#define ROUNDUP2( x, n ) ((( x ) + (( n ) - 1 )) & ~(( n ) - 1 ))

#if INTERNAL_DEBUG
#define DEBUG_CODE( a ) a
#else
#define DEBUG_CODE( a )
#endif

#define RETAIL_ASSERT( a ) ( (a) ? TRUE : \
    RtlpDebugPageHeapAssert( "Page heap: assert: (" #a ")\n" ))

#define DEBUG_ASSERT( a ) DEBUG_CODE( RETAIL_ASSERT( a ))

#define HEAP_HANDLE_FROM_ROOT( HeapRoot ) \
    ((PVOID)(((PCHAR)(HeapRoot)) - PAGE_SIZE ))

#define IF_GENERATE_EXCEPTION( Flags, Status ) {                \
    if (( Flags ) & HEAP_GENERATE_EXCEPTIONS )                  \
        RtlpDebugPageHeapException((ULONG)(Status));            \
    }

#define OUT_OF_VM_BREAK( Flags, szText ) {                      \
    if (( Flags ) & HEAP_BREAK_WHEN_OUT_OF_VM )                 \
        RtlpDebugPageHeapBreak(( szText ));                     \
    }

//
// List manipulation macros
//

#define ENQUEUE_HEAD( Node, Head, Tail ) {          \
            (Node)->pNextAlloc = (Head);            \
            if ((Head) == NULL )                    \
                (Tail) = (Node);                    \
            (Head) = (Node);                        \
            }

#define ENQUEUE_TAIL( Node, Head, Tail ) {          \
            if ((Tail) == NULL )                    \
                (Head) = (Node);                    \
            else                                    \
                (Tail)->pNextAlloc = (Node);        \
            (Tail) = (Node);                        \
            }

#define DEQUEUE_NODE( Node, Prev, Head, Tail ) {    \
            PVOID Next = (Node)->pNextAlloc;        \
            if ((Head) == (Node))                   \
                (Head) = Next;                      \
            if ((Tail) == (Node))                   \
                (Tail) = (Prev);                    \
            if ((Prev) != (NULL))                   \
                (Prev)->pNextAlloc = Next;          \
            }

//
// Bias/unbias pointer
//

#define BIAS_POINTER(p)      ((PVOID)((ULONG_PTR)(p) | (ULONG_PTR)0x01))
#define UNBIAS_POINTER(p)    ((PVOID)((ULONG_PTR)(p) & ~((ULONG_PTR)0x01)))
#define IS_BIASED_POINTER(p) ((PVOID)((ULONG_PTR)(p) & (ULONG_PTR)0x01))

//
// Scramble/unscramble
//
// We scramble heap pointers in the header blocks in order to make them
// look as kernel pointers and cause an AV if used. This is not totally
// accurate on IA64 but still likely to cause an AV.
//

#if defined(_WIN64)
#define SCRAMBLE_VALUE ((ULONG_PTR)0x8000000000000000)
#else
#define SCRAMBLE_VALUE ((ULONG_PTR)0x80000000)
#endif

#define SCRAMBLE_POINTER(P) ((PVOID)((ULONG_PTR)(P) ^ SCRAMBLE_VALUE))
#define UNSCRAMBLE_POINTER(P) ((PVOID)((ULONG_PTR)(P) ^ SCRAMBLE_VALUE))

//
// Protect/Unprotect heap structures macros
//
// The Protect/Unprotect functions are #if zeroed for now because there is
// an issue to be resolved when destroying a heap. At that moment we need
// to modify the global list of heaps and for this we need to touch the
// heap structure for another heap. In order to do this we need to unprotect
// and later protect it and for that we need to acquire the lock of that heap.
// But this is prone to causing deadlocks. Until we will find a smart scheme
// for doing this we will disable the whole /protect feature. Note also that
// the same problem exists in the heap create code path where we have to update
// the global list of heaps too.
//
// The best fix for this would be to move the fwrd/bwrd pointers for the heap
// list from the DPH_HEAP_ROOT structure into the special R/W page that stores
// the heap lock (needs to be always R/W).
//

#define PROTECT_HEAP_STRUCTURES( HeapRoot ) {                           \
            if ((HeapRoot)->HeapFlags & HEAP_PROTECTION_ENABLED ) {     \
                RtlpDebugPageHeapProtectStructures( (HeapRoot) );       \
            }                                                           \
        }                                                               \

#define UNPROTECT_HEAP_STRUCTURES( HeapRoot ) {                         \
            if ((HeapRoot)->HeapFlags & HEAP_PROTECTION_ENABLED ) {     \
                RtlpDebugPageHeapUnProtectStructures( (HeapRoot) );     \
            }                                                           \
        }                                                               \

//
// RtlpDebugPageHeap
//
// Global variable that marks that page heap is enabled. It is set
// in \nt\base\ntdll\ldrinit.c by reading the GlobalFlag registry
// value (system wide or per process one) and checking if the
// FLG_HEAP_PAGE_ALLOCS is set.
//

BOOLEAN RtlpDebugPageHeap;

//
// Internal version used to figure out what are people running
// in various VBLs.
//

PCHAR RtlpDphVersion = "01/30/2001";

//
// Page heaps list manipulation.
//
// We maintain a list of all page heaps in the process to support
// APIs like GetProcessHeaps. The list is also useful for debug
// extensions that need to iterate the heaps. The list is protected
// by RtlpDphHeapListCriticalSection lock.
//

BOOLEAN RtlpDphHeapListHasBeenInitialized;
RTL_CRITICAL_SECTION RtlpDphHeapListCriticalSection;
PDPH_HEAP_ROOT RtlpDphHeapListHead;
PDPH_HEAP_ROOT RtlpDphHeapListTail;
ULONG RtlpDphHeapListCount;

//
// `RtlpDebugPageHeapGlobalFlags' stores the global page heap flags.
// The value of this variable is copied into the per heap
// flags (ExtraFlags field) during heap creation.
//
// The initial value is so that by default we use page heap only with
// normal allocations. This way if system wide global flag for page
// heap is set the machine will still boot. After that we can enable
// page heap with "sudden death" for specific processes. The most useful
// flags for this case would be:
//
//    PAGE_HEAP_ENABLE_PAGE_HEAP       |
//    PAGE_HEAP_COLLECT_STACK_TRACES   ;
//
// If no flags specified the default is page heap light with
// stack trace collection.
//

ULONG RtlpDphGlobalFlags = PAGE_HEAP_COLLECT_STACK_TRACES;

//
// Page heap global flags.
//
// These values are read from registry in \nt\base\ntdll\ldrinit.c.
//

ULONG RtlpDphSizeRangeStart;
ULONG RtlpDphSizeRangeEnd;
ULONG RtlpDphDllRangeStart;
ULONG RtlpDphDllRangeEnd;
ULONG RtlpDphRandomProbability;
WCHAR RtlpDphTargetDlls [512];
UNICODE_STRING RtlpDphTargetDllsUnicode;

//
// If not zero controls the probability with which
// allocations will be failed on purpose by page heap
// manager. Timeout represents the initial period during
// process initialization when faults are not allowed.
//

ULONG RtlpDphFaultProbability;
ULONG RtlpDphFaultTimeOut;

//
// This variable offers volatile fault injection.
// It can be set/reset from debugger to disable/enable
// fault injection.
//

ULONG RtlpDphDisableFaults;

//
// `RtlpDphDebugLevel' controls debug messages in the code.
//

#define DPH_DEBUG_INTERNAL_VALIDATION 0x0001
#define DPH_DEBUG_RESERVED_2          0x0002
#define DPH_DEBUG_RESERVED_4          0x0004
#define DPH_DEBUG_RESERVED_8          0x0008
#define DPH_DEBUG_DECOMMIT_RANGES     0x0010
#define DPH_DEBUG_SLOW_CHECKS         0x0080
#define DPH_DEBUG_SHOW_VM_LIMITS      0x0100

ULONG RtlpDphDebugLevel;

//
// Threshold for delaying a free operation in the normal heap.
// If we get over this limit we start actually freeing blocks.
//

SIZE_T RtlpDphDelayedFreeCacheSize = 256 * PAGE_SIZE;

//
// Process wide trace database and the maximum size it can
// grow to.
//

SIZE_T RtlpDphTraceDatabaseMaximumSize = 256 * PAGE_SIZE;
PRTL_TRACE_DATABASE RtlpDphTraceDatabase;

//
// Support for normal heap allocations
//
// In order to make better use of memory available page heap will
// allocate some of the block into a normal NT heap that it manages.
// We will call these blocks "normal blocks" as opposed to "page blocks".
//
// All normal blocks have the requested size increased by DPH_BLOCK_INFORMATION.
// The address returned is of course of the first byte after the block
// info structure. Upon free, blocks are checked for corruption and
// then released into the normal heap.
//
// All these normal heap functions are called with the page heap
// lock acquired.
//

PVOID
RtlpDphNormalHeapAllocate (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    SIZE_T Size
    );

BOOLEAN
RtlpDphNormalHeapFree (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID Block
    );

PVOID
RtlpDphNormalHeapReAllocate (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID OldBlock,
    SIZE_T Size
    );

SIZE_T
RtlpDphNormalHeapSize (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID Block
    );

BOOLEAN
RtlpDphNormalHeapSetUserFlags(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    );

BOOLEAN
RtlpDphNormalHeapSetUserValue(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address,
    IN PVOID UserValue
    );

BOOLEAN
RtlpDphNormalHeapGetUserInfo(
    IN PDPH_HEAP_ROOT Heap,
    IN  ULONG  Flags,
    IN  PVOID  Address,
    OUT PVOID* UserValue,
    OUT PULONG UserFlags
    );

BOOLEAN
RtlpDphNormalHeapValidate(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address
    );

//
// Support for DPH_BLOCK_INFORMATION management
//
// This header information prefixes both the normal and page heap
// blocks.
//

#define DPH_CONTEXT_GENERAL                     0
#define DPH_CONTEXT_FULL_PAGE_HEAP_FREE         1
#define DPH_CONTEXT_FULL_PAGE_HEAP_REALLOC      2
#define DPH_CONTEXT_FULL_PAGE_HEAP_DESTROY      3
#define DPH_CONTEXT_NORMAL_PAGE_HEAP_FREE       4
#define DPH_CONTEXT_NORMAL_PAGE_HEAP_REALLOC    5
#define DPH_CONTEXT_NORMAL_PAGE_HEAP_SETFLAGS   6
#define DPH_CONTEXT_NORMAL_PAGE_HEAP_SETVALUE   7
#define DPH_CONTEXT_NORMAL_PAGE_HEAP_GETINFO    8
#define DPH_CONTEXT_DELAYED_FREE                9
#define DPH_CONTEXT_DELAYED_DESTROY             10

VOID
RtlpDphReportCorruptedBlock (
    PVOID Heap,
    ULONG Context,
    PVOID Block,
    ULONG Reason
    );

BOOLEAN
RtlpDphIsNormalHeapBlock (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    );

BOOLEAN
RtlpDphIsNormalFreeHeapBlock (
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    );

BOOLEAN
RtlpDphIsPageHeapBlock (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    );

BOOLEAN
RtlpDphWriteNormalHeapBlockInformation (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    SIZE_T RequestedSize,
    SIZE_T ActualSize
    );

BOOLEAN
RtlpDphWritePageHeapBlockInformation (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    SIZE_T RequestedSize,
    SIZE_T ActualSize
    );

BOOLEAN
RtlpDphGetBlockSizeFromCorruptedBlock (
    PVOID Block,
    PSIZE_T Size
    );

//
// Delayed free queue (of normal heap allocations) management
//

VOID
RtlpDphInitializeDelayedFreeQueue (
    );

VOID
RtlpDphAddToDelayedFreeQueue (
    PDPH_BLOCK_INFORMATION Info
    );

BOOLEAN
RtlpDphNeedToTrimDelayedFreeQueue (
    PSIZE_T TrimSize
    );

VOID
RtlpDphTrimDelayedFreeQueue (
    SIZE_T TrimSize,
    ULONG Flags
    );

VOID
RtlpDphFreeDelayedBlocksFromHeap (
    PVOID PageHeap,
    PVOID NormalHeap
    );

//
// Decision normal heap vs. page heap
//

BOOLEAN
RtlpDphShouldAllocateInPageHeap (
    PDPH_HEAP_ROOT Heap,
    SIZE_T Size
    );

BOOLEAN
RtlpDphVmLimitCanUsePageHeap (
    );

//
// Stack trace detection for trace database.
//

PRTL_TRACE_BLOCK
RtlpDphLogStackTrace (
    ULONG FramesToSkip
    );

//
//  Page heap general support functions
//

VOID
RtlpDebugPageHeapBreak(
    IN PCH Text
    );

BOOLEAN
RtlpDebugPageHeapAssert(
    IN PCH Text
    );

VOID
RtlpDebugPageHeapEnterCritSect(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN ULONG          Flags
    );

INLINE
VOID
RtlpDebugPageHeapLeaveCritSect(
    IN PDPH_HEAP_ROOT HeapRoot
    );

VOID
RtlpDebugPageHeapException(
    IN ULONG ExceptionCode
    );

PVOID
RtlpDebugPageHeapPointerFromHandle(
    IN PVOID HeapHandle
    );

PCCH
RtlpDebugPageHeapProtectionText(
    IN     ULONG Access,
    IN OUT PCHAR Buffer
    );

//
// Virtual memory manipulation functions
//

BOOLEAN
RtlpDebugPageHeapRobustProtectVM(
    IN PVOID   VirtualBase,
    IN SIZE_T  VirtualSize,
    IN ULONG   NewAccess,
    IN BOOLEAN Recursion
    );

INLINE
BOOLEAN
RtlpDebugPageHeapProtectVM(
    IN PVOID   VirtualBase,
    IN SIZE_T  VirtualSize,
    IN ULONG   NewAccess
    );

INLINE
PVOID
RtlpDebugPageHeapAllocateVM(
    IN SIZE_T nSize
    );

INLINE
BOOLEAN
RtlpDebugPageHeapReleaseVM(
    IN PVOID pVirtual
    );

INLINE
BOOLEAN
RtlpDebugPageHeapCommitVM(
    IN PVOID pVirtual,
    IN SIZE_T nSize
    );

INLINE
BOOLEAN
RtlpDebugPageHeapDecommitVM(
    IN PVOID pVirtual,
    IN SIZE_T nSize
    );

//
// Target dlls logic
//
// RtlpDphTargetDllsLoadCallBack is called in ntdll\ldrapi.c
// (LdrpLoadDll) whenever a new dll is loaded in the process
// space.
//

VOID
RtlpDphTargetDllsLogicInitialize (
    );

VOID
RtlpDphTargetDllsLoadCallBack (
    PUNICODE_STRING Name,
    PVOID Address,
    ULONG Size
    );

const WCHAR *
RtlpDphIsDllTargeted (
    const WCHAR * Name
    );

//
// Internal heap validation
//

VOID
RtlpDphInternalValidatePageHeap (
    PDPH_HEAP_ROOT Heap,
    PUCHAR ExemptAddress,
    SIZE_T ExemptSize
    );

VOID
RtlpDphValidateInternalLists (
    PDPH_HEAP_ROOT Heap
    );

//
// Fault injection logic
//

BOOLEAN
RtlpDphShouldFaultInject (
    );


//
// Free delayed cache internal checking
//

VOID
RtlpDphCheckFreeDelayedCache (
    PVOID CheckBlock,
    SIZE_T CheckSize
    );


//
// Defined in \base\ntdll\resource.c
//

VOID
RtlpCheckForCriticalSectionsInMemoryRange(
    IN PVOID StartAddress,
    IN SIZE_T RegionSize,
    IN PVOID Information
    );


/////////////////////////////////////////////////////////////////////
///////////////////////////////// Page heap general support functions
/////////////////////////////////////////////////////////////////////

VOID
RtlpDebugPageHeapBreak(
    IN PCH Text
    )
{
    DbgPrint( Text );
    DbgBreakPoint();
}

BOOLEAN
RtlpDebugPageHeapAssert(
    IN PCH Text
    )
{
    RtlpDebugPageHeapBreak( Text );
    return FALSE;
}

VOID
RtlpDebugPageHeapEnterCritSect(
    IN PDPH_HEAP_ROOT HeapRoot,
    IN ULONG          Flags
    )
{
    if (HeapRoot->FirstThread == NULL) {
        HeapRoot->FirstThread = NtCurrentTeb()->ClientId.UniqueThread;
    }

    if (Flags & HEAP_NO_SERIALIZE) {

        //
        // If current thread has a different ID than the first thread
        // that got into this heap then we break. Avoid this check if
        // this allocation comes from Global/Local Heap APIs because
        // they lock the heap in a separate call and then they call
        // NT heap APIs with no_serialize flag set.
        //
        // Note. We avoid this check if we do not have the specific flag
        // on. This is so because MPheap-like heaps can give false 
        // positives.
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CHECK_NO_SERIALIZE_ACCESS)) {
            if (RtlpDebugPageHeapPointerFromHandle(RtlProcessHeap()) != HeapRoot) {
                if (HeapRoot->FirstThread != NtCurrentTeb()->ClientId.UniqueThread) {
                    
                    VERIFIER_STOP (APPLICATION_VERIFIER_UNSYNCHRONIZED_ACCESS,
                                   "multithreaded access in HEAP_NO_SERIALIZE heap",
                                   HeapRoot, "Heap handle",
                                   HeapRoot->FirstThread, "First thread that used the heap",
                                   NtCurrentTeb()->ClientId.UniqueThread, "Current thread using the heap",
                                   1, "/no_sync option used");
                }
            }
        }

        if (! RtlTryEnterCriticalSection( HeapRoot->HeapCritSect )) {

            if (HeapRoot->nRemoteLockAcquired == 0) {

                //
                //  Another thread owns the CritSect.  This is an application
                //  bug since multithreaded access to heap was attempted with
                //  the HEAP_NO_SERIALIZE flag specified.
                //

                VERIFIER_STOP (APPLICATION_VERIFIER_UNSYNCHRONIZED_ACCESS,
                               "multithreaded access in HEAP_NO_SERIALIZE heap",
                               HeapRoot, "Heap handle",
                               HeapRoot->HeapCritSect->OwningThread, "Thread owning heap lock",
                               NtCurrentTeb()->ClientId.UniqueThread, "Current thread trying to acquire the heap lock",
                               0, "");

                //
                //  In the interest of allowing the errant app to continue,
                //  we'll force serialization and continue.
                //

                HeapRoot->HeapFlags &= ~HEAP_NO_SERIALIZE;

            }

            RtlEnterCriticalSection( HeapRoot->HeapCritSect );

        }
    }
    else {
        RtlEnterCriticalSection( HeapRoot->HeapCritSect );
    }
}

INLINE
VOID
RtlpDebugPageHeapLeaveCritSect(
    IN PDPH_HEAP_ROOT HeapRoot
    )
{
    RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
}

VOID
RtlpDebugPageHeapException(
    IN ULONG ExceptionCode
    )
{
    EXCEPTION_RECORD ER;

    ER.ExceptionCode    = ExceptionCode;
    ER.ExceptionFlags   = 0;
    ER.ExceptionRecord  = NULL;
    ER.ExceptionAddress = RtlpDebugPageHeapException;
    ER.NumberParameters = 0;
    RtlRaiseException( &ER );
}

PVOID
RtlpDebugPageHeapPointerFromHandle(
    IN PVOID HeapHandle
    )
{
    try {
        if (((PHEAP)(HeapHandle))->ForceFlags & HEAP_FLAG_PAGE_ALLOCS) {

            PDPH_HEAP_ROOT HeapRoot = (PVOID)(((PCHAR)(HeapHandle)) + PAGE_SIZE );

            if (HeapRoot->Signature == DPH_HEAP_SIGNATURE) {
                return HeapRoot;
            }
        }
    }
    except( EXCEPTION_EXECUTE_HANDLER ) {
    }

    VERIFIER_STOP (APPLICATION_VERIFIER_BAD_HEAP_HANDLE,
                   "heap handle with incorrect signature",
                   HeapHandle, "Heap handle", 
                   0, "", 0, "", 0, "");

    return NULL;
}

PCCH
RtlpDebugPageHeapProtectionText(
    IN     ULONG Access,
    IN OUT PCHAR Buffer
    )
{
    switch (Access) {
    case PAGE_NOACCESS:          return "PAGE_NOACCESS";
    case PAGE_READONLY:          return "PAGE_READONLY";
    case PAGE_READWRITE:         return "PAGE_READWRITE";
    case PAGE_WRITECOPY:         return "PAGE_WRITECOPY";
    case PAGE_EXECUTE:           return "PAGE_EXECUTE";
    case PAGE_EXECUTE_READ:      return "PAGE_EXECUTE_READ";
    case PAGE_EXECUTE_READWRITE: return "PAGE_EXECUTE_READWRITE";
    case PAGE_EXECUTE_WRITECOPY: return "PAGE_EXECUTE_WRITECOPY";
    case PAGE_GUARD:             return "PAGE_GUARD";
    case 0:                      return "UNKNOWN";
    default:                     sprintf( Buffer, "0x%08X", Access );
        return Buffer;
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////// Virtual memory manipulation functions
/////////////////////////////////////////////////////////////////////

BOOLEAN
RtlpDebugPageHeapRobustProtectVM(
    IN PVOID   VirtualBase,
    IN SIZE_T  VirtualSize,
    IN ULONG   NewAccess,
    IN BOOLEAN Recursion
    )
{
    PVOID  CopyOfVirtualBase = VirtualBase;
    SIZE_T CopyOfVirtualSize = VirtualSize;
    ULONG  OldAccess;
    NTSTATUS Status;

    Status = ZwProtectVirtualMemory(
        NtCurrentProcess(),
        &CopyOfVirtualBase,
        &CopyOfVirtualSize,
        NewAccess,
        &OldAccess
        );

    if (NT_SUCCESS( Status ))
        return TRUE;

    if (! Recursion) {

        //
        //  We failed to change the protection on a range of memory.
        //  This can happen if if the range of memory spans more than
        //  one adjancent blocks allocated by separate calls to
        //  ZwAllocateVirtualMemory.  It also seems fails occasionally
        //  for reasons unknown to me, but always when attempting to
        //  change the protection on more than one page in a single call.
        //  So, fall back to changing pages individually in this range.
        //  This should be rare, so it should not be a performance problem.
        //

        PCHAR VirtualExtent = (PCHAR)ROUNDUP2((ULONG_PTR)((PCHAR)VirtualBase + VirtualSize ), PAGE_SIZE );
        PCHAR VirtualPage   = (PCHAR)((ULONG_PTR)VirtualBase & ~( PAGE_SIZE - 1 ));
        BOOLEAN SuccessAll  = TRUE;
        BOOLEAN SuccessOne;

        while (VirtualPage < VirtualExtent) {

            SuccessOne = RtlpDebugPageHeapRobustProtectVM(
                VirtualPage,
                PAGE_SIZE,
                NewAccess,
                TRUE
                );

            if (! SuccessOne) {
                SuccessAll = FALSE;
            }

            VirtualPage += PAGE_SIZE;

        }

        return SuccessAll;      // TRUE if all succeeded, FALSE if any failed
    }

    else {

        MEMORY_BASIC_INFORMATION mbi;
        CHAR OldProtectionText[ 12 ];     // big enough for "0x12345678"
        CHAR NewProtectionText[ 12 ];     // big enough for "0x12345678"

        mbi.Protect = 0;    // in case ZwQueryVirtualMemory fails

        ZwQueryVirtualMemory(
            NtCurrentProcess(),
            VirtualBase,
            MemoryBasicInformation,
            &mbi,
            sizeof( mbi ),
            NULL
            );

        DbgPrint(
            "Page heap: Failed changing VM at %08X size 0x%X\n"
            "          from %s to %s (Status %08X)\n",
            VirtualBase,
            VirtualSize,
            RtlpDebugPageHeapProtectionText( mbi.Protect, OldProtectionText ),
            RtlpDebugPageHeapProtectionText( NewAccess, NewProtectionText ),
            Status
            );
    }

    return FALSE;
}

INLINE
BOOLEAN
RtlpDebugPageHeapProtectVM(
    IN PVOID   VirtualBase,
    IN SIZE_T  VirtualSize,
    IN ULONG   NewAccess
    )
{
    return RtlpDebugPageHeapRobustProtectVM( VirtualBase, VirtualSize, NewAccess, FALSE );
}

INLINE
PVOID
RtlpDebugPageHeapAllocateVM(
    IN SIZE_T nSize
    )
{
    NTSTATUS Status;
    PVOID pVirtual;

    pVirtual = NULL;

    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
        &pVirtual,
        0,
        &nSize,
        MEM_COMMIT,
        PAGE_NOACCESS );

    return NT_SUCCESS( Status ) ? pVirtual : NULL;
}

INLINE
BOOLEAN
RtlpDebugPageHeapReleaseVM(
    IN PVOID pVirtual
    )
{
    SIZE_T nSize = 0;

    return NT_SUCCESS( RtlpHeapFreeVirtualMemory( NtCurrentProcess(),
        &pVirtual,
        &nSize,
        MEM_RELEASE ));
}

INLINE
BOOLEAN
RtlpDebugPageHeapCommitVM(
    IN PVOID pVirtual,
    IN SIZE_T nSize
    )
{
    PCHAR pStart, pEnd, pCurrent;
    NTSTATUS Status;
    SIZE_T CommitSize;
    BOOLEAN Failed = FALSE;

    pStart = (PCHAR)((ULONG_PTR)pVirtual & ~(PAGE_SIZE - 1));
    pEnd = (PCHAR)(((ULONG_PTR)pVirtual + nSize) & ~(PAGE_SIZE - 1));

    for (pCurrent = pStart; pCurrent < pEnd; pCurrent += PAGE_SIZE) {

        CommitSize = PAGE_SIZE;

        Status = ZwAllocateVirtualMemory(
            NtCurrentProcess(),
            &pCurrent,
            0,
            &CommitSize,
            MEM_COMMIT,
            PAGE_NOACCESS);

        if (! NT_SUCCESS(Status)) {

            //
            // The call can fail in low memory conditions. In this case we
            // try to recover and will probably fail the original allocation.
            //

            if ((RtlpDphDebugLevel & DPH_DEBUG_DECOMMIT_RANGES)) {

                VERIFIER_STOP (APPLICATION_VERIFIER_INTERNAL_WARNING,
                               "page heap failed to commit memory",
                               pCurrent, "",
                               CommitSize, "",
                               Status, "",
                               0, "");
            }

            Failed = TRUE;
            break;
        }
    }


    if (Failed) {

        //
        // We need to roll back whatever succeeded.
        //

        for (pCurrent -= PAGE_SIZE; pCurrent >= pStart && pCurrent < pEnd; pCurrent -= PAGE_SIZE) {

            CommitSize = PAGE_SIZE;

            Status = RtlpHeapFreeVirtualMemory(
                NtCurrentProcess(),
                &pCurrent,
                &CommitSize,
                MEM_DECOMMIT);

            if (! NT_SUCCESS(Status)) {

                //
                // There is now valid reason known to me for a correct free operation
                // failure. So, in this case we make a little bit of fuss about it.
                //

                VERIFIER_STOP (APPLICATION_VERIFIER_INTERNAL_WARNING,
                               "page heap failed to decommit memory",
                               pCurrent, "",
                               CommitSize, "",
                               Status, "",
                               0, "");
            }
        }
    }

    if (Failed) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

INLINE
BOOLEAN
RtlpDebugPageHeapDecommitVM(
    IN PVOID pVirtual,
    IN SIZE_T nSize
    )
{
    PCHAR pStart, pEnd, pCurrent;
    NTSTATUS Status;
    SIZE_T DecommitSize;
    BOOLEAN Failed = FALSE;

    pStart = (PCHAR)((ULONG_PTR)pVirtual & ~(PAGE_SIZE - 1));
    pEnd = (PCHAR)(((ULONG_PTR)pVirtual + nSize) & ~(PAGE_SIZE - 1));

    for (pCurrent = pStart; pCurrent < pEnd; pCurrent += PAGE_SIZE) {

        DecommitSize = PAGE_SIZE;

        Status = RtlpHeapFreeVirtualMemory(
            NtCurrentProcess(),
            &pCurrent,
            &DecommitSize,
            MEM_DECOMMIT);

        if (! NT_SUCCESS(Status)) {

            //
            // There is now valid reason known to me for a correct free operation
            // failure. So, in this case we make a little bit of fuss about it.
            //

            VERIFIER_STOP (APPLICATION_VERIFIER_INTERNAL_WARNING,
                           "page heap failed to commit memory",
                           pCurrent, "",
                           DecommitSize, "",
                           Status, "",
                           0, "");

            Failed = TRUE;
        }
    }

    if (Failed) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

/////////////////////////////////////////////////////////////////////
//////////////////////////////////////// Internal page heap functions
/////////////////////////////////////////////////////////////////////

PDPH_HEAP_BLOCK
RtlpDebugPageHeapTakeNodeFromUnusedList(
    IN PDPH_HEAP_ROOT pHeap
    )
{
    PDPH_HEAP_BLOCK pNode = pHeap->pUnusedNodeListHead;
    PDPH_HEAP_BLOCK pPrev = NULL;

    //
    //  UnusedNodeList is LIFO with most recent entry at head of list.
    //

    if (pNode) {

        DEQUEUE_NODE( pNode, pPrev, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

        pHeap->nUnusedNodes -= 1;

    }

    return pNode;
}

VOID
RtlpDebugPageHeapReturnNodeToUnusedList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode
    )
{
    //
    //  UnusedNodeList is LIFO with most recent entry at head of list.
    //

    ENQUEUE_HEAD( pNode, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

    pHeap->nUnusedNodes += 1;
}

PDPH_HEAP_BLOCK
RtlpDebugPageHeapFindBusyMem(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  PVOID                 pUserMem,
    OUT PDPH_HEAP_BLOCK *pPrevAlloc
    )
{
    PDPH_HEAP_BLOCK pNode = pHeap->pBusyAllocationListHead;
    PDPH_HEAP_BLOCK pPrev = NULL;

    while (pNode != NULL) {

        if (pNode->pUserAllocation == pUserMem) {

            if (pPrevAlloc)
                *pPrevAlloc = pPrev;

            return pNode;
        }

        pPrev = pNode;
        pNode = pNode->pNextAlloc;
    }

    return NULL;
}

VOID
RtlpDebugPageHeapRemoveFromAvailableList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode,
    IN PDPH_HEAP_BLOCK pPrev
    )
{
    DEQUEUE_NODE( pNode, pPrev, pHeap->pAvailableAllocationListHead, pHeap->pAvailableAllocationListTail );

    pHeap->nAvailableAllocations -= 1;
    pHeap->nAvailableAllocationBytesCommitted -= pNode->nVirtualBlockSize;
}

VOID
RtlpDebugPageHeapPlaceOnFreeList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pAlloc
    )
{
    //
    //  FreeAllocationList is stored FIFO to enhance finding
    //  reference-after-freed bugs by keeping previously freed
    //  allocations on the free list as long as possible.
    //

    pAlloc->pNextAlloc = NULL;

    ENQUEUE_TAIL( pAlloc, pHeap->pFreeAllocationListHead, pHeap->pFreeAllocationListTail );

    pHeap->nFreeAllocations += 1;
    pHeap->nFreeAllocationBytesCommitted += pAlloc->nVirtualBlockSize;
}

VOID
RtlpDebugPageHeapRemoveFromFreeList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode,
    IN PDPH_HEAP_BLOCK pPrev
    )
{
    DEQUEUE_NODE( pNode, pPrev, pHeap->pFreeAllocationListHead, pHeap->pFreeAllocationListTail );

    pHeap->nFreeAllocations -= 1;
    pHeap->nFreeAllocationBytesCommitted -= pNode->nVirtualBlockSize;

    pNode->StackTrace = NULL;
}

VOID
RtlpDebugPageHeapPlaceOnVirtualList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode
    )
{
    //
    //  VirtualStorageList is LIFO so that releasing VM blocks will
    //  occur in exact reverse order.
    //

    ENQUEUE_HEAD( pNode, pHeap->pVirtualStorageListHead, pHeap->pVirtualStorageListTail );

    pHeap->nVirtualStorageRanges += 1;
    pHeap->nVirtualStorageBytes += pNode->nVirtualBlockSize;
}

VOID
RtlpDebugPageHeapPlaceOnBusyList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode
    )
{
    //
    //  BusyAllocationList is LIFO to achieve better temporal locality
    //  of reference (older allocations are farther down the list).
    //

    ENQUEUE_HEAD( pNode, pHeap->pBusyAllocationListHead, pHeap->pBusyAllocationListTail );

    pHeap->nBusyAllocations += 1;
    pHeap->nBusyAllocationBytesCommitted  += pNode->nVirtualBlockSize;
    pHeap->nBusyAllocationBytesAccessible += pNode->nVirtualAccessSize;
}

VOID
RtlpDebugPageHeapRemoveFromBusyList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode,
    IN PDPH_HEAP_BLOCK pPrev
    )
{
    DEQUEUE_NODE( pNode, pPrev, pHeap->pBusyAllocationListHead, pHeap->pBusyAllocationListTail );

    pHeap->nBusyAllocations -= 1;
    pHeap->nBusyAllocationBytesCommitted  -= pNode->nVirtualBlockSize;
    pHeap->nBusyAllocationBytesAccessible -= pNode->nVirtualAccessSize;
}

PDPH_HEAP_BLOCK
RtlpDebugPageHeapSearchAvailableMemListForBestFit(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  SIZE_T                nSize,
    OUT PDPH_HEAP_BLOCK *pPrevAvailNode
    )
{
    PDPH_HEAP_BLOCK pAvail, pFound, pAvailPrev, pFoundPrev;
    SIZE_T nAvail, nFound;

    nFound     = 0x7FFFFFFF;
    pFound     = NULL;
    pFoundPrev = NULL;
    pAvailPrev = NULL;
    pAvail     = pHeap->pAvailableAllocationListHead;

    while (( pAvail != NULL ) && ( nFound > nSize )) {

        nAvail = pAvail->nVirtualBlockSize;

        if (( nAvail >= nSize ) && ( nAvail < nFound )) {
            nFound     = nAvail;
            pFound     = pAvail;
            pFoundPrev = pAvailPrev;
        }

        pAvailPrev = pAvail;
        pAvail     = pAvail->pNextAlloc;
    }

    *pPrevAvailNode = pFoundPrev;
    return pFound;
}

//
// Counters for # times coalesce operations got rejected
// to avoid cross-VAD issues.
//

LONG RtlpDphCoalesceStatistics [4];

#define ALIGN_TO_SIZE(P, Sz) (((ULONG_PTR)(P)) & ~((ULONG_PTR)(Sz) - 1))

BOOLEAN
RtlpDphSameVirtualRegion (
    IN PDPH_HEAP_BLOCK Left,
    IN PDPH_HEAP_BLOCK Right
    )
/*++

Routine description:

    This function tries to figure out if two nodes are part of the
    same VAD. The function is used during coalescing in order to avoid
    merging together blocks from different VADs. If we do not do this
    we will break applications that do GDI calls.

    SilviuC: this can be done differently if we keep the VAD address in 
    every node and make sure to propagate the value when nodes get split.
    Then this function will just be a comparison of the two values.
        
--*/
{
    PVOID LeftRegion;
    MEMORY_BASIC_INFORMATION MemoryInfo;
    NTSTATUS Status;
    SIZE_T ReturnLength;

    //
    // If blocks are in the same 64K chunk we are okay.
    //

    if (ALIGN_TO_SIZE(Left->pVirtualBlock, VM_UNIT_SIZE) 
        == ALIGN_TO_SIZE(Right->pVirtualBlock, VM_UNIT_SIZE)) {

        InterlockedIncrement (&(RtlpDphCoalesceStatistics[2]));
        return TRUE;
    }

    //
    // Call query() to find out what is the start address of the
    // VAD for each node.
    //

    Status = ZwQueryVirtualMemory (NtCurrentProcess(),
                                   Left->pVirtualBlock,
                                   MemoryBasicInformation,
                                   &MemoryInfo,
                                   sizeof MemoryInfo,
                                   &ReturnLength);
    
    if (! NT_SUCCESS(Status)) {
        InterlockedIncrement (&(RtlpDphCoalesceStatistics[3]));
        return FALSE;
    }

    LeftRegion = MemoryInfo.AllocationBase;

    Status = ZwQueryVirtualMemory (NtCurrentProcess(),
                                   Right->pVirtualBlock,
                                   MemoryBasicInformation,
                                   &MemoryInfo,
                                   sizeof MemoryInfo,
                                   &ReturnLength);
    
    if (! NT_SUCCESS(Status)) {
        InterlockedIncrement (&(RtlpDphCoalesceStatistics[3]));
        return FALSE;
    }

    if (LeftRegion == MemoryInfo.AllocationBase) {
        
        InterlockedIncrement (&(RtlpDphCoalesceStatistics[0]));
        return TRUE;
    }
    else {

        InterlockedIncrement (&(RtlpDphCoalesceStatistics[1]));
        return FALSE;
    }
}


VOID
RtlpDebugPageHeapCoalesceNodeIntoAvailable(
    IN PDPH_HEAP_ROOT pHeap,
    IN PDPH_HEAP_BLOCK pNode
    )
{
    PDPH_HEAP_BLOCK pPrev;
    PDPH_HEAP_BLOCK pNext;
    PUCHAR pVirtual;
    SIZE_T nVirtual;

    pPrev = NULL;
    pNext = pHeap->pAvailableAllocationListHead;

    pVirtual = pNode->pVirtualBlock;
    nVirtual = pNode->nVirtualBlockSize;

    pHeap->nAvailableAllocationBytesCommitted += nVirtual;
    pHeap->nAvailableAllocations += 1;

    //
    //  Walk list to insertion point.
    //

    while (( pNext ) && ( pNext->pVirtualBlock < pVirtual )) {
        pPrev = pNext;
        pNext = pNext->pNextAlloc;
    }

    if (pPrev) {

        if (((pPrev->pVirtualBlock + pPrev->nVirtualBlockSize) == pVirtual) && 
             RtlpDphSameVirtualRegion (pPrev, pNode)) {

            //
            //  pPrev and pNode are adjacent, so simply add size of
            //  pNode entry to pPrev entry.
            //

            pPrev->nVirtualBlockSize += nVirtual;

            RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNode );

            pHeap->nAvailableAllocations--;

            pNode    = pPrev;
            pVirtual = pPrev->pVirtualBlock;
            nVirtual = pPrev->nVirtualBlockSize;

        }

        else {

            //
            //  pPrev and pNode are not adjacent, so insert the pNode
            //  block into the list after pPrev.
            //

            pNode->pNextAlloc = pPrev->pNextAlloc;
            pPrev->pNextAlloc = pNode;

        }
    }

    else {

        //
        //  pNode should be inserted at head of list.
        //

        pNode->pNextAlloc = pHeap->pAvailableAllocationListHead;
        pHeap->pAvailableAllocationListHead = pNode;

    }


    if (pNext) {

        if (((pVirtual + nVirtual) == pNext->pVirtualBlock) &&
             RtlpDphSameVirtualRegion (pNode, pNext)) { 

            //
            //  pNode and pNext are adjacent, so simply add size of
            //  pNext entry to pNode entry and remove pNext entry
            //  from the list.
            //

            pNode->nVirtualBlockSize += pNext->nVirtualBlockSize;

            pNode->pNextAlloc = pNext->pNextAlloc;

            if (pHeap->pAvailableAllocationListTail == pNext) {
                pHeap->pAvailableAllocationListTail = pNode;
            }

            RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNext );

            pHeap->nAvailableAllocations--;

        }
    }

    else {

        //
        //  pNode is tail of list.
        //

        pHeap->pAvailableAllocationListTail = pNode;

    }
}


VOID
RtlpDebugPageHeapCoalesceFreeIntoAvailable(
    IN PDPH_HEAP_ROOT pHeap,
    IN ULONG          nLeaveOnFreeList
    )
{
    PDPH_HEAP_BLOCK pNode = pHeap->pFreeAllocationListHead;
    SIZE_T               nFree = pHeap->nFreeAllocations;
    PDPH_HEAP_BLOCK pNext;

    DEBUG_ASSERT( nFree >= nLeaveOnFreeList );

    while (( pNode ) && ( nFree-- > nLeaveOnFreeList )) {

        pNext = pNode->pNextAlloc;  // preserve next pointer across shuffling

        RtlpDebugPageHeapRemoveFromFreeList( pHeap, pNode, NULL );

        RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );

        pNode = pNext;

    }

    DEBUG_ASSERT(( nFree = (volatile SIZE_T)( pHeap->nFreeAllocations )) >= nLeaveOnFreeList );
    DEBUG_ASSERT(( pNode != NULL ) || ( nFree == 0 ));

}

// forward
BOOLEAN
RtlpDebugPageHeapGrowVirtual(
    IN PDPH_HEAP_ROOT pHeap,
    IN SIZE_T         nSize
    );

PDPH_HEAP_BLOCK
RtlpDebugPageHeapFindAvailableMem(
    IN  PDPH_HEAP_ROOT        pHeap,
    IN  SIZE_T                nSize,
    OUT PDPH_HEAP_BLOCK *pPrevAvailNode,
    IN  BOOLEAN               bGrowVirtual
    )
{
    PDPH_HEAP_BLOCK pAvail;
    ULONG                nLeaveOnFreeList;

    //
    // If we use uncommitted ranges it is really important to
    // call FindAvailableMemory only with page aligned sizes.
    //

    if ((pHeap->ExtraFlags & PAGE_HEAP_SMART_MEMORY_USAGE)) {
        DEBUG_ASSERT ((nSize & ~(PAGE_SIZE - 1)) == nSize);
    }

    //
    //  First search existing AvailableList for a "best-fit" block
    //  (the smallest block that will satisfy the request).
    //

    pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
        pHeap,
        nSize,
        pPrevAvailNode
        );

    while (( pAvail == NULL ) && ( pHeap->nFreeAllocations > MIN_FREE_LIST_LENGTH )) {

        //
        //  Failed to find sufficient memory on AvailableList.  Coalesce
        //  3/4 of the FreeList memory to the AvailableList and try again.
        //  Continue this until we have sufficient memory in AvailableList,
        //  or the FreeList length is reduced to MIN_FREE_LIST_LENGTH entries.
        //  We don't shrink the FreeList length below MIN_FREE_LIST_LENGTH
        //  entries to preserve the most recent MIN_FREE_LIST_LENGTH entries
        //  for reference-after-freed purposes.
        //

        nLeaveOnFreeList = pHeap->nFreeAllocations / 4;

        if (nLeaveOnFreeList < MIN_FREE_LIST_LENGTH)
            nLeaveOnFreeList = MIN_FREE_LIST_LENGTH;

        RtlpDebugPageHeapCoalesceFreeIntoAvailable( pHeap, nLeaveOnFreeList );

        pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
            pHeap,
            nSize,
            pPrevAvailNode
            );

    }


    if (( pAvail == NULL ) && ( bGrowVirtual )) {

        //
        //  After coalescing FreeList into AvailableList, still don't have
        //  enough memory (large enough block) to satisfy request, so we
        //  need to allocate more VM.
        //

        if (RtlpDebugPageHeapGrowVirtual( pHeap, nSize )) {

            pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                pHeap,
                nSize,
                pPrevAvailNode
                );

            if (pAvail == NULL) {

                //
                //  Failed to satisfy request with more VM.  If remainder
                //  of free list combined with available list is larger
                //  than the request, we might still be able to satisfy
                //  the request by merging all of the free list onto the
                //  available list.  Note we lose our MIN_FREE_LIST_LENGTH
                //  reference-after-freed insurance in this case, but it
                //  is a rare case, and we'd prefer to satisfy the allocation.
                //

                if (( pHeap->nFreeAllocationBytesCommitted +
                    pHeap->nAvailableAllocationBytesCommitted ) >= nSize) {

                    RtlpDebugPageHeapCoalesceFreeIntoAvailable( pHeap, 0 );

                    pAvail = RtlpDebugPageHeapSearchAvailableMemListForBestFit(
                        pHeap,
                        nSize,
                        pPrevAvailNode
                        );
                }
            }
        }
    }

    //
    // If we use uncommitted ranges we need to commit the memory
    // range now. Note that the memory will be committed but
    // the protection on it will be N/A.
    //

    if (pAvail && (pHeap->ExtraFlags & PAGE_HEAP_SMART_MEMORY_USAGE)) {

        BOOLEAN Success;

        //
        // (SilviuC): The memory here might be already committed if we use
        // it for the first time. Whenever we allocate virtual memory to grow
        // the heap we commit it. This is the reason the consumption does not
        // decrease as spectacular as we expected. We will need to fix it in
        // the future. It affects 0x43 flags.
        //

        Success = RtlpDebugPageHeapCommitVM (pAvail->pVirtualBlock, nSize);

        if (!Success) {

            //
            // We did not manage to commit memory for this block. This
            // can happen in low memory conditions. We will return null.
            // There is no need to do anything with the node we obtained.
            // It is already in the Available list where it should be anyway.

            return NULL;
        }
    }

    return pAvail;
}

VOID
RtlpDebugPageHeapPlaceOnPoolList(
    IN PDPH_HEAP_ROOT       pHeap,
    IN PDPH_HEAP_BLOCK pNode
    )
{

    //
    //  NodePoolList is FIFO.
    //

    pNode->pNextAlloc = NULL;

    ENQUEUE_TAIL( pNode, pHeap->pNodePoolListHead, pHeap->pNodePoolListTail );

    pHeap->nNodePoolBytes += pNode->nVirtualBlockSize;
    pHeap->nNodePools     += 1;

}

VOID
RtlpDebugPageHeapAddNewPool(
    IN PDPH_HEAP_ROOT pHeap,
    IN PVOID          pVirtual,
    IN SIZE_T         nSize,
    IN BOOLEAN        bAddToPoolList
    )
{
    PDPH_HEAP_BLOCK pNode, pFirst;
    ULONG n, nCount;

    //
    //  Assume pVirtual points to committed block of nSize bytes.
    //

    pFirst = pVirtual;
    nCount = (ULONG)(nSize  / sizeof( DPH_HEAP_BLOCK ));

    for (n = nCount - 1, pNode = pFirst; n > 0; pNode++, n--)
        pNode->pNextAlloc = pNode + 1;

    pNode->pNextAlloc = NULL;

    //
    //  Now link this list into the tail of the UnusedNodeList
    //

    ENQUEUE_TAIL( pFirst, pHeap->pUnusedNodeListHead, pHeap->pUnusedNodeListTail );

    pHeap->pUnusedNodeListTail = pNode;

    pHeap->nUnusedNodes += nCount;

    if (bAddToPoolList) {

        //
        //  Now add an entry on the PoolList by taking a node from the
        //  UnusedNodeList, which should be guaranteed to be non-empty
        //  since we just added new nodes to it.
        //

        pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );

        DEBUG_ASSERT( pNode != NULL );

        pNode->pVirtualBlock     = pVirtual;
        pNode->nVirtualBlockSize = nSize;

        RtlpDebugPageHeapPlaceOnPoolList( pHeap, pNode );

    }
}

PDPH_HEAP_BLOCK
RtlpDebugPageHeapAllocateNode(
    IN PDPH_HEAP_ROOT pHeap
    )
{
    PDPH_HEAP_BLOCK pNode, pPrev, pReturn;
    PUCHAR pVirtual;
    SIZE_T nVirtual;
    SIZE_T nRequest;

    DEBUG_ASSERT( ! pHeap->InsideAllocateNode );
    DEBUG_CODE( pHeap->InsideAllocateNode = TRUE );

    pReturn = NULL;

    if (pHeap->pUnusedNodeListHead == NULL) {

        //
        //  We're out of nodes -- allocate new node pool
        //  from AvailableList.  Set bGrowVirtual to FALSE
        //  since growing virtual will require new nodes, causing
        //  recursion.  Note that simply calling FindAvailableMem
        //  might return some nodes to the pUnusedNodeList, even if
        //  the call fails, so we'll check that the UnusedNodeList
        //  is still empty before we try to use or allocate more
        //  memory.
        //

        nRequest = POOL_SIZE;

        pNode = RtlpDebugPageHeapFindAvailableMem(
            pHeap,
            nRequest,
            &pPrev,
            FALSE
            );

        if (( pHeap->pUnusedNodeListHead == NULL ) && ( pNode == NULL )) {

            //
            //  Reduce request size to PAGE_SIZE and see if
            //  we can find at least a page on the available
            //  list.
            //

            nRequest = PAGE_SIZE;

            pNode = RtlpDebugPageHeapFindAvailableMem(
                pHeap,
                nRequest,
                &pPrev,
                FALSE
                );

        }

        if (pHeap->pUnusedNodeListHead == NULL) {

            if (pNode == NULL) {

                //
                //  Insufficient memory on Available list.  Try allocating a
                //  new virtual block.
                //

                nRequest = POOL_SIZE;
                nVirtual = RESERVE_SIZE;
                pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

                if (pVirtual == NULL) {

                    //
                    //  Unable to allocate full RESERVE_SIZE block,
                    //  so reduce request to single VM unit (64K)
                    //  and try again.
                    //

                    nVirtual = VM_UNIT_SIZE;
                    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

                    if (pVirtual == NULL) {

                        //
                        //  Can't allocate any VM.
                        //

                        goto EXIT;
                    }
                }
            }

            else {

                RtlpDebugPageHeapRemoveFromAvailableList( pHeap, pNode, pPrev );

                pVirtual = pNode->pVirtualBlock;
                nVirtual = pNode->nVirtualBlockSize;

            }

            //
            //  We now have allocated VM referenced by pVirtual,nVirtual.
            //  Make nRequest portion of VM accessible for new node pool.
            //

            if (! RtlpDebugPageHeapProtectVM( pVirtual, nRequest, PAGE_READWRITE )) {

                if (pNode == NULL) {
                    RtlpDebugPageHeapReleaseVM( pVirtual );
                }
                else {
                    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );
                }

                goto EXIT;
            }

            //
            //  Now we have accessible memory for new pool.  Add the
            //  new memory to the pool.  If the new memory came from
            //  AvailableList versus fresh VM, zero the memory first.
            //

            if (pNode != NULL) {
                RtlZeroMemory( pVirtual, nRequest );
            }

            RtlpDebugPageHeapAddNewPool( pHeap, pVirtual, nRequest, TRUE );

            //
            //  If any memory remaining, put it on available list.
            //

            if (pNode == NULL) {

                //
                //  Memory came from new VM -- add appropriate list entries
                //  for new VM and add remainder of VM to free list.
                //

                pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
                DEBUG_ASSERT( pNode != NULL );
                pNode->pVirtualBlock     = pVirtual;
                pNode->nVirtualBlockSize = nVirtual;
                RtlpDebugPageHeapPlaceOnVirtualList( pHeap, pNode );

                pNode = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
                DEBUG_ASSERT( pNode != NULL );
                pNode->pVirtualBlock     = pVirtual + nRequest;
                pNode->nVirtualBlockSize = nVirtual - nRequest;

                RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );

            }

            else {

                if (pNode->nVirtualBlockSize > nRequest) {

                    pNode->pVirtualBlock     += nRequest;
                    pNode->nVirtualBlockSize -= nRequest;

                    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pNode );
                }

                else {

                    //
                    //  Used up entire available block -- return node to
                    //  unused list.
                    //

                    RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pNode );

                }
            }
        }
    }

    pReturn = RtlpDebugPageHeapTakeNodeFromUnusedList( pHeap );
    DEBUG_ASSERT( pReturn != NULL );

    EXIT:

    DEBUG_CODE( pHeap->InsideAllocateNode = FALSE );
    return pReturn;
}

BOOLEAN
RtlpDebugPageHeapGrowVirtual(
    IN PDPH_HEAP_ROOT pHeap,
    IN SIZE_T         nSize
    )
{
    PDPH_HEAP_BLOCK pVirtualNode;
    PDPH_HEAP_BLOCK pAvailNode;
    PVOID  pVirtual;
    SIZE_T nVirtual;

    pVirtualNode = RtlpDebugPageHeapAllocateNode( pHeap );

    if (pVirtualNode == NULL) {
        return FALSE;
    }

    pAvailNode = RtlpDebugPageHeapAllocateNode( pHeap );

    if (pAvailNode == NULL) {
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pVirtualNode );
        return FALSE;
    }

    nSize    = ROUNDUP2( nSize, VM_UNIT_SIZE );
    nVirtual = ( nSize > RESERVE_SIZE ) ? nSize : RESERVE_SIZE;
    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

    if (( pVirtual == NULL ) && ( nSize < RESERVE_SIZE )) {
        nVirtual = nSize;
        pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );
    }

    if (pVirtual == NULL) {
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pVirtualNode );
        RtlpDebugPageHeapReturnNodeToUnusedList( pHeap, pAvailNode );
        return FALSE;
    }

    pVirtualNode->pVirtualBlock     = pVirtual;
    pVirtualNode->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapPlaceOnVirtualList( pHeap, pVirtualNode );

    pAvailNode->pVirtualBlock     = pVirtual;
    pAvailNode->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapCoalesceNodeIntoAvailable( pHeap, pAvailNode );

    return TRUE;
}

VOID
RtlpDebugPageHeapProtectStructures(
    IN PDPH_HEAP_ROOT pHeap
    )
{
#if 0
    
    PDPH_HEAP_BLOCK pNode;

    //
    //  Assume CritSect is owned so we're the only thread twiddling
    //  the protection.
    //

    DEBUG_ASSERT( pHeap->HeapFlags & HEAP_PROTECTION_ENABLED );

    if (--pHeap->nUnProtectionReferenceCount == 0) {

        pNode = pHeap->pNodePoolListHead;

        while (pNode != NULL) {

            RtlpDebugPageHeapProtectVM( pNode->pVirtualBlock,
                pNode->nVirtualBlockSize,
                PAGE_READONLY );

            pNode = pNode->pNextAlloc;

        }
    }

    //
    // Protect the main NT heap structure associated with page heap.
    // Nobody should touch this outside of page heap code paths.
    //
    
    RtlpDebugPageHeapProtectVM (pHeap->NormalHeap,
                                PAGE_SIZE,
                                PAGE_READONLY);
#endif
}


VOID
RtlpDebugPageHeapUnProtectStructures(
    IN PDPH_HEAP_ROOT pHeap
    )
{
#if 0
    
    PDPH_HEAP_BLOCK pNode;

    DEBUG_ASSERT( pHeap->HeapFlags & HEAP_PROTECTION_ENABLED );

    if (pHeap->nUnProtectionReferenceCount == 0) {

        pNode = pHeap->pNodePoolListHead;

        while (pNode != NULL) {

            RtlpDebugPageHeapProtectVM( pNode->pVirtualBlock,
                pNode->nVirtualBlockSize,
                PAGE_READWRITE );

            pNode = pNode->pNextAlloc;

        }
    }

    //
    // Unprotect the main NT heap structure associatied with page heap.
    //
    
    RtlpDebugPageHeapProtectVM (pHeap->NormalHeap,
                                PAGE_SIZE,
                                PAGE_READWRITE);

    pHeap->nUnProtectionReferenceCount += 1;

#endif
}


VOID
RtlpDphPreProcessing (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags
    )
{
    RtlpDebugPageHeapEnterCritSect (Heap, Flags);
    DEBUG_CODE (RtlpDebugPageHeapVerifyIntegrity (Heap));
    UNPROTECT_HEAP_STRUCTURES (Heap);

#if 0
    RtlpDphValidateInternalLists (Heap);
#endif
}


VOID
RtlpDphPostProcessing (
    PDPH_HEAP_ROOT Heap
    )
{
#if 0
    RtlpDphValidateInternalLists (Heap);
#endif

    PROTECT_HEAP_STRUCTURES (Heap);
    DEBUG_CODE (RtlpDebugPageHeapVerifyIntegrity (Heap));
    RtlpDebugPageHeapLeaveCritSect (Heap);
}


/////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Exception management
/////////////////////////////////////////////////////////////////////

#define EXN_STACK_OVERFLOW   0
#define EXN_NO_MEMORY        1
#define EXN_ACCESS_VIOLATION 2
#define EXN_IGNORE_AV        3
#define EXN_OTHER            4

ULONG RtlpDphException[8];


ULONG
RtlpDphUnexpectedExceptionFilter (
    ULONG ExceptionCode,
    PVOID ExceptionRecord,
    PDPH_HEAP_ROOT Heap,
    BOOLEAN IgnoreAccessViolations
    )
/*++

Routine Description:

    This routine is the exception filter used by page heap operations. The role
    of the function is to bring the page heap in a consistent state (unlock
    heap lock, protect page heap metadata, etc.) if an exception has been raised.
    The exception can be raised for legitimate reasons (e.g. STATUS_NO_MEMORY 
    from HeapAlloc()) or because there is some sort of corruption. 
    
    Legitimate exceptions do not cause breaks but an unrecognized exception will 
    cause a break. The break is continuable at least with respect to page heap.


Arguments:

    ExceptionCode - exception code
    ExceptionRecord - structure with pointers to .exr and .cxr
    Heap - heap in which code was executing at the time of exception
    IgnoreAccessViolations - sometimes we want to ignore this (e.g. HeapSize).

Return Value:

    Always EXCEPTION_CONTINUE_SEARCH. The philosophy of this exception filter
    function is that if we get an exception we bring back page heap in a consistent
    state and then let the exception go to the next exception handler.


Environment:

    Called within page heap APIs if an exception is raised. 
--*/
{
    if (ExceptionCode == STATUS_NO_MEMORY) {

        //
        // Underlying NT heap functions can legitimately raise this
        // exception. 
        //


        InterlockedIncrement (&(RtlpDphException[EXN_NO_MEMORY]));
    }
    else if (Heap != NULL && ExceptionCode == STATUS_STACK_OVERFLOW) {

        //
        // We go to the next exception handler for stack overflows.
        //

        InterlockedIncrement (&(RtlpDphException[EXN_STACK_OVERFLOW]));
    }
    else if (ExceptionCode == STATUS_ACCESS_VIOLATION) {

        if (IgnoreAccessViolations == FALSE) {
            
            VERIFIER_STOP (APPLICATION_VERIFIER_UNEXPECTED_EXCEPTION,
                           "unexpected exception raised in heap code path",
                           Heap, "Heap handle involved",
                           ExceptionCode, "Exception code",
                           ExceptionRecord, "Exception record (.exr on 1st word, .cxr on 2nd word)",
                           0, "");
            
            InterlockedIncrement (&(RtlpDphException[EXN_ACCESS_VIOLATION]));
        }
        else {
            
            InterlockedIncrement (&(RtlpDphException[EXN_IGNORE_AV]));
        }
    }
    else {

        //
        // Any other exceptions will go to the next exception handler.
        //

        InterlockedIncrement (&(RtlpDphException[EXN_OTHER]));
    }

    RtlpDphPostProcessing (Heap);

    return EXCEPTION_CONTINUE_SEARCH;
}

#if DBG
#define ASSERT_UNEXPECTED_CODE_PATH() ASSERT(0 && "unexpected code path")
#else
#define ASSERT_UNEXPECTED_CODE_PATH()
#endif

/////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// Internal debug functions
/////////////////////////////////////////////////////////////////////

#if INTERNAL_DEBUG

VOID
RtlpDebugPageHeapVerifyList(
    IN PDPH_HEAP_BLOCK pListHead,
    IN PDPH_HEAP_BLOCK pListTail,
    IN SIZE_T               nExpectedLength,
    IN SIZE_T               nExpectedVirtual,
    IN PCCH                 pListName
    )
{
    PDPH_HEAP_BLOCK pPrev = NULL;
    PDPH_HEAP_BLOCK pNode = pListHead;
    PDPH_HEAP_BLOCK pTest = pListHead ? pListHead->pNextAlloc : NULL;
    ULONG                nNode = 0;
    SIZE_T               nSize = 0;

    while (pNode) {

        if (pNode == pTest) {
            DbgPrint( "Page heap: Internal %s list is circular\n", pListName );
            DbgBreakPoint ();
            return;
        }

        nNode += 1;
        nSize += pNode->nVirtualBlockSize;

        if (pTest) {
            pTest = pTest->pNextAlloc;
            if (pTest) {
                pTest = pTest->pNextAlloc;
            }
        }

        pPrev = pNode;
        pNode = pNode->pNextAlloc;

    }

    if (pPrev != pListTail) {
        DbgPrint( "Page heap: Internal %s list has incorrect tail pointer\n", pListName );
        DbgBreakPoint ();
    }

    if (( nExpectedLength != 0xFFFFFFFF ) && ( nExpectedLength != nNode )) {
        DbgPrint( "Page heap: Internal %s list has incorrect length\n", pListName );
        DbgBreakPoint ();
    }

    if (( nExpectedVirtual != 0xFFFFFFFF ) && ( nExpectedVirtual != nSize )) {
        DbgPrint( "Page heap: Internal %s list has incorrect virtual size\n", pListName );
        DbgBreakPoint ();
    }

}

VOID
RtlpDebugPageHeapVerifyIntegrity(
    IN PDPH_HEAP_ROOT pHeap
    )
{

    RtlpDebugPageHeapVerifyList(
        pHeap->pVirtualStorageListHead,
        pHeap->pVirtualStorageListTail,
        pHeap->nVirtualStorageRanges,
        pHeap->nVirtualStorageBytes,
        "VIRTUAL"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pBusyAllocationListHead,
        pHeap->pBusyAllocationListTail,
        pHeap->nBusyAllocations,
        pHeap->nBusyAllocationBytesCommitted,
        "BUSY"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pFreeAllocationListHead,
        pHeap->pFreeAllocationListTail,
        pHeap->nFreeAllocations,
        pHeap->nFreeAllocationBytesCommitted,
        "FREE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pAvailableAllocationListHead,
        pHeap->pAvailableAllocationListTail,
        pHeap->nAvailableAllocations,
        pHeap->nAvailableAllocationBytesCommitted,
        "AVAILABLE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pUnusedNodeListHead,
        pHeap->pUnusedNodeListTail,
        pHeap->nUnusedNodes,
        0xFFFFFFFF,
        "FREENODE"
        );

    RtlpDebugPageHeapVerifyList(
        pHeap->pNodePoolListHead,
        pHeap->pNodePoolListTail,
        pHeap->nNodePools,
        pHeap->nNodePoolBytes,
        "NODEPOOL"
        );
}

#endif // #if INTERNAL_DEBUG

/////////////////////////////////////////////////////////////////////
///////////////////////////// Exported page heap management functions
/////////////////////////////////////////////////////////////////////

//
//  Here's where the exported interface functions are defined.
//

#if (( DPH_CAPTURE_STACK_TRACE ) && ( i386 ) && ( FPO ))
#pragma optimize( "y", off )    // disable FPO for consistent stack traces
#endif

PVOID
RtlpDebugPageHeapCreate(
    IN ULONG  Flags,
    IN PVOID  HeapBase    OPTIONAL,
    IN SIZE_T ReserveSize OPTIONAL,
    IN SIZE_T CommitSize  OPTIONAL,
    IN PVOID  Lock        OPTIONAL,
    IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL
    )
{
    SYSTEM_BASIC_INFORMATION SystemInfo;
    PDPH_HEAP_BLOCK     Node;
    PDPH_HEAP_ROOT           HeapRoot;
    PVOID                    HeapHandle;
    PUCHAR                   pVirtual;
    SIZE_T                   nVirtual;
    SIZE_T                   Size;
    NTSTATUS                 Status;

    //
    // If `Parameters' is -1 then this is a recursive call to
    // RtlpDebugPageHeapCreate and we will return NULL so that
    // the normal heap manager will create a normal heap.
    // I agree this is a hack but we need this so that we maintain
    // a very loose dependency between the normal and page heap
    // manager.
    //

    if ((SIZE_T)Parameters == (SIZE_T)-1) {
        return NULL;
    }

    //
    //  We don't handle heaps where HeapBase is already allocated
    //  from user or where Lock is provided by user.
    //

    DEBUG_ASSERT( HeapBase == NULL );
    DEBUG_ASSERT( Lock == NULL );

    if (( HeapBase != NULL ) || ( Lock != NULL ))
        return NULL;

    //
    //  Note that we simply ignore ReserveSize, CommitSize, and
    //  Parameters as we always have a growable heap with our
    //  own thresholds, etc.
    //

    ZwQuerySystemInformation( SystemBasicInformation,
        &SystemInfo,
        sizeof( SystemInfo ),
        NULL );

    RETAIL_ASSERT( SystemInfo.PageSize == PAGE_SIZE );
    RETAIL_ASSERT( SystemInfo.AllocationGranularity == VM_UNIT_SIZE );
    DEBUG_ASSERT(( PAGE_SIZE + POOL_SIZE + PAGE_SIZE ) < VM_UNIT_SIZE );

    nVirtual = RESERVE_SIZE;
    pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

    if (pVirtual == NULL) {

        nVirtual = VM_UNIT_SIZE;
        pVirtual = RtlpDebugPageHeapAllocateVM( nVirtual );

        if (pVirtual == NULL) {

            OUT_OF_VM_BREAK( Flags, "Page heap: Insufficient memory to create heap\n" );
            IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
            return NULL;
        }
    }

    if (! RtlpDebugPageHeapProtectVM( pVirtual, PAGE_SIZE + POOL_SIZE + PAGE_SIZE, PAGE_READWRITE )) {

        RtlpDebugPageHeapReleaseVM( pVirtual );
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        return NULL;
    }

    //
    //  Out of our initial allocation, the initial page is the fake
    //  retail HEAP structure.  The second page begins our DPH_HEAP
    //  structure followed by (POOL_SIZE-sizeof(DPH_HEAP)) bytes for
    //  the initial pool.  The next page contains out CRIT_SECT
    //  variable, which must always be READWRITE.  Beyond that, the
    //  remainder of the virtual allocation is placed on the available
    //  list.
    //
    //  |_____|___________________|_____|__ _ _ _ _ _ _ _ _ _ _ _ _ __|
    //
    //  ^pVirtual
    //
    //  ^FakeRetailHEAP
    //
    //        ^HeapRoot
    //
    //            ^InitialNodePool
    //
    //                            ^CRITICAL_SECTION
    //
    //                                  ^AvailableSpace
    //
    //
    //
    //  Our DPH_HEAP structure starts at the page following the
    //  fake retail HEAP structure pointed to by the "heap handle".
    //  For the fake HEAP structure, we'll fill it with 0xEEEEEEEE
    //  except for the Heap->Flags and Heap->ForceFlags fields,
    //  which we must set to include our HEAP_FLAG_PAGE_ALLOCS flag,
    //  and then we'll make the whole page read-only.
    //

    RtlFillMemory( pVirtual, PAGE_SIZE, FILL_BYTE );

    ((PHEAP)pVirtual)->Flags      = Flags | HEAP_FLAG_PAGE_ALLOCS;
    ((PHEAP)pVirtual)->ForceFlags = Flags | HEAP_FLAG_PAGE_ALLOCS;

    if (! RtlpDebugPageHeapProtectVM( pVirtual, PAGE_SIZE, PAGE_READONLY )) {

        RtlpDebugPageHeapReleaseVM( pVirtual );
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        return NULL;
    }

    HeapRoot = (PDPH_HEAP_ROOT)( pVirtual + PAGE_SIZE );

    HeapRoot->Signature    = DPH_HEAP_SIGNATURE;
    HeapRoot->HeapFlags    = Flags;
    HeapRoot->HeapCritSect = (PVOID)((PCHAR)HeapRoot + POOL_SIZE );

    //
    // Copy the page heap global flags into per heap flags.
    //

    HeapRoot->ExtraFlags = RtlpDphGlobalFlags;

    //
    // If page heap meta data protection was requested we transfer
    // the bit into the HeapFlags field.
    //

    if ((HeapRoot->ExtraFlags & PAGE_HEAP_PROTECT_META_DATA)) {
        HeapRoot->HeapFlags |= HEAP_PROTECTION_ENABLED;
    }

    //
    // If the PAGE_HEAP_UNALIGNED_ALLOCATIONS bit is set
    // in ExtraFlags we will set the HEAP_NO_ALIGNMENT flag
    // in the HeapFlags. This last bit controls if allocations
    // will be aligned or not. The reason we do this transfer is
    // that ExtraFlags can be set from the registry whereas the
    // normal HeapFlags cannot.
    //

    if ((HeapRoot->ExtraFlags & PAGE_HEAP_UNALIGNED_ALLOCATIONS)) {
        HeapRoot->HeapFlags |= HEAP_NO_ALIGNMENT;
    }

    //
    // Initialize the seed for the random generator used to decide
    // from where should we make allocations if random decision
    // flag is on.
    //

    {
        LARGE_INTEGER PerformanceCounter;

        PerformanceCounter.LowPart = 0xABCDDCBA;

        NtQueryPerformanceCounter (
            &PerformanceCounter,
            NULL);

        HeapRoot->Seed = PerformanceCounter.LowPart;
    }

    //
    // Create the normal heap associated with the page heap.
    // The last parameter value (-1) is very important because
    // it stops the recursive call into page heap create.
    //
    // Note that it is very important to reset the NO_SERIALIZE
    // bit because normal heap operations can happen in random
    // threads when the free delayed cache gets trimmed.
    //

    HeapRoot->NormalHeap = RtlCreateHeap (
        Flags & (~HEAP_NO_SERIALIZE),
        HeapBase,
        ReserveSize,
        CommitSize,
        Lock,
        (PRTL_HEAP_PARAMETERS)-1 );

    if (HeapRoot->NormalHeap == NULL) {

        RtlpDebugPageHeapReleaseVM( pVirtual );
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
        return NULL;
    }

    //
    // Initialize heap lock.
    //

    RtlInitializeCriticalSection( HeapRoot->HeapCritSect );

    //
    //  On the page that contains our DPH_HEAP structure, use
    //  the remaining memory beyond the DPH_HEAP structure as
    //  pool for allocating heap nodes.
    //

    RtlpDebugPageHeapAddNewPool( HeapRoot,
        HeapRoot + 1,
        POOL_SIZE - sizeof( DPH_HEAP_ROOT ),
        FALSE
        );

    //
    //  Make initial PoolList entry by taking a node from the
    //  UnusedNodeList, which should be guaranteed to be non-empty
    //  since we just added new nodes to it.
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = (PVOID)HeapRoot;
    Node->nVirtualBlockSize = POOL_SIZE;
    RtlpDebugPageHeapPlaceOnPoolList( HeapRoot, Node );

    //
    //  Make VirtualStorageList entry for initial VM allocation
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = pVirtual;
    Node->nVirtualBlockSize = nVirtual;
    RtlpDebugPageHeapPlaceOnVirtualList( HeapRoot, Node );

    //
    //  Make AvailableList entry containing remainder of initial VM
    //  and add to (create) the AvailableList.
    //

    Node = RtlpDebugPageHeapAllocateNode( HeapRoot );
    DEBUG_ASSERT( Node != NULL );
    Node->pVirtualBlock     = pVirtual + ( PAGE_SIZE + POOL_SIZE + PAGE_SIZE );
    Node->nVirtualBlockSize = nVirtual - ( PAGE_SIZE + POOL_SIZE + PAGE_SIZE );
    RtlpDebugPageHeapCoalesceNodeIntoAvailable( HeapRoot, Node );

    //
    // Get heap creation stack trace.
    //

    HeapRoot->CreateStackTrace = RtlpDphLogStackTrace(1);

    //
    //  Initialize heap internal structure protection.
    //

    HeapRoot->nUnProtectionReferenceCount = 1;          // initialize

    //
    //  If this is the first heap creation in this process, then we
    //  need to initialize the process heap list critical section,
    // the global delayed free queue for normal blocks and the
    // trace database.
    //

    if (! RtlpDphHeapListHasBeenInitialized) {

        RtlpDphHeapListHasBeenInitialized = TRUE;

        RtlInitializeCriticalSection( &RtlpDphHeapListCriticalSection );
        RtlpDphInitializeDelayedFreeQueue ();

        //
        // Do not make fuss if the trace database creation fails.
        // This is something we can live with.
        //
        // The number of buckets is chosen to be a prime not too
        // close to a power of two (Knuth says so). Three possible
        // values are: 1567, 3089, 6263.
        //

        RtlpDphTraceDatabase = RtlTraceDatabaseCreate (
            6263,
            RtlpDphTraceDatabaseMaximumSize,
            0,
            0,
            NULL);

#if DBG
        if (RtlpDphTraceDatabase == NULL) {
            DbgPrint ("Page heap: warning: failed to create trace database for %p",
                HeapRoot);
        }
#endif
        //
        // Create the Unicode string containing the target dlls.
        // If no target dlls have been specified the string will
        // be initialized with the empty string.
        //

        RtlInitUnicodeString (
            &RtlpDphTargetDllsUnicode,
            RtlpDphTargetDlls);

        //
        // Initialize the target dlls logic
        //

        RtlpDphTargetDllsLogicInitialize ();
    }

    //
    //  Add this heap entry to the process heap linked list.
    //

    RtlEnterCriticalSection( &RtlpDphHeapListCriticalSection );

    if (RtlpDphHeapListHead == NULL) {
        RtlpDphHeapListHead = HeapRoot;
        RtlpDphHeapListTail = HeapRoot;
    }
    else {
        HeapRoot->pPrevHeapRoot = RtlpDphHeapListTail;
        UNPROTECT_HEAP_STRUCTURES(RtlpDphHeapListTail);
        RtlpDphHeapListTail->pNextHeapRoot = HeapRoot;
        PROTECT_HEAP_STRUCTURES(RtlpDphHeapListTail);
        RtlpDphHeapListTail                = HeapRoot;
    }

    PROTECT_HEAP_STRUCTURES( HeapRoot );                // now protected

    RtlpDphHeapListCount += 1;

    RtlLeaveCriticalSection( &RtlpDphHeapListCriticalSection );

    DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));

#if 0 // ISSUE: SilviuC: use DbgPrintEx instead.
    DbgPrint( "Page heap: process 0x%X created heap @ %p (%p, flags 0x%X)\n",
        NtCurrentTeb()->ClientId.UniqueProcess,
        HEAP_HANDLE_FROM_ROOT( HeapRoot ),
        HeapRoot->NormalHeap,
        HeapRoot->ExtraFlags);
#endif

    if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION)) {
        RtlpDphInternalValidatePageHeap (HeapRoot, NULL, 0);
    }

    return HEAP_HANDLE_FROM_ROOT( HeapRoot );       // same as pVirtual

}

PVOID
RtlpDebugPageHeapAllocate(
    IN PVOID  HeapHandle,
    IN ULONG  Flags,
    IN SIZE_T Size
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK pAvailNode;
    PDPH_HEAP_BLOCK pPrevAvailNode;
    PDPH_HEAP_BLOCK pBusyNode;
    SIZE_T               nBytesAllocate;
    SIZE_T               nBytesAccess;
    SIZE_T               nActual;
    PVOID                pVirtual;
    PVOID                pReturn;
    PUCHAR               pBlockHeader;
    ULONG Reason;
    BOOLEAN ForcePageHeap = FALSE;

    //
    // Reject extreme size requests.
    //

#if defined(_IA64_)
    if (Size > 0x8000000000000000) {
#else
    if (Size > 0x80000000) {
#endif

        VERIFIER_STOP (APPLICATION_VERIFIER_EXTREME_SIZE_REQUEST,
                       "extreme size request",
                       HeapHandle, "Heap handle", 
                       Size, "Size requested", 
                       0, "", 
                       0, "");
        return NULL;
    }

    //
    // Check if it is time to do fault injection.
    //

    if (RtlpDphShouldFaultInject ()) {
        return NULL;
    }

    //
    // Check if we have a biased heap pointer which signals
    // a forced page heap allocation (no normal heap).
    //

    if (IS_BIASED_POINTER(HeapHandle)) {
        HeapHandle = UNBIAS_POINTER(HeapHandle);
        ForcePageHeap = TRUE;
    }

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return NULL;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);

    try {

        //
        // We cannot validate the heap when a forced allocation into page heap
        // is requested due to accounting problems. Allocate is called in this way
        // from ReAllocate while the old node (just about to be freed) is in limbo
        // and is not accounted in any internal structure.
        //

        if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION) && !ForcePageHeap) {
            RtlpDphInternalValidatePageHeap (HeapRoot, NULL, 0);
        }

        Flags |= HeapRoot->HeapFlags;

        //
        // Figure out if we need to minimize memory impact. This
        // might trigger an allocation in the normal heap.
        //

        if (! ForcePageHeap) {

            if (! (RtlpDphShouldAllocateInPageHeap (HeapRoot, Size))) {

                pReturn = RtlpDphNormalHeapAllocate (
                    HeapRoot,
                    Flags,
                    Size);

                goto EXIT;
            }
        }

        //
        // Check the heap a little bit on checked builds.
        //

        DEBUG_CODE( RtlpDebugPageHeapVerifyIntegrity( HeapRoot ));

        pReturn = NULL;

        //
        //  Validate requested size so we don't overflow
        //  while rounding up size computations.  We do this
        //  after we've acquired the critsect so we can still
        //  catch serialization problems.
        //

        if (Size > 0x7FFF0000) {
            OUT_OF_VM_BREAK( Flags, "Page heap: Invalid allocation size\n" );
            goto EXIT;
        }

        //
        //  Determine number of pages needed for READWRITE portion
        //  of allocation and add an extra page for the NO_ACCESS
        //  memory beyond the READWRITE page(s).
        //

        nBytesAccess  = ROUNDUP2( Size + sizeof(DPH_BLOCK_INFORMATION), PAGE_SIZE );
        nBytesAllocate = nBytesAccess + PAGE_SIZE;

        //
        //  RtlpDebugPageHeapFindAvailableMem will first attempt to satisfy
        //  the request from memory on the Available list.  If that fails,
        //  it will coalesce some of the Free list memory into the Available
        //  list and try again.  If that still fails, new VM is allocated and
        //  added to the Available list.  If that fails, the function will
        //  finally give up and return NULL.
        //

        pAvailNode = RtlpDebugPageHeapFindAvailableMem(
            HeapRoot,
            nBytesAllocate,
            &pPrevAvailNode,
            TRUE
            );

        if (pAvailNode == NULL) {
            OUT_OF_VM_BREAK( Flags, "Page heap: Unable to allocate virtual memory\n" );
            goto EXIT;
        }

        //
        //  Now can't call AllocateNode until pAvailNode is
        //  adjusted and/or removed from Avail list since AllocateNode
        //  might adjust the Avail list.
        //

        pVirtual = pAvailNode->pVirtualBlock;

        if (nBytesAccess > 0) {

            if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

                if (! RtlpDebugPageHeapProtectVM( (PUCHAR)pVirtual + PAGE_SIZE, nBytesAccess, PAGE_READWRITE )) {
                    goto EXIT;
                }
            }
            else {

                if (! RtlpDebugPageHeapProtectVM( pVirtual, nBytesAccess, PAGE_READWRITE )) {
                    goto EXIT;
                }
            }
        }

        //
        // If we use uncommitted ranges we need to decommit the protection
        // page at the end. BAckward overruns flag disables smart memory
        // usage flag.
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            // nothing

        }
        else {

            if ((HeapRoot->ExtraFlags & PAGE_HEAP_SMART_MEMORY_USAGE)) {

                RtlpDebugPageHeapDecommitVM (
                    (PCHAR)pVirtual + nBytesAccess,
                    PAGE_SIZE);
            }
        }

        //
        //  pAvailNode (still on avail list) points to block large enough
        //  to satisfy request, but it might be large enough to split
        //  into two blocks -- one for request, remainder leave on
        //  avail list.
        //

        if (pAvailNode->nVirtualBlockSize > nBytesAllocate) {

            //
            //  Adjust pVirtualBlock and nVirtualBlock size of existing
            //  node in avail list.  The node will still be in correct
            //  address space order on the avail list.  This saves having
            //  to remove and then re-add node to avail list.  Note since
            //  we're changing sizes directly, we need to adjust the
            //  avail and busy list counters manually.
            //
            //  Note: since we're leaving at least one page on the
            //  available list, we are guaranteed that AllocateNode
            //  will not fail.
            //

            pAvailNode->pVirtualBlock                    += nBytesAllocate;
            pAvailNode->nVirtualBlockSize                -= nBytesAllocate;
            HeapRoot->nAvailableAllocationBytesCommitted -= nBytesAllocate;

            pBusyNode = RtlpDebugPageHeapAllocateNode( HeapRoot );

            DEBUG_ASSERT( pBusyNode != NULL );

            pBusyNode->pVirtualBlock     = pVirtual;
            pBusyNode->nVirtualBlockSize = nBytesAllocate;

        }

        else {

            //
            //  Entire avail block is needed, so simply remove it from avail list.
            //

            RtlpDebugPageHeapRemoveFromAvailableList( HeapRoot, pAvailNode, pPrevAvailNode );

            pBusyNode = pAvailNode;

        }

        //
        //  Now pBusyNode points to our committed virtual block.
        //

        if (HeapRoot->HeapFlags & HEAP_NO_ALIGNMENT)
            nActual = Size;
        else
            nActual = ROUNDUP2( Size, USER_ALIGNMENT );

        pBusyNode->nVirtualAccessSize = nBytesAccess;
        pBusyNode->nUserRequestedSize = Size;
        pBusyNode->nUserActualSize    = nActual;

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            pBusyNode->pUserAllocation    = pBusyNode->pVirtualBlock
                + PAGE_SIZE;
        }
        else {

            pBusyNode->pUserAllocation    = pBusyNode->pVirtualBlock
                + pBusyNode->nVirtualAccessSize
                - nActual;
        }

        pBusyNode->UserValue          = NULL;
        pBusyNode->UserFlags          = Flags & HEAP_SETTABLE_USER_FLAGS;

        //
        //  RtlpDebugPageHeapAllocate gets called from RtlDebugAllocateHeap,
        //  which gets called from RtlAllocateHeapSlowly, which gets called
        //  from RtlAllocateHeap.  To keep from wasting lots of stack trace
        //  storage, we'll skip the bottom 3 entries, leaving RtlAllocateHeap
        //  as the first recorded entry.
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {

            pBusyNode->StackTrace = RtlpDphLogStackTrace(3);

            if (pBusyNode->StackTrace) {

                RtlTraceDatabaseLock (RtlpDphTraceDatabase);
                pBusyNode->StackTrace->UserCount += 1;
                pBusyNode->StackTrace->UserSize += pBusyNode->nUserRequestedSize;
                pBusyNode->StackTrace->UserContext = HeapRoot;
                RtlTraceDatabaseUnlock (RtlpDphTraceDatabase);
            }
        }
        else {
            pBusyNode->StackTrace = NULL;
        }

        RtlpDebugPageHeapPlaceOnBusyList( HeapRoot, pBusyNode );

        pReturn = pBusyNode->pUserAllocation;

        //
        //  For requests the specify HEAP_ZERO_MEMORY, we'll fill the
        //  user-requested portion of the block with zeros. For requests
        //  that don't specify HEAP_ZERO_MEMORY, we fill the whole user block
        //  with DPH_PAGE_BLOCK_INFIX.
        //

        if ((Flags & HEAP_ZERO_MEMORY)) {


            RtlZeroMemory( pBusyNode->pUserAllocation, Size );
        }
        else {

            RtlFillMemory( pBusyNode->pUserAllocation, Size, DPH_PAGE_BLOCK_INFIX);
        }

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            // nothing

        }
        else {

            RtlpDphWritePageHeapBlockInformation (
                HeapRoot,
                pBusyNode->pUserAllocation,
                Size,
                nBytesAccess);
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);

    if (pReturn == NULL) {
        IF_GENERATE_EXCEPTION (Flags, STATUS_NO_MEMORY);
    }

    return pReturn;
}


BOOLEAN
RtlpDebugPageHeapFree(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
{

    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node, Prev;
    BOOLEAN              Success;
    PCH                  p;
    ULONG Reason;

    //
    // Skip over null frees. These are valid in C++.
    //

    if (Address == NULL) {

        return TRUE;
    }

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return FALSE;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION)) {
            RtlpDphInternalValidatePageHeap (HeapRoot, NULL, 0);
        }

        Flags |= HeapRoot->HeapFlags;

        Success = FALSE;

        Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, &Prev );

        if (Node == NULL) {

            //
            // No wonder we did not find the block in the page heap
            // structures because the block was probably allocated
            // from the normal heap. Or there is a real bug.
            // If there is a bug NormalHeapFree will break into debugger.
            //

            Success = RtlpDphNormalHeapFree (

                HeapRoot,
                Flags,
                Address);

            goto EXIT;
        }

        //
        // Check if there are any orphan critical sections in the block to be freed.
        //

        RtlpCheckForCriticalSectionsInMemoryRange (Address, 
                                                   Node->nUserRequestedSize,
                                                   NULL);

        //
        //  If tail was allocated, make sure filler not overwritten
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            if (Node->nVirtualAccessSize > 0) {
                RtlpDebugPageHeapProtectVM( Node->pVirtualBlock + PAGE_SIZE,
                    Node->nVirtualAccessSize,
                    PAGE_NOACCESS );
            }
        }
        else {

            if (! (RtlpDphIsPageHeapBlock (HeapRoot, Address, &Reason, TRUE))) {

                RtlpDphReportCorruptedBlock (HeapRoot,
                                             DPH_CONTEXT_FULL_PAGE_HEAP_FREE,
                                             Address, 
                                             Reason);
            }

            if (Node->nVirtualAccessSize > 0) {

                //
                // Mark the block as freed. The information is gone if we
                // will decommit the region but will remain if smart memory
                // flag is not set and can help debug failures.
                //

                {
                    PDPH_BLOCK_INFORMATION Info = (PDPH_BLOCK_INFORMATION)(Node->pUserAllocation);

                    Info -= 1;
                    Info->StartStamp -= 1;
                    Info->EndStamp -= 1;
                }

                RtlpDebugPageHeapProtectVM( Node->pVirtualBlock,
                    Node->nVirtualAccessSize,
                    PAGE_NOACCESS );
            }
        }

        RtlpDebugPageHeapRemoveFromBusyList( HeapRoot, Node, Prev );

        //
        // If we use uncommitted ranges we need to decommit the memory
        // range now for the allocation. Note that the next page (guard)
        // was already decommitted when we allocated the block.
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            // nothing

        }
        else {

            if ((HeapRoot->ExtraFlags & PAGE_HEAP_SMART_MEMORY_USAGE)) {

                RtlpDebugPageHeapDecommitVM (
                    Node->pVirtualBlock,
                    Node->nVirtualAccessSize);
            }
        }


        RtlpDebugPageHeapPlaceOnFreeList( HeapRoot, Node );

        //
        //  RtlpDebugPageHeapFree gets called from RtlDebugFreeHeap, which
        //  gets called from RtlFreeHeapSlowly, which gets called from
        //  RtlFreeHeap.  To keep from wasting lots of stack trace storage,
        //  we'll skip the bottom 3 entries, leaving RtlFreeHeap as the
        //  first recorded entry.
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {

            if (Node->StackTrace) {

                RtlTraceDatabaseLock (RtlpDphTraceDatabase);

                if (Node->StackTrace->UserCount > 0) {
                    Node->StackTrace->UserCount -= 1;
                }

                if (Node->StackTrace->UserSize >= Node->nUserRequestedSize) {
                    Node->StackTrace->UserSize -= Node->nUserRequestedSize;
                }

                RtlTraceDatabaseUnlock (RtlpDphTraceDatabase);
            }

            Node->StackTrace = RtlpDphLogStackTrace(3);
        }
        else {
            Node->StackTrace = NULL;
        }

        Success = TRUE;
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    if (! Success) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_ACCESS_VIOLATION );
    }

    return Success;
}

PVOID
RtlpDebugPageHeapReAllocate(
    IN PVOID  HeapHandle,
    IN ULONG  Flags,
    IN PVOID  Address,
    IN SIZE_T Size
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK OldNode, OldPrev, NewNode;
    PVOID                NewAddress;
    PUCHAR               p;
    SIZE_T               CopyDataSize;
    ULONG                SaveFlags;
    BOOLEAN ReallocInNormalHeap = FALSE;
    ULONG Reason;
    BOOLEAN ForcePageHeap = FALSE;
    BOOLEAN OriginalAllocationInPageHeap = FALSE;

    //
    // Reject extreme size requests.
    //

#if defined(_IA64_)
    if (Size > 0x8000000000000000) {
#else
    if (Size > 0x80000000) {
#endif

        VERIFIER_STOP (APPLICATION_VERIFIER_EXTREME_SIZE_REQUEST,
                       "extreme size request",
                       HeapHandle, "Heap handle", 
                       Size, "Size requested", 
                       0, "",
                       0, "");
        return NULL;
    }

    //
    // Check if it is time to do fault injection.
    //

    if (RtlpDphShouldFaultInject ()) {
        return NULL;
    }

    //
    // Check if we have a biased heap pointer which signals
    // a forced page heap allocation (no normal heap).
    //

    if (IS_BIASED_POINTER(HeapHandle)) {
        HeapHandle = UNBIAS_POINTER(HeapHandle);
        ForcePageHeap = TRUE;
    }

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return NULL;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION)) {
            RtlpDphInternalValidatePageHeap (HeapRoot, NULL, 0);
        }

        Flags |= HeapRoot->HeapFlags;

        NewAddress = NULL;

        //
        //  Check Flags for non-moveable reallocation and fail it
        //  unconditionally.  Apps that specify this flag should be
        //  prepared to deal with failure anyway.
        //

        if (Flags & HEAP_REALLOC_IN_PLACE_ONLY) {
            goto EXIT;
        }

        //
        //  Validate requested size so we don't overflow
        //  while rounding up size computations.  We do this
        //  after we've acquired the critsect so we can still
        //  catch serialization problems.
        //

        if (Size > 0x7FFF0000) {
            OUT_OF_VM_BREAK( Flags, "Page heap: Invalid allocation size\n" );
            goto EXIT;
        }

        OldNode = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, &OldPrev );

        if (OldNode) {
            OriginalAllocationInPageHeap = TRUE;
        }

        if (OldNode == NULL) {

            //
            // No wonder we did not find the block in the page heap
            // structures because the block was probably allocated
            // from the normal heap. Or there is a real bug. If there
            // is a bug NormalHeapReAllocate will break into debugger.
            //

            NewAddress = RtlpDphNormalHeapReAllocate (

                HeapRoot,
                Flags,
                Address,
                Size);

            goto EXIT;
        }

        //
        //  If tail was allocated, make sure filler not overwritten
        //

        if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            // nothing
        }
        else {

            if (! (RtlpDphIsPageHeapBlock (HeapRoot, Address, &Reason, TRUE))) {

                RtlpDphReportCorruptedBlock (HeapRoot,
                                             DPH_CONTEXT_FULL_PAGE_HEAP_REALLOC,
                                             Address, 
                                             Reason);
            }
        }

        //
        //  Before allocating a new block, remove the old block from
        //  the busy list.  When we allocate the new block, the busy
        //  list pointers will change, possibly leaving our acquired
        //  Prev pointer invalid.
        //

        RtlpDebugPageHeapRemoveFromBusyList( HeapRoot, OldNode, OldPrev );

        //
        //  Allocate new memory for new requested size.  Use try/except
        //  to trap exception if Flags caused out-of-memory exception.
        //

        try {

            if (!ForcePageHeap && !(RtlpDphShouldAllocateInPageHeap (HeapRoot, Size))) {

                NewAddress = RtlpDphNormalHeapAllocate (
                    HeapRoot,
                    Flags,
                    Size);

                ReallocInNormalHeap = TRUE;
            }
            else {

                //
                // Force the allocation in page heap by biasing
                // the heap handle. Validate the heap here since when we use
                // biased pointers validation inside Allocate is disabled.
                //

                if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION)) {
                    RtlpDphInternalValidatePageHeap (HeapRoot, OldNode->pVirtualBlock, OldNode->nVirtualBlockSize);
                }

                NewAddress = RtlpDebugPageHeapAllocate(
                    BIAS_POINTER(HeapHandle),
                    Flags,
                    Size);

                //
                // When we get back from the page heap call we will get
                // back read only meta data that we need to make read write.
                //

                UNPROTECT_HEAP_STRUCTURES( HeapRoot );

                if ((RtlpDphDebugLevel & DPH_DEBUG_INTERNAL_VALIDATION)) {
                    RtlpDphInternalValidatePageHeap (HeapRoot, OldNode->pVirtualBlock, OldNode->nVirtualBlockSize);
                }

                ReallocInNormalHeap = FALSE;
            }
        }
        except( EXCEPTION_EXECUTE_HANDLER ) {
        }

        //
        // We managed to make a new allocation (normal or page heap).
        // Now we need to copy from old to new all sorts of stuff
        // (contents, user flags/values).
        //

        if (NewAddress) {

            //
            // Copy old block contents into the new node.
            //

            CopyDataSize = OldNode->nUserRequestedSize;

            if (CopyDataSize > Size) {
                CopyDataSize = Size;
            }

            if (CopyDataSize > 0) {

                RtlCopyMemory(
                    NewAddress,
                    Address,
                    CopyDataSize
                    );
            }

            //
            // If new allocation was done in page heap we need to detect the new node
            // and copy over user flags/values.
            //

            if (! ReallocInNormalHeap) {

                NewNode = RtlpDebugPageHeapFindBusyMem( HeapRoot, NewAddress, NULL );

                //
                // This block could not be in normal heap therefore from this
                // respect the call above should always succeed.
                //

                DEBUG_ASSERT( NewNode != NULL );

                NewNode->UserValue = OldNode->UserValue;
                NewNode->UserFlags = ( Flags & HEAP_SETTABLE_USER_FLAGS ) ?
                    ( Flags & HEAP_SETTABLE_USER_FLAGS ) :
                OldNode->UserFlags;

            }

            //
            // We need to cover the case where old allocation was in page heap.
            // In this case we still need to cleanup the old node and
            // insert it back in free list. Actually the way the code is written
            // we take this code path only if original allocation was in page heap.
            // This is the reason for the assert.
            //


            RETAIL_ASSERT (OriginalAllocationInPageHeap);

            if (OriginalAllocationInPageHeap) {

                if (OldNode->nVirtualAccessSize > 0) {
                    RtlpDebugPageHeapProtectVM( OldNode->pVirtualBlock,
                        OldNode->nVirtualAccessSize,
                        PAGE_NOACCESS );
                }

                //
                // If we use uncommitted ranges we need to decommit the memory
                // range now. Note that the next page (guard) was already decommitted
                // when we made the allocation.
                //

                if ((HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

                    // nothing

                }
                else {

                    if ((HeapRoot->ExtraFlags & PAGE_HEAP_SMART_MEMORY_USAGE)) {

                        RtlpDebugPageHeapDecommitVM (
                            OldNode->pVirtualBlock,
                            OldNode->nVirtualAccessSize);
                    }
                }

                RtlpDebugPageHeapPlaceOnFreeList( HeapRoot, OldNode );

                //
                // RtlpDebugPageHeapReAllocate gets called from RtlDebugReAllocateHeap,
                // which gets called from RtlReAllocateHeap.  To keep from wasting
                // lots of stack trace storage, we'll skip the bottom 2 entries,
                // leaving RtlReAllocateHeap as the first recorded entry in the
                // freed stack trace.
                //
                // Note. For realloc we need to do the accounting for free in the
                // trace block. The accounting for alloc is done in the real
                // alloc operation which always happens for page heap reallocs.
                //

                if ((HeapRoot->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {

                    if (OldNode->StackTrace) {

                        RtlTraceDatabaseLock (RtlpDphTraceDatabase);

                        if (OldNode->StackTrace->UserCount > 0) {
                            OldNode->StackTrace->UserCount -= 1;
                        }

                        if (OldNode->StackTrace->UserSize >= OldNode->nUserRequestedSize) {
                            OldNode->StackTrace->UserSize -= OldNode->nUserRequestedSize;
                        }

                        RtlTraceDatabaseUnlock (RtlpDphTraceDatabase);
                    }

                    OldNode->StackTrace = RtlpDphLogStackTrace(2);
                }
                else {
                    OldNode->StackTrace = NULL;
                }
            }
        }

        else {

            //
            //  Failed to allocate a new block.  Return old block to busy list.
            //

            if (OriginalAllocationInPageHeap) {

                RtlpDebugPageHeapPlaceOnBusyList( HeapRoot, OldNode );
            }

        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    if (NewAddress == NULL) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_NO_MEMORY );
    }

    return NewAddress;
}

#if (( DPH_CAPTURE_STACK_TRACE ) && ( i386 ) && ( FPO ))
#pragma optimize( "", on )      // restore original optimizations
#endif

PVOID
RtlpDebugPageHeapDestroy(
    IN PVOID HeapHandle
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_ROOT       PrevHeapRoot;
    PDPH_HEAP_ROOT       NextHeapRoot;
    PDPH_HEAP_BLOCK Node;
    PDPH_HEAP_BLOCK Next;
    ULONG                Flags;
    PUCHAR               p;
    ULONG Reason;
    PVOID NormalHeap;

    if (HeapHandle == RtlProcessHeap()) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_DESTROY_PROCESS_HEAP,
                       "attempt to destroy process heap", 
                       HeapHandle, "Process heap handle", 
                       0, "", 0, "", 0, "");
        
        return NULL;
    }

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return NULL;

    // silviuc: should find another way to detect destroy while using
    // Flags = HeapRoot->HeapFlags | HEAP_NO_SERIALIZE;
    Flags = HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        //
        // Save normal heap pointer for later.
        //

        NormalHeap = HeapRoot->NormalHeap;

        //
        // Free all blocks in the delayed free queue that belong to the
        // normal heap just about to be destroyed. Note that this is
        // not a bug. The application freed the blocks correctly but
        // we delayed the free operation.
        //

        RtlpDphFreeDelayedBlocksFromHeap (HeapRoot, NormalHeap);

        //
        //  Walk all busy allocations and check for tail fill corruption
        //

        Node = HeapRoot->pBusyAllocationListHead;

        while (Node) {

            if (! (HeapRoot->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

                if (! (RtlpDphIsPageHeapBlock (HeapRoot, Node->pUserAllocation, &Reason, TRUE))) {

                    RtlpDphReportCorruptedBlock (HeapRoot,
                                                 DPH_CONTEXT_FULL_PAGE_HEAP_DESTROY,
                                                 Node->pUserAllocation, 
                                                 Reason);
                }
            }

            //
            // Check if this active block contains a critical section. Since the
            // block will be freed this will leak a critical section. Unfortunately
            // we cannot do the same check for light page heap blocks due to the
            // loose interaction between page heap and NT heap (we want to keep it
            // this way to avoid compatibility issues).
            //

            RtlpCheckForCriticalSectionsInMemoryRange (Node->pUserAllocation, 
                                                       Node->nUserRequestedSize,
                                                       NULL);
            
            //
            // Move to next node.
            //

            Node = Node->pNextAlloc;
        }

        //
        //  Remove this heap entry from the process heap linked list.
        //

        RtlEnterCriticalSection( &RtlpDphHeapListCriticalSection );

        if (HeapRoot->pPrevHeapRoot) {
            UNPROTECT_HEAP_STRUCTURES( HeapRoot->pPrevHeapRoot );
            HeapRoot->pPrevHeapRoot->pNextHeapRoot = HeapRoot->pNextHeapRoot;
            PROTECT_HEAP_STRUCTURES( HeapRoot->pPrevHeapRoot );
        }
        else {
            RtlpDphHeapListHead = HeapRoot->pNextHeapRoot;
        }

        if (HeapRoot->pNextHeapRoot) {
            UNPROTECT_HEAP_STRUCTURES( HeapRoot->pNextHeapRoot );
            HeapRoot->pNextHeapRoot->pPrevHeapRoot = HeapRoot->pPrevHeapRoot;
            PROTECT_HEAP_STRUCTURES( HeapRoot->pNextHeapRoot );
        }
        else {
            RtlpDphHeapListTail = HeapRoot->pPrevHeapRoot;
        }

        RtlpDphHeapListCount -= 1;

        RtlLeaveCriticalSection( &RtlpDphHeapListCriticalSection );


        //
        //  Must release critical section before deleting it; otherwise,
        //  checked build Teb->CountOfOwnedCriticalSections gets out of sync.
        //

        RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
        RtlDeleteCriticalSection( HeapRoot->HeapCritSect );

        //
        //  This is weird.  A virtual block might contain storage for
        //  one of the nodes necessary to walk this list.  In fact,
        //  we're guaranteed that the root node contains at least one
        //  virtual alloc node.
        //
        //  Each time we alloc new VM, we make that the head of the
        //  of the VM list, like a LIFO structure.  I think we're ok
        //  because no VM list node should be on a subsequently alloc'd
        //  VM -- only a VM list entry might be on its own memory (as
        //  is the case for the root node).  We read pNode->pNextAlloc
        //  before releasing the VM in case pNode existed on that VM.
        //  I think this is safe -- as long as the VM list is LIFO and
        //  we don't do any list reorganization.
        //

        Node = HeapRoot->pVirtualStorageListHead;

        while (Node) {
            Next = Node->pNextAlloc;
            if (! RtlpDebugPageHeapReleaseVM( Node->pVirtualBlock )) {

                VERIFIER_STOP (APPLICATION_VERIFIER_INTERNAL_ERROR,
                               "unable to release virtual memory",
                               0, "", 0, "", 0, "", 0, "");
            }
            
            Node = Next;
        }

        //
        // Destroy normal heap. Note that this will not make a recursive
        // call into this function because this is not a page heap and
        // code in NT heap manager will detect this.
        //

        RtlDestroyHeap (NormalHeap);

    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              NULL,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    //
    //  That's it.  All the VM, including the root node, should now
    //  be released.  RtlDestroyHeap always returns NULL.
    //

#if 0 // ISSUE: SilviuC: use DbgPrintEx instead.
    DbgPrint( "Page heap: process 0x%X destroyed heap @ %p (%p)\n",
              HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess),
              HeapRoot,
              NormalHeap);
#endif

    return NULL;
}

SIZE_T
RtlpDebugPageHeapSize(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node;
    SIZE_T               Size;

    Size = -1;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL) {
        return Size;
    }

    Flags |= HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

        if (Node == NULL) {

            //
            // No wonder we did not find the block in the page heap
            // structures because the block was probably allocated
            // from the normal heap. Or there is a real bug. If there
            // is a bug NormalHeapSize will break into debugger.
            //

            Size = RtlpDphNormalHeapSize (

                HeapRoot,
                Flags,
                Address);

            goto EXIT;
        }
        else {
            Size = Node->nUserRequestedSize;
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              TRUE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:
        
    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    if (Size == -1) {
        IF_GENERATE_EXCEPTION( Flags, STATUS_ACCESS_VIOLATION );
    }

    return Size;
}

ULONG
RtlpDebugPageHeapGetProcessHeaps(
    ULONG NumberOfHeaps,
    PVOID *ProcessHeaps
    )
{
    PDPH_HEAP_ROOT HeapRoot;
    ULONG          Count;

    //
    //  Although we'd expect GetProcessHeaps never to be called
    //  before at least the very first heap creation, we should
    //  still be safe and initialize the critical section if
    //  necessary.
    //

    if (! RtlpDphHeapListHasBeenInitialized) {
        RtlpDphHeapListHasBeenInitialized = TRUE;
        RtlInitializeCriticalSection( &RtlpDphHeapListCriticalSection );
    }

    RtlEnterCriticalSection( &RtlpDphHeapListCriticalSection );

    if (RtlpDphHeapListCount <= NumberOfHeaps) {

        for (HeapRoot  = RtlpDphHeapListHead, Count = 0;
            HeapRoot != NULL;
            HeapRoot  = HeapRoot->pNextHeapRoot, Count += 1) {

            *ProcessHeaps++ = HEAP_HANDLE_FROM_ROOT( HeapRoot );
        }

        if (Count != RtlpDphHeapListCount) {

            VERIFIER_STOP (APPLICATION_VERIFIER_UNKNOWN_ERROR,
                           "process heap list count is wrong",
                           Count, "Actual count",
                           RtlpDphHeapListCount, "Page heap count",
                           0, "",
                           0, "");
        }

    }
    else {

        //
        //  User's buffer is too small.  Return number of entries
        //  necessary for subsequent call to succeed.  Buffer
        //  remains untouched.
        //

        Count = RtlpDphHeapListCount;

    }

    RtlLeaveCriticalSection( &RtlpDphHeapListCriticalSection );

    return Count;
}

ULONG
RtlpDebugPageHeapCompact(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return 0;

    Flags |= HeapRoot->HeapFlags;

    RtlpDebugPageHeapEnterCritSect( HeapRoot, Flags );

    //
    //  Don't do anything, but we did want to acquire the critsect
    //  in case this was called with HEAP_NO_SERIALIZE while another
    //  thread is in the heap code.
    //

    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return 0;
}

BOOLEAN
RtlpDebugPageHeapValidate(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node;
    BOOLEAN Result = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if (HeapRoot == NULL)
        return FALSE;

    Flags |= HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Node = Address ? RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL ) : NULL;

        if (Node == NULL) {

            Result = RtlpDphNormalHeapValidate (
                HeapRoot,
                Flags,
                Address);
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              TRUE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    if (Address) {
        if (Node) {
            return TRUE;
        }
        else {
            return Result;
        }
    }
    else {
        return TRUE;
    }
}

NTSTATUS
RtlpDebugPageHeapWalk(
    IN PVOID HeapHandle,
    IN OUT PRTL_HEAP_WALK_ENTRY Entry
    )
{
#if DBG
    DbgPrint ("Page heap: warning: failing HeapWalk call with STATUS_NOT_IMPLEMENTED.\n");
#endif

    return STATUS_NOT_IMPLEMENTED;
}

BOOLEAN
RtlpDebugPageHeapLock(
    IN PVOID HeapHandle
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );

    if (HeapRoot == NULL) {
        return FALSE;
    }

    RtlpDebugPageHeapEnterCritSect( HeapRoot, HeapRoot->HeapFlags );

    return TRUE;
}

BOOLEAN
RtlpDebugPageHeapUnlock(
    IN PVOID HeapHandle
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );

    if (HeapRoot == NULL) {
        return FALSE;
    }

    RtlpDebugPageHeapLeaveCritSect( HeapRoot );

    return TRUE;
}

BOOLEAN
RtlpDebugPageHeapSetUserValue(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address,
    IN PVOID UserValue
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

        if ( Node == NULL ) {

            //
            // If we cannot find the node in page heap structures it might be
            // because it has been allocated from normal heap.
            //

            Success = RtlpDphNormalHeapSetUserValue (
                HeapRoot,
                Flags,
                Address,
                UserValue);

            goto EXIT;
        }
        else {
            Node->UserValue = UserValue;
            Success = TRUE;
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:
        
    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    return Success;
}

BOOLEAN
RtlpDebugPageHeapGetUserInfo(
    IN  PVOID  HeapHandle,
    IN  ULONG  Flags,
    IN  PVOID  Address,
    OUT PVOID* UserValue,
    OUT PULONG UserFlags
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

        if ( Node == NULL ) {

            //
            // If we cannot find the node in page heap structures it might be
            // because it has been allocated from normal heap.
            //

            Success = RtlpDphNormalHeapGetUserInfo (
                HeapRoot,
                Flags,
                Address,
                UserValue,
                UserFlags);

            goto EXIT;
        }
        else {
            if ( UserValue != NULL )
                *UserValue = Node->UserValue;
            if ( UserFlags != NULL )
                *UserFlags = Node->UserFlags;
            Success = TRUE;
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:
        
    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    return Success;
}

BOOLEAN
RtlpDebugPageHeapSetUserFlags(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN PVOID Address,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    )
{
    PDPH_HEAP_ROOT       HeapRoot;
    PDPH_HEAP_BLOCK Node;
    BOOLEAN              Success;

    Success = FALSE;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return Success;

    Flags |= HeapRoot->HeapFlags;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Node = RtlpDebugPageHeapFindBusyMem( HeapRoot, Address, NULL );

        if ( Node == NULL ) {

            //
            // If we cannot find the node in page heap structures it might be
            // because it has been allocated from normal heap.
            //

            Success = RtlpDphNormalHeapSetUserFlags (
                HeapRoot,
                Flags,
                Address,
                UserFlagsReset,
                UserFlagsSet);

            goto EXIT;
        }
        else {
            Node->UserFlags &= ~( UserFlagsReset );
            Node->UserFlags |=    UserFlagsSet;
            Success = TRUE;
        }
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    EXIT:

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    return Success;
}

BOOLEAN
RtlpDebugPageHeapSerialize(
    IN PVOID HeapHandle
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, 0);


    HeapRoot->HeapFlags &= ~HEAP_NO_SERIALIZE;

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);

    return TRUE;
}

NTSTATUS
RtlpDebugPageHeapExtend(
    IN PVOID  HeapHandle,
    IN ULONG  Flags,
    IN PVOID  Base,
    IN SIZE_T Size
    )
{
    return STATUS_SUCCESS;
}

NTSTATUS
RtlpDebugPageHeapZero(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
{
    return STATUS_SUCCESS;
}

NTSTATUS
RtlpDebugPageHeapReset(
    IN PVOID HeapHandle,
    IN ULONG Flags
    )
{
    return STATUS_SUCCESS;
}

NTSTATUS
RtlpDebugPageHeapUsage(
    IN PVOID HeapHandle,
    IN ULONG Flags,
    IN OUT PRTL_HEAP_USAGE Usage
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    //
    //  Partial implementation since this information is kind of meaningless.
    //

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return STATUS_INVALID_PARAMETER;

    if ( Usage->Length != sizeof( RTL_HEAP_USAGE ))
        return STATUS_INFO_LENGTH_MISMATCH;

    memset( Usage, 0, sizeof( RTL_HEAP_USAGE ));
    Usage->Length = sizeof( RTL_HEAP_USAGE );

    //
    // Get the heap lock, unprotect heap structures, etc.
    //

    RtlpDphPreProcessing (HeapRoot, Flags);


    try {

        Usage->BytesAllocated       = HeapRoot->nBusyAllocationBytesAccessible;
        Usage->BytesCommitted       = HeapRoot->nVirtualStorageBytes;
        Usage->BytesReserved        = HeapRoot->nVirtualStorageBytes;
        Usage->BytesReservedMaximum = HeapRoot->nVirtualStorageBytes;
    }
    except (RtlpDphUnexpectedExceptionFilter (_exception_code(), 
                                              _exception_info(),
                                              HeapRoot,
                                              FALSE)) {

        //
        // The exception filter always returns EXCEPTION_CONTINUE_SEARCH.
        //

        ASSERT_UNEXPECTED_CODE_PATH ();
    }

    //
    // Prepare page heap for exit (unlock heap lock, protect structures, etc.).
    //

    RtlpDphPostProcessing (HeapRoot);


    return STATUS_SUCCESS;
}

BOOLEAN
RtlpDebugPageHeapIsLocked(
    IN PVOID HeapHandle
    )
{
    PDPH_HEAP_ROOT HeapRoot;

    HeapRoot = RtlpDebugPageHeapPointerFromHandle( HeapHandle );
    if ( HeapRoot == NULL )
        return FALSE;

    if ( RtlTryEnterCriticalSection( HeapRoot->HeapCritSect )) {
        RtlLeaveCriticalSection( HeapRoot->HeapCritSect );
        return FALSE;
    }
    else {
        return TRUE;
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////// Page heap vs. normal heap decision making
/////////////////////////////////////////////////////////////////////

//
// 0 - full page heap
// 1 - light page heap
//

LONG RtlpDphBlockDistribution[2];

BOOLEAN
RtlpDphShouldAllocateInPageHeap (
    PDPH_HEAP_ROOT HeapRoot,
    SIZE_T Size
    )
/*++

Routine Description:

    This routine decides if the current allocation should be made in full
    page heap or light page heap.

Parameters:

    HeapRoot - heap descriptor for the current allocation request.
    
    Size - size of the current allocation request.

Return Value:

    True if this should be a full page heap allocation and false otherwise.     

--*/
{
    SYSTEM_PERFORMANCE_INFORMATION PerfInfo;
    NTSTATUS Status;
    ULONG Random;
    ULONG Percentage;

    //
    // If page heap is not enabled => normal heap.
    //

    if (! (HeapRoot->ExtraFlags & PAGE_HEAP_ENABLE_PAGE_HEAP)) {
        InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
        return FALSE;
    }

    //
    // If call not generated from one of the target dlls => normal heap
    // We do this check up front to avoid the slow path where we check
    // if VM limits have been hit.
    //

    else if ((HeapRoot->ExtraFlags & PAGE_HEAP_USE_DLL_NAMES)) {

        //
        // We return false. The calls generated from target
        // dlls will never get into this function and therefore
        // we just return false signalling that we do not want
        // page heap verification for the rest of the world.
        //
        
        InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
        return FALSE;
    }

    //
    // Check memory availability. If we tend to exhaust virtual space
    // or page file then we will go to the normal heap.
    //

    else if (RtlpDphVmLimitCanUsePageHeap() == FALSE) {
        InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
        return FALSE;
    }

    //
    // If in size range => page heap
    //

    else if ((HeapRoot->ExtraFlags & PAGE_HEAP_USE_SIZE_RANGE)) {

        if (Size >= RtlpDphSizeRangeStart && Size <= RtlpDphSizeRangeEnd) {
            InterlockedIncrement (&(RtlpDphBlockDistribution[0]));
            return TRUE;
        }
        else {
            InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
            return FALSE;
        }
    }

    //
    // If in dll range => page heap
    //

    else if ((HeapRoot->ExtraFlags & PAGE_HEAP_USE_DLL_RANGE)) {

        PVOID StackTrace[32];
        ULONG Count;
        ULONG Index;
        ULONG Hash;

        Count = RtlCaptureStackBackTrace (
            1,
            32,
            StackTrace,
            &Hash);

        //
        // (SilviuC): should read DllRange as PVOIDs
        //

        for (Index = 0; Index < Count; Index += 1) {
            if (PtrToUlong(StackTrace[Index]) >= RtlpDphDllRangeStart
                && PtrToUlong(StackTrace[Index]) <= RtlpDphDllRangeEnd) {

                InterlockedIncrement (&(RtlpDphBlockDistribution[0]));
                return TRUE;
            }
        }

        InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
        return FALSE;
    }

    //
    // If randomly decided => page heap
    //

    else if ((HeapRoot->ExtraFlags & PAGE_HEAP_USE_RANDOM_DECISION)) {

        Random = RtlRandom (& (HeapRoot->Seed));

        if ((Random % 100) < RtlpDphRandomProbability) {
            InterlockedIncrement (&(RtlpDphBlockDistribution[0]));
            return TRUE;
        }
        else {
            InterlockedIncrement (&(RtlpDphBlockDistribution[1]));
            return FALSE;
        }
    }

    //
    // For all other cases we will allocate in the page heap.
    //

    else {

        InterlockedIncrement (&(RtlpDphBlockDistribution[0]));
        return TRUE;
    }
}

//
// Vm limit related globals.
//

LONG RtlpDphVmLimitNoPageHeap;
LONG RtlpDphVmLimitHits[2];
#define SIZE_1_MB 0x100000

BOOLEAN
RtlpDphVmLimitCanUsePageHeap (
    )
/*++

Routine Description:

    This routine decides if we have good conditions for a full page heap
    allocation to be successful. It checks two things: the pagefile commit
    available on the system and the virtual space available in the current
    process. Since full page heap uses at least 2 pages for each allocation 
    it can potentially exhaust both these resources. The current criteria are:
    
    (1) if less than 32Mb of pagefile commit are left we switch to light 
    page heap
    
    (2) if less than 128Mb of empty virtual space is left we switch to light 
    page heap

Parameters:

    None.

Return Value:

    True if full page heap allocations are allowed and false otherwise.     

--*/
{
    SYSTEM_PERFORMANCE_INFORMATION PerfInfo;
    SYSTEM_BASIC_INFORMATION MemInfo;
    VM_COUNTERS VmCounters;
    NTSTATUS Status;
    LONG Value;
    LONG Calls;
    ULONGLONG Total;

    //
    // Find if full page heap is currently allowed.
    //

    Value = InterlockedCompareExchange (&RtlpDphVmLimitNoPageHeap,
                                        0,
                                        0);

    //
    // Query system for page file availability etc.
    //

    Status = NtQuerySystemInformation (SystemPerformanceInformation,
                                       &PerfInfo,
                                       sizeof(PerfInfo),
                                       NULL);

    if (!NT_SUCCESS(Status)) {
        return FALSE;
    }

    Status = NtQuerySystemInformation (SystemBasicInformation,
                                       &MemInfo,
                                       sizeof(MemInfo),
                                       NULL);

    if (!NT_SUCCESS(Status)) {
        return FALSE;
    }

    Status = NtQueryInformationProcess (NtCurrentProcess(),
                                        ProcessVmCounters,
                                        &VmCounters,
                                        sizeof(VM_COUNTERS),
                                        NULL);

    if (!NT_SUCCESS(Status)) {
        return FALSE;
    }

    //
    // First check that we have enough virtual space left in the process.
    // If less than 128Mb are left we will disable full page heap allocs.
    //

    Total = (MemInfo.MaximumUserModeAddress - MemInfo.MinimumUserModeAddress);

    if (Total - VmCounters.VirtualSize < 128 * SIZE_1_MB) {

        if (Value == 0) {
            if ((RtlpDphDebugLevel & DPH_DEBUG_SHOW_VM_LIMITS)) {
                DbgPrint ("Page heap: pid 0x%X: vm limit: vspace: disabling full page heap \n",
                          HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess));
            }
        }

        InterlockedIncrement (&(RtlpDphVmLimitHits[0]));
        InterlockedExchange (&RtlpDphVmLimitNoPageHeap, 1);
        return FALSE;
    }

    //
    // Next check for page file availability. If less than 32Mb are
    // available for commit we disable full page heap. Note that
    // CommitLimit does not reflect future pagefile extension potential.
    // Therefore pageheap will scale down even if the pagefile has not
    // been extended to its maximum.
    //

    Total = PerfInfo.CommitLimit - PerfInfo.CommittedPages;
    Total *= MemInfo.PageSize;

    if (Total - VmCounters.PagefileUsage < 32 * SIZE_1_MB) {

        if (Value == 0) {
            if ((RtlpDphDebugLevel & DPH_DEBUG_SHOW_VM_LIMITS)) {
                DbgPrint ("Page heap: pid 0x%X: vm limit: pfile: disabling full page heap \n",
                          HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess));
            }
        }

        InterlockedIncrement (&(RtlpDphVmLimitHits[1]));
        InterlockedExchange (&RtlpDphVmLimitNoPageHeap, 1);
        return FALSE;
    }

    if (Value == 1) {
        
        if ((RtlpDphDebugLevel & DPH_DEBUG_SHOW_VM_LIMITS)) {
            DbgPrint ("Page heap: pid 0x%X: vm limit: reenabling full page heap \n",
                      HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess));
        }

        InterlockedExchange (&RtlpDphVmLimitNoPageHeap, 0);
    }
    
    return TRUE;
}


/////////////////////////////////////////////////////////////////////
//////////////////////////////////// DPH_BLOCK_INFORMATION management
/////////////////////////////////////////////////////////////////////

VOID
RtlpDphReportCorruptedBlock (
    PVOID Heap,
    ULONG Context,
    PVOID Block,
    ULONG Reason
    )
{
    SIZE_T Size;
    DPH_BLOCK_INFORMATION Info;
    BOOLEAN InfoRead = FALSE;
    BOOLEAN SizeRead = FALSE;

    try {
        RtlCopyMemory (&Info, (PDPH_BLOCK_INFORMATION)Block - 1, sizeof Info);
        InfoRead = TRUE;
    }
    except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (RtlpDphGetBlockSizeFromCorruptedBlock (Block, &Size)) {
        SizeRead = TRUE;
    }

    //
    // If we did not even manage to read the entire block header
    // report exception. If we managed to read the header we will let it
    // run through the other messages and only in the end report exception.
    //

    if (!InfoRead && (Reason & DPH_ERROR_RAISED_EXCEPTION)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "exception raised while verifying block header",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }

    if ((Reason & DPH_ERROR_DOUBLE_FREE)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "block already freed",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_INFIX_PATTERN)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted infix pattern for freed block",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_HEAP_POINTER)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted heap pointer or using wrong heap",
                       Heap, "Heap used in the call",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       (InfoRead ? (UNSCRAMBLE_POINTER(Info.Heap)) : 0), "Heap owning the block");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_SUFFIX_PATTERN)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted suffix pattern",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_PREFIX_PATTERN)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted prefix pattern",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_START_STAMP)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted start stamp",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       (InfoRead ? Info.StartStamp : 0), "Corrupted stamp");
    }
    
    if ((Reason & DPH_ERROR_CORRUPTED_END_STAMP)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "corrupted end stamp",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       (InfoRead ? Info.EndStamp : 0), "Corrupted stamp");
    }

    if ((Reason & DPH_ERROR_RAISED_EXCEPTION)) {
        
        VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                       "exception raised while verifying block",
                       Heap, "Heap handle",
                       Block, "Heap block", 
                       (SizeRead ? Size : 0), "Block size",
                       0, "");
    }

    //
    // Catch all case.
    //

    VERIFIER_STOP (APPLICATION_VERIFIER_CORRUPTED_HEAP_BLOCK,
                   "corrupted heap block",
                   Heap, "Heap handle",
                   Block, "Heap block", 
                   (SizeRead ? Size : 0), "Block size",
                   0, "");
}

BOOLEAN
RtlpDphIsPageHeapBlock (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    )
{
    PDPH_BLOCK_INFORMATION Info;
    BOOLEAN Corrupted = FALSE;
    PUCHAR Current;
    PUCHAR FillStart;
    PUCHAR FillEnd;

    DEBUG_ASSERT (Reason != NULL);
    *Reason = 0;

    try {

        Info = (PDPH_BLOCK_INFORMATION)Block - 1;

        //
        // Start checking ...
        //

        if (Info->StartStamp != DPH_PAGE_BLOCK_START_STAMP_ALLOCATED) {
            *Reason |= DPH_ERROR_CORRUPTED_START_STAMP;
            Corrupted = TRUE;

            if (Info->StartStamp == DPH_PAGE_BLOCK_START_STAMP_FREE) {
                *Reason |= DPH_ERROR_DOUBLE_FREE;
            }
        }

        if (Info->EndStamp != DPH_PAGE_BLOCK_END_STAMP_ALLOCATED) {
            *Reason |= DPH_ERROR_CORRUPTED_END_STAMP;
            Corrupted = TRUE;
        }

        if (Info->Heap != Heap) {
            *Reason |= DPH_ERROR_CORRUPTED_HEAP_POINTER;
            Corrupted = TRUE;
        }

        //
        // Check the block suffix byte pattern.
        //

        if (CheckPattern) {

            FillStart = (PUCHAR)Block + Info->RequestedSize;
            FillEnd = (PUCHAR)ROUNDUP2((ULONG_PTR)FillStart, PAGE_SIZE);

            for (Current = FillStart; Current < FillEnd; Current++) {

                if (*Current != DPH_PAGE_BLOCK_SUFFIX) {

                    *Reason |= DPH_ERROR_CORRUPTED_SUFFIX_PATTERN;
                    Corrupted = TRUE;
                    break;
                }
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER) {

        *Reason |= DPH_ERROR_RAISED_EXCEPTION;
        Corrupted = TRUE;
    }

    if (Corrupted) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

BOOLEAN
RtlpDphIsNormalHeapBlock (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    )
{
    PDPH_BLOCK_INFORMATION Info;
    BOOLEAN Corrupted = FALSE;
    PUCHAR Current;
    PUCHAR FillStart;
    PUCHAR FillEnd;

    DEBUG_ASSERT (Reason != NULL);
    *Reason = 0;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    try {

        if (UNSCRAMBLE_POINTER(Info->Heap) != Heap) {
            *Reason |= DPH_ERROR_CORRUPTED_HEAP_POINTER;
            Corrupted = TRUE;
        }

        if (Info->StartStamp != DPH_NORMAL_BLOCK_START_STAMP_ALLOCATED) {
            *Reason |= DPH_ERROR_CORRUPTED_START_STAMP;
            Corrupted = TRUE;
            
            if (Info->StartStamp == DPH_NORMAL_BLOCK_START_STAMP_FREE) {
                *Reason |= DPH_ERROR_DOUBLE_FREE;
            }
        }

        if (Info->EndStamp != DPH_NORMAL_BLOCK_END_STAMP_ALLOCATED) {
            *Reason |= DPH_ERROR_CORRUPTED_END_STAMP;
            Corrupted = TRUE;
        }

        //
        // Check the block suffix byte pattern.
        //

        if (CheckPattern) {

            FillStart = (PUCHAR)Block + Info->RequestedSize;
            FillEnd = FillStart + USER_ALIGNMENT;

            for (Current = FillStart; Current < FillEnd; Current++) {

                if (*Current != DPH_NORMAL_BLOCK_SUFFIX) {

                    *Reason |= DPH_ERROR_CORRUPTED_SUFFIX_PATTERN;
                    Corrupted = TRUE;
                    break;
                }
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER) {

        *Reason |= DPH_ERROR_RAISED_EXCEPTION;
        Corrupted = TRUE;
    }

    if (Corrupted) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

BOOLEAN
RtlpDphIsNormalFreeHeapBlock (
    PVOID Block,
    PULONG Reason,
    BOOLEAN CheckPattern
    )
{
    PDPH_BLOCK_INFORMATION Info;
    BOOLEAN Corrupted = FALSE;
    PUCHAR Current;
    PUCHAR FillStart;
    PUCHAR FillEnd;

    DEBUG_ASSERT (Reason != NULL);
    *Reason = 0;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    try {

        //
        // If heap pointer is null we will just ignore this field.
        // This can happen during heap destroy operations where
        // the page heap got destroyed but the normal heap is still
        // alive.
        //

        if (Info->StartStamp != DPH_NORMAL_BLOCK_START_STAMP_FREE) {
            *Reason |= DPH_ERROR_CORRUPTED_START_STAMP;
            Corrupted = TRUE;
        }

        if (Info->EndStamp != DPH_NORMAL_BLOCK_END_STAMP_FREE) {
            *Reason |= DPH_ERROR_CORRUPTED_END_STAMP;
            Corrupted = TRUE;
        }

        //
        // Check the block suffix byte pattern.
        //

        if (CheckPattern) {

            FillStart = (PUCHAR)Block + Info->RequestedSize;
            FillEnd = FillStart + USER_ALIGNMENT;

            for (Current = FillStart; Current < FillEnd; Current++) {

                if (*Current != DPH_NORMAL_BLOCK_SUFFIX) {

                    *Reason |= DPH_ERROR_CORRUPTED_SUFFIX_PATTERN;
                    Corrupted = TRUE;
                    break;
                }
            }
        }

        //
        // Check the block infix byte pattern.
        //

        if (CheckPattern) {

            FillStart = (PUCHAR)Block;
            FillEnd = FillStart
                + ((Info->RequestedSize > USER_ALIGNMENT) ? USER_ALIGNMENT : Info->RequestedSize);

            for (Current = FillStart; Current < FillEnd; Current++) {

                if (*Current != DPH_FREE_BLOCK_INFIX) {

                    *Reason |= DPH_ERROR_CORRUPTED_INFIX_PATTERN;
                    Corrupted = TRUE;
                    break;
                }
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER) {

        *Reason |= DPH_ERROR_RAISED_EXCEPTION;
        Corrupted = TRUE;
    }

    if (Corrupted) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}


BOOLEAN
RtlpDphWritePageHeapBlockInformation (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    SIZE_T RequestedSize,
    SIZE_T ActualSize
    )
{
    PDPH_BLOCK_INFORMATION Info;
    PUCHAR FillStart;
    PUCHAR FillEnd;
    ULONG Hash;

    //
    // Size and stamp information
    //

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    Info->Heap = Heap;
    Info->RequestedSize = RequestedSize;
    Info->ActualSize = ActualSize;
    Info->StartStamp = DPH_PAGE_BLOCK_START_STAMP_ALLOCATED;
    Info->EndStamp = DPH_PAGE_BLOCK_END_STAMP_ALLOCATED;

    //
    // Fill the block suffix pattern.
    // We fill up to USER_ALIGNMENT bytes.
    //

    FillStart = (PUCHAR)Block + RequestedSize;
    FillEnd = (PUCHAR)ROUNDUP2((ULONG_PTR)FillStart, PAGE_SIZE);

    RtlFillMemory (FillStart, FillEnd - FillStart, DPH_PAGE_BLOCK_SUFFIX);

    //
    // Capture stack trace
    //

    if ((Heap->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {
        Info->StackTrace = RtlpDphLogStackTrace (3);
    }
    else {
        Info->StackTrace = NULL;
    }

    //
    // Call the old logging function (SteveWo's trace database).
    // We do this so that tools that are used for leak detection
    // (e.g. umdh) will work even if page heap is enabled.
    // If the trace database was not created this function will
    // return immediately.
    //

    Info->TraceIndex = RtlLogStackBackTrace ();

    return TRUE;
}

BOOLEAN
RtlpDphWriteNormalHeapBlockInformation (
    PDPH_HEAP_ROOT Heap,
    PVOID Block,
    SIZE_T RequestedSize,
    SIZE_T ActualSize
    )
{
    PDPH_BLOCK_INFORMATION Info;
    PUCHAR FillStart;
    PUCHAR FillEnd;
    ULONG Hash;
    ULONG Reason;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    //
    // Size and stamp information
    //

    Info->Heap = SCRAMBLE_POINTER(Heap);
    Info->RequestedSize = RequestedSize;
    Info->ActualSize = ActualSize;
    Info->StartStamp = DPH_NORMAL_BLOCK_START_STAMP_ALLOCATED;
    Info->EndStamp = DPH_NORMAL_BLOCK_END_STAMP_ALLOCATED;

    Info->FreeQueue.Blink = NULL;
    Info->FreeQueue.Flink = NULL;

    //
    // Fill the block suffix pattern.
    // We fill only USER_ALIGNMENT bytes.
    //

    FillStart = (PUCHAR)Block + RequestedSize;
    FillEnd = FillStart + USER_ALIGNMENT;

    RtlFillMemory (FillStart, FillEnd - FillStart, DPH_NORMAL_BLOCK_SUFFIX);

    //
    // Capture stack trace
    //

    if ((Heap->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {

        Info->StackTrace = RtlpDphLogStackTrace (4);

        if (Info->StackTrace) {

            RtlTraceDatabaseLock (RtlpDphTraceDatabase);
            ((PRTL_TRACE_BLOCK)(Info->StackTrace))->UserCount += 1;
            ((PRTL_TRACE_BLOCK)(Info->StackTrace))->UserSize += RequestedSize;
            ((PRTL_TRACE_BLOCK)(Info->StackTrace))->UserContext = Heap;
            RtlTraceDatabaseUnlock (RtlpDphTraceDatabase);
        }

    }
    else {
        Info->StackTrace = NULL;
    }

    //
    // Call the old logging function (SteveWo's trace database).
    // We do this so that tools that are used for leak detection
    // (e.g. umdh) will work even if page heap is enabled.
    // If the trace database was not created this function will
    // return immediately.
    //

    Info->TraceIndex = RtlLogStackBackTrace ();

    return TRUE;
}

BOOLEAN
RtlpDphGetBlockSizeFromCorruptedBlock (
    PVOID Block,
    PSIZE_T Size
    )
//
// This function gets called from RtlpDphReportCorruptedBlock only.
// It tries to extract a size for the block when an error is reported.
// If it cannot get the size it will return false.
//
{
    PDPH_BLOCK_INFORMATION Info;
    BOOLEAN Success = FALSE;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    try {

        if (Info->StartStamp == DPH_NORMAL_BLOCK_START_STAMP_FREE
            || Info->StartStamp == DPH_NORMAL_BLOCK_START_STAMP_ALLOCATED
            || Info->StartStamp == DPH_PAGE_BLOCK_START_STAMP_FREE
            || Info->StartStamp == DPH_NORMAL_BLOCK_START_STAMP_ALLOCATED) {

            *Size = Info->RequestedSize;
            Success = TRUE;
        }
        else {

            Success = FALSE;
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER) {

        Success = FALSE;
    }

    return Success;
}


/////////////////////////////////////////////////////////////////////
/////////////////////////////// Normal heap allocation/free functions
/////////////////////////////////////////////////////////////////////

PVOID
RtlpDphNormalHeapAllocate (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    SIZE_T Size
    )
{
    PVOID Block;
    PDPH_BLOCK_INFORMATION Info;
    ULONG Hash;
    SIZE_T ActualSize;
    SIZE_T RequestedSize;
    ULONG Reason;

    //
    // Reject extreme size requests.
    //

#if defined(_IA64_)
    if (Size > 0x8000000000000000) {
#else
    if (Size > 0x80000000) {
#endif

        VERIFIER_STOP (APPLICATION_VERIFIER_EXTREME_SIZE_REQUEST,
                       "extreme size request",
                       Heap, "Heap handle", 
                       Size, "Size requested", 
                       0, "",
                       0, "");
        return NULL;
    }

    RequestedSize = Size;
    ActualSize = Size + sizeof(DPH_BLOCK_INFORMATION) + USER_ALIGNMENT;

    //
    // We need to reset the NO_SERIALIZE flag because a free operation can be
    // active in another thread due to free delayed cache trimming. If the
    // allocation operation will raise an exception (e.g. OUT_OF_MEMORY) we are
    // safe to let it go here. It will be caught by the exception handler 
    // established in the main page heap entry (RtlpDebugPageHeapAlloc).
    //

    Block = RtlAllocateHeap (
        Heap->NormalHeap,
        Flags & (~HEAP_NO_SERIALIZE),
        ActualSize);

    if (Block == NULL) {

        //
        // (SilviuC): If we have memory pressure we might want
        // to trim the delayed free queues. We do not do this
        // right now because the threshold is kind of small and there
        // are many benefits in keeping this cache around.
        //

        return NULL;
    }


    RtlpDphWriteNormalHeapBlockInformation (
        Heap,
        (PDPH_BLOCK_INFORMATION)Block + 1,
        RequestedSize,
        ActualSize);

    if (! (Flags & HEAP_ZERO_MEMORY)) {

        RtlFillMemory ((PDPH_BLOCK_INFORMATION)Block + 1,
                       RequestedSize,
                       DPH_NORMAL_BLOCK_INFIX);
    }

    return (PVOID)((PDPH_BLOCK_INFORMATION)Block + 1);
}


BOOLEAN
RtlpDphNormalHeapFree (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID Block
    )
{
    PDPH_BLOCK_INFORMATION Info;
    BOOLEAN Success;
    ULONG Reason;
    ULONG Hash;
    SIZE_T TrimSize;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    if (! RtlpDphIsNormalHeapBlock(Heap, Block, &Reason, TRUE)) {

        RtlpDphReportCorruptedBlock (Heap,
                                     DPH_CONTEXT_NORMAL_PAGE_HEAP_FREE,
                                     Block, 
                                     Reason);

        return FALSE;
    }

    //
    // Check if there are any orphan critical sections in the block to be freed.
    //

    RtlpCheckForCriticalSectionsInMemoryRange (Block, 
                                               Info->RequestedSize,
                                               NULL);

    //
    // Save the free stack trace.
    //

    if ((Heap->ExtraFlags & PAGE_HEAP_COLLECT_STACK_TRACES)) {

        if (Info->StackTrace) {

            RtlTraceDatabaseLock (RtlpDphTraceDatabase);
            ((PRTL_TRACE_BLOCK)(Info->StackTrace))->UserCount -= 1;
            ((PRTL_TRACE_BLOCK)(Info->StackTrace))->UserSize -= Info->RequestedSize;
            RtlTraceDatabaseUnlock (RtlpDphTraceDatabase);
        }

        Info->StackTrace = RtlpDphLogStackTrace (3);
    }
    else {
        Info->StackTrace = NULL;
    }

    //
    // Mark the block as freed.
    //

    Info->StartStamp -= 1;
    Info->EndStamp -= 1;

    //
    // Wipe out all the information in the block so that it cannot
    // be used while free. The pattern looks like a kernel pointer
    // and if we are lucky enough the buggy code might use a value
    // from the block as a pointer and instantly access violate.
    //

    RtlFillMemory (Info + 1,
                   Info->RequestedSize,
                   DPH_FREE_BLOCK_INFIX);

    //
    // Add block to the delayed free queue.
    //

    RtlpDphAddToDelayedFreeQueue (Info);

    //
    // If we are over the threshold we need to really free
    // some of the guys.
    //

    Success = TRUE;

    if (RtlpDphNeedToTrimDelayedFreeQueue(&TrimSize)) {

        RtlpDphTrimDelayedFreeQueue (TrimSize, Flags);
    }

    return Success;
}


PVOID
RtlpDphNormalHeapReAllocate (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID OldBlock,
    SIZE_T Size
    )
{
    PVOID Block;
    PDPH_BLOCK_INFORMATION Info;
    ULONG Hash;
    SIZE_T CopySize;
    ULONG Reason;

    //
    // Reject extreme size requests.
    //

#if defined(_IA64_)
    if (Size > 0x8000000000000000) {
#else
    if (Size > 0x80000000) {
#endif

        VERIFIER_STOP (APPLICATION_VERIFIER_EXTREME_SIZE_REQUEST,
                       "extreme size request",
                       Heap, "Heap handle", 
                       Size, "Size requested", 
                       0, "",
                       0, "");
        return NULL;
    }

    Info = (PDPH_BLOCK_INFORMATION)OldBlock - 1;

    if (! RtlpDphIsNormalHeapBlock(Heap, OldBlock, &Reason, TRUE)) {

        RtlpDphReportCorruptedBlock (Heap,
                                     DPH_CONTEXT_NORMAL_PAGE_HEAP_REALLOC,
                                     OldBlock, 
                                     Reason);

        return NULL;
    }

    Block = RtlpDphNormalHeapAllocate (Heap, Flags, Size);

    if (Block == NULL) {
        return NULL;
    }

    //
    // Copy old block stuff into the new block and then
    // free old block.
    //

    if (Size < Info->RequestedSize) {
        CopySize = Size;
    }
    else {
        CopySize = Info->RequestedSize;
    }

    RtlCopyMemory (Block, OldBlock, CopySize);

    //
    // Free the old guy.
    //

    RtlpDphNormalHeapFree (Heap, Flags, OldBlock);

    return Block;
}


SIZE_T
RtlpDphNormalHeapSize (
    PDPH_HEAP_ROOT Heap,
    ULONG Flags,
    PVOID Block
    )
{
    PDPH_BLOCK_INFORMATION Info;
    SIZE_T Result;
    ULONG Reason;

    Info = (PDPH_BLOCK_INFORMATION)Block - 1;

    if (! RtlpDphIsNormalHeapBlock(Heap, Block, &Reason, FALSE)) {

        //
        // We cannot stop here for a wrong block.
        // The users might use this function to validate
        // if a block belongs to the heap or not. However
        // they should use HeapValidate for that.
        //

#if DBG
        DbgPrint ("Page heap: warning: HeapSize called with "
            "invalid block @ %p (reason %0X) \n", Block, Reason);
#endif

        return (SIZE_T)-1;
    }

    Result = RtlSizeHeap (
        Heap->NormalHeap,
        Flags,
        Info);

    if (Result == (SIZE_T)-1) {
        return Result;
    }
    else {
        return Result - sizeof(*Info) - USER_ALIGNMENT;
    }
}


BOOLEAN
RtlpDphNormalHeapSetUserFlags(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address,
    IN ULONG UserFlagsReset,
    IN ULONG UserFlagsSet
    )
{
    BOOLEAN Success;
    ULONG Reason;

    if (! RtlpDphIsNormalHeapBlock(Heap, Address, &Reason, FALSE)) {

        RtlpDphReportCorruptedBlock (Heap,
                                     DPH_CONTEXT_NORMAL_PAGE_HEAP_SETFLAGS,
                                     Address, 
                                     Reason);

        return FALSE;
    }

    Success = RtlSetUserFlagsHeap (
        Heap->NormalHeap,
        Flags,
        (PDPH_BLOCK_INFORMATION)Address - 1,
        UserFlagsReset,
        UserFlagsSet);

    return Success;
}


BOOLEAN
RtlpDphNormalHeapSetUserValue(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address,
    IN PVOID UserValue
    )
{
    BOOLEAN Success;
    ULONG Reason;

    if (! RtlpDphIsNormalHeapBlock(Heap, Address, &Reason, FALSE)) {

        RtlpDphReportCorruptedBlock (Heap,
                                     DPH_CONTEXT_NORMAL_PAGE_HEAP_SETVALUE,
                                     Address, 
                                     Reason);

        return FALSE;
    }

    Success = RtlSetUserValueHeap (
        Heap->NormalHeap,
        Flags,
        (PDPH_BLOCK_INFORMATION)Address - 1,
        UserValue);

    return Success;
}


BOOLEAN
RtlpDphNormalHeapGetUserInfo(
    IN PDPH_HEAP_ROOT Heap,
    IN  ULONG  Flags,
    IN  PVOID  Address,
    OUT PVOID* UserValue,
    OUT PULONG UserFlags
    )
{
    BOOLEAN Success;
    ULONG Reason;

    if (! RtlpDphIsNormalHeapBlock(Heap, Address, &Reason, FALSE)) {

        RtlpDphReportCorruptedBlock (Heap,
                                     DPH_CONTEXT_NORMAL_PAGE_HEAP_GETINFO,
                                     Address, 
                                     Reason);

        return FALSE;
    }

    Success = RtlGetUserInfoHeap (
        Heap->NormalHeap,
        Flags,
        (PDPH_BLOCK_INFORMATION)Address - 1,
        UserValue,
        UserFlags);

    return Success;
}


BOOLEAN
RtlpDphNormalHeapValidate(
    IN PDPH_HEAP_ROOT Heap,
    IN ULONG Flags,
    IN PVOID Address
    )
{
    BOOLEAN Success;
    ULONG Reason;

    if (Address == NULL) {

        //
        // Validation for the whole heap.
        //

        Success = RtlValidateHeap (
            Heap->NormalHeap,
            Flags,
            Address);
    }
    else {

        //
        // Validation for a heap block.
        //

        if (! RtlpDphIsNormalHeapBlock(Heap, Address, &Reason, TRUE)) {

            //
            // We cannot break in this case because the function might indeed
            // be called with invalid block. On checked builds we print a
            // warning just in case the invalid block was not intended.
            //

#if DBG
            DbgPrint ("Page heap: warning: validate called with "
                      "invalid block @ %p (reason %0X) \n", Address, Reason);
#endif

            return FALSE;
        }

        Success = RtlValidateHeap (
            Heap->NormalHeap,
            Flags,
            (PDPH_BLOCK_INFORMATION)Address - 1);
    }

    return Success;
}


/////////////////////////////////////////////////////////////////////
////////////////////////////////// Delayed free queue for normal heap
/////////////////////////////////////////////////////////////////////


RTL_CRITICAL_SECTION RtlpDphDelayedFreeQueueLock;

SIZE_T RtlpDphMemoryUsedByDelayedFreeBlocks;
SIZE_T RtlpDphNumberOfDelayedFreeBlocks;

LIST_ENTRY RtlpDphDelayedFreeQueue;

VOID
RtlpDphInitializeDelayedFreeQueue (
    )
{
    RtlInitializeCriticalSection (&RtlpDphDelayedFreeQueueLock);
    InitializeListHead (&RtlpDphDelayedFreeQueue);

    RtlpDphMemoryUsedByDelayedFreeBlocks = 0;
    RtlpDphNumberOfDelayedFreeBlocks = 0;
}


VOID
RtlpDphAddToDelayedFreeQueue (
    PDPH_BLOCK_INFORMATION Info
    )
{
    RtlEnterCriticalSection (&RtlpDphDelayedFreeQueueLock);

    InsertTailList (&(RtlpDphDelayedFreeQueue), &(Info->FreeQueue));

    RtlpDphMemoryUsedByDelayedFreeBlocks += Info->ActualSize;
    RtlpDphNumberOfDelayedFreeBlocks += 1;

    RtlLeaveCriticalSection (&RtlpDphDelayedFreeQueueLock);
}

BOOLEAN
RtlpDphNeedToTrimDelayedFreeQueue (
    PSIZE_T TrimSize
    )
{
    BOOLEAN Result;

    RtlEnterCriticalSection (&RtlpDphDelayedFreeQueueLock);

    if (RtlpDphMemoryUsedByDelayedFreeBlocks > RtlpDphDelayedFreeCacheSize) {

        *TrimSize = RtlpDphMemoryUsedByDelayedFreeBlocks - RtlpDphDelayedFreeCacheSize;

        if (*TrimSize < PAGE_SIZE) {
            *TrimSize = PAGE_SIZE;
        }

        Result = TRUE;
    }
    else {

        Result = FALSE;
    }

    RtlLeaveCriticalSection (&RtlpDphDelayedFreeQueueLock);
    return Result;
}

VOID
RtlpDphTrimDelayedFreeQueue (
    SIZE_T TrimSize,
    ULONG Flags
    )
/*++

Routine Description:

    This routine trims the delayed free queue (global per process).
    If trim size is zero it will trim up to a global threshold
    (RtlpDphDelayedFreeCacheSize) otherwise uses `TrimSize'.

    Note. This function might become a little bit of a bottleneck
    because it is called by every free operation. Because of this
    it is better to always call RtlpDphNeedToTrimDelayedFreeQueue
    first.

Arguments:

    TrimSize: amount to trim (in bytes). If zero it trims down to
    a global threshold.

    Flags: flags for free operation.

Return Value:

    None.

Environment:

    Called from RtlpDphNormalXxx (normal heap management) routines.

--*/

{
    ULONG Reason;
    SIZE_T CurrentTrimmed = 0;
    PDPH_BLOCK_INFORMATION QueueBlock;
    PLIST_ENTRY ListEntry;

    RtlEnterCriticalSection (&RtlpDphDelayedFreeQueueLock);

    if (TrimSize == 0) {
        if (RtlpDphMemoryUsedByDelayedFreeBlocks > RtlpDphDelayedFreeCacheSize) {

            TrimSize = RtlpDphMemoryUsedByDelayedFreeBlocks - RtlpDphDelayedFreeCacheSize;
        }
    }

    while (TRUE) {

        //
        // Did we achieve our trimming goal?
        //

        if (CurrentTrimmed >= TrimSize) {
            break;
        }

        //
        // The list can get empty since we remove blocks from it.
        //

        if (IsListEmpty(&RtlpDphDelayedFreeQueue)) {
            break;
        }

        ListEntry = RemoveHeadList (&RtlpDphDelayedFreeQueue);
        QueueBlock = CONTAINING_RECORD (ListEntry, DPH_BLOCK_INFORMATION, FreeQueue);

        if (! RtlpDphIsNormalFreeHeapBlock(QueueBlock + 1, &Reason, TRUE)) {

            RtlpDphReportCorruptedBlock (NULL,
                                         DPH_CONTEXT_DELAYED_FREE,
                                         QueueBlock + 1, 
                                         Reason);
        }

        RtlpDphMemoryUsedByDelayedFreeBlocks -= QueueBlock->ActualSize;
        RtlpDphNumberOfDelayedFreeBlocks -= 1;
        CurrentTrimmed += QueueBlock->ActualSize;

        QueueBlock->StartStamp -= 1;
        QueueBlock->EndStamp -= 1;

        //
        // We protect against any mishaps when we call into NT heap. Note that we
        // cannot use the original flags used for free because this free operation
        // may happen in another thread. Plus we do not want unsynchronized access
        // anyway.
        //

        try {

            RtlFreeHeap (((PDPH_HEAP_ROOT)(UNSCRAMBLE_POINTER(QueueBlock->Heap)))->NormalHeap, 
                         0, 
                         QueueBlock);
        }
        except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    RtlLeaveCriticalSection (&RtlpDphDelayedFreeQueueLock);
}


VOID
RtlpDphFreeDelayedBlocksFromHeap (
    PVOID PageHeap,
    PVOID NormalHeap
    )
{
    ULONG Reason;
    PDPH_BLOCK_INFORMATION Block;
    PLIST_ENTRY Current;
    PLIST_ENTRY Next;

    RtlEnterCriticalSection (&RtlpDphDelayedFreeQueueLock);

    for (Current = RtlpDphDelayedFreeQueue.Flink;
         Current != &RtlpDphDelayedFreeQueue;
         Current = Next) {

        Next = Current->Flink;

        Block = CONTAINING_RECORD (Current, DPH_BLOCK_INFORMATION, FreeQueue);

        if (UNSCRAMBLE_POINTER(Block->Heap) != PageHeap) {
            continue;
        }

        //
        // We need to delete this block;
        //

        RemoveEntryList (Current);
        Block = CONTAINING_RECORD (Current, DPH_BLOCK_INFORMATION, FreeQueue);

        //
        // Prevent probing of this field during RtlpDphIsNormalFreeBlock.
        //

        Block->Heap = 0;

        //
        // Check if the block about to be freed was touched.
        //

        if (! RtlpDphIsNormalFreeHeapBlock(Block + 1, &Reason, TRUE)) {

            RtlpDphReportCorruptedBlock (PageHeap,
                                         DPH_CONTEXT_DELAYED_DESTROY,
                                         Block + 1, 
                                         Reason);
        }

        RtlpDphMemoryUsedByDelayedFreeBlocks -= Block->ActualSize;
        RtlpDphNumberOfDelayedFreeBlocks -= 1;

        //
        // (SilviuC): ISSUE: Not sure what flags to use here because the flags from the original
        // call have been lost (we do not store them somewhere in the delayed queue). 
        // Zero should work though. The safest fix would be to add a new field in
        // DPH_BLOCK_INFORMATION that stores the flags used during the original free
        // and uses them again here.
        //

        Block->StartStamp -= 1;
        Block->EndStamp -= 1;

        //
        // We protect against any mishaps when we call into NT heap. Note that we
        // cannot use the original flags used for free because this free operation
        // may happen in another thread. Plus we do not want unsynchronized access
        // anyway.
        //

        try {
            
            RtlFreeHeap (NormalHeap, 
                         0, 
                         Block);
        }
        except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    RtlLeaveCriticalSection (&RtlpDphDelayedFreeQueueLock);
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// Stack trace detection
/////////////////////////////////////////////////////////////////////

PRTL_TRACE_BLOCK
RtlpDphLogStackTrace (
    ULONG FramesToSkip
    )
{
    PVOID Trace [DPH_MAX_STACK_LENGTH];
    ULONG Hash;
    ULONG Count;
    PRTL_TRACE_BLOCK Block;
    BOOLEAN Result;

    Count = RtlCaptureStackBackTrace (
        1 + FramesToSkip,
        DPH_MAX_STACK_LENGTH,
        Trace,
        &Hash);

    if (Count == 0 || RtlpDphTraceDatabase == NULL) {
        return NULL;
    }

    Result = RtlTraceDatabaseAdd (
        RtlpDphTraceDatabase,
        Count,
        Trace,
        &Block);

    if (Result == FALSE) {
        return NULL;
    }
    else {
        return Block;
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Target dlls logic
/////////////////////////////////////////////////////////////////////

RTL_CRITICAL_SECTION RtlpDphTargetDllsLock;
LIST_ENTRY RtlpDphTargetDllsList;
BOOLEAN RtlpDphTargetDllsInitialized;

typedef struct _DPH_TARGET_DLL {

    LIST_ENTRY List;
    UNICODE_STRING Name;
    PVOID StartAddress;
    PVOID EndAddress;

} DPH_TARGET_DLL, * PDPH_TARGET_DLL;

VOID
RtlpDphTargetDllsLogicInitialize (
    )
{
    RtlInitializeCriticalSection (&RtlpDphTargetDllsLock);
    InitializeListHead (&RtlpDphTargetDllsList);
    RtlpDphTargetDllsInitialized = TRUE;
}

VOID
RtlpDphTargetDllsLoadCallBack (
    PUNICODE_STRING Name,
    PVOID Address,
    ULONG Size
    )
//
// This function is not called right now but it will get called
// from \base\ntdll\ldrapi.c whenever a dll gets loaded. This
// gives page heap the opportunity to update per dll data structures
// that are not used right now for anything.
//
{
    PDPH_TARGET_DLL Descriptor;

    //
    // Get out if we are in some weird condition.
    //

    if (! RtlpDphTargetDllsInitialized) {
        return;
    }

    if (! RtlpDphIsDllTargeted (Name->Buffer)) {
        return;
    }

    Descriptor = RtlAllocateHeap (RtlProcessHeap(), 0, sizeof *Descriptor);

    if (Descriptor == NULL) {
        return;
    }

    if (! RtlCreateUnicodeString (&(Descriptor->Name), Name->Buffer)) {
        RtlFreeHeap (RtlProcessHeap(), 0, Descriptor);
        return;
    }

    Descriptor->StartAddress = Address;
    Descriptor->EndAddress = (PUCHAR)Address + Size;

    RtlEnterCriticalSection (&RtlpDphTargetDllsLock);
    InsertTailList (&(RtlpDphTargetDllsList), &(Descriptor->List));
    RtlLeaveCriticalSection (&RtlpDphTargetDllsLock);

    //
    // Print a message if a target dll has been identified.
    //

    DbgPrint("Page heap: loaded target dll %ws [%p - %p]\n",
             Descriptor->Name.Buffer,
             Descriptor->StartAddress,
             Descriptor->EndAddress);
}

const WCHAR *
RtlpDphIsDllTargeted (
    const WCHAR * Name
    )
{
    const WCHAR * All;
    ULONG I, J;

    All = RtlpDphTargetDllsUnicode.Buffer;

    for (I = 0; All[I]; I += 1) {

        for (J = 0; All[I+J] && Name[J]; J += 1) {
            if (RtlUpcaseUnicodeChar(All[I+J]) != RtlUpcaseUnicodeChar(Name[J])) {
                break;
            }
        }

        if (Name[J]) {
            continue;
        }
        else {
            // we got to the end of string
            return &(All[I]);
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Validation checks
/////////////////////////////////////////////////////////////////////

PDPH_HEAP_BLOCK
RtlpDphSearchBlockInList (
    PDPH_HEAP_BLOCK List,
    PUCHAR Address
    )
{
    PDPH_HEAP_BLOCK Current;

    for (Current = List; Current; Current = Current->pNextAlloc) {
        if (Current->pVirtualBlock == Address) {
            return Current;
        }
    }

    return NULL;
}

PVOID RtlpDphLastValidationStack;
PVOID RtlpDphCurrentValidationStack;

VOID
RtlpDphInternalValidatePageHeap (
    PDPH_HEAP_ROOT Heap,
    PUCHAR ExemptAddress,
    SIZE_T ExemptSize
    )
{
    PDPH_HEAP_BLOCK Range;
    PDPH_HEAP_BLOCK Node;
    PUCHAR Address;
    BOOLEAN FoundLeak;

    RtlpDphLastValidationStack = RtlpDphCurrentValidationStack;
    RtlpDphCurrentValidationStack = RtlpDphLogStackTrace (0);
    FoundLeak = FALSE;

    for (Range = Heap->pVirtualStorageListHead;
         Range != NULL;
         Range = Range->pNextAlloc) {

        Address = Range->pVirtualBlock;

        while (Address < Range->pVirtualBlock + Range->nVirtualBlockSize) {

            //
            // Ignore DPH_HEAP_ROOT structures.
            //

            if ((Address >= (PUCHAR)Heap - PAGE_SIZE) && (Address <  (PUCHAR)Heap + 5 * PAGE_SIZE)) {
                Address += PAGE_SIZE;
                continue;
            }

            //
            // Ignore exempt region (temporarily out of all structures).
            //

            if ((Address >= ExemptAddress) && (Address < ExemptAddress + ExemptSize)) {
                Address += PAGE_SIZE;
                continue;
            }

            Node = RtlpDphSearchBlockInList (Heap->pBusyAllocationListHead, Address);

            if (Node) {
                Address += Node->nVirtualBlockSize;
                continue;
            }

            Node = RtlpDphSearchBlockInList (Heap->pFreeAllocationListHead, Address);

            if (Node) {
                Address += Node->nVirtualBlockSize;
                continue;
            }

            Node = RtlpDphSearchBlockInList (Heap->pAvailableAllocationListHead, Address);

            if (Node) {
                Address += Node->nVirtualBlockSize;
                continue;
            }

            Node = RtlpDphSearchBlockInList (Heap->pNodePoolListHead, Address);

            if (Node) {
                Address += Node->nVirtualBlockSize;
                continue;
            }

            DbgPrint ("Block @ %p has been leaked \n", Address);
            FoundLeak = TRUE;

            Address += PAGE_SIZE;
        }
    }

    if (FoundLeak) {

        DbgPrint ("Page heap: Last stack @ %p, Current stack @ %p \n",
            RtlpDphLastValidationStack,
            RtlpDphCurrentValidationStack);

        DbgBreakPoint ();
    }
}


VOID
RtlpDphValidateInternalLists (
    PDPH_HEAP_ROOT Heap
    )
/*++

Routine Description:

    This routine is called to validate the busy and free lists of a page heap
    if /protect bit is enabled. In the wbemstress lab we have seen a corruption
    of the busy list with the start of the busy list pointing towards the end of
    the free list. This is the reason we touch very carefully the nodes that are
    in the busy list.
    

--*/
{
    
    PDPH_HEAP_BLOCK StartNode;
    PDPH_HEAP_BLOCK EndNode;
    PDPH_HEAP_BLOCK Node;
    ULONG NumberOfBlocks;
    PDPH_BLOCK_INFORMATION Block;

    //
    // Nothing to do if /protect is not enabled.
    //

    if (! (Heap->ExtraFlags & PAGE_HEAP_PROTECT_META_DATA)) {
        return;
    }

    RtlpDphLastValidationStack = RtlpDphCurrentValidationStack;
    RtlpDphCurrentValidationStack = RtlpDphLogStackTrace (0);
    
    StartNode = Heap->pBusyAllocationListHead;
    EndNode = Heap->pBusyAllocationListTail;

    try {

        //
        // Sanity checks.
        //

        if (Heap->nBusyAllocations == 0) {
            
            return;
        }

        if (StartNode == NULL || StartNode->pVirtualBlock == NULL) {


            DbgPrint ("Page heap: corruption detected: %u: \n", __LINE__);
            DbgBreakPoint ();
        }

        if (EndNode == NULL || EndNode->pVirtualBlock == NULL) {


            DbgPrint ("Page heap: corruption detected: %u: \n", __LINE__);
            DbgBreakPoint ();
        }

        //
        // First check if StartNode is also in the free list. This was the typical
        // corruption pattern that I have seen in the past.
        //

        if (RtlpDphSearchBlockInList (Heap->pFreeAllocationListHead, StartNode->pVirtualBlock)) {
            DbgPrint ("Page heap: corruption detected: %u: \n", __LINE__);
            DbgPrint ("Corruption detected: %u: \n", __LINE__);
            DbgBreakPoint ();
        }

        //
        // Make sure that we have in the busy list exactly the number of blocks we think
        // we should have.
        //

        NumberOfBlocks = 0;

        for (Node = StartNode; Node != NULL; Node = Node->pNextAlloc) {

            NumberOfBlocks += 1;
        }

        if (NumberOfBlocks != Heap->nBusyAllocations) {

            DbgPrint ("Page heap: corruption detected: %u: \n", __LINE__);
            DbgBreakPoint ();
        }

        //
        // Take all nodes in the busy list and make sure they seem to be allocated, that is
        // they have the required pattern. This is skipped if we have the /backwards option
        // enabled since in this case we do not put magic patterns.
        //

        if (! (Heap->ExtraFlags & PAGE_HEAP_CATCH_BACKWARD_OVERRUNS)) {

            for (Node = StartNode; Node != NULL; Node = Node->pNextAlloc) {

                Block = (PDPH_BLOCK_INFORMATION)(Node->pUserAllocation) - 1;

                if (Block->StartStamp != DPH_PAGE_BLOCK_START_STAMP_ALLOCATED) {

                    DbgPrint ("Page heap: corruption detected: wrong stamp for node %p \n", Node);
                    DbgBreakPoint ();
                }
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER) {

        DbgPrint ("Page heap: corruption detected: exception raised \n");
        DbgBreakPoint ();
    }
}


/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// Fault injection logic
/////////////////////////////////////////////////////////////////////

BOOLEAN RtlpDphFaultSeedInitialized;
BOOLEAN RtlpDphFaultProcessEnoughStarted;
ULONG RtlpDphFaultInjectionDisabled;

ULONG RtlpDphFaultSeed;
ULONG RtlpDphFaultSuccessRate;
ULONG RtlpDphFaultFailureRate;

#define NO_OF_FAULT_STACKS 128
PVOID RtlpDphFaultStacks [NO_OF_FAULT_STACKS];
ULONG RtlpDphFaultStacksIndex;

#define ENOUGH_TIME ((DWORDLONG)(5 * 1000 * 1000 * 10)) // 5 secs
LARGE_INTEGER RtlpDphFaultStartTime;
LARGE_INTEGER RtlpDphFaultCurrentTime;

BOOLEAN
RtlpDphShouldFaultInject (
    )
{
    ULONG Index;
    DWORDLONG Delta;

    if (RtlpDphFaultProbability == 0) {
        return FALSE;
    }

    if (RtlpDphDisableFaults != 0) {
        return FALSE;
    }

    //
    // Make sure we do not fault inject if at least one guy
    // requested our mercy by calling RtlpDphDisableFaultInjection.
    //
    if (InterlockedExchangeAdd (&RtlpDphFaultInjectionDisabled, 1) > 0) {

        InterlockedDecrement (&RtlpDphFaultInjectionDisabled);
        return FALSE;
    }
    else {

        InterlockedDecrement (&RtlpDphFaultInjectionDisabled);
    }

    //
    // Make sure we do not fault while the process is getting
    // initialized. In principle we should deal with these bugs
    // also but it is not really a priority right now.
    //

    if (RtlpDphFaultProcessEnoughStarted == FALSE) {

        if ((DWORDLONG)(RtlpDphFaultStartTime.QuadPart) == 0) {

                NtQuerySystemTime (&RtlpDphFaultStartTime);
                return FALSE;
            }
        else {

            NtQuerySystemTime (&RtlpDphFaultCurrentTime);
            Delta = (DWORDLONG)(RtlpDphFaultCurrentTime.QuadPart)
                - (DWORDLONG)(RtlpDphFaultStartTime.QuadPart);

            if (Delta < ENOUGH_TIME) {
                return FALSE;
            }

            if (Delta <= ((DWORDLONG)RtlpDphFaultTimeOut * 1000 * 1000 * 10)) {
                return FALSE;
            }

            DbgPrint( "Page heap: enabling fault injection for process 0x%X \n",
                      HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess));

            RtlpDphFaultProcessEnoughStarted = TRUE;
        }
    }

    //
    // Initialize the seed if we need to.
    //

    if (RtlpDphFaultSeedInitialized == FALSE) {

        LARGE_INTEGER PerformanceCounter;

        PerformanceCounter.LowPart = 0xABCDDCBA;

        NtQueryPerformanceCounter (
            &PerformanceCounter,
            NULL);

        RtlpDphFaultSeed = PerformanceCounter.LowPart;
        RtlpDphFaultSeedInitialized = TRUE;
    }

    if ((RtlRandom(&RtlpDphFaultSeed) % 10000) < RtlpDphFaultProbability) {

        Index = InterlockedExchangeAdd (&RtlpDphFaultStacksIndex, 1);
        Index &= (NO_OF_FAULT_STACKS - 1);
        RtlpDphFaultStacks[Index] = RtlpDphLogStackTrace (2);

        RtlpDphFaultFailureRate += 1;
        return TRUE;
    }
    else {

        RtlpDphFaultSuccessRate += 1;
        return FALSE;
    }
}

ULONG RtlpDphFaultInjectionDisabled;

VOID
RtlpDphDisableFaultInjection (
    )
{
    InterlockedIncrement (&RtlpDphFaultInjectionDisabled);
}

VOID
RtlpDphEnableFaultInjection (
    )
{
    InterlockedDecrement (&RtlpDphFaultInjectionDisabled);
}


/////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// Debug code
/////////////////////////////////////////////////////////////////////

#if INTERNAL_DEBUG

PVOID RtlpDphLastCheckTrace [16];


VOID
RtlpDphCheckFreeDelayedCache (
    PVOID CheckBlock,
    SIZE_T CheckSize
    )
{
    ULONG Reason;
    PDPH_BLOCK_INFORMATION Block;
    PLIST_ENTRY Current;
    PLIST_ENTRY Next;
    ULONG Hash;

    if (RtlpDphDelayedFreeQueue.Flink == NULL) {
        return;
    }

    RtlEnterCriticalSection (&RtlpDphDelayedFreeQueueLock);

    for (Current = RtlpDphDelayedFreeQueue.Flink;
         Current != &RtlpDphDelayedFreeQueue;
         Current = Next) {

        Next = Current->Flink;

        if (Current >= (PLIST_ENTRY)CheckBlock &&
            Current < (PLIST_ENTRY)((SIZE_T)CheckBlock + CheckSize)) {

            DbgPrint ("Page heap: block %p contains freed block %p \n", CheckBlock, Current);
            DbgBreakPoint ();
        }


        Block = CONTAINING_RECORD (Current, DPH_BLOCK_INFORMATION, FreeQueue);

        Block->Heap = UNSCRAMBLE_POINTER(Block->Heap);

        //
        // Check if the block about to be freed was touched.
        //

        if (! RtlpDphIsNormalFreeHeapBlock(Block + 1, &Reason, FALSE)) {

            RtlpDphReportCorruptedBlock (NULL,
                                         DPH_CONTEXT_DELAYED_FREE,
                                         Block + 1, 
                                         Reason);
        }

        //
        // Check busy bit
        //

        if ((((PHEAP_ENTRY)Block - 1)->Flags & HEAP_ENTRY_BUSY) == 0) {
            DbgPrint ("Page heap: block %p has busy bit reset \n", Block);
            DbgBreakPoint ();
        }
        
        Block->Heap = SCRAMBLE_POINTER(Block->Heap);
    }

    RtlZeroMemory (RtlpDphLastCheckTrace, 
                   sizeof RtlpDphLastCheckTrace);

    RtlCaptureStackBackTrace (0,
                              16,
                              RtlpDphLastCheckTrace,
                              &Hash);
    
    RtlLeaveCriticalSection (&RtlpDphDelayedFreeQueueLock);
}

#endif // #if INTERNAL_DEBUG


/////////////////////////////////////////////////////////////////////
///////////////////////////////////// Page heap global initialization
/////////////////////////////////////////////////////////////////////

#if 0

//
// It is not worth intercepting exceptions before anybody else. On one hand
// it is useful to detect if there is code that hides problems behind try/except's
// but on the other hand the code to detect if it is a legitimate AV or a
// bad one is fairly complicated and we have chances of getting double exceptions
// and other weird issues. We better leave all this business to the debugger if
// for instance it is configured to catch first chance exceptions.
//

LONG 
NTAPI
RtlpDphPageHeapExceptionFilter (
    struct _EXCEPTION_POINTERS * ExnInfo
    )
{
    PEXCEPTION_RECORD Exn;
    HANDLE CurrentThread;

    Exn = ExnInfo->ExceptionRecord;

    if (Exn->ExceptionCode == STATUS_ACCESS_VIOLATION) {

        if (NtCurrentPeb()->BeingDebugged) {

            if (Exn->NumberParameters > 1) {

                //
                // We skip over AV's in the first 64K. This skips C++ issues
                // where people free(0). This happens in Java VM stuff for instance.
                //

                if (Exn->ExceptionInformation[1] > 0x10000) {

                    VERIFIER_STOP (APPLICATION_VERIFIER_ACCESS_VIOLATION
                                   | APPLICATION_VERIFIER_DO_NOT_BREAK,
                                   "first chance access violation (address, .exr, .cxr)",
                                   Exn->ExceptionInformation[1],
                                   ExnInfo->ExceptionRecord,
                                   ExnInfo->ContextRecord,
                                   0);
                }
            }
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}


BOOLEAN
RtlpDphInitializePageHeapPackage (
    )
{
    //
    // SilviuC: we do not establish a first chance AV filter for now.
    //

    PVOID Handler;

    Handler = RtlAddVectoredExceptionHandler (1, RtlpDphPageHeapExceptionFilter);

    if (Handler == NULL) {
        
        DbgPrint ("Page heap: failed to establish an exception filter \n");
        
        return FALSE;
    }

    return TRUE;
}

#endif // #if 0

#endif // DEBUG_PAGE_HEAP

//
// End of module
//
