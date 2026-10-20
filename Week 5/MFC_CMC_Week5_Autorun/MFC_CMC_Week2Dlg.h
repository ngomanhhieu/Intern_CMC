#pragma once
#include "CMainDlg.h"

class CMFCCMCWeek2Dlg : public CDialogEx
{
public:
	CMFCCMCWeek2Dlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFC_CMC_WEEK2_DIALOG };
#endif

protected:
	HICON m_hIcon;

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	DECLARE_MESSAGE_MAP()

public:
	CString m_strUsername;
	CString m_strPassword;

	afx_msg void OnClickedBtnLogin();
	afx_msg void OnClickedBtnRegister();
	afx_msg void OnClickedCheckAutorun();
};

// ---- Autorun: HKCU\Software\Microsoft\Windows\CurrentVersion\Run ----
bool IsAutorunEnabled();
bool SetAutorun(bool enable);