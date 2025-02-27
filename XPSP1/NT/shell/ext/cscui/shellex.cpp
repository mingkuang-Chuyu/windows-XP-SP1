// Authors;
//   Jeff Saathoff (jeffreys)
//
// Notes;
//   Context Menu and Property Sheet shell extensions

#include "pch.h"
#include "options.h"    // ..\viewer\options.h
#include "firstpin.h"
#include "uihooks.h"    // Self-host notifications
#include "msgbox.h"
#include "strings.h"
#include "nopin.h"
#include <ccstock.h>

#define CSC_PROP_NO_CSC         0x00000001L
#define CSC_PROP_MULTISEL       0x00000002L
#define CSC_PROP_PINNED         0x00000004L
#define CSC_PROP_SYNCABLE       0x00000008L
#define CSC_PROP_ADMIN_PINNED   0x00000010L
#define CSC_PROP_INHERIT_PIN    0x00000020L
#define CSC_PROP_DCON_MODE      0x00000040L

// Thread data for unpinning files
typedef struct
{
    CscFilenameList *pNamelist;
    DWORD            dwUpdateFlags;
    HWND             hwndOwner;
    BOOL             bOffline;
} CSC_UNPIN_DATA;

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Shell extension object implementation                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

STDAPI CCscShellExt::CreateInstance(REFIID riid, LPVOID *ppv)
{
    HRESULT hr;
    CCscShellExt *pThis = new CCscShellExt;
    if (pThis)
    {
        hr = pThis->QueryInterface(riid, ppv);
        pThis->Release();                           // release initial ref
    }
    else
        hr = E_OUTOFMEMORY;

    return hr;
}


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Shell extension object implementation (IUnknown)                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CCscShellExt::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(CCscShellExt, IShellExtInit),
        QITABENT(CCscShellExt, IContextMenu),
        QITABENT(CCscShellExt, IShellIconOverlayIdentifier),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) CCscShellExt::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CCscShellExt::Release()
{
    if (InterlockedDecrement(&m_cRef))
        return m_cRef;

    delete this;
    return 0;
}

// IShellExtInit

STDMETHODIMP CCscShellExt::Initialize(LPCITEMIDLIST /*pidlFolder*/, IDataObject *pdobj, HKEY /*hKeyProgID*/)
{
    IUnknown_Set((IUnknown **)&m_lpdobj, pdobj);
    return S_OK;
}


// IContextMenu
//
//  PURPOSE: Called by the shell just before the context menu is displayed.
//           This is where you add your specific menu items.
//
//  PARAMETERS:
//    hMenu      - Handle to the context menu
//    iMenu      - Index of where to begin inserting menu items
//    idCmdFirst - Lowest value for new menu ID's
//    idCmtLast  - Highest value for new menu ID's
//    uFlags     - Specifies the context of the menu event
//
//  RETURN VALUE:
//    HRESULT signifying success or failure.
//

STDMETHODIMP CCscShellExt::QueryContextMenu(HMENU hMenu, UINT iMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    HRESULT hr = ResultFromShort(0);
    UINT idCmd = idCmdFirst;
    TCHAR szMenu[MAX_PATH];
    MENUITEMINFO mii;
    CConfig& config = CConfig::GetSingleton();
    
    if ((uFlags & (CMF_DEFAULTONLY | CMF_VERBSONLY)) || !m_lpdobj)
        return hr;

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::QueryContextMenu");
    TraceAssert(IsCSCEnabled());

    //
    // Check the pin status and CSC-ability of the current selection
    //
    m_dwUIStatus = 0;
    if (FAILED(CheckFileStatus(m_lpdobj, &m_dwUIStatus)))
        m_dwUIStatus = CSC_PROP_NO_CSC;

    if (m_dwUIStatus & CSC_PROP_NO_CSC)
        TraceLeaveResult(hr);

    //
    // Add a menu separator
    //
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_TYPE;
    mii.fType = MFT_SEPARATOR;

    InsertMenuItem(hMenu, iMenu++, TRUE, &mii);

    if (!config.NoMakeAvailableOffline())
    {
        if (SUCCEEDED(hr = CanAllFilesBePinned(m_lpdobj)))
        {
            if (S_OK == hr)
            {
                mii.fState = MFS_ENABLED;  // All files in selection can be pinned.
            }
            else
            {
                mii.fState = MFS_DISABLED; // 1+ files in selection cannot be pinned.
            }

            //
            // Add the "Make Available Offline" menu item
            //
            LoadString(g_hInstance, IDS_MENU_PIN, szMenu, ARRAYSIZE(szMenu));

            mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
            mii.fType = MFT_STRING;
            if (m_dwUIStatus & (CSC_PROP_ADMIN_PINNED | CSC_PROP_PINNED))
            {
                mii.fState = MFS_CHECKED;
                if (m_dwUIStatus & (CSC_PROP_ADMIN_PINNED | CSC_PROP_INHERIT_PIN))
                    mii.fState |= MFS_DISABLED;
            }
            mii.wID = idCmd++;
            mii.dwTypeData = szMenu;

            InsertMenuItem(hMenu, iMenu++, TRUE, &mii);
        }
    }

    if (m_dwUIStatus & (CSC_PROP_SYNCABLE | CSC_PROP_PINNED | CSC_PROP_ADMIN_PINNED))
    {
        //
        // Add the "Synchronize" menu item
        //
        LoadString(g_hInstance, IDS_MENU_SYNCHRONIZE, szMenu, ARRAYSIZE(szMenu));

        mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
        mii.fType = MFT_STRING;
        mii.fState = MFS_ENABLED;
        mii.wID = idCmd++;
        mii.dwTypeData = szMenu;

        InsertMenuItem(hMenu, iMenu++, TRUE, &mii);
    }    

    //
    // Return the number of menu items we added.
    //
    hr = ResultFromShort(idCmd - idCmdFirst);

    TraceLeaveResult(hr);
}


//
//  FUNCTION: IContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO)
//
//  PURPOSE: Called by the shell after the user has selected on of the
//           menu items that was added in QueryContextMenu().
//
//  PARAMETERS:
//    lpcmi - Pointer to an CMINVOKECOMMANDINFO structure
//
//  RETURN VALUE:
//    HRESULT signifying success or failure.
//
//  COMMENTS:
//

