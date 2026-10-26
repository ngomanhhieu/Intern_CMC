// LoginDlg.h : man hinh dang nhap
#pragma once
#include "ThemedDialog.h"

class CLoginDlg : public CThemedDialog
{
public:
	CLoginDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LOGIN };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void PaintContent(CDC& dc, const CRect& client);
	afx_msg void OnClickedLogin();
	afx_msg void OnClickedGoRegister();
	afx_msg LRESULT OnNetEvent(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()

private:
	void SetStatus(const CString& text, bool error);
	void SetBusy(bool busy);

	CString m_status;
	bool m_statusError = false;
	bool m_busy = false;
};
