// MainDlg.cpp : man hinh chat chinh
// Mo man hinh: GET_USERS. Chon nguoi: GET_MESSAGES + MARK_READ. Gui tin: CHAT.
#include "pch.h"
#include <algorithm>
#include "ChatClient.h"
#include "MainDlg.h"
#include "Theme.h"

using json::Value;

using theme::S;
using theme::FromUtf8;
using theme::ToUtf8;

#define IDC_CHATVIEW 2000

CMainDlg::CMainDlg(CWnd* pParent)
	: CThemedDialog(IDD_MAIN, pParent)
{
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CThemedDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CONTACTS, m_list);
	DDX_Control(pDX, IDC_COMPOSER, m_input);
}

BEGIN_MESSAGE_MAP(CMainDlg, CThemedDialog)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETCURSOR()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BTN_SEND, &CMainDlg::OnClickedSend)
	ON_BN_CLICKED(IDC_BTN_LOGOUT, &CMainDlg::OnClickedLogout)
	ON_LBN_SELCHANGE(IDC_CONTACTS, &CMainDlg::OnSelChangeContacts)
	ON_EN_CHANGE(IDC_SEARCH, &CMainDlg::OnChangeSearch)
	ON_EN_CHANGE(IDC_COMPOSER, &CMainDlg::OnChangeComposer)
	ON_NOTIFY(EN_REQUESTRESIZE, IDC_COMPOSER, &CMainDlg::OnComposerResize)
	ON_MESSAGE(WM_COMPOSER_RECALC, &CMainDlg::OnComposerRecalc)
	ON_MESSAGE(WM_NET_EVENT, &CMainDlg::OnNetEvent)
END_MESSAGE_MAP()

BOOL CMainDlg::OnInitDialog()
{
	CThemedDialog::OnInitDialog();

	SetWindowText(_T("CMC Chat - ") + theApp.m_display);
	m_sidebar = S(330);
	m_header = S(72);
	m_composer = S(80);

	theme::MakeFont(m_fontName, 17, FW_SEMIBOLD);
	theme::MakeFont(m_fontListAvatar, 20, FW_SEMIBOLD);
	theme::MakeFont(m_fontSmallBold, 13, FW_SEMIBOLD);

	SetButtonStyle(IDC_BTN_SEND, BTN_PRIMARY);
	SetButtonStyle(IDC_BTN_LOGOUT, BTN_GHOST);
	m_list.Init(&m_contacts, &m_fontNormal, &m_fontBold, &m_fontSmall, &m_fontSmallBold, &m_fontListAvatar);
	m_chat.Create(this, IDC_CHATVIEW);

	((CEdit*)GetDlgItem(IDC_SEARCH))->SetCueBanner(_T("Tìm kiếm người dùng"), TRUE);   // TRUE: hien ca khi dang focus
	m_input.Setup(&m_fontNormal, theme::Field, theme::Text, theme::SubText,
		_T("Nhập tin nhắn... (Enter để gửi, Shift+Enter để xuống dòng)"));
	m_input.LimitText(4000);
	m_input.EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_SEND)->EnableWindow(FALSE);

	SetClientSize(1060, 700);
	Layout();

	// Nhan goi tin va xin danh sach nguoi dung
	theApp.m_net.SetNotifyWindow(m_hWnd);
	theApp.m_net.Send(proto::GET_USERS);
	theApp.m_net.Send(proto::GET_AVATAR, Value::Object().Set("username", ToUtf8(theApp.m_user)));

	SetTimer(TIMER_TICK, 1000, NULL);   // tat "dang nhap..." khi het han, cap nhat "hoat dong X phut truoc"
	GotoDlgCtrl(GetDlgItem(IDC_SEARCH));
	return FALSE;
}

void CMainDlg::OnTimer(UINT_PTR id)
{
	if (id != TIMER_TICK) return;
	bool changed = false;
	for (Contact& c : m_contacts)
	{
		if (c.typingUntil != 0 && !c.IsTyping())
		{
			c.typingUntil = 0;              // het "dang nhap..."
			changed = true;
		}
	}
	if (changed)
	{
		m_list.Invalidate(FALSE);
		if (m_current >= 0) m_chat.Refresh(m_chat.IsAtBottom());
	}
	InvalidateHeader();                     // "hoat dong X phut truoc" doi theo thoi gian
}

