/******************************Module*Header*******************************\
* Module Name: wndstuff.c
*
* This file contains the code to support a simple window that has
* a menu with a single item called "Test". When "Test" is selected
* vTest(HWND) is called.
*
* Created: 09-Dec-1992 10:44:31
* Author: Kirk Olynyk [kirko]
*
* Copyright (c) 1991 Microsoft Corporation
*
\**************************************************************************/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include "wndstuff.h"

HANDLE  ghInstance;
HWND    ghwndMain;
HBRUSH  ghbrWhite;

/***************************************************************************\
* main(argc, argv[])
*
* Sets up the message loop.
*
* History:
*  04-07-91 -by- KentD
* Wrote it.
\***************************************************************************/

_cdecl
main(
    INT   argc,
    PCHAR argv[])
{
    MSG    msg;
    HANDLE haccel;

    DONTUSE(argc);
    DONTUSE(argv);

    ghInstance = GetModuleHandle(NULL);

    if (!bInitApp())
    {
        return(0);
    }

    haccel = LoadAccelerators(ghInstance, MAKEINTRESOURCE(1));
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, haccel, &msg))
        {
             TranslateMessage(&msg);
             DispatchMessage(&msg);
        }
    }
    return(1);
}

/***************************************************************************\
* bInitApp()
*
* Initializes app.
*
* History:
*  04-07-91 -by- KentD
* Wrote it.
\***************************************************************************/

BOOL bInitApp(VOID)
{
    WNDCLASS wc;

    ghbrWhite = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));

    wc.style            = 0;
    wc.lpfnWndProc      = lMainWindowProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghInstance;
    wc.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = ghbrWhite;
    wc.lpszMenuName     = "MainMenu";
    wc.lpszClassName    = "TestClass";
    if (!RegisterClass(&wc))
    {
        return(FALSE);
    }
    ghwndMain =
      CreateWindowEx(
        0,
        "TestClass",
        "Win32 Test",
        MY_WINDOWSTYLE_FLAGS,
        80,
        70,
        400,
        80,
        NULL,
        NULL,
        ghInstance,
        NULL
        );
    if (ghwndMain == NULL)
    {
        return(FALSE);
    }
    SetFocus(ghwndMain);
    return(TRUE);
}

/***************************************************************************\
* lMainWindowProc(hwnd, message, wParam, lParam)
*
* Processes all messages for the main window.
*
* History:
*  04-07-91 -by- KentD
* Wrote it.
\***************************************************************************/

LONG
lMainWindowProc(
    HWND    hwnd,
    UINT    message,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    if (hwnd == ghwndMain)
    {
        switch (message)
        {
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
            case MM_DELETE:         vDelete(hwnd);           break;
            case MM_CONSTANTALPHA:  vConstantAlpha(hwnd);    break;
            case MM_PERPIXELALPHA:  vPerPixelAlpha(hwnd);    break;
            case MM_OPAQUE1:        vOpaque1(hwnd);          break;
            case MM_COLORKEY1:      vColorKey1(hwnd);        break;
            case MM_FLIP:           vFlip(hwnd);             break;
            case MM_MINIMIZE:       vMinimize(hwnd);         break;
            case MM_FADE:           vFade(hwnd);             break;
            case MM_BZZT:           vBzzt(hwnd);             break;
            case MM_WIPE1:          vWipe1(hwnd);            break;
            case MM_WIPE2:          vWipe2(hwnd);            break;
            case MM_WIPE3:          vWipe3(hwnd);            break;
            case MM_WIPE4:          vWipe4(hwnd);            break;
            case MM_TIMEREFRESH:    vTimeRefresh(hwnd);      break;
            }
            break;
    
        case WM_MOVE:
            vMove((short) LOWORD(lParam), (short) HIWORD(lParam));
            break;
    
        case WM_DESTROY:
    
            DeleteObject(ghbrWhite);
            PostQuitMessage(0);
            return(DefWindowProc(hwnd, message, wParam, lParam));
    
        default:
    
            return(DefWindowProc(hwnd, message, wParam, lParam));
        }
    }
    else
    {
        return(DefWindowProc(hwnd, message, wParam, lParam));
    }

    return(0);
}
