
// Task1_CMC.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CTask1CMCApp:
// See Task1_CMC.cpp for the implementation of this class
//

class CTask1CMCApp : public CWinApp
{
public:
	CTask1CMCApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CTask1CMCApp theApp;