STDMETHODIMP
CCscShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
    HRESULT hr = S_OK;
    UINT iCmd = 0;
    CscFilenameList *pfnl = NULL;   // Namelist object.
    BOOL fPin;
    BOOL bSubFolders = FALSE;
    DWORD dwUpdateFlags = 0;

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::InvokeCommand");
    TraceAssert(IsCSCEnabled());
    TraceAssert(!(m_dwUIStatus & CSC_PROP_NO_CSC));

    if (HIWORD(lpcmi->lpVerb))
    {
        if (!lstrcmpiA(lpcmi->lpVerb, STR_PIN_VERB))
        {
            iCmd = 0;
            m_dwUIStatus &= ~CSC_PROP_PINNED;
        }
        else if (!lstrcmpiA(lpcmi->lpVerb, STR_UNPIN_VERB))
        {
            iCmd = 0;
            m_dwUIStatus |= CSC_PROP_PINNED;
        }
        else if (!lstrcmpiA(lpcmi->lpVerb, STR_SYNC_VERB))
        {
            iCmd = 1;
        }
        else
        {
            Trace((TEXT("Unknown command \"%S\""), lpcmi->lpVerb));
            ExitGracefully(hr, E_INVALIDARG, "Invalid command");
        }
    }
    else
    {
        iCmd = LOWORD(lpcmi->lpVerb);

        // If we didn't add the "Make Available Offline" verb, adjust the index
        if (CConfig::GetSingleton().NoMakeAvailableOffline())
            iCmd++;
    }
    if (iCmd >= 2)
        ExitGracefully(hr, E_INVALIDARG, "Invalid command");

    pfnl = new CscFilenameList;
    if (!pfnl)
        ExitGracefully(hr, E_OUTOFMEMORY, "Unable to create CscFilenameList object");

    hr = BuildFileList(m_lpdobj, lpcmi->hwnd, pfnl, &bSubFolders);
    FailGracefully(hr, "Unable to build file list");

    switch (iCmd)
    {
    case 0:  // "Make  available offline" menu choice - Pin files
        if (!FirstPinWizardCompleted())
        {
            //
            // User has never seen the "first pin" wizard.
            // 
            if (S_FALSE == ShowFirstPinWizard(lpcmi->hwnd))
            {
                //
                // User cancelled wizard.  Abort pinning operation.
                //
                ExitGracefully(hr, S_OK, "User cancelled first-pin wizard");
            }
        }
        fPin = !(m_dwUIStatus & CSC_PROP_PINNED);
        if (!fPin && (m_dwUIStatus & CSC_PROP_DCON_MODE))
        {
            // Unpin while disconnected causes things to disappear.
            // Warn the user.
            if (IDCANCEL == CscMessageBox(lpcmi->hwnd,
                                          MB_OKCANCEL | MB_ICONWARNING,
                                          g_hInstance,
                                          IDS_CONFIRM_UNPIN_OFFLINE))
            {
                ExitGracefully(hr, E_FAIL, "User cancelled disconnected unpin operation");
            }
        }
        // If there is a directory in the list AND we're pinning AND 
        // the "AlwaysPinSubFolders" policy is NOT set, ask the user 
        // whether to go deep or not.
        // If the policy IS set we automatically do a recursive pin.
        if (bSubFolders && (!fPin || !CConfig::GetSingleton().AlwaysPinSubFolders()))
        {
            switch (DialogBox(g_hInstance,
                    MAKEINTRESOURCE(fPin ? IDD_CONFIRM_PIN : IDD_CONFIRM_UNPIN),
                    lpcmi->hwnd,
                    _ConfirmPinDlgProc))
            {
            case IDYES:
                // nothing
                break;
            case IDNO:
                bSubFolders = FALSE; // no subfolders
                break;
            case IDCANCEL:
                ExitGracefully(hr, E_FAIL, "User cancelled (un)pin operation");
                break;
            }
        }

        if (bSubFolders)
            dwUpdateFlags |= CSC_UPDATE_PIN_RECURSE;

        if (fPin)
        {
            // Self-host notification callback
            CSCUI_NOTIFYHOOK((CSCH_Pin, TEXT("Pinning %1!d! selected items"), pfnl->GetFileCount()));

            // Set the flags for pin + quick sync
            dwUpdateFlags |= CSC_UPDATE_SELECTION | CSC_UPDATE_STARTNOW
                                | CSC_UPDATE_PINFILES | CSC_UPDATE_FILL_QUICK;
        }
        else
        {
            HANDLE hThread;
            DWORD dwThreadID;
            CSC_UNPIN_DATA *pUnpinData = (CSC_UNPIN_DATA *)LocalAlloc(LPTR, sizeof(*pUnpinData));

            // Self-host notification callback
            CSCUI_NOTIFYHOOK((CSCH_Unpin, TEXT("Unpinning %1!d! selected items"), pfnl->GetFileCount()));

            //
            // No sync is required to unpin files, so let's do it in this
            // process rather than starting SyncMgr.  However, let's do
            // it in the background in case there's a lot to unpin.
            //
            if (pUnpinData)
            {
                pUnpinData->pNamelist = pfnl;
                pUnpinData->dwUpdateFlags = dwUpdateFlags;
                pUnpinData->hwndOwner = lpcmi->hwnd;
                pUnpinData->bOffline = !!(m_dwUIStatus & CSC_PROP_DCON_MODE);
                hThread = CreateThread(NULL,
                                       0,
                                       _UnpinFilesThread,
                                       pUnpinData,
                                       0,
                                       &dwThreadID);
                if (hThread)
                {
                    // The thread will delete pUnpinData and pUnpinData->pNamelist
                    pfnl = NULL;

                    // We give the async thread a little time to complete, during which we
                    // put up the busy cursor.  This is solely to let the user see that
                    // some work is being done...
                    HCURSOR hCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                    WaitForSingleObject(hThread, 750);
                    CloseHandle(hThread);
                    SetCursor(hCur);
                }
                else
                {
                    LocalFree(pUnpinData);
                }
            }

            // Clear the flags to prevent sync below
            dwUpdateFlags = 0;
        }
        break;

    case 1: // Synchronize
        // Set the flags for a full sync
        dwUpdateFlags = CSC_UPDATE_SELECTION | CSC_UPDATE_STARTNOW
                            | CSC_UPDATE_REINT | CSC_UPDATE_FILL_ALL
                            | CSC_UPDATE_SHOWUI_ALWAYS | CSC_UPDATE_NOTIFY_DONE;
        break;
    }

    //
    // Update the files we are pinning or synchronizing.
    // Setting the "ignore access" flag will cause us to ignore the 
    // user/guest/other access info and sync all selected files.  We want
    // this behavior as the operation was initiated by a user's explicit
    // selection of files/folders in explorer.
    //
    if (dwUpdateFlags && pfnl->GetFileCount())
    {
        if (!::IsSyncInProgress())
        {
            hr = CscUpdateCache(dwUpdateFlags | CSC_UPDATE_IGNORE_ACCESS, pfnl);
        }
        else
        {
            //
            // A sync is in progress.  Tell user why they can't currently
            // pin or sync.
            //
            const UINT rgidsMsg[] = { IDS_CANTPIN_SYNCINPROGRESS,
                                      IDS_CANTSYNC_SYNCINPROGRESS };

            CscMessageBox(lpcmi->hwnd, 
                          MB_OK | MB_ICONINFORMATION, 
                          g_hInstance, 
                          rgidsMsg[iCmd]);
        }
    }