// ---- Bo cuc ----
void CMainDlg::Layout()
{
	if (!m_list.GetSafeHwnd() || !m_chat.GetSafeHwnd()) return;

	CRect c;
	GetClientRect(&c);
	int sb = m_sidebar;

	// Cot trai
	GetDlgItem(IDC_BTN_LOGOUT)->MoveWindow(sb - S(16) - S(96), S(18), S(96), S(36));
	SetField(IDC_SEARCH, CRect(S(16), m_header + S(4), sb - S(16), m_header + S(44)), true);
	m_list.MoveWindow(0, m_header + S(56), sb, c.bottom - m_header - S(56));

	// Cot phai
	int top = m_header + 1;
	int bottom = c.bottom - m_composer;
	m_chat.MoveWindow(sb + 1, top, c.right - sb - 1, max(0, bottom - top));

	int btnW = S(84);
	CRect frame(sb + S(16), c.bottom - m_composer + S(16), c.right - S(16) - btnW - S(10), c.bottom - S(16));
	SetField(IDC_COMPOSER, frame, true);
	GetDlgItem(IDC_BTN_SEND)->MoveWindow(c.right - S(16) - btnW, frame.bottom - S(48), btnW, S(48));   // nut Gui giu co dinh o day

	Invalidate(FALSE);
}

void CMainDlg::OnSize(UINT nType, int cx, int cy)
{
	CThemedDialog::OnSize(nType, cx, cy);
	Layout();
	UpdateComposerHeight();   // rong/hep di -> so dong doi -> tinh lai chieu cao o nhap
}

void CMainDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	lpMMI->ptMinTrackSize.x = S(800);
	lpMMI->ptMinTrackSize.y = S(520);
	CThemedDialog::OnGetMinMaxInfo(lpMMI);
}

void CMainDlg::InvalidateHeader()
{
	CRect c;
	GetClientRect(&c);
	InvalidateRect(CRect(m_sidebar, 0, c.right, m_header), FALSE);
}

