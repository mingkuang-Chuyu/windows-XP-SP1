/*++

Copyright (c) 1999-2000  Microsoft Corporation

Module Name:

    locks.h

Abstract:

    Various C++ class for sync. object

Author:

    HueiWang    2/17/2000

--*/
#ifndef __LOCKS_H
#define __LOCKS_H

#include <windows.h>
#include <winbase.h>
#include "assert.h"

#define ARRAY_COUNT(a) sizeof(a) / sizeof(a[0])

//
// Semaphore template
//
template <int init_count, int max_count>
class CTSemaphore 
{
private:
    HANDLE m_semaphore;

public:
    CTSemaphore() : m_semaphore(NULL)
    { 
        m_semaphore = CreateSemaphore(
                                    NULL, 
                                    init_count, 
                                    max_count, 
                                    NULL
                                ); 
        assert(m_semaphore != NULL);
    }

    ~CTSemaphore()                 
    { 
        if(m_semaphore) 
        {
            CloseHandle(m_semaphore); 
        }
    }

    DWORD 
    Acquire(
        int WaitTime=INFINITE, 
        BOOL bAlertable=FALSE
        ) 
    /*++

    --*/
    { 
        return WaitForSingleObjectEx(
                                m_semaphore, 
                                WaitTime, 
                                bAlertable
                            );
    }

    BOOL
    AcquireEx(
            HANDLE hHandle, 
            int dwWaitTime=INFINITE, 
            BOOL bAlertable=FALSE
        ) 
    /*++

    --*/
    { 
        BOOL bSuccess = TRUE;
        DWORD dwStatus;
        HANDLE hHandles[] = {m_semaphore, hHandle};

        if(hHandle == NULL || hHandle == INVALID_HANDLE_VALUE)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            bSuccess = FALSE;
        }
        else
        {
            dwStatus = WaitForMultipleObjectsEx(
                                        sizeof(hHandles)/sizeof(hHandles[0]),
                                        hHandles,
                                        FALSE,
                                        dwWaitTime,
                                        bAlertable
                                    );

            if(dwStatus != WAIT_OBJECT_0)
            {
                bSuccess = FALSE;
            }
        }

        return bSuccess;
    }


    BOOL Release(long count=1)          
    { 
        return ReleaseSemaphore(m_semaphore, count, NULL); 
    }

    BOOL IsGood()
    { 
        return m_semaphore != NULL; 
    }
};


//
// Critical section C++ class.
//
class CCriticalSection 
{

    CRITICAL_SECTION m_CS;
    BOOL m_bGood;

public:
    CCriticalSection(
        DWORD dwSpinCount = 4000    // see InitializeCriticalSection...
    ) : m_bGood(TRUE)
    { 
        
        __try {
            InitializeCriticalSectionAndSpinCount(&m_CS,  dwSpinCount); 
        } 
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            SetLastError(GetExceptionCode());
            m_bGood = FALSE;
        }
    }

    ~CCriticalSection()              
    { 
        if(IsGood() == TRUE)
        {
            DeleteCriticalSection(&m_CS); 
            m_bGood = FALSE;
        }
    }

    BOOL
    IsGood() 
    { 
        return m_bGood; 
    }

    void Lock() 
    {
        EnterCriticalSection(&m_CS);
    }

    void UnLock()
    {
        LeaveCriticalSection(&m_CS);
    }

    BOOL TryLock()
    {
        return TryEnterCriticalSection(&m_CS);
    }
};

//
// Critical section locker, this class lock the critical section
// at object constructor and release object at destructor, purpose is to
// prevent forgoting to release a critical section.
//
// usage is
//
// void
// Foo( void )
// {
//      CCriticalSectionLocker l( <some CCriticalSection instance> )
//
// }
// 
//
class CCriticalSectionLocker 
{

private:
    CCriticalSection& m_cs;

public:
    CCriticalSectionLocker( CCriticalSection& m ) : m_cs(m) 
    { 
        m.Lock(); 
    }

    ~CCriticalSectionLocker() 
    { 
        m_cs.UnLock(); 
    }
};