exit_gracefully:

    delete pfnl;

    TraceLeaveResult(hr);
}


//
//  FUNCTION: IContextMenu::GetCommandString(UINT, UINT, UINT, LPSTR, UINT)
//
//  PURPOSE: Called by the shell after the user has selected on of the
//           menu items that was added in QueryContextMenu().
//
//  PARAMETERS:
//    lpcmi - Pointer to an CMINVOKECOMMANDINFO structure
//
//  RETURN VALUE:
//    HRESULT signifying success or failure.
//
//  COMMENTS:
//
STDMETHODIMP
CCscShellExt::GetCommandString(UINT_PTR iCmd,
                               UINT uFlags,
                               LPUINT /*reserved*/,
                               LPSTR pszString,
                               UINT cchMax)
{
    HRESULT hr = E_UNEXPECTED;

    if (uFlags == GCS_VALIDATE)
        hr = S_FALSE;

    if (iCmd > 1)
        return hr;

    hr = S_OK;

    if (uFlags == GCS_HELPTEXT)
    {
        LoadString(g_hInstance, iCmd ? IDS_HELP_UPDATE_SEL : IDS_HELP_PIN, (LPTSTR)pszString, cchMax);
    }
    else if (uFlags == GCS_VERB)
    {
        lstrcpyn((LPTSTR)pszString, iCmd ? TEXT(STR_SYNC_VERB) : ((m_dwUIStatus & CSC_PROP_PINNED) ? TEXT(STR_UNPIN_VERB) : TEXT(STR_PIN_VERB)), cchMax);
    }
    else if (uFlags != GCS_VALIDATE)
    {
        // Must be some other flag that we don't handle
        hr = E_NOTIMPL;
    }

    return hr;
}


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Shell extension object implementation (IShellIconOverlayIdentifier)       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CCscShellExt::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
{
    HRESULT hr = S_FALSE;  // assume not pinned
    DWORD dwHintFlags;
    DWORD dwErr;
    LPTSTR pszUNC = NULL;
    LPTSTR pszSlash;

    USES_CONVERSION;

    //
    // Make sure we have a UNC path
    //
    GetRemotePath(W2CT(pwszPath), &pszUNC);
    if (!pszUNC)
        return S_FALSE;

    //
    // Ask CSC if this is a pinned file
    //
    dwHintFlags = 0;
    if (CSCQueryFileStatus(pszUNC, NULL, NULL, &dwHintFlags))
    {
        if (dwHintFlags & (FLAG_CSC_HINT_PIN_USER | FLAG_CSC_HINT_PIN_ADMIN))
            hr = S_OK;
    }
    else
    {
        dwErr = GetLastError();
        if (ERROR_FILE_NOT_FOUND != dwErr)
        {
            //
            // Need to check for 0 to accomodate GetLastError
            // returning 0 on CSCQueryFileStatus failure.
            // I'll talk to Shishir about getting this fixed.
            // [brianau - 5/13/99]
            //
            // Most of these were fixed for Windows 2000. If we
            // hit the assertion below we should file a bug and
            // get Shishir to fix it.
            // [jeffreys - 1/24/2000]
            //
            if (0 == dwErr)
            {
                ASSERTMSG(FALSE, "CSCQueryFileStatus failed with error = 0");
                dwErr = ERROR_GEN_FAILURE;
            }

            hr = HRESULT_FROM_WIN32(dwErr);
        }
    }

    DWORD dwAttribTest = FILE_ATTRIBUTE_ENCRYPTED;
    if (!CConfig::GetSingleton().AlwaysPinSubFolders())
        dwAttribTest |= FILE_ATTRIBUTE_DIRECTORY;
    
    if (S_FALSE == hr && !(dwAttrib & dwAttribTest))
    {
        //
        // Check to see if pinning is disallowed by system policy.
        //
        hr = m_NoPinList.IsPinAllowed(pszUNC);
        if (S_OK == hr)
        {
            hr = S_FALSE; // Reset
            //
            // If we get here, then either CSCQueryFileStatus succeeded but the file
            // isn't pinned, or the file isn't in the cache (ERROR_FILE_NOT_FOUND).
            // Also, policy allows pinning of this file/folder.
            //
            // Check whether the parent folder has the pin-inherit-user or
            // admin-pin flag and pin this file if necessary.
            //
            // Note that we don't pin encrypted files here.
            // Also note that pinning of folder is policy-dependent.  The default
            // behavior is to NOT pin folders (only files).  If the 
            // "AlwaysPinSubFolders" policy is set, we will pin folders.
            //
            pszSlash = PathFindFileName(pszUNC);
            if (pszSlash && pszUNC != pszSlash)
            {
                --pszSlash;
                *pszSlash = TEXT('\0'); // truncate the path

                // Check the parent status
                if (CSCQueryFileStatus(pszUNC, NULL, NULL, &dwHintFlags) &&
                    (dwHintFlags & (FLAG_CSC_HINT_PIN_USER | FLAG_CSC_HINT_PIN_ADMIN)))
                {
                    // The parent is pinned, so pin this file with the same flags
                    if (dwHintFlags & FLAG_CSC_HINT_PIN_USER)
                        dwHintFlags |= FLAG_CSC_HINT_PIN_INHERIT_USER;

                    // Restore the rest of the path
                    *pszSlash = TEXT('\\');
            
                    //
                    // To avoid a nasty race condition between purging and auto-pinning we need
                    // to disable auto-pinning when a purge is in progress.  The race condition
                    // can occur if a shell folder for the files being purged is open.  We purge
                    // a file and send out a change notify.  The shell updates the icon overlay
                    // and calls our overlay handler to remove the overlay.  Our handler notices 
                    // that the parent folder is pinned so we re-pin the file which places it 
                    // back in the cache.  Ugh... [brianau - 11/01/99]
                    //
                    // p.s.:  Note that this check calls WaitForSingleObject so we only
                    //        do it AFTER we're sure that we want to pin the file.  We don't
                    //        want to do the "wait" and THEN decide the file should not be
                    //        pinned because it's not a UNC path or it's a directory.
                    //
                    if (!IsPurgeInProgress())
                    {
                        if (CSCPinFile(pszUNC, dwHintFlags, NULL, NULL, NULL))
                            hr = S_OK;
                    }
                }
            }
        }
    }

    LocalFreeString(&pszUNC);

    return hr;
}

