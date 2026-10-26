#include "pch.h"
#include "ChatView.h"
#include "Theme.h"

using theme::S;

static const long long GROUP_GAP = 2 * 60;       // < 2 phut: cung nhom
static const long long SEPARATOR_GAP = 15 * 60;  // >= 15 phut: chen vach thoi gian

BEGIN_MESSAGE_MAP(CChatView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_CONTEXTMENU()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CChatView::Create(CWnd* parent, UINT id)
{
	CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	if (!CWnd::Create(cls, _T(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPSIBLINGS, CRect(0, 0, 10, 10), parent, id))
		return FALSE;

	theme::MakeFont(m_font, 15);
	theme::MakeFont(m_fontSmall, 12);
	theme::MakeFont(m_fontSmallBold, 12, FW_SEMIBOLD);
	theme::MakeFont(m_fontAvatar, 13, FW_SEMIBOLD);
	theme::MakeFont(m_fontBigAvatar, 30, FW_SEMIBOLD);
	theme::MakeFont(m_fontTitle, 19, FW_SEMIBOLD);
	return TRUE;
}

void CChatView::SetContact(Contact* c)
{
	m_contact = c;
	m_scroll = 0;
	Refresh(true);
}

void CChatView::Refresh(bool scrollToBottom)
{
	Layout();
	if (scrollToBottom) m_scroll = MaxScroll();
	ScrollTo(m_scroll);
	Invalidate(FALSE);
}

int CChatView::MaxScroll() const
{
	CRect rc;
	GetClientRect(&rc);
	return max(0, m_total - rc.Height());
}

bool CChatView::IsAtBottom() const
{
	return m_scroll >= MaxScroll() - S(40);
}

// Tin it thi day xuong sat day
int CChatView::ViewOffset() const
{
	CRect rc;
	GetClientRect(&rc);
	if (m_total < rc.Height()) return rc.Height() - m_total;
	return -m_scroll;
}

void CChatView::Layout()
{
	m_items.clear();
	m_total = 0;
	if (!m_contact || !GetSafeHwnd()) return;

	CRect client;
	GetClientRect(&client);
	int W = client.Width();
	int padX = S(20);
	int avatar = S(28);
	int padBX = S(12), padBY = S(8);
	int maxBubble = max(S(160), W * 65 / 100);

	CClientDC dc(this);
	CFont* old = dc.SelectObject(&m_font);

	const std::vector<ChatMsg>& msgs = m_contact->msgs;
	int lastMine = -1;                       // tin cuoi cua minh -> co the hien "Da xem"
	for (size_t i = 0; i < msgs.size(); i++)
		if (msgs[i].isMe) lastMine = (int)i;
	int y = S(16);
	for (size_t i = 0; i < msgs.size(); i++)
	{
		const ChatMsg& m = msgs[i];
		const ChatMsg* prev = i > 0 ? &msgs[i - 1] : nullptr;
		const ChatMsg* next = i + 1 < msgs.size() ? &msgs[i + 1] : nullptr;

		bool sep = !prev || m.time - prev->time >= SEPARATOR_GAP || !theme::SameDay(m.time, prev->time);
		if (sep)
		{
			Item s;
			s.separator = true;
			s.text = theme::FormatSeparator(m.time);
			s.rc = CRect(0, y, W, y + S(36));
			m_items.push_back(s);
			y += S(36);
		}

		bool newGroup = sep || prev->isMe != m.isMe || m.time - prev->time > GROUP_GAP;
		if (!sep) y += newGroup ? S(12) : S(2);

		bool nextSep = !next || next->time - m.time >= SEPARATOR_GAP || !theme::SameDay(next->time, m.time);
		bool endGroup = nextSep || next->isMe != m.isMe || next->time - m.time > GROUP_GAP;

		// Do kich thuoc chu (co tu xuong dong)
		CString text = m.text;
		text.Replace(_T("\n"), _T("\r\n"));
		CRect tr(0, 0, maxBubble - 2 * padBX, 0);
		dc.DrawText(text, &tr, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_EXPANDTABS);
		int bw = tr.Width() + 2 * padBX;
		int bh = tr.Height() + 2 * padBY;

		Item it;
		it.text = text;
		it.isMe = m.isMe;
		it.time = m.time;
		it.showAvatar = !m.isMe && endGroup;
		it.showTime = endGroup;
		it.seen = (int)i == lastMine && m.id <= m_contact->seenId;
		int x = m.isMe ? W - padX - bw : padX + avatar + S(8);
		it.rc = CRect(x, y, x + bw, y + bh);
		m_items.push_back(it);

		y += bh;
		if (endGroup) y += S(18);   // cho dong gio phia duoi
	}

	// Nguoi kia dang go: bong bong 3 cham o cuoi
	if (m_contact->IsTyping())
	{
		y += S(12);
		Item t;
		t.typing = true;
		t.showAvatar = true;
		int x = padX + avatar + S(8);
		t.rc = CRect(x, y, x + S(62), y + S(36));
		m_items.push_back(t);
		y += S(36);
	}
	m_total = y + S(12);
	dc.SelectObject(old);
}

void CChatView::UpdateScrollBar()
{
	CRect rc;
	GetClientRect(&rc);
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = max(0, m_total - 1);
	si.nPage = rc.Height();
	si.nPos = m_scroll;
	SetScrollInfo(SB_VERT, &si, TRUE);
}

void CChatView::ScrollTo(int pos)
{
	m_scroll = max(0, min(pos, MaxScroll()));
	UpdateScrollBar();
	Invalidate(FALSE);
}

void CChatView::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	bool bottom = IsAtBottom();
	Layout();
	ScrollTo(bottom ? MaxScroll() : m_scroll);
}

void CChatView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CRect rc;
	GetClientRect(&rc);
	int pos = m_scroll;
	switch (nSBCode)
	{
	case SB_LINEUP:        pos -= S(40); break;
	case SB_LINEDOWN:      pos += S(40); break;
	case SB_PAGEUP:        pos -= rc.Height(); break;
	case SB_PAGEDOWN:      pos += rc.Height(); break;
	case SB_TOP:           pos = 0; break;
	case SB_BOTTOM:        pos = MaxScroll(); break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
	{
		SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
		GetScrollInfo(SB_VERT, &si);
		pos = si.nTrackPos;
		break;
	}
	}
	ScrollTo(pos);
}