void CMainDlg::PaintContent(CDC& dc, const CRect& client)
{
	int sb = m_sidebar;

	// Duong ke chia cac vung
	dc.FillSolidRect(sb, 0, 1, client.bottom, theme::Border);
	dc.FillSolidRect(sb, m_header, client.right - sb, 1, theme::Border);
	dc.FillSolidRect(sb, client.bottom - m_composer, client.right - sb, 1, theme::Border);

	// Cot trai: avatar + ten cua minh
	CRect me(S(16), S(16), S(16) + S(40), S(56));
	theme::DrawAvatar(dc, me, theApp.m_display, theApp.m_user, &m_fontAvatar);
	theme::DrawOnlineDot(dc, me, S(12));
	int textRight = sb - S(16) - S(96) - S(8);
	dc.SelectObject(&m_fontName);
	dc.SetTextColor(theme::Text);
	CRect r(me.right + S(10), S(14), textRight, S(38));
	dc.DrawText(theApp.m_display, &r, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	dc.SelectObject(&m_fontSmall);
	dc.SetTextColor(theme::SubText);
	r = CRect(me.right + S(10), S(38), textRight, S(58));
	dc.DrawText(_T("@") + theApp.m_user, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

	// Tieu de khung chat: nguoi dang chat + trang thai
	if (m_current >= 0)
	{
		const Contact& c = m_contacts[m_current];
		CRect av(sb + S(20), S(16), sb + S(60), S(56));
		theme::DrawAvatar(dc, av, c.display, c.user, &m_fontAvatar);
		if (c.online) theme::DrawOnlineDot(dc, av, S(12));

		dc.SelectObject(&m_fontName);
		dc.SetTextColor(theme::Text);
		r = CRect(av.right + S(12), S(14), client.right - S(20), S(38));
		dc.DrawText(c.display, &r, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

		dc.SelectObject(&m_fontSmall);
		dc.SetTextColor(c.IsTyping() ? theme::Primary : c.online ? theme::Online : theme::SubText);
		r = CRect(av.right + S(12), S(38), client.right - S(20), S(58));
		CString state = c.IsTyping() ? CString(_T("Đang nhập...")) :
			c.online ? CString(_T("Đang hoạt động")) : theme::FormatLastSeen(c.lastSeen);
		CString status = state + _T("  ·  @") + c.user;
		dc.DrawText(status, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	}
	else
	{
		dc.SelectObject(&m_fontName);
		dc.SetTextColor(theme::Text);
		r = CRect(sb + S(20), 0, client.right - S(20), m_header);
		dc.DrawText(_T("CMC Chat"), &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}
}

// ---- Danh sach nguoi dung ----
int CMainDlg::FindContact(const CString& user)
{
	for (int i = 0; i < (int)m_contacts.size(); i++)
		if (m_contacts[i].user.CompareNoCase(user) == 0) return i;
	return -1;
}

int CMainDlg::FindOrAddContact(const CString& user, const CString& display)
{
	int i = FindContact(user);
	if (i >= 0) return i;
	Contact c;
	c.user = user;
	c.display = display.IsEmpty() ? user : display;
	m_contacts.push_back(c);
	return (int)m_contacts.size() - 1;
}

void CMainDlg::RebuildList()
{
	CString filter;
	GetDlgItemText(IDC_SEARCH, filter);
	filter.Trim();
	filter.MakeLower();

	std::vector<int> order;
	for (int i = 0; i < (int)m_contacts.size(); i++)
	{
		CString d = m_contacts[i].display, u = m_contacts[i].user;
		d.MakeLower();
		u.MakeLower();
		if (filter.IsEmpty() || d.Find(filter) >= 0 || u.Find(filter) >= 0)
			order.push_back(i);
	}

	// Sap xep: tin moi nhat, roi online, roi ten
	std::sort(order.begin(), order.end(), [this](int a, int b) {
		const Contact& x = m_contacts[a];
		const Contact& y = m_contacts[b];
		if (x.lastTime != y.lastTime) return x.lastTime > y.lastTime;
		if (x.online != y.online) return x.online;
		return x.display.CompareNoCase(y.display) < 0;
	});

	int top = m_list.GetTopIndex();
	m_list.SetRedraw(FALSE);
	m_list.ResetContent();
	for (int idx : order)
	{
		int row = m_list.AddString(_T(""));
		m_list.SetItemData(row, (DWORD_PTR)idx);
		if (idx == m_current) m_list.SetCurSel(row);
	}
	if (top < m_list.GetCount()) m_list.SetTopIndex(top);
	m_list.SetRedraw(TRUE);
	m_list.Invalidate();
}

void CMainDlg::OnChangeSearch()
{
	RebuildList();
}

// O nhap tu cao theo so dong (toi da 6 dong, qua thi hien thanh cuon)
static const int COMPOSER_MAX_LINES = 6;

void CMainDlg::UpdateComposerHeight()
{
	if (!m_input.GetSafeHwnd()) return;
	m_input.RefreshCue();

	// So dong dang hien
	int lines = max(1, m_input.GetLineCount());
	if (lines >= 2)
	{
		int h = m_input.PosFromChar(m_input.LineIndex(1)).y - m_input.PosFromChar(0).y;
		if (h > 0) m_inputLineH = h;           // do chieu cao dong that cua RichEdit
	}
	int lineH = m_inputLineH > 0 ? m_inputLineH : m_lineHeight;

	int shown = min(lines, COMPOSER_MAX_LINES);
	int composer = max(S(48), shown * lineH + S(24)) + S(32);
	if (composer != m_composer)
	{
		m_composer = composer;
		Layout();
	}
	m_input.SendMessage(EM_SCROLLCARET);       // luon thay dong dang go
}

void CMainDlg::OnChangeComposer()
{
	UpdateComposerHeight();
	NotifyTyping();
}

// Dang go -> bao nguoi kia "dang nhap...", toi da 1 lan / 2 giay
void CMainDlg::NotifyTyping()
{
	if (m_current < 0 || m_input.IsEmpty()) return;
	ULONGLONG now = GetTickCount64();
	if (now - m_lastTypingSent < 2000) return;
	m_lastTypingSent = now;
	theApp.m_net.Send(proto::TYPING, Value::Object().Set("to", ToUtf8(m_contacts[m_current].user)));
}

void CMainDlg::OnComposerResize(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	UpdateComposerHeight();
}

LRESULT CMainDlg::OnComposerRecalc(WPARAM, LPARAM)
{
	UpdateComposerHeight();
	NotifyTyping();
	return 0;
}

void CMainDlg::OnSelChangeContacts()
{
	int idx = m_list.ContactAt(m_list.GetCurSel());
	if (idx >= 0) SelectContact(idx);
}

void CMainDlg::SelectContact(int idx)
{
	m_current = idx;
	Contact& c = m_contacts[idx];

	if (!c.historyRequested)
	{
		c.historyRequested = true;
		theApp.m_net.Send(proto::GET_MESSAGES, Value::Object().Set("with", ToUtf8(c.user)));
	}
	if (c.unread > 0)
	{
		c.unread = 0;
		theApp.m_net.Send(proto::MARK_READ, Value::Object().Set("with", ToUtf8(c.user)));
		m_list.Invalidate(FALSE);
	}

	m_chat.SetContact(&c);
	GetDlgItem(IDC_COMPOSER)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_SEND)->EnableWindow(TRUE);
	GotoDlgCtrl(GetDlgItem(IDC_COMPOSER));
	InvalidateHeader();
	Invalidate(FALSE);
}

// ---- Gui tin ----
BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	// Enter trong o nhap = gui, Shift+Enter = xuong dong
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN &&
		pMsg->hwnd == GetDlgItem(IDC_COMPOSER)->GetSafeHwnd() && !(GetKeyState(VK_SHIFT) & 0x8000))
	{
		OnClickedSend();
		return TRUE;
	}
	return CThemedDialog::PreTranslateMessage(pMsg);
}

void CMainDlg::OnClickedSend()
{
	if (m_current < 0) return;

	CString text = m_input.GetPlainText();
	text.Trim();
	if (text.IsEmpty()) return;

	std::string utf8 = ToUtf8(text);
	if ((int)utf8.size() > proto::MAX_CONTENT)
	{
		MessageBox(_T("Tin nhắn quá dài."), _T("CMC Chat"), MB_ICONWARNING);
		return;
	}

	if (!theApp.m_net.Send(proto::CHAT, Value::Object().Set("to", ToUtf8(m_contacts[m_current].user)).Set("content", utf8)))
	{
		MessageBox(_T("Không gửi được tin nhắn: mất kết nối tới server."), _T("CMC Chat"), MB_ICONWARNING);
		return;
	}
	m_input.SetWindowText(_T(""));
	UpdateComposerHeight();    // thu o nhap ve 1 dong
	GotoDlgCtrl(&m_input);
}

void CMainDlg::OnClickedLogout()
{
	theApp.m_net.Send(proto::LOGOUT);
	EndDialog(IDRETRY);   // quay lai man hinh dang nhap
}

void CMainDlg::OnClose()
{
	theApp.m_net.Send(proto::LOGOUT);
	EndDialog(IDCANCEL);  // thoat app
}

// ---- Goi tin tu server ----
LRESULT CMainDlg::OnNetEvent(WPARAM, LPARAM)
{
	proto::Packet p;
	while (theApp.m_net.Pop(p))
	{
		if (p.action == proto::GET_AVATAR)
		{
			HandleAvatar(p.body);           // chua co anh thi bo qua, khong bao loi
			continue;
		}

		// Loi tu server: hien ly do
		if (p.action == proto::ERROR_MSG || (p.action != NET_CLOSED && p.body["status"].Str() == "failed"))
		{
			MessageBox(FromUtf8(p.body["reason"].Str()), _T("CMC Chat"), MB_ICONWARNING);
			continue;
		}

		switch (p.action)
		{
		case proto::GET_USERS:    HandleUsers(p.body); break;
		case proto::GET_MESSAGES: HandleMessages(p.body); break;
		case proto::CHAT:         HandleChat(p.body); break;
		case proto::USER_STATUS:  HandleUserStatus(p.body); break;
		case proto::TYPING:       HandleTyping(p.body); break;
		case proto::SEEN:         HandleSeen(p.body); break;
		case NET_CLOSED:
			MessageBox(_T("Mất kết nối tới server. Hãy đăng nhập lại."), _T("CMC Chat"), MB_ICONWARNING);
			EndDialog(IDRETRY);
			return 0;
		}
	}
	return 0;
}

// Danh sach nguoi dung
void CMainDlg::HandleUsers(const Value& body)
{
	const Value& users = body["users"];
	for (size_t i = 0; i < users.Size(); i++)
	{
		const Value& u = users[i];
		int idx = FindOrAddContact(FromUtf8(u["username"].Str()), FromUtf8(u["display_name"].Str()));
		Contact& c = m_contacts[idx];
		c.display = FromUtf8(u["display_name"].Str());
		c.online = u["online"].AsBool();
		c.unread = (int)u["unread"].Int();
		c.lastFromMe = FromUtf8(u["last_from"].Str()).CompareNoCase(theApp.m_user) == 0;
		c.lastText = FromUtf8(u["last_content"].Str());
		c.lastTime = u["last_time"].Int();
		c.lastSeen = u["last_seen"].Int();

		if (u["has_avatar"].AsBool() && !theme::HasAvatarImage(c.user))
			theApp.m_net.Send(proto::GET_AVATAR, Value::Object().Set("username", ToUtf8(c.user)));
	}
	RebuildList();
}

// Anh avatar cua 1 nguoi (ca khi ai do vua doi anh)
void CMainDlg::HandleAvatar(const Value& body)
{
	std::string image;
	if (body["status"].Str() != "ok" || !proto::Base64Decode(body["data"].Str(), image)) return;
	theme::SetAvatarImage(FromUtf8(body["username"].Str()), image);
	m_list.Invalidate(FALSE);
	m_chat.Invalidate(FALSE);
	Invalidate(FALSE);
}

// Bam vao avatar cua minh -> chon anh moi
CRect CMainDlg::MyAvatarRect() const
{
	return CRect(S(16), S(16), S(56), S(56));
}

void CMainDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (MyAvatarRect().PtInRect(point))
	{
		ChangeMyAvatar();
		return;
	}
	CThemedDialog::OnLButtonDown(nFlags, point);
}

BOOL CMainDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	CPoint pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	if (pWnd == this && MyAvatarRect().PtInRect(pt))
	{
		::SetCursor(::LoadCursor(NULL, IDC_HAND));
		return TRUE;
	}
	return CThemedDialog::OnSetCursor(pWnd, nHitTest, message);
}

