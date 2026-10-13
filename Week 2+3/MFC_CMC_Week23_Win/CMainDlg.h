#pragma once
#include <vector>
#include <map>

struct ChatMessage
{
	CString text;
	bool isMe;
	CTime time;
};

struct ChatRow
{
	int lineIndex;
	int lineCount;
	bool isMe;
	bool isTime;
	CString text;
	CString timeStr;
};

struct Account
{
	LPCTSTR user;
	LPCTSTR pass;
};

static const Account g_accounts[] = {
	{ _T("hieu"), _T("123") },
	{ _T("mai"),  _T("123") },
	{ _T("nam"),  _T("123") }
};
static const int ACCOUNT_COUNT = 3;

class CMainDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CMainDlg)

public:
	CMainDlg(CWnd* pParent = nullptr);
	virtual ~CMainDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MAIN_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
	DECLARE_MESSAGE_MAP()

public:
	CListCtrl m_listChat;
	CListCtrl m_listUsers;
	CString m_strMessage;

	CString m_username;
	CString m_currentPeer;
	std::vector<CString> m_peers;
	std::vector<bool> m_unread;
	std::vector<ChatRow> m_rows;
	std::map<CString, std::vector<ChatMessage>> m_history;

	CTime m_lastTime;
	bool m_hasLastTime = false;

	CImageList m_imgListHeight;
	CImageList m_imgListUsers;
	CFont m_fontHeader;
	CFont m_fontList;
	CFont m_fontSmall;

	static HWND FindChatWindow(const CString& user, HWND exclude);
	int AddAvatar(const CString& name);
	void AddMessage(const CString& peer, const CString& text, bool isMe);
	void AppendRows(const ChatMessage& msg);
	void SelectPeer(int index);
	void UpdatePeerLabel(int index);

	afx_msg void OnClickedBtnSend();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
	afx_msg void OnLvnItemchangedListUsers(NMHDR* pNMHDR, LRESULT* pResult);
};