BOOL CChatView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	ScrollTo(m_scroll - zDelta * S(60) / WHEEL_DELTA);
	return TRUE;
}

void CChatView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();   // de lan chuot cuon duoc khung chat
	CWnd::OnLButtonDown(nFlags, point);
}

// Chuot phai vao bong bong -> "Sao chep"
void CChatView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CPoint pt = point;
	ScreenToClient(&pt);
	int off = ViewOffset();
	for (const Item& it : m_items)
	{
		if (it.separator) continue;
		CRect r = it.rc;
		r.OffsetRect(0, off);
		if (!r.PtInRect(pt)) continue;

		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, _T("Sao chép tin nhắn"));
		if (menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, this) == 1 && OpenClipboard())
		{
			EmptyClipboard();
			size_t bytes = (it.text.GetLength() + 1) * sizeof(TCHAR);
			HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (h)
			{
				memcpy(GlobalLock(h), (LPCTSTR)it.text, bytes);
				GlobalUnlock(h);
				SetClipboardData(CF_UNICODETEXT, h);
			}
			CloseClipboard();
		}
		return;
	}
}

BOOL CChatView::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

void CChatView::PaintEmpty(CDC& dc, const CRect& client)
{
	dc.FillSolidRect(client, m_contact ? theme::Bg : theme::Panel);
	CPoint c = client.CenterPoint();

	if (!m_contact)
	{
		dc.SelectObject(&m_fontTitle);
		dc.SetTextColor(theme::Text);
		CRect r(client.left, c.y - S(30), client.right, c.y);
		dc.DrawText(_T("Chọn một cuộc trò chuyện"), &r, DT_CENTER | DT_SINGLELINE | DT_BOTTOM);
		dc.SelectObject(&m_font);
		dc.SetTextColor(theme::SubText);
		r = CRect(client.left, c.y + S(4), client.right, c.y + S(30));
		dc.DrawText(_T("Chọn một người ở danh sách bên trái để bắt đầu nhắn tin"), &r, DT_CENTER | DT_SINGLELINE | DT_TOP);
		return;
	}

	// Chua co tin: avatar to + loi chao
	int av = S(80);
	CRect avatar(c.x - av / 2, c.y - S(90), c.x + av / 2, c.y - S(90) + av);
	theme::DrawAvatar(dc, avatar, m_contact->display, m_contact->user, &m_fontBigAvatar);

	dc.SelectObject(&m_fontTitle);
	dc.SetTextColor(theme::Text);
	CRect r(client.left, avatar.bottom + S(10), client.right, avatar.bottom + S(40));
	dc.DrawText(m_contact->display, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

	dc.SelectObject(&m_font);
	dc.SetTextColor(theme::SubText);
	r = CRect(client.left, r.bottom, client.right, r.bottom + S(26));
	CString hint = m_contact->historyLoaded ? _T("Hãy gửi lời chào đến ") + m_contact->display : CString(_T("Đang tải tin nhắn..."));
	dc.DrawText(hint, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

void CChatView::OnPaint()
{
	CPaintDC paint(this);
	CRect client;
	GetClientRect(&client);

	CDC dc;
	dc.CreateCompatibleDC(&paint);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&paint, max(1, client.Width()), max(1, client.Height()));
	CBitmap* oldBmp = dc.SelectObject(&bmp);
	CFont* oldFont = dc.SelectObject(&m_font);
	dc.SetBkMode(TRANSPARENT);

	if (!m_contact || m_items.empty())
		PaintEmpty(dc, client);
	else
	{
		dc.FillSolidRect(client, theme::Bg);
		int off = ViewOffset();
		int padBX = S(12), padBY = S(8);
		int avatar = S(28);

		for (const Item& it : m_items)
		{
			CRect r = it.rc;
			r.OffsetRect(0, off);
			if (r.bottom + S(20) < 0 || r.top > client.bottom) continue;   // ngoai vung nhin

			if (it.separator)
			{
				dc.SelectObject(&m_fontSmallBold);
				dc.SetTextColor(theme::SubText);
				dc.DrawText(it.text, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
				continue;
			}

			// Bong bong "dang nhap...": 3 cham xam
			if (it.typing)
			{
				theme::FillRound(dc, r, r.Height() / 2, theme::Field);
				int cy = r.CenterPoint().y, d = S(4);
				for (int k = -1; k <= 1; k++)
				{
					int cx = r.CenterPoint().x + k * S(11);
					theme::FillCircle(dc, CRect(cx - d, cy - d, cx + d, cy + d), theme::SubText);
				}
				CRect a(S(20), r.bottom - avatar, S(20) + avatar, r.bottom);
				theme::DrawAvatar(dc, a, m_contact->display, m_contact->user, &m_fontAvatar);
				continue;
			}

			// Bong bong
			theme::FillRound(dc, r, min(S(18), r.Height() / 2), it.isMe ? theme::Primary : theme::Field);
			dc.SelectObject(&m_font);
			dc.SetTextColor(it.isMe ? RGB(255, 255, 255) : theme::Text);
			CRect tr = r;
			tr.DeflateRect(padBX, padBY);
			dc.DrawText(it.text, &tr, DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_EXPANDTABS);

			// Avatar nguoi kia o tin cuoi nhom
			if (it.showAvatar && m_contact)
			{
				CRect a(S(20), r.bottom - avatar, S(20) + avatar, r.bottom);
				theme::DrawAvatar(dc, a, m_contact->display, m_contact->user, &m_fontAvatar);
			}

			// Gio duoi tin cuoi nhom
			if (it.showTime)
			{
				dc.SelectObject(&m_fontSmall);
				dc.SetTextColor(theme::SubText);
				CRect t(r.left, r.bottom + S(2), r.right, r.bottom + S(18));
				if (it.isMe)
				{
					t.left = client.left;
					CString label = theme::FormatClock(it.time);
					if (it.seen) label += _T("  ·  Đã xem");
					dc.DrawText(label, &t, DT_RIGHT | DT_TOP | DT_SINGLELINE);
				}
				else
				{
					t.right = client.right;
					dc.DrawText(theme::FormatClock(it.time), &t, DT_LEFT | DT_TOP | DT_SINGLELINE);
				}
			}
		}
	}

	paint.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
	dc.SelectObject(oldFont);
	dc.SelectObject(oldBmp);
}