void CMainDlg::ChangeMyAvatar()
{
	CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("Ảnh (*.png;*.jpg;*.jpeg;*.bmp;*.gif)|*.png;*.jpg;*.jpeg;*.bmp;*.gif||"), this);
	if (dlg.DoModal() != IDOK) return;

	std::string png;
	if (!theme::MakeAvatarPng(dlg.GetPathName(), png))
	{
		MessageBox(_T("Không đọc được ảnh này."), _T("CMC Chat"), MB_ICONWARNING);
		return;
	}
	theApp.m_net.Send(proto::SET_AVATAR, Value::Object().Set("data", proto::Base64Encode(png)));
}

// Lich su chat
void CMainDlg::HandleMessages(const Value& body)
{
	int idx = FindContact(FromUtf8(body["with"].Str()));
	if (idx < 0) return;
	Contact& c = m_contacts[idx];

	const Value& list = body["messages"];
	std::vector<ChatMsg> msgs;
	long long maxId = 0;
	for (size_t i = 0; i < list.Size(); i++)
	{
		ChatMsg m;
		m.id = list[i]["id"].Int();
		m.isMe = FromUtf8(list[i]["from"].Str()).CompareNoCase(theApp.m_user) == 0;
		m.text = FromUtf8(list[i]["content"].Str());
		m.time = list[i]["time"].Int();
		maxId = max(maxId, m.id);
		msgs.push_back(m);
	}
	// Giu tin moi den trong luc cho lich su
	for (const ChatMsg& m : c.msgs)
		if (m.id > maxId) msgs.push_back(m);

	c.msgs.swap(msgs);
	c.historyLoaded = true;
	c.seenId = max(c.seenId, body["seen_id"].Int());
	if (idx == m_current) m_chat.Refresh(true);
}

