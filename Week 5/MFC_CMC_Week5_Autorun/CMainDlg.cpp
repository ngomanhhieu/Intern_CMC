// CMainDlg.cpp : implementation file
#include "pch.h"
#include "MFC_CMC_Week2.h"
#include "afxdialogex.h"
#include "CMainDlg.h"
#include <atlimage.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")

#define CHAT_TITLE_PREFIX _T("CMC Chat - ")
#define ROW_HEIGHT        26
#define BUBBLE_WIDTH      220
#define BUBBLE_PAD        10
#define TIME_GAP_MINUTES  5
#define TIME_GUTTER       44



static const DWORD MAX_MSG_BYTES = 4 * 1024 * 1024;
static std::atomic<bool> g_stop(false);

// CString (UTF-16) <-> byte: gui thang bo nho wchar_t, khong doi bang ma nao.
// Hai ben deu la app Windows Unicode nen hieu nhau; tieng Viet co dau giu nguyen.
static std::string ToBytes(const CString& s)
{
	return std::string((const char*)(LPCTSTR)s, s.GetLength() * sizeof(TCHAR));
}

static CString FromBytes(const char* p, int bytes)
{
	CString s;
	int n = bytes / (int)sizeof(TCHAR);
	if (n <= 0) return s;
	TCHAR* buf = s.GetBuffer(n);
	memcpy(buf, p, n * sizeof(TCHAR));
	s.ReleaseBuffer(n);
	return s;
}

// Tach "nguoigui\tnoidung"
static bool SplitMessage(const std::string& data, CString& sender, CString& text)
{
	CString all = FromBytes(data.data(), (int)data.size());
	int p = all.Find(_T('\t'));
	if (p <= 0) return false;
	sender = all.Left(p);
	text = all.Mid(p + 1);
	return true;
}

// Dia chi cua mot tai khoan, doc tu peers.ini canh file exe.
// Mac dinh: 127.0.0.1 va cong BASE_PORT + chi so tai khoan.
static void GetPeerAddr(const CString& user, CString& ip, int& port)
{
	int idx = 0;
	for (int i = 0; i < ACCOUNT_COUNT; i++)
		if (user == g_accounts[i].user) idx = i;

	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);
	CString ini(exePath);
	ini = ini.Left(ini.ReverseFind(_T('\\'))) + _T("\\peers.ini");

	TCHAR buf[128];
	GetPrivateProfileString(user, _T("ip"), _T("127.0.0.1"), buf, 128, ini);
	ip = buf;
	port = (int)GetPrivateProfileInt(user, _T("port"), BASE_PORT + idx, ini);
}

// Doc du len byte (tra false neu ngat ket noi hoac app dang dong)
static bool RecvAll(SOCKET s, char* buf, int len)
{
	int got = 0;
	while (got < len)
	{
		if (g_stop) return false;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		timeval tv = { 0, 500000 };
		int r = select(0, &fds, NULL, NULL, &tv);
		if (r == SOCKET_ERROR) return false;
		if (r == 0) continue;
		int n = recv(s, buf + got, len - got, 0);
		if (n <= 0) return false;
		got += n;
	}
	return true;
}

// Thread xu ly mot ket noi den: doc tung khung, day len UI thread
static void ClientThread(SOCKET s, HWND hwnd)
{
	for (;;)
	{
		unsigned char hdr[4];
		if (!RecvAll(s, (char*)hdr, 4)) break;
		DWORD len = ((DWORD)hdr[0] << 24) | ((DWORD)hdr[1] << 16) | ((DWORD)hdr[2] << 8) | (DWORD)hdr[3];
		if (len == 0 || len > MAX_MSG_BYTES) break;

		std::string data(len, '\0');
		if (!RecvAll(s, &data[0], (int)len)) break;

		RecvMsg* m = new RecvMsg;
		if (!SplitMessage(data, m->sender, m->text)) { delete m; continue; }
		if (!::PostMessage(hwnd, WM_APP_RECV, 0, (LPARAM)m))
			delete m;
	}
	closesocket(s);
}

// Thread lang nghe: accept ket noi moi
static void AcceptLoop(SOCKET listenSock, HWND hwnd)
{
	while (!g_stop)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(listenSock, &fds);
		timeval tv = { 0, 500000 };
		int r = select(0, &fds, NULL, NULL, &tv);
		if (r == SOCKET_ERROR) break;
		if (r == 0) continue;

		SOCKET c = accept(listenSock, NULL, NULL);
		if (c != INVALID_SOCKET)
			std::thread(ClientThread, c, hwnd).detach();
	}
}

