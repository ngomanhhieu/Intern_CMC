// RegisterDlg.h : man hinh tao tai khoan
#pragma once
#include "ThemedDialog.h"
#include "NetClient.h"

class CRegisterDlg : public CThemedDialog
{
public:
	CRegisterDlg(const CString& server, int port, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REGISTER };
#endif

	CString m_registeredUser;    // dang ky thanh cong -> man hinh dang nhap dien san

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void PaintContent(CDC& dc, const CRect& client);
	afx_msg void OnClickedRegister();
	afx_msg void OnClickedBack();
	afx_msg LRESULT OnNetEvent(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()

private:
	void SetStatus(const CString& text, bool error);
	void SetBusy(bool busy);
	CString Validate(CString& user, CString& display, CString& pass);

	CNetClient m_net;            // ket noi rieng, chi dung de dang ky xong la dong
	CString m_server;
	int m_port;
	CString m_status;
	bool m_statusError = false;
	bool m_busy = false;
};
