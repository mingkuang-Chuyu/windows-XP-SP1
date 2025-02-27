// Copyright (c) <1995-1999> Microsoft Corporation

#include "shellprv.h"
#pragma  hdrstop

#include <msi.h>
#include <msip.h>

#include <aclapi.h>     // for TreeResetNamedSecurityInfo

#include "shlwapip.h" // for SHGlobalCounterDecrement
#include "ynlist.h"

#define INTERNAL_COPY_ENGINE
#include "copy.h"
#include "shell32p.h"
#include "control.h"
#include "cdburn.h"
#include "propsht.h"
#include "prshtcpp.h"

#define REG_VAL_GENERAL_RENAMEHTMLFILE  TEXT("RenameHtmlFile")

#define TF_DEBUGCOPY 0x00800000

#define VERBOSE_STATUS

// REVIEW, we should tune this size down as small as we can
// to get smoother multitasking (without effecting performance)
#define COPYMAXBUFFERSIZE       0x10000 // 0xFFFF this is 32-bit code!
#define MIN_MINTIME4FEEDBACK    5       // is it worth showing estimated time to completion feedback?
#define MS_RUNAVG               10000   // ms, window for running average time to completion estimate
#define MS_TIMESLICE             2000    // ms, (MUST be > 1000!) first average time to completion estimate

#define MAXDIRDEPTH             128     // # of directories we will deal with recursivly

#define SHOW_PROGRESS_TIMEOUT   1000    // 1 second
#define MINSHOWTIME             1000    // 1 sec

// progress dialog message
#define PDM_SHUTDOWN     WM_APP
#define PDM_NOOP        (WM_APP + 1)
#define PDM_UPDATE      (WM_APP + 2)


#define OPER_MASK           0x0F00
#define OPER_ENTERDIR       0x0100
#define OPER_LEAVEDIR       0x0200
#define OPER_DOFILE         0x0300
#define OPER_ERROR          0x0400

#define FOFuncToStringID(wFunc) (IDS_UNDO_FILEOP + wFunc)

//
//  The following is a list of folder suffixes in all international languages. This list is NOT
// read from a resource because we do NOT want the strings in this list to be mistakenly localized.
// This list will allow NT5 shell to operate on files created by any international version of 
// office 9.
//  This list is taken from "http://officeweb/specs/webclient/files.htm"
//
//  WARNING: Do not localize the strings in this table. Do not make any changes to this table 
//  without consulting AlanRa (Office9 PM)
//
static const LPCTSTR c_apszSuffixes[] = 
{
    TEXT(".files"),
    TEXT("_files"),
    TEXT("-Dateien"),
    TEXT("_fichiers"),
    TEXT("_bestanden"),
    TEXT("_file"),
    TEXT("_archivos"),
    TEXT("-filer"),
    TEXT("_tiedostot"),
    TEXT("_pliki"),
    TEXT("_soubory"),
    TEXT("_elemei"),
    TEXT("_ficheiros"),
    TEXT("_arquivos"),
    TEXT("_dosyalar"),
    TEXT("_datoteke"),
    TEXT("_fitxers"),
    TEXT("_failid"),
    TEXT("_fails"),
    TEXT("_bylos"),
    TEXT("_fajlovi"),
    TEXT("_fitxategiak"),
};

// The reg value under HKCU\REGSTR_PATH_EXPLORER that specifies Connection ON/OFF switch
#define REG_VALUE_NO_FILEFOLDER_CONNECTION  TEXT("NoFileFolderConnection")

////////////////////////////////////////////////////////////////////////////
///// directory tree cache.


// this is set if pdtnChild has not been traversed (as opposed to NULL which means
// there are no children
#define DTN_DELAYED ((PDIRTREENODE)-1)


// DIRTREENODE is a node in a linked list/tree cache of the directory structure.
// except for the top level (which is specified by the caller of the api), the order
// are all files first, then all directories.

typedef struct _dirtreenode {

    struct _dirtreenode *pdtnNext; // sibling
    struct _dirtreenode *pdtnChild; // head of children linked list
    struct _dirtreenode *pdtnParent;

    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeLow;
    DWORD nFileSizeHigh;

    LARGE_INTEGER liFileSizeCopied;
    BOOL  fNewRoot : 1;
    BOOL  fDummy : 1;   // this marks the node as a dummy node (a wildcard that didn't match anything)
    BOOL  fConnectedElement : 1; // this marks the node as an element that was implicitly added
    // to the Move/Copy source list because of an office9 type of
    // connection established in the registry.

    //The following is a union because not all nodes need all the fields.
    union {
        // The following is valid only if fConnectedElement is FALSE.
        struct  _dirtreenode *pdtnConnected;  

        // The following structure is valid only if fConnectedElemet is TRUE.
        struct  {
            LPTSTR  pFromConnected;     // if fNewRoot && fConnectedElement, then these two elements
            LPTSTR  pToConnected;       // have the pFrom and pTo.
            DWORD   dwConfirmation;     // The result of confirnation given by end-user
        } ConnectedInfo;
    };

    TCHAR szShortName[14];
    TCHAR szName[1]; // this struct is dynamic

} DIRTREENODE, *PDIRTREENODE;

typedef struct {
    BOOL  fChanged;
    DWORD dwFiles; // number of files
    DWORD dwFolders; // number of folders
    LARGE_INTEGER liSize; // total size of all files
} DIRTOTALS, *PDIRTOTALS;

typedef struct {
    UINT oper;
    DIRTOTALS dtAll; // totals for all files
    DIRTOTALS dtDone; // totals of what's done
    BOOL fChangePosted;

    PDIRTREENODE pdtn; // first directory tree node
    PDIRTREENODE pdtnCurrent;
    PDIRTREENODE pdtnConnectedItems;  //Pointer to the begining of connected elements node.
    TCHAR    bDiskCheck[26];

    // how much does each operation cost in the progress...
    int iFilePoints;
    int iFolderPoints;
    int iSizePoints;


    LPTSTR pTo;  // this holds the top level target list
    LPTSTR pFrom; // this holds the top level source list
    BOOL    fMultiDest;

    TCHAR szSrcPath[MAX_PATH];
    TCHAR szDestPath[MAX_PATH]; // this is the current destination for pdtn and all it's children (not siblings)
    // lpszDestPath includes pdtn's first path component

    HDSA hdsaRenamePairs;

} DIRTREEHEADER, *PDIRTREEHEADER;

//  We spend a lot of time creating simple PIDLs, so use this cache
//  to speed things up.
typedef struct SIMPLEPIDLCACHE {
    IBindCtx *pbcFile;          // Empty filesys bind context for files
    IBindCtx *pbcFolder;        // Empty filesys bind context for folders
    IShellFolder *psfDesktop;   // Desktop folder (for ParseDisplayName)
    int iInit;                  // 0 = not inited; 1 = inited; -1 = init failed
    IShellFolder *psf;          // Current folder
    LPITEMIDLIST pidlFolder;    // Current folder
    TCHAR szFolder[MAX_PATH];   // Current folder
} SIMPLEPIDLCACHE, *PSIMPLEPIDLCACHE;

typedef struct {
    int          nRef;          // struct reference count

    int          nSourceFiles;
    LPTSTR       lpCopyBuffer; // global file copy buffer
    UINT         uSize;         // size of this buffer
    FILEOP_FLAGS fFlags;        // from SHFILEOPSTRUCT
    HWND         hwndProgress;  // dialog/progress window
    HWND         hwndDlgParent; // parent window for message boxes
    CONFIRM_DATA cd;            // confirmation stuff

    UNDOATOM    *lpua;           // the undo atom that this file operation will make
    BOOL        fNoConfirmRecycle;
    BOOL        bAbort;
    BOOL        fMerge;   // are we doing a merge of folders

    BOOL        fDone;
    BOOL        fProgressOk;
    BOOL        fDTBuilt;
    BOOL        fLostEncryptOk;
    BOOL        fFromCDRom;         // Clear readonly bits if copying from CDRom

    // folowing fields are used for giving estimated time for completion
    // feedback to the user during longer than MINTIME4FEEDBACK operations
    BOOL  fFlushWrites;     // Should we flush writes for destinations on slow links

    DWORD dwPreviousTime;       // calculate transfer rate
    int  iLastProgressPoints;   // how many progress points we had the last time we updated the time est
    DWORD dwPointsPerSec;
    LPCTSTR lpszProgressTitle;
    LPSHFILEOPSTRUCT lpfo;

    DIRTREEHEADER dth;
    BOOL        fInitialize;
    const WIN32_FIND_DATA* pfd;
    BOOL        bStreamLossPossible;    // Could stream loss happen in this directory?

    SIMPLEPIDLCACHE spc;
} COPY_STATE, *LPCOPY_STATE;

// we have a seperate struct that we pass off to the FOUIThread so that he can get to the pcs,
// but since the FOUIThread can outlive the main thread (!!) in some cases, we can't let him have a
// ref to pcs->lpfo since it is owned by SHFileOperations caller and we crash if we try to refrence
// it after SHFileOperation returns and the caller has freed the memory. The only two things the 
// FOUIThread uses out of the pcs->lpfo are the wFunc and the lpszProgressTitle (to see if the
// recycle bin was being emptied or not), so we make private copies of that info for the thread.
typedef struct {
    COPY_STATE* pcs;
    UINT wFunc;
    BOOL bIsEmptyRBOp;
} FOUITHREADINFO, *PFOUITHREADINFO;

// Information to determine folder's movability to the recycle bin
typedef struct {
    BOOL            bProcessedRoot; // tells if we are the first call in the recursive chain and we need to do root-specific processing
    int             cchBBDir;       // count of characters in the recycle bin dir (eg "C:\Recycler\<sid>")
    int             cchDelta;       // count of characters that the path will increase (or decrease if negative) by when moved under the recycle bin directory
    ULONGLONG       cbSize;         // size of the folder
    TCHAR           szNonDeletableFile[MAX_PATH];   // an output buffer that holds the name of the file that cannot be deleted, if one exists
    TCHAR           szPath[MAX_PATH];   // scratch buffer for stack savings when recursing
    WIN32_FIND_DATA fd;             // also for stack savings
} FOLDERDELETEINFO;


// function declarations
void _ProcessNameMappings(LPTSTR pszTarget, HDSA hdsaRenamePairs);
int GetNameDialog(HWND hwnd, COPY_STATE *pcs, BOOL fMultiple,UINT wOp, LPTSTR pFrom, LPTSTR pTo);
void AddRenamePairToHDSA(LPCTSTR pszOldPath, LPCTSTR pszNewPath, HDSA* phdsaRenamePairs);
BOOL FOQueryAbort(COPY_STATE *pcs);
UINT DTAllocConnectedItemNodes(PDIRTREEHEADER pdth, COPY_STATE *pcs, WIN32_FIND_DATA *pfd, LPTSTR pszPath, BOOL fRecurse, PDIRTREENODE *ppdtnConnectedItems);
void CALLBACK FOUndo_Invoke(UNDOATOM *lpua);
LONG CheckFolderSizeAndDeleteability(LPCTSTR pszDir, FOLDERDELETEINFO* pfdi, LPCOPY_STATE pcs);
BOOL DeleteFileBB(LPTSTR pszFile, INT *piReturn, COPY_STATE *pcs, BOOL fIsDir, WIN32_FIND_DATA *pfd, HDPA *phdpaDeletedFiles);


BOOL DTDiskCheck(PDIRTREEHEADER pdth, COPY_STATE *pcs, LPTSTR pszPath)
{
    int iDrive = PathGetDriveNumber(pszPath);

    if (iDrive != -1)
    {
        if (!pdth->bDiskCheck[iDrive])
        {
            HWND hwnd = pcs->hwndDlgParent;
            TCHAR szDrive[] = TEXT("A:\\");
            szDrive[0] += (CHAR)iDrive;

            // Sometimes pszPath is a dir and sometimes it's a file.  All we really care about is if the
            // drive is ready (inserted, formated, net path mapped, etc).  We know that we don't have a
            // UNC path because PathGetDriveNumber would have failed and we are already busted in terms
            // of mounted volumes, again because we use PathGetDriveNumber, so we don't have to worry about
            // these two cases.  As such we build the root path and use that instead.
            pdth->bDiskCheck[iDrive] = SUCCEEDED(SHPathPrepareForWrite(((pcs->fFlags & FOF_NOERRORUI) ? NULL : hwnd), NULL, szDrive, 0));
        }
        return pdth->bDiskCheck[iDrive];
    }

    return TRUE;    // always succeed for net drives
}


//--------------------------------------------------------------------------------------------
// Simple pidl cache stuff
//--------------------------------------------------------------------------------------------

void SimplePidlCache_Release(SIMPLEPIDLCACHE *pspc)
{
    ATOMICRELEASE(pspc->pbcFile);
    ATOMICRELEASE(pspc->pbcFolder);
    ATOMICRELEASE(pspc->psfDesktop);
    ATOMICRELEASE(pspc->psf);
    ILFree(pspc->pidlFolder);
}

const WIN32_FIND_DATA c_fdFolder = { FILE_ATTRIBUTE_DIRECTORY };

BOOL SimplePidlCache_Init(SIMPLEPIDLCACHE *pspc)
{
    ASSERT(pspc->iInit == 0);

    if (SUCCEEDED(SHCreateFileSysBindCtx(NULL, &pspc->pbcFile)) &&
            SUCCEEDED(SHCreateFileSysBindCtx(&c_fdFolder, &pspc->pbcFolder)) &&
            SUCCEEDED(SHGetDesktopFolder(&pspc->psfDesktop)))
    {
        pspc->psf = pspc->psfDesktop;
        pspc->psf->lpVtbl->AddRef(pspc->psf);
        // It's okay to leave pidlFolder as NULL; ILCombine won't barf

        pspc->iInit = 1;
        return TRUE;
    }
    else
    {
        pspc->iInit = -1;
        return FALSE;
    }
}

LPITEMIDLIST SimplePidlCache_GetFilePidl(SIMPLEPIDLCACHE *pspc, LPCTSTR pszFile)
{
    LPITEMIDLIST pidlChild;
    LPITEMIDLIST pidlRet;
    LPTSTR pszFileName;
    TCHAR szFolder[MAX_PATH];
    USES_CONVERSION;

    if (pspc->iInit < 0)
        return NULL;                // Initialization failed

    if (!pspc->iInit && !SimplePidlCache_Init(pspc))
        return NULL;

    // If this file is in a different folder from the one we cached,
    // need to dump the old one and get a new one.

    lstrcpyn(szFolder, pszFile, ARRAYSIZE(szFolder));
    PathRemoveFileSpec(szFolder);

    // We use StrCmpC instead of lstrcmpi because the vast majority
    // of the time, the path will match even in case, and if we get
    // it wrong, it's no big whoop: we just don't use the cache.

    if (StrCmpC(pspc->szFolder, szFolder) != 0)
    {
        LPITEMIDLIST pidlFolder = NULL; // In case it's on the desktop
        IShellFolder *psf;

        if (szFolder[0])            // An actual folder
        {
            // Get a simple pidl to the folder.
            if (FAILED(pspc->psfDesktop->lpVtbl->ParseDisplayName(pspc->psfDesktop, NULL,
                            pspc->pbcFolder, T2W(szFolder), NULL, &pidlFolder, NULL)))
                return NULL;
        }
        else                        // Going for the desktop
        {
            /* pidlFolder already preinitialized to NULL */
        }

        // Bind to that folder
        if (FAILED(SHBindToObject(pspc->psfDesktop, IID_X_PPV_ARG(IShellFolder, pidlFolder, &psf))))
        {
            ILFree(pidlFolder);
            return NULL;
        }

        // Woo-hoo, everybody is happy.  Save the results into our cache.

        ATOMICRELEASE(pspc->psf);
        pspc->psf = psf;

        ILFree(pspc->pidlFolder);
        pspc->pidlFolder = pidlFolder;

        lstrcpyn(pspc->szFolder, szFolder, ARRAYSIZE(pspc->szFolder));

    }

    // Get a simple pidl to the filename
    pszFileName = PathFindFileName(pszFile);        // T2W is a macro with multiple evaluation
    if (FAILED(pspc->psf->lpVtbl->ParseDisplayName(pspc->psf, NULL, pspc->pbcFile,
                    T2W(pszFileName), NULL, &pidlChild, NULL)))
        return NULL;

    // Combine it with the parent
    pidlRet = ILCombine(pspc->pidlFolder, pidlChild);
    ILFree(pidlChild);

    return pidlRet;
}

//--------------------------------------------------------------------------------------------
// ConvertToConnectedItemname:
//      Given a file/folder name, this function checks to see if it has any connection and if 
// there is a connection, then it will convert the given name to that of the connected element
// and return length of the prefix. If no connection exists, it returns zero.
//  The fDirectory parameter specifies if the given filename is a FOLDER or not!
//
//  dwBuffSize: The size of pszFileName buffer in CHARACTERS.
//
//  Examples:
//      "foo.htm"   =>  "foo*"  (returns 3 because the prefix("foo") length is 3)
//      "foobar files" =>  "foobar.htm?"  (returns 6 as the prefix length)
//                                          
//--------------------------------------------------------------------------------------------
int ConvertToConnectedItemName(LPTSTR pszFileName, DWORD dwBuffSize, BOOL fDirectory)
{
    LPTSTR  pszDest, pszConnectedElemSuffix;
    int     iPrefixLength;

    if (fDirectory)
    {
        // Look for a suffix which is one of the standard suffixes.
        if (!(pszDest = (LPTSTR)PathFindSuffixArray(pszFileName, c_apszSuffixes, ARRAYSIZE(c_apszSuffixes))))
            return FALSE;

        // " files" suffix is found. Replace it with ".htm?"
        pszConnectedElemSuffix = TEXT(".htm?");
    }
    else
    {
        // Look for the extension ".htm" or ".html" and replace it with "*".
        if (!(pszDest = PathFindExtension(pszFileName)))
            return FALSE;

        if (lstrcmpi(pszDest, TEXT(".htm")) && (lstrcmpi(pszDest, TEXT(".html"))))
            return FALSE;

        // Extension ".htm" or ".html" is found. Replace it with "*"
        pszConnectedElemSuffix = (LPTSTR)c_szStar;
    }

    iPrefixLength = (int)(pszDest - pszFileName);

    //Check if the input buffer is big enough to over-write the suffix in-place
    if ((((int)dwBuffSize - iPrefixLength) - 1) < lstrlen(pszConnectedElemSuffix))
        return 0;

    //Replace the source suffix with the connected element's suffix.
    lstrcpy(pszDest, pszConnectedElemSuffix);

    return(iPrefixLength);
}

PDIRTREENODE DTAllocNode(PDIRTREEHEADER pdth, WIN32_FIND_DATA* pfd, PDIRTREENODE pdtnParent, PDIRTREENODE pdtnNext, BOOL fConnectedElement)
{
    int iLen = pfd ? lstrlen(pfd->cFileName) * sizeof(TCHAR) : 0;
    PDIRTREENODE pdtn = (PDIRTREENODE)LocalAlloc(LPTR, sizeof(DIRTREENODE) + iLen);
    if (pdtn)
    {
        pdtn->fConnectedElement = fConnectedElement;

        // Initializing the following to NULL is not needed because of the LPTR (zero init) done
        // above.
        // if (fConnectedElement)
        //{
        //    pdtn->ConnectedInfo.pFromConnected = pdtn->ConnectedInfo.pToConnected = NULL;
        //    pdtn->ConnectedInfo.dwConfirmation = 0;
        //}
        //else
        //    pdtn->pdtnConnected = NULL;

        pdtn->pdtnParent = pdtnParent;
        pdtn->pdtnNext   = pdtnNext;

        if (pfd)
        {
            pdtn->dwFileAttributes = pfd->dwFileAttributes;
            pdtn->ftCreationTime   = pfd->ftCreationTime;
            pdtn->ftLastWriteTime  = pfd->ftLastWriteTime;
            pdtn->nFileSizeLow     = pfd->nFileSizeLow;
            pdtn->nFileSizeHigh    = pfd->nFileSizeHigh;

            // only the stuff we care about
            lstrcpy(pdtn->szShortName, pfd->cAlternateFileName);
            lstrcpy(pdtn->szName, pfd->cFileName);


            if (ISDIRFINDDATA(*pfd))
            {
                pdth->dtAll.dwFolders++;
                pdtn->pdtnChild = DTN_DELAYED;
            }
            else
            {
                LARGE_INTEGER li;

                li.LowPart = pfd->nFileSizeLow;
                li.HighPart = pfd->nFileSizeHigh;

                pdth->dtAll.liSize.QuadPart += li.QuadPart;
                pdth->dtAll.dwFiles++;
            }
            // increment the header stats
            pdth->dtAll.fChanged = TRUE;
        }
    }

    return pdtn;
}

#if defined(DEBUG)  /// && defined(DEBUGCOPY)
void DebugDumpPDTN(PDIRTREENODE pdtn, LPTSTR ptext)
{
    DebugMsg(TF_DEBUGCOPY, TEXT("***** PDTN %x  (%s)"), pdtn, ptext);
    //Safe-guard against pdtn being NULL!
    if (pdtn)
    {
        DebugMsg(TF_DEBUGCOPY, TEXT("** %s %s"), pdtn->szShortName, pdtn->szName);
        DebugMsg(TF_DEBUGCOPY, TEXT("** %x %d"), pdtn->dwFileAttributes, pdtn->nFileSizeLow);
        DebugMsg(TF_DEBUGCOPY, TEXT("** %x %x %x"), pdtn->pdtnParent, pdtn->pdtnNext, pdtn->pdtnChild);
        DebugMsg(TF_DEBUGCOPY, TEXT("** NewRoot:%x, Connected:%x, Dummy:%x"), pdtn->fNewRoot, pdtn->fConnectedElement, pdtn->fDummy);
        if (pdtn->fConnectedElement)
        {
            DebugMsg(TF_DEBUGCOPY, TEXT("**** Connected: pFromConnected:%s, pToConnected:%s, dwConfirmation:%x"), pdtn->ConnectedInfo.pFromConnected, 
                    pdtn->ConnectedInfo.pToConnected, pdtn->ConnectedInfo.dwConfirmation);
        }
        else
        {
            DebugMsg(TF_DEBUGCOPY, TEXT("**** Origin: pdtnConnected:%x"), pdtn->pdtnConnected);
        }
    }
    else
    {
        DebugMsg(TF_DEBUGCOPY, TEXT("** NULL pointer(PDTN)"));
    }
}
#else
#define DebugDumpPDTN(p, x) 0
#endif