// Ket noi toi peer, gui 1 khung roi dong
static bool SendOne(const CString& ip, int port, const std::string& payload)
{
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	CStringA ipA(ip);
	char portBuf[16];
	sprintf_s(portBuf, "%d", port);

	addrinfo* res = nullptr;
	if (getaddrinfo(ipA, portBuf, &hints, &res) != 0 || !res) return false;

	SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (s == INVALID_SOCKET) { freeaddrinfo(res); return false; }

	// connect co timeout 3 giay
	u_long nb = 1;
	ioctlsocket(s, FIONBIO, &nb);
	bool ok = false;
	if (connect(s, res->ai_addr, (int)res->ai_addrlen) == 0)
		ok = true;
	else if (WSAGetLastError() == WSAEWOULDBLOCK)
	{
		fd_set wf, ef;
		FD_ZERO(&wf); FD_ZERO(&ef);
		FD_SET(s, &wf); FD_SET(s, &ef);
		timeval tv = { 3, 0 };
		if (select(0, NULL, &wf, &ef, &tv) > 0 && FD_ISSET(s, &wf))
			ok = true;
	}
	freeaddrinfo(res);

	if (ok)
	{
		nb = 0;
		ioctlsocket(s, FIONBIO, &nb);

		DWORD len = (DWORD)payload.size();
		std::string buf;
		buf.push_back((char)((len >> 24) & 0xFF));
		buf.push_back((char)((len >> 16) & 0xFF));
		buf.push_back((char)((len >> 8) & 0xFF));
		buf.push_back((char)(len & 0xFF));
		buf += payload;

		int sent = 0, total = (int)buf.size();
		while (sent < total)
		{
			int n = send(s, buf.data() + sent, total - sent, 0);
			if (n <= 0) { ok = false; break; }
			sent += n;
		}
		shutdown(s, SD_SEND);
	}
	closesocket(s);
	return ok;
}

static void WrapText(CDC& dc, const CString& text, int maxWidth, std::vector<CString>& lines)
{
	lines.clear();
	CString line;
	int lastSpace = -1;

	for (int i = 0; i < text.GetLength(); i++)
	{
		TCHAR ch = text[i];
		if (ch == _T('\r')) continue;
		if (ch == _T('\n'))
		{
			lines.push_back(line);
			line.Empty();
			lastSpace = -1;
			continue;
		}

		line += ch;
		if (ch == _T(' ')) lastSpace = line.GetLength() - 1;

		if (dc.GetTextExtent(line).cx > maxWidth && line.GetLength() > 1)
		{
			if (lastSpace > 0)
			{
				lines.push_back(line.Left(lastSpace));
				line = line.Mid(lastSpace + 1);
			}
			else
			{
				lines.push_back(line.Left(line.GetLength() - 1));
				line = line.Right(1);
			}
			lastSpace = line.ReverseFind(_T(' '));
		}
	}
	if (!line.IsEmpty() || lines.empty())
		lines.push_back(line);
}

IMPLEMENT_DYNAMIC(CMainDlg, CDialogEx)

CMainDlg::CMainDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MAIN_DIALOG, pParent)
	, m_strMessage(_T(""))
{
}

CMainDlg::~CMainDlg()
{
	StopNetwork();
}

bool CMainDlg::StartNetwork()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
	g_stop = false;

	CString ip;
	int port;
	GetPeerAddr(m_username, ip, port);   // chi dung port cua chinh minh

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) return false;

	// Chiem cong doc quyen: mo lan 2 cung tai khoan se bi loi bind
	int yes = 1;
	setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&yes, sizeof(yes));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((u_short)port);
	if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR || listen(s, SOMAXCONN) == SOCKET_ERROR)
	{
		closesocket(s);
		return false;
	}

	m_listen = s;
	m_netRunning = true;
	m_acceptThread = std::thread(AcceptLoop, m_listen, m_hWnd);
	m_sendThread = std::thread(&CMainDlg::SendLoop, this);
	return true;
}

void CMainDlg::StopNetwork()
{
	if (!m_netRunning) return;
	m_netRunning = false;

	{
		std::lock_guard<std::mutex> lk(m_outMutex);
		g_stop = true;
	}
	m_outCv.notify_all();

	if (m_acceptThread.joinable()) m_acceptThread.join();
	if (m_sendThread.joinable()) m_sendThread.join();
	if (m_listen != INVALID_SOCKET)
	{
		closesocket(m_listen);
		m_listen = INVALID_SOCKET;
	}
}

