// ChatClient.h : lop ung dung
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"
#include "NetClient.h"

class CChatClientApp : public CWinApp
{
public:
	virtual BOOL InitInstance();

	CNetClient m_net;        // 1 ket noi toi server, dung chung cho man hinh dang nhap va man hinh chat
	CString m_user;          // username sau khi dang nhap
	CString m_display;       // ten hien thi

	DECLARE_MESSAGE_MAP()
};

extern CChatClientApp theApp;