// Nguoi kia dang go
void CMainDlg::HandleTyping(const Value& body)
{
	int idx = FindContact(FromUtf8(body["from"].Str()));
	if (idx < 0) return;
	bool wasTyping = m_contacts[idx].IsTyping();
	m_contacts[idx].typingUntil = GetTickCount64() + 4000;   // het sau 4 giay neu khong go nua
	if (wasTyping) return;
	m_list.Invalidate(FALSE);
	if (idx == m_current)
	{
		m_chat.Refresh(m_chat.IsAtBottom());
		InvalidateHeader();
	}
}

// Nguoi kia da xem tin cua minh
void CMainDlg::HandleSeen(const Value& body)
{
	int idx = FindContact(FromUtf8(body["by"].Str()));
	if (idx < 0) return;
	m_contacts[idx].seenId = max(m_contacts[idx].seenId, body["last_id"].Int());
	if (idx == m_current) m_chat.Refresh(m_chat.IsAtBottom());
}

// Tin nhan moi
void CMainDlg::HandleChat(const Value& body)
{
	CString from = FromUtf8(body["from"].Str());
	CString to = FromUtf8(body["to"].Str());
	bool isMe = from.CompareNoCase(theApp.m_user) == 0;
	CString peer = isMe ? to : from;
	if (peer.IsEmpty()) return;

	int idx = FindOrAddContact(peer, peer);
	Contact& c = m_contacts[idx];

	ChatMsg m;
	m.id = body["id"].Int();
	m.isMe = isMe;
	m.text = FromUtf8(body["content"].Str());
	m.time = body["time"].Int();

	bool exists = false;
	for (const ChatMsg& x : c.msgs)
		if (x.id == m.id) { exists = true; break; }
	if (exists) return;

	bool wasAtBottom = m_chat.IsAtBottom();
	if (!isMe) c.typingUntil = 0;           // tin da den -> het "dang nhap..."
	c.msgs.push_back(m);
	c.lastText = m.text;
	c.lastFromMe = isMe;
	c.lastTime = m.time;

	if (idx == m_current)
	{
		if (!isMe) theApp.m_net.Send(proto::MARK_READ, Value::Object().Set("with", ToUtf8(c.user)));
		m_chat.Refresh(isMe || wasAtBottom);
	}
	else if (!isMe)
		c.unread++;

	// Nhay taskbar neu cua so khong duoc chon
	if (!isMe && ::GetForegroundWindow() != m_hWnd)
	{
		FLASHWINFO fi = { sizeof(fi), m_hWnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
		::FlashWindowEx(&fi);
		MessageBeep(MB_OK);
	}
	RebuildList();
}

// Ai do online / offline
void CMainDlg::HandleUserStatus(const Value& body)
{
	CString user = FromUtf8(body["username"].Str());
	if (user.IsEmpty()) return;
	int idx = FindOrAddContact(user, FromUtf8(body["display_name"].Str()));
	m_contacts[idx].display = FromUtf8(body["display_name"].Str());
	m_contacts[idx].online = body["online"].AsBool();
	m_contacts[idx].lastSeen = body["last_seen"].Int();
	if (!m_contacts[idx].online) m_contacts[idx].typingUntil = 0;
	RebuildList();
	if (idx == m_current) InvalidateHeader();
}