BOOL  DoesSuffixMatch(LPTSTR  lpSuffix, const LPCTSTR *apSuffixes, int iArraySize)
{
    while (iArraySize--)
    {
        // Note: This must be a case sensitive compare, because we don't want to pickup 
        // "Program Files".
        if (!lstrcmp(lpSuffix, *apSuffixes++))
            return TRUE;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------------
//
//  DTPathToDTNode:
//      This function is used to build a list of nodes that correspond to the given pszPath.
// This list is built under "ppdtn".  If ppdtnConnectedItems is given, another list of nodes that
// correspond to the connected elements(files/folders) of the nodes in the first list is also built
// under "ppdtnConnectedItems".
//
// WARNING: This parties directly on pszPath and pfd so that it doesn't need to allocate
// on the stack.  This recurses, so we want to use as little stack as possible
//
// this will wack off one component from pszPath
//
//
// ppdtn: Points to where the header of the list being built will be stored.
// ppdtnConnectedItems: If this is NULL, then we are not interested in finding and building the 
//                      connected elements. If this is NOT null, it points to where the header of
//                      the connected items list will be stored.
// fConnectedElement: Each node being built under ppdtn needs to be marked with this bit.
// iPrefixLength: This parameter is zero if fConnectedElement is FALSE. Otherwise, it contains the
//              Length of the prefix part of the file or foldername (path is NOT included).
//              For example, if "c:\windows\foo*" is passed in, iPrefixLength is 3 (length of "foo")
//
// dwFilesOrFolders parameter can specify if we need to look for only FILES or FOLDERs or BOTH.

#define     DTF_FILES_ONLY      0x00000001      //Operate only on Files.
#define     DTF_FOLDERS_ONLY    0x00000002      //Operate only on Folders.
#define     DTF_FILES_AND_FOLDERS  (DTF_FILES_ONLY | DTF_FOLDERS_ONLY)  //Operate on files AND folders.

UINT DTPathToDTNode(PDIRTREEHEADER pdth, COPY_STATE *pcs, LPTSTR pszPath, BOOL fRecurse,
        DWORD dwFilesOrFolders, PDIRTREENODE* ppdtn, WIN32_FIND_DATA *pfd,
        PDIRTREENODE pdtnParent, PDIRTREENODE* ppdtnConnectedItems, BOOL fConnectedElement,
        int iPrefixLength)
{
    int iError = 0;

    // this points to the var where all items are inserted.
    // folders are placed after it, files are placed before

    // keep the stack vars to a minimum because this is recursive
    PDIRTREENODE *ppdtnMiddle = ppdtn;
    BOOL fNeedToFindNext = TRUE;
    HANDLE hfind = FindFirstFile(pszPath, pfd);

    DebugMsg(TF_DEBUGCOPY, TEXT("DTPathToDTNode Entering %s"), pszPath);
    *ppdtnMiddle = NULL; // in case there are no children

    if (hfind == INVALID_HANDLE_VALUE)
    {
        // this is allowable only if the path is wild...
        // and the parent exists
        if (PathIsWild(pszPath))
        {
            PathRemoveFileSpec(pszPath);
            if (PathFileExists(pszPath))
            {
                return 0;
            }
        }
        return OPER_ERROR | ERROR_FILE_NOT_FOUND;
    }

    //Remove the filespec before passing it onto DTAllocConnectedItemNodes.
    PathRemoveFileSpec(pszPath);

    do
    {
        // We skip the following files:
        //      "." and ".." filenames
        //      Folders when DTF_FILES_ONLY is set
        //      Files when DTF_FOLDERS_ONLY is set

        if (!PathIsDotOrDotDot(pfd->cFileName) &&
                (((dwFilesOrFolders & DTF_FILES_ONLY) && !ISDIRFINDDATA(*pfd)) || 
                 ((dwFilesOrFolders & DTF_FOLDERS_ONLY) && ISDIRFINDDATA(*pfd)))) 
        {
            //Check if we are looking for connected elements
            if ((!pdtnParent) && fConnectedElement)
            {
                // We found what we are looking for. If we are looking for a top-level connected item and 
                // if it is a folder, then we need to make sure that the suffix exactly matches one of the
                // suffixes in the array c_apszSuffixes[].
                LPTSTR  lpSuffix = (LPTSTR)(pfd->cFileName + iPrefixLength);

                if (ISDIRFINDDATA(*pfd))  
                {
                    // What we found is a directory!
                    // See if it has one of the standard suffixes for connected folders.
                    if (!DoesSuffixMatch(lpSuffix, c_apszSuffixes, ARRAYSIZE(c_apszSuffixes)))
                        continue; //This is not what we look for. So, find next.
                }
                else
                {
                    // What we found is a file (i.e Not a directory)
                    // See if it has one of the standard suffixes for html files.
                    if (lstrcmpi(lpSuffix, TEXT(".htm")) && lstrcmpi(lpSuffix, TEXT(".html")))
                        continue; //This is not what we look for. So, find next.
                }

                // Now we know that we found the connected element that we looked for.
                // So, no need to FindNext again. We can get out of the loop after processing
                // it once.
                fNeedToFindNext = FALSE;
            }

            *ppdtnMiddle = DTAllocNode(pdth, pfd, pdtnParent, *ppdtnMiddle, fConnectedElement);

            if (!*ppdtnMiddle)
            {
                FindClose(hfind);
                return OPER_ERROR | ERROR_NOT_ENOUGH_MEMORY;
            }

            // make sure that the parent's pointer always points to the head of
            // this linked list
            if (*ppdtn == (*ppdtnMiddle)->pdtnNext)
                *ppdtn = (*ppdtnMiddle);

            DebugDumpPDTN(*ppdtnMiddle, TEXT("DTPathToDTNode, DTAllocNode"));

            //We need to check for Connected elements only for the top level items
            if ((!(pcs->fFlags & FOF_NO_CONNECTED_ELEMENTS)) && ppdtnConnectedItems)
            {
                //Make sure this is a top level item
                ASSERT(!pdtnParent);

                //Create a list of connected items and attach it to the head of the list.
                iError = DTAllocConnectedItemNodes(pdth, pcs, pfd, pszPath, fRecurse, ppdtnConnectedItems);

                DebugDumpPDTN(*ppdtnConnectedItems, TEXT("DTPathToDTNode, DTAllocConnectedNodes"));

                // It is possible that the connected files do not exist. That condition is not really
                // an error. So, we check for insufficient memory error condition alone here.
                if (iError == (OPER_ERROR | ERROR_NOT_ENOUGH_MEMORY))
                {
                    FindClose(hfind);
                    return(iError);
                }

                //If a connected item exists, then make the origin item point to this connected item.
                if (*ppdtnConnectedItems)
                {
                    (*ppdtnMiddle)->pdtnConnected = *ppdtnConnectedItems;
                    // Also by default, set the Confirmation result to NO so that the connected element
                    // will not be copied/moved etc., in case of a conflict. However, if the origin had
                    // a conflict, we would put up a confirmation dlg and the result of that dlg will 
                    // over-write this value.
                    (*ppdtnConnectedItems)->ConnectedInfo.dwConfirmation = IDNO;
                }

                //Move to the last node in the connected items list.
                while (*ppdtnConnectedItems)
                    ppdtnConnectedItems = &((*ppdtnConnectedItems)->pdtnNext);
            }
            else
            {
                // This should have been initialized to zero during allocation, but lets be paranoid
                ASSERT(NULL == (*ppdtnMiddle)->pdtnConnected);
            }

            // if this is not a directory, move the ppdtnMiddle up one
            if (!ISDIRFINDDATA(*pfd))
            {
                ppdtnMiddle = &(*ppdtnMiddle)->pdtnNext;
            }

        }

    } while (fNeedToFindNext && !FOQueryAbort(pcs) && FindNextFile(hfind, pfd));

    iError = 0;  //It is possible that iError contains other errors value now! So, reset it!

    FindClose(hfind);

    // now go and recurse into folders (if desired)
    // we don't have to check to see if these pdtn's are dirs, because the
    // way we inserted them above ensures that everything in from of
    // ppdtnMiddle are folders

    // we're going to tack on a specific child
    // then add the *.* after that

    while (!FOQueryAbort(pcs) && *ppdtnMiddle)
    {
        BOOL fRecurseThisItem = fRecurse;

        if ((*ppdtnMiddle)->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            // Recurse into reparse points unless they asked not to
            if (pcs->fFlags & FOF_NORECURSEREPARSE)
            {
                fRecurseThisItem = FALSE;
            }
        }

        if (fRecurseThisItem)
        {
            if (PathAppend(pszPath, (*ppdtnMiddle)->szName))
            {
                if (PathAppend(pszPath, c_szStarDotStar))
                {

                    // NULL indicates that we do not want to get the connected elements.
                    // This is because we want the connected elements only for the top-level items.
                    iError = DTPathToDTNode(pdth, pcs, pszPath, TRUE, DTF_FILES_AND_FOLDERS,
                            &((*ppdtnMiddle)->pdtnChild), pfd, *ppdtnMiddle, NULL, fConnectedElement, 0);

                }
                else
                {
                    iError = OPER_ERROR | DE_INVALIDFILES;
                }

                PathRemoveFileSpec(pszPath);
            }
            else
            {
                iError = OPER_ERROR | DE_INVALIDFILES;
            }
        }
        else
        {
            // if we don't want to recurse, just mark them all as having no children
            (*ppdtnMiddle)->pdtnChild = NULL;
        }

        if (iError)
        {
            return iError;
        }

        ppdtnMiddle = &(*ppdtnMiddle)->pdtnNext;
    }

    return 0;
}

UINT DTAllocConnectedItemNodes(PDIRTREEHEADER pdth, COPY_STATE *pcs, WIN32_FIND_DATA *pfd, LPTSTR pszPath, BOOL fRecurse, PDIRTREENODE *ppdtnConnectedItems)
{
    // Since DTAllocConnectedItemNodes() gets called only for the top-level items in the src list,
    // there is no danger of this function getting called recursively. Hence, I didn't worry about
    // allocating the following on the stack.
    // If "too-much-stack-is-used" problem arises, we can optimize the stack usage by splitting
    // the following function into two such that the most common case (of no connection) 
    // doesn't use much stack. 
    DWORD   dwFileOrFolder;
    TCHAR   szFullPath[MAX_PATH];
    TCHAR   szFileName[MAX_PATH];
    WIN32_FIND_DATA  fd;
    int     iPrefixLength;  //This is the length of "foo" if the filename is "foo.htm" or "foo files"

    //Make a copy of the filename; This copy will get munged by ConvertToConnectedItemName().
    lstrcpy(szFileName, pfd->cFileName);
    // Convert the given file/foder name into the connected item's name with wild card characters.
    if (!(iPrefixLength = ConvertToConnectedItemName(szFileName, ARRAYSIZE(szFileName), ISDIRFINDDATA(*pfd))))
        return 0; //No connections exist for the given folder/file.

    // Now szFileName has the name of connected element with wildcard character.

    // If the given element is a directory, we want to look for connected FILES only  and
    // if the given element is a file, we want to look for connected FOLDERS only.
    dwFileOrFolder = ISDIRFINDDATA(*pfd) ? DTF_FILES_ONLY : DTF_FOLDERS_ONLY;

    // Form the file/folder name with the complete path!
    lstrcpy(szFullPath, pszPath);
    PathAppend(szFullPath, szFileName); 

    // The file-element has some "connected" items.
    DebugMsg(TF_DEBUGCOPY, TEXT("DTAllocConnectedItemNodes Looking for %s"), szFullPath);

    return(DTPathToDTNode(pdth, pcs, szFullPath, fRecurse, dwFileOrFolder, ppdtnConnectedItems, &fd, NULL, NULL, TRUE, iPrefixLength));
}

void DTInitProgressPoints(PDIRTREEHEADER pdth, COPY_STATE *pcs)
{
    pdth->iFilePoints = 1;
    pdth->iFolderPoints = 1;

    switch (pcs->lpfo->wFunc)
    {
        case FO_RENAME:
        case FO_DELETE:
            pdth->iSizePoints = 0;
            break;

        case FO_COPY:
            pdth->iSizePoints = 1;
            break;

        case FO_MOVE:
            if (PathIsSameRoot(pcs->lpfo->pFrom, pcs->lpfo->pTo))
            {
                pdth->iSizePoints = 0;
            }
            else
            {
                // if it's across volumes, these points increase
                // because we need to nuke the source as well as
                // create the target...
                // whereas we don't need to nuke the "size" of the source
                pdth->iFilePoints = 2;
                pdth->iFolderPoints = 2;
                pdth->iSizePoints = 1;
            }
            break;
    }
}

UINT DTBuild(COPY_STATE* pcs)
{
    PDIRTREEHEADER pdth = &pcs->dth;
    WIN32_FIND_DATA fd;
    TCHAR szPath[MAX_PATH];
    PDIRTREENODE *ppdtn;
    PDIRTREENODE *ppdtnConnectedItems;
    int iError = 0;

    pcs->dth.pFrom = (LPTSTR)pcs->lpfo->pFrom;
    pcs->dth.pTo = (LPTSTR)pcs->lpfo->pTo;
    // A tree of original items will be built under ppdtn.
    ppdtn = &pdth->pdtn;
    // A tree of items "connected" to the orginal items will be built under ppdtnConnectedItems.
    ppdtnConnectedItems = &pdth->pdtnConnectedItems;

    DTInitProgressPoints(pdth, pcs);
    while (!FOQueryAbort(pcs) && *pdth->pFrom)
    {
        BOOL fRecurse = TRUE;

        switch (pcs->lpfo->wFunc)
        {
            case FO_MOVE:
                // The move operation doesn't need to recurse if we are moving from and to the same
                // volume.  In this case we know that we don't need to display any warnings for
                // things like LFN to 8.3 filename conversion or stream loss.  Instead, we can do
                // the operation with one single win32 file operation that just does a rename.

                // NTRAID89119-2000/02/25-toddb
                // This is only true if we don't cross a mount point!  If we cross
                // a mount point then we might have to warn about these things.

                if ((pcs->fFlags & FOF_NORECURSION) || PathIsSameRoot(pdth->pFrom, pdth->pTo))
                {
                    fRecurse = FALSE;
                }
                break;

            case FO_COPY:
                // For a copy we always recurse unless we're told not to.
                if (pcs->fFlags & FOF_NORECURSION)
                {
                    fRecurse = FALSE;
                }
                break;

            case FO_RENAME:
                // for a rename we never recurse
                fRecurse = FALSE;
                break;

            case FO_DELETE:
                // for a delete we don't need to recurse IF the recycle bin will be able to handle
                // the given item.  If the recycle bin handles the delete then we can undo from
                // the recycle bin if we need to.
                if ((pcs->fFlags & FOF_ALLOWUNDO) && BBWillRecycle(pdth->pFrom, NULL))
                {
                    fRecurse = FALSE;
                }
                break;
        }

        lstrcpy(szPath, pdth->pFrom);

        DebugMsg(TF_DEBUGCOPY, TEXT("DTBuild: %s"), szPath);

        // If the file is on removable media, we need to check for media in the drive.
        // Prompt the user to insert the media if it's missing.
        if (!DTDiskCheck(pdth, pcs, szPath))
        {
            iError = ERROR_CANCELLED;
            break;
        }

        iError = DTPathToDTNode(pdth, pcs, szPath, fRecurse,
                ((PathIsWild(pdth->pFrom) && (pcs->lpfo->fFlags & FOF_FILESONLY)) ? DTF_FILES_ONLY : DTF_FILES_AND_FOLDERS), 
                ppdtn,&fd, NULL, ppdtnConnectedItems, FALSE, 0);

        DebugMsg(TF_DEBUGCOPY, TEXT("DTBuild: returned %d"), iError);

        // FEATURE: If an error occured we should allow the user to skip the file that caused the error.  That way
        // if one of the source files doesn't exists the rest will still get copied.  Do this only in the multi-
        // source case, blah blah blah.  This helps in the case where one of the source files cannot be moved or
        // copied (usually due to Access Denied, could be insuffecent permissions or file is in use, etc).

        if (iError)
            break;

        if (!(*ppdtn) && PathIsWild(pdth->pFrom))
        {
            // no files are associated with this path... this
            // can happen when we have wildcards...
            // alloc a dummy node
            *ppdtn = DTAllocNode(pdth, NULL, NULL, NULL, FALSE);
            if (*ppdtn)
            {
                (*ppdtn)->fDummy = TRUE;
            }
        }


        if (*ppdtn)
        {
            // mark this as the start of a root spec... this is
            // necessary in case we have several wild specs
            (*ppdtn)->fNewRoot = TRUE;
        }

        if (*ppdtnConnectedItems)
        {
            // Mark this as the start of a root spec.
            (*ppdtnConnectedItems)->fNewRoot = TRUE;
            // For connected items, we need to remember the path.
            (*ppdtnConnectedItems)->ConnectedInfo.pFromConnected = pdth->pFrom;
            (*ppdtnConnectedItems)->ConnectedInfo.pToConnected = pdth->pTo;
        }


        while (*ppdtn)
        {
            ppdtn = &(*ppdtn)->pdtnNext;
        }

        while (*ppdtnConnectedItems)
        {
            ppdtnConnectedItems = &(*ppdtnConnectedItems)->pdtnNext;
        }

        pdth->pFrom += lstrlen(pdth->pFrom) + 1;
        if (pcs->lpfo->wFunc != FO_DELETE && (pcs->lpfo->fFlags & FOF_MULTIDESTFILES))
        {
            pdth->pTo += lstrlen(pdth->pTo) + 1;
        }
    }

    //Attach the "ConnectedElements" Tree to the end of the source element tree.
    *ppdtn = pcs->dth.pdtnConnectedItems;

    pcs->dth.pFrom = (LPTSTR)pcs->lpfo->pFrom;
    pcs->dth.pTo = (LPTSTR)pcs->lpfo->pTo;
    pcs->fDTBuilt = TRUE;

    // set up the initial time information
    pcs->dwPreviousTime = GetTickCount();
    pcs->dwPointsPerSec = 0;
    pcs->iLastProgressPoints = 0;
    return iError;
}

#define DTNIsRootNode(pdtn) ((pdtn)->pdtnParent == NULL)
#define DTNIsDirectory(pdtn) (pdtn->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
// This macro determines if the given node is an "Origin" of a connection. i.e. Does this node 
// point to a connected element that needs to be moved/copied etc., along with it?
// For example, if "foo.htm" is moved, "foo files" is also moved. 
// Here, "foo.htm" is the "Connect origin" (fConnectedElement = FALSE; pdtnConnected is valid)
// and "foo files" is the "connected element". (fConnectedElement = TRUE;)
#define DTNIsConnectOrigin(pdtn) ((!pdtn->fConnectedElement) && (pdtn->pdtnConnected != NULL))
#define DTNIsConnected(pdtn)    (pdtn && (pdtn->fConnectedElement))

//
UINT DTEnumChildren(PDIRTREEHEADER pdth, COPY_STATE *pcs, BOOL fRecurse, DWORD dwFileOrFolder)
{
    int iError = 0;
    if (pdth->pdtnCurrent->pdtnChild == DTN_DELAYED)
    {
        WIN32_FIND_DATA fd;

        // fill in all the children and update the stats in pdth
        if (PathAppend(pdth->szSrcPath, c_szStarDotStar))
        {
            iError = DTPathToDTNode(pdth, pcs, pdth->szSrcPath, fRecurse, dwFileOrFolder,
                    &pdth->pdtnCurrent->pdtnChild, &fd, pdth->pdtnCurrent, NULL, pdth->pdtnCurrent->fConnectedElement, 0);
        }
        else
        {
            iError = OPER_ERROR | DE_INVALIDFILES;
        }

        // If we get "File Not Found" Error now and if it is a connected item, then this item 
        // must have already been moved/renamed/deleted etc., So, this is not really an error. 
        // All this means is that this connected item was also explicitly selected and hence appeared
        // as or "Origin" item earlier in the list and it had already been operated upon.
        // So, reset the error here.
        // (Example: If end-user selects "foo.htm" AND "foo files" folder and moves them, then we
        // will get a file-not-found error when we attempt to move the connected items. To avoid
        // this error dialog, we reset the error here.)

        if (DTNIsConnected(pdth->pdtnCurrent) && (iError == (OPER_ERROR | ERROR_FILE_NOT_FOUND)))
            iError = 0;  
    }
    return iError;
}

//
// DTNGetConfirmationResult:
//    When a file("foo.htm") is moved/copied, we may put up a confirmation dialog in case 
// of a conflict and the end-user might have responded saying "Yes", "no" etc., When the 
// corresponding connected element ("foo files") is also moved/copied etc., we should NOT put up
// a confirmation dialog again. We must simply store the answer to the original confirmation and
// use it later. 
//  This function retries the result of the original confirmation from the top-level connected 
// element.
int  DTNGetConfirmationResult(PDIRTREENODE pdtn)
{
    //Confirmation results are saved only for Connected items; Not for Connection Origins.
    if (!pdtn || !DTNIsConnected(pdtn))
        return 0;

    //Confirmation results are stored only at the top-level node. So, go there.
    while (pdtn->pdtnParent)
        pdtn = pdtn->pdtnParent;

    return(pdtn->ConnectedInfo.dwConfirmation);
}

void DTGetWin32FindData(PDIRTREENODE pdtn, WIN32_FIND_DATA* pfd)
{
    // only the stuff we care about
    lstrcpy(pfd->cAlternateFileName, pdtn->szShortName);
    lstrcpy(pfd->cFileName, pdtn->szName);

    pfd->dwFileAttributes = pdtn->dwFileAttributes;
    pfd->ftCreationTime   = pdtn->ftCreationTime;
    pfd->ftLastWriteTime  = pdtn->ftLastWriteTime;
    pfd->nFileSizeLow     = pdtn->nFileSizeLow;
    pfd->nFileSizeHigh     = pdtn->nFileSizeHigh;
}

void DTSetFileCopyProgress(PDIRTREEHEADER pdth, LARGE_INTEGER liRead)
{
    LARGE_INTEGER liDelta;

    liDelta.QuadPart = (liRead.QuadPart - pdth->pdtnCurrent->liFileSizeCopied.QuadPart);

    DebugMsg(TF_DEBUGCOPY, TEXT("DTSetFileCopyProgress %d %d %d"), liDelta.LowPart, liRead.LowPart, pdth->dtDone.liSize.QuadPart);
    pdth->pdtnCurrent->liFileSizeCopied.QuadPart += liDelta.QuadPart;
    pdth->dtDone.liSize.QuadPart += liDelta.QuadPart;
    DebugMsg(TF_DEBUGCOPY, TEXT("DTSetFileCopyProgress %d %d"), liDelta.LowPart, pdth->dtDone.liSize.LowPart);
    pdth->dtDone.fChanged = TRUE;
}

void DTFreeNode(PDIRTREEHEADER pdth, PDIRTREENODE pdtn)
{
    if (pdth)
    {
        ASSERT(pdtn->pdtnChild == NULL || pdtn->pdtnChild == DTN_DELAYED);

        // we're done with this node..  update the header totals
        if (DTNIsDirectory(pdtn))
        {
            pdth->dtDone.dwFolders++;
        }
        else
        {
            LARGE_INTEGER li;

            li.LowPart = pdtn->nFileSizeLow;
            li.HighPart = pdtn->nFileSizeHigh;

            pdth->dtDone.dwFiles++;
            pdth->dtDone.liSize.QuadPart += (li.QuadPart - pdtn->liFileSizeCopied.QuadPart);
        }

        pdth->dtDone.fChanged = TRUE;

        // repoint parent pointer
        if (!pdtn->pdtnParent)
        {

            // no parent... must be a root type thing
            ASSERT(pdth->pdtn == pdtn);
            pdth->pdtn = pdtn->pdtnNext;

        }
        else
        {

            ASSERT(pdtn->pdtnParent->pdtnChild == pdtn);
            if (pdtn->pdtnParent->pdtnChild == pdtn)
            {
                // if my parent was pointing to me, point him to my sib
                pdtn->pdtnParent->pdtnChild = pdtn->pdtnNext;
            }
        }
    }

    LocalFree(pdtn);
}

// this frees all children of (but NOT including) the current node.
// it doesn' free the current node because it's assumed that
// DTGoToNextNode will be called right afterwards, and that will
// free the current node
void DTFreeChildrenNodes(PDIRTREEHEADER pdth, PDIRTREENODE pdtn)
{
    PDIRTREENODE pdtnChild = pdtn->pdtnChild;
    while (pdtnChild && pdtnChild != DTN_DELAYED)
    {
        PDIRTREENODE pdtnNext = pdtnChild->pdtnNext;

        // recurse and free these children
        if (DTNIsDirectory(pdtnChild))
        {
            DTFreeChildrenNodes(pdth, pdtnChild);
        }

        DTFreeNode(pdth, pdtnChild);
        pdtnChild = pdtnNext;
    }

    pdtn->pdtnChild = NULL;
}

void DTForceEnumChildren(PDIRTREEHEADER pdth)
{
    if (!pdth->pdtnCurrent->pdtnChild)
        pdth->pdtnCurrent->pdtnChild = DTN_DELAYED;
}

void DTAbortCurrentNode(PDIRTREEHEADER pdth)
{
    DTFreeChildrenNodes((pdth), (pdth)->pdtnCurrent);
    if (pdth->oper == OPER_ENTERDIR)
        pdth->oper = OPER_LEAVEDIR;
}

void DTCleanup(PDIRTREEHEADER pdth)
{
    PDIRTREENODE pdtn;

    while (pdth->pdtnCurrent && pdth->pdtnCurrent->pdtnParent)
    {
        // in case we bailed deep in a tree
        pdth->pdtnCurrent = pdth->pdtnCurrent->pdtnParent;
    }

    while (pdth->pdtnCurrent)
    {
        pdtn = pdth->pdtnCurrent;
        pdth->pdtnCurrent = pdtn->pdtnNext;
        DTFreeChildrenNodes(NULL, pdtn);
        DTFreeNode(NULL, pdtn);
    }
}

BOOL DTInitializePaths(PDIRTREEHEADER pdth, COPY_STATE *pcs)
{
    ASSERT(pdth->pdtnCurrent);    // If we have no current node then how can we Initialize its paths?

    lstrcpyn(pdth->szSrcPath, pdth->pFrom, ARRAYSIZE(pdth->szSrcPath));

    // For the "Origins" we need to do this only if a wild card exists. However, for connected elements,
    // we need to do this everytime because connected elements may not exist for every "Origins"
    if (PathIsWild(pdth->pFrom) || (pdth->pdtnCurrent->fNewRoot && DTNIsConnected(pdth->pdtnCurrent)))
    {
        PathRemoveFileSpec(pdth->szSrcPath);
        if (!PathAppend(pdth->szSrcPath, pdth->pdtnCurrent->szName))
            return FALSE;
    }

    if (!pdth->pTo)
    {
        // no dest, make it the same as the source and we're done
        lstrcpyn(pdth->szDestPath, pdth->szSrcPath, ARRAYSIZE(pdth->szSrcPath));
        return TRUE;
    }

    if (pdth->pTo)
    {
        lstrcpyn(pdth->szDestPath, pdth->pTo, ARRAYSIZE(pdth->szSrcPath));
    }

    if (!pdth->fMultiDest)
    {
        if (!PathAppend(pdth->szDestPath, pdth->pdtnCurrent->szName))
            return FALSE;
    }
    else
    {
        //When undo of a move operation is done, fMultiDest is set.
        // When fMultiDest is set, we need to strip out the filename given by pTo and 
        // append the current filename.
        // For RENAME operations, the source and destination names are different. This is handled 
        // seperately below. So, we handle only other operations here where source and dest names are the same.
        if ((pcs->lpfo->wFunc != FO_RENAME) && pdth->pdtnCurrent->fNewRoot && DTNIsConnected(pdth->pdtnCurrent))
        {
            PathRemoveFileSpec(pdth->szDestPath);
            if (!PathAppend(pdth->szDestPath, pdth->pdtnCurrent->szName))
                return FALSE;
        }
    }

    //We will never try to rename a connected element! Make sure we don't hit this!
    ASSERT(!((pcs->lpfo->wFunc == FO_RENAME) && DTNIsConnected(pdth->pdtnCurrent)));

    return TRUE;

}

UINT DTValidatePathNames(PDIRTREEHEADER pdth, UINT operation, COPY_STATE * pcs)
{
    if (pcs->lpfo->wFunc != FO_DELETE)
    {
        // Why process name mappings?  Here's why.  If we are asked to copy directory "c:\foo" and
        // file "c:\foo\file" to another directory (say "d:\") we might have a name confilct when
        // we copy "c:\foo" so instead we create "d:\Copy Of foo".  Later, we walk to the second
        // dirtree node and we are asked to copy "c:\foo\file" to "d:\foo", all of which is valid.
        // HOWEVER, it's not what we want to do.  We use _ProccessNameMappings to convert
        // "d:\foo\file" into "d:\Copy of foo\file".
        _ProcessNameMappings(pdth->szDestPath, pdth->hdsaRenamePairs);

        // REVIEW, do we need to do the name mapping here or just let the
        // VFAT do it?  if vfat does it we need to rip out all of the GetNameDialog() stuff.

        if ((operation != OPER_LEAVEDIR) &&
                !IsLFNDrive(pdth->szDestPath) &&
                PathIsLFNFileSpec(PathFindFileName(pdth->szSrcPath)) &&
                PathIsLFNFileSpec(PathFindFileName(pdth->szDestPath)))
        {

            int iRet;
            TCHAR szOldDest[MAX_PATH];

            lstrcpyn(szOldDest, pdth->szDestPath, ARRAYSIZE(szOldDest));
            iRet = GetNameDialog(pcs->hwndDlgParent, pcs,
                    (pcs->nSourceFiles != 1) || !DTNIsRootNode(pdth->pdtnCurrent), // if we're entering a dir, multiple spec, or not at root
                    operation, pdth->szSrcPath, pdth->szDestPath);

            switch (iRet)
            {
                case IDNO:
                case IDCANCEL:
                    return iRet;

                default:
                    AddRenamePairToHDSA(szOldDest, pdth->szDestPath, &pcs->dth.hdsaRenamePairs);
                    break;
            }
        }

        if (operation == OPER_ENTERDIR)
        {
            // Make sure the new directory is not a subdir of the original...

            int cchFrom = lstrlen(pdth->szSrcPath);

            // NTRAID89511-2000/02/25-KishoreP
            // Shouldn't we get the short names for both these directories and compair those?
            // Otherwise I can copy "C:\Long Directory Name" to "C:\LongDi~1\foo" without error.

            if (!(pcs->fFlags & FOF_RENAMEONCOLLISION) &&
                    !StrCmpNI(pdth->szSrcPath, pdth->szDestPath, cchFrom))
            {
                TCHAR chNext = pdth->szDestPath[cchFrom]; // Get the next char in the dest.

                if (!chNext)
                {
                    return OPER_ERROR | DE_DESTSAMETREE;
                }
                else if (chNext == TEXT('\\'))
                {
                    // The two fully qualified strings are equal up to the end
                    // of the source directory ==> the destination is a subdir.
                    // Must return an error.

                    // if, stripping the last file name and the backslash give the same length, they are the
                    // same file/folder
                    if ((PathFindFileName(pdth->szDestPath) - pdth->szDestPath - 1) ==
                            lstrlen(pdth->szSrcPath))
                    {
                        return OPER_ERROR | DE_DESTSAMETREE;
                    }
                    else
                    {
                        return OPER_ERROR | DE_DESTSUBTREE;
                    }
                }
            }
        }
    }
    return 0;
}

// this moves to the next node (child, sib, parent) and sets up the
// directory path info and oper state
UINT DTGoToNextNode(PDIRTREEHEADER pdth, COPY_STATE *pcs)
{
    UINT oper = OPER_ENTERDIR; // the default
    int iError;

    if (!pdth->pdtnCurrent)
    {
        pdth->pdtnCurrent = pdth->pdtn;

        if (pdth->pdtnCurrent)
        {
            if (pdth->pdtnCurrent->fDummy)
            {
                // if this is just a placeholder... go on to the next one
                return DTGoToNextNode(pdth, pcs);
            }

            if (!DTInitializePaths(pdth, pcs))
            {
                return OPER_ERROR | DE_INVALIDFILES;
            }
        }
        else
        {
            // Our tree is completely empty.

            // REVIEW: What do we do here?  If pdtnCurrent is still NULL then our list is completely empty.
            // Is that a bug or what?  My hunch is that we should return an error code here, most likely
            // OPER_ERROR | DE_INVALIDFILES.  If we do nothing here then we will fail silently.
            return OPER_ERROR | DE_INVALIDFILES;
        }
    }
    else
    {
        UINT iError;
        BOOL fFreeLastNode = TRUE;
        PDIRTREENODE pdtnLastCurrent = pdth->pdtnCurrent;
        TCHAR szTemp[MAX_PATH];

        if (iError = DTEnumChildren(pdth, pcs, FALSE, DTF_FILES_AND_FOLDERS))
            return iError;

        if (pdth->pdtnCurrent->pdtnChild)
        {

            fFreeLastNode = FALSE;
            pdth->pdtnCurrent = pdth->pdtnCurrent->pdtnChild;

            // if the source long name is too long, try the short name
            if (!PathCombine(szTemp, pdth->szSrcPath, pdth->pdtnCurrent->szName))
            {
                if (!PathCombine(szTemp, pdth->szSrcPath, pdth->pdtnCurrent->szShortName))
                {
                    return OPER_ERROR | DE_INVALIDFILES;
                }
            }
            StrCpyN(pdth->szSrcPath, szTemp, ARRAYSIZE(pdth->szSrcPath));

            // if the dest long name is too long, try the short name
            if (!PathCombine(szTemp, pdth->szDestPath, pdth->pdtnCurrent->szName))
            {
                if (!PathCombine(szTemp, pdth->szDestPath, pdth->pdtnCurrent->szShortName))
                {
                    return OPER_ERROR | DE_INVALIDFILES;
                }
            }
            StrCpyN(pdth->szDestPath, szTemp, ARRAYSIZE(pdth->szDestPath));
        }
        else if (pdth->oper == OPER_ENTERDIR)
        {
            // if the last operation was an enterdir and it has no children
            // (because it failed the above test
            // then we should do a leave dir on it now
            oper = OPER_LEAVEDIR;
            fFreeLastNode = FALSE;

        }
        else if (pdth->pdtnCurrent->pdtnNext)
        {
            pdth->pdtnCurrent = pdth->pdtnCurrent->pdtnNext;

            if (!pdth->pdtnCurrent->pdtnParent)
            {
                // if this was the top, we need to build the next path info
                // from scratch

                if (pdth->pdtnCurrent->fNewRoot)
                {
                    if (pdth->pdtnCurrent->fConnectedElement)
                    {
                        // Since this is a new root in a Connected list, the pFrom and pTo are
                        // stored in the node itself. This is needed because Connected elements may
                        // not exist for every item in the source list and we do not want to create dummy
                        // nodes for each one of them. So, pFrom and pTo are stored for every NewRoot of
                        // connected elements and we use these here.
                        pdth->pFrom = pdth->pdtnCurrent->ConnectedInfo.pFromConnected;
                        pdth->pTo = pdth->pdtnCurrent->ConnectedInfo.pToConnected;
                    }
                    else
                    {
                        // go to the next path pair
                        pdth->pFrom += lstrlen(pdth->pFrom) + 1;
                        if (pdth->pTo)
                        {
                            if (pdth->fMultiDest)
                            {
                                pdth->pTo += lstrlen(pdth->pTo) + 1;
                            }
                        }
                    }
                }

                if (pdth->pdtnCurrent->fDummy)
                {
                    // if this is just a placeholder... go on to the next one
                    if (fFreeLastNode)
                    {
                        DTFreeNode(pdth, pdtnLastCurrent);
                    }
                    return DTGoToNextNode(pdth, pcs);
                }

                DTInitializePaths(pdth, pcs);
            }
            else
            {

                PathRemoveFileSpec(pdth->szSrcPath);
                PathRemoveFileSpec(pdth->szDestPath);

                // if the source long name is too long, try the short name
                if (!PathCombine(szTemp, pdth->szSrcPath, pdth->pdtnCurrent->szName))
                {
                    if (!PathCombine(szTemp, pdth->szSrcPath, pdth->pdtnCurrent->szShortName))
                    {
                        return OPER_ERROR | DE_INVALIDFILES;
                    }
                }
                StrCpyN(pdth->szSrcPath, szTemp, ARRAYSIZE(pdth->szSrcPath));

                // if the dest long name is too long, try the short name
                if (!PathCombine(szTemp, pdth->szDestPath, pdth->pdtnCurrent->szName))
                {
                    if (!PathCombine(szTemp, pdth->szDestPath, pdth->pdtnCurrent->szShortName))
                    {
                        return OPER_ERROR | DE_INVALIDFILES;
                    }
                }
                StrCpyN(pdth->szDestPath, szTemp, ARRAYSIZE(pdth->szDestPath));
            }
        }
        else
        {
            oper = OPER_LEAVEDIR;
            PathRemoveFileSpec(pdth->szSrcPath);
            PathRemoveFileSpec(pdth->szDestPath);
            pdth->pdtnCurrent = pdth->pdtnCurrent->pdtnParent;
        }

        if (fFreeLastNode)
        {
            DTFreeNode(pdth, pdtnLastCurrent);
        }
    }

    if (!pdth->pdtnCurrent)
    {
        // no more!  we're done!
        return 0;
    }

    DebugDumpPDTN(pdth->pdtnCurrent, TEXT("PDTNCurrent"));

    if (oper == OPER_ENTERDIR)
    {
        if (pcs->lpfo->wFunc == FO_RENAME || !DTNIsDirectory(pdth->pdtnCurrent))
        {
            oper = OPER_DOFILE;
        }
    }

    if (DTNIsRootNode(pdth->pdtnCurrent))
    {
        // we need to diskcheck the source and target because this might
        // be the first time we've seen this drive
        if (!DTDiskCheck(pdth, pcs, pdth->szSrcPath) ||
                !DTDiskCheck(pdth, pcs, pdth->szDestPath))
        {
            pcs->bAbort = TRUE;
            return 0;
        }
    }

    iError = DTValidatePathNames(pdth, oper, pcs);
    if (iError)
    {
        if (iError & OPER_ERROR)
        {
            //For connected nodes, ignore the error and silently abort the node!
            if (DTNIsConnected(pdth->pdtnCurrent))
            {
                DTAbortCurrentNode(pdth);
                return DTGoToNextNode(pdth, pcs);
            }
            else
                return iError;
        }
        else
        {
            switch (iError) 
            {
                case IDNO:
                    DTAbortCurrentNode(pdth);
                    pcs->lpfo->fAnyOperationsAborted = TRUE;
                    return DTGoToNextNode(pdth, pcs);

                case IDCANCEL:
                    // User cancelled the operation
                    pcs->bAbort = TRUE;
                    return 0;
            }
        }
    }

    pdth->oper = oper;
    return oper;
}

int  CopyMoveRetry(COPY_STATE *pcs, LPCTSTR pszDest, int error, ULARGE_INTEGER* puliFileSize);
void CopyError(LPCOPY_STATE, LPCTSTR, LPCTSTR, int, UINT, int);

void SetProgressTime(COPY_STATE *pcs);
void SetProgressText(COPY_STATE *pcs, LPCTSTR pszFrom, LPCTSTR pszTo);
void FOUndo_AddInfo(UNDOATOM *lpua, LPTSTR lpszSrc, LPTSTR lpszDest, DWORD dwAttributes);
void CALLBACK FOUndo_Release(UNDOATOM *lpua);
void FOUndo_FileReallyDeleted(LPTSTR lpszFile);
void AddRenamePairToHDSA(LPCTSTR pszOldPath, LPCTSTR pszNewPath, HDSA* phdsaRenamePairs);
BOOL_PTR CALLBACK FOFProgressDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam);

typedef struct {
    LPTSTR lpszName;
    DWORD dwAttributes;
} FOUNDO_DELETEDFILEINFO, *LPFOUNDO_DELETEDFILEINFO;

typedef struct {
    HDPA hdpa;
    HDSA hdsa;
} FOUNDODATA, *LPFOUNDODATA;


void ReleasePCS(COPY_STATE *pcs)
{
    if (0 == InterlockedDecrement(&pcs->nRef))
    {
        SimplePidlCache_Release(&pcs->spc);
        LocalFree(pcs);
    }
}

DWORD CALLBACK AddRefPCS(COPY_STATE *pcs)
{
    return InterlockedIncrement(&pcs->nRef);
}

DWORD CALLBACK FOUIThreadProc(COPY_STATE *pcs)
{
    DebugMsg(TF_DEBUGCOPY, TEXT("FOUIThreadProc -- Begin"));

    Sleep(SHOW_PROGRESS_TIMEOUT);

    if (!pcs->fDone)
    {
        HWND hwndParent;
        FOUITHREADINFO fouiti = {0};

        ENTERCRITICAL;
        if (!pcs->fDone)
        {
            // need to check again within the critsec to make sure that pcs->lpfo is still valid
            fouiti.pcs = pcs;
            fouiti.wFunc = pcs->lpfo->wFunc;
            fouiti.bIsEmptyRBOp = ((pcs->lpfo->lpszProgressTitle == MAKEINTRESOURCE(IDS_BB_EMPTYINGWASTEBASKET)) ||
                    (pcs->lpfo->lpszProgressTitle == MAKEINTRESOURCE(IDS_BB_DELETINGWASTEBASKETFILES)));

            hwndParent = pcs->lpfo->hwnd;
        }
        LEAVECRITICAL;

        if (fouiti.pcs)
        {
            HWND hwnd = CreateDialogParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_MOVECOPYPROGRESS),
                    hwndParent, FOFProgressDlgProc, (LPARAM)&fouiti);
            if (hwnd)
            {
                MSG msg;
                DWORD dwShowTime;
                int iShowTimeLeft;

                // crit section to sync with main thread termination
                ENTERCRITICAL;
                if (!pcs->fDone)
                {
                    pcs->hwndProgress = hwnd;
                }
                LEAVECRITICAL;

                dwShowTime = GetTickCount();
                while (!pcs->fDone && GetMessage(&msg, NULL, 0, 0)) 
                {
                    if (!pcs->fDone && !IsDialogMessage(pcs->hwndProgress, &msg)) 
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }

                // if we've put it up, we need to keep it up for at least some minimal amount of time
                iShowTimeLeft = MINSHOWTIME - (GetTickCount() - dwShowTime);
                if (iShowTimeLeft > 0) 
                {
                    DebugMsg(TF_DEBUGCOPY, TEXT("FOUIThreadProc -- doing an extra sleep"));
                    Sleep(iShowTimeLeft);
                }

                // Keep us from doing this while other thread processing...
                ENTERCRITICAL;
                pcs->hwndProgress = NULL;
                LEAVECRITICAL;

                DestroyWindow(hwnd);
            }
        }
        else
        {
            // main thread must have finished
            ASSERT(pcs->fDone);
        }
    }
    ReleasePCS(pcs);

    DebugMsg(TF_DEBUGCOPY, TEXT("FOUIThreadProc -- End . Completed"));
    return 0;
}


