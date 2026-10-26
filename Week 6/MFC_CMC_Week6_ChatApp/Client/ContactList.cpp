#include "pch.h"
#include "ContactList.h"
#include "Theme.h"

using theme::S;

void CContactList::Init(std::deque<Contact>* contacts, CFont* name, CFont* nameBold, CFont* smallFont, CFont* smallBoldFont, CFont* avatar)
{
	m_contacts = contacts;
	m_fontName = name;
	m_fontNameBold = nameBold;
	m_fontSmall = smallFont;
	m_fontSmallBold = smallBoldFont;
	m_fontAvatar = avatar;
	SetItemHeight(0, S(72));
}

void CContactList::DrawItem(LPDRAWITEMSTRUCT lpDis)
{
	if ((int)lpDis->itemID < 0 || !m_contacts) return;
	int idx = (int)lpDis->itemData;
	if (idx < 0 || idx >= (int)m_contacts->size()) return;
	const Contact& c = (*m_contacts)[idx];

	CRect item(lpDis->rcItem);
	CDC* target = CDC::FromHandle(lpDis->hDC);

	// Ve vao bitmap tam roi copy ra (chong nhap nhay)
	CDC dc;
	dc.CreateCompatibleDC(target);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(target, item.Width(), item.Height());
	CBitmap* oldBmp = dc.SelectObject(&bmp);
	CRect rc(0, 0, item.Width(), item.Height());
	dc.FillSolidRect(rc, theme::Bg);
	dc.SetBkMode(TRANSPARENT);

	bool selected = (lpDis->itemState & ODS_SELECTED) != 0;
	if (selected)
	{
		CRect sel = rc;
		sel.DeflateRect(S(8), S(2));
		theme::FillRound(dc, sel, S(10), theme::Selected);
	}

	// Avatar + cham online
	int av = S(48);
	CRect avatar(S(18), (rc.Height() - av) / 2, S(18) + av, (rc.Height() + av) / 2);
	theme::DrawAvatar(dc, avatar, c.display, c.user, m_fontAvatar);
	if (c.online)
		theme::DrawOnlineDot(dc, avatar, S(14));

	bool unread = c.unread > 0;
	int textLeft = avatar.right + S(12);
	int textRight = rc.right - S(18);
	int mid = rc.Height() / 2;

	// Dong 1: ten + gio tin cuoi
	CString when = theme::FormatShortTime(c.lastTime);
	dc.SelectObject(unread ? m_fontSmallBold : m_fontSmall);
	int whenW = when.IsEmpty() ? 0 : dc.GetTextExtent(when).cx;
	CRect whenRc(textRight - whenW, mid - S(24), textRight, mid);
	dc.SetTextColor(unread ? theme::Primary : theme::SubText);
	dc.DrawText(when, &whenRc, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);

	dc.SelectObject(unread ? m_fontNameBold : m_fontName);
	dc.SetTextColor(theme::Text);
	CRect nameRc(textLeft, mid - S(26), textRight - whenW - S(8), mid);
	dc.DrawText(c.display, &nameRc, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

	// Dong 2: xem truoc tin cuoi + so tin chua doc
	int badgeW = 0;
	if (unread)
	{
		CString n;
		n.Format(_T("%d"), c.unread > 99 ? 99 : c.unread);
		dc.SelectObject(m_fontSmallBold);
		int h = S(20);
		badgeW = max(h, dc.GetTextExtent(n).cx + S(12));
		CRect badge(textRight - badgeW, mid + S(4), textRight, mid + S(4) + h);
		theme::FillRound(dc, badge, h / 2, theme::Primary);
		dc.SetTextColor(RGB(255, 255, 255));
		dc.DrawText(n, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		badgeW += S(8);
	}

	CString preview;
	if (c.lastTime > 0)
	{
		preview = c.lastText;
		preview.Replace(_T("\r"), _T(""));
		preview.Replace(_T("\n"), _T(" "));
		if (c.lastFromMe) preview = _T("Bạn: ") + preview;
	}
	else
		preview = c.online ? _T("Đang hoạt động") : _T("Chưa có tin nhắn");

	bool typing = c.IsTyping();
	if (typing) preview = _T("Đang nhập...");
	dc.SelectObject(unread || typing ? m_fontSmallBold : m_fontSmall);
	dc.SetTextColor(typing ? theme::Primary : unread ? theme::Text : theme::SubText);
	CRect prevRc(textLeft, mid + S(2), textRight - badgeW, mid + S(26));
	dc.DrawText(preview, &prevRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

	target->BitBlt(item.left, item.top, item.Width(), item.Height(), &dc, 0, 0, SRCCOPY);
	dc.SelectObject(oldBmp);
}
