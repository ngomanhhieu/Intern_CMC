
// MFC_CMC_Week2.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CMFCCMCWeek2App:
// See MFC_CMC_Week2.cpp for the implementation of this class
//

class CMFCCMCWeek2App : public CWinApp
{
public:
	CMFCCMCWeek2App();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CMFCCMCWeek2App theApp;