// this queries the progress dialog for a cancel and yields.
// it also will show the progress dialog if a certain amount of time has passed
//
// returns:
//    TRUE      cacnel was pressed, abort the operation
//    FALSE     continue
BOOL FOQueryAbort(COPY_STATE *pcs)
{
    if (!pcs->bAbort && pcs->hwndProgress) 
    {
        if (pcs->hwndProgress != pcs->hwndDlgParent) 
        {
            // do this here rather than on the FOUIThreadProc so that we don't have
            // synchronization problems with this thread popping up a dialog on
            // hwndDlgParent then the progress dialog coming up afterwards on top.
            pcs->hwndDlgParent = pcs->hwndProgress;
            ShowWindow(pcs->hwndProgress, SW_SHOW);
            SetForegroundWindow(pcs->hwndProgress);
            SetFocus(GetDlgItem(pcs->hwndProgress, IDCANCEL));

            SetProgressText(pcs, pcs->dth.szSrcPath,
                    pcs->lpfo->wFunc == FO_DELETE ? NULL : pcs->dth.szDestPath);
        } 
        else 
        {
            MSG msg;

            // win95 handled messages in here.
            // we need to do the same in order to flush the input queue as well as
            // for backwards compatability.

            // we need to flush the input queue now because hwndProgress is
            // on a different thread... which means it has attached thread inputs
            // inorder to unlock the attached threads, we need to remove some
            // sort of message until there's none left... any type of message..
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (!IsDialogMessage(pcs->hwndProgress, &msg)) 
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        if (pcs->dth.dtAll.fChanged || pcs->dth.dtDone.fChanged) 
        {
            if (!pcs->dth.fChangePosted) 
            {
                // set the flag first because with async threads
                // the progress window could handle it and clear the
                // bit before we set it.. then we'd lose further messages
                // thinking that one was still pending
                pcs->dth.fChangePosted = TRUE;
                if (!PostMessage(pcs->hwndProgress, PDM_UPDATE, 0, 0))
                    pcs->dth.fChangePosted = FALSE;
            }
        }
    }

    return pcs->bAbort;
}




typedef struct _confdlg_data {
    LPCTSTR pFileDest;
    LPCTSTR pFileSource;
    LPCTSTR pStreamNames;
    const WIN32_FIND_DATA *pfdDest;
    const WIN32_FIND_DATA *pfdSource;

    BOOL bShowCancel;           // allow cancel out of this operation
    BOOL bShowDates;            // use date/size info in message
    UINT uDeleteWarning;        // warn that the delete's not going to the wastebasket
    BOOL bFireIcon;
    BOOL bShrinkDialog;         // should we move the buttons up to the text?
    int  nSourceFiles;          // if != 1 used to build "n files" string
    int idText;                 // if != 0 use to override string in dlg template
    CONFIRM_FLAG fConfirm;      // we will confirm things set here
    CONFIRM_FLAG fYesMask;      // these bits are cleared in fConfirm on "yes"
    // Only use fYesMask for things that should be confirmed once per operation
    CONFIRM_FLAG fYesToAllMask; // these bits are cleared in fConfirm on "yes to all"
    //COPY_STATE *pcs;
    CONFIRM_DATA *pcd;
    void (*InitConfirmDlg)(HWND hDlg, struct _confdlg_data *pcd);  // routine to initialize dialog
    BOOL bARPWarning; 
} CONFDLG_DATA;


BOOL BuildDateLine(LPTSTR pszDateLine, const WIN32_FIND_DATA *pFind, LPCTSTR pFileName)
{
    TCHAR szTemplate[64];
    TCHAR szNum[32], szTmp[64];
    WIN32_FIND_DATA fd;
    ULARGE_INTEGER liFileSize;

    if (!pFind) 
    {
        HANDLE hfind = FindFirstFile(pFileName, &fd);
        ASSERT(hfind != INVALID_HANDLE_VALUE);
        FindClose(hfind);
        pFind = &fd;
    }

    liFileSize.LowPart  = pFind->nFileSizeLow;
    liFileSize.HighPart = pFind->nFileSizeHigh;

    // There are cases where the date is 0, this is especially true when the 
    // source is from a file contents...
    if (pFind->ftLastWriteTime.dwLowDateTime || pFind->ftLastWriteTime.dwHighDateTime)
    {
        DWORD dwFlags = FDTF_LONGDATE | FDTF_RELATIVE | FDTF_LONGTIME;

        SHFormatDateTime(&pFind->ftLastWriteTime, &dwFlags, szTmp, SIZECHARS(szTmp));

        LoadString(HINST_THISDLL, IDS_DATESIZELINE, szTemplate, ARRAYSIZE(szTemplate));
        wsprintf(pszDateLine, szTemplate, StrFormatByteSize64(liFileSize.QuadPart, szNum, ARRAYSIZE(szNum)),
                szTmp);
    }
    else
    {
        // Simpy output the number to the string
        StrFormatByteSize64(liFileSize.QuadPart, pszDateLine, 64);
        if (liFileSize.QuadPart == 0)
            return FALSE;
    }
    return TRUE;    // valid data in the strings
}


// hide the cancel button and move "Yes" and "No" over to the right positions.
//
// "Yes" is IDYES
// "No"  is IDNO
//

#define HideYesToAllAndCancel(hdlg) HideConfirmButtons(hdlg, IDCANCEL)
#define HideYesToAllAndNo(hdlg) HideConfirmButtons(hdlg, IDNO)

void HideConfirmButtons(HWND hdlg, int idHide)
{
    HWND hwndCancel = GetDlgItem(hdlg, IDCANCEL);
    HWND hwndYesToAll = GetDlgItem(hdlg, IDD_YESTOALL);
    if (hwndCancel)
    {
        RECT rcCancel;
        HWND hwndNo;
        GetWindowRect(hwndCancel, &rcCancel);

        hwndNo = GetDlgItem(hdlg, IDNO);
        if (hwndNo)
        {
            RECT rcNo;
            HWND hwndYes;

            GetWindowRect(hwndNo, &rcNo);

            MapWindowRect(NULL, hdlg, &rcCancel);

            SetWindowPos(hwndNo, NULL, rcCancel.left, rcCancel.top,
                    0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

            hwndYes = GetDlgItem(hdlg, IDYES);
            if (hwndYes)
            {
                MapWindowRect(NULL, hdlg, &rcNo);

                SetWindowPos(hwndYes, NULL, rcNo.left, rcNo.top,
                        0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
            }
        }

        // Although the function is called "Hide", we actually destroy
        // the windows, because keyboard accelerators for hidden windows
        // are still active!
        if (hwndYesToAll)
            DestroyWindow(hwndYesToAll);
        DestroyWindow(GetDlgItem(hdlg, idHide));
    }
}

int MoveDlgItem(HWND hDlg, UINT id, int y)
{
    RECT rc;
    HWND hwnd = GetDlgItem(hDlg, id);
    if (hwnd)
    {
        GetWindowRect(hwnd, &rc);
        MapWindowRect(NULL, hDlg, &rc);
        SetWindowPos(hwnd, NULL, rc.left, y, 0,0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return rc.top - y; // return how much it moved
    }
    return 0;
}

void ShrinkDialog(HWND hDlg, UINT idText)
{
    RECT rc;
    int y;
    HWND hwnd;
    hwnd = GetDlgItem(hDlg, idText);
    ASSERT(hwnd);
    GetWindowRect(hwnd, &rc);
    MapWindowRect(NULL, hDlg, &rc);
    y = rc.bottom + 12;

    // move all the buttons
    MoveDlgItem(hDlg, IDNO, y);
    MoveDlgItem(hDlg, IDCANCEL, y);
    MoveDlgItem(hDlg, IDD_YESTOALL, y);
    y = MoveDlgItem(hDlg, IDYES, y);

    // now resize the entire dialog
    GetWindowRect(hDlg, &rc);
    SetWindowPos(hDlg, NULL, 0, 0, rc.right - rc.left, rc.bottom - y - rc.top, SWP_NOMOVE | SWP_NOZORDER |SWP_NOACTIVATE);
}

void InitConfirmDlg(HWND hDlg, CONFDLG_DATA *pcd)
{
    TCHAR szMessage[255];
    TCHAR szDeleteWarning[80];
    TCHAR szSrc[32];
    TCHAR szFriendlyName[MAX_PATH];
    SHFILEINFO  sfi;
    SHFILEINFO sfiDest;
    LPTSTR pszFileDest = NULL;
    LPTSTR pszMsg, pszSource;
    int i;
    int cxWidth;
    RECT rc;
    HDC hdc;
    HFONT hfont;
    HFONT hfontSave;
    SIZE size;

    BOOL bIsARPWarning = pcd->bARPWarning;

    ASSERT((bIsARPWarning && (pcd->nSourceFiles == 1)) || (!bIsARPWarning));

    hdc = GetDC(hDlg);
    hfont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
    hfontSave = (HFONT)SelectObject(hdc, hfont);

    // get the size of the text boxes
    GetWindowRect(GetDlgItem(hDlg, pcd->idText), &rc);
    cxWidth = rc.right - rc.left;

    //
    // There are cases where, if the filename has no spaces, the static text
    // control will put the entire file name with our quote character down
    // on the 2nd line.  To account for this, we subtract off the width of a
    // the quote character. Since the quote character comes from the resource
    // string, it could really be just about an character, with just about
    // any width.  So we assume its about the width of the letter 0, which
    // should be more than wide enough.
    size.cx = 0;
    GetTextExtentPoint(hdc, TEXT("0"), 1, &size);
    cxWidth -= size.cx * 2;

    if (!bIsARPWarning && !pcd->bShowCancel)
        HideYesToAllAndCancel(hDlg);

    switch (pcd->nSourceFiles) 
    {
        case -1:
            LoadString(HINST_THISDLL, IDS_SELECTEDFILES, szSrc, ARRAYSIZE(szSrc));
            pszSource = szSrc;
            break;

        case 1:
            if (bIsARPWarning)
            {
                TCHAR szTarget[MAX_PATH];
                DWORD cchFriendlyName = ARRAYSIZE(szFriendlyName);
                HRESULT hres = GetPathFromLinkFile(pcd->pFileSource, szTarget, ARRAYSIZE(szTarget));
                if (S_OK == hres)
                {
                    if (SUCCEEDED(AssocQueryString(ASSOCF_VERIFY | ASSOCF_OPEN_BYEXENAME, ASSOCSTR_FRIENDLYAPPNAME,
                                    szTarget, NULL, szFriendlyName, &cchFriendlyName)))
                    {
                        pszSource = szFriendlyName;
                    }
                    else
                    {
                        pszSource = PathFindFileName(szTarget);
                    }
                }
                else if (S_FALSE == hres)
                {
                    TCHAR szProductCode[MAX_PATH];
                    szProductCode[0] = 0;

                    if ((ERROR_SUCCESS == MsiDecomposeDescriptor(szTarget, szProductCode, NULL, NULL, NULL)) && 
                            (ERROR_SUCCESS == MsiGetProductInfo(szProductCode, INSTALLPROPERTY_PRODUCTNAME, szFriendlyName, &cchFriendlyName)))
                    {
                        pszSource = szFriendlyName;
                    }
                    else
                        goto UNKNOWNAPP;

                }
                else
                {
UNKNOWNAPP:
                    LoadString(HINST_THISDLL, IDS_UNKNOWNAPPLICATION, szSrc, ARRAYSIZE(szSrc));
                    pszSource = szSrc;
                }
            }
            else
            {
                SHGetFileInfo(pcd->pFileSource,
                        (pcd->fConfirm==CONFIRM_DELETE_FOLDER || pcd->fConfirm==CONFIRM_WONT_RECYCLE_FOLDER)? FILE_ATTRIBUTE_DIRECTORY : 0,
                        &sfi, sizeof(sfi), SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES);
                pszSource = sfi.szDisplayName;
                PathCompactPath(hdc, pszSource, cxWidth);
            }
            break;

        default:
            pszSource = AddCommas(pcd->nSourceFiles, szSrc, ARRAYSIZE(szSrc));
            break;
    }

    // if we're supposed to show the date info, grab the icons and format the date string
    if (pcd->bShowDates) 
    {
        SHFILEINFO  sfi2;
        TCHAR szDateSrc[64], szDateDest[64];

        BuildDateLine(szDateSrc, pcd->pfdSource, pcd->pFileSource);
        SetDlgItemText(hDlg, IDD_FILEINFO_NEW,  szDateSrc);

        BuildDateLine(szDateDest, pcd->pfdDest, pcd->pFileDest);
        SetDlgItemText(hDlg, IDD_FILEINFO_OLD,  szDateDest);

        SHGetFileInfo(pcd->pFileDest, pcd->pfdDest ? pcd->pfdDest->dwFileAttributes : 0, &sfi2, sizeof(sfi2),
                pcd->pfdDest ? (SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON) : (SHGFI_ICON|SHGFI_LARGEICON));
        ReplaceDlgIcon(hDlg, IDD_ICON_OLD, sfi2.hIcon);

        SHGetFileInfo(pcd->pFileSource, pcd->pfdSource ? pcd->pfdSource->dwFileAttributes : 0, &sfi2, sizeof(sfi2),
                pcd->pfdSource ? (SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON) : (SHGFI_ICON|SHGFI_LARGEICON));
        ReplaceDlgIcon(hDlg, IDD_ICON_NEW, sfi2.hIcon);
    }


    if (!bIsARPWarning)
    {
        // there are multiple controls:
        // IDD_TEXT contains regular text (normal file/folder)
        // IDD_TEXT1 - IDD_TEXT4 contain optional secondary text

        for (i = IDD_TEXT; i <= IDD_TEXT4; i++) 
        {
            if (i == pcd->idText) 
            {
                szMessage[0] = 0;
                GetDlgItemText(hDlg, i, szMessage, ARRAYSIZE(szMessage));
            } 
            else 
            {
                HWND hwndCtl = GetDlgItem(hDlg, i);
                if (hwndCtl)
                    ShowWindow(hwndCtl, SW_HIDE);
            }
        }
    }
    else
    {
        GetDlgItemText(hDlg, IDD_ARPWARNINGTEXT, szMessage, ARRAYSIZE(szMessage));
    }

    // REVIEW Is there some better way?  The code above always hides
    // this control, and I don't see a way around this

    if (pcd->pStreamNames) 
    {
        SetDlgItemText(hDlg, IDD_TEXT1, pcd->pStreamNames);
        ShowWindow(GetDlgItem(hDlg, IDD_TEXT1), SW_SHOW);
    }

    if (pcd->bShrinkDialog)
        ShrinkDialog(hDlg, pcd->idText);

    if (pcd->pFileDest) 
    {
        SHGetFileInfo(pcd->pFileDest, 0,
                &sfiDest, sizeof(sfiDest), SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES);
        pszFileDest = sfiDest.szDisplayName;
        PathCompactPath(hdc, pszFileDest, cxWidth);
    }

    if (pcd->uDeleteWarning) 
    {
        LPITEMIDLIST pidl;

        if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, CSIDL_BITBUCKET, &pidl)))
        {
            SHFILEINFO fi;

            if (SHGetFileInfo((LPCTSTR)pidl, 0, &fi, sizeof(fi), SHGFI_PIDL | SHGFI_ICON |SHGFI_LARGEICON))
            {
                ReplaceDlgIcon(hDlg, IDD_ICON_WASTEBASKET, fi.hIcon);
            }
            ILFree(pidl);
        }
        LoadString(HINST_THISDLL, pcd->uDeleteWarning, szDeleteWarning, ARRAYSIZE(szDeleteWarning));
    } 
    else
        szDeleteWarning[0] = 0;

    if (pcd->bFireIcon) 
    {
        ReplaceDlgIcon(hDlg, IDD_ICON_WASTEBASKET, LoadImage(HINST_THISDLL, MAKEINTRESOURCE(IDI_NUKEFILE), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS));
    }

    pszMsg = ShellConstructMessageString(HINST_THISDLL, szMessage,
            pszSource, pszFileDest, szDeleteWarning);

    if (pszMsg) 
    {
        SetDlgItemText(hDlg, pcd->idText, pszMsg);
        LocalFree(pszMsg);
    }


    SelectObject(hdc, hfontSave);
    ReleaseDC(hDlg, hdc);
}


BOOL_PTR CALLBACK ConfirmDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    CONFDLG_DATA *pcd = (CONFDLG_DATA *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (wMsg)
    {
        case WM_INITDIALOG:
            SetWindowLongPtr(hDlg, DWLP_USER, lParam);
            pcd = (CONFDLG_DATA *)lParam;
            pcd->InitConfirmDlg(hDlg, pcd);
            break;

        case WM_DESTROY:
            // Handle case where the allocation of the PCD failed.
            if (!pcd)
                break;

            if (pcd->bShowDates) 
            {
                ReplaceDlgIcon(hDlg, IDD_ICON_NEW, NULL);
                ReplaceDlgIcon(hDlg, IDD_ICON_OLD, NULL);
            }

            ReplaceDlgIcon(hDlg, IDD_ICON_WASTEBASKET, NULL);
            break;

        case WM_COMMAND:
            if (!pcd)
                break;

            switch (GET_WM_COMMAND_ID(wParam, lParam)) 
            {
                case IDNO:
                    if (GetKeyState(VK_SHIFT) < 0)      // force NOTOALL
                    {
                        // I use the fYesToAllMask here.  There used to be a fNoToAllMask but I
                        // removed it.  When you select "No To All" what you are saying is that
                        // anything I would be saying yes to all for I am actually saying "no to
                        // all" for.  I feel that it is confusing and unnecessary to have both.
                        pcd->pcd->fNoToAll |= pcd->fYesToAllMask;
                    }
                    EndDialog(hDlg, IDNO);
                    break;

                case IDD_YESTOALL:
                    // pcd is the confirmation data for just this file/folder.  pcd->pcd is the
                    // confirm data for the entire copy operation.  When we get a Yes To All we
                    // remove the coresponding bits from the entire operation.
                    pcd->pcd->fConfirm &= ~pcd->fYesToAllMask;
                    EndDialog(hDlg, IDYES);
                    break;

                case IDYES:
                    // There are some messages that we only want to tell the use once even if they
                    // select Yes instead of Yes To All.  As such we sometimes remove bits from the
                    // global confirm state even on a simple Yes.  This mask is usually zero.
                    pcd->pcd->fConfirm &= ~pcd->fYesMask;
                    EndDialog(hDlg, IDYES);
                    break;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    break;
            }
            break;

        case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code)
            {
                case NM_RETURN:
                case NM_CLICK:
                    {
                        TCHAR szModule[MAX_PATH];
                        if (GetSystemDirectory(szModule, ARRAYSIZE(szModule)))
                        {
                            if (PathAppend(szModule, TEXT("appwiz.cpl")))
                            {
                                TCHAR szParam[1 + MAX_PATH + 2 + MAX_CCH_CPLNAME]; // See MakeCPLCommandLine function
                                TCHAR szAppwiz[64];

                                LoadString(g_hinst, IDS_APPWIZCPL, szAppwiz, SIZECHARS(szAppwiz));
                                MakeCPLCommandLine(szModule, szAppwiz, szParam, ARRAYSIZE(szParam));
                                SHRunControlPanelEx(szParam, NULL, FALSE);
                            }
                        }
                        EndDialog(hDlg, IDNO);
                    }
                    break;
            }
            break;

        default:
            return FALSE;
    }
    return TRUE;
}

void SetConfirmMaskAndText(CONFDLG_DATA *pcd, DWORD dwFileAttributes, LPCTSTR pszFile)
{
    if (IS_SYSTEM_HIDDEN(dwFileAttributes) && !ShowSuperHidden())
    {
        dwFileAttributes &= ~FILE_ATTRIBUTE_SUPERHIDDEN;
    }

    // we used to have a desktop.ini "ConfirmFileOp" flag that was set
    // to avoid this case, but there are no folders that are marked READONLY
    // or SYSTEM for a reason other than the shell, so don't consider any as such
    if ((dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY)) &&
            (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        dwFileAttributes &= ~(FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
    }

    if (dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
    {
        pcd->fConfirm = CONFIRM_SYSTEM_FILE;
        pcd->fYesToAllMask |= CONFIRM_SYSTEM_FILE;
        pcd->idText = IDD_TEXT2;
    }
    else if (dwFileAttributes & FILE_ATTRIBUTE_READONLY)
    {
        pcd->fConfirm = CONFIRM_READONLY_FILE;
        pcd->fYesToAllMask |= CONFIRM_READONLY_FILE;
        pcd->idText = IDD_TEXT1;
    }
    else if (pszFile && ((dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
            PathIsRegisteredProgram(pszFile))
    {
        pcd->fConfirm = CONFIRM_PROGRAM_FILE;
        pcd->fYesToAllMask |= CONFIRM_PROGRAM_FILE;
        pcd->idText = IDD_TEXT3;
    }
}


void PauseAnimation(COPY_STATE *pcs, BOOL bStop)
{
    // only called from within the hwndProgress wndproc so assum it's there
    if (bStop)
        Animate_Stop(GetDlgItem(pcs->hwndProgress, IDD_ANIMATE));
    else
        Animate_Play(GetDlgItem(pcs->hwndProgress, IDD_ANIMATE), -1, -1, -1);
}

// confirm a file operation UI.
//
// this routine uses the CONFIRM_DATA in the copy state structure to
// decide if it needs to put up a dailog to confirm the given file operation.
//
// in:
//    pcs           current copy state (confirm flags, hwnd)
//    fConfirm      only one bit may be set! (operation to confirm)
//    pFileSource   source file
//    pFileDest     optional destination file
//    pfdSource
//    pfdDest       find data describing the destination
//
// returns:
//      IDYES
//      IDNO
//      IDCANCEL
//      ERROR_ (DE_) error codes (DE_MEMORY)
//
int ConfirmFileOp(HWND hwnd, COPY_STATE *pcs, CONFIRM_DATA *pcd,
        int nSourceFiles, int cDepth, CONFIRM_FLAG fConfirm,
        LPCTSTR pFileSource, const WIN32_FIND_DATA *pfdSource,
        LPCTSTR pFileDest,   const WIN32_FIND_DATA *pfdDest,
        LPCTSTR pStreamNames)
{
    int dlg;
    int ret;
    CONFDLG_DATA cdd;
    CONFIRM_FLAG fConfirmType;

    if (pcs)
        nSourceFiles = pcs->nSourceFiles;

    cdd.pfdSource = pfdSource;
    cdd.pfdDest = NULL; // pfdDest is only partially filed in
    cdd.pFileSource = pFileSource;
    cdd.pFileDest = pFileDest;
    cdd.pcd = pcd;
    cdd.fConfirm      = fConfirm;       // default, changed below
    cdd.fYesMask      = 0;
    cdd.fYesToAllMask = 0;
    cdd.nSourceFiles = 1;               // default to individual file names in message
    cdd.idText = IDD_TEXT;              // default string from the dlg template
    cdd.bShowCancel = ((nSourceFiles != 1) || cDepth);
    cdd.uDeleteWarning = 0;
    cdd.bFireIcon = FALSE;
    cdd.bShowDates = FALSE;
    cdd.bShrinkDialog = FALSE;
    cdd.InitConfirmDlg = InitConfirmDlg;
    cdd.pStreamNames   = NULL;
    cdd.bARPWarning    = FALSE;

    fConfirmType = fConfirm & CONFIRM_FLAG_TYPE_MASK;

    switch (fConfirmType)
    {
        case CONFIRM_DELETE_FILE:
        case CONFIRM_DELETE_FOLDER:
            {
                BOOL bIsFolderShortcut = FALSE;

                cdd.bShrinkDialog = TRUE;
                // find data for source is in pdfDest
                if ((nSourceFiles != 1) && (pcd->fConfirm & CONFIRM_MULTIPLE))
                {
                    // this is the special CONFIRM_MULTIPLE case (usuall SHIFT+DELETE, or
                    // SHIFT+DRAG to Recycle Bin). if the user says yes to this, they 
                    // basically get no more warnings.
                    cdd.nSourceFiles = nSourceFiles;
                    if ((fConfirm & CONFIRM_WASTEBASKET_PURGE) ||
                            (!pcs || !(pcs->fFlags & FOF_ALLOWUNDO)) ||
                            !BBWillRecycle(cdd.pFileSource, NULL))
                    {
                        // have the fire icon and the REALLY delete warning
                        cdd.uDeleteWarning = IDS_FOLDERDELETEWARNING;
                        cdd.bFireIcon = TRUE;
                        if (pcs)
                            pcs->fFlags &= ~FOF_ALLOWUNDO;

                        if (nSourceFiles == -1)
                        {
                            // -1 indicates that there were >= MAX_EMPTY_FILES files, so we stoped counting
                            // them all up for perf. We use the more generic message in this case.
                            cdd.idText = IDD_TEXT3;
                        }
                        else
                        {
                            // use the "are you sure you want to nuke XX files?" message
                            cdd.idText = IDD_TEXT4;
                        }
                    }
                    else
                    {
                        // uDeleteWarning must be set for the proper recycle icon to be loaded.
                        cdd.uDeleteWarning = IDS_FOLDERDELETEWARNING;
                    }

                    if (!pcs || !pcs->fNoConfirmRecycle)
                    {
                        POINT ptInvoke;
                        HWND hwndPos = NULL;

                        if ((GetNumberOfMonitors() > 1) && GetCursorPos(&ptInvoke))
                        {
                            HMONITOR hMon = MonitorFromPoint(ptInvoke, MONITOR_DEFAULTTONULL);
                            if (hMon)
                            {
                                hwndPos = _CreateStubWindow(&ptInvoke, hwnd);
                            }
                        }

                        ret = (int)DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_DELETE_MULTIPLE), hwndPos ? hwndPos : hwnd, ConfirmDlgProc, (LPARAM)&cdd);

                        if (hwndPos)
                        {
                            DestroyWindow(hwndPos);
                        }

                        if (ret != IDYES)
                        {
                            return IDCANCEL;
                        }
                    }

                    // clear all other possible warnings
                    pcd->fConfirm &= ~(CONFIRM_MULTIPLE | CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER);
                    cdd.fConfirm &= ~(CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER);
                    cdd.nSourceFiles = 1;       // use individual file name
                }

                SetConfirmMaskAndText(&cdd, pfdDest->dwFileAttributes, cdd.pFileSource);

                if ((pfdDest->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
                        PathIsShortcut(cdd.pFileSource, pfdDest->dwFileAttributes))
                {
                    // Its a folder and its a shortcut... must be a FolderShortcut!
                    bIsFolderShortcut = TRUE;

                    // since its a folder, we need to clear out all of these warnings
                    cdd.fYesMask      |= CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER | CONFIRM_MULTIPLE;
                    cdd.fYesToAllMask |= CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER | CONFIRM_MULTIPLE;
                }

                // we want to treat FolderShortcuts as "files" instead of folders. We do this so we don't display dialogs
                // that say stuff like "do you want to delete this and all of its contents" when to the user, this looks like
                // an item instead of a folder (eg nethood shortcut).
                if ((fConfirmType == CONFIRM_DELETE_FILE) || bIsFolderShortcut)
                {
                    dlg = DLG_DELETE_FILE;
                    if ((nSourceFiles == 1) && PathIsShortcutToProgram(cdd.pFileSource))
                    {
                        dlg = DLG_DELETE_FILE_ARP;
                        cdd.idText = IDD_ARPWARNINGTEXT;
                        cdd.bShrinkDialog = FALSE;
                        cdd.bARPWarning = TRUE;
                    }

                    if ((fConfirm & CONFIRM_WASTEBASKET_PURGE)      ||
                            (!pcs || !(pcs->fFlags & FOF_ALLOWUNDO))    ||
                            !BBWillRecycle(cdd.pFileSource, NULL))
                    {
                        // we are really nuking it, so show the appropriate icon/dialog
                        cdd.bFireIcon = TRUE;

                        if (pcs)
                        {
                            pcs->fFlags &= ~FOF_ALLOWUNDO;
                        }

                        cdd.uDeleteWarning = IDS_FILEDELETEWARNING;

                        if (cdd.idText == IDD_TEXT)
                        {
                            cdd.idText = IDD_TEXT4;
                        }
                    }
                    else
                    {
                        // we are recycling it
                        cdd.uDeleteWarning = IDS_FILERECYCLEWARNING;
                    }

                }
                else
                {
                    // fConfirmType == CONFIRM_DELETE_FOLDER
                    if (pcs)
                    {
                        // show cancel on NEXT confirm dialog
                        pcs->nSourceFiles = -1;
                    }

                    cdd.fYesMask      |= CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER | CONFIRM_MULTIPLE;
                    cdd.fYesToAllMask |= CONFIRM_DELETE_FILE | CONFIRM_DELETE_FOLDER | CONFIRM_MULTIPLE;

                    dlg = DLG_DELETE_FOLDER;

                    if ((fConfirm & CONFIRM_WASTEBASKET_PURGE)      ||
                            (!pcs || !(pcs->fFlags & FOF_ALLOWUNDO))    ||
                            !BBWillRecycle(cdd.pFileSource, NULL))
                    {
                        // we are really nuking it, so show the appropriate icon/dialog
                        cdd.bFireIcon = TRUE;

                        if (pcs)
                        {
                            pcs->fFlags &= ~FOF_ALLOWUNDO;
                        }

                        cdd.uDeleteWarning = IDS_FOLDERDELETEWARNING;
                    }
                    else
                    {
                        // we are recycling it
                        cdd.uDeleteWarning = IDS_FOLDERRECYCLEWARNING;
                    }
                }

                //
                // NTRAID#NTBUG9-100335-2001/01/03-jeffreys
                // See also #128485 in the OSR v 4.1 database
                //
                // The fix for 128485 added the BBWillRecycle check below, but this
                // caused NTBUG9-100335.  These 2 bugs say the opposite things.
                // We've had several customer complaints (see dupes of 100335)
                // so I'm putting it back to the way it worked in Windows 2000.
                //
                if (pcs && pcs->fNoConfirmRecycle /*&& BBWillRecycle(cdd.pFileSource, NULL)*/)
                {
                    cdd.fConfirm = 0;
                }
            }
            break;

        case CONFIRM_WONT_RECYCLE_FILE:
        case CONFIRM_WONT_RECYCLE_FOLDER:
            cdd.bShrinkDialog = TRUE;
            cdd.nSourceFiles = 1;
            cdd.bFireIcon = TRUE;
            cdd.idText = IDD_TEXT;
            cdd.fYesMask = CONFIRM_MULTIPLE;
            cdd.fConfirm = fConfirmType;
            cdd.fYesToAllMask = fConfirmType | CONFIRM_MULTIPLE;

            // set the dialog to be file or folder
            if (fConfirmType == CONFIRM_WONT_RECYCLE_FOLDER)
            {
                dlg = DLG_WONT_RECYCLE_FOLDER;
            }
            else
            {
                dlg = DLG_WONT_RECYCLE_FILE;
            }
            break;

        case CONFIRM_PATH_TOO_LONG:
            cdd.bShrinkDialog = TRUE;
            cdd.nSourceFiles = 1;
            cdd.bFireIcon = TRUE;
            cdd.idText = IDD_TEXT;
            cdd.fYesMask = CONFIRM_MULTIPLE;
            cdd.fConfirm = CONFIRM_PATH_TOO_LONG;
            cdd.fYesToAllMask = CONFIRM_PATH_TOO_LONG | CONFIRM_MULTIPLE;
            dlg = DLG_PATH_TOO_LONG;
            break;

        case CONFIRM_WONT_RECYCLE_OFFLINE:
            cdd.bShrinkDialog = TRUE;
            cdd.nSourceFiles = 1;
            cdd.bFireIcon = TRUE;
            cdd.idText = IDD_TEXT;
            cdd.fYesMask = CONFIRM_MULTIPLE;
            cdd.fConfirm = fConfirmType;
            cdd.fYesToAllMask = fConfirmType | CONFIRM_MULTIPLE;
            dlg = DLG_WONT_RECYCLE_OFFLINE;
            break;

        case CONFIRM_STREAMLOSS:
            cdd.bShrinkDialog = FALSE;
            cdd.nSourceFiles  = 1;
            cdd.idText        = IDD_TEXT;
            cdd.fConfirm      = CONFIRM_STREAMLOSS;
            cdd.fYesToAllMask = CONFIRM_STREAMLOSS;
            cdd.pStreamNames  = pStreamNames;
            dlg = DLG_STREAMLOSS_ON_COPY;
            break;

        case CONFIRM_FAILED_ENCRYPT:
            cdd.bShrinkDialog = FALSE;
            cdd.nSourceFiles = 1;
            cdd.idText = IDD_TEXT;
            cdd.bShowCancel = TRUE;
            cdd.fConfirm = CONFIRM_FAILED_ENCRYPT;
            cdd.fYesToAllMask = CONFIRM_FAILED_ENCRYPT;
            dlg = DLG_FAILED_ENCRYPT;
            break;

        case CONFIRM_LOST_ENCRYPT_FILE:
        case CONFIRM_LOST_ENCRYPT_FOLDER:
            cdd.bShrinkDialog = FALSE;
            cdd.nSourceFiles = 1;
            cdd.idText = IDD_TEXT;
            cdd.bShowCancel = TRUE;
            cdd.fConfirm = CONFIRM_LOST_ENCRYPT_FILE | CONFIRM_LOST_ENCRYPT_FOLDER;
            cdd.fYesToAllMask = CONFIRM_LOST_ENCRYPT_FILE | CONFIRM_LOST_ENCRYPT_FOLDER;
            if (fConfirmType == CONFIRM_LOST_ENCRYPT_FILE)
            {
                dlg = DLG_LOST_ENCRYPT_FILE;
            }
            else
            {
                dlg = DLG_LOST_ENCRYPT_FOLDER;
            }
            break;

        case CONFIRM_REPLACE_FILE:
            cdd.bShowDates = TRUE;
            cdd.fYesToAllMask = CONFIRM_REPLACE_FILE;
            SetConfirmMaskAndText(&cdd, pfdDest->dwFileAttributes, NULL);
            dlg = DLG_REPLACE_FILE;
            break;

        case CONFIRM_REPLACE_FOLDER:
            cdd.bShowCancel = TRUE;
            if (pcs) pcs->nSourceFiles = -1;        // show cancel on NEXT confirm dialog
            // this implies operations on the files
            cdd.fYesMask = CONFIRM_REPLACE_FILE;
            cdd.fYesToAllMask = CONFIRM_REPLACE_FILE | CONFIRM_REPLACE_FOLDER;
            dlg = DLG_REPLACE_FOLDER;
            break;

        case CONFIRM_MOVE_FILE:
            cdd.fYesToAllMask = CONFIRM_MOVE_FILE;
            SetConfirmMaskAndText(&cdd, pfdSource->dwFileAttributes, NULL);
            dlg = DLG_MOVE_FILE;
            break;

        case CONFIRM_MOVE_FOLDER:
            cdd.bShowCancel = TRUE;
            cdd.fYesToAllMask = CONFIRM_MOVE_FOLDER;
            SetConfirmMaskAndText(&cdd, pfdSource->dwFileAttributes, cdd.pFileSource);
            dlg = DLG_MOVE_FOLDER;
            break;

        case CONFIRM_RENAME_FILE:
            SetConfirmMaskAndText(&cdd, pfdSource->dwFileAttributes, NULL);
            dlg = DLG_RENAME_FILE;
            break;

        case CONFIRM_RENAME_FOLDER:
            cdd.bShowCancel = TRUE;
            if (pcs) pcs->nSourceFiles = -1;        // show cancel on NEXT confirm dialog
            SetConfirmMaskAndText(&cdd, pfdSource->dwFileAttributes, cdd.pFileSource);
            dlg = DLG_RENAME_FOLDER;
            break;

        default:
            DebugMsg(DM_WARNING, TEXT("bogus confirm option"));
            return IDCANCEL;
    }

    // Does this operation need to be confirmed?
    if (pcd->fConfirm & cdd.fConfirm)
    {
        // Has the user already said "No To All" for this operation?
        if ((pcd->fNoToAll & cdd.fConfirm) == cdd.fConfirm)
        {
            ret = IDNO;
        }
        else
        {
            // HACK for multimon, make sure the file operation dialog box comes
            // up on the correct monitor
            POINT ptInvoke;
            HWND hwndPos = NULL;

            if ((GetNumberOfMonitors() > 1) && GetCursorPos(&ptInvoke))
            {
                HMONITOR hMon = MonitorFromPoint(ptInvoke, MONITOR_DEFAULTTONULL);
                if (hMon)
                {
                    hwndPos = _CreateStubWindow(&ptInvoke, hwnd);
                }
            }
            ret = (int)DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(dlg), (hwndPos ? hwndPos : hwnd), ConfirmDlgProc, (LPARAM)&cdd);

            if (hwndPos)
                DestroyWindow(hwndPos);

            if (ret == -1)
                ret = ERROR_NOT_ENOUGH_MEMORY;
        }
    }
    else
    {
        ret = IDYES;
    }

    return ret;
}

//
//  DTNIsParentConnectOrigin()
//
//      When a folder ("c:\foo files") is moved to a different drive ("a:\"), the source and the
//  destinations have different roots, and therefore the "fRecursive" flag is turned ON by default.
//  This results in confirmations obtained for the individual files ("c:\foo files\aaa.gif") 
// rather than the folder itself. We need to first find the parent and then save the confirmation 
// in the connected element of it's parent. This function gets the top-most parent and then
// checks to see if it is a connect origin and if so returns that parent pointer.
//

PDIRTREENODE DTNGetConnectOrigin(PDIRTREENODE pdtn)
{
    PDIRTREENODE    pdtnParent = pdtn;

    //Get the top-level parent of the given node.
    while (pdtn)
    {
        pdtnParent = pdtn;
        pdtn = pdtn->pdtnParent;
    }

    //Now check if the parent is a connect origin.
    if (pdtnParent && DTNIsConnectOrigin(pdtnParent))
        return pdtnParent; //If so, return him.
    else
        return NULL;
}

//
// CachedConfirmFileOp()
//
//    When a file("foo.htm") is moved/copied, we may put up a confirmation dialog in case 
// of a conflict and the end-user might have responded saying "Yes", "no" etc., When the 
// corresponding connected element ("foo files") is also moved/copied etc., we should NOT put up
// a confirmation dialog again. We must simply store the answer to the original confirmation and
// use it later. 
//  
//  What this function does is: if the given node is a connected element, it simply retrieves the
// confirmation for the original operation and returns.  If the given element is NOT a connected 
// element, then this function calls the ConfirmFileOp and stores the confirmation result in 
// it's connected element sothat, it later it can be used by the connected element.
//

int CachedConfirmFileOp(HWND hwnd, COPY_STATE *pcs, CONFIRM_DATA *pcd,
        int nSourceFiles, int cDepth, CONFIRM_FLAG fConfirm,
        LPCTSTR pFileSource, const WIN32_FIND_DATA *pfdSource,
        LPCTSTR pFileDest,   const WIN32_FIND_DATA *pfdDest,
        LPCTSTR pStreamNames)

{
    int result;

    //See if this is a connected item.
    if (DTNIsConnected(pcs->dth.pdtnCurrent))
    {
        // Since this is a connected item, the confirmation must already have been obtained from
        // the user and get it from the cache!
        result = DTNGetConfirmationResult(pcs->dth.pdtnCurrent);
    }
    else
    {
        PDIRTREENODE    pdtnConnectOrigin;

        result = ConfirmFileOp(hwnd, pcs, pcd, nSourceFiles, cDepth, fConfirm, pFileSource, 
                pfdSource, pFileDest, pfdDest, pStreamNames);

        //Check if this node has a connection.
        if (pdtnConnectOrigin = DTNGetConnectOrigin(pcs->dth.pdtnCurrent))
        {
            pdtnConnectOrigin->pdtnConnected->ConnectedInfo.dwConfirmation = result;

            // PERF: Can we check for the result to be IDCANCEL or IDNO and if so make the
            // connected node a Dummy? Currently this won't work because current code assumes
            // that dummy nodes do not have children. This connected node might have some children.
            // if ((result == IDCANCEL) || (result == IDNO))
            //    pdtnConnectOrigin->pdtnConnected->fDummy = TRUE;
        }

    }

    return result;
}

void GuessAShortName(LPCTSTR p, LPTSTR szT)
{
    int i, j, fDot, cMax;

    for (i = j = fDot = 0, cMax = 8; *p; p++)
    {
        if (*p == TEXT('.'))
        {
            // if there was a previous dot, step back to it
            // this way, we get the last extension
            if (fDot)
                i -= j+1;

            // set number of chars to 0, put the dot in
            j = 0;
            szT[i++] = TEXT('.');

            // remember we saw a dot and set max 3 chars.
            fDot = TRUE;
            cMax = 3;
        }
        else if (j < cMax && (PathGetCharType(*p) & GCT_SHORTCHAR))
        {
            // if *p is a lead byte, we move forward one more
            if (IsDBCSLeadByte(*p))
            {
                szT[i] = *p++;
                if (++j >= cMax)
                    continue;
                ++i;
            }
            j++;
            szT[i++] = *p;
        }
    }
    szT[i] = 0;
}

/* GetNameDialog
 *
 *  Runs the dialog box to prompt the user for a new filename when copying
 *  or moving from HPFS to FAT.
 */

typedef struct {
    LPTSTR pszDialogFrom;
    LPTSTR pszDialogTo;
    BOOL bShowCancel;
} GETNAME_DATA;

BOOL_PTR CALLBACK GetNameDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    TCHAR szT[14];
    TCHAR szTo[MAX_PATH];
    GETNAME_DATA * pgn = (GETNAME_DATA *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (wMsg) 
    {
        case WM_INITDIALOG:
            SetWindowLongPtr(hDlg, DWLP_USER, lParam);

            pgn = (GETNAME_DATA *)lParam;

            // inform the user of the old name
            PathSetDlgItemPath(hDlg, IDD_FROM, pgn->pszDialogFrom);

            // directory the file will go into
            PathRemoveFileSpec(pgn->pszDialogTo);
            PathSetDlgItemPath(hDlg, IDD_DIR, pgn->pszDialogTo);

            // generate a guess for the new name
            GuessAShortName(PathFindFileName(pgn->pszDialogFrom), szT);

            lstrcpy(szTo, pgn->pszDialogTo);
            PathAppend(szTo, szT);
            // make sure that name is unique
            PathYetAnotherMakeUniqueName(szTo, szTo, NULL, NULL);
            SetDlgItemText(hDlg, IDD_TO, PathFindFileName(szTo));
            SendDlgItemMessage(hDlg, IDD_TO, EM_LIMITTEXT, 13, 0L);

            SHAutoComplete(GetDlgItem(hDlg, IDD_TO), 0);

            if (!pgn->bShowCancel)
                HideYesToAllAndNo(hDlg);
            break;

        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam)) 
            {
                case IDD_YESTOALL:
                case IDYES:
                    GetDlgItemText(hDlg, IDD_TO, szT, ARRAYSIZE(szT));
                    PathAppend(pgn->pszDialogTo, szT);
                    PathQualify(pgn->pszDialogTo);
                    // fall through
                case IDNO:
                case IDCANCEL:
                    EndDialog(hDlg,GET_WM_COMMAND_ID(wParam, lParam));
                    break;

                case IDD_TO:
                    {
                        LPCTSTR p;
                        GetDlgItemText(hDlg, IDD_TO, szT, ARRAYSIZE(szT));
                        for (p = szT; *p; p = CharNext(p)) 
                        {
                            if (!(PathGetCharType(*p) & GCT_SHORTCHAR))
                                break;
                        }

                        EnableWindow(GetDlgItem(hDlg,IDYES), ((!*p) && (p != szT)));
                    }
                    break;

                default:
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }

    return TRUE;
}

int GetNameDialog(HWND hwnd, COPY_STATE *pcs, BOOL fMultiple,UINT wOp, LPTSTR pFrom, LPTSTR pTo)
{
    int iRet;

    // if we don't want to confirm this, just mock up a string and return ok
    if (!(pcs->cd.fConfirm & CONFIRM_LFNTOFAT)) 
    {
        TCHAR szTemp[MAX_PATH];
        GuessAShortName(PathFindFileName(pFrom), szTemp);
        PathRemoveFileSpec(pTo);
        PathAppend(pTo, szTemp);
        // make sure that name is unique
        PathYetAnotherMakeUniqueName(pTo, pTo, NULL, NULL);
        iRet = IDYES;
    } 
    else 
    {
        GETNAME_DATA gn;
        gn.pszDialogFrom = pFrom;
        gn.pszDialogTo = pTo;
        gn.bShowCancel = fMultiple;

        iRet = (int)DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_LFNTOFAT), hwnd, GetNameDlgProc, (LPARAM)(GETNAME_DATA *)&gn);
        if (iRet == IDD_YESTOALL)
            pcs->cd.fConfirm &= ~CONFIRM_LFNTOFAT;
    }
    return iRet;
}

