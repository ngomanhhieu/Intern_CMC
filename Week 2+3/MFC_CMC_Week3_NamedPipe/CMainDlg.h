#pragma once
#include <vector>
#include <map>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

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

// Message do thread mang gui sang giao dien (UI thread)
#define WM_APP_RECV      (WM_APP + 1)   // lParam = RecvMsg*
#define WM_APP_SENDFAIL  (WM_APP + 2)   // lParam = CString* (ten nguoi nhan)

// Mot tin nhan nhan duoc tu Named Pipe
struct RecvMsg
{
	CString sender;
	CString text;
};

// Mot tin nhan dang cho gui di
struct OutMsg
{
	CString peer;
	CString text;
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
	afx_msg void OnDestroy();
	afx_msg LRESULT OnAppRecv(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAppSendFail(WPARAM wParam, LPARAM lParam);
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

	// ---- Named Pipe ----
	bool m_startFailed = false;          // true neu khong tao duoc pipe (da dang nhap o cua so khac)
	bool m_netRunning = false;
	HANDLE m_hPipe = INVALID_HANDLE_VALUE;   // pipe server cua minh: \\.\pipe\CMCChat_<ten>
	std::thread m_recvThread;            // cho client ket noi + doc tin
	std::thread m_sendThread;            // gui tin theo thu tu
	std::mutex m_outMutex;
	std::condition_variable m_outCv;
	std::deque<OutMsg> m_outQueue;
	bool m_failBoxShown = false;

	bool StartNetwork();
	void StopNetwork();
	void SendLoop();
	int AddAvatar(const CString& name);
	void AddMessage(const CString& peer, const CString& text, bool isMe);
	void AppendRows(const ChatMessage& msg);
	void SelectPeer(int index);
	void UpdatePeerLabel(int index);

	afx_msg void OnClickedBtnSend();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnLvnItemchangedListUsers(NMHDR* pNMHDR, LRESULT* pResult);
};