// Thread gui: lay tin trong hang doi, gui lan luot de giu dung thu tu
void CMainDlg::SendLoop()
{
	for (;;)
	{
		OutMsg m;
		{
			std::unique_lock<std::mutex> lk(m_outMutex);
			m_outCv.wait(lk, [this] { return g_stop || !m_outQueue.empty(); });
			if (g_stop) return;
			m = m_outQueue.front();
			m_outQueue.pop_front();
		}

		std::string payload = ToBytes(m_username + _T("\t") + m.text);
		if (!SendOne(m.ip, m.port, payload))
			::PostMessage(m_hWnd, WM_APP_SENDFAIL, 0, (LPARAM)new CString(m.peer));
	}
}

void CMainDlg::OnDestroy()
{
	StopNetwork();
	CDialogEx::OnDestroy();
}

LRESULT CMainDlg::OnAppRecv(WPARAM, LPARAM lParam)
{
	RecvMsg* m = (RecvMsg*)lParam;
	if (!m) return 0;

	// Chi nhan tin tu tai khoan co trong danh sach
	for (size_t i = 0; i < m_peers.size(); i++)
	{
		if (m_peers[i] == m->sender)
		{
			AddMessage(m->sender, m->text, false);
			break;
		}
	}
	delete m;
	return 0;
}

LRESULT CMainDlg::OnAppSendFail(WPARAM, LPARAM lParam)
{
	CString* peer = (CString*)lParam;
	if (peer && !m_failBoxShown)
	{
		m_failBoxShown = true;
		MessageBox(_T("Không gửi được tin tới ") + *peer + _T(" (chưa online hoặc sai IP/cổng trong peers.ini)."),
			_T("Thông báo"), MB_ICONINFORMATION);
		m_failBoxShown = false;
	}
	delete peer;
	return 0;
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_CHAT, m_listChat);
	DDX_Control(pDX, IDC_LIST_USERS, m_listUsers);
	DDX_Text(pDX, IDC_EDIT_MESSAGE, m_strMessage);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SEND, &CMainDlg::OnClickedBtnSend)
	ON_WM_DRAWITEM()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_APP_RECV, &CMainDlg::OnAppRecv)
	ON_MESSAGE(WM_APP_SENDFAIL, &CMainDlg::OnAppSendFail)
	ON_WM_MOUSEACTIVATE()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_USERS, &CMainDlg::OnLvnItemchangedListUsers)
END_MESSAGE_MAP()

int CMainDlg::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	return MA_ACTIVATE;
}

int CMainDlg::AddAvatar(const CString& name)
{
	const int S = 44;

	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);
	CString dir(exePath);
	dir = dir.Left(dir.ReverseFind(_T('\\')));
	CString file = dir + _T("\\avatars\\") + name + _T(".png");

	CClientDC dcScreen(this);
	CDC dc;
	dc.CreateCompatibleDC(&dcScreen);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dcScreen, S, S);
	CBitmap* pOldBmp = dc.SelectObject(&bmp);

	dc.FillSolidRect(0, 0, S, S, RGB(255, 255, 255));

	CRgn rgn;
	rgn.CreateEllipticRgn(0, 0, S, S);
	dc.SelectClipRgn(&rgn);

	CImage img;
	if (SUCCEEDED(img.Load(file)))
	{
		dc.SetStretchBltMode(HALFTONE);
		img.Draw(dc.GetSafeHdc(), 0, 0, S, S);
	}
	else
	{
		dc.FillSolidRect(0, 0, S, S, RGB(0, 132, 255));
		dc.SetTextColor(RGB(255, 255, 255));
		dc.SetBkMode(TRANSPARENT);
		CFont* pOldFont = dc.SelectObject(&m_fontHeader);
		CString letter = name.Left(1);
		letter.MakeUpper();
		CRect r(0, 0, S, S);
		dc.DrawText(letter, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOldFont);
	}

	dc.SelectClipRgn(NULL);
	dc.SelectObject(pOldBmp);

	return m_imgListUsers.Add(&bmp, (CBitmap*)NULL);
}