STDAPI_(void) SHFreeNameMappings(void *hNameMappings)
{
    HDSA hdsaRenamePairs = (HDSA)hNameMappings;
    int i;

    if (!hdsaRenamePairs)
        return;

    i = DSA_GetItemCount(hdsaRenamePairs) - 1;
    for (; i >= 0; i--)
    {
        SHNAMEMAPPING FAR* prp = DSA_GetItemPtr(hdsaRenamePairs, i);

        LocalFree(prp->pszOldPath);
        LocalFree(prp->pszNewPath);
    }

    DSA_Destroy(hdsaRenamePairs);
}

void _ProcessNameMappings(LPTSTR pszTarget, HDSA hdsaRenamePairs)
{
    int i;

    if (!hdsaRenamePairs)
        return;

    for (i = DSA_GetItemCount(hdsaRenamePairs) - 1; i >= 0; i--)
    {
        TCHAR  cTemp;
        SHNAMEMAPPING FAR* prp = DSA_GetItemPtr(hdsaRenamePairs, i);

        //  I don't call StrCmpNI 'cause I already know cchOldPath, and
        //  it has to do a couple of lstrlen()s to calculate it.
        cTemp = pszTarget[prp->cchOldPath];
        pszTarget[prp->cchOldPath] = 0;

        //  Does the target match this collision renaming entry?
        // NOTE: We are trying to compare a path to a path.  prp->pszOldPath
        // does not have a trailing "\" character, so this isn't covered
        // by the lstrcmpi below.  As such, cTemp had best be the path
        // seperator character to ensure that the modified pszTarget is actually
        // a path and not a filename or a longer path name that doesn't match
        // but happens to start with the same characters as prp->pszOldPath.
        if ((cTemp == TEXT('\\')) && !lstrcmpi(pszTarget, prp->pszOldPath))
        {
            // Get subtree string of the target.
            TCHAR *pszSubTree = &(pszTarget[prp->cchOldPath + 1]);

            // Generate the new target path.
            PathCombine(pszTarget, prp->pszNewPath, pszSubTree);

            break;
        }
        else
        {
            // Restore the trounced character.
            pszTarget[prp->cchOldPath] = cTemp;
        }
    }
}

/* Sets the status dialog item in the modeless status dialog box. */

// used for both the drag drop status dialogs and the manual user
// entry dialogs so be careful what you change

void SetProgressText(COPY_STATE *pcs, LPCTSTR pszFrom, LPCTSTR pszTo)
{
    HWND hwndProgress = pcs->hwndProgress;

    if (hwndProgress && !(pcs->fFlags & FOF_SIMPLEPROGRESS))
    {
        TCHAR szFrom[MAX_PATH], szTo[MAX_PATH];
        LPTSTR pszMsg = NULL;

        HDC hdc;
        HFONT hfont;
        HFONT hfontSave;
        RECT rc;
        int cxWidth;
        SIZE size;

        //
        // Compute the size we can use for our file names (REVIEW: Cache this result?)
        //
        hdc = GetDC(hwndProgress);
        hfont = (HFONT)SendMessage(hwndProgress, WM_GETFONT, 0, 0);
        hfontSave = (HFONT)SelectObject(hdc, hfont);

        GetWindowRect(GetDlgItem(hwndProgress, IDD_TONAME), &rc);
        cxWidth = rc.right - rc.left;

        if (NULL != pszTo && pcs->fFlags & FOF_MULTIDESTFILES)
        {
            lstrcpy(szFrom, pszTo);
        }
        else
        {
            lstrcpy(szFrom, pszFrom);
        }
        PathStripPath(szFrom);
        PathCompactPath(hdc, szFrom, cxWidth);
        SetDlgItemText(hwndProgress, IDD_NAME, szFrom);

        lstrcpy(szFrom, pszFrom);
        if (szFrom[0]) 
        {
            LPTSTR pszResource = MAKEINTRESOURCE(IDS_FROM);
            LPTSTR pszToUsable = NULL;

            szTo[0] = TEXT('\0');
            if (pszTo)
            {
                pszToUsable = szTo;
                pszResource = MAKEINTRESOURCE(IDS_FROMTO);
            }

            pszMsg = ShellConstructMessageString(HINST_THISDLL,
                    pszResource, "", pszToUsable);

            if (NULL != pszMsg)
            {
                GetTextExtentPoint(hdc, pszMsg, lstrlen(pszMsg), &size);
                cxWidth -= size.cx;
                LocalFree(pszMsg);
            }

            //
            // Now build the file names
            //
            PathRemoveFileSpec(szFrom);
            PathStripPath(szFrom);

            if (pszTo)
            {
                lstrcpy(szTo, pszTo);
                PathRemoveFileSpec(szTo);
                PathStripPath(szTo);

                PathCompactPath(hdc, szFrom, cxWidth/2);
                PathCompactPath(hdc, szTo, cxWidth/2);
            }
            else
            {
                PathCompactPath(hdc, szFrom, cxWidth);
            }

            //
            // Now create the real message
            //
            pszMsg = ShellConstructMessageString(HINST_THISDLL,
                    pszResource, szFrom, pszToUsable);

        } 
        else if (!pcs->fDTBuilt) 
        {
            TCHAR szFunc[80];
            if (LoadString(HINST_THISDLL, FOFuncToStringID(pcs->lpfo->wFunc),
                        szFunc, ARRAYSIZE(szFunc))) 
            {
                pszMsg = ShellConstructMessageString(HINST_THISDLL,
                        MAKEINTRESOURCE(IDS_PREPARINGTO), szFunc);
            }
        }

        if (pszMsg)
        {
            SetDlgItemText(hwndProgress, IDD_TONAME, pszMsg);
            LocalFree(pszMsg);
        }

        SelectObject(hdc, hfontSave);
        ReleaseDC(hwndProgress, hdc);

    }
}

void SetProgressTimeEst(COPY_STATE *pcs, DWORD dwTimeLeft)
{
    TCHAR szFmt[60];
    TCHAR szOut[70];
    DWORD dwTime;

    if (pcs->hwndProgress) 
    {
        if (dwTimeLeft > 4*60*60)           // 4 hours and over you get no text
        {
            szFmt[0] = TEXT('\0');
        }
        else if (dwTimeLeft > 60)
        {
            // Note that dwTime is at least 2, so we only need a plural form
            LoadString(HINST_THISDLL, IDS_TIMEEST_MINUTES, szFmt, ARRAYSIZE(szFmt));
            dwTime = (dwTimeLeft / 60) + 1;
        }
        else
        {
            LoadString(HINST_THISDLL, IDS_TIMEEST_SECONDS, szFmt, ARRAYSIZE(szFmt));
            // Round up to 5 seconds so it doesn't look so random
            dwTime = ((dwTimeLeft+4) / 5) * 5;
        }

        wsprintf(szOut, szFmt, dwTime);

        SetDlgItemText(pcs->hwndProgress, IDD_TIMEEST, szOut);
    }
}


// this updates the animation, which could change because we could switch between 
// doing a move to recycle bin and really nuke if the file/folder was bigger that
// the allowable size of the recycle bin.
void UpdateProgressAnimation(COPY_STATE *pcs)
{
    if (pcs->hwndProgress && pcs->lpfo)
    {
        INT_PTR idAni, idAniCurrent;
        HWND hwndAnimation;
        switch (pcs->lpfo->wFunc) 
        {
            case FO_DELETE:
                if ((pcs->lpfo->lpszProgressTitle == MAKEINTRESOURCE(IDS_BB_EMPTYINGWASTEBASKET)) ||
                        (pcs->lpfo->lpszProgressTitle == MAKEINTRESOURCE(IDS_BB_DELETINGWASTEBASKETFILES))) 
                {
                    idAni = IDA_FILENUKE;
                    break;
                } 
                else if (!(pcs->fFlags & FOF_ALLOWUNDO)) 
                {
                    idAni = IDA_FILEDELREAL;
                    break;
                } // else fall through to default

            default:
                idAni = (IDA_FILEMOVE + (int)pcs->lpfo->wFunc - FO_MOVE);
        }

        hwndAnimation = GetDlgItem(pcs->hwndProgress,IDD_ANIMATE);

        idAniCurrent = (INT_PTR) GetProp(hwndAnimation, TEXT("AnimationID"));

        if (idAni != idAniCurrent)
        {
            // the one we should be using is different from the one we have, 
            // so update it

            // close the old clip
            Animate_Close(hwndAnimation);

            // open the new one
            Animate_Open(hwndAnimation, idAni);

            // if the window is enabled, start the new animation playing
            if (IsWindowEnabled(pcs->hwndProgress))
                Animate_Play(hwndAnimation, -1, -1, -1);

            // set the current idAni
            SetProp(hwndAnimation, TEXT("AnimationID"), (HANDLE)idAni);

            // at the same time we update the animation, we also update the text,
            // so that the two will always be in sync
            SetProgressText(pcs, pcs->dth.szSrcPath, pcs->lpfo->wFunc == FO_DELETE ? NULL : pcs->dth.szDestPath);
        }
    }
}


void SendProgressMessage(COPY_STATE *pcs, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (pcs->hwndProgress)
        SendDlgItemMessage(pcs->hwndProgress, IDD_PROBAR, uMsg, wParam, lParam);
}


//
// creates folder and all parts of the path if necessary (parent does not need
// to exists) and verifies that the contents of the folder will be visibile.
//
// in:
//    hwnd      hwnd to post UI on
//    pszPath   full path to create
//    psa       security attributes
//
// returns:
//      ERROR_SUCCESS (0)   success
//      ERROR_              failure
//

STDAPI_(int) SHCreateDirectoryEx(HWND hwnd, LPCTSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
    int ret = ERROR_SUCCESS;

    if (PathIsRelative(pszPath))
    {
        // if not a "full" path bail
        // to ensure that we dont create a dir in the current working directory
        SetLastError(ERROR_BAD_PATHNAME);
        return ERROR_BAD_PATHNAME;
    }

    if (!Win32CreateDirectory(pszPath, psa)) 
    {
        TCHAR *pEnd, *pSlash, szTemp[MAX_PATH];

        ret = GetLastError();

        // There are certain error codes that we should bail out here
        // before going through and walking up the tree...
        switch (ret)
        {
            case ERROR_FILENAME_EXCED_RANGE:
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                return ret;
        }

        lstrcpyn(szTemp, pszPath, ARRAYSIZE(szTemp));
        pEnd = PathAddBackslash(szTemp); // for the loop below

        // assume we have 'X:\' to start this should even work
        // on UNC names because will will ignore the first error

        pSlash = szTemp + 3;

        // create each part of the dir in order

        while (*pSlash) 
        {
            while (*pSlash && *pSlash != TEXT('\\'))
                pSlash = CharNext(pSlash);

            if (*pSlash) 
            {
                ASSERT(*pSlash == TEXT('\\'));

                *pSlash = 0;    // terminate path at seperator

                ret = Win32CreateDirectory(szTemp, pSlash + 1 == pEnd ? psa : NULL) ? ERROR_SUCCESS : GetLastError();

            }
            *pSlash++ = TEXT('\\');     // put the seperator back
        }
    }

    if (ERROR_SUCCESS != ret)
    {
        // We failed, so let's try to display error UI.
        if (hwnd && ERROR_CANCELLED != ret)
        {               
            SHSysErrorMessageBox(hwnd, NULL, IDS_CANNOTCREATEFOLDER, ret,
                    pszPath ? PathFindFileName(pszPath) : NULL, 
                    MB_OK | MB_ICONEXCLAMATION);

            ret = ERROR_CANCELLED; // Indicate we already displayed Error UI.
        }
    }   
    return ret;
}

STDAPI_(int) SHCreateDirectory(HWND hwnd, LPCTSTR pszPath)
{
    return SHCreateDirectoryEx(hwnd, pszPath, NULL);
}

#ifdef UNICODE
STDAPI_(int) SHCreateDirectoryExA(HWND hwnd, LPCSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
    WCHAR wsz[MAX_PATH];
    SHAnsiToUnicode(pszPath, wsz, SIZECHARS(wsz));
    return SHCreateDirectoryEx(hwnd, wsz, psa);
}
#else
STDAPI_(int) SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
    char sz[MAX_PATH];
    SHUnicodeToAnsi(pszPath, sz, SIZECHARS(sz));
    return SHCreateDirectoryEx(hwnd, sz, psa);
}
#endif


// call MPR to find out the speed of a given path
//
// returns
//        0 for unknown
//      144 for 14.4 modems
//       96 for 9600
//       24 for 2400
//
// if the device does not return a speed we return 0
//

DWORD GetPathSpeed(LPCTSTR pszPath)
{
    NETCONNECTINFOSTRUCT nci;
    NETRESOURCE nr;
    TCHAR szPath[MAX_PATH];

    lstrcpyn(szPath, pszPath, ARRAYSIZE(szPath));
    PathStripToRoot(szPath);    // get a root to this path

    memset(&nci, 0, sizeof(nci));
    nci.cbStructure = sizeof(nci);

    memset(&nr, 0, sizeof(nr));
    if (PathIsUNC(szPath))
        nr.lpRemoteName = szPath;
    else
    {
        // Don't bother for local drives
        if (!IsRemoteDrive(DRIVEID(szPath)))
            return 0;

        // we are passing in a local drive and MPR does not like us to pass a
        // local name as Z:\ but only wants Z:
        szPath[2] = 0;   // Strip off after character and :
        nr.lpLocalName = szPath;
    }

    // dwSpeed is returned by MultinetGetConnectionPerformance
    MultinetGetConnectionPerformance(&nr, &nci);

    return nci.dwSpeed;
}


DWORD CopyCallbackProc(LARGE_INTEGER liTotSize, LARGE_INTEGER liBytes,
        LARGE_INTEGER liStreamSize, LARGE_INTEGER liStreamBytes,
        DWORD dwStream, DWORD dwCallback,
        HANDLE hSource, HANDLE hDest, void *pv)
{
    COPY_STATE *pcs = (COPY_STATE *)pv;

    DebugMsg(DM_TRACE, TEXT("CopyCallbackProc[%08lX], totsize=%08lX, bytes=%08lX"),
            dwCallback,  liTotSize.LowPart, liBytes.LowPart);

    if (FOQueryAbort(pcs))
        return PROGRESS_CANCEL;

    DTSetFileCopyProgress(&pcs->dth, liBytes);

    if (pcs->fInitialize)
    {
        // preserve the create date when moving across volumes, otherwise use the
        // create date the file system picked when we did the CreateFile()
        // always preserve modified date (ftLastWriteTime)
        // bummer is we loose accuracy when going to VFAT compared to NT servers

        SetFileTime((HANDLE)hDest, (pcs->lpfo->wFunc == FO_MOVE) ? &pcs->pfd->ftCreationTime : NULL,
                NULL, &pcs->pfd->ftLastWriteTime);

        pcs->fInitialize = FALSE;
    }

    switch (dwCallback)
    {
        case CALLBACK_STREAM_SWITCH:
            break;
        case CALLBACK_CHUNK_FINISHED:
            break;
        default:
            break;
    }
    return PROGRESS_CONTINUE;
}

// copy the SECURITY_DESCRIPTOR for two files
//
// in:
//      pszSource       fully qualified source path
//      pszDest         fully qualified destination path
//
// returns:
//      0       ERROR_SUCCESS
//      WIN32 error codes
//

    DWORD 
CopyFileSecurity(LPCTSTR pszSource, LPCTSTR pszDest)
{
    DWORD err = ERROR_SUCCESS;
    BOOL fRet = TRUE;
    BYTE buf[512];

    //    arbitrarily saying do everything we can
    //    except SACL_SECURITY_INFORMATION because
    SECURITY_INFORMATION si = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    PSECURITY_DESCRIPTOR psd = (PSECURITY_DESCRIPTOR) buf;
    DWORD cbPsd = sizeof(buf);

    if (!SHRestricted(REST_FORCECOPYACLWITHFILE))
    {
        // shell restriction so return access denied?
        return ERROR_ACCESS_DENIED;
    }    

    fRet = GetFileSecurity(pszSource, si, psd, cbPsd, &cbPsd);
    if (!fRet)
    {
        err = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == err)
        {
            // just need to resize the buffer and try again

            psd = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, cbPsd);
            if (psd)
            {
                fRet = GetFileSecurity(pszSource, si, psd, cbPsd, &cbPsd);
            }
            else
            {
                err = ERROR_NOT_ENOUGH_MEMORY;
            }
        }
    }

    if (fRet)
    {
        fRet = SetFileSecurity(pszDest, si, psd);
        if (!fRet)
            err = GetLastError();
    }

    if (psd && psd != buf)
        LocalFree(psd);

    if (fRet)
        return ERROR_SUCCESS;

    return err;
}

// reset the SECURITY_DESCRIPTOR on a file or directory
//
// in:
//      pszDest         fully qualified destination path
//
// returns:
//      0       ERROR_SUCCESS
//      WIN32 error codes
//

    DWORD 
ResetFileSecurity(LPCTSTR pszDest)
{
    DWORD err = ERROR_SUCCESS;

    if (!SHRestricted(REST_FORCECOPYACLWITHFILE))
    {
        ACL acl;
        InitializeAcl(&acl, sizeof(acl), ACL_REVISION);

        // TreeResetNamedSecurityInfo has a callback mechanism, but
        // we currently don't use it. Note that the paths passed to
        // the callback look like
        //     "\Device\HarddiskVolume1\dir\name"

        err = TreeResetNamedSecurityInfo((LPTSTR)pszDest,
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
                NULL,
                NULL,
                &acl,
                NULL,
                FALSE, // KeepExplicit (perms on children)
                NULL,
                ProgressInvokeNever,
                NULL);
    }

    return err;
}

//
// in:
//      hwnd            Window to report things to.
//      pszSource       fully qualified source path
//      pszDest         fully qualified destination path
//      pfd             source file find data (size/date/time/attribs)
//
// returns:
//      ERROR_SUCCESS (0)
//      other Win32 ERROR_ codes
//

UINT FileCopy(COPY_STATE *pcs, LPCTSTR pszSource, LPCTSTR pszDest, const WIN32_FIND_DATA *pfd, BOOL fCreateAlways)
{
    UINT iRet = ERROR_CANCELLED;
    BOOL fRetryPath = FALSE;
    BOOL fRetryAttr = FALSE;
    BOOL fCopyOrMoveSucceeded = FALSE;
    BOOL fSecurityObtained = FALSE;
    DWORD dwCopyFlags;

    // Buffers for security info

    BYTE rgbSecurityDescriptor[512];
    SECURITY_INFORMATION si = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    PSECURITY_DESCRIPTOR psd = (PSECURITY_DESCRIPTOR) rgbSecurityDescriptor;
    DWORD cbPsd = sizeof(rgbSecurityDescriptor);

    // Make sure we can start
    if (FOQueryAbort(pcs))
        return ERROR_CANCELLED;

    //
    // Now do the file copy/move
    //

    // Get the security info from the source file.  If there is a problem
    // (e.g. the file is on FAT) we ignore it and proceed with the copy/move.

    if (!(pcs->fFlags & FOF_NOCOPYSECURITYATTRIBS))
    {
        if (SHRestricted(REST_FORCECOPYACLWITHFILE))
        {
            if (GetFileSecurity(pszSource, si, psd, cbPsd, &cbPsd))
            {
                fSecurityObtained = TRUE;
            }
            else
            {
                if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                {
                    psd = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, cbPsd);
                    if (psd)
                    {
                        if (GetFileSecurity(pszSource, si, psd, cbPsd, &cbPsd))
                        {
                            fSecurityObtained = TRUE;
                        }
                    }
                }
            }
        }
    }

TryCopyAgain:

    pcs->fInitialize = TRUE;
    pcs->pfd = pfd;
    SetProgressText(pcs, pszSource, pszDest);

    dwCopyFlags = 0;
    if (pcs->fLostEncryptOk)
    {
        dwCopyFlags |= COPY_FILE_ALLOW_DECRYPTED_DESTINATION;
    }

    if (FO_MOVE == pcs->lpfo->wFunc)
    {
        fCopyOrMoveSucceeded = MoveFileWithProgress(pszSource, pszDest, CopyCallbackProc, pcs, MOVEFILE_COPY_ALLOWED | (fCreateAlways ? MOVEFILE_REPLACE_EXISTING : 0));
    }
    else
    {
        if (!fCreateAlways)
        {
            dwCopyFlags |= COPY_FILE_FAIL_IF_EXISTS;
        }
        fCopyOrMoveSucceeded = CopyFileEx(pszSource, pszDest, CopyCallbackProc, pcs, &pcs->bAbort, dwCopyFlags);
    }

    if (!fCopyOrMoveSucceeded)  
    {
        int iLastError = (int)GetLastError();

        DebugMsg(TF_DEBUGCOPY, TEXT("FileCopy() failed, get last error returned 0x%08x"), iLastError);

        switch (iLastError)
        {
            // Let the caller handle this one
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS: // nt5 221893 CopyFileEx now returns this for some reason...
                iRet = ERROR_FILE_EXISTS;
                goto Exit;

            case ERROR_DISK_FULL:
                if (PathIsUNC(pszDest) || !IsRemovableDrive(DRIVEID(pszDest)) || PathIsSameRoot(pszDest,pszSource))
                {
                    break;
                }

                iLastError = ERROR_DISK_FULL;
                // Fall through

            case ERROR_PATH_NOT_FOUND:
                if (!fRetryPath)
                {
                    // ask the user to stick in another disk or empty wastebasket
                    ULARGE_INTEGER ulFileSize;
                    ulFileSize.LowPart = pfd->nFileSizeLow;
                    ulFileSize.HighPart = pfd->nFileSizeHigh;
                    iLastError = CopyMoveRetry(pcs, pszDest, iLastError, &ulFileSize);
                    if (!iLastError)
                    {
                        fRetryPath = TRUE;
                        goto TryCopyAgain;
                    }
                    CopyError(pcs, pszSource, pszDest, (UINT)iLastError | ERRORONDEST, FO_COPY, OPER_DOFILE);
                    iRet = ERROR_CANCELLED;
                    goto Exit;
                }
                break;

            case ERROR_ENCRYPTION_FAILED:
                if (pfd->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED && FALSE == pcs->fLostEncryptOk)
                {
                    int result;
                    result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs,
                            &pcs->cd, pcs->nSourceFiles,
                            FALSE,
                            CONFIRM_LOST_ENCRYPT_FILE,
                            pszSource, pfd, pszDest, NULL, NULL);

                    switch (result)
                    {
                        case IDYES:
                            pcs->fLostEncryptOk = TRUE;
                            goto TryCopyAgain;

                        case IDNO:
                        case IDCANCEL:
                            pcs->bAbort = TRUE;
                            iRet = result;
                            break;

                        default:
                            iRet = result;
                            break;
                    }
                }
                break;

            case ERROR_ACCESS_DENIED:
                // check if the filename is too long
                if (lstrlen(PathFindFileName(pszSource)) + lstrlen(pszDest) >= MAX_PATH)
                {
                    iLastError = DE_FILENAMETOOLONG;
                }
                else if (!fRetryAttr)
                {
                    // If the file is readonly, reset the readonly attribute
                    // and have another go at it
                    DWORD dwAttributes = GetFileAttributes(pszDest);
                    if (0xFFFFFFFF != dwAttributes)
                    {
                        dwAttributes &= ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
                        if (SetFileAttributes(pszDest, dwAttributes))
                        {
                            fRetryAttr = TRUE;
                            goto TryCopyAgain;
                        }
                    }

                    // GetFileAttributes() 10 lines above clobers GetLastError() and CopyError()
                    // needs it.
                    SetLastError(iLastError);
                }
                break;
        }

        if (!pcs->bAbort)
        {
            CopyError(pcs, pszSource, pszDest, iLastError, FO_COPY, OPER_DOFILE);
        }

        iRet = ERROR_CANCELLED;  // error already reported
        goto Exit;
    }

    // If copying from a CDRom - clear the read-only bit
    if (pcs->fFromCDRom)
    {
        SetFileAttributes(pszDest, pfd->dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    // Set the source's security on the destination, ignoring any error.
    if (fSecurityObtained)
    {
        SetFileSecurity(pszDest, si, psd);
    }


    SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, pszDest, NULL);

    if (FO_MOVE == pcs->lpfo->wFunc)
    {
        // Let windows waiting on notifications of Source know of change.  We have to check
        // to see if the file is actually gone in order to tell if it actually moved or not.

        if (!PathFileExists(pszSource))
            SHChangeNotify(SHCNE_DELETE, SHCNF_PATH, pszSource, NULL);
    }
    else if (0 == StrCmpIC(pfd->cFileName, TEXT("desktop.ini")))
    {
        // clean out stuff from the desktop.ini
        WritePrivateProfileSection(TEXT("DeleteOnCopy"), NULL, pszDest);
    }

    iRet = ERROR_SUCCESS;   // 0

Exit:

    // If we had to alloc a buffer for the security descriptor,
    // free it now.

    if (psd && (rgbSecurityDescriptor != psd))
        LocalFree(psd);

    return iRet;
}

// note: this is a very slow call
DWORD GetFreeClusters(LPCTSTR szPath)
{
    DWORD dwFreeClus;
    DWORD dwTemp;

    if (GetDiskFreeSpace(szPath, &dwTemp, &dwTemp, &dwFreeClus, &dwTemp))
        return dwFreeClus;
    else
        return (DWORD)-1;
}

// note: this is a very slow call
BOOL TotalCapacity(LPCTSTR szPath, ULARGE_INTEGER *puliDiskSize)
{
    int idDrive = PathGetDriveNumber(szPath);
    if (idDrive != -1) 
    {
        TCHAR szDrive[5];
        ULARGE_INTEGER ullDiskFreeForUser;

        PathBuildRoot(szDrive, idDrive);

        return GetDiskFreeSpaceEx(szDrive, &ullDiskFreeForUser, puliDiskSize, NULL);
    }

    return FALSE;
}


typedef struct
{
    LPTSTR pszTitle;
    LPTSTR pszText;
} DISKERRORPARAM;

