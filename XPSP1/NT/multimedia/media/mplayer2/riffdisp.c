/*-----------------------------------------------------------------------------+
| RIFFDISP.C                                                                   |
|                                                                              |
| (C) Copyright Microsoft Corporation 1991.  All rights reserved.              |
|                                                                              |
| Revision History                                                             |
|    Oct-1992 MikeTri Ported to WIN32 / WIN16 common code                      |
|                                                                              |
+-----------------------------------------------------------------------------*/

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <commdlg.h>
#ifdef WIN16
#include "port16.h"
#endif
#include "mplayer.h"
#include "riffdisp.h"

static  HWND        hwndPreview;
static  HANDLE      hdibPreview;
static  TCHAR       achPreview[80];
static  HFONT       hfontPreview;
static  HINSTANCE   hMSVideo;

typedef HANDLE      HDRAWDIB;
typedef HDRAWDIB    (FAR PASCAL *PFNDRAWDIBOPEN)(void);
typedef BOOL        (FAR PASCAL *PFNDRAWDIBCLOSE)(HDRAWDIB hdd);
typedef BOOL        (FAR PASCAL *PFNDRAWDIBDRAW)(HDRAWDIB hdd,HDC hdc,int xDst,int yDst,int dxDst,int dyDst,LPBITMAPINFOHEADER lpbi,LPVOID lpBits,int xSrc,int ySrc,int dxSrc,int dySrc,UINT wFlags);

static  HDRAWDIB    hdd;
static  PFNDRAWDIBOPEN pfnDrawDibOpen;
static  PFNDRAWDIBCLOSE pfnDrawDibClose;
static  PFNDRAWDIBDRAW pfnDrawDibDraw;

#define GetHInstance()  (HINSTANCE)(SELECTOROF((LPVOID)&hwndPreview))

/***************************************************************************
 *
 ****************************************************************************/

//#define FOURCC_RIFF mmioFOURCC('R','I','F','F')
#define FOURCC_INFO mmioFOURCC('I','N','F','O')
#define FOURCC_DISP mmioFOURCC('D','I','S','P')
#define FOURCC_INAM mmioFOURCC('I','N','A','M')
#define FOURCC_ISBJ mmioFOURCC('I','S','B','J')

BOOL   PreviewOpen(HWND hwnd);
BOOL   PreviewFile(HWND hwnd, LPTSTR szFile);
BOOL   PreviewPaint(HWND hwnd);
BOOL   PreviewClose(HWND hwnd);

HANDLE ReadDisp(LPTSTR lpszFile, int cf, LPTSTR pv, int iLen);
HANDLE ReadInfo(LPTSTR lpszFile, FOURCC fcc, LPTSTR pv, int iLen);
HANDLE GetRiffDisp(LPTSTR lpszFile, LPTSTR szText, int iLen);

/***************************************************************************
 *
 ****************************************************************************/

BOOL PreviewOpen(HWND hwnd)
{
    LOGFONT lf;
    UINT    w;

    if (hwndPreview)
        return FALSE;

    hwndPreview = hwnd;

    w = SetErrorMode(SEM_NOOPENFILEERRORBOX);
    hMSVideo = LoadLibrary(TEXT("MSVIDEO.DLL"));
    SetErrorMode(w);

#ifdef WIN32
    if (hMSVideo != NULL)
#else
    if (hMSVideo > HINSTANCE_ERROR)
#endif
    {
        pfnDrawDibOpen  = (PFNDRAWDIBOPEN)GetProcAddress(hMSVideo, ANSI_TEXT("DrawDibOpen"));
        pfnDrawDibClose = (PFNDRAWDIBCLOSE)GetProcAddress(hMSVideo, ANSI_TEXT("DrawDibClose"));
        pfnDrawDibDraw  = (PFNDRAWDIBDRAW)GetProcAddress(hMSVideo, ANSI_TEXT("DrawDibDraw"));

        if (pfnDrawDibOpen)
            hdd = pfnDrawDibOpen();
    }

    SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(lf), (LPVOID)&lf, 0);
    hfontPreview = CreateFontIndirect(&lf);
}

/***************************************************************************
 *
 ****************************************************************************/