STDMETHODIMP
CCscShellExt::GetOverlayInfo (LPWSTR pwszIconFile,
                              int cchMax,
                              int * pIndex,
                              DWORD * pdwFlags)
{

    if (cchMax < (lstrlen(c_szDllName) + 1))
    {
        return E_OUTOFMEMORY;
    }

#ifdef UNICODE
    lstrcpyn (pwszIconFile, c_szDllName, cchMax);
#else
    MultiByteToWideChar (CP_ACP, 0, c_szDllName, -1, pwszIconFile, cchMax);
#endif

    // Using the ID doesn't work on Win95. We're currently not shipping
    // on Win95, but if that changes, we may need to fix this.  The overlay icon
    // is deliberately early in the DLL (right after the CSC icon) so the index
    // should always be one, but using the ID is still preferable.
#ifdef WINNT
    // Use positive #'s for indexes, negative for ID's
    *pIndex = -IDI_PIN_OVERLAY;
#else
    *pIndex = 1;
#endif
    *pdwFlags = (ISIOI_ICONFILE | ISIOI_ICONINDEX);

    return S_OK;
}

STDMETHODIMP
CCscShellExt::GetPriority (int * pIPriority)
{
    *pIPriority = 1;

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CCscShellExt implementation                                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

BOOL
ShareIsCacheable(LPCTSTR pszUNC, BOOL bPathIsFile, LPTSTR *ppszConnectionName, PDWORD pdwShareStatus)
{
    TCHAR szShare[MAX_PATH];
    DWORD dwShareStatus = 0;

    *ppszConnectionName = NULL;

    // CSCQueryFileStatus can fail for multiple reasons, one of which is that
    // there is no database entry and no existing SMB connection to the share.
    // To handle the no-connection part, we try to connect to the share and
    // retry CSCQueryFileStatus.
    //
    // However, there may be a non-SMB connnection which the SMB RDR doesn't
    // know about, so we have to check for a connection first. If there is a
    // non-SMB connection and we connect again, we would end up disconnecting
    // the pre-existing connection later, since we think we made the connection.
    //
    // If there is a non-SMB connection, then caching is not possible.
    //
    // Note that we can get here without a connection in at least 3 ways:
    // 1. When exploring on \\server and the context menu is for \\server\share.
    // 2. When checking a link target, which may live on a different server
    //    than what we're exploring.
    // 3. When checking a folder which is a DFS junction (we need to connect
    //    to the 'child' share).


    // Use a deep path to get correct results in DFS scenarios, but strip the
    // filename if it's not a directory.
    lstrcpyn(szShare, pszUNC, ARRAYSIZE(szShare));
    if (bPathIsFile)
    {
        PathRemoveFileSpec(szShare);
    }

    // CSCQueryShareStatus is currently unable to return permissions in
    // some cases (e.g. DFS), so don't use the permission parameters.
    if (!CSCQueryShareStatus(szShare, &dwShareStatus, NULL, NULL, NULL, NULL))
    {
        if (!ShareIsConnected(szShare) && ConnectShare(szShare, ppszConnectionName))
        {
            if (!CSCQueryShareStatus(szShare, &dwShareStatus, NULL, NULL, NULL, NULL))
            {
                dwShareStatus = FLAG_CSC_SHARE_STATUS_NO_CACHING;

                // We're going to return FALSE; kill the connection
                if (*ppszConnectionName)
                {
                    WNetCancelConnection2(*ppszConnectionName, 0, FALSE);
                    LocalFreeString(ppszConnectionName);
                }
            }
        }
        else
        {
            dwShareStatus = FLAG_CSC_SHARE_STATUS_NO_CACHING;
        }
    }

    *pdwShareStatus = dwShareStatus;

    return !((dwShareStatus & FLAG_CSC_SHARE_STATUS_CACHING_MASK) == FLAG_CSC_SHARE_STATUS_NO_CACHING);
}

BOOL
IsSameServer(LPCTSTR pszUNC, LPCTSTR pszServer)
{
    ULONG nLen;
    LPTSTR pszSlash;

    pszUNC += 2;    // Skip leading backslashes

    pszSlash = StrChr(pszUNC, TEXT('\\'));
    if (pszSlash)
        nLen = (ULONG)(pszSlash - pszUNC);
    else
        nLen = lstrlen(pszUNC);

    return (CSTR_EQUAL == CompareString(LOCALE_SYSTEM_DEFAULT,
                                        NORM_IGNORECASE,
                                        pszUNC,
                                        nLen,
                                        pszServer,
                                        -1));
}

STDMETHODIMP
CCscShellExt::CheckOneFileStatus(LPCTSTR pszItem,
                                 DWORD   dwAttr,        // SFGAO_* flags
                                 BOOL    bShareChecked,
                                 LPDWORD pdwStatus)     // CSC_PROP_* flags
{
    HRESULT hr = S_OK;
    LPTSTR pszConnectionName = NULL;
    DWORD dwHintFlags = 0;

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::CheckOneFileStatus");
    TraceAssert(pszItem && *pszItem);
    TraceAssert(pdwStatus);

    if (!PathIsUNC(pszItem))
        ExitGracefully(hr, HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME), "Not a network path");

    // If server is local machine, fail.  Don't allow someone to
    // cache a local path via a net share.
    if (IsSameServer(pszItem, m_szLocalMachine))
        ExitGracefully(hr, HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME), "Locally redirected path");

    // Check whether the share is cacheable
    // To handle DFS correctly, we need to re-check share status for folders,
    // since they may be DFS junction points and have different cache settings.
    if (!bShareChecked || (dwAttr & SFGAO_FOLDER))
    {
        DWORD dwShareStatus = 0;

        if (!ShareIsCacheable(pszItem, !(dwAttr & SFGAO_FOLDER), &pszConnectionName, &dwShareStatus))
            ExitGracefully(hr, E_FAIL, "Share not cacheable");

        if (dwShareStatus & FLAG_CSC_SHARE_STATUS_DISCONNECTED_OP)
            *pdwStatus |= CSC_PROP_DCON_MODE;
    }

    // Check the file status
    if (!CSCQueryFileStatus(pszItem, NULL, NULL, &dwHintFlags))
    {
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_FILE_NOT_FOUND)
        {
            if (NO_ERROR == dwErr)
                dwErr = ERROR_GEN_FAILURE;
            ExitGracefully(hr, HRESULT_FROM_WIN32(dwErr), "CSCQueryFileStatus failed");
        }
    }
    else
    {
        if (dwAttr & SFGAO_FOLDER)
        {
            // CSCQueryFileStatus succeeded, so this folder is in the cache.
            // Enable the sync menu.
            if (PathIsRoot(pszItem))
            {
                // Special note for "\\server\share" items: CSCQueryFileStatus
                // can succeed even if nothing on the share is cached. Only
                // enable CSC_PROP_SYNCABLE if something on this share is cached.
                CSCSHARESTATS shareStats;
                CSCGETSTATSINFO si = { SSEF_NONE,  // No exclusions
                                       SSUF_TOTAL, // Interested in total only.
                                       false,      // No access info reqd (faster).
                                       false };     

                _GetShareStatisticsForUser(pszItem, &si, &shareStats);
                if (shareStats.cTotal)
                    *pdwStatus |= CSC_PROP_SYNCABLE;
            }
            else
            {
                *pdwStatus |= CSC_PROP_SYNCABLE;
            }
        }

        const bool bPinSubFolders = CConfig::GetSingleton().AlwaysPinSubFolders();
        if (!(*pdwStatus & CSC_PROP_INHERIT_PIN) && 
            (!(dwAttr & SFGAO_FOLDER) || bPinSubFolders))
        {
            TCHAR szParent[MAX_PATH];
            DWORD dwParentHints = 0;

            // It's a file OR it's a folder and the "AlwaysPinSubFolders" 
            // policy is set.. Check whether the parent is pinned.
            lstrcpyn(szParent, pszItem, ARRAYSIZE(szParent));
            PathRemoveFileSpec(szParent);
            if (CSCQueryFileStatus(szParent, NULL, NULL, &dwParentHints)
                && (dwParentHints & FLAG_CSC_HINT_PIN_USER))
            {
                *pdwStatus |= CSC_PROP_INHERIT_PIN;
            }
        }
    }

    // If it's not pinned, turn off pinned flag
    if (0 == (dwHintFlags & FLAG_CSC_HINT_PIN_USER))
        *pdwStatus &= ~CSC_PROP_PINNED;

    // If it's not admin pinned, turn off admin pinned flag
    if (0 == (dwHintFlags & FLAG_CSC_HINT_PIN_ADMIN))
        *pdwStatus &= ~CSC_PROP_ADMIN_PINNED;

