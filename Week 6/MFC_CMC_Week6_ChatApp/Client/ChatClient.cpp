// ChatClient.cpp : diem vao cua ung dung client
#include "pch.h"
#include "framework.h"
#include "ChatClient.h"
#include "Theme.h"
#include "LoginDlg.h"
#include "MainDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CChatClientApp, CWinApp)
END_MESSAGE_MAP()

CChatClientApp theApp;

BOOL CChatClientApp::InitInstance()
{
	SetProcessDPIAware();                    // tu ve theo DPI that -> chu khong bi mo khi Windows scale 125%/150%

	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
	InitCommonControlsEx(&icc);
	CWinApp::InitInstance();
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);   // TSF (bo go tieng Viet) trong RichEdit can COM
	AfxInitRichEdit5();                      // nap MSFTEDIT.DLL cho o nhap tin (RICHEDIT50W)

	if (!AfxSocketInit())                    // WSAStartup
	{
		AfxMessageBox(_T("Không khởi động được Winsock."));
		return FALSE;
	}

	SetRegistryKey(_T("CMC"));               // luu server / username lan truoc vao HKCU\Software\CMC
	theme::InitDpi();
	CGdiplusInit gdiplus;

	// Dang nhap -> chat. Dang xuat / mat ket noi thi quay lai dang nhap
	// Khong gan m_pMainWnd, neu khong MFC thoat app khi dialog dau dong
	for (;;)
	{
		CLoginDlg login;
		if (login.DoModal() != IDOK)
			break;

		CMainDlg main;
		INT_PTR r = main.DoModal();
		m_net.Disconnect();
		if (r != IDRETRY)
			break;
	}

	m_net.Disconnect();
	theme::ClearAvatarImages();              // xoa anh truoc khi tat GDI+
	CoUninitialize();
	return FALSE;
}