BOOL CMainDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CString(CHAT_TITLE_PREFIX) + m_username);

	CRect rect;
	m_listChat.GetClientRect(&rect);
	m_listChat.InsertColumn(0, _T(""), LVCFMT_LEFT, rect.Width() - GetSystemMetrics(SM_CXVSCROLL) - 4);
	m_imgListHeight.Create(1, ROW_HEIGHT, ILC_COLOR32, 1, 1);
	m_listChat.SetImageList(&m_imgListHeight, LVSIL_SMALL);

	m_listUsers.SetExtendedStyle(m_listUsers.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	m_listUsers.GetClientRect(&rect);
	m_listUsers.InsertColumn(0, _T(""), LVCFMT_LEFT, rect.Width() - 4);
	m_imgListUsers.Create(44, 44, ILC_COLOR32, 3, 3);
	m_listUsers.SetImageList(&m_imgListUsers, LVSIL_SMALL);

	m_fontList.CreateFont(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));
	m_listUsers.SetFont(&m_fontList);

	m_fontHeader.CreateFont(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));
	GetDlgItem(IDC_STATIC_PEER)->SetFont(&m_fontHeader);

	m_fontSmall.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));

	for (int i = 0; i < ACCOUNT_COUNT; i++)
	{
		CString name = g_accounts[i].user;
		if (name == m_username) continue;

		m_peers.push_back(name);
		m_unread.push_back(false);
		int img = AddAvatar(name);
		m_listUsers.InsertItem((int)m_peers.size() - 1, name, img);
	}

	if (!m_peers.empty())
		m_listUsers.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	if (!StartNetwork())
	{
		MessageBox(_T("Không mở được cổng TCP để nhận tin. Tài khoản này có thể đang đăng nhập ở cửa sổ khác, hoặc cổng bị chiếm."),
			_T("Lỗi"), MB_ICONWARNING);
		m_startFailed = true;
		EndDialog(IDCANCEL);
	}

	return TRUE;
}

void CMainDlg::OnOK()
{
	if (::GetFocus() == GetDlgItem(IDC_EDIT_MESSAGE)->GetSafeHwnd())
		OnClickedBtnSend();
}

void CMainDlg::OnLvnItemchangedListUsers(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED))
		SelectPeer(pNMLV->iItem);
}

void CMainDlg::SelectPeer(int index)
{
	if (index < 0 || index >= (int)m_peers.size()) return;

	m_currentPeer = m_peers[index];
	m_unread[index] = false;
	UpdatePeerLabel(index);

	SetDlgItemText(IDC_STATIC_PEER, m_currentPeer);

	m_rows.clear();
	m_hasLastTime = false;
	m_listChat.DeleteAllItems();
	std::vector<ChatMessage>& hist = m_history[m_currentPeer];
	for (size_t i = 0; i < hist.size(); i++)
		AppendRows(hist[i]);
	if (m_listChat.GetItemCount() > 0)
		m_listChat.EnsureVisible(m_listChat.GetItemCount() - 1, FALSE);
}

void CMainDlg::UpdatePeerLabel(int index)
{
	CString label = m_peers[index];
	if (m_unread[index]) label += _T("  (mới)");
	m_listUsers.SetItemText(index, 0, label);
}

void CMainDlg::AppendRows(const ChatMessage& msg)
{
	if (!m_hasLastTime || (msg.time - m_lastTime).GetTotalMinutes() >= TIME_GAP_MINUTES)
	{
		CTime now = CTime::GetCurrentTime();
		bool today = (msg.time.GetYear() == now.GetYear()
			&& msg.time.GetMonth() == now.GetMonth()
			&& msg.time.GetDay() == now.GetDay());

		ChatRow tr;
		tr.lineIndex = 0;
		tr.lineCount = 1;
		tr.isMe = false;
		tr.isTime = true;
		tr.text = msg.time.Format(today ? _T("%H:%M") : _T("%H:%M  %d/%m/%Y"));
		m_rows.push_back(tr);
		m_listChat.InsertItem((int)m_rows.size() - 1, _T(""));
	}
	m_lastTime = msg.time;
	m_hasLastTime = true;

	CClientDC dc(&m_listChat);
	CFont* pFont = m_listChat.GetFont();
	CFont* pOldFont = pFont ? dc.SelectObject(pFont) : NULL;

	std::vector<CString> lines;
	WrapText(dc, msg.text, BUBBLE_WIDTH - 2 * BUBBLE_PAD, lines);

	if (pOldFont) dc.SelectObject(pOldFont);

	CString timeStr = msg.time.Format(_T("%H:%M"));
	for (int i = 0; i < (int)lines.size(); i++)
	{
		ChatRow row;
		row.lineIndex = i;
		row.lineCount = (int)lines.size();
		row.isMe = msg.isMe;
		row.isTime = false;
		row.text = lines[i];
		row.timeStr = timeStr;
		m_rows.push_back(row);
		m_listChat.InsertItem((int)m_rows.size() - 1, _T(""));
	}
}