exit_gracefully:

    if (pszConnectionName)
    {
        WNetCancelConnection2(pszConnectionName, 0, FALSE);
        LocalFreeString(&pszConnectionName);
    }

    TraceLeaveResult(hr);
}

BOOL
_PathIsUNCServer(LPCTSTR pszPath)
{
    int i;

    if (!pszPath)
        return FALSE;

    for (i = 0; *pszPath; pszPath++ )
    {
        if (pszPath[0]==TEXT('\\') && pszPath[1]) // don't count a trailing slash
        {
            i++;
        }
    }

    return (i == 2);
}

STDMETHODIMP CCscShellExt::CheckFileStatus(IDataObject *pdobj, DWORD *pdwStatus)        // CSC_PROP_* flags
{
    LPTSTR pszConnectionName = NULL;
    UINT i;
    BOOL bShareOK = FALSE;
    TCHAR szItem[MAX_PATH];
    CIDArray ida;

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::CheckFileStatus");
    TraceAssert(pdobj != NULL);
    TraceAssert(IsCSCEnabled());

    if (pdwStatus)
        *pdwStatus = 0;

    // Assume that everything is both user and system pinned.  If anything
    // is not pinned, clear the appropriate flag and treat the entire
    // selection as non-pinned.
    DWORD dwStatus = CSC_PROP_PINNED | CSC_PROP_ADMIN_PINNED;

    HRESULT hr = ida.Initialize(pdobj);
    FailGracefully(hr, "Can't get ID List format from data object");

    if (ida.Count() > 1)
        dwStatus |= CSC_PROP_MULTISEL;

    // Check the parent path
    hr = ida.GetFolderPath(szItem, ARRAYSIZE(szItem));
    FailGracefully(hr, "No parent path");

    if (ida.Count() > 1 && PathIsUNC(szItem) && !_PathIsUNCServer(szItem))
    {
        DWORD dwShareStatus = 0;

        if (!ShareIsCacheable(szItem, FALSE, &pszConnectionName, &dwShareStatus))
            ExitGracefully(hr, E_FAIL, "Share not cacheable");

        if (dwShareStatus & FLAG_CSC_SHARE_STATUS_DISCONNECTED_OP)
            dwStatus |= CSC_PROP_DCON_MODE;

        // No need to check share status again inside CheckOneFileStatus
        bShareOK = TRUE;
    }

    // Loop over each selected item
    for (i = 0; i < ida.Count(); i++)
    {
        // Get the attributes
        DWORD dwAttr = SFGAO_FILESYSTEM | SFGAO_LINK | SFGAO_FOLDER;
        hr = ida.GetItemPath(i, szItem, ARRAYSIZE(szItem), &dwAttr);
        FailGracefully(hr, "Unable to get item attributes");

        if (!(dwAttr & SFGAO_FILESYSTEM))
            ExitGracefully(hr, E_FAIL, "Not a filesystem object");

        // Is it a shortcut?
        if (dwAttr & SFGAO_LINK)
        {
            LPTSTR pszTarget = NULL;

            // Check the target
            GetLinkTarget(szItem, &pszTarget);
            if (pszTarget)
            {
                hr = CheckOneFileStatus(pszTarget, 0, FALSE, &dwStatus);
                LocalFreeString(&pszTarget);

                if (SUCCEEDED(hr) && !PathIsUNC(szItem))
                {
                    // The link is local, but the target is remote, so don't
                    // bother checking status of the link itself.  Just go
                    // with the target status and move on to the next item.
                    continue;
                }
            }
        } 

        hr = CheckOneFileStatus(szItem, dwAttr, bShareOK, &dwStatus);
        FailGracefully(hr, "File not cacheable");
    }

exit_gracefully:

    if (pszConnectionName)
    {
        WNetCancelConnection2(pszConnectionName, 0, FALSE);
        LocalFreeString(&pszConnectionName);
    }

    if (SUCCEEDED(hr) && pdwStatus != NULL)
        *pdwStatus = dwStatus;

    TraceLeaveResult(hr);
}