//
// Safe counter class.
//
class CSafeCounter 
{

private:

    long m_Counter;

public:

    CSafeCounter(
        long init_value=0
        ) : 
        m_Counter(init_value) 
    /*++

    --*/
    {
    }

    ~CSafeCounter() 
    {
    }

    operator+=(long dwValue)
    {
        long dwNewValue;

        dwNewValue = InterlockedExchangeAdd( 
                                    &m_Counter, 
                                    dwValue 
                                );
        return dwNewValue += dwValue;
    }

    operator-=(long dwValue)
    {
        long dwNewValue;

        dwNewValue = InterlockedExchangeAdd( 
                                    &m_Counter, 
                                    -dwValue 
                                );
        return dwNewValue -= dwValue;
    }

    operator++() 
    {
        return InterlockedIncrement(&m_Counter);
    }

    operator++(int) 
    {
        long lValue;

        lValue = InterlockedIncrement(&m_Counter);
        return --lValue;
    }

    operator--() 
    {
        return InterlockedDecrement(&m_Counter);
    }

    operator--(int) 
    {
        long lValue;

        lValue = InterlockedDecrement(&m_Counter);
        return ++lValue;
    }

    operator long()
    {
        return InterlockedExchange(&m_Counter, m_Counter);
    }

    operator=(const long dwValue)
    {
        InterlockedExchange(&m_Counter, dwValue);
        return dwValue;
    }
};    

//-------------------------------------------------------------------------
//
// Reader/Writer lock, modified from MSDN
//

typedef enum { 
            WRITER_LOCK, 
            READER_LOCK, 
            NO_LOCK 
    } RWLOCK_TYPE;


class CRWLock 
{ 
private:
    HANDLE hMutex;
    HANDLE hWriterMutex;
    HANDLE hReaderEvent;
    long   iReadCount;
    long   iWriteCount;

    long   iReadEntry;
    long   iWriteEntry;

public:

    CRWLock()  
    { 
        BOOL bSuccess=Init(); 
        assert(bSuccess == TRUE);
    }

    ~CRWLock() 
    { 
        Cleanup();  
    }

    //-----------------------------------------------------------
    BOOL 
    Init()
    {
        hReaderEvent=CreateEvent(NULL,TRUE,FALSE,NULL);
        hMutex = CreateEvent(NULL,FALSE,TRUE,NULL);
        hWriterMutex = CreateMutex(NULL,FALSE,NULL);
        if(!hReaderEvent || !hMutex || !hWriterMutex)
            return FALSE;

        iReadCount = -1;
        iWriteCount = -1;
        iReadEntry = 0;
        iWriteEntry = 0;

        return (TRUE);
    }

    //-----------------------------------------------------------
    void 
    Cleanup()
    {
        CloseHandle(hReaderEvent);
        CloseHandle(hMutex);
        CloseHandle(hWriterMutex);

        iReadCount = -1;
        iWriteCount = -1;
        iReadEntry = 0;
        iWriteEntry = 0;
    }

    //-----------------------------------------------------------
    void 
    Acquire(
        RWLOCK_TYPE lockType
        )
    /*++

    ++*/
    {
        if (lockType == WRITER_LOCK) 
        {  
            InterlockedIncrement(&iWriteCount);
            WaitForSingleObject(hWriterMutex,INFINITE);
            WaitForSingleObject(hMutex, INFINITE);

            assert(InterlockedIncrement(&iWriteEntry) == 1);
            assert(InterlockedExchange(&iReadEntry, iReadEntry) == 0);
        } 
        else 
        {   
            if (InterlockedIncrement(&iReadCount) == 0) 
            { 
                WaitForSingleObject(hMutex, INFINITE);
                SetEvent(hReaderEvent);
            }

            WaitForSingleObject(hReaderEvent,INFINITE);
            InterlockedIncrement(&iReadEntry);
            assert(InterlockedExchange(&iWriteEntry, iWriteEntry) == 0);
        }
    }