BOOL PreviewClose(HWND hwnd)
{
    if (hwndPreview != hwnd)
        return FALSE;

    if (hdibPreview)
        GlobalFree(hdibPreview);

    if (hfontPreview)
        DeleteObject(hfontPreview);

    if (hdd)
        pfnDrawDibClose(hdd);

#ifdef WIN32
    if (hMSVideo != NULL)
#else
    if (hMSVideo >= HINSTANCE_ERROR)
#endif
        FreeLibrary(hMSVideo);

    achPreview[0] = 0;
    hdd           = NULL;
    hMSVideo      = NULL;
    hwndPreview   = NULL;
    hdibPreview   = NULL;
    hfontPreview  = NULL;
}

/***************************************************************************
 *
 ****************************************************************************/

BOOL PreviewFile(HWND hwnd, LPTSTR szFile)
{
    if (hwndPreview != hwnd)
        return FALSE;

    achPreview[0] = 0;

    if (hdibPreview)
        GlobalFree(hdibPreview);

    hdibPreview = NULL;

    if (szFile)
    {
        hdibPreview = GetRiffDisp(szFile, achPreview, BYTE_COUNT(achPreview));
    }

    PreviewPaint(hwnd);
    return TRUE;
}

/***************************************************************************
 *
 ****************************************************************************/

BOOL PreviewPaint(HWND hwnd)
{
    RECT    rc;
    RECT    rcPreview;
    RECT    rcImage;
    RECT    rcText;
    HDC     hdc;
    HBRUSH  hbr;
    int     dx;
    int     dy;
    LPBITMAPINFOHEADER lpbi;

    if (hwndPreview != hwnd)
        return FALSE;

    //
    // locate the preview in the lower corner of the dialog (below the
    // cancel button)
    //
    GetClientRect(hwnd, &rcPreview);
    GetWindowRect(GetDlgItem(hwnd, IDCANCEL), &rc);
    ScreenToClient(hwnd, (LPPOINT)&rc);
    ScreenToClient(hwnd, (LPPOINT)&rc+1);

    rcPreview.top   = rc.bottom + (rc.bottom - rc.top) + 12;
    rcPreview.left  = rc.left;
    rcPreview.right = rc.right;
    rcPreview.bottom -= 4;    // leave a little room at the bottom

    hdc = GetDC(hwnd);
#ifdef WIN32
    hbr = (HBRUSH)DefWindowProc(hwnd, WM_CTLCOLORDLG, (WPARAM)hdc, (LPARAM)hwnd);
#else
    hbr = (HBRUSH)DefWindowProc(hwnd, WM_CTLCOLOR, (WPARAM)hdc, MAKELONG(hwnd, CTLCOLOR_DLG));
#endif
    SelectObject(hdc, hfontPreview);
    SetStretchBltMode(hdc, COLORONCOLOR);

    InflateRect(&rcPreview, 4, 1);
    FillRect(hdc, &rcPreview, hbr);
    IntersectClipRect(hdc, rcPreview.left, rcPreview.top, rcPreview.right, rcPreview.bottom);
    InflateRect(&rcPreview, -4, -1);

    //
    // compute the text rect, using DrawText
    //
    rcText = rcPreview;
    rcText.bottom = rcText.top;

    DrawText(hdc, achPreview, -1, &rcText, DT_CALCRECT|DT_LEFT|DT_WORDBREAK);

    //
    // compute the image size
    //
    if (hdibPreview && hdd)
    {
        lpbi = GlobalLock(hdibPreview);

        //
        // DISP(CF_DIB) chunks are messed up they contain a DIB file! not
        // a CF_DIB, skip over the header if it is there.
        //
        if (lpbi->biSize != sizeof(BITMAPINFOHEADER))
            lpbi = (LPBITMAPINFOHEADER)((LPBYTE)lpbi + sizeof(BITMAPFILEHEADER));

        rcImage = rcPreview;

        //
        //  if wider than preview area scale to fit
        //
        if ((int)lpbi->biWidth > rcImage.right - rcImage.left)
        {
            rcImage.bottom = rcImage.top + MulDiv((int)lpbi->biHeight,rcImage.right-rcImage.left,(int)lpbi->biWidth);
        }
        //
        //  if x2 will fit then use it
        //
        else if ((int)lpbi->biWidth * 2 < rcImage.right - rcImage.left)
        {
            rcImage.right  = rcImage.left + (int)lpbi->biWidth*2;
            rcImage.bottom = rcImage.top + (int)lpbi->biHeight*2;
        }
        //
        //  else center the image in the preview area
        //
        else
        {
            rcImage.right  = rcImage.left + (int)lpbi->biWidth;
            rcImage.bottom = rcImage.top + (int)lpbi->biHeight;
        }

        if (rcImage.bottom > rcPreview.bottom - (rcText.bottom - rcText.top))
        {
            rcImage.bottom = rcPreview.bottom - (rcText.bottom - rcText.top);

            rcImage.right = rcPreview.left +
                              MulDiv((int)lpbi->biWidth,
                                     rcImage.bottom-rcImage.top,
                                     (int)lpbi->biHeight);
            rcImage.left = rcPreview.left;
        }
    }
    else
    {
        SetRectEmpty(&rcImage);
    }

    //
    //  now center
    //
    dx = ((rcPreview.right - rcPreview.left) - (rcText.right - rcText.left))/2;
    OffsetRect(&rcText, dx, 0);

    dx = ((rcPreview.right - rcPreview.left) - (rcImage.right - rcImage.left))/2;
    OffsetRect(&rcImage, dx, 0);

    dy  = rcPreview.bottom - rcPreview.top;
    dy -= rcImage.bottom - rcImage.top;
    dy -= rcText.bottom - rcText.top;

    if (dy < 0)
        dy = 0;
    else
        dy = dy / 2;

    OffsetRect(&rcImage, 0, dy);
    OffsetRect(&rcText, 0, dy + rcImage.bottom - rcImage.top + 2);

    //
    //  now draw
    //
    DrawText(hdc, achPreview, -1, &rcText, DT_LEFT|DT_WORDBREAK);

    if (hdibPreview && hdd)
    {
        pfnDrawDibDraw(hdd,
                       hdc,
                       rcImage.left,
                       rcImage.top,
                       rcImage.right  - rcImage.left,
                       rcImage.bottom - rcImage.top,
                       lpbi,
                       NULL,
                       0,
                       0,
                       -1,
                       -1,
                       0);

        InflateRect(&rcImage, 1, 1);
        FrameRect(hdc, &rcImage, GetStockObject(BLACK_BRUSH));
    }

    ReleaseDC(hwnd, hdc);
    return TRUE;
}