//
// Determines if a folder has subfolders.
// Returns:
//      S_OK          = Has subfolders.
//      S_FALSE       = No subfolders.
//      E_OUTOFMEMORY = Insufficient memory.
//
HRESULT
CCscShellExt::FolderHasSubFolders(
    LPCTSTR pszPath,
    CscFilenameList *pfnl
    )
{
    if (NULL == pszPath || TEXT('\0') == *pszPath)
        return E_INVALIDARG;

    HRESULT hr = S_FALSE;
    const TCHAR szWildcard[] = TEXT("*.*");
    UINT cchFolder = lstrlen(pszPath) + 1;  // +1 for '\\'
    LPTSTR pszTemp = (LPTSTR)LocalAlloc(LPTR, (cchFolder + MAX_PATH + 1) * sizeof(TCHAR));
    if (NULL != pszTemp)
    {
        PathCombine(pszTemp, pszPath, szWildcard);
        WIN32_FIND_DATA fd;
        HANDLE hFind = FindFirstFile(pszTemp, &fd);
        if (INVALID_HANDLE_VALUE != hFind)
        {
            do
            {
                if ((FILE_ATTRIBUTE_DIRECTORY & fd.dwFileAttributes) && !PathIsDotOrDotDot(fd.cFileName))
                {
                    if (IsHiddenSystem(fd.dwFileAttributes))
                    {
                        // This subfolder is "super hidden".  Build the full path
                        // and silently add it to the file list, but don't set the
                        // result to S_OK (we don't want superhidden subfolders to
                        // cause prompts).
                        lstrcpy(&pszTemp[cchFolder], fd.cFileName);
                        pfnl->AddFile(pszTemp, true);
                    }
                    else
                        hr = S_OK;  // don't break, there may be superhidden folders
                }
            }
            while(FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        LocalFree(pszTemp);
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}


STDMETHODIMP CCscShellExt::BuildFileList(IDataObject *pdobj, HWND hwndOwner, 
                                         CscFilenameList *pfnl, LPBOOL pbSubFolders)
{
    UINT i;
    TCHAR szItem[MAX_PATH];
    CIDArray ida;
    BOOL bDirectory;

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::BuildFileList");
    TraceAssert(pdobj != NULL);
    TraceAssert(pfnl != NULL);

    HCURSOR hCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    HRESULT hr = ida.Initialize(pdobj);
    FailGracefully(hr, "Can't get ID List format from data object");

    // Loop over each selected item
    for (i = 0; i < ida.Count(); i++)
    {
        // Get the attributes
        DWORD dwAttr = SFGAO_FILESYSTEM | SFGAO_LINK | SFGAO_FOLDER;
        hr = ida.GetItemPath(i, szItem, ARRAYSIZE(szItem), &dwAttr);
        FailGracefully(hr, "Unable to get item attributes");

        if (!(dwAttr & SFGAO_FILESYSTEM))
            continue;

        // Is it a shortcut?
        if (dwAttr & SFGAO_LINK)
        {
            LPTSTR pszTarget = NULL;

            // Check the target
            GetLinkTarget(szItem, &pszTarget);
            if (pszTarget)
            {
                // Add the target to the file list
                if (!pfnl->FileExists(pszTarget, false))
                    pfnl->AddFile(pszTarget, false);

                LocalFreeString(&pszTarget);
            }
        } 

        bDirectory = (dwAttr & SFGAO_FOLDER);

        if (pbSubFolders && bDirectory && !*pbSubFolders)
            *pbSubFolders = (S_OK == FolderHasSubFolders(szItem, pfnl));

        // Add the item to the file list
        pfnl->AddFile(szItem, !!bDirectory);

        // If it's an html file, look for a directory of the same name
        // and add it to the file list if necessary.
        //
        // We're supposed to look for a localized version of "Files"
        // tacked on to the root name.  For example, given "foo.htm" we
        // should look for a directory named "foo Files" where the "Files"
        // part comes from a list of localized strings provided by Office.
        // We don't bother and just look for a directory named "foo".
        //
        if (!bDirectory && PathIsHTMLFile(szItem))
        {
            // Truncate the path
            LPTSTR pszExtn = PathFindExtension(szItem);
            if (pszExtn)
                *pszExtn = NULL;

            // Check for existence
            dwAttr = GetFileAttributes(szItem);

            if ((DWORD)-1 != dwAttr && (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
                pfnl->AddFile(szItem, true);
        }
    }

exit_gracefully:

    SetCursor(hCur);

    TraceLeaveResult(hr);
}


#define _WNET_ENUM_BUFFER_SIZE      4000

BOOL
ShareIsConnected(LPCTSTR pszUNC)
{
    HANDLE hEnum;
    PVOID  pBuffer;
    BOOL   fShareIsConnected  = FALSE;

    pBuffer = (PVOID)LocalAlloc(LMEM_FIXED, _WNET_ENUM_BUFFER_SIZE);

    if (NULL != pBuffer)
    {
        //
        // Enumerate all connected disk resources
        //
        if (NO_ERROR == WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_DISK, 0, NULL, &hEnum))
        {
            //
            // Look at each connected share.  If we find the share we're looking for,
            // we know it's connected so we can quit looking.
            //
            while (!fShareIsConnected)
            {
                LPNETRESOURCE pnr;
                DWORD cEnum = (DWORD)-1;
                DWORD dwBufferSize = _WNET_ENUM_BUFFER_SIZE;

                if (NO_ERROR != WNetEnumResource(hEnum, &cEnum, pBuffer, &dwBufferSize))
                    break;

                for (pnr = (LPNETRESOURCE)pBuffer; cEnum > 0; cEnum--, pnr++)
                {
                    if (NULL != pnr->lpRemoteName &&
                        0 == lstrcmpi(pnr->lpRemoteName, pszUNC))
                    {
                        // Found it
                        fShareIsConnected = TRUE;
                        break;
                    }
                }
            }

            WNetCloseEnum(hEnum);
        }
        LocalFree(pBuffer);
    }

    return fShareIsConnected;
}


BOOL
ConnectShare(LPCTSTR pszUNC, LPTSTR *ppszAccessName)
{
    NETRESOURCE nr;
    DWORD dwResult;
    DWORD dwErr;
    TCHAR szAccessName[MAX_PATH];
    DWORD cchAccessName = ARRAYSIZE(szAccessName);

    TraceEnter(TRACE_SHELLEX, "CCscShellExt::ConnectShare");
    TraceAssert(pszUNC && *pszUNC);

    nr.dwType = RESOURCETYPE_DISK;
    nr.lpLocalName = NULL;
    nr.lpRemoteName = (LPTSTR)pszUNC;
    nr.lpProvider = NULL;

    szAccessName[0] = TEXT('\0');

    dwErr = WNetUseConnection(NULL,
                              &nr,
                              NULL,
                              NULL,
                              0,
                              szAccessName,
                              &cchAccessName,
                              &dwResult);

    Trace((TEXT("Connecting %s (%d)"), pszUNC, dwErr));

    if (ppszAccessName && NOERROR == dwErr)
    {
        LocalAllocString(ppszAccessName, szAccessName);
    }

    TraceLeaveValue(NOERROR == dwErr);
}


DWORD WINAPI
CCscShellExt::_UnpinFilesThread(LPVOID pvThreadData)
{
    CSC_UNPIN_DATA *pUnpinData = reinterpret_cast<CSC_UNPIN_DATA *>(pvThreadData);
    if (pUnpinData)
    {
        HINSTANCE hInstThisDll = LoadLibrary(c_szDllName);
        if (hInstThisDll)
        {
            CscUnpinFileList(pUnpinData->pNamelist,
                            (pUnpinData->dwUpdateFlags & CSC_UPDATE_PIN_RECURSE),
                            pUnpinData->bOffline,
                            NULL, NULL, 0);
        }

        delete pUnpinData->pNamelist;
        LocalFree(pUnpinData);

        if (hInstThisDll)
        {
            FreeLibraryAndExitThread(hInstThisDll, 0);
        }
    }
    return 0;
}


INT_PTR CALLBACK
CCscShellExt::_ConfirmPinDlgProc(HWND hDlg,
                                 UINT uMsg,
                                 WPARAM wParam,
                                 LPARAM lParam)
{
    INT_PTR bResult = TRUE;
    switch (uMsg)
    {
    case WM_INITDIALOG:
        CheckRadioButton(hDlg, IDC_PIN_NO_RECURSE, IDC_PIN_RECURSE, IDC_PIN_RECURSE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            break;

        case IDOK:
            // Return IDYES to indicate that the operation should be recursive.
            // Return IDNO to indicate no recursion.
            EndDialog(hDlg, BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_PIN_RECURSE) ? IDYES : IDNO);
            break;
        }
        break;

    default:
        bResult = FALSE;    // message not handled
    }

    return bResult;
}


//
// Given an IDataObject ptr representing a selection of files from 
// within the shell, this function determins if pinning of any of 
// the files and directories is disallowed via system policy.
//
// Returns: S_OK    - All files in data object can be pinned.
//          S_FALSE - At least one file in data object cannot be pinned.
//
HRESULT
CCscShellExt::CanAllFilesBePinned(
    IDataObject *pdtobj
    )
{
    TraceEnter(TRACE_SHELLEX, "CCscShellExt::CanAllFilesBePinned");

    //
    // Quick check to see if ANY pin restrictions are in place.
    //
    HRESULT hr = m_NoPinList.IsAnyPinDisallowed();
    if (S_OK == hr)
    {
        //
        // Yes, at least one restriction was read from registry.
        //
        CscFilenameList fnl;
        hr = BuildFileList(m_lpdobj, 
                           GetDesktopWindow(),
                           &fnl,
                           NULL);
        if (SUCCEEDED(hr))
        {
            //
            // Iterate over all UNC paths in the data object
            // until we either exhaust the list or find one for which
            // pinning is disallowed.
            //
            CscFilenameList::ShareIter si = fnl.CreateShareIterator();
            CscFilenameList::HSHARE hShare;

            while(si.Next(&hShare))
            {
                TCHAR szUncPath[MAX_PATH];
                wnsprintf(szUncPath, ARRAYSIZE(szUncPath), TEXT("%s\\"), fnl.GetShareName(hShare));
                const int cchShare = lstrlen(szUncPath);

                CscFilenameList::FileIter fi = fnl.CreateFileIterator(hShare);

                LPCTSTR pszFile;
                while(NULL != (pszFile = fi.Next()))
                {
                    //
                    // Assemble the full UNC path string.
                    // If the item is a directory, will need to truncate the trailing
                    // "\*" characters.
                    //
                    lstrcpyn(szUncPath + cchShare, pszFile, ARRAYSIZE(szUncPath) - cchShare);
                    LPTSTR pszEnd = szUncPath + lstrlen(szUncPath) - 1;
                    while(pszEnd > szUncPath && (TEXT('\\') == *pszEnd || TEXT('*') == *pszEnd))
                    {
                        *pszEnd-- = TEXT('\0');
                    }
                    if (S_FALSE == m_NoPinList.IsPinAllowed(szUncPath))
                    {
                        Trace((TEXT("Policy prevents pinning of \"%s\""), szUncPath));
                        TraceLeaveResult(S_FALSE);
                    }
                }
            }
        }
    }
    TraceLeaveResult(SUCCEEDED(hr) ? S_OK : hr);
}


//
// Support for recursively unpinning a tree with progress updates
//
typedef struct _UNPIN_FILES_DATA
{
    BOOL                    bSubfolders;
    BOOL                    bOffline;
    PFN_UNPINPROGRESSPROC   pfnProgressCB;
    LPARAM                  lpContext;
} UNPIN_FILES_DATA, *PUNPIN_FILES_DATA;

DWORD WINAPI
_UnpinCallback(LPCTSTR             pszItem,
               ENUM_REASON         eReason,
               DWORD               /*dwStatus*/,
               DWORD               dwHintFlags,
               DWORD               dwPinCount,
               LPWIN32_FIND_DATA   pFind32,
               LPARAM              lpContext)
{
    PUNPIN_FILES_DATA pufd = reinterpret_cast<PUNPIN_FILES_DATA>(lpContext);

    // Skip folders if we aren't recursing
    if (eReason == ENUM_REASON_FOLDER_BEGIN && !pufd->bSubfolders)
        return CSCPROC_RETURN_SKIP;

    // Update progress
    if (pufd->pfnProgressCB)
    {
        DWORD dwResult = (*pufd->pfnProgressCB)(pszItem, pufd->lpContext);
        if (CSCPROC_RETURN_CONTINUE != dwResult)
            return dwResult;
    }

    // Unpin the item if it's pinned.  For folders,
    // do this before recursing.
    if ((eReason == ENUM_REASON_FILE || eReason == ENUM_REASON_FOLDER_BEGIN)
        && (dwHintFlags & FLAG_CSC_HINT_PIN_USER))
    {
        CSCUnpinFile(pszItem,
                     FLAG_CSC_HINT_PIN_USER | FLAG_CSC_HINT_PIN_INHERIT_USER,
                     NULL,
                     NULL,
                     &dwHintFlags);

        ShellChangeNotify(pszItem, pFind32, FALSE);
    }

    // Delete items that are no longer pinned.  For folders,
    // do this after recursing.
    if (eReason == ENUM_REASON_FILE || eReason == ENUM_REASON_FOLDER_END)
    {
        if (!dwHintFlags && !dwPinCount)
        {
            if (NOERROR == CscDelete(pszItem) && pufd->bOffline)
            {
                // Removing from the cache while in offline mode means
                // it's no longer available, so remove it from view.
                ShellChangeNotify(pszItem,
                                  pFind32,
                                  FALSE,
                                  (pFind32->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SHCNE_RMDIR : SHCNE_DELETE);
            }
        }
    }

    return CSCPROC_RETURN_CONTINUE;
}

DWORD
_UnpinOneShare(CscFilenameList *pfnl,
               CscFilenameList::HSHARE hShare,
               PUNPIN_FILES_DATA pufd)
{
    DWORD dwResult = CSCPROC_RETURN_CONTINUE;
    LPCTSTR pszFile;
    LPCTSTR pszShare = pfnl->GetShareName(hShare);
    CscFilenameList::FileIter fi = pfnl->CreateFileIterator(hShare);

    // Iterate over the filenames associated with the share.
    while (pszFile = fi.Next())
    {
        TCHAR szFullPath[MAX_PATH];
        TCHAR szRelativePath[MAX_PATH];
        WIN32_FIND_DATA fd;
        ULONG cchFile = lstrlen(pszFile) + 1; // include NULL
        DWORD dwPinCount = 0;
        DWORD dwHintFlags = 0;

        ZeroMemory(&fd, sizeof(fd));
        fd.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

        // Directories have a trailing "\*"
        if (StrChr(pszFile, TEXT('*')))
        {
            // It's a directory. Trim off the "\*"
            cchFile -= 2;
            fd.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

            // When unpinning at the share level, pszFile points to "*"
            // and cchFile is now zero.
        }

        szRelativePath[0] = TEXT('\0');
        lstrcpyn(szRelativePath, pszFile, (int)min(cchFile, ARRAYSIZE(szRelativePath)));

        // Build the full path
        PathCombine(szFullPath, pszShare, szRelativePath);

        pszFile = PathFindFileName(szFullPath);
        lstrcpyn(fd.cFileName, pszFile ? pszFile : szFullPath, ARRAYSIZE(fd.cFileName));

        // Update progress
        if (pufd->pfnProgressCB)
        {
            dwResult = (*pufd->pfnProgressCB)(szFullPath, pufd->lpContext);
            switch (dwResult)
            {
            case CSCPROC_RETURN_SKIP:
                continue;
            case CSCPROC_RETURN_ABORT:
                break;
            }
        }

        // Unpin it
        CSCUnpinFile(szFullPath,
                     FLAG_CSC_HINT_PIN_USER | FLAG_CSC_HINT_PIN_INHERIT_USER,
                     NULL,
                     &dwPinCount,
                     &dwHintFlags);

        ShellChangeNotify(szFullPath, &fd, FALSE);

        // If it's a directory, unpin its contents
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            _CSCEnumDatabase(szFullPath,
                             pufd->bSubfolders,
                             _UnpinCallback,
                             (LPARAM)pufd);
        }

        // Is it still pinned?
        if (!dwHintFlags && !dwPinCount)
        {
            // Remove it from the cache (folders may still contain children
            // so we expect this to fail sometimes).
            if (NOERROR == CscDelete(szFullPath) && pufd->bOffline)
            {
                // Removing from the cache while in offline mode means
                // it's no longer available, so remove it from view.
                ShellChangeNotify(szFullPath,
                                  &fd,
                                  FALSE,
                                  (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SHCNE_RMDIR : SHCNE_DELETE);
            }
        }
    }

    return dwResult;
}

void
CscUnpinFileList(CscFilenameList      *pfnl,
                 BOOL                  bSubfolders,
                 BOOL                  bOffline,
                 LPCTSTR               pszShare,
                 PFN_UNPINPROGRESSPROC pfnProgressCB,
                 LPARAM                lpContext)
{
    UNPIN_FILES_DATA ufd;
    DWORD dwResult = CSCPROC_RETURN_CONTINUE;
    CscFilenameList::HSHARE hShare;

    if (NULL == pfnl || !pfnl->IsValid() || 0 == pfnl->GetFileCount())
        return;

    ufd.bSubfolders = bSubfolders;
    ufd.bOffline = bOffline;
    ufd.pfnProgressCB = pfnProgressCB;
    ufd.lpContext = lpContext;

    if (pszShare)   // enumerate this share only
    {
        if (pfnl->GetShareHandle(pszShare, &hShare))
            _UnpinOneShare(pfnl, hShare, &ufd);
    }
    else            // enumerate everything in the list
    {
        CscFilenameList::ShareIter si = pfnl->CreateShareIterator();

        while (si.Next(&hShare) && dwResult != CSCPROC_RETURN_ABORT)
        {
            dwResult = _UnpinOneShare(pfnl, hShare, &ufd);
        }
    }

    // Flush the shell notify queue
    ShellChangeNotify(NULL, TRUE);
}