    //-----------------------------------------------------------
    void 
    Release(
        RWLOCK_TYPE lockType
        )
    /*++

    ++*/
    {
        if (lockType == WRITER_LOCK) 
        { 
            InterlockedDecrement(&iWriteEntry);
            InterlockedDecrement(&iWriteCount);
            SetEvent(hMutex);
            ReleaseMutex(hWriterMutex);
        } 
        else if(lockType == READER_LOCK) 
        {
            InterlockedDecrement(&iReadEntry);
            if (InterlockedDecrement(&iReadCount) < 0) 
            { 
              ResetEvent(hReaderEvent);
              SetEvent(hMutex);
            }
        }
    }

    long GetReadCount()   
    { 
        return iReadCount+1;  
    }

    long GetWriteCount()  
    { 
        return iWriteCount+1; 
    }
};

//---------------------------------------------------------------------

class CCMutex 
{
    HANDLE  hMutex;

public:

    CCMutex() : hMutex(NULL) { 
        hMutex=CreateMutex(NULL, FALSE, NULL); 
    }

    ~CCMutex() { 
        CloseHandle(hMutex); 
    }  

    DWORD 
    Lock(
        DWORD dwWaitTime=INFINITE, 
        BOOL bAlertable=FALSE
        ) 
    /*++

    --*/
    { 
        return WaitForSingleObjectEx(
                                hMutex, 
                                dwWaitTime, 
                                bAlertable
                            );
    }

    BOOL 
    Unlock() 
    {
        return ReleaseMutex(hMutex);
    }
};

//---------------------------------------------------------------------------------

class CCEvent 
{

    BOOL bManual;
    HANDLE  hEvent;

public:

    CCEvent(
        BOOL bManual, 
        BOOL bInitState
        ) : 
        hEvent(NULL), 
        bManual(bManual) 
    /*++

    --*/
    {
        hEvent=CreateEvent(
                            NULL, 
                            bManual, 
                            bInitState, 
                            NULL
                        );
    }

    ~CCEvent() 
    {
        if( NULL != hEvent )
        {
            CloseHandle(hEvent);
        }
    }

    DWORD
    MsgLock(
        DWORD dwWaitTime = INFINITE
    )
    /*++

    --*/
    {
        HANDLE wait_event[1] = {hEvent};
        DWORD result ; 
        MSG msg ; 

        // The message loop lasts until we get a WM_QUIT message,
        // upon which we shall return from the function.
        while (TRUE)
        {
            // block-local variable 

            // Read all of the messages in this next loop, 
            // removing each message as we read it.
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
            { 
                // If it's a quit message, we're out of here.
                if (msg.message == WM_QUIT)  
                {
                    return WAIT_ABANDONED; 
                }

                // Otherwise, dispatch the message.
                DispatchMessage(&msg); 
            } // End of PeekMessage while loop.

            // Wait for any message sent or posted to this queue 
            // or for one of the passed handles be set to signaled.
            result = MsgWaitForMultipleObjects(
                                        1, 
                                        wait_event,
                                        TRUE, 
                                        dwWaitTime, 
                                        QS_ALLINPUT | QS_ALLPOSTMESSAGE | QS_ALLEVENTS 
                                    ); 

            // The result tells us the type of event we have.
            if (result == WAIT_OBJECT_0 + 1)
            {
                // New messages have arrived. 
                // Continue to the top of the always while loop to 
                // dispatch them and resume waiting.
                continue;
            } 
            else 
            { 
                break;
            } // End of else clause.
        } // End of the always while loop. 

        return result;
    }
    
    DWORD 
    Lock( 
        DWORD dwWaitTime=INFINITE, 
        BOOL bAlertable=FALSE
        ) 
    /*++

    --*/
    {
        return WaitForSingleObjectEx(
                                hEvent, 
                                dwWaitTime, 
                                bAlertable
                            );
    }

    BOOL 
    SetEvent() 
    {
        return ::SetEvent(hEvent);
    }

    BOOL 
    ResetEvent() 
    {
        return ::ResetEvent(hEvent);
    }

    BOOL 
    PulseEvent() 
    {
        return ::PulseEvent(hEvent);
    }

    BOOL 
    IsManual() 
    {
        return bManual;
    }
};

#endif