BOOL_PTR CALLBACK DiskErrDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    switch (uMessage)
    {
        case WM_INITDIALOG:
            {
                DISKERRORPARAM *pDiskError = (DISKERRORPARAM *) lParam;
                if (pDiskError)
                {
                    SetWindowText(hDlg, pDiskError->pszTitle);
                    SetDlgItemText(hDlg, IDC_DISKERR_EXPLAIN, pDiskError->pszText);
                }
                Static_SetIcon(GetDlgItem(hDlg, IDC_DISKERR_STOPICON), 
                        LoadIcon(NULL, IDI_HAND));
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                case IDCANCEL:
                case IDC_DISKERR_LAUNCHCLEANUP:
                    EndDialog (hDlg, LOWORD(wParam));
                    break;

                default:
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


void DisplayFileOperationError(HWND hParent, int idVerb, int wFunc, int nError, LPCTSTR pszReason, LPCTSTR pszPath, LPCTSTR pszDest)
{
    TCHAR szBuffer[80];
    DISKERRORPARAM diskparams;

    // Grab title from resource 
    if (LoadString(HINST_THISDLL, IDS_FILEERROR + wFunc, szBuffer, ARRAYSIZE(szBuffer)))
    {
        diskparams.pszTitle = szBuffer;
    }
    else
    { 
        diskparams.pszTitle = NULL;
    }

    // Build Message to display
    diskparams.pszText = ShellConstructMessageString(HINST_THISDLL, 
            MAKEINTRESOURCE(idVerb), pszReason, PathFindFileName(pszPath));

    if (diskparams.pszText)
    {
        int idDrive = DriveIDFromBBPath(pszDest);
        //if we want to show Disk cleanup do our stuff, otherwise do MessageBox
        if (nError == ERROR_DISK_FULL && 
                IsBitBucketableDrive(idDrive) &&
                !PathIsUNC(pszDest) &&
                GetDiskCleanupPath(NULL, 0))
        {
            if (DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_DISKERR), hParent,
                        DiskErrDlgProc, (LPARAM)&diskparams) == IDC_DISKERR_LAUNCHCLEANUP)
            {
                LaunchDiskCleanup(hParent, idDrive, DISKCLEANUP_NOFLAG);
            }
        }
        else
        {
            MessageBox(hParent, diskparams.pszText, diskparams.pszTitle, 
                    MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
        }
        LocalFree(diskparams.pszText);
    }
}


/***********************************************************************\
DESCRIPTION:
We received an SHARINGVIOLATION or ACCESSDENIED error.  We want
to generate the most accruate error message for the user to inform
them better.  These are the cases we care about:

ERROR_ACCESS_DENIED: This is the legacy case with the message:
"Access is denied. The source file may be in use."
DE_DEST_IS_CDROM:  This is displayed in case the user copies a file to
their cd-rom drive.
DE_DEST_IS_CDRECORD: user deletes from CD recordable drive, we need an error
message that isn't so scary about "cant copy files to CD".
DE_DEST_IS_DVD:  This is displayed in case the user copies a file to
their DVD drive
DE_SHARING_VIOLATION: The file can't be copied because it's open by someone
who doesn't allow others to read the file while they
use it.
DE_PERMISSIONDENIED:  This should be displayed if the user doesn't have
the ACLs (security permissions) to read/copy the file.
\***********************************************************************/
int GenAccessDeniedError(LPCTSTR pszSource, LPCTSTR pszDest, int nError)
{
    int nErrorMsg = ERROR_ACCESS_DENIED;
    int iDrive = PathGetDriveNumber(pszDest);

    if (iDrive != -1)
    {
        if (IsCDRomDrive(iDrive))
        {
            WCHAR szDrive[4];
            // check if user is deleting from cd-r drive.  error message saying "cant copy or move files to cd drive"
            // doesn't apply.  since we're about to put up ui its not like we have to be super fast here, call into cdburning code.
            if (SUCCEEDED(CDBurn_GetRecorderDriveLetter(szDrive, ARRAYSIZE(szDrive))) &&
                    (DRIVEID(szDrive) == iDrive))
            {
                nErrorMsg = DE_DEST_IS_CDRECORD;
            }
            else
            {
                nErrorMsg = DE_DEST_IS_CDROM;
            }
        }

        if (DriveIsDVD(iDrive))
            nErrorMsg = DE_DEST_IS_DVD;
    }

    // TODO: DE_SHARING_VIOLATION, DE_PERMISSIONDENIED
    return nErrorMsg;
}


//
// The following function reports errors for the copy engine
//
// Parameters
//      pszSource       source file name
//      pszDest         destination file name
//      nError          dos (or our exteneded) error code
//                      0xFFFF for special case NET error
//      wFunc           FO_* values
//      nOper           OPER_* values, operation being performed
//

void CopyError(LPCOPY_STATE pcs, LPCTSTR pszSource, LPCTSTR pszDest, int nError, UINT wFunc, int nOper)
{
    TCHAR szReason[200];
    TCHAR szFile[MAX_PATH];
    int idVerb;
    BOOL bDest;
    BOOL fSysError = FALSE;
    DWORD dwError = GetLastError();       // get Extended error now before we blow it away.

    if (!pcs || (pcs->fFlags & FOF_NOERRORUI))
        return;      // caller doesn't want to report errors

    bDest = nError & ERRORONDEST;        // was dest file cause of error
    nError &= ~ERRORONDEST;              // clear the dest bit

    // We also may need to remap some new error codes into old error codes
    //
    if (nError == ERROR_BAD_PATHNAME)
        nError = DE_INVALIDFILES;

    if (nError == ERROR_CANCELLED)        // user abort
        return;

    lstrcpyn(szFile, bDest ? pszDest : pszSource, ARRAYSIZE(szFile));
    if (!szFile[0])
    {
        LoadString(HINST_THISDLL, IDS_FILE, szFile, ARRAYSIZE(szFile));
    }
    else
    {
        // make the path fits on the screen
        RECT rcMonitor;
        HWND hwnd = pcs->hwndProgress ? pcs->hwndProgress : pcs->hwndDlgParent;
        GetMonitorRect(MonitorFromWindow(hwnd, TRUE), &rcMonitor);

        PathCompactPath(NULL, szFile, (rcMonitor.right - rcMonitor.left) / 3);
    }

    // get the verb string
    // since we now recycle folders as well as files, added OPER_ENTERDIR check here
    if ((nOper == OPER_DOFILE) || (nOper == OPER_ENTERDIR) || (nOper == 0))
    {
        if ((nError != -1) && bDest)
        {
            idVerb = IDS_REPLACING;
        }
        else
        {
            idVerb = IDS_VERBS + wFunc;
        }
    }
    else
    {
        idVerb = IDS_ACTIONS + (nOper >> 8);
    }

    // get the reason string
    if (nError == 0xFFFF)
    {
        DWORD dw;
        WNetGetLastError(&dw, szReason, ARRAYSIZE(szReason), NULL, 0);
    }
    else
    {
        // transform some error cases

        if (bDest)
        {
            // This caseing of error codes is error prone.. it would
            // be better to find the explicit ones we wish to map to
            // this one instead of trying to guess all the ones
            // we don't want to map...
            if ((nError == ERROR_DISK_FULL) ||
                    ((nError != ERROR_ACCESS_DENIED) &&
                     (nError != ERROR_NETWORK_ACCESS_DENIED) &&
                     (nError != ERROR_WRITE_PROTECT) &&
                     (nError != ERROR_BAD_NET_NAME) &&
                     (GetFreeClusters(pszDest) == 0L)))
            {
                nError = ERROR_DISK_FULL;
            }
            else if (dwError == ERROR_WRITE_FAULT)
            {
                nError = ERROR_WRITE_FAULT;
            }
            else if (dwError == ERROR_INVALID_NAME)
            {
                nError = ERROR_INVALID_NAME;
            }
        }
        else
        {
            if (nError == ERROR_ACCESS_DENIED)
            {
                // Check the extended error for more info about the error...
                // We just map these errors to something generic that
                // tells the user something weird is going on.
                switch (dwError)
                {
                    case ERROR_CRC:
                    case ERROR_SEEK:
                    case ERROR_SECTOR_NOT_FOUND:
                    case ERROR_READ_FAULT:
                    case ERROR_GEN_FAILURE:
                        nError = ERROR_GEN_FAILURE;
                        break;


                        // We can't test for ERROR_FILE_NOT_FOUND because in the case where we copy to
                        // a write-protected dest we check to see if the reason we got access denied was
                        // because there's already a read-only file there.  If there isn't _that_ test is
                        // going to SetLastError() to ERROR_FILE_NOT_FOUND and that's what we're going to
                        // report as an error. [davepl]
                        // 
                        // case ERROR_FILE_NOT_FOUND:
                        //    nError = ERROR_GEN_FAILURE;
                        //    break;

                    case ERROR_SHARING_VIOLATION:
                    case ERROR_ACCESS_DENIED:
                        nError = GenAccessDeniedError(pszSource, pszDest, nError);
                        break;
                    default:
                        TraceMsg(TF_WARNING, "CopyEngine: hit error %x , not currently special cased", dwError);
                        break;
                }
            }
            else
            {
                // This error occures when a user drags & drops a file from point a to
                // point b twice.  The second time fails because the first time hasn't finished.
                if (nError == (OPER_ERROR | ERROR_FILE_NOT_FOUND))
                {
                    nError = ERROR_GEN_FAILURE;
                }
            }
        }
    }

    // the error munging above is in several places, but there are some errors that we
    // know for SURE the user will never want to see so zap them to generic failures.
    // this whole thing needs a redesign... we shouldnt depend generally on errors getting
    // UI ("There is not enough space on the disk.") because then we get crap like this.
    // but everybody already knows that.
    switch (nError)
    {
        case ERROR_SWAPERROR:             //  Error performing inpage operation.
            nError = ERROR_GEN_FAILURE;
            break;
    }

    if (nError <= DE_ERROR_MAX)
    {
        BOOL fOverridden = FALSE;

        if (nError == ERROR_SHARING_VIOLATION)
        {
            // in the sharing violation case we can try to be a little better in the error UI by
            // going through the running object table and seeing if the file is registered in there.
            // if it's not in there, no biggie, just use our normal handling.
            PWSTR pszApp;
            if (SUCCEEDED(FindAppForFileInUse(bDest ? pszDest : pszSource, &pszApp)))
            {
                PWSTR pszMessage = ShellConstructMessageString(HINST_THISDLL, MAKEINTRESOURCE(IDS_SHAREVIOLATION_HINT), pszApp);
                if (pszMessage)
                {
                    StrCpyN(szReason, pszMessage, ARRAYSIZE(szReason));
                    fOverridden = TRUE;
                    LocalFree(pszMessage);
                }
                LocalFree(pszApp);
            }
        }

        if (!fOverridden)
        {
            fSysError = !LoadString(HINST_THISDLL, IDS_REASONS + nError, szReason, ARRAYSIZE(szReason));
        }
    }

    if (nOper == OPER_DOFILE)
    {
        PathRemoveExtension(szFile);
    }

    if (fSysError)
    {
        SHSysErrorMessageBox(pcs->hwndDlgParent, MAKEINTRESOURCE(IDS_FILEERROR + wFunc),
                idVerb, nError, PathFindFileName(szFile),
                MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
    }
    else
    {
        if (nError > DE_ERROR_MAX &&
                0 == FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    nError,
                    0,
                    szReason,
                    ARRAYSIZE(szReason),
                    NULL))
        {
            szReason[0] = 0;
        }

        DisplayFileOperationError(pcs->hwndDlgParent, idVerb, wFunc, nError, szReason, szFile, pszDest);
    }
}


//
// The following function is used to retry failed move/copy operations
// due to out of disk situations or path not found errors
// on the destination.
//
// parameters:
//      pszDest         Fully qualified path to destination file (ANSI)
//      nError          type of error: ERROR_DISK_FULL or ERROR_PATH_NOT_FOUND
//      dwFileSize      amount of space needed for this file if ERROR_DISK_FULL
//
// returns:
//      0       success (destination path has been created)
//      != 0    dos error code including ERROR_CANCELLED
//

int CopyMoveRetry(COPY_STATE *pcs, LPCTSTR pszDest, int nError, ULARGE_INTEGER* pulFileSize)
{
    UINT wFlags;
    int  result;
    LPCTSTR wID;
    TCHAR szTemp[MAX_PATH];
    BOOL fFirstRetry = TRUE;

    if (pcs->fFlags & FOF_NOERRORUI)
    {
        result = ERROR_CANCELLED;
        goto ErrorExit;
    }

    lstrcpyn(szTemp, pszDest, ARRAYSIZE(szTemp));
    PathRemoveFileSpec(szTemp);

    do
    {
        // until the destination path has been created
        if (nError == ERROR_PATH_NOT_FOUND)
        {
            if (!(pcs->fFlags & FOF_NOCONFIRMMKDIR))
            {
                wID = MAKEINTRESOURCE(IDS_PATHNOTTHERE);
                wFlags = MB_ICONEXCLAMATION | MB_YESNO;
            }
            else
            {
                wID = 0;
            }
        }
        else  // ERROR_DISK_FULL
        {
            ULARGE_INTEGER ulDiskSize;

            wFlags = MB_ICONEXCLAMATION | MB_RETRYCANCEL;
            if (pulFileSize && TotalCapacity(pszDest, &ulDiskSize) && pulFileSize->QuadPart > ulDiskSize.QuadPart)
            {
                wID = MAKEINTRESOURCE(IDS_FILEWONTFIT);
            }
            else
            {
                wID = MAKEINTRESOURCE(IDS_DESTFULL);
            }
        }

        if (wID)
        {
            // szTemp will be ignored if there's no %1%s in the string.
            result = ShellMessageBox(HINST_THISDLL, pcs->hwndDlgParent, wID, MAKEINTRESOURCE(IDS_UNDO_FILEOP + pcs->lpfo->wFunc), wFlags, (LPTSTR)szTemp);
        }
        else
        {
            result = IDYES;
        }

        if (result == IDRETRY || result == IDYES)
        {
            TCHAR szDrive[5];
            int idDrive;

            // Allow the disk to be formatted
            // REVIEW, could this be FO_MOVE as well?
            if (FAILED(SHPathPrepareForWrite(((pcs->fFlags & FOF_NOERRORUI) ? NULL : pcs->hwndDlgParent), NULL, szTemp, SHPPFW_DEFAULT)))
                return ERROR_CANCELLED;

            idDrive = PathGetDriveNumber(szTemp);
            if (idDrive != -1)
                PathBuildRoot(szDrive, idDrive);
            else
                szDrive[0] = 0;

            // if we're not copying to the root
            if (lstrcmpi(szTemp, szDrive))
            {
                result = SHCreateDirectory(pcs->hwndDlgParent, szTemp);

                if (result == ERROR_CANCELLED)
                    goto ErrorExit;
                if (result == ERROR_ALREADY_EXISTS)
                {
                    // if SHPathPrepareForWrite created the directory we shouldn't treat this as an error
                    result = 0;
                }
                else if (result && (nError == ERROR_PATH_NOT_FOUND))
                {
                    result |= ERRORONDEST;

                    //  We try twice to allow the recyclebin to be flushed.
                    if (fFirstRetry)
                        fFirstRetry = FALSE;
                    else
                        goto ErrorExit;
                }
            }
            else
            {
                result = 0;
            }
        }
        else
        {
            result = ERROR_CANCELLED;
            goto ErrorExit;
        }
    } while (result);

ErrorExit:
    return result;            // success
}


BOOL ValidFilenames(LPCTSTR pList)
{
    if (!*pList)
        return FALSE;

    for (; *pList; pList += lstrlen(pList) + 1)
    {
        if (PathIsInvalid(pList))
        {
            return FALSE;
        }
    }

    return TRUE;
}

void AddRenamePairToHDSA(LPCTSTR pszOldPath, LPCTSTR pszNewPath, HDSA* phdsaRenamePairs)
{
    //
    //  Update our collision mapping table
    //
    if (!*phdsaRenamePairs)
        *phdsaRenamePairs = DSA_Create(sizeof(SHNAMEMAPPING), 4);

    if (*phdsaRenamePairs)
    {
        SHNAMEMAPPING rp;
        rp.cchOldPath = lstrlen(pszOldPath);
        rp.cchNewPath = lstrlen(pszNewPath);

        rp.pszOldPath = StrDup(pszOldPath);
        if (rp.pszOldPath)
        {
            rp.pszNewPath = StrDup(pszNewPath);
            if (rp.pszNewPath)
            {
                if (DSA_AppendItem(*phdsaRenamePairs, &rp) == -1)
                {
                    LocalFree(rp.pszOldPath);
                    LocalFree(rp.pszNewPath);
                }
            }
            else
            {
                LocalFree(rp.pszOldPath);
            }
        }
    }
}

BOOL _HandleRename(LPCTSTR pszSource, LPTSTR pszDest, FILEOP_FLAGS fFlags, COPY_STATE * pcs)
{
    TCHAR *pszConflictingName = PathFindFileName(pszSource);
    TCHAR szTemp[MAX_PATH];
    TCHAR szTemplate[MAX_PATH];
    LPTSTR lpszLongPlate;

    PathRemoveFileSpec(pszDest);

    if (LoadString(HINST_THISDLL, IDS_COPYLONGPLATE, szTemplate, ARRAYSIZE(szTemplate)))
    {
        LPTSTR lpsz;
        lpsz = pszConflictingName;
        lpszLongPlate = szTemplate;
        // see if the first part of the template is the same as the name "Copy #"
        while (*lpsz && *lpszLongPlate &&
                *lpsz == *lpszLongPlate &&
                *lpszLongPlate != TEXT('('))
        {
            lpsz++;
            lpszLongPlate++;
        }

        if (*lpsz == TEXT('(') && *lpszLongPlate == TEXT('('))
        {
            // conflicting name already in the template, use it instead
            lpszLongPlate = pszConflictingName;
        }
        else
        {
            // otherwise build our own
            // We need to make sure not to overflow a max buffer.
            int ichFixed = lstrlen(szTemplate) + lstrlen(pszDest) + 5;
            lpszLongPlate = szTemplate;

            if ((ichFixed + lstrlen(pszConflictingName)) <= MAX_PATH)
            {
                lstrcat(lpszLongPlate, pszConflictingName);
            }
            else
            {
                // Need to remove some of the name
                LPTSTR pszExt = StrRChr(pszConflictingName, NULL, TEXT('.'));
                if (pszExt)
                {
                    int ichTemplate = lstrlen(lpszLongPlate);
                    lstrcpyn(lpszLongPlate + ichTemplate,
                            pszConflictingName,
                            max(MAX_PATH - ichFixed - lstrlen(pszExt), 0));

                    // use as much of the buffer as possible
                    StrCatBuff(lpszLongPlate, pszExt, max(MAX_PATH - ichFixed + ichTemplate, 0));
                }
                else
                {
                    lstrcpyn(lpszLongPlate + lstrlen(lpszLongPlate),
                            pszConflictingName,
                            MAX_PATH - ichFixed);
                }
            }
        }
    }
    else
    {
        lpszLongPlate = NULL;
    }

    if (PathYetAnotherMakeUniqueName(szTemp, pszDest, pszConflictingName, lpszLongPlate))
    {
        //
        //  If there are any other files in the queue which are to
        //  be copied into a subtree of pszDest, we must update them
        //  as well.
        //

        //  Put the new (renamed) target in pszDest.
        lstrcpy(pszDest, szTemp);

        //  Rebuild the old dest name and put it in szTemp.
        //  I'm going for minimum stack usage here, so I don't want more
        //  than one MAX_PATH lying around.
        PathRemoveFileSpec(szTemp);
        PathAppend(szTemp, pszConflictingName);

        AddRenamePairToHDSA(szTemp, pszDest, &pcs->dth.hdsaRenamePairs);

        return TRUE;
    }

    return FALSE;
}

// test input for "multiple" filespec
//
// examples:
//      1       foo.bar                 (single non directory file)
//      -1      *.exe                   (wild card on any of the files)
//      n       foo.bar bletch.txt      (number of files)
//

int CountFiles(LPCTSTR pInput)
{
    int count;
    for (count = 0; *pInput; pInput += lstrlen(pInput) + 1, count++)
    {
        // wild cards imply multiple files
        if (PathIsWild(pInput))
            return -1;
    }
    return count;

}

#define ISDIGIT(c)  ((c) >= TEXT('0') && (c) <= TEXT('9'))

BOOL IsCompressedVolume(LPCTSTR pszSource, DWORD dwAttributes)
{
    int i;
    LPTSTR pszFileName, pszExtension;
    TCHAR szPath[MAX_PATH];

    // must be marked system and hidden
    if (!IS_SYSTEM_HIDDEN(dwAttributes))
        return FALSE;

    lstrcpy(szPath, pszSource);
    pszFileName = PathFindFileName(szPath);
    pszExtension = PathFindExtension(pszFileName);

    // make sure the extension is a 3 digit number
    if (!*pszExtension)
        return FALSE;       // no extension

    for (i = 1; i < 4; i++) 
    {
        if (!pszExtension[i] || !ISDIGIT(pszExtension[i]))
            return FALSE;
    }

    // make sure it's null terminated here
    if (pszExtension[4])
        return FALSE;

    // now knock off the extension and make sure the stem matches
    *pszExtension = 0;
    if (lstrcmpi(pszFileName, TEXT("DRVSPACE")) &&
            lstrcmpi(pszFileName, TEXT("DBLSPACE"))) 
    {
        return FALSE;
    }

    // make sure it's in the root
    PathRemoveFileSpec(szPath);
    if (!PathIsRoot(szPath)) 
    {
        return FALSE;
    }

    return TRUE;        // passed all tests!
}

void _DeferMoveDlgItem(HDWP hdwp, HWND hDlg, int nItem, int x, int y)
{
    RECT rc;
    HWND hwnd = GetDlgItem(hDlg, nItem);

    GetClientRect(hwnd, &rc);
    MapWindowPoints(hwnd, hDlg, (LPPOINT) &rc, 2);

    DeferWindowPos(hdwp, hwnd, 0, rc.left + x, rc.top + y, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

void _RecalcWindowHeight(HWND hWnd, LPTSTR lpszText)
{
    HDC hdc = GetDC(hWnd);
    RECT rc;
    HWND hwndText = GetDlgItem(hWnd,IDC_MBC_TEXT);
    HDWP hdwp;
    int iHeightDelta, cx;

    // Get the starting rect of the text area (for the width)
    GetClientRect(hwndText, &rc);
    MapWindowPoints(hwndText, hWnd, (LPPOINT) &rc, 2);

    // Calc how high the static text area needs to be, given the above width
    iHeightDelta = RECTHEIGHT(rc);
    cx = RECTWIDTH(rc);
    DrawText(hdc, lpszText, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_INTERNAL | DT_EDITCONTROL);

    iHeightDelta = RECTHEIGHT(rc) - iHeightDelta;
    cx = RECTWIDTH(rc) - cx; // Should only change for really long words w/o spaces
    if (cx < 0)
        cx = 0;

    ReleaseDC(hWnd, hdc);

    hdwp = BeginDeferWindowPos(4);
    if (hdwp)
    {
        hdwp = DeferWindowPos(hdwp, hwndText, 0, rc.left, rc.top, RECTWIDTH(rc), RECTHEIGHT(rc), SWP_NOZORDER | SWP_NOACTIVATE);
        if (hdwp)
        {
            _DeferMoveDlgItem(hdwp, hWnd, IDC_MESSAGEBOXCHECKEX, 0, iHeightDelta);
            _DeferMoveDlgItem(hdwp, hWnd, IDYES, cx, iHeightDelta);
            _DeferMoveDlgItem(hdwp, hWnd, IDNO, cx, iHeightDelta);

            EndDeferWindowPos(hdwp);
        }
    }

    GetWindowRect(hWnd, &rc);
    SetWindowPos(hWnd, 0, rc.left - (cx/2), rc.top - (iHeightDelta/2), RECTWIDTH(rc)+cx, RECTHEIGHT(rc)+iHeightDelta, SWP_NOZORDER | SWP_NOACTIVATE);
    return;
}

BOOL_PTR CALLBACK RenameMsgBoxCheckDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // we only handle the WM_INITDIALOG so that we can resize the dialog
        // approprately and to set the default button to IDNO
        case WM_INITDIALOG:
            {
                HWND hwndNO = GetDlgItem(hDlg, IDNO);

                _RecalcWindowHeight(hDlg, (LPTSTR)lParam);

                SetDlgItemText(hDlg,IDC_MBC_TEXT,(LPTSTR)lParam);

                SendMessage(hDlg, DM_SETDEFID, IDNO, 0);
                SetFocus(hwndNO);

                return FALSE; // we set the focus, so return false
            }
    }

    // didnt handle this message
    return FALSE;
}

int ConfirmRenameOfConnectedItem(COPY_STATE *pcs, WIN32_FIND_DATA *pfd, LPTSTR szSource)
{
    int result = IDYES; //For non-connected elements, the default is IDYES!
    LPTSTR  pszMessage;
    LPTSTR  lpConnectedItem, lpConnectOrigin;
    LPTSTR  lpStringID;

    //Check if this item being renamed has a connected item.
    if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
    {
        //Yes! It has a connected element! Form the strings to create the confirmation dialog!

        //Get the name of the connected element
        lpConnectedItem = PathFindFileName(pcs->dth.pdtnCurrent->pdtnConnected->szName);
        lpConnectOrigin = PathFindFileName(pcs->dth.pFrom);

        // Mark the connected item as dummy as this will never get renamed.
        // (Note that this connected node could be a folder. It is still OK to mark it as 
        // dummy because for rename operation, a folder is treated just like a file in 
        // DTGotoNextNode()).
        pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;

        if (pfd && (pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            lpStringID = MAKEINTRESOURCE(IDS_HTML_FOLDER_RENAME);
        else
            lpStringID = MAKEINTRESOURCE(IDS_HTML_FILE_RENAME);

        //Load the confirmation message and format it!
        pszMessage = ShellConstructMessageString(HINST_THISDLL, lpStringID, 
                lpConnectedItem, lpConnectOrigin);

        if (pszMessage)
        {
            //Get the confirmation from the end-user;
            result = SHMessageBoxCheckEx(pcs->hwndDlgParent, HINST_THISDLL, 
                    MAKEINTRESOURCE(DLG_RENAME_MESSAGEBOXCHECK), 
                    RenameMsgBoxCheckDlgProc,
                    (void *)pszMessage,
                    IDYES, 
                    REG_VAL_GENERAL_RENAMEHTMLFILE);
            //It is possible we get IDCANCEL if the "X" in the caption is clicked to clost
            // the dialog. The following code makes sure we get one of the return code that we want.
            if ((result != IDYES) && (result != IDNO))
                result = IDNO;

            SHFree(pszMessage);
        }
        else
            result = IDNO;  //For connected elements, the default is "Don't rename";
    }
    else
    {
        if (DTNIsConnected(pcs->dth.pdtnCurrent))
            result = IDNO;  //Connected elements, do not get renamed.
    }

    return result;
}

int AllConfirmations(COPY_STATE *pcs, WIN32_FIND_DATA *pfd, UINT oper, UINT wFunc,
        LPTSTR szSource, LPTSTR szDest, BOOL bTimeToUpdate, 
        WIN32_FIND_DATA *pfdDest, LPINT lpret)
{
    int result = IDYES;
    LPTSTR p;
    LPTSTR pszStatusDest = NULL;
    CONFIRM_FLAG fConfirm;
    WIN32_FIND_DATA *pfdUse1 = NULL;
    WIN32_FIND_DATA *pfdUse2;
    BOOL fSetProgress = FALSE;
    BOOL fShowConfirm = FALSE;

    switch (oper | wFunc)
    {
        case OPER_ENTERDIR | FO_MOVE:
            if (PathIsSameRoot(szSource, szDest))
            {
                fConfirm = CONFIRM_MOVE_FOLDER;
                pfdUse1 = pfd;
                pfdUse2 = pfdDest;
                fShowConfirm = TRUE;
            }
            break;

        case OPER_ENTERDIR | FO_DELETE:
            // Confirm removal of directory on this pass.  The directories
            // are actually removed on the OPER_LEAVEDIR pass
            if (DTNIsRootNode(pcs->dth.pdtnCurrent))
                fSetProgress = TRUE;        

            if (!PathIsRoot(szSource))
            {
                fShowConfirm = TRUE;
                pfdUse2 = pfd;
                fConfirm = CONFIRM_DELETE_FOLDER;
                szDest = NULL;
            }

            break;

        case OPER_DOFILE | FO_RENAME:
            // pszStatusDest = szDest;
            fSetProgress = TRUE;

            p = PathFindFileName(szSource);
            if (!IntlStrEqNI(szSource, szDest, (int)(p - szSource)))
            {
                result = DE_DIFFDIR;
            }
            else
            {
                if (pfd && (pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    fConfirm = CONFIRM_RENAME_FOLDER;
                else
                    fConfirm =  CONFIRM_RENAME_FILE;

                if (PathIsRoot(szSource) || (PathIsRoot(szDest)))
                {
                    result = DE_ROOTDIR | ERRORONDEST;
                }
                else
                {
                    // We need to bring up a special confirmation dialog if this file/folder being
                    // renamed has a connected element (if "foo.htm" or "foo files" is renamed that
                    // will break the links).
                    result = ConfirmRenameOfConnectedItem(pcs, pfd, szSource);

                    if (result != IDNO)
                    {
                        fShowConfirm = TRUE;
                        pfdUse2 = pfdDest;
                        pfdUse1 = pfd;
                    }
                }
            }
            break;

        case OPER_DOFILE | FO_MOVE:

            fSetProgress = TRUE;
            pszStatusDest = szDest;
            if (PathIsRoot(szSource))
            {
                result = DE_ROOTDIR;
            }
            else if (PathIsRoot(szDest))
            {
                result = DE_ROOTDIR | ERRORONDEST;
            }
            else
            {
                fConfirm = CONFIRM_MOVE_FILE;
                fShowConfirm = TRUE;
                pfdUse2 = pfdDest;
                pfdUse1 = pfd;
            }
            break;

        case OPER_DOFILE | FO_DELETE:
            fSetProgress = TRUE;

            if (IsCompressedVolume(szSource, pfd->dwFileAttributes))
            {
                CopyError(pcs, szSource, szDest, DE_COMPRESSEDVOLUME, wFunc, oper);
                result = IDNO;
            }
            else
            {
                fShowConfirm = TRUE;
                szDest = NULL;
                pfdUse2 = pfd;
                fConfirm = CONFIRM_DELETE_FILE;
            }
            break;

    }

    if (fShowConfirm)
    {
        result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs, &pcs->cd, pcs->nSourceFiles, !DTNIsRootNode(pcs->dth.pdtnCurrent), fConfirm,
                szSource, pfdUse1, szDest, pfdUse2, NULL);
    }

    if (oper == OPER_DOFILE || oper == OPER_ENTERDIR)
    {
        if ((wFunc == FO_MOVE) || (wFunc == FO_COPY))
        {
            if ((result != IDNO) && (result != IDCANCEL))
            {   
                LPTSTR pszDataToBeLost;
                WCHAR wszDestDir[MAX_PATH];
                BOOL  bNoStreamLossThisDir = FALSE;

                lstrcpy(wszDestDir, szDest);
                PathRemoveFileSpec(wszDestDir);

                // Files with multiple streams will suffer stream loss on a downlevel
                // copy, but CopyFile special-cases native structure storage.

                pszDataToBeLost = GetDownlevelCopyDataLossText(szSource, wszDestDir, (oper == OPER_ENTERDIR), &bNoStreamLossThisDir);
                if (pszDataToBeLost)
                {
                    fConfirm     = CONFIRM_STREAMLOSS;
                    pfdUse2      = pfd;

                    result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs, &pcs->cd, pcs->nSourceFiles, !DTNIsRootNode(pcs->dth.pdtnCurrent), fConfirm,
                            szSource, pfdUse1, szDest, pfdUse2, pszDataToBeLost);
                    LocalFree(pszDataToBeLost);
                }
                else if (bNoStreamLossThisDir)
                {
                    // pcs->bStreamLossPossible = FALSE;                    
                }
            }
        }   
    }

    // We only really care about OPER_ENTERDIR when deleting and
    // OPER_DOFILE when renaming, but I guess the hook will figure it out

    if ((result == IDYES) &&
            ISDIRFINDDATA(*pfd) &&
            (oper==OPER_ENTERDIR || oper==OPER_DOFILE))
    {

        result = CallFileCopyHooks(pcs->hwndDlgParent, wFunc, pcs->fFlags,
                szSource, pfd->dwFileAttributes,
                szDest, pfdDest->dwFileAttributes);
    }

    if ((result != IDCANCEL) && (result != IDNO) && fSetProgress && bTimeToUpdate)
        SetProgressText(pcs, szSource, pszStatusDest);

    return result;
}


// return TRUE if they're the same file
// assumes that given two file specs, the short name will
// be identical (except case)
BOOL SameFile(LPTSTR pszSource, LPTSTR pszDest)
{
    TCHAR szShortSrc[MAX_PATH];
    if (GetShortPathName(pszSource, szShortSrc, ARRAYSIZE(szShortSrc)))
    {
        TCHAR szShortDest[MAX_PATH];
        if (GetShortPathName(pszDest, szShortDest, ARRAYSIZE(szShortDest)))
            return !lstrcmpi(szShortSrc, szShortDest);
    }

    return FALSE;
}


// make sure we aren't operating on the current dir to avoid
// ERROR_CURRENT_DIRECTORY kinda errors

void AvoidCurrentDirectory(LPCTSTR p)
{
    TCHAR szTemp[MAX_PATH];

    GetCurrentDirectory(ARRAYSIZE(szTemp), szTemp);
    if (lstrcmpi(szTemp, p) == 0)
    {
        DebugMsg(TF_DEBUGCOPY, TEXT("operating on current dir(%s), cd .."), p);
        PathRemoveFileSpec(szTemp);
        SetCurrentDirectory(szTemp);
    }
}

// this resolves short/long name collisions such as moving
// "NewFolde" onto a dir with "New Folder" whose short name is "NEWFOLDE"
//
// we resolve this by renaming "New Folder" to a unique short name (like TMP1)
//
// making a temporary file of name "NEWFOLDE"
//
// renaming TMP1 back to "New Folder"  (at which point it will have a new short
// name like "NEWFOL~1"

// PERF: it'd be faster if we didn't make the temporary file, but that
// would require that we rename the file back to the long name at the
// end of the operation.. which would mean we'd need to queue them all up..
// too much for right now.
BOOL ResolveShortNameCollisions(LPCTSTR lpszDest, WIN32_FIND_DATA *pfd)
{
    BOOL fRet = FALSE;

    // first verify that we're in the name collision.
    // we are if lpszDest is the same as the pfd's short name which is different
    // than it's long name.

    if (!lstrcmpi(PathFindFileName(lpszDest), pfd->cAlternateFileName) &&
            lstrcmpi(pfd->cAlternateFileName, pfd->cFileName))
    {
        // yes... do the renaming
        TCHAR szTemp[MAX_PATH];
        TCHAR szLongName[MAX_PATH];

        lstrcpy(szTemp, lpszDest);
        PathRemoveFileSpec(szTemp);

        // build the original long name
        lstrcpy(szLongName, szTemp);
        PathAppend(szLongName, pfd->cFileName);

        GetTempFileName(szTemp, c_szNULL, 1, szTemp);
        DebugMsg(TF_DEBUGCOPY, TEXT("Got %s as a temp file"), szTemp);
        // rename "New Folder" to "tmp1"
        if (Win32MoveFile(szLongName, szTemp, ISDIRFINDDATA(*pfd)))
        {
            // make a temporary "NewFolde"
            fRet = CreateWriteCloseFile(NULL, lpszDest, NULL, 0);
            ASSERT(fRet);

            // move it back...

            if (!Win32MoveFile(szTemp, szLongName, ISDIRFINDDATA(*pfd)))
            {
                //
                //  Can't move it back, so delete the empty dir and then
                //  move it back.  Return FALSE to denote failure.
                //
                DeleteFile(lpszDest);
                Win32MoveFile(szTemp, szLongName, ISDIRFINDDATA(*pfd));
                fRet = FALSE;
            }
            else
            {
                // send this out because we could have confused views
                // with this swapping files around...  by the time they get the first
                // move file notification, the temp file is likely gone
                // so they could blow that off.. which would mess up the rest of this.
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, szLongName, NULL);
                //
                //  We've now created an empty dir entry of this name type.
                //
                Win32DeleteFile(lpszDest);
            }

            DebugMsg(TF_DEBUGCOPY, TEXT("ResolveShortNameCollision: %s = original, %s = destination,\n %s = temp file, %d = return"), szLongName, lpszDest, szTemp, fRet);
        }
    }
    return fRet;
}


typedef struct { LPTSTR szFilename; int iResult; } RENAMEEXEMPTIONINFO;
RENAMEEXEMPTIONINFO g_rgExemptions[] = {
    { TEXT("thumbs.db"), IDYES }
};
    
// return values.
//
// IDCANCEL = bail out of all operations
// IDNO = skip this one
// IDRETRY = try operation again
// IDUNKNOWN = this (collision) is not the problem
#define IDUNKNOWN IDOK
int CheckForRenameCollision(COPY_STATE *pcs, UINT oper, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfdDest, WIN32_FIND_DATA* pfd)
{
    int iRet = IDUNKNOWN;

    ASSERT((pcs->lpfo->wFunc != FO_DELETE) && (oper != OPER_LEAVEDIR));


    /* Check to see if we are overwriting an existing file or
       directory.  If so, better confirm */

    if ((oper == OPER_DOFILE) ||
            ((oper == OPER_ENTERDIR) && (pcs->fFlags & FOF_RENAMEONCOLLISION)))
    {
        HANDLE  hfindT;

        // REVIEW this slows things down checking for the dest file
        if ((hfindT = FindFirstFile(pszDest, pfdDest)) != INVALID_HANDLE_VALUE)
        {
            FindClose(hfindT);

            iRet = IDCANCEL;

            if (pcs->lpfo->wFunc != FO_RENAME || !SameFile(pszSource, pszDest))
            {

                if (!ResolveShortNameCollisions(pszDest, pfdDest))
                {
                    if (pcs->fFlags & FOF_RENAMEONCOLLISION)
                    {
                        //  The client wants us to generate a new name for the
                        //  source file to avoid a collision at the destination
                        //  dir.  Must also update the current queue and the
                        //  copy root.
                        _HandleRename(pszSource, pszDest, pcs->fFlags, pcs);
                        iRet = IDRETRY;
                    }
                    else
                    {
                        int result = IDRETRY;

                        if (pcs->lpfo->wFunc == FO_RENAME)
                        {
                            return ERROR_ALREADY_EXISTS;
                        }

                        // Is this a super-hidden file we don't want to prompt the
                        // user regarding?
                        if (IS_SYSTEM_HIDDEN(pfd->dwFileAttributes) &&
                            IS_SYSTEM_HIDDEN(pfdDest->dwFileAttributes) && 
                            !ShowSuperHidden())
                        {
                            int cExempt = 0;
                            for (; cExempt < ARRAYSIZE(g_rgExemptions); cExempt++)
                            {
                                if (0 == StrCmpI(g_rgExemptions[cExempt].szFilename, PathFindFileName(pszSource)))
                                {
                                    result = g_rgExemptions[cExempt].iResult;
                                    break;
                                }
                            }
                        }
                        
                        // REVIEW, if the destination file we are copying over
                        // is actually a directory we are doomed.  we can
                        // try to remove the dir but that will fail if there
                        // are files there.  we probably need a special error message
                        // for this case.

                        if (result == IDRETRY)
                        {
                            result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs,
                                    &pcs->cd, pcs->nSourceFiles,
                                    !DTNIsRootNode(pcs->dth.pdtnCurrent),
                                    CONFIRM_REPLACE_FILE,
                                    pszSource, pfd, pszDest, pfdDest, NULL);
                        }
                        switch (result)
                        {
                            case IDYES:

                                if ((pcs->lpfo->wFunc == FO_MOVE) && (PathIsSameRoot(pszSource, pszDest)))
                                {
                                    int ret;
                                    // For FO_MOVE we need to delete the
                                    // destination first.  Do that now.

                                    // FEATURE this replace options should be undable
                                    ret = Win32DeleteFile(pszDest) ? 0 : GetLastError();

                                    if (ret)
                                    {
                                        ret |= ERRORONDEST;
                                        result = ret;
                                    }
                                }
                                if (pcs->lpua)
                                    FOUndo_Release(pcs->lpua);
                                iRet = IDRETRY;
                                break;

                            case IDNO:
                            case IDCANCEL:
                                pcs->lpfo->fAnyOperationsAborted = TRUE;
                                iRet = result;
                                break;

                            default:
                                iRet = result;
                                break;
                        }
                    }
                }
                else
                {
                    iRet = IDRETRY;
                }
            }
        }
    }

    return iRet;
}

int LeaveDir_Delete(COPY_STATE *pcs, LPTSTR pszSource)
{
    int ret;
    if (PathIsRoot(pszSource))
        return 0;

    AvoidCurrentDirectory(pszSource);

    // We already confirmed the delete at MKDIR time, so attempt
    // to delete the directory

    ret = Win32RemoveDirectory(pszSource) ? 0 : GetLastError();
    if (!ret)
    {
        FOUndo_FileReallyDeleted(pszSource);
    }
    return ret;
}


int EnterDir_Copy(COPY_STATE* pcs, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfd, WIN32_FIND_DATA * pfdDest, BOOL fRenameTried)
{
    int ret;
    int result;
    BOOL fSetDestAttributes = FALSE;
    DWORD dwDesiredAttributes = pfd->dwFileAttributes;

    // Whenever we enter a directory, we need to reset the bStreamLossPossible flag,
    // since we could have stepped out from an NTFS->NTFS to NTFS->FAT scenario via
    // a junction point

    pcs->bStreamLossPossible = TRUE;

    // SHMoveFile restricts the based on path length. To be consistent, we make the same
    // restricton on Copy directory also.
    if (IsDirPathTooLongForCreateDir(pszDest))
    {
        ret = ERROR_FILENAME_EXCED_RANGE;
    }
    else
    {
        BOOL fWithoutTemplate = FALSE;

        if (pcs->fLostEncryptOk)
        {
            dwDesiredAttributes &= ~FILE_ATTRIBUTE_ENCRYPTED;     // Pretend its not encrypted
            fSetDestAttributes = TRUE;
            fWithoutTemplate = TRUE;
        }

        if (pfd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            if (!(pcs->fFlags & FOF_NORECURSEREPARSE))
            {
                dwDesiredAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;  // Pretend like its just a folder
                fSetDestAttributes = TRUE;
                fWithoutTemplate = TRUE;
            }
        }

        if (fWithoutTemplate)
        {
            ret = (CreateDirectory(pszDest, NULL) ? 0 : GetLastError());

            // Since we didn't call CreateDirectoryEx, we need to manually
            // propogate the attributes to the dest directory.
            fSetDestAttributes = TRUE;
        }
        else
        {
            ret = (CreateDirectoryEx(pszSource, pszDest, NULL) ? 0 : GetLastError());
        }

        if (ret == ERROR_SUCCESS)
        {
            SHChangeNotify(SHCNE_MKDIR, SHCNF_PATH, pszDest, NULL);
        }
    }

    switch (ret)
    {
        case 0:     // successful folder creation (or it already exists)
            // propogate the attributes (if there are any)

            if (pcs->fFromCDRom)
            {
                // Don't propogate read-only from CDRoms
                dwDesiredAttributes &= ~FILE_ATTRIBUTE_READONLY;
                fSetDestAttributes = TRUE;
            }

            if (fSetDestAttributes)
            {
                // Avoid setting FILE_ATTRIBUTE_DIRECTORY, since its
                // already a directory, and is less error prone.
                SetFileAttributes(pszDest, dwDesiredAttributes);
            }

            //  we should set the security ACLs here on NT
            //  we ignore any kind of failure though, is that OK?
            //
            CopyFileSecurity(pszSource, pszDest);

            // add to the undo atom
            if (pcs->lpua)
            {
                if (DTNIsRootNode(pcs->dth.pdtnCurrent) && !DTNIsConnected(pcs->dth.pdtnCurrent))
                    FOUndo_AddInfo(pcs->lpua, pszSource, pszDest, 0);
            }
            break;

        case ERROR_ALREADY_EXISTS:
        case ERROR_DISK_FULL:
        case ERROR_ACCESS_DENIED:
        case ERROR_INVALID_NAME:
            {
                DWORD dwFileAttributes;

                if (!fRenameTried)
                {
                    int result = CheckForRenameCollision(pcs, OPER_ENTERDIR, pszSource, pszDest, pfdDest, pfd);
                    switch (result)
                    {
                        case IDUNKNOWN:
                            break;

                        case IDRETRY:
                            return EnterDir_Copy(pcs, pszSource, pszDest, pfd, pfdDest, TRUE);

                        case IDCANCEL:
                            pcs->bAbort = TRUE;
                            return result;

                        case IDNO:
                            return result;

                        default:
                            return result;
                    }
                }

                dwFileAttributes = GetFileAttributes(pszDest);

                if (dwFileAttributes == (DWORD)-1)
                {
                    // The dir does not exist, so it looks like a problem
                    // with a read-only drive or disk full

                    if (ret == ERROR_DISK_FULL &&
                            IsRemovableDrive(DRIVEID(pszDest)) &&
                            !PathIsSameRoot(pszDest, pszSource))
                    {
                        ret = CopyMoveRetry(pcs, pszDest, ERROR_DISK_FULL, NULL);
                        if (!ret)
                        {
                            return EnterDir_Copy(pcs, pszSource, pszDest, pfd, pfdDest, fRenameTried);
                        }
                        else
                        {
                            pcs->bAbort = TRUE;
                            return ret;
                        }
                    }

                    // Maybe its an encrypted folder thats losing its encryption?
                    if (pfd->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED)
                    {
                        int result;
                        result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs,
                                &pcs->cd, pcs->nSourceFiles,
                                FALSE,
                                CONFIRM_LOST_ENCRYPT_FOLDER,
                                pszSource, pfd, pszDest, NULL, NULL);

                        switch (result)
                        {
                            case IDYES:
                                pcs->fLostEncryptOk = TRUE;
                                return EnterDir_Copy(pcs, pszSource, pszDest, pfd, pfdDest, fRenameTried);

                            case IDNO:
                            case IDCANCEL:
                                pcs->bAbort = TRUE;
                                ret = result;
                                break;

                            default:
                                ret = result;
                                break;
                        }
                        return ret;
                    }

                    CopyError(pcs, pszSource, pszDest, ERROR_ACCESS_DENIED | ERRORONDEST, FO_COPY, OPER_DOFILE);
                    pcs->bAbort = TRUE;
                    return ret;
                }

                if (!(dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    // A file with this name already exists
                    CopyError(pcs, pszSource, pszDest, DE_FLDDESTISFILE | ERRORONDEST, FO_COPY, OPER_DOFILE);
                    pcs->bAbort = TRUE;
                    return ret;
                }

                result = CachedConfirmFileOp(pcs->hwndDlgParent, pcs, &pcs->cd,
                        pcs->nSourceFiles,
                        !DTNIsRootNode(pcs->dth.pdtnCurrent),
                        CONFIRM_REPLACE_FOLDER,
                        pszSource, pfd, pszDest, pfdDest, NULL);
                switch (result)
                {
                    case IDYES:
                        ret = 0;    // convert to no error
                        pcs->fMerge = TRUE;
                        if (pcs->lpua)
                            FOUndo_Release(pcs->lpua);
                        break;

                    case IDNO:
                        DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder
                        pcs->lpfo->fAnyOperationsAborted = TRUE;
                        ret = IDNO;  // Don't put up error message on this one...
                        // Since the end-user cancelled the copy operation on this folder, we can cancel the 
                        // copy operation on the corresponding connected file too!
                        if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
                            pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;
                        break;

                    case IDCANCEL:
                        pcs->lpfo->fAnyOperationsAborted = TRUE;
                        pcs->bAbort = TRUE;
                        // Since the end-user cancelled the copy operation on this folder, we can cancel the 
                        // copy operation on the corresponding connected file too!
                        if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
                            pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;
                        break;

                    default:
                        result = ret;
                        break;
                }
                break;
            }

        case ERROR_CANCELLED:
            pcs->bAbort = TRUE;
            break;

        case ERROR_FILENAME_EXCED_RANGE:
            DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder
            break;

        default:    // ret != 0 (dos error code)
            ret |= ERRORONDEST;
            break;
    }

    return ret;
}

int EnterDir_Move(COPY_STATE* pcs, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfd, WIN32_FIND_DATA * pfdDest, BOOL fRenameTried)
{
    int ret;

    // Whenever we enter a directory, we need to reset the bStreamLossPossible flag,
    // since we could have stepped out from an NTFS->NTFS to NTFS->FAT scenario via
    // a junction point

    pcs->bStreamLossPossible = TRUE;

    // if these are in the same drive, try using MoveFile on it.
    // if that fails then fail through to the copy

    if (PathIsSameRoot(pszSource, pszDest))
    {
        AvoidCurrentDirectory(pszSource);

        ret = Win32MoveFile(pszSource, pszDest, TRUE) ? 0 : GetLastError();

        switch (ret)
        {
            case 0:

                DebugMsg(TF_DEBUGCOPY, TEXT("Move Folder worked!"));

                DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder

                // add to the undo atom
                if (pcs->lpua && DTNIsRootNode(pcs->dth.pdtnCurrent) && !DTNIsConnected(pcs->dth.pdtnCurrent))
                    FOUndo_AddInfo(pcs->lpua, pszSource, pszDest, 0);

                if (!SHRestricted(REST_NOENCRYPTONMOVE) && 
                        !(pfd->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED))
                {
                    TCHAR szDestDir[MAX_PATH];
                    DWORD dwAttribs;

                    StrCpyN(szDestDir, pszDest, ARRAYSIZE(szDestDir));
                    PathRemoveFileSpec(szDestDir);
                    dwAttribs = GetFileAttributes(szDestDir);

                    if ((dwAttribs != -1) && (dwAttribs & FILE_ATTRIBUTE_ENCRYPTED))
                    {
                        // Encrypt the directory by pretending we are the
                        // property sheet properties->Advanced.  Fill in the fake 
                        // information and call the helper.
                        FILEPROPSHEETPAGE fpsp;
                        FOLDERCONTENTSINFO fci;
                        fci.fIsCompressionAvailable = FALSE; 
                        fci.fMultipleFiles = TRUE;          
                        ZeroMemory(&fpsp, SIZEOF(fpsp));
                        fpsp.hDlg = GetWindow(pcs->hwndDlgParent, GW_CHILD);
                        fpsp.fRecursive = TRUE;
                        fpsp.fIsDirectory = TRUE;
                        fpsp.pfci = &fci;
                        // As long as asInitial.* == asCurrent.* it won't be changed 
                        fpsp.asInitial.fReadOnly = BST_INDETERMINATE;
                        fpsp.asInitial.fHidden   = BST_INDETERMINATE;
                        fpsp.asInitial.fIndex    = BST_INDETERMINATE;
                        fpsp.asInitial.fArchive  = BST_INDETERMINATE;
                        fpsp.asInitial.fCompress = BST_INDETERMINATE;
                        fpsp.asInitial.fEncrypt  = BST_UNCHECKED; // Not encrypted yet
                        fpsp.asInitial.fRecordingEnabled = BST_INDETERMINATE;
                        fpsp.asCurrent.fReadOnly = BST_INDETERMINATE;
                        fpsp.asCurrent.fHidden   = BST_INDETERMINATE;
                        fpsp.asCurrent.fIndex    = BST_INDETERMINATE;
                        fpsp.asCurrent.fArchive  = BST_INDETERMINATE;
                        fpsp.asCurrent.fCompress = BST_INDETERMINATE;
                        fpsp.asCurrent.fEncrypt  = BST_CHECKED;  // Now encrypt
                        fpsp.asCurrent.fRecordingEnabled = BST_INDETERMINATE;
                        ApplyRecursiveFolderAttribs(pszDest, &fpsp);
                    }
                }

                // Win32MoveFile on a single-volume leaves the original ACL
                // intact.  If necessary, pick up perms from the destination.
                if (pcs->fFlags & FOF_NOCOPYSECURITYATTRIBS)
                {
                    ResetFileSecurity(pszDest);
                }
                return 0;

            case ERROR_PATH_NOT_FOUND:
                ret = CopyMoveRetry(pcs, pszDest, ret, NULL);
                if (!ret)
                    return EnterDir_Move(pcs, pszSource, pszDest, pfd, pfdDest, fRenameTried);
                return ret;

            case ERROR_ALREADY_EXISTS:
            case ERROR_FILE_EXISTS:
                if (!fRenameTried)
                {
                    int result = CheckForRenameCollision(pcs, OPER_ENTERDIR, pszSource, pszDest, pfdDest, pfd);
                    switch (result)
                    {
                        case IDUNKNOWN:
                            break;
                        case IDRETRY:
                            return EnterDir_Move(pcs, pszSource, pszDest, pfd, pfdDest, TRUE);

                        case IDCANCEL:
                            pcs->bAbort = TRUE;
                            return result;

                        case IDNO:
                            return result;

                        default:
                            return result;
                    }
                }
                break;

            case ERROR_FILENAME_EXCED_RANGE:
            case ERROR_ONLY_IF_CONNECTED:
                DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder
                return ret;
        }
    }

    // we're going to recurse in.... if we've not enumerated the children for
    // this folder, set it for delayed enumeration now.
    if (!pcs->dth.pdtnCurrent->pdtnChild)
    {
        pcs->dth.pdtnCurrent->pdtnChild = DTN_DELAYED;
    }

    if (DTNIsConnected(pcs->dth.pdtnCurrent) && !PathFileExists(pszSource))
    {
        // This can happen if the end-user moved "foo.htm" AND "foo files" together.
        // As a result the connected element "foo files" has already been moved.
        DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder
        return(0); //No error! This connected element seems to have been moved.
    }

    return EnterDir_Copy(pcs, pszSource, pszDest, pfd, pfdDest, FALSE);
}

int EnterDir_Delete(COPY_STATE * pcs, WIN32_FIND_DATA *pfdSrc, LPTSTR pszSource, HDPA *phdpaDeletedFiles)
{
    int iRet = 0;

    if (!DTNIsRootNode(pcs->dth.pdtnCurrent))
    {
        // we are not at a root node... when doing a delete this can only mean
        // that we are really nuking the folder. we dont need to enum children
        // because we already did a non-lazy enum at the root node.
        return iRet;
    }
    else if (!pcs->lpua)
    {
NukeFolder:
        // we are at a root node and we have no undo atom, this means that we
        // really want to nuke this whole dir, so enum the children
        DTForceEnumChildren(&pcs->dth);
        // do a non-layz enum of the children to prevent the progress
        // bar from going back and forth as we recurse down into any subdirs.
        DTEnumChildren(&pcs->dth, pcs, TRUE, DTF_FILES_AND_FOLDERS);
        return iRet;
    }

    if (DeleteFileBB(pszSource, &iRet, pcs, TRUE, pfdSrc, phdpaDeletedFiles)) 
    {
        DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder
    } 
    else 
    {
        // DeleteFileBB failed, check iRet to find out why

        switch (iRet)
        {
            case BBDELETE_PATH_TOO_LONG:
            case BBDELETE_SIZE_TOO_BIG:
            case BBDELETE_NUKE_OFFLINE:
                {
                    // This is the case where the folder is too big to fit in the Recycle Bin or the folder
                    // is offline. We have no choice but to really nuke it, but we warn the user first since
                    // they may have thought that it was being sent to the recycle bin.
                    int result = CachedConfirmFileOp(pcs->hwndDlgParent, 
                            pcs,
                            &pcs->cd, 
                            pcs->nSourceFiles, 
                            FALSE, 
                            (iRet == BBDELETE_SIZE_TOO_BIG) ?
                            CONFIRM_WONT_RECYCLE_FOLDER :
                            ((iRet == BBDELETE_NUKE_OFFLINE) ?
                             CONFIRM_WONT_RECYCLE_OFFLINE :
                             CONFIRM_PATH_TOO_LONG), 
                            pszSource, 
                            pfdSrc, 
                            NULL, 
                            NULL,
                            NULL);
                    switch (result) 
                    {
                        case IDNO:
                            // user said "please dont really nuke the file"
                            DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder

                            pcs->lpfo->fAnyOperationsAborted = TRUE;

                            iRet = IDNO;  // Don't put up error message for this case

                            //Because the Delete on this FOLDER is aborted, we can cancel the "Delete"
                            // on the corresponding FILE too!
                            if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
                            {
                                pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;
                            }
                            break;

                        case IDCANCEL:
                            // user canceled the operation
                            pcs->lpfo->fAnyOperationsAborted = TRUE;

                            pcs->bAbort = TRUE;

                            //Because the Delete on this FOLDER is cancelled, we can cancel the "Delete"
                            // on the corresponding FILE too!
                            if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
                            {
                                pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;
                            }
                            break;

                        case IDYES:
                        default:
                            // user said "please nuke the file"
                            // assume noerror
                            iRet = 0;

                            // set this so the is correct progress animation is displayed
                            if (pcs)
                            {
                                pcs->fFlags &= ~FOF_ALLOWUNDO;
                            }

                            // dont allow undo since we are really nuking it (cant bring it back...)
                            if (pcs->lpua)
                            {
                                FOUndo_Release(pcs->lpua);
                            }

                            UpdateProgressAnimation(pcs);
                            goto NukeFolder;
                            break;
                    }
                }
                break;

            case BBDELETE_CANNOT_DELETE:
                {
                    // This is the non-deletable file case. Note: this is an NT only case, and
                    // it could be caused by acls or the fact that the file is currently in use.
                    // We attemt to really delete the file (which should fail) so we can generate
                    // the proper error value
                    DWORD dwAttributes = GetFileAttributes(pszSource);

                    if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        iRet = Win32RemoveDirectory(pszSource);
                    }
                    else
                    {
                        iRet = Win32DeleteFile(pszSource);
                    }

                    if (!iRet)
                    {
                        // indeed, the file/folder could not be deleted. 
                        // Get last error to find out why
                        iRet = GetLastError();
                    }
                    else
                    {
                        // DeleteFileBB said that it couldn't be deleted, but we just nuked it. We will
                        // end up falling into this case when we hit things like Mounted Volumes.

                        // As Obi-Wan would say: "You don't need to see his identification... these aren't
                        // the droids you are looking for... He can go about his business... Move along."
                        iRet = ERROR_SUCCESS;
                        DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder

                        // dont allow undo since we reall nuked it (cant bring it back...)
                        if (pcs->lpua)
                        {
                            FOUndo_Release(pcs->lpua);
                        }
                    }
                }
                break;

            case BBDELETE_FORCE_NUKE:
                {
                    // This is the catch-all case. If iRet = BDETETE_FORCE_NUKE, then we just nuke the 
                    // file without warning.

                    // return noerror so we recurse into this dir and nuke it
                    iRet = ERROR_SUCCESS;

                    // set this so the is correct progress animation is displayed
                    if (pcs)
                    {
                        pcs->fFlags &= ~FOF_ALLOWUNDO;
                    }

                    // dont allow undo since we are really nuking it (cant bring it back...)
                    if (pcs->lpua)
                    {
                        FOUndo_Release(pcs->lpua);
                    }

                    UpdateProgressAnimation(pcs);

                    goto NukeFolder;
                }
                break;

            case BBDELETE_CANCELLED:
                {
                    // user canceled the operation
                    pcs->lpfo->fAnyOperationsAborted = TRUE;

                    pcs->bAbort = TRUE;

                    //Because the Delete on this FOLDER is cancelled, we can cancel the "Delete"
                    // on the corresponding FILE too!
                    if (DTNIsConnectOrigin(pcs->dth.pdtnCurrent))
                    {
                        pcs->dth.pdtnCurrent->pdtnConnected->fDummy = TRUE;
                    }
                }

            case BBDELETE_UNKNOWN_ERROR:
            default:
                {
                    iRet = GetLastError();
                    ASSERT(iRet != ERROR_SUCCESS);
                }
                break;

        }
    } // DeleteFileBB

    return iRet;
}

