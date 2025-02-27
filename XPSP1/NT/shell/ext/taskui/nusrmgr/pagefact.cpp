// PageFact.cpp: implementation of the CPageFactory class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PageFact.h"
#include "MainPage.h"
#include "UserPage.h"
#include "LogonPage.h"

//////////////////////////////////////////////////////////////////////
// ITaskPageFactory
//////////////////////////////////////////////////////////////////////

BEGIN_PAGE_MAP(CPageFactory)
    PAGE_ENTRY(CMainPage)
    PAGE_ENTRY(CUserPage)
    PAGE_ENTRY(CLogonPage)
END_PAGE_MAP()