/***************************************************************************
 *
 ****************************************************************************/

static UINT (FAR PASCAL *lpfnOldHook)(HWND, unsigned, WPARAM, LPARAM);

    /* Combo boxes */
#define cmb1        0x0470
#define cmb2        0x0471
    /* Listboxes */
#define lst1        0x0460
#define lst2        0x0461
    /* Edit controls */
#define edt1        0x0480

#define ID_TIMER    1234
#define PREVIEWWAIT 1000

UINT FAR PASCAL _EXPORT GetFileNamePreviewHook(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
{
    int i;
    TCHAR ach[80];
    UINT Code;

    switch (msg) {
        case WM_COMMAND:
            Code = GET_WM_COMMAND_CMD(wParam,lParam);
            switch (LOWORD(wParam)) {
                case lst1:
                    if (Code == LBN_SELCHANGE)
                    {
                        KillTimer(hwnd, ID_TIMER);
                        SetTimer(hwnd, ID_TIMER, PREVIEWWAIT, NULL);
                    }
                    break;

                case IDOK:
                case IDCANCEL:
                    KillTimer(hwnd, ID_TIMER);
                    PreviewFile(hwnd, NULL);
                    break;

                case cmb1:
                case cmb2:
                case lst2:
                    if (Code == LBN_SELCHANGE)
                    {
                        KillTimer(hwnd, ID_TIMER);
                        PreviewFile(hwnd, NULL);
                    }
                    break;
            }
            break;

        case WM_TIMER:
            if (wParam == ID_TIMER)
            {
                KillTimer(hwnd, ID_TIMER);

                ach[0] = 0;
                i = (int)SendDlgItemMessage(hwnd, lst1, LB_GETCURSEL, 0, 0L);
                SendDlgItemMessage(hwnd, lst1, LB_GETTEXT, i, (LONG)(LPTSTR)ach);
                PreviewFile(hwnd, ach);
                return TRUE;
            }
            break;

        case WM_QUERYNEWPALETTE:
        case WM_PALETTECHANGED:
        case WM_PAINT:
            PreviewPaint(hwnd);
            break;

        case WM_INITDIALOG:
            PreviewOpen(hwnd);
            break;

        case WM_DESTROY:
            PreviewClose(hwnd);
            break;
    }

    if (lpfnOldHook)
        return (*lpfnOldHook)(hwnd, msg, wParam, lParam);
    else
        return FALSE;
}

/***************************************************************************
 *
 ****************************************************************************/

BOOL FAR PASCAL GetOpenFileNamePreview(LPOPENFILENAME lpofn)
{
    BOOL fHook;
    BOOL f;

    if (hwndPreview)
        return GetOpenFileName(lpofn);

    fHook = (BOOL)(lpofn->Flags & OFN_ENABLEHOOK);

    if (fHook)
        lpfnOldHook = lpofn->lpfnHook;

    lpofn->lpfnHook = (LPOFNHOOKPROC)MakeProcInstance((FARPROC)GetFileNamePreviewHook, GetHInstance());
    lpofn->Flags |= OFN_ENABLEHOOK;

    f = GetOpenFileName(lpofn);

#ifndef WIN32
    FreeProcInstance((FARPROC)lpofn->lpfnHook);
#endif

    if (fHook)
        lpofn->lpfnHook = lpfnOldHook;
    else
        lpofn->Flags &= ~OFN_ENABLEHOOK;

    return f;
}

/****************************************************************************
 *
 *  get both the DISP(CF_DIB) and the DISP(CF_TEXT) info in one pass, this is
 *  much faster than doing multiple passes over the file.
 *
 ****************************************************************************/

HANDLE GetRiffDisp(LPTSTR lpszFile, LPTSTR szText, int iLen)
{
    HMMIO       hmmio;
    MMCKINFO    ck;
    MMCKINFO    ckINFO;
    MMCKINFO    ckRIFF;
    HANDLE      h = NULL;
    LONG        lSize;
    DWORD       dw;
    HCURSOR     hcur = NULL;

    if (szText)
        szText[0] = 0;

    /* 20/9/92 - reverse engineering... (Laurie Griffiths)
    |  This routine ACTUALLY only ever gets called from PreviewFile.  That's
    |  in this file, so this routine should have been PRIVATE or STATIC.
    |  PreviewFile is itself called from only 3 places, also all in this file.
    |  Of these, two have NULL as the lpszFile (which is passed straight through
    |  to here) and the other one has text derived from
    |           SendDlgItemMessage(..., LB_GETTEXT, ...lpszFile)
    |  So if HIWORD(lpszFile) is ever zero, it's hard to see what good it does
    |  to take the LOWORD and cast it as an HMMIO (which would be even sillier
    |  in 32 bit land).  I reckon it will just always return.
    */

#ifdef WIN32
    if (lpszFile == NULL) return NULL;
    hmmio = mmioOpen(lpszFile, NULL, MMIO_ALLOCBUF | MMIO_READ);

#else   // 16 bit idio[syncra]cy?
    /* Open the file */
    if (HIWORD(lpszFile))
        hmmio = mmioOpen(lpszFile, NULL, MMIO_ALLOCBUF | MMIO_READ);
    else
        hmmio = (HMMIO)LOWORD(lpszFile);
#endif

    if (hmmio == NULL)
        return NULL;

    mmioSeek(hmmio, 0, SEEK_SET);

    /* descend the input file into the RIFF chunk */
    if (mmioDescend(hmmio, &ckRIFF, NULL, 0) != 0)
        goto error;

    if (ckRIFF.ckid != FOURCC_RIFF)
        goto error;

    while (!mmioDescend(hmmio, &ck, &ckRIFF, 0))
    {
        if (ck.ckid == FOURCC_DISP)
        {
            if (hcur == NULL)
                hcur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            /* Read dword into dw, break if read unsuccessful */
            if (mmioRead(hmmio, (LPVOID)&dw, sizeof(dw)) != sizeof(dw))
                goto error;

            /* Find out how much memory to allocate */
            lSize = ck.cksize - sizeof(dw);

            if ((int)dw == CF_DIB && h == NULL)
            {
                /* get a handle to memory to hold the description and lock it down */
                if ((h = GlobalAlloc(GHND, lSize+4)) == NULL)
                    goto error;

                if (mmioRead(hmmio, GlobalLock(h), lSize) != lSize)
                    goto error;
            }
            else if ((int)dw == CF_TEXT && szText[0] == 0)
            {
                if (lSize > iLen-1)
                    lSize = iLen-1;

                szText[lSize] = 0;

                if (mmioRead(hmmio, (LPSTR)szText, lSize) != lSize)
                    goto error;
            }
        }
        else if (ck.ckid    == FOURCC_LIST &&
                 ck.fccType == FOURCC_INFO &&
                 szText[0]  == 0)
        {
            while (!mmioDescend(hmmio, &ckINFO, &ck, 0))
            {
                switch (ckINFO.ckid)
                {
                    case FOURCC_INAM:
//                  case FOURCC_ISBJ:

                        lSize = ck.cksize;

                        if (lSize > iLen-1)
                            lSize = iLen-1;

                        szText[lSize] = 0;

                        if (mmioRead(hmmio, (LPSTR)szText, lSize) != lSize)
                            goto error;

                        break;
                }

                if (mmioAscend(hmmio, &ckINFO, 0))
                    break;
            }
        }

        //
        // if we have both a picture and a title, then exit.
        //
        if (h != NULL && szText[0] != 0)
            break;

        /* Ascend so that we can descend into next chunk
         */
        if (mmioAscend(hmmio, &ck, 0))
            break;
    }

    goto exit;

error:
    if (h)
        GlobalFree(h);
    h = NULL;

exit:
#ifdef WIN32
    if (hmmio != NULL)
        mmioClose(hmmio, 0);
#else
    if (hmmio && HIWORD(lpszFile))
        mmioClose(hmmio, 0);
#endif

    if (hcur)
        SetCursor(hcur);

    return h;
}

#if 0
*
* /***************************************************************************
*  *
*  ****************************************************************************/
*
* HANDLE FAR PASCAL GetRiffPicture(LPTSTR szFile)
* {
*     return ReadDisp(szFile, CF_DIB, NULL, 0);
* }
*
* * /***************************************************************************
* *  *
* *  ****************************************************************************/
* *
* * BOOL FAR PASCAL GetRiffTitle(LPSTR szFile, LPSTR szTitle, int iLen)
* * {
* *     return ReadDisp(szFile, CF_TEXT,     szTitle, iLen) ||
* *            ReadInfo(szFile, FOURCC_INAM, szTitle, iLen) ||
* *            ReadInfo(szFile, FOURCC_ISBJ, szTitle, iLen) ;
* * }
* /****************************************************************************
*  *
*  ****************************************************************************/
*
* HANDLE ReadDisp(LPSTR lpszFile, int cf, LPSTR pv, int iLen)
* {
*     HMMIO       hmmio;
*     MMCKINFO    ckinfo;
*     MMCKINFO    ckRIFF;
*     HANDLE      h = NULL;
*     DWORD       dwSize;
*     DWORD       dw;
*
*     if (pv)
*         ((LPSTR)pv)[0] = 0;
*
*     /* Open the file */
*     if (HIWORD(lpszFile))
*         hmmio = mmioOpen(lpszFile, NULL, MMIO_ALLOCBUF | MMIO_READ);
*     else
*         hmmio = (HMMIO)LOWORD(lpszFile);
*
*     if (hmmio == NULL)
*         return NULL;
*
*     mmioSeek(hmmio, 0, SEEK_SET);
*
*     /* descend the input file into the RIFF chunk */
*     if (mmioDescend(hmmio, &ckRIFF, NULL, 0) != 0)
*         goto error;
*
*     if (ckRIFF.ckid != FOURCC_RIFF)
*         goto error;
*
*     /* search the file for a 'DISP' chunk */
*     ckinfo.ckid = FOURCC_DISP;
*     while (!mmioDescend(hmmio, &ckinfo, &ckRIFF, MMIO_FINDCHUNK))
*     {
*         /* Read dword into dw, break if read unsuccessful */
*         if (mmioRead(hmmio, (LPVOID)&dw, sizeof(dw)) != sizeof(dw))
*             goto error;
*
*         /* Check to see if the type is right */
*         if ((int)dw == cf)
*         {
*             /* Find out how much memory to allocate */
*             dwSize = ckinfo.cksize - sizeof(dw);
*
*             if (pv == NULL)
*             {
*                 /* get a handle to memory to hold the description and lock it down */
*                 h = GlobalAlloc(GHND, dwSize+1);
*
*                 if (!h)
*                     goto error;
*
*                 pv = GlobalLock(h);
*             }
*             else
*             {
*                 if (dwSize > (DWORD)iLen-1)
*                     dwSize = (DWORD)iLen-1;
*
*                 ((LPSTR)pv)[(int)dwSize] = 0;
*             }
*
*             if (mmioRead(hmmio, pv, (LONG)dwSize) != (LONG)dwSize)
*                 goto error;
*
*             if (HIWORD(lpszFile))
*                 mmioClose(hmmio, 0);
*
*             return h ? h : (HANDLE)(UINT)dwSize;
*         }
*
*         /* Ascend so that we can descend into next chunk
*          */
*         if(mmioAscend(hmmio, &ckinfo, 0))
*             break;
*     }
*
* error:
*     if (hmmio && HIWORD(lpszFile))
*         mmioClose(hmmio, 0);
*
*     if (h)
*         GlobalFree(h);
*
*     return NULL;
* }
*
* /****************************************************************************
*  *
*  ****************************************************************************/
*
* HANDLE ReadInfo(LPSTR lpszFile, FOURCC fcc, LPSTR pv, int iLen)
* {
*     HMMIO       hmmio;
*     HANDLE      h=NULL;
*     MMCKINFO    ckinfo;
*     MMCKINFO    ckLIST;
*     MMCKINFO    ckRIFF;
*     DWORD       dwSize;
*
*     if (pv)
*         ((LPSTR)pv)[0] = 0;
*
*     /* Open the file */
*     if (HIWORD(lpszFile))
*         hmmio = mmioOpen(lpszFile, NULL, MMIO_ALLOCBUF | MMIO_READ);
*     else
*         hmmio = (HMMIO)LOWORD(lpszFile);
*
*     if (hmmio == NULL)
*         return NULL;
*
*     mmioSeek(hmmio, 0, SEEK_SET);
*
*     /* descend the input file into the RIFF chunk */
*     if (mmioDescend(hmmio, &ckRIFF, NULL, 0) != 0)
*         goto error;
*
*     if (ckRIFF.ckid != FOURCC_RIFF)
*         goto error;
*
*     ckLIST.fccType = FOURCC_INFO;
*     if (mmioDescend(hmmio, &ckLIST, &ckRIFF, MMIO_FINDLIST) != 0)
*         goto error;
*
*     ckinfo.ckid = fcc;
*     if (mmioDescend(hmmio, &ckinfo, &ckLIST, MMIO_FINDCHUNK) != 0)
*         goto error;
*
*     /* Find out how much memory to allocate/read */
*     dwSize = ckinfo.cksize;
*
*     if (pv == NULL)
*     {
*         /* get a handle to memory to hold the description and lock it down */
*         h = GlobalAlloc(GHND, dwSize+1);
*
*         if (!h)
*             goto error;
*
*         pv = GlobalLock(h);
*     }
*     else
*     {
*         if (dwSize > (DWORD)iLen-1)
*             dwSize = (DWORD)iLen-1;
*
*         ((LPSTR)pv)[(int)dwSize] = 0;
*     }
*
*     /* read the description into the allocated memory */
*     if (mmioRead(hmmio, pv, (LONG)dwSize) != (LONG)dwSize)
*         goto error;
*
*     if (HIWORD(lpszFile))
*         mmioClose(hmmio, 0);
*
*     return h ? h : (HANDLE)(UINT)dwSize;
*
* error:
*     if (hmmio && HIWORD(lpszFile))
*         mmioClose(hmmio, 0);
*
*     if (h)
*         GlobalFree(h);
*
*     return NULL;
* }
*
#endif
