// MainDlg.h : man hinh chat chinh
#pragma once
#include <deque>
#include "ThemedDialog.h"
#include "ContactList.h"
#include "ChatView.h"
#include "ComposerEdit.h"

class CMainDlg : public CThemedDialog
{
public:
	CMainDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MAIN };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK() {}           // Enter khong dong cua so
	virtual void OnCancel() {}       // Esc khong dong cua so
	virtual void PaintContent(CDC& dc, const CRect& client);

	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnTimer(UINT_PTR id);
	afx_msg void OnClickedSend();
	afx_msg void OnClickedLogout();
	afx_msg void OnSelChangeContacts();
	afx_msg void OnChangeSearch();
	afx_msg void OnChangeComposer();
	afx_msg void OnComposerResize(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnComposerRecalc(WPARAM, LPARAM);
	afx_msg LRESULT OnNetEvent(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()

private:
	void Layout();
	void RebuildList();                      // loc theo o tim kiem + sap xep, giu nguyen dong dang chon
	void SelectContact(int idx);
	int FindContact(const CString& user);
	int FindOrAddContact(const CString& user, const CString& display);
	void InvalidateHeader();
	void UpdateComposerHeight();             // o nhap tin cao theo so dong dang hien (ke ca chu bo go dang soan)

	void HandleUsers(const json::Value& body);
	void HandleMessages(const json::Value& body);
	void HandleChat(const json::Value& body);
	void HandleUserStatus(const json::Value& body);
	void HandleAvatar(const json::Value& body);
	CRect MyAvatarRect() const;
	void ChangeMyAvatar();
	void HandleTyping(const json::Value& body);
	void HandleSeen(const json::Value& body);
	void NotifyTyping();

	static const UINT_PTR TIMER_TICK = 1;
	ULONGLONG m_lastTypingSent = 0;

	std::deque<Contact> m_contacts;          // deque: them phan tu khong lam hong con tro cu (ChatView giu con tro)
	int m_current = -1;                      // chi so nguoi dang chat trong m_contacts

	CContactList m_list;
	CChatView m_chat;
	CFont m_fontName;
	CFont m_fontListAvatar;
	CFont m_fontSmallBold;

	int m_sidebar = 0;                       // do rong cot trai (pixel that)
	int m_header = 0;                        // chieu cao thanh tieu de
	int m_composer = 0;                      // chieu cao vung nhap tin (tu cao len theo so dong dang go)
	CComposerEdit m_input;                   // o nhap tin (RichEdit, ho tro bo go tieng Viet)
	int m_inputLineH = 0;                    // chieu cao 1 dong trong o nhap (do tu RichEdit)
};
