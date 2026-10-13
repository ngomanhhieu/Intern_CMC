// CMainDlg.cpp : implementation file
#include "pch.h"
#include "MFC_CMC_Week2.h"
#include "afxdialogex.h"
#include "CMainDlg.h"
#include <atlimage.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic>
#include <tuple>
#pragma comment(lib, "ws2_32.lib")

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#define CHAT_TITLE_PREFIX _T("CMC Chat - ")
#define ROW_HEIGHT        26
#define BUBBLE_WIDTH      220
#define BUBBLE_PAD        10
#define TIME_GAP_MINUTES  5
#define TIME_GUTTER       44



static const int  CHUNK_BYTES = 1200;      // < MTU, tranh phan manh IP
static const int  HEADER_BYTES = 8;
static const int  MAX_CHUNKS = 256;        // toi da ~300 KB / tin
static const DWORD ASSEMBLE_TIMEOUT_MS = 5000;
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

struct Assembly
{
	std::vector<std::string> parts;
	std::vector<bool> have;
	int got = 0;
	DWORD tick = 0;
};

// Thread nhan: recvfrom, ghep manh, day tin hoan chinh len UI thread
static void RecvLoop(SOCKET s, HWND hwnd)
{
	typedef std::tuple<unsigned long, unsigned short, unsigned int> Key;
	std::map<Key, Assembly> pending;
	std::vector<char> buf(65536);

	while (!g_stop)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		timeval tv = { 0, 500000 };
		int r = select(0, &fds, NULL, NULL, &tv);

		// don cac tin ghep do dang qua han
		DWORD now = GetTickCount();
		for (auto it = pending.begin(); it != pending.end();)
		{
			if (now - it->second.tick > ASSEMBLE_TIMEOUT_MS) it = pending.erase(it);
			else ++it;
		}

		if (r == SOCKET_ERROR) break;
		if (r == 0) continue;

		sockaddr_in from = {};
		int fromLen = sizeof(from);
		int n = recvfrom(s, buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fromLen);
		if (n == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err == WSAECONNRESET || err == WSAEMSGSIZE) continue;
			break;
		}
		if (n < HEADER_BYTES + 1) continue;   // goi rac

		const unsigned char* d = (const unsigned char*)buf.data();
		unsigned int id = ((unsigned int)d[0] << 24) | ((unsigned int)d[1] << 16) | ((unsigned int)d[2] << 8) | d[3];
		int idx = (d[4] << 8) | d[5];
		int total = (d[6] << 8) | d[7];
		if (total < 1 || total > MAX_CHUNKS || idx >= total) continue;

		Key key(from.sin_addr.s_addr, from.sin_port, id);
		Assembly& as = pending[key];
		if (as.parts.empty())
		{
			as.parts.resize(total);
			as.have.assign(total, false);
			as.tick = now;
		}
		if ((int)as.parts.size() != total) { pending.erase(key); continue; }
		if (as.have[idx]) continue;           // manh bi gui trung

		as.parts[idx].assign((const char*)d + HEADER_BYTES, n - HEADER_BYTES);
		as.have[idx] = true;
		as.got++;
		if (as.got < total) continue;

		std::string data;
		for (int i = 0; i < total; i++) data += as.parts[i];
		pending.erase(key);

		RecvMsg* m = new RecvMsg;
		if (!SplitMessage(data, m->sender, m->text)) { delete m; continue; }
		if (!::PostMessage(hwnd, WM_APP_RECV, 0, (LPARAM)m))
			delete m;
	}
}

// Cat payload thanh cac datagram va sendto toi peer.
// UDP khong biet ben kia co nhan duoc hay khong; chi bao loi neu khong gui duoc o phia minh.
static bool SendDatagrams(SOCKET s, const CString& ip, int port, unsigned int msgId, const std::string& payload)
{
	int total = ((int)payload.size() + CHUNK_BYTES - 1) / CHUNK_BYTES;
	if (total < 1 || total > MAX_CHUNKS) return false;

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	CStringA ipA(ip);
	char portBuf[16];
	sprintf_s(portBuf, "%d", port);
	addrinfo* res = nullptr;
	if (getaddrinfo(ipA, portBuf, &hints, &res) != 0 || !res) return false;

	bool ok = true;
	for (int i = 0; i < total && ok; i++)
	{
		size_t off = (size_t)i * CHUNK_BYTES;
		size_t len = payload.size() - off;
		if (len > (size_t)CHUNK_BYTES) len = CHUNK_BYTES;

		std::string pkt;
		pkt.push_back((char)((msgId >> 24) & 0xFF));
		pkt.push_back((char)((msgId >> 16) & 0xFF));
		pkt.push_back((char)((msgId >> 8) & 0xFF));
		pkt.push_back((char)(msgId & 0xFF));
		pkt.push_back((char)((i >> 8) & 0xFF));
		pkt.push_back((char)(i & 0xFF));
		pkt.push_back((char)((total >> 8) & 0xFF));
		pkt.push_back((char)(total & 0xFF));
		pkt.append(payload, off, len);

		if (sendto(s, pkt.data(), (int)pkt.size(), 0, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR)
			ok = false;
		else if ((i & 15) == 15)
			Sleep(1);                      // tin rat dai: nhuong mot chut de khong tran bo dem
	}
	freeaddrinfo(res);
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

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) return false;

	// Chiem cong doc quyen: mo lan 2 cung tai khoan se bi loi bind
	int yes = 1;
	setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&yes, sizeof(yes));

	// Windows: neu gui toi cong UDP chua co ai nghe, recvfrom bao WSAECONNRESET (ICMP).
	// Tat hanh vi nay de thread nhan khong bi dung.
	BOOL newBehavior = FALSE;
	DWORD bytes = 0;
	WSAIoctl(s, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), NULL, 0, &bytes, NULL, NULL);

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((u_short)port);
	if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		closesocket(s);
		return false;
	}

	m_sock = s;
	m_netRunning = true;
	m_recvThread = std::thread(RecvLoop, m_sock, m_hWnd);
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

	if (m_recvThread.joinable()) m_recvThread.join();
	if (m_sendThread.joinable()) m_sendThread.join();
	if (m_sock != INVALID_SOCKET)
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
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
		if (!SendDatagrams(m_sock, m.ip, m.port, m_msgId++, payload))
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
		MessageBox(_T("Không gửi được tin tới ") + *peer + _T(" (sai IP/cổng trong peers.ini hoặc tin quá dài)."),
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
		MessageBox(_T("Không mở được cổng UDP để nhận tin. Tài khoản này có thể đang đăng nhập ở cửa sổ khác, hoặc cổng bị chiếm."),
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

	// Dua vao hang doi, thread gui se gui datagram UDP toi peer
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