void CMainDlg::AddMessage(const CString& peer, const CString& text, bool isMe)
{
	ChatMessage msg;
	msg.text = text;
	msg.isMe = isMe;
	msg.time = CTime::GetCurrentTime();
	m_history[peer].push_back(msg);

	if (peer == m_currentPeer)
	{
		AppendRows(msg);
		m_listChat.EnsureVisible(m_listChat.GetItemCount() - 1, FALSE);
	}
	else
	{
		for (int i = 0; i < (int)m_peers.size(); i++)
		{
			if (m_peers[i] == peer)
			{
				m_unread[i] = true;
				UpdatePeerLabel(i);
			}
		}
	}
}

void CMainDlg::OnClickedBtnSend()
{
	UpdateData(TRUE);
	if (m_strMessage.IsEmpty()) return;
	if (m_currentPeer.IsEmpty())
	{
		MessageBox(_T("Hãy chọn người để nhắn ở danh sách bên trái!"), _T("Thông báo"), MB_ICONINFORMATION);
		return;
	}

	CString text = m_strMessage;
	AddMessage(m_currentPeer, text, true);

	// Dua vao hang doi, thread gui se ket noi TCP toi peer
	OutMsg om;
	om.peer = m_currentPeer;
	om.text = text;
	GetPeerAddr(m_currentPeer, om.ip, om.port);
	{
		std::lock_guard<std::mutex> lk(m_outMutex);
		m_outQueue.push_back(om);
	}
	m_outCv.notify_one();

	m_strMessage = _T("");
	UpdateData(FALSE);
	GetDlgItem(IDC_EDIT_MESSAGE)->SetFocus();
}

void CMainDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl != IDC_LIST_CHAT)
	{
		CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
		return;
	}

	int index = lpDrawItemStruct->itemID;
	if (index < 0 || index >= (int)m_rows.size()) return;

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	int saved = dc.SaveDC();

	CRect rcItem(lpDrawItemStruct->rcItem);
	dc.FillSolidRect(rcItem, RGB(255, 255, 255));
	dc.IntersectClipRect(rcItem);

	ChatRow& row = m_rows[index];
	CFont* pFont = m_listChat.GetFont();
	if (pFont) dc.SelectObject(pFont);
	dc.SetBkMode(TRANSPARENT);

	if (row.isTime)
	{
		dc.SetTextColor(RGB(140, 140, 140));
		dc.DrawText(row.text, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.RestoreDC(saved);
		dc.Detach();
		return;
	}

	int rowH = rcItem.Height();

	CRect rcBubble;
	rcBubble.top = rcItem.top - row.lineIndex * rowH + 3;
	rcBubble.bottom = rcItem.top + (row.lineCount - row.lineIndex) * rowH - 3;
	if (row.isMe)
	{
		rcBubble.right = rcItem.right - TIME_GUTTER;
		rcBubble.left = rcBubble.right - BUBBLE_WIDTH;
	}
	else
	{
		rcBubble.left = rcItem.left + TIME_GUTTER;
		rcBubble.right = rcBubble.left + BUBBLE_WIDTH;
	}

	COLORREF bubbleColor = row.isMe ? RGB(0, 132, 255) : RGB(230, 230, 230);
	COLORREF textColor = row.isMe ? RGB(255, 255, 255) : RGB(0, 0, 0);

	CBrush brush(bubbleColor);
	CPen pen(PS_SOLID, 1, bubbleColor);
	dc.SelectObject(&brush);
	dc.SelectObject(&pen);
	dc.RoundRect(rcBubble, CPoint(16, 16));

	dc.SetTextColor(textColor);
	CRect rcText(rcBubble.left + BUBBLE_PAD, rcItem.top, rcBubble.right - BUBBLE_PAD, rcItem.bottom);
	dc.DrawText(row.text, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (row.lineIndex == row.lineCount - 1)
	{
		dc.SelectObject(&m_fontSmall);
		dc.SetTextColor(RGB(140, 140, 140));

		CRect rcTime = rcItem;
		if (row.isMe)
		{
			rcTime.left = rcItem.right - TIME_GUTTER;
			rcTime.right = rcItem.right - 4;
			dc.DrawText(row.timeStr, &rcTime, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		else
		{
			rcTime.left = rcItem.left + 4;
			rcTime.right = rcItem.left + TIME_GUTTER;
			dc.DrawText(row.timeStr, &rcTime, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
	}

	dc.RestoreDC(saved);
	dc.Detach();
}