BOOL DoFile_Win32DeleteFileWithPidl(LPCTSTR pszFile, SIMPLEPIDLCACHE *pspc)
{
    LPITEMIDLIST pidlFile = NULL;
    int iRet;
    if (pspc)
    {
        pidlFile = SimplePidlCache_GetFilePidl(pspc, pszFile);
    }
    iRet = Win32DeleteFilePidl(pszFile, pidlFile);
    ILFree(pidlFile);
    return iRet;
}

int DoFile_Delete(COPY_STATE* pcs, WIN32_FIND_DATA *pfdSrc, LPTSTR pszSource, HDPA *phdpaDeletedFiles, BOOL fShouldSuspendEvents)
{
    int iRet = 0;

    // if we dont have an undo atom or this isint a root node or if this is a network file 
    // then we need to really nuke it
    if (!pcs->lpua || !DTNIsRootNode(pcs->dth.pdtnCurrent) || IsNetDrive(PathGetDriveNumber(pszSource)))
    {
        iRet = DoFile_Win32DeleteFileWithPidl(pszSource, fShouldSuspendEvents ? NULL : &pcs->spc) ? 0 : GetLastError();
        if (!iRet)
        {
            FOUndo_FileReallyDeleted(pszSource);
        }
    }
    else if (!DeleteFileBB(pszSource, &iRet, pcs, FALSE, pfdSrc, phdpaDeletedFiles))
    {
        // DeleteFileBB failed, check iRet to find out why

        switch (iRet)
        {
            case BBDELETE_SIZE_TOO_BIG:
            case BBDELETE_NUKE_OFFLINE:
                {
                    // This is the case where the file is too big to fit in the Recycle Bin. We have no
                    // choice but to really nuke it, but we warn the user first since they may have thought
                    // that it was being sent to the recycle bin.
                    int result = CachedConfirmFileOp(pcs->hwndDlgParent, 
                            pcs,
                            &pcs->cd, 
                            pcs->nSourceFiles, 
                            FALSE, 
                            (iRet == BBDELETE_SIZE_TOO_BIG) ?
                            CONFIRM_WONT_RECYCLE_FOLDER :
                            CONFIRM_WONT_RECYCLE_OFFLINE, 
                            pszSource, 
                            pfdSrc, 
                            NULL, 
                            NULL,
                            NULL);

                    switch (result) 
                    {
                        case IDNO:
                            // user said "please dont really nuke the file"
                            pcs->lpfo->fAnyOperationsAborted = TRUE;
                            iRet = IDNO;  // Don't put up error message for this case
                            // WARNING: It is tempting to mark the corresponding connected folder as dummy here.
                            // But, this will not work because currently folders (nodes with children) can not be
                            // marked as dummy.
                            break;

                        case IDCANCEL:
                            // user canceled the operation
                            pcs->lpfo->fAnyOperationsAborted = TRUE;
                            pcs->bAbort = TRUE;
                            // WARNING: It is tempting to mark the corresponding connected folder as dummy here.
                            // But, this will not work because currently folders (nodes with children) can not be
                            // marked as dummy.
                            break;

                        case IDYES:
                        default:
                            // user said "please nuke the file"
                            // set this so the is correct progress animation is displayed
                            if (pcs)
                            {
                                pcs->fFlags &= ~FOF_ALLOWUNDO;
                            }

                            // dont allow undo since we are really nuking it
                            if (pcs->lpua)
                            {
                                FOUndo_Release(pcs->lpua);
                            }

                            UpdateProgressAnimation(pcs);

                            iRet = DoFile_Win32DeleteFileWithPidl(pszSource, &pcs->spc) ? 0 : GetLastError();
                            break;
                    }
                }
                break;

            case BBDELETE_CANNOT_DELETE:
                {
                    // This is the non-deletable file case. Note: this is an NT only case, and
                    // it could be caused by acls or the fact that the file is currently in use.
                    // We attemt to really delete the file (which should fail) so we can generate
                    // the proper error value
                    iRet = Win32DeleteFile(pszSource);

                    if (!iRet)
                    {
                        // indeed, the file/folder could not be deleted.
                        // Get last error to find out why
                        iRet = GetLastError();
                    }
                    else
                    {
                        // DeleteFileBB said that it couldn't be deleted, but we just nuked it. We will
                        // end up falling into this case when we hit things like Mounted Volumes and other
                        // reparse points that we can't "recycle".

                        // As Obi-Wan would say: "You don't need to see his identification... these aren't
                        // the droids you are looking for... He can go about his business... Move along."
                        iRet = ERROR_SUCCESS;
                        DTAbortCurrentNode(&pcs->dth);    // so we don't recurse down this folder

                        // dont allow undo since we really nuked it (cant bring it back...)
                        if (pcs->lpua)
                        {
                            FOUndo_Release(pcs->lpua);
                        }
                    }
                }
                break;

            case BBDELETE_FORCE_NUKE:
                {
                    // This is the catch-all case. If iRet = BDETETE_FORCE_NUKE, then we just nuke the 
                    // file without warning.

                    // set this so the is correct progress animation is displayed
                    if (pcs)
                    {
                        pcs->fFlags &= ~FOF_ALLOWUNDO;
                    }

                    // dont allow undo since we are going to nuke this file
                    if (pcs->lpua)
                    {
                        FOUndo_Release(pcs->lpua);
                    }

                    UpdateProgressAnimation(pcs);

                    iRet = DoFile_Win32DeleteFileWithPidl(pszSource, &pcs->spc) ? 0 : GetLastError();
                }
                break;

            case BBDELETE_CANCELLED:
                {
                    // user canceled the operation
                    pcs->lpfo->fAnyOperationsAborted = TRUE;
                    pcs->bAbort = TRUE;
                }
                break;

            case BBDELETE_UNKNOWN_ERROR:
            default:
                {
                    iRet = GetLastError();
                    ASSERT(iRet != ERROR_SUCCESS);
                }
                break;

        }
    } // !DeleteFileBB

    return iRet;
}

int DoFile_Copy(COPY_STATE* pcs, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfd, WIN32_FIND_DATA * pfdDest, BOOL fRenameTried)
{
    /* Now try to copy the file.  Do extra error processing only
       in 2 cases:
       1) If a removeable drive is full let the user stick in a new disk
       2) If the path doesn't exist (the user typed in
       and explicit path that doesn't exits) ask if
       we should create it for him. */

    int ret = FileCopy(pcs, pszSource, pszDest, pfd, fRenameTried);

    if (ret == ERROR_CANCELLED)
    {
        pcs->bAbort = TRUE;
        return ret;
    }

    if ((ret & ~ERRORONDEST) == ERROR_FILE_EXISTS)
    {
        if (!fRenameTried)
        {
            int result = CheckForRenameCollision(pcs, OPER_DOFILE, pszSource, pszDest, pfdDest, pfd);
            switch (result)
            {
                case IDUNKNOWN:
                    break;

                case IDRETRY:
                    return DoFile_Copy(pcs, pszSource, pszDest, pfd, pfdDest, TRUE);

                case IDCANCEL:
                    pcs->bAbort = TRUE;
                    return result;

                case IDNO:
                    return result;

                default:
                    return result;
            }
        }
    }


    if ((((ret & ~ERRORONDEST) == ERROR_DISK_FULL) &&
                IsRemovableDrive(DRIVEID(pszDest))) ||
            ((ret & ~ERRORONDEST) == ERROR_PATH_NOT_FOUND))
    {
        ULARGE_INTEGER ulFileSize;
        ulFileSize.LowPart = pfd->nFileSizeLow;
        ulFileSize.HighPart = pfd->nFileSizeHigh;
        ret = CopyMoveRetry(pcs, pszDest, ret & ~ERRORONDEST, &ulFileSize);
        if (!ret)
        {
            return DoFile_Copy(pcs, pszSource, pszDest, pfd, pfdDest, fRenameTried);
        }
        else
        {
            pcs->bAbort = TRUE;
            return ret;
        }
    }

    if (!ret)
    {
        // add to the undo atom
        // if we're doing a copy, only keep track of the highest most
        // level.. unless we're doing a merge sort of copy
        if (pcs->lpua)
        {
            if (DTNIsRootNode(pcs->dth.pdtnCurrent) && !DTNIsConnected(pcs->dth.pdtnCurrent))
                FOUndo_AddInfo(pcs->lpua, pszSource, pszDest, 0);
        }

        // if we copied in a new desktop ini, send out an update event for the paretn
        if (!lstrcmpi(PathFindFileName(pszDest), c_szDesktopIni))
        {
            TCHAR szDest[MAX_PATH];
            lstrcpyn(szDest, pszDest, ARRAYSIZE(szDest));
            PathRemoveFileSpec(szDest);
            SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, szDest, NULL);
        }
    }

    return ret;
}

int DoFile_Move(COPY_STATE* pcs, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfd, WIN32_FIND_DATA * pfdDest, BOOL fRenameTried)
{
    int ret = 0;
    if (PathIsRoot(pszSource))
    {
        return DE_ROOTDIR;
    }
    if (PathIsRoot(pszDest))
    {
        return DE_ROOTDIR | ERRORONDEST;
    }

    AvoidCurrentDirectory(pszSource);

    if (PathIsSameRoot(pszSource, pszDest))
    {
TryAgain:
        ret = Win32MoveFile(pszSource, pszDest, ISDIRFINDDATA(*pfd)) ? 0 : GetLastError();

        // try to create the destination if it is not there
        if (ret == ERROR_PATH_NOT_FOUND)
        {
            ret = CopyMoveRetry(pcs, pszDest, ret, NULL);
            if (!ret)
            {
                goto TryAgain;
            }
        }

        if (ret == ERROR_ALREADY_EXISTS)
        {
            if (!fRenameTried)
            {
                int result = CheckForRenameCollision(pcs, OPER_DOFILE, pszSource, pszDest, pfdDest, pfd);
                switch (result)
                {
                    case IDUNKNOWN:
                        break;
                    case IDRETRY:
                        fRenameTried = TRUE;
                        goto TryAgain;

                    case IDCANCEL:
                        pcs->bAbort = TRUE;
                        return result;

                    case IDNO:
                        return result;

                    default:
                        return result;
                }
            }
        }

        if ((ret == ERROR_SUCCESS)                              &&
                !SHRestricted(REST_NOENCRYPTONMOVE)                 &&
                !(pfd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(pfd->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED))
        {
            TCHAR szDestDir[MAX_PATH];
            DWORD dwAttribs;

            // We are moving a file that is NOT encrypted. On Win2k, we need to check to see if this was a move to 
            // an encrypted folder. If so, we automatically encrypt the file.

            lstrcpyn(szDestDir, pszDest, ARRAYSIZE(szDestDir));
            PathRemoveFileSpec(szDestDir);
            dwAttribs = GetFileAttributes(szDestDir);

            if ((dwAttribs != -1) && (dwAttribs & FILE_ATTRIBUTE_ENCRYPTED))
            {
                // sainity check
                ASSERT(dwAttribs & FILE_ATTRIBUTE_DIRECTORY);

                // attempt to encrypt the file
                if (!SHEncryptFile(pszDest, TRUE))
                {
                    int result = CachedConfirmFileOp(pcs->hwndDlgParent, 
                            pcs,
                            &pcs->cd, 
                            pcs->nSourceFiles, 
                            FALSE, 
                            CONFIRM_FAILED_ENCRYPT, 
                            pszDest, 
                            pfd,   // since we just moved it, the attibs should be the same as the src 
                            NULL, 
                            NULL,
                            NULL);
                    switch (result)
                    {
                        case IDCANCEL:
                            // user canceled the operation
                            pcs->lpfo->fAnyOperationsAborted = TRUE;
                            pcs->bAbort = TRUE;
                            break;

                        case IDNO:
                            // user choose to "restore" the file to its original location
                            ret = Win32MoveFile(pszDest, pszSource, ISDIRFINDDATA(*pfd)) ? 0 : GetLastError();

                        case IDYES:
                        default:
                            // user ignored the error
                            break;
                    }
                }
            }
        }

        if (ret == ERROR_SUCCESS)
        {
            if (pcs->lpua && DTNIsRootNode(pcs->dth.pdtnCurrent) && !DTNIsConnected(pcs->dth.pdtnCurrent))
            {
                // add to the undo atom
                FOUndo_AddInfo(pcs->lpua, pszSource, pszDest, 0);
            }

            // Win32MoveFile on a single-volume leaves the original ACL
            // intact.  If necessary, pick up perms from the destination.
            if (pcs->fFlags & FOF_NOCOPYSECURITYATTRIBS)
            {
                ResetFileSecurity(pszDest);
            }

            // if we copied in a new desktop ini, send out an update event for the paretn
            if (!lstrcmpi(PathFindFileName(pszDest), c_szDesktopIni))
            {
                TCHAR szDest[MAX_PATH];
                lstrcpyn(szDest, pszDest, ARRAYSIZE(szDest));
                PathRemoveFileSpec(szDest);
                SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, szDest, NULL);
            }
        }
    }
    else
    {
        // we must force all copies to go through
        // straight so we can remove the source
        if (DTNIsConnected(pcs->dth.pdtnCurrent) && !PathFileExists(pszSource))
        {
            //This can happen if "foo.htm" and "foo files" were moved by the end-user.
            // The connected file had already been moved and hence this is not an error!
            ret = 0; //No error! That file has been moved already!
        }
        else
        {
            ret = DoFile_Copy(pcs, pszSource, pszDest, pfd, pfdDest, FALSE);
        }
    }

    return ret;
}

int DoFile_Rename(COPY_STATE* pcs, LPTSTR pszSource, LPTSTR pszDest,
        WIN32_FIND_DATA *pfd, WIN32_FIND_DATA * pfdDest, BOOL fRenameTried)
{
    LPTSTR p = PathFindFileName(pszSource);

    /* Get raw source and dest paths.  Check to make sure the
       paths are the same */
    int ret = !IntlStrEqNI(pszSource, pszDest, (int)(p - pszSource));
    if (ret)
    {
        return DE_DIFFDIR;
    }

    return DoFile_Move(pcs, pszSource, pszDest, pfd, pfdDest, fRenameTried);
}


int MoveCopyInitPCS(COPY_STATE * pcs)
{
    BOOL fMultiDest = FALSE;
    int ret = 0;
    LPTSTR p = NULL;
    TCHAR szDestPath[MAX_PATH];

    pcs->nSourceFiles = CountFiles(pcs->lpfo->pFrom);      // multiple source files?

    pcs->fProgressOk = TRUE;

    // skip destination processing if we are deleting files
    if (pcs->lpfo->wFunc != FO_DELETE)
    {
        lstrcpyn(szDestPath, pcs->lpfo->pTo, ARRAYSIZE(szDestPath));
        if (!szDestPath[0])           // NULL dest is same as "."
        {
            szDestPath[0] = TEXT('.');
            szDestPath[1] = 0;
        }

        if (PathIsInvalid(szDestPath))
        {
            CopyError(pcs, c_szNULL, c_szNULL, DE_INVALIDFILES | ERRORONDEST, pcs->lpfo->wFunc, 0);
            return ERROR_ACCESS_DENIED;
        }

        if (pcs->lpfo->wFunc == FO_RENAME)
        {
            // don't let them rename multiple files to one single file

            if ((pcs->nSourceFiles != 1) && !PathIsWild(szDestPath))
            {
                CopyError(pcs, c_szNULL, c_szNULL, DE_MANYSRC1DEST, pcs->lpfo->wFunc, 0);
                return DE_MANYSRC1DEST;
            }
            fMultiDest = TRUE;
        }
        else    // FO_COPY or FO_MOVE at this point
        {
            fMultiDest = ((pcs->fFlags & FOF_MULTIDESTFILES) &&
                    (pcs->nSourceFiles == CountFiles(pcs->lpfo->pTo)));

            if (!fMultiDest)
            {
                // for backwards compat.
                // copy c:\foo.bar c:\folder\foo.bar  means
                // multi dest if foo.bar doesn't exist.
                // Hack if it is a root we special case this for the offline 
                // floppy case...
                if (pcs->nSourceFiles == 1 && !PathIsRoot(szDestPath) && 
                        !PathIsDirectory(szDestPath))
                {
                    fMultiDest = TRUE;
                }
            }

        }
    }

    pcs->dth.fMultiDest = fMultiDest;

    return 0;
}


DWORD   g_dwStopWatchMode = 0xffffffff;  // Shell performance mode

// actually this does move/copy/rename/delete
int MoveCopyDriver(COPY_STATE *pcs)
{
    int ret;
    WIN32_FIND_DATA fdSrc;
    WIN32_FIND_DATA fdDest;
    HDPA hdpaDeletedFiles = NULL;
    LPSHFILEOPSTRUCT lpfo = pcs->lpfo;
    TCHAR szText[28];
    BOOL bInitialAllowUndo = FALSE;
    DWORD dwLastUpdateTime = 0;
    BOOL fShouldSuspendEvents = FALSE;
    HANDLE hEventRunning;

    if (g_dwStopWatchMode)
    {
        if (g_dwStopWatchMode == 0xffffffff)
        {
            g_dwStopWatchMode = StopWatchMode();    // Since the stopwatch funcs live in shdocvw, delay this call so we don't load shdocvw until we need to
        }

        if (g_dwStopWatchMode)
        {
            lstrcpy((LPTSTR)szText, TEXT("Shell "));
            switch (lpfo->wFunc)
            {
                case FO_COPY:
                    lstrcat((LPTSTR)szText, TEXT("Copy  "));
                    break;
                case FO_MOVE:
                    lstrcat((LPTSTR)szText, TEXT("Move  "));
                    break;
                case FO_DELETE:
                    lstrcat((LPTSTR)szText, TEXT("Delete"));
                    break;
                case FO_RENAME:
                    lstrcat((LPTSTR)szText, TEXT("Rename"));
                    break;
                default:
                    lstrcat((LPTSTR)szText, TEXT("Copy? "));
                    break;
            }
            lstrcat((LPTSTR)szText, TEXT(": Start"));
            StopWatch_Start(SWID_COPY, (LPCTSTR)szText, SPMODE_SHELL | SPMODE_DEBUGOUT);
        }
    }

    // start by assuming an error.  Non-zero means an error has occured.  If we don't
    // start with this assumption then we will return success if MoveCopyInitPCS fails.
    ret = ERROR_GEN_FAILURE;

    if (!ValidFilenames(lpfo->pFrom))
    {
        CopyError(pcs, c_szNULL, c_szNULL, DE_INVALIDFILES, lpfo->wFunc, 0);
        return ERROR_ACCESS_DENIED;
    }

    StartCopyEngine(&hEventRunning);

    // Check the pcs destination directory to make sure it is valid for the given source file list
    if (MoveCopyInitPCS(pcs))
    {
        goto ExitLoop;          // Destination is invalid so we bail out
    }

    // Build a tree where each node is a source file, a dest file, and an operation to perform
    ret = DTBuild(pcs);
    if (ret)
    {
        goto ShowMessageBox;
    }

    // Speed optimization: for a delete, sending all FSNotifies really bogs down the system,
    // so we skip it and rely on the file system notifies.
    if ((lpfo->wFunc == FO_DELETE || lpfo->wFunc == FO_MOVE) && pcs->dth.dtAll.dwFiles > 100)
    {
        // Only suspend notifies for local moves
        if (lpfo->wFunc == FO_MOVE)
        {
            if (lpfo->pTo)
            {
                int idDrive = PathGetDriveNumber(lpfo->pFrom);
                if (idDrive == PathGetDriveNumber(lpfo->pTo) && !IsNetDrive(idDrive))
                {
                    fShouldSuspendEvents = TRUE;
                }
            }
        }
        else
        {
            fShouldSuspendEvents = TRUE;
        }
    }

    if (fShouldSuspendEvents)
    {
        // SuspendSHNotify can fail if another thread is using it. Only one thread at a time can suspend notify.
        fShouldSuspendEvents = SuspendSHNotify();
    }


    // save off the initial state of the allowundo flag
    if (pcs->fFlags & FOF_ALLOWUNDO)
    {
        bInitialAllowUndo = TRUE;
    }

    // When first starting, we assume that stream loss is possible until we prove
    // otherwise for the current directory.  This gets reset to true each time we
    // enter a new dir via EnterDir_Move or EnterDir_Copy

    pcs->bStreamLossPossible = TRUE;

    for (;;)
    {
        BOOL bUpdateAnimation = FALSE;
        int result;
        DWORD dwTickCount;
        BOOL bTimeToUpdate = FALSE;

        pcs->dth.oper = DTGoToNextNode(&pcs->dth,pcs);

        dwTickCount = GetTickCount();
        if ((dwTickCount - dwLastUpdateTime) > 10)
        {
            dwLastUpdateTime = dwTickCount;
            bTimeToUpdate = TRUE;
        }

        if ((pcs->dth.oper & OPER_MASK) == OPER_ERROR)
        {
            CopyError(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, LOBYTE(pcs->dth.oper), pcs->lpfo->wFunc, OPER_DOFILE);
            // If the directory is copied but a file inside that directory could not 
            // be copied because of long filename, check to see if this is
            // a connected element. If so, invoke undo, sothat we getback the orginal html file 
            // in the same place as the associated folder.
            if ((pcs->dth.oper == (OPER_ERROR | DE_INVALIDFILES)) &&
                    (DTNIsConnected(pcs->dth.pdtnCurrent)))
            {
                if (pcs->lpua)
                {
                    pcs->lpua->foFlags |= FOF_NOCONFIRMATION;
                    FOUndo_Invoke(pcs->lpua);
                    pcs->lpua = NULL;
                }
            }
            break;
        }

        if (!pcs->dth.oper || pcs->bAbort)     // all done?
        {
            break;
        }

        if (DTNIsRootNode(pcs->dth.pdtnCurrent) && (pcs->dth.oper != OPER_LEAVEDIR))
        {
            int iDrive;

            // check to see if we switched between doing a move to
            // recycle bin and a true delete (this would happen when
            // there was an object that was too big for the recycle bin)
            if (!(pcs->fFlags & FOF_ALLOWUNDO) && bInitialAllowUndo)
            {
                // reset the allowundo flag since we have a new root node, and we
                // want to attempt to send it to the recycle bin
                pcs->fFlags |= FOF_ALLOWUNDO;

                // we delay to update the progress animation till we are basically
                // done, which allows us to keep the progress and animation in sync
                bUpdateAnimation = TRUE;
            }

            pcs->fMerge = FALSE;
            pcs->fFromCDRom = FALSE;

            // Check source for being a CDRom
            iDrive = PathGetDriveNumber(pcs->dth.szSrcPath);
            if (-1 != iDrive)
            {
                TCHAR szDrive[4];
                if (DRIVE_CDROM == GetDriveType(PathBuildRoot(szDrive, iDrive)))
                {
                    pcs->fFromCDRom = TRUE;
                }
            }
        }

        DTGetWin32FindData(pcs->dth.pdtnCurrent, &fdSrc);
        fdDest.dwFileAttributes = 0;

        DebugMsg(TF_DEBUGCOPY, TEXT("MoveCopyDriver(): Oper %x From(%s) To(%s)"), pcs->dth.oper, (LPCTSTR)pcs->dth.szSrcPath, (LPCTSTR)pcs->dth.szDestPath);

        // some operation that may effect the destination (have a collision)
        if ((pcs->lpfo->wFunc != FO_DELETE) && (pcs->dth.oper != OPER_LEAVEDIR))
        {
            // this compare needs to be case sensitive, and locale insensitive
            if (!StrCmpC(pcs->dth.szSrcPath, pcs->dth.szDestPath) &&
                    !(pcs->fFlags & FOF_RENAMEONCOLLISION))
            {
                // Source and dest are the same file, and name collision
                // resolution is not turned on, so we just return an error.

                // TODO: Show the error dialog here and allow for SKIP

                ret = DE_SAMEFILE;
                goto ShowMessageBox;
            }
        }

        result = AllConfirmations(pcs, &fdSrc, pcs->dth.oper, pcs->lpfo->wFunc, pcs->dth.szSrcPath, 
                pcs->dth.szDestPath, bTimeToUpdate, &fdDest, &ret);
        switch (result)
        {
            case IDNO:
                DTAbortCurrentNode(&pcs->dth);
                lpfo->fAnyOperationsAborted = TRUE;
                continue;

            case IDCANCEL:
                pcs->bAbort = TRUE;
                goto ExitLoop;

            case IDYES:
                break;

            default:
                ret = result;
                goto ShowMessageBox;
        }

        /* Now determine which operation to perform */

        switch (pcs->dth.oper | pcs->lpfo->wFunc)
        {
            // Note that ENTERDIR is not done for a root, even though LEAVEDIR is
            case OPER_ENTERDIR | FO_MOVE:  // Create dest, verify source delete
                ret = EnterDir_Move(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, &fdSrc, &fdDest, FALSE);
                break;

            case OPER_ENTERDIR | FO_COPY:  // Create destination directory
                ret = EnterDir_Copy(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, &fdSrc, &fdDest, FALSE);
                break;

            case OPER_LEAVEDIR | FO_MOVE:
            case OPER_LEAVEDIR | FO_DELETE:
                ret = LeaveDir_Delete(pcs, pcs->dth.szSrcPath);
                break;

            case OPER_LEAVEDIR | FO_COPY:
                break;

            case OPER_DOFILE | FO_COPY:
                ret = DoFile_Copy(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, &fdSrc, &fdDest, FALSE);
                break;

            case OPER_DOFILE | FO_RENAME:
                ret = DoFile_Rename(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, &fdSrc, &fdDest, FALSE);
                break;

            case OPER_DOFILE | FO_MOVE:
                ret = DoFile_Move(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, &fdSrc, &fdDest, FALSE);
                break;

            case OPER_ENTERDIR | FO_DELETE:
                ret = EnterDir_Delete(pcs, &fdSrc, pcs->dth.szSrcPath, &hdpaDeletedFiles);
                break;

            case OPER_DOFILE | FO_DELETE:
                ret = DoFile_Delete(pcs, &fdSrc, pcs->dth.szSrcPath, &hdpaDeletedFiles, fShouldSuspendEvents);
                break;

            default:
                DebugMsg(DM_ERROR, TEXT("Invalid file operation"));
                ret = 0;         // internal error
                break;
        } // switch (pcs->dth.oper | pcs->lpfo->wFunc)

        if (pcs->bAbort)
            break;

        if (ret == IDNO)
        {
            pcs->lpfo->fAnyOperationsAborted = TRUE;
        }
        else if (ret)
        {      // any errors?
ShowMessageBox:
            // If source file is a connected item and is not found, that means that
            // we have already moved/deleted/renamed it. So, don't report that as error!
            if ((!pcs->dth.pdtnCurrent) || (!pcs->dth.pdtnCurrent->fConnectedElement) || 
                    ((ret != ERROR_FILE_NOT_FOUND) && (ret != ERROR_PATH_NOT_FOUND)))
            {
                CopyError(pcs, pcs->dth.szSrcPath, pcs->dth.szDestPath, ret, pcs->lpfo->wFunc, pcs->dth.oper);

                // If the directory is copied but a file inside that directory could not 
                // be copied because of long filename, check to see if this is
                // a connected element. If so, invoke undo, sothat we getback the orginal html file 
                // in the same place as the associated folder.
                if ((ret == ERROR_FILENAME_EXCED_RANGE) &&
                        (DTNIsConnected(pcs->dth.pdtnCurrent)))
                {
                    if (pcs->lpua)
                    {
                        pcs->lpua->foFlags |= FOF_NOCONFIRMATION;
                        FOUndo_Invoke(pcs->lpua);
                        pcs->lpua = NULL;
                    }
                }
                break;
            }
        }

        if (bTimeToUpdate)
        {
            // perform the delayed update of the dialog
            if (bUpdateAnimation)
            {
                UpdateProgressAnimation(pcs);
                bUpdateAnimation = FALSE;
            }
            // We check to see if we are finished here (instead of at the
            // start) since we want to keep the progress a step behind what
            // we are doing to ensure we have the correct progress animation
            // and text (since FOQueryAbort updates the progress text)
            if (FOQueryAbort(pcs))
                break;
        }
    }

ExitLoop:

    // this happens in error cases where we broke out of the pcr loop
    // without hitting the end

    lpfo->hNameMappings = pcs->dth.hdsaRenamePairs;

    DTCleanup(&pcs->dth);
    BBFinishDelete(hdpaDeletedFiles);

    if (fShouldSuspendEvents)
    {
        ResumeSHNotify();

        if (lpfo->wFunc == FO_DELETE)
        {
            TCHAR szBBPath[MAX_PATH];

            // Since we probably blew away any chance at having the FSNotify work, make sure
            // we update item for this path...  we can send the message on any drive since
            // the bitbucket listens for changes on all drives.
            DriveIDToBBPath(DriveIDFromBBPath(lpfo->pFrom), szBBPath);
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, szBBPath, NULL);
        }
    }


    if (g_dwStopWatchMode)
    {
        lstrcpy((LPTSTR)&szText[12], TEXT(": Stop "));
        StopWatch_Stop(SWID_COPY, (LPCTSTR)szText, SPMODE_SHELL | SPMODE_DEBUGOUT);
    }

    EndCopyEngine(hEventRunning);
    
    return ret;
}



void SetWindowTextFromRes(HWND hwnd, int id)
{
    TCHAR szTemp[80];

    LoadString(HINST_THISDLL, id, szTemp, ARRAYSIZE(szTemp));
    SetWindowText(hwnd, szTemp);
}

int CountProgressPoints(COPY_STATE *pcs, PDIRTOTALS pdt)
{
    // point value for each item
    int iTotal = 0;
    UINT uSize = pcs->uSize;

    if (!uSize)
    {
        uSize = 32*1024;
    }
    // now add it up.
    iTotal += (UINT)((pdt->liSize.QuadPart/uSize) * pcs->dth.iSizePoints);
    iTotal += pdt->dwFiles * pcs->dth.iFilePoints;
    iTotal += pdt->dwFolders * pcs->dth.iFolderPoints;

    return iTotal;
}

void UpdateProgressDialog(COPY_STATE* pcs)
{
    int iRange;  // from 0 to iRange
    int iPos;  // how much is done.

    if (pcs->fProgressOk)
    {

        if (pcs->dth.dtAll.fChanged)
        {
            pcs->dth.dtAll.fChanged = FALSE;
            iRange = CountProgressPoints(pcs, &pcs->dth.dtAll);
            SendProgressMessage(pcs, PBM_SETRANGE32, 0, iRange);
            DebugMsg(TF_DEBUGCOPY, TEXT("UpdateProgressDialog iRange = %d "), iRange);
        }

        if (pcs->dth.dtDone.fChanged)
        {
            pcs->dth.dtDone.fChanged = FALSE;
            iPos = CountProgressPoints(pcs, &pcs->dth.dtDone);
            SendProgressMessage(pcs, PBM_SETPOS, iPos, 0);
            DebugMsg(TF_DEBUGCOPY, TEXT("UpdateProgressDialog iPos = %d "), iPos);
        }
    }
}

// NOTE: !! do NOT refrence pcs->lpfo anywhere in this dialog proc !! 
//
// It can be freed while we are still running. If you need to get information from it, 
// add a new member to the FOUITHREADINFO struct and copy the value from the pcs->lpfo
// into the member (while holding the critsec) right before we create this dlg.
BOOL_PTR CALLBACK FOFProgressDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    FOUITHREADINFO* pfouiti = (FOUITHREADINFO*)GetWindowLongPtr(hDlg, DWLP_USER);
    COPY_STATE *pcs = (pfouiti ? pfouiti->pcs : NULL);

    if (WM_INITDIALOG == wMsg)
    {
        SetWindowLongPtr(hDlg, DWLP_USER, lParam);

        pfouiti = (FOUITHREADINFO*)lParam;
        pcs = pfouiti->pcs;

        SetWindowTextFromRes(hDlg, IDS_ACTIONTITLE + pfouiti->wFunc);

        if (pcs->fFlags & FOF_SIMPLEPROGRESS)
        {
            TCHAR szFrom[MAX_PATH];
            if (pcs->lpszProgressTitle)
            {
                if (IS_INTRESOURCE(pcs->lpszProgressTitle))
                {
                    LoadString(HINST_THISDLL, PtrToUlong(pcs->lpszProgressTitle), szFrom, ARRAYSIZE(szFrom));
                    pcs->lpszProgressTitle = szFrom;
                }
                SetDlgItemText(hDlg, IDD_NAME, pcs->lpszProgressTitle);
                // null it so we only set it once
                pcs->lpszProgressTitle = NULL;
            }
        }

        return FALSE;
    }

    if (pcs)
    {
        switch (wMsg)
        {
            case WM_TIMER:
                if (IsWindowEnabled(hDlg))
                    SetProgressTime(pcs);
                break;

            case WM_SHOWWINDOW:
                if (wParam)
                {
                    int idAni;
                    HWND hwndAnimation;

                    ASSERT(pfouiti->wFunc >= FO_MOVE && pfouiti->wFunc <= FO_DELETE);
                    ASSERT(FO_COPY==FO_MOVE+1);
                    ASSERT(FO_DELETE==FO_COPY+1);
                    ASSERT(IDA_FILECOPY==IDA_FILEMOVE+1);
                    ASSERT(IDA_FILEDEL ==IDA_FILECOPY+1);

                    switch (pfouiti->wFunc)
                    {
                        case FO_DELETE:
                            if (pfouiti->bIsEmptyRBOp)
                            {
                                idAni = IDA_FILENUKE;
                                break;
                            }
                            else if (!(pcs->fFlags & FOF_ALLOWUNDO))
                            {
                                idAni = IDA_FILEDELREAL;
                                break;
                            }
                            // else fall through

                        default:
                            idAni = (IDA_FILEMOVE + (int)pfouiti->wFunc - FO_MOVE);
                    }

                    hwndAnimation = GetDlgItem(hDlg,IDD_ANIMATE);

                    Animate_Open(hwndAnimation, IntToPtr(idAni));

                    SetProp(hwndAnimation, TEXT("AnimationID"), IntToPtr(idAni));

                    // a timer every MS_TIMESLICE seconds to update the progress time estimate
                    SetTimer(hDlg, 1, MS_TIMESLICE, NULL);
                }
                break;

            case WM_ENABLE:
                if (wParam)
                {
                    if (pcs->dwPreviousTime)
                    {
                        // if we're enabling it, set the previous time to now
                        // because no action has happened while we were disabled
                        pcs->dwPreviousTime = GetTickCount();
                    }
                }
                else
                {
                    SetProgressTime(pcs);
                }
                PauseAnimation(pcs, wParam == 0);
                break;

            case WM_COMMAND:
                switch (GET_WM_COMMAND_ID(wParam, lParam))
                {
                    case IDCANCEL:
                        pcs->bAbort = TRUE;
                        ShowWindow(hDlg, SW_HIDE);
                        break;
                }
                break;

            case PDM_SHUTDOWN:
                // Make sure this window is shown before telling the user there
                // is a problem
                // ignore FOF_NOERRORUI here because of the nature of the situation
                ShellMessageBox(HINST_THISDLL, hDlg, MAKEINTRESOURCE(IDS_CANTSHUTDOWN),
                        NULL, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                break;

            case PDM_NOOP:
                // a dummy id that we can take so that folks can post to us and make
                // us go through the main loop
                break;

            case PDM_UPDATE:
                pcs->dth.fChangePosted = FALSE;
                UpdateProgressDialog(pcs);
                break;

            case WM_QUERYENDSESSION:
                // Post a message telling the dialog to show the "We can't shutdown now"
                // dialog and return to USER right away, so we don't have to worry about
                // the user not clicking the OK button before USER puts up its "this
                // app didn't respond" dialog
                PostMessage(hDlg, PDM_SHUTDOWN, 0, 0);

                // Make sure the dialog box procedure returns FALSE
                SetWindowLongPtr(hDlg, DWLP_MSGRESULT, FALSE);
                return TRUE;

            default:
                return FALSE;
        }
    }
    return TRUE;
}

int CALLBACK FOUndo_FileReallyDeletedCallback(UNDOATOM *lpua, LPARAM lParam)
{
    LPTSTR * ppsz = (LPTSTR*)lParam;
    // this is our signal to nuke the rest
    if (!*ppsz)
        return EUA_DELETE;

    switch (lpua->uType) 
    {
        case IDS_RENAME:
        case IDS_COPY:
        case IDS_MOVE:
        case IDS_DELETE: 
            {
                LPFOUNDODATA lpud = (LPFOUNDODATA)lpua->lpData;
                HDPA hdpa = lpud->hdpa;
                // only the destinations matter.
                int i, iMax = DPA_GetPtrCount(hdpa);
                for (i = 1; i <= iMax; i += 2) 
                {
                    LPTSTR lpsz = DPA_GetPtr(hdpa, i);
                    if (lstrcmpi(lpsz, *ppsz) == 0) 
                    {
                        *ppsz = NULL;
                        break;
                    }
                }
            }
            break;
    }

    // this is our signal to nuke the rest
    if (!*ppsz)
        return EUA_DELETE;
    else
        return EUA_DONOTHING;
}

// someone really really deleted a file.  make sure we no longer have
// any undo information pointing to it.
void FOUndo_FileReallyDeleted(LPTSTR lpszFile)
{
    EnumUndoAtoms(FOUndo_FileReallyDeletedCallback, (LPARAM)&lpszFile);
}


int CALLBACK FOUndo_FileRestoredCallback(UNDOATOM *lpua, LPARAM lParam)
{
    LPTSTR psz = (LPTSTR)lParam;

    switch (lpua->uType) 
    {
        case IDS_DELETE: 
            {
                LPFOUNDODATA lpud = (LPFOUNDODATA)lpua->lpData;
                HDPA hdpa = lpud->hdpa;
                LPTSTR lpsz;
                int i, iMax;

                ASSERT(hdpa);
                // only the destinations matter.
                iMax = DPA_GetPtrCount(hdpa);
                for (i = 1; i <= iMax; i += 2) 
                {
                    lpsz = DPA_GetPtr(hdpa, i);
                    if (lstrcmpi(lpsz, psz) == 0) 
                    {
                        ENTERCRITICAL;

                        Str_SetPtr(&lpsz, NULL);
                        lpsz = DPA_GetPtr(hdpa, i - 1);
                        Str_SetPtr(&lpsz, NULL);
                        DPA_DeletePtr(hdpa, i);
                        DPA_DeletePtr(hdpa, i - 1);

                        LEAVECRITICAL;

                        if (DPA_GetPtrCount(hdpa))
                            return EUA_ABORT;
                        else
                            return EUA_DELETEABORT;
                    }
                }
            }
            break;
    }
    return EUA_DONOTHING;
}

// this means someone restored a file (via ui in the bitbucket)
// so we need to clean up the undo info.
void FOUndo_FileRestored(LPCTSTR lpszFile)
{
    EnumUndoAtoms(FOUndo_FileRestoredCallback, (LPARAM)lpszFile);
}


void FOUndo_AddInfo(UNDOATOM *lpua, LPTSTR lpszSrc, LPTSTR lpszDest, DWORD dwAttributes)
{
    HDPA hdpa;
    LPTSTR lpsz = NULL;
    int i;
    LPFOUNDODATA lpud;

    if (lpua->lpData == (void *)-1)
        return;

    if (!lpua->lpData) 
    {
        lpua->lpData = LocalAlloc(LPTR, sizeof(FOUNDODATA));
        if (!lpua->lpData)
            return;

        ((LPFOUNDODATA)lpua->lpData)->hdpa = (void *)DPA_Create(4);
    }

    lpud = lpua->lpData;

    hdpa = lpud->hdpa;
    if (!hdpa)
        return;

    // if it's a directory that got deleted, we're just going to save it's
    // attributes so that we can recreate it later.
    // directories do NOT get moved into the wastebasket
    if ((lpua->uType == IDS_DELETE) && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        FOUNDO_DELETEDFILEINFO dfi;
        if (!lpud->hdsa)
        {
            lpud->hdsa = DSA_Create(sizeof(FOUNDO_DELETEDFILEINFO),  4);
            if (!lpud->hdsa)
                return;
        }

        Str_SetPtr(&lpsz, lpszSrc);
        dfi.lpszName = lpsz;
        dfi.dwAttributes = dwAttributes;
        DSA_AppendItem(lpud->hdsa, &dfi);
    }
    else
    {    
        Str_SetPtr(&lpsz, lpszSrc);
        if (!lpsz)
            return;

        if ((i = DPA_AppendPtr(hdpa, lpsz)) == -1)
        {
            return;
        }

        lpsz = NULL;
        Str_SetPtr(&lpsz, lpszDest);
        if (!lpsz ||
                DPA_AppendPtr(hdpa, lpsz) == -1)
        {
            DPA_DeletePtr(hdpa, i);
        }
    }
}


LPTSTR DPA_ToFileList(HDPA hdpa, int iStart, int iEnd, int iIncr)
{
    LPTSTR lpsz;
    LPTSTR lpszReturn;
    int ichSize;
    int ichTemp;
    int i;

    // undo copy by deleting destinations
    lpszReturn = (LPTSTR)(void*)LocalAlloc(LPTR, 1);
    if (!lpszReturn)
    {
        return NULL;
    }

    ichSize = 1;
    // build the NULL separated file list
    // go from the end to the front.. restore in reverse order!
    for (i = iEnd; i >= iStart ; i -= iIncr)
    {
        LPTSTR psz;

        lpsz = DPA_GetPtr(hdpa, i);
        ASSERT(lpsz);

        ichTemp  = ichSize - 1;

        ichSize += (lstrlen(lpsz) + 1);
        psz = (LPTSTR)(void*)LocalReAlloc((HLOCAL)lpszReturn, ichSize * sizeof(TCHAR),
                LMEM_MOVEABLE|LMEM_ZEROINIT);
        if (!psz)
        {
            break;
        }
        lpszReturn = psz;
        lstrcpy(lpszReturn + ichTemp, lpsz);
    }

    if ((i + iIncr) != iStart)
    {
        LocalFree((HLOCAL)lpszReturn);
        lpszReturn = NULL;
    }
    return lpszReturn;
}

// from dpa to:
// 'file 1', 'file 2' and 'file 3'
LPTSTR DPA_ToQuotedFileList(HDPA hdpa, int iStart, int iEnd, int iIncr)
{
    LPTSTR lpsz;
    LPTSTR lpszReturn;
    TCHAR szFile[MAX_PATH];
    int ichSize;
    int ichTemp;
    int i;
    SHELLSTATE ss;

    // undo copy by deleting destinations
    lpszReturn = (LPTSTR)(void*)LocalAlloc(LPTR, 1);
    if (!lpszReturn)
    {
        return NULL;
    }

    SHGetSetSettings(&ss, SSF_SHOWEXTENSIONS|SSF_SHOWALLOBJECTS, FALSE);

    ichSize = 1;
    // build the quoted file list
    for (i = iStart; i < iEnd ; i += iIncr)
    {
        LPTSTR psz;

        ichTemp  = ichSize - 1;

        // get the name (filename only without extension)
        lpsz = DPA_GetPtr(hdpa, i);
        lstrcpy(szFile, PathFindFileName(lpsz));
        if (!ss.fShowExtensions)
        {
            PathRemoveExtension(szFile);
        }

        // grow the buffer and add it in
        ichSize += lstrlen(szFile) + 2;
        psz = (LPTSTR)(void*)LocalReAlloc((HLOCAL)lpszReturn, ichSize * sizeof(TCHAR),
                LMEM_MOVEABLE|LMEM_ZEROINIT);
        if (!psz)
        {
            LocalFree(lpszReturn);
            lpszReturn = NULL;
            break;
        }
        lpszReturn = psz;

        // is it too long?
        if (ichSize >= MAX_PATH)
        {
            lstrcat(lpszReturn, c_szEllipses);
            return lpszReturn;
        }
        else
        {
            StrCatBuff(lpszReturn, TEXT("'"), ichSize); //A single quote BEFORE the filename.
            StrCatBuff(lpszReturn, szFile, ichSize);
            StrCatBuff(lpszReturn, TEXT("'"), ichSize); //A single quote AFTER the filename.
        }

        ASSERT(ichSize == ichTemp + (lstrlen(lpszReturn + ichTemp) + 1));
        ichTemp  = ichSize - 1;

        // check to see if we need the "and"
        if ((i + iIncr) < iEnd)
        {
            TCHAR szTemp[40];
            int id;

            ichSize += 40;

            if ((i + (iIncr*2)) >= iEnd)
            {
                id = IDS_SPACEANDSPACE;
            }
            else
            {
                id = IDS_COMMASPACE;
            }

            psz = (LPTSTR)LocalReAlloc((HLOCAL)lpszReturn, ichSize * sizeof(TCHAR),
                    LMEM_MOVEABLE|LMEM_ZEROINIT);
            if (!psz)
            {
                LocalFree(lpszReturn);
                lpszReturn = NULL;
                break;
            }
            lpszReturn = psz;
            LoadString(HINST_THISDLL, id, szTemp, ARRAYSIZE(szTemp));
            StrCatBuff(lpszReturn, szTemp, ichSize);
            ichSize = ichTemp + (lstrlen(lpszReturn + ichTemp) + 1);
        }
    }
    return lpszReturn;
}


void CALLBACK FOUndo_GetText(UNDOATOM *lpua, TCHAR * buffer, int type)
{
    LPFOUNDODATA lpud = (LPFOUNDODATA)lpua->lpData;
    HDPA hdpa = lpud->hdpa;

    if (type == UNDO_MENUTEXT)
    {
        LoadString(HINST_THISDLL, lpua->uType, buffer, MAX_PATH);
    }
    else
    {
        TCHAR szTemplate[80];
        // thank god for growable stacks..
        TCHAR szFile1[MAX_PATH];
        TCHAR szFile2[MAX_PATH];
        TCHAR szFile1Short[30];
        TCHAR szFile2Short[30];
        TCHAR *lpszFile1;
        TCHAR *lpszFile2;

        // get the template
        LoadString(HINST_THISDLL, lpua->uType + (IDS_UNDO_FILEOPHELP - IDS_UNDO_FILEOP), szTemplate, ARRAYSIZE(szTemplate));

        if (lpua->uType == IDS_RENAME)
        {
            SHELLSTATE ss;
            LPTSTR pszTemp;

            // fill in the file names
            lpszFile1 = DPA_GetPtr(hdpa, 0);
            lpszFile2 = DPA_GetPtr(hdpa, 1);
            lstrcpy(szFile1, PathFindFileName(lpszFile1));
            lstrcpy(szFile2, PathFindFileName(lpszFile2));

            SHGetSetSettings(&ss, SSF_SHOWEXTENSIONS, FALSE);
            if (!ss.fShowExtensions)
            {
                PathRemoveExtension(szFile1);
                PathRemoveExtension(szFile2);
            }

            // length sanity check
            // don't just whack "..." at 30 bytes into szFile1 since that may be a dbcs character...
            PathCompactPathEx(szFile1Short, szFile1, ARRAYSIZE(szFile1Short), 0);
            PathCompactPathEx(szFile2Short, szFile2, ARRAYSIZE(szFile2Short), 0);

            pszTemp = ShellConstructMessageString(HINST_THISDLL, szTemplate, szFile1Short, szFile2Short);
            if (pszTemp)
            {
                StrCpyN(buffer, pszTemp, MAX_PATH);
                LocalFree(pszTemp);
            }
        }
        else
        {
            TCHAR *lpszFile1;
            HDPA hdpaFull = hdpa;
            // in the case of delete (where ther's an hdsa)
            // we need to add in the names of folders deleted
            // we do this by cloning the hdpa and tacking on our names.
            if (lpud->hdsa)
            {
                hdpaFull = DPA_Clone(hdpa, NULL);
                if (hdpaFull)
                {
                    int iMax;
                    int i;
                    LPFOUNDO_DELETEDFILEINFO lpdfi;
                    iMax = DSA_GetItemCount(lpud->hdsa);
                    for (i = 0; i < iMax; i++)
                    {
                        lpdfi = DSA_GetItemPtr(lpud->hdsa, i);
                        DPA_AppendPtr(hdpaFull, lpdfi->lpszName);
                        DPA_AppendPtr(hdpaFull, lpdfi->lpszName);
                    }
                }
                else
                {
                    hdpaFull = hdpa;
                }
            }
            lpszFile1 = DPA_ToQuotedFileList(hdpaFull, 0, DPA_GetPtrCount(hdpaFull), 2);
            wnsprintf(buffer, MAX_PATH, szTemplate, lpszFile1);
            LocalFree((HLOCAL)lpszFile1);
            if (hdpaFull != hdpa)
            {
                DPA_Destroy(hdpaFull);
            }
        }
    }
}


void CALLBACK FOUndo_Release(UNDOATOM *lpua)
{
    LPFOUNDODATA lpud = (LPFOUNDODATA)lpua->lpData;
    int i;
    LPTSTR lpsz;
    if (lpud && (lpud != (void *)-1))
    {
        HDPA hdpa = lpud->hdpa;
        HDSA hdsa = lpud->hdsa;
        if (hdpa)
        {
            i = DPA_GetPtrCount(hdpa) - 1;
            for (; i >= 0; i--)
            {
                lpsz = DPA_FastGetPtr(hdpa, i);
                Str_SetPtr(&lpsz, NULL);
            }
            DPA_Destroy(hdpa);
        }

        if (hdsa)
        {
            LPFOUNDO_DELETEDFILEINFO lpdfi;
            i = DSA_GetItemCount(hdsa) - 1;
            for (; i >= 0 ; i--)
            {
                lpdfi = DSA_GetItemPtr(hdsa, i);
                Str_SetPtr(&lpdfi->lpszName, NULL);
            }
            DSA_Destroy(hdsa);
        }
        LocalFree(lpud);
        lpua->lpData = (void *)-1;
    }
}

DWORD WINAPI FOUndo_InvokeThreadInit(UNDOATOM *lpua)
{
    LPFOUNDODATA lpud = (LPFOUNDODATA)lpua->lpData;
    HDPA hdpa = lpud->hdpa;
    HWND hwnd = lpua->hwnd;
    BOOL fNukeAtom = TRUE;
    SHFILEOPSTRUCT sFileOp =
    {
        hwnd,
        0,
        NULL,
        NULL,
        0,
    } ;
    int iMax;

    SuspendUndo(TRUE);
    iMax = DPA_GetPtrCount(hdpa);
    switch (lpua->uType)
    {
        case IDS_RENAME:
            {
                TCHAR szFromPath[MAX_PATH + 1];
                if (iMax < 2)
                    goto Exit;

                sFileOp.wFunc = FO_RENAME;
                sFileOp.pFrom = DPA_GetPtr(hdpa, 1);
                sFileOp.pTo = DPA_GetPtr(hdpa, 0);
                if (sFileOp.pFrom && sFileOp.pTo)
                {
                    lstrcpy(szFromPath, sFileOp.pFrom);
                    szFromPath[lstrlen(sFileOp.pFrom) + 1] = 0;
                    sFileOp.pFrom = szFromPath;
                    SHFileOperation(&sFileOp);
                    if (sFileOp.fAnyOperationsAborted)
                    {
                        fNukeAtom = FALSE;
                    }
                }
                // In the rename case the DPA owns these pointers, in all other cases
                // they must be freed below.  To prevent freeing these during a rename
                // we NULL them out when we're done with them
                sFileOp.pFrom = NULL;
                sFileOp.pTo = NULL;
            }
            break;

        case IDS_COPY:
            sFileOp.pFrom = DPA_ToFileList(hdpa, 1, iMax - 1, 2);
            if (!sFileOp.pFrom)
                goto Exit;
            sFileOp.wFunc = FO_DELETE;
            //
            // If this delete is occuring because of an automatic undo caused by
            // connected files, then do not ask for confirmation.
            //
            if (lpua->foFlags & FOF_NOCONFIRMATION)
                sFileOp.fFlags |= FOF_NOCONFIRMATION;

            SHFileOperation(&sFileOp);
            if (sFileOp.fAnyOperationsAborted)
            {
                fNukeAtom = FALSE;
            }
            break;

        case IDS_MOVE:
            sFileOp.pFrom = DPA_ToFileList(hdpa, 1, iMax-1, 2);
            sFileOp.pTo = DPA_ToFileList(hdpa, 0, iMax-2, 2);
            if (!sFileOp.pFrom || !sFileOp.pTo)
                goto Exit;
            sFileOp.wFunc = FO_MOVE;
            sFileOp.fFlags = FOF_MULTIDESTFILES;
            if (lpua->foFlags & FOF_NOCOPYSECURITYATTRIBS)
            {
                sFileOp.fFlags |= FOF_NOCOPYSECURITYATTRIBS;
            }
            SHFileOperation(&sFileOp);
            if (sFileOp.fAnyOperationsAborted)
            {
                fNukeAtom = FALSE;
            }
            break;

        case IDS_DELETE:
            {
                // first create any directories
                if (lpud->hdsa)
                {
                    HDSA hdsa = lpud->hdsa;
                    int i;
                    // do it in reverse order to get the parentage right
                    for (i = DSA_GetItemCount(hdsa) - 1; i >= 0; i--)
                    {
                        LPFOUNDO_DELETEDFILEINFO lpdfi = DSA_GetItemPtr(hdsa, i);
                        if (lpdfi)
                        {
                            if (Win32CreateDirectory(lpdfi->lpszName, NULL))
                            {
                                SetFileAttributes(lpdfi->lpszName, lpdfi->dwAttributes & ~FILE_ATTRIBUTE_DIRECTORY);
                            }
                        }
                    }
                }

                if (iMax)
                {
                    sFileOp.pFrom = DPA_ToFileList(hdpa, 1, iMax-1, 2);
                    sFileOp.pTo = DPA_ToFileList(hdpa, 0, iMax-2, 2);
                    if (!sFileOp.pFrom || !sFileOp.pTo)
                        goto Exit;
                    UndoBBFileDelete(sFileOp.pTo, sFileOp.pFrom);
                }
                break;
            }
    }
    SHChangeNotify(0, SHCNF_FLUSH | SHCNF_FLUSHNOWAIT, NULL, NULL);

Exit:
    if (sFileOp.pFrom)
        LocalFree((HLOCAL)sFileOp.pFrom);
    if (sFileOp.pTo)
        LocalFree((HLOCAL)sFileOp.pTo);

    SuspendUndo(FALSE);
    if (fNukeAtom)
        NukeUndoAtom(lpua);
    return 1;
}

void CALLBACK FOUndo_Invoke(UNDOATOM *lpua)
{
    DWORD idThread;
    HANDLE hthread = CreateThread(NULL, 0, FOUndo_InvokeThreadInit, lpua, 0, &idThread);
    if (hthread) 
        CloseHandle(hthread);
}

UNDOATOM *FOAllocUndoAtom(LPSHFILEOPSTRUCT lpfo)
{
    UNDOATOM *lpua = (UNDOATOM *)LocalAlloc(LPTR, sizeof(*lpua));
    if (lpua)
    {
        lpua->uType = FOFuncToStringID(lpfo->wFunc);
        lpua->GetText = FOUndo_GetText;
        lpua->Invoke = FOUndo_Invoke;
        lpua->Release = FOUndo_Release;
        lpua->foFlags = 0;

        if (lpfo->fFlags & FOF_NOCOPYSECURITYATTRIBS)
        {
            lpua->foFlags |= FOF_NOCOPYSECURITYATTRIBS;
        }
    }
    return lpua;
}

//============================================================================
//
// The following function is the mainline function for COPYing, RENAMEing,
// DELETEing, and MOVEing single or multiple files.
//
// in:
// hwnd         the parent to create the progress dialog from if FOF_CREATEPROGRESSDLG is set.
//
//
// wFunc        operation to be performed:
//              FO_DELETE - Delete files in pFrom (pTo unused)
//              FO_RENAME - Rename files
//              FO_MOVE   - Move files in pFrom to pTo
//              FO_COPY   - Copy files in pFrom to pTo
//
// pFrom        list of source file specs either qualified or
//              unqualified.  unqualified names will be qualified based on the current
//              global current directories.  examples include
//              "foo.txt bar.txt *.bak ..\*.old dir_name"
//
// pTo          destination file spec.
//
// fFlags       flags that control the operation
//
// returns:
//      0 indicates success
//      != 0 is the DE_ (dos error code) of last failed operation
//
//
//===========================================================================

int WINAPI SHFileOperation(LPSHFILEOPSTRUCT lpfo)
{
    int ret;
    BOOL bRecycledStuff = FALSE;
    COPY_STATE *pcs; 

    if (!lpfo || !lpfo->pFrom)
    {
        // return an error instead of waiting to AV
        return ERROR_INVALID_PARAMETER;
    }

    lpfo->fAnyOperationsAborted = FALSE;
    lpfo->hNameMappings = NULL;

    if (lpfo->wFunc < FO_MOVE || lpfo->wFunc > FO_RENAME)   // validate
    {
        // NOTE: We used to return 0 here (win95gold -> IE401). 
        //
        // If we run into app compat bugs because they were relying on the old 
        // buggy return value, then add an app hack here.
        // 
        // this is not a DE_ error, and I don't care!
        return ERROR_INVALID_PARAMETER;
    }

    pcs = (COPY_STATE*)LocalAlloc(LPTR, sizeof(COPY_STATE));
    if (!pcs)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    pcs->nRef = 1;

    //
    //  REVIEW:  We want to allow copying of a file within a given directory
    //           by having default renaming on collisions within a directory.
    //
    if (!(lpfo->fFlags & FOF_NOCONFIRMATION))
    {
        pcs->cd.fConfirm =
            CONFIRM_DELETE_FILE         |
            CONFIRM_DELETE_FOLDER       |
            CONFIRM_REPLACE_FILE        |
            CONFIRM_REPLACE_FOLDER      |
            CONFIRM_WONT_RECYCLE_FILE   |
            CONFIRM_WONT_RECYCLE_FOLDER |
            CONFIRM_PATH_TOO_LONG       |
            //          CONFIRM_MOVE_FILE           |
            //          CONFIRM_MOVE_FOLDER         |
            //          CONFIRM_RENAME_FILE         |
            //          CONFIRM_RENAME_FOLDER       |
            CONFIRM_SYSTEM_FILE         |
            CONFIRM_READONLY_FILE       |
            CONFIRM_MULTIPLE            |
            CONFIRM_PROGRAM_FILE        |
            CONFIRM_STREAMLOSS          |
            CONFIRM_FAILED_ENCRYPT      |
            CONFIRM_LFNTOFAT            |
            CONFIRM_WONT_RECYCLE_OFFLINE |
            CONFIRM_LOST_ENCRYPT_FILE |
            CONFIRM_LOST_ENCRYPT_FOLDER;
    }

    if (lpfo->fFlags & FOF_WANTNUKEWARNING)
    {
        // We will warn the user that the thing they thought was going to be recycled is
        // now really going to be nuked. (eg drag-drop folder on recycle bin, but it turns
        // out that the folder is too big for the bitbucket, so we confirm on the wont-recycle
        // cases).
        // 
        // Also, we keep the system file / readonly file / progran file warnings around for good
        // measure.
        pcs->cd.fConfirm |= CONFIRM_WONT_RECYCLE_FILE   |
            CONFIRM_WONT_RECYCLE_FOLDER |
            CONFIRM_PATH_TOO_LONG       |
            CONFIRM_SYSTEM_FILE         |
            CONFIRM_READONLY_FILE       |
            CONFIRM_PROGRAM_FILE        |
            CONFIRM_WONT_RECYCLE_OFFLINE;
    }


    pcs->fFlags = lpfo->fFlags;   // duplicate some stuff here
    pcs->lpszProgressTitle = lpfo->lpszProgressTitle;
    pcs->lpfo = lpfo;

    // Check to see if we need to operate on the "connected" files and folders too!
    if (!(pcs->fFlags & FOF_NO_CONNECTED_ELEMENTS))
    {
        DWORD   dwFileFolderConnection = 0;
        DWORD   dwSize = sizeof(dwFileFolderConnection);
        DWORD   dwType = REG_DWORD;

        if (SHGetValue(HKEY_CURRENT_USER, REGSTR_PATH_EXPLORER, 
                    REG_VALUE_NO_FILEFOLDER_CONNECTION, &dwType, &dwFileFolderConnection, 
                    &dwSize) == ERROR_SUCCESS)
        {
            //If the registry says "No connection", then set the flags accordingly.
            if (dwFileFolderConnection == 1)
            {
                pcs->fFlags = pcs->fFlags | FOF_NO_CONNECTED_ELEMENTS;
            }
        }
    }

    // Always create a progress dialog
    // Note that it will be created invisible, and will be shown if the
    // operation takes longer than a second to perform
    // Note the parent of this window is NULL so it will get the QUERYENDSESSION
    // message
    if (!(pcs->fFlags & FOF_SILENT))
    {
        SHCreateThread(FOUIThreadProc, pcs, 0, AddRefPCS);
    }
    else 
    {
        // To be compatible with Win95 semantics...
        if (!lpfo->hwnd)
        {
            pcs->fFlags |= FOF_NOERRORUI;
        }
    }

    if (lpfo->hwnd)
    {
        // The caller will be disabled if we ever show the progress window
        // We need to make sure this is not disabled now because if it is and
        // another dialog uses this as its parent, USER code will tell this
        // window it got the focus while it is still disabled, which keeps it
        // from passing that focus down to its children
        // EnableWindow(lpfo->hwnd, FALSE);
        pcs->hwndDlgParent = lpfo->hwnd;
    }

    // do this always.. even if this is not an undoable op, we could be
    // affecting something that is.
    SuspendUndo(TRUE);

    if (lpfo->fFlags & FOF_ALLOWUNDO)
    {
        pcs->lpua = FOAllocUndoAtom(lpfo);
        if (lpfo->wFunc == FO_DELETE)
        {
            // We check the shell state to see if the user has turned on the
            // "Don't confirm deleting recycle bin contents" flag.  If yes,
            // then we store this flag and check against it if this case occures.
            // Review: Not a super common case, why not just check when the
            // flag is actually needed?
            SHELLSTATE ss;
            SHGetSetSettings(&ss, SSF_NOCONFIRMRECYCLE, FALSE);
            pcs->fNoConfirmRecycle = ss.fNoConfirmRecycle;

            if (InitBBGlobals())
            {
                // since we are going to be recycling stuff, we add ourselves to the
                // global list of threads who are recycling
                SHGlobalCounterIncrement(g_hgcNumDeleters);
                bRecycledStuff = TRUE;
            }
            else
            {
                // this shouldnt happen, but if it does we can't send stuff to the Recycle
                // Bin, instead we remove the undo flag so that everything is really nuked.
                lpfo->fFlags &= ~FOF_ALLOWUNDO;
                LocalFree(pcs->lpua);
                pcs->lpua = NULL;
            }
        }

    }

    // While doing the file operation, tell PnP not to suspend
    // the machine.  Otherwise, you could be copying a lot of files
    // over the network and have the laptop suddenly hibernate on you
    // because PnP thought you were idle.
    //
    // Indicate that we only need the system.  It's okay if the display
    // goes into low-power mode, as long as we can keep copying.
    //

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

    ret = MoveCopyDriver(pcs);

    SetThreadExecutionState(ES_CONTINUOUS);

    if (pcs->bAbort)
    {
        ASSERT(pcs->lpfo == lpfo);
        lpfo->fAnyOperationsAborted = TRUE;
    }

    if (bRecycledStuff)
    {
        SHUpdateRecycleBinIcon();

        if (0 == SHGlobalCounterDecrement(g_hgcNumDeleters))
        {
            // We were the last guy who was deleting stuff. Thus, we need to
            // check to see if any of the bitbuckets info files neeed compacting or purging
            CheckCompactAndPurge();
        }
    }

    if (pcs->lpCopyBuffer)
    {
        LocalFree((HLOCAL)pcs->lpCopyBuffer);
        pcs->lpCopyBuffer = NULL;
    }

    if (pcs->lpua)
    {
        if (pcs->lpua->lpData && (pcs->lpua->lpData != (void *)-1))
        {
            AddUndoAtom(pcs->lpua);
        }
        else
        {
            FOUndo_Release(pcs->lpua);
            NukeUndoAtom(pcs->lpua);
        }
    }

    // NTRAID89119 (toddb): This code is totally busted in respect to mounted volumes.
    // We will send a change notify for the drive on which your volume is mounted
    // instead of on the volume which actually had a free space change.  We need
    // to update PathGetDriveNumber to handle mounted volumes

    // notify of freespace changes
    // rename doesn't change drive usage
    if (lpfo->wFunc != FO_RENAME)
    {
        int idDriveSrc;
        int idDriveDest = -1;
        DWORD dwDrives = 0; // bitfield for drives

        if (lpfo->wFunc == FO_COPY)
        {
            // nothing changes on the source
            idDriveSrc = -1;
        }
        else
        {
            idDriveSrc = PathGetDriveNumber(lpfo->pFrom);
        }

        if (lpfo->pTo)
        {
            idDriveDest = PathGetDriveNumber(lpfo->pTo);
        }

        if ((lpfo->wFunc == FO_MOVE) && (idDriveDest == idDriveSrc))
        {
            // no freespace nothing changes
            idDriveSrc = -1;
            idDriveDest = -1;
        }

        // NTRAID89119: What if idDriveSrc or idDriveDest are > 32?  This is totally
        // possible under NT by using mounted volumes.  SHChangeNotify is busted
        // in this respect.

        if (idDriveSrc != -1)
        {
            dwDrives |= (1 << idDriveSrc);
        }

        if (idDriveDest != -1)
        {
            dwDrives |= (1 << idDriveDest);
        }

        if (dwDrives)
        {
            SHChangeNotify(SHCNE_FREESPACE, SHCNF_DWORD, IntToPtr(dwDrives), 0);
        }
    }

    SuspendUndo(FALSE);

    if (!(lpfo->fFlags & FOF_WANTMAPPINGHANDLE))
    {
        SHFreeNameMappings(lpfo->hNameMappings);
        lpfo->hNameMappings = NULL;
    }

    // shut down the progress dialog
    //    
    // this is necessary so that the ui thread won't block
    pcs->fProgressOk = TRUE;

    ENTERCRITICAL;  // need to take critsec to sync w/ the UI thread
    pcs->fDone = TRUE;
    if (pcs->hwndProgress)
    {
        PostMessage(pcs->hwndProgress, PDM_NOOP, 0, 0);
    }
    LEAVECRITICAL;

    if (lpfo->hwnd)
    {
        EnableWindow(lpfo->hwnd, TRUE);
    }

    ReleasePCS(pcs);

    return ret;
}

#ifdef UNICODE
int WINAPI SHFileOperationA(LPSHFILEOPSTRUCTA lpfo)
{
    int iResult;
    UINT uTotalSize;
    UINT uSize;
    UINT uSizeTitle;
    UINT uSizeW;
    SHFILEOPSTRUCTW shop;
    LPCSTR lpAnsi;
    LPWSTR lpBuffer;
    LPWSTR lpTemp;

    COMPILETIME_ASSERT(sizeof(SHFILEOPSTRUCTW) == sizeof(SHFILEOPSTRUCTA));

    hmemcpy(&shop, lpfo, sizeof(SHFILEOPSTRUCTW));

    //
    // Thunk the strings as appropriate
    //
    uTotalSize = 0;
    if (lpfo->pFrom)
    {
        lpAnsi = lpfo->pFrom;
        do {
            uSize = lstrlenA(lpAnsi) + 1;
            uTotalSize += uSize;
            lpAnsi += uSize;
        } while (uSize != 1);
    }

    if (lpfo->pTo)
    {
        lpAnsi = lpfo->pTo;
        do {
            uSize = lstrlenA(lpAnsi) + 1;
            uTotalSize += uSize;
            lpAnsi += uSize;
        } while (uSize != 1);
    }

    if ((lpfo->fFlags & FOF_SIMPLEPROGRESS) && lpfo->lpszProgressTitle != NULL)
    {
        uSizeTitle = lstrlenA(lpfo->lpszProgressTitle) + 1;
        uTotalSize += uSizeTitle;
    }

    if (uTotalSize != 0)
    {
        lpTemp = lpBuffer = LocalAlloc(LPTR, uTotalSize*sizeof(WCHAR));
        if (!lpBuffer)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        lpBuffer = NULL;
    }

    //
    // Now convert the strings
    //
    if (lpfo->pFrom)
    {
        shop.pFrom = lpTemp;
        lpAnsi = lpfo->pFrom;
        do
        {
            uSize = lstrlenA(lpAnsi) + 1;
            uSizeW = MultiByteToWideChar(CP_ACP, 0,
                    lpAnsi, uSize,
                    lpTemp, uSize);
            lpAnsi += uSize;
            lpTemp += uSizeW;
        } while (uSize != 1);
    }
    else
    {
        shop.pFrom = NULL;
    }

    if (lpfo->pTo)
    {
        shop.pTo = lpTemp;
        lpAnsi = lpfo->pTo;
        do
        {
            uSize = lstrlenA(lpAnsi) + 1;
            uSizeW = MultiByteToWideChar(CP_ACP, 0,
                    lpAnsi, uSize,
                    lpTemp, uSize);
            lpAnsi += uSize;
            lpTemp += uSizeW;
        } while (uSize != 1);
    }
    else
    {
        shop.pTo = NULL;
    }


    if ((lpfo->fFlags & FOF_SIMPLEPROGRESS) && lpfo->lpszProgressTitle != NULL)
    {
        shop.lpszProgressTitle = lpTemp;
        MultiByteToWideChar(CP_ACP, 0,
                lpfo->lpszProgressTitle, uSizeTitle,
                lpTemp, uSizeTitle);
    }
    else
    {
        shop.lpszProgressTitle = NULL;
    }

    iResult = SHFileOperationW(&shop);

    // link up the two things in the SHFILEOPSTRUCT that could have changed
    lpfo->fAnyOperationsAborted = shop.fAnyOperationsAborted;
    lpfo->hNameMappings = shop.hNameMappings;

    if (lpBuffer)
        LocalFree(lpBuffer);

    return iResult;
}

#else

int WINAPI SHFileOperationW(LPSHFILEOPSTRUCTW lpfo)
{
    return E_NOTIMPL;   
}
#endif


// In:
//      pcs:  copy_state structure containing the state of the copy
//
// feedback: If the estimated time to copmplete a copy is larger than
//   MINTIME4FEEDBACK, the user is given a time to completion estimate in minutes.
//   The estimate is calculated using a MS_RUNAVG seconds running average.  The
//   initial estimate is done after MS_TIMESLICE

void SetProgressTime(COPY_STATE *pcs)
{
    DWORD dwNow = GetTickCount();

    if (pcs->dwPreviousTime)
    {

        int iPointsTotal = CountProgressPoints(pcs, &pcs->dth.dtAll);
        int iPointsDone = CountProgressPoints(pcs, &pcs->dth.dtDone);
        int iPointsDelta = iPointsDone - pcs->iLastProgressPoints;
        DWORD dwTimeLeft;

        //
        // A couple of times the shell has reported bad time remaining
        // we need to find out why.
        //
        ASSERT(iPointsTotal >= 0);
        ASSERT(iPointsDone >= 0);
        ASSERT(iPointsTotal >= iPointsDone);
        ASSERT(iPointsDelta >= 0);

        // has enough time elapsed to update the display
        // We do this every 10 seconds, but we'll do the first one after
        // only a few seconds

        if (iPointsDelta && (iPointsDone > 0) && (dwNow - pcs->dwPreviousTime))
        {
            DWORD dwPointsPerSec;
            DWORD dwTime; // how many tenths of a second have gone by

            // We take 10 times the number of Points and divide by the number of
            // tenths of a second to minimize both overflow and roundoff
            dwTime = (dwNow - pcs->dwPreviousTime)/100;
            if (dwTime == 0)
                dwTime = 1;
            dwPointsPerSec = iPointsDelta * 10 / dwTime;
            if (!dwPointsPerSec)
            {
                // This could happen if the net went to sleep for a couple
                // minutes while trying to copy a small (512 byte) buffer
                dwPointsPerSec = 1;
            }

            // if we didn't have enough time to get a good sample,
            // don't use this last bit as a time estimater
            if ((dwNow - pcs->dwPreviousTime) < (MS_TIMESLICE/2))
            {
                dwPointsPerSec = pcs->dwPointsPerSec;
            }

            if (pcs->dwPointsPerSec)
            {
                // Take a weighted average of the current transfer rate and the
                // previously computed one, just to try to smooth out
                // some random fluctuations

                dwPointsPerSec = (dwPointsPerSec + (pcs->dwPointsPerSec * 2)) / 3;
            }

            // never allow 0 points per second.. just tack it on to next time
            if (dwPointsPerSec)
            {
                pcs->dwPointsPerSec = dwPointsPerSec;

                // Calculate time remaining (round up by adding 1)
                // We only get here every 10 seconds, so always update
                dwTimeLeft = ((iPointsTotal - iPointsDone) / dwPointsPerSec) + 1;

                // It would be odd to show "1 second left" and then immediately
                // clear it
                if (dwTimeLeft >= MIN_MINTIME4FEEDBACK)
                {
                    // display new estimate of time left
                    SetProgressTimeEst(pcs, dwTimeLeft);
                }
            }

        }
        // Reset previous time and # of Points read
        pcs->dwPreviousTime = dwNow;
        pcs->iLastProgressPoints = iPointsDone;
    }
}

void InitClipConfirmDlg(HWND hDlg, CONFDLG_DATA *pcd)
{
    TCHAR szMessage[255];
    TCHAR szDeleteWarning[80];
    SHFILEINFO sfiDest;
    LPTSTR pszFileDest = NULL;
    LPTSTR pszMsg, pszSource;
    int i;
    int cxWidth;
    RECT rc;

    // get the size of the text boxes
    GetWindowRect(GetDlgItem(hDlg, pcd->idText), &rc);
    cxWidth = rc.right - rc.left;

    // get the source display name
    pszSource = PathFindFileName(pcd->pFileSource);
    PathCompactPath(NULL, pszSource, cxWidth);

    // get the dest display name
    SHGetFileInfo(pcd->pFileDest, 0,
            &sfiDest, sizeof(sfiDest), SHGFI_DISPLAYNAME | SHGFI_USEFILEATTRIBUTES);
    pszFileDest = sfiDest.szDisplayName;
    PathCompactPath(NULL, pszFileDest, cxWidth);

    // if we're supposed to show the date info, grab the icons and format the date string
    if (pcd->bShowDates) 
    {
        SHFILEINFO sfi2;
        TCHAR szDateSrc[64], szDateDest[64];

        // likely that this data may be incomplete... leave it saying "Unknown date and size"
        if (BuildDateLine(szDateSrc, pcd->pfdSource, pcd->pFileSource))
            SetDlgItemText(hDlg, IDD_FILEINFO_NEW,  szDateSrc);

        BuildDateLine(szDateDest, pcd->pfdDest, pcd->pFileDest);
        SetDlgItemText(hDlg, IDD_FILEINFO_OLD,  szDateDest);

        SHGetFileInfo(pcd->pFileSource, pcd->pfdSource ? pcd->pfdSource->dwFileAttributes : 0, &sfi2, sizeof(sfi2),
                pcd->pfdSource ? (SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON) : (SHGFI_ICON|SHGFI_LARGEICON));
        ReplaceDlgIcon(hDlg, IDD_ICON_NEW, sfi2.hIcon);

        SHGetFileInfo(pcd->pFileDest, pcd->pfdDest ? pcd->pfdDest->dwFileAttributes : 0, &sfi2, sizeof(sfi2),
                pcd->pfdDest ? (SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON) : (SHGFI_ICON|SHGFI_LARGEICON));
        ReplaceDlgIcon(hDlg, IDD_ICON_OLD, sfi2.hIcon);
    }

    // there are 5 controls:
    // IDD_TEXT contains regular text (normal file/folder)
    // IDD_TEXT1 through IDD_TEXT4 contain optional secondary text
    for (i = IDD_TEXT; i <= IDD_TEXT4; i++)
    {
        if (i == pcd->idText)
        {
            szMessage[0] = 0;
            GetDlgItemText(hDlg, i, szMessage, ARRAYSIZE(szMessage));
        }
        else
        {
            HWND hwndCtl = GetDlgItem(hDlg, i);
            if (hwndCtl)
            {
                ShowWindow(hwndCtl, SW_HIDE);
            }
        }
    }

    szDeleteWarning[0] = 0;

    pszMsg = ShellConstructMessageString(HINST_THISDLL, szMessage,
            pszSource, pszFileDest, szDeleteWarning);

    if (pszMsg)
    {
        SetDlgItemText(hDlg, pcd->idText, pszMsg);
        LocalFree(pszMsg);
    }
}

void FileDescToWin32FileData(LPFILEDESCRIPTOR pfdsc, LPWIN32_FIND_DATA pwfd)
{
    ZeroMemory(pwfd, sizeof(*pwfd));

    if (pfdsc->dwFlags & FD_ATTRIBUTES)
        pwfd->dwFileAttributes = pfdsc->dwFileAttributes;
    if (pfdsc->dwFlags & FD_CREATETIME)
        hmemcpy(&pwfd->ftCreationTime, &pfdsc->ftCreationTime, sizeof(FILETIME));
    if (pfdsc->dwFlags & FD_ACCESSTIME)
        hmemcpy(&pwfd->ftLastAccessTime, &pfdsc->ftLastAccessTime, sizeof(FILETIME));
    if (pfdsc->dwFlags & FD_WRITESTIME)
        hmemcpy(&pwfd->ftLastWriteTime, &pfdsc->ftLastWriteTime, sizeof(FILETIME));
    if (pfdsc->dwFlags & FD_FILESIZE)
    {
        pwfd->nFileSizeHigh = pfdsc->nFileSizeHigh;
        pwfd->nFileSizeLow = pfdsc->nFileSizeLow;
    }
    lstrcpy(pwfd->cFileName, pfdsc->cFileName);
}

INT_PTR ValidateCreateFileFromClip(HWND hwnd, LPFILEDESCRIPTOR pfdscSrc, TCHAR *pszPathDest, PYNLIST pynl)
{
    WIN32_FIND_DATA wfdSrc, wfdDest;
    CONFDLG_DATA cdd;
    CONFIRM_DATA cd;
    COPY_STATE cs;
    INT_PTR result;

    //
    // If the destination does not exist, we are done.
    //
    HANDLE hff = FindFirstFile(pszPathDest, &wfdDest);
    if (hff == INVALID_HANDLE_VALUE)
    {
        return IDYES;
    }
    FindClose(hff);

    //
    // Maybe this was just a short name collision and
    // we can quickly get out of here.
    //
    if (ResolveShortNameCollisions(pszPathDest, &wfdDest))
    {
        return IDYES;
    }

    //
    // Most of the helper functions want a WIN32_FILE_DATA
    // and not a FILEDESCRIPTOR, so we create wfd for the
    // source file on the fly.
    //
    FileDescToWin32FileData(pfdscSrc, &wfdSrc);

    //
    // Take care of the easy cases - can't copy a file to a dir
    // or a dir to a file.
    //
    if ((wfdDest.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            ((wfdSrc.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0))
    {
        ZeroMemory(&cs, sizeof(cs));
        cs.hwndDlgParent = hwnd;

        CopyError(&cs, wfdSrc.cFileName, pszPathDest, DE_FILEDESTISFLD | ERRORONDEST, FO_COPY, OPER_DOFILE);
        return IDNO;
    }
    else if (((wfdDest.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
            (wfdSrc.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        ZeroMemory(&cs, sizeof(cs));
        cs.hwndDlgParent = hwnd;

        CopyError(&cs, wfdSrc.cFileName, pszPathDest, DE_FLDDESTISFILE | ERRORONDEST, FO_COPY, OPER_DOFILE);

        AddToNoList(pynl, pszPathDest);

        return IDNO;
    }

    //
    // We need a confirmation dialog.  Fill in the
    // ConfirmDialogData (cdd) here.
    //

    ZeroMemory(&cdd, sizeof(cdd));

    cdd.InitConfirmDlg = InitClipConfirmDlg;
    cdd.idText = IDD_TEXT;
    cdd.pFileSource = pfdscSrc->cFileName;
    cdd.pfdSource = &wfdSrc;
    cdd.pFileDest = pszPathDest;
    cdd.pfdDest = &wfdDest;
    cdd.bShowDates = FALSE;
    cdd.pcd = &cd;

    ZeroMemory(&cd, sizeof(cd));
    cd.fConfirm = CONFIRM_REPLACE_FILE;
    cdd.fYesToAllMask = CONFIRM_REPLACE_FILE;

    if (((wfdDest.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
            (wfdDest.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY)))
    {
        if (wfdDest.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
        {
            cdd.idText = IDD_TEXT2;
        }
        else if (wfdDest.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
        {
            cdd.idText = IDD_TEXT1;
        }
    }

    //
    // What we do now depends on whether we are processing a directory
    // or a file.
    //
    if (wfdDest.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        //
        // If this directory is already in the yes list,
        // the parent directory must have already conflicted
        // and the user said "yes, move the dir contents over".
        //
        if (IsInYesList(pynl, pszPathDest))
        {
            result = IDYES;
        }
        else
        {
            //
            // Copying directory to a destination with the same directory.
            //
            result = DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_REPLACE_FOLDER), hwnd, ConfirmDlgProc, (LPARAM)&cdd);

            if (result == IDYES)
            {
                if (cd.fConfirm & CONFIRM_REPLACE_FILE)
                {
                    AddToYesList(pynl, pszPathDest);
                }
                else
                {
                    SetYesToAll(pynl);
                }
            }
            else if (result == IDNO)
            {
                AddToNoList(pynl, pszPathDest);
            }
        }
    }
    else
    {
        if (IsInYesList(pynl, pszPathDest))
        {
            result = IDYES;
        }
        else
        {
            //
            // Copying a file to a destination with the same file.
            //
            cdd.bShowDates = TRUE;

            result = DialogBoxParam(HINST_THISDLL, MAKEINTRESOURCE(DLG_REPLACE_FILE), hwnd, ConfirmDlgProc, (LPARAM)&cdd);

            if (result == IDYES)
            {
                if ((cd.fConfirm & CONFIRM_REPLACE_FILE) == 0)
                {
                    SetYesToAll(pynl);
                }
            }
        }
    }

    return result;
}


// We can get transient file locks for moving files, for example extracting a thumbnail on a background task
// so we wrap our single check in a loop of multiple checks with a short nap between.  We'd expect
// to get the ERROR_SHARING_VIOLATION, but in practice we also say ERROR_ACCESS_DENIED, so we'll try that
// as well

#define MAX_DELETE_ATTEMPTS  5
#define SLEEP_DELETE_ATTEMPT 1000

BOOL _IsFileDeletable(LPCTSTR pszFile)
{
    int iAttempt = 0;
    BOOL bRet;
    while (!(bRet = IsFileDeletable(pszFile)) && (iAttempt < MAX_DELETE_ATTEMPTS))
    {
        DWORD dwError = GetLastError();

        if ((dwError == ERROR_ACCESS_DENIED) || (dwError == ERROR_SHARING_VIOLATION))
        {
            iAttempt++;
            Sleep(SLEEP_DELETE_ATTEMPT);
        }
        else
        {
            break;
        }
    }

    return (bRet);
}

BOOL _IsDirectoryDeletable(LPCTSTR pszDir)
{
    int iAttempt = 0;
    BOOL bRet;
    while (!(bRet = IsDirectoryDeletable(pszDir)) && (iAttempt < MAX_DELETE_ATTEMPTS))
    {
        DWORD dwError = GetLastError();

        if ((dwError == ERROR_ACCESS_DENIED) || (dwError == ERROR_SHARING_VIOLATION))
        {
            iAttempt++;
            Sleep(SLEEP_DELETE_ATTEMPT);
        }
        else
        {
            break;
        }
    }

    return (bRet);
}

// This function adds up the sizes of the files in pszDir, and
// also makes sure that all those files are "delete-able"
//
// return:  ERROR_SUCCESS - everything is fine, all the files in the dir are deleteable
//          else          - the dir cant be deleted because something inside is non-deletable
//
// NOTE: other in-out params are in the pfdi
//
LONG CheckFolderSizeAndDeleteability(LPCTSTR pszDir, FOLDERDELETEINFO* pfdi, LPCOPY_STATE pcs)
{
    LONG lRet = ERROR_SUCCESS;  // keep stack to a minimum as this is a recursive function!
    BOOL bHasChildren = FALSE;

    if (FOQueryAbort(pcs))
        return ERROR_CANCELLED;

    if (NULL == pszDir)
        return E_INVALIDARG;

    // do the root specific processing
    if (!pfdi->bProcessedRoot)
    {
        // since the the destination folder could be something like "DC100000.oldext", calculate how many characters are
        // going to be in the new destination directory: "C:\recycler\sid" + "\" + "DC100000.oldext" == the new root directory length
        pfdi->cchDelta = (pfdi->cchBBDir + 1 + 8 + 1 + lstrlen(PathFindExtension(pszDir))) - lstrlen(pszDir);

        // set this so that we only do the above processing for the root folder
        pfdi->bProcessedRoot = TRUE;
    }

    if (PathCombine(pfdi->szPath, pszDir, c_szStarDotStar))
    {
        HANDLE hfind = FindFirstFile(pfdi->szPath, &pfdi->fd);

        // remove the *.* we added, because we will be recycling this buffer
        PathRemoveFileSpec(pfdi->szPath);

        if (hfind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!PathIsDotOrDotDot(pfdi->fd.cFileName))
                {
                    bHasChildren = TRUE;

                    // append the subfile/subfolder to the parent path
                    if (!PathAppend(pfdi->szPath, pfdi->fd.cFileName))
                    {
                        // PathAppend failed, try to append the short name
                        if (!pfdi->fd.cAlternateFileName[0] || !PathAppend(pfdi->szPath, pfdi->fd.cAlternateFileName))
                        {
                            // no alternate name or we failed to append that as well, assume we failed because the path is too long
                            lRet = ERROR_FILENAME_EXCED_RANGE;

                            // pass back the name of the non-deleteable file/folder in pfdi->szNonDeletableFile
                            lstrcpyn(pfdi->szNonDeletableFile, pfdi->szPath, ARRAYSIZE(pfdi->szNonDeletableFile));
                        }
                    }

                    if (lRet == ERROR_SUCCESS)
                    {
                        // we have to check to see if the path will exceed MAX_PATH if we were to move this file to the recycle
                        // bin (C:\Recycler\<sid>). the increase in path length due to the recycle bin dir could be enough to 
                        // put us over MAX_PATH and we would have problems later.
                        if ((lstrlen(pfdi->szPath) + pfdi->cchDelta + 1) > MAX_PATH) // +1 for NULL
                        {
                            TraceMsg(TF_BITBUCKET, "CheckFolderSizeAndDeleteability: path '%s' would exceed MAX_PATH if moved to the recycle bin!", pfdi->szPath);
                            lRet = ERROR_FILENAME_EXCED_RANGE;

                            // pass back the name of the non-deleteable file/folder in pfdi->szNonDeletableFile
                            lstrcpyn(pfdi->szNonDeletableFile, pfdi->szPath, ARRAYSIZE(pfdi->szNonDeletableFile));
                        }
                        else
                        {
                            if (pfdi->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                            {
                                // its a directory, so recurse
                                lRet = CheckFolderSizeAndDeleteability(pfdi->szPath, pfdi, pcs);
                            }
                            else 
                            {
                                // its a file.
                                ULARGE_INTEGER ulTemp;

                                if (!_IsFileDeletable(pfdi->szPath))
                                {
                                    // we cant delete this file, find out why
                                    lRet = GetLastError();
                                    ASSERT(lRet != ERROR_SUCCESS);

                                    // pass back the name of the non-deleteable file in pfdi->szNonDeletableFile
                                    lstrcpyn(pfdi->szNonDeletableFile, pfdi->szPath, ARRAYSIZE(pfdi->szNonDeletableFile));
                                }

                                ulTemp.LowPart  = pfdi->fd.nFileSizeLow;
                                ulTemp.HighPart = pfdi->fd.nFileSizeHigh;
                                pfdi->cbSize += ulTemp.QuadPart;
                            }
                        }
                    }

                    // remove the file/dir name we appended
                    PathRemoveFileSpec(pfdi->szPath);
                }

            } while ((lRet == ERROR_SUCCESS) && FindNextFile(hfind, &pfdi->fd));

            FindClose(hfind);

            // if this dir has no children, see if we can simply delete it
            if (!bHasChildren && !_IsDirectoryDeletable(pszDir))
            {
                lRet = GetLastError();
                ASSERT(lRet != ERROR_SUCCESS);

                // pass back the name of the non-deleteable file in pfdi->szNonDeletableFile
                lstrcpyn(pfdi->szNonDeletableFile, pszDir, ARRAYSIZE(pfdi->szNonDeletableFile));
            }
        }
        else
        {
            // if FindFirstFile fails, check to see if the directory itself is deleteable
            if (!_IsDirectoryDeletable(pszDir))
            {
                lRet = GetLastError();
                ASSERT(lRet != ERROR_SUCCESS);

                // pass back the name of the non-deleteable file in pfdi->szNonDeletableFile
                lstrcpyn(pfdi->szNonDeletableFile, pszDir, ARRAYSIZE(pfdi->szNonDeletableFile));
            }
        }
    }
    else
    {
        // if PathCombine fails, assume its because the path is too long
        lRet = ERROR_FILENAME_EXCED_RANGE;

        // pass back the name of the non-deleteable file in pfdi->szNonDeletableFile
        lstrcpyn(pfdi->szNonDeletableFile, pszDir, ARRAYSIZE(pfdi->szNonDeletableFile));
    }

    return lRet;
}

// This takes over for what BBDeleteFile used to do (init, check, delete)... but does it with the ability to cancel
BOOL DeleteFileBB(LPTSTR pszFile, INT *piRet, COPY_STATE *pcs, BOOL fIsDir, WIN32_FIND_DATA *pfd, HDPA *phdpaDeletedFiles)
{
    ULARGE_INTEGER ulSize;
    int idDrive = DriveIDFromBBPath(pszFile);

    // Init
    if (!BBDeleteFileInit(pszFile, piRet))
        return FALSE;

    // Check if we can delete this properly
    if (fIsDir) 
    {
        DWORD dwError;
        FOLDERDELETEINFO fdi = {0};

        fdi.cchBBDir = BBRecyclePathLength(idDrive);

        dwError = CheckFolderSizeAndDeleteability(pszFile, &fdi, pcs);

        if (dwError != ERROR_SUCCESS) 
        {
            // if CheckFolderSizeAndDeleteability can fail if a file cant be recycled.
            // In this case, it appends the name of the file to pszFile, so we know who 
            // the undeletable file is. 
            if ((dwError == ERROR_FILENAME_EXCED_RANGE) ||
                    (dwError == ERROR_BUFFER_OVERFLOW))
            {
                // it failed because a new path would be to long after being moveed under the "C:\recycler\sid" directory
                *piRet = BBDELETE_PATH_TOO_LONG;
            }
            else if (dwError == ERROR_CANCELLED)
            {
                // user hit the cancel button
                *piRet = BBDELETE_CANCELLED;
            }
            else
            {
                // must be a non-deletable directory, so set piRet = BBDELETE_CANNOT_DELETE so our caller
                // can detect this case, also pass the name of the non-deletable file back out so we can give
                // a better error message to the user
                *piRet = BBDELETE_CANNOT_DELETE;
                ASSERT(*fdi.szPath);
                lstrcpy(pszFile, fdi.szNonDeletableFile);
            }

            TraceMsg(TF_BITBUCKET, "DeleteFileBB : early error (%x) on file (%s)", dwError, pszFile);
            return FALSE;
        }

        ulSize.QuadPart = fdi.cbSize;
    }
    else 
    {
        if (!_IsFileDeletable(pszFile))
        {
            // We set piRet = BBDELETE_CANNOT_DELETE so our caller can detect
            // that this file cant be recycled.
            *piRet = BBDELETE_CANNOT_DELETE;
            return FALSE;
        }

        ulSize.LowPart  = pfd->nFileSizeLow;
        ulSize.HighPart = pfd->nFileSizeHigh;
    }

    // check to make sure it's not bigger than the allowed wastebasket..
    if (!BBCheckDeleteFileSize(idDrive, ulSize)) 
    {
        // we set piRet = BBDELETE_SIZE_TOO_BIG so our caller can 
        // detect the "file/folder too big" case
        *piRet = BBDELETE_SIZE_TOO_BIG;

        return FALSE;
    }

    return BBDeleteFile(pszFile, piRet, pcs->lpua, fIsDir, phdpaDeletedFiles, ulSize);
}

void StartCopyEngine(HANDLE *phEventRunning)
{
    *phEventRunning = CreateEvent(CreateAllAccessSecurityAttributes(NULL, NULL, NULL), TRUE, FALSE, L"ShellCopyEngineRunning");
    if (*phEventRunning)
    {
        SetEvent(*phEventRunning);
    }
}

void EndCopyEngine(HANDLE hEventRunning)
{
    // signal that we're done.  this will always trigger so there will be some weirdness if the
    // user does simultaneous copies, but it's not worth making a semaphore to keep track.
    HANDLE hEventFinished = CreateEvent(CreateAllAccessSecurityAttributes(NULL, NULL, NULL), TRUE, FALSE, L"ShellCopyEngineFinished");
    if (hEventFinished)
    {
        SetEvent(hEventFinished);
        CloseHandle(hEventFinished);
    }

    if (hEventRunning)
    {
        // close out the event that says we're running.
        ResetEvent(hEventRunning);
        CloseHandle(hEventRunning);
    }
}

BOOL IsCopyEngineRunning()
{
    BOOL bRet=FALSE;
    HANDLE hEventCopyRunning = OpenEvent(SYNCHRONIZE, FALSE, L"ShellCopyEngineRunning");
    if (hEventCopyRunning)
    {
        // probe the event with a wait, if it times out the copy engine isn't running so we're done.
        bRet = (WAIT_OBJECT_0 == WaitForSingleObject(hEventCopyRunning, 0));
        CloseHandle(hEventCopyRunning);
    }
    return bRet;
}
