#include "pch.h"
#include "resource.h"
#include "ThemedDialog.h"
#include "Theme.h"

using theme::S;

CThemedDialog::CThemedDialog(UINT id, CWnd* parent)
	: CDialogEx(id, parent)
{
}

BEGIN_MESSAGE_MAP(CThemedDialog, CDialogEx)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
	ON_WM_DRAWITEM()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

BOOL CThemedDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	theme::MakeFont(m_fontNormal, 15);
	theme::MakeFont(m_fontBold, 15, FW_SEMIBOLD);
	theme::MakeFont(m_fontSmall, 13);
	theme::MakeFont(m_fontTitle, 28, FW_BOLD);
	theme::MakeFont(m_fontAvatar, 17, FW_SEMIBOLD);
	m_brField.CreateSolidBrush(theme::Field);
	m_brBg.CreateSolidBrush(theme::Bg);

	// Do chieu cao 1 dong chu de canh edit giua khung
	CClientDC dc(this);
	CFont* old = dc.SelectObject(&m_fontNormal);
	TEXTMETRIC tm;
	dc.GetTextMetrics(&tm);
	dc.SelectObject(old);
	m_lineHeight = tm.tmHeight;

	// Moi control con dung font Segoe UI 15px
	for (CWnd* w = GetWindow(GW_CHILD); w; w = w->GetNextWindow())
		w->SetFont(&m_fontNormal);

	HICON icon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	SetIcon(icon, TRUE);
	SetIcon(icon, FALSE);
	return TRUE;
}

void CThemedDialog::SetButtonStyle(UINT id, BtnStyle style)
{
	m_buttons[id] = style;
	if (CWnd* w = GetDlgItem(id))
	{
		w->ModifyStyle(0, BS_OWNERDRAW);
		w->Invalidate();
	}
}

void CThemedDialog::SetField(UINT editId, const CRect& frameIn, bool px)
{
	CRect frame = px ? frameIn : CRect(S(frameIn.left), S(frameIn.top), S(frameIn.right), S(frameIn.bottom));

	bool found = false;
	for (Field& f : m_fields)
		if (f.id == editId) { f.frame = frame; found = true; }
	if (!found) m_fields.push_back({ editId, frame });

	CWnd* edit = GetDlgItem(editId);
	if (!edit) return;

	// Canh edit vao giua khung
	CRect rc = frame;
	rc.DeflateRect(S(14), 0);
	if (edit->GetStyle() & ES_MULTILINE)
		rc.DeflateRect(0, S(10));
	else
	{
		int top = frame.top + (frame.Height() - m_lineHeight) / 2;
		rc.top = top;
		rc.bottom = top + m_lineHeight;
	}
	edit->MoveWindow(rc);
	Invalidate();
}

CRect CThemedDialog::FieldFrame(UINT editId) const
{
	for (const Field& f : m_fields)
		if (f.id == editId) return f.frame;
	return CRect();
}

void CThemedDialog::MoveCtrl(UINT id, const CRect& rc96)
{
	if (CWnd* w = GetDlgItem(id))
		w->MoveWindow(S(rc96.left), S(rc96.top), S(rc96.Width()), S(rc96.Height()));
}

void CThemedDialog::SetClientSize(int w, int h)
{
	CRect rc(0, 0, S(w), S(h));
	AdjustWindowRectEx(&rc, GetStyle(), FALSE, GetExStyle());
	SetWindowPos(NULL, 0, 0, rc.Width(), rc.Height(), SWP_NOMOVE | SWP_NOZORDER);
	CenterWindow();
}

BOOL CThemedDialog::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;   // ve toan bo trong OnPaint (tranh nhap nhay)
}

void CThemedDialog::OnPaint()
{
	CPaintDC paint(this);
	CRect client;
	GetClientRect(&client);

	// Ve vao bitmap tam (chong nhap nhay)
	CDC dc;
	dc.CreateCompatibleDC(&paint);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&paint, client.Width(), client.Height());
	CBitmap* oldBmp = dc.SelectObject(&bmp);
	dc.FillSolidRect(client, theme::Bg);
	dc.SetBkMode(TRANSPARENT);

	CWnd* focus = GetFocus();
	for (const Field& f : m_fields)
	{
		CWnd* edit = GetDlgItem(f.id);
		if (!edit || !edit->IsWindowVisible()) continue;
		bool focused = focus && focus->GetSafeHwnd() == edit->GetSafeHwnd();
		theme::FillRound(dc, f.frame, S(10), theme::Field, focused ? theme::Primary : theme::Field, focused ? S(2) : 1);
	}

	PaintContent(dc, client);

	paint.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
	dc.SelectObject(oldBmp);
}

HBRUSH CThemedDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	// To nen xam cho o nhap (ca khi bi khoa)
	if (nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_STATIC)
	{
		for (const Field& f : m_fields)
		{
			if (pWnd->GetDlgCtrlID() == (int)f.id)
			{
				pDC->SetBkColor(theme::Field);
				pDC->SetTextColor(pWnd->IsWindowEnabled() ? theme::Text : theme::SubText);
				return m_brField;
			}
		}
	}
	if (nCtlColor == CTLCOLOR_STATIC || nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_BTN)
	{
		pDC->SetBkColor(theme::Bg);
		pDC->SetTextColor(theme::Text);
		return m_brBg;
	}
	return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

// Ve lai vien xanh khi o nhap duoc chon
BOOL CThemedDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
	UINT code = HIWORD(wParam);
	if (code == EN_SETFOCUS || code == EN_KILLFOCUS)
	{
		CRect frame = FieldFrame(LOWORD(wParam));
		if (!frame.IsRectEmpty())
		{
			frame.InflateRect(S(2), S(2));
			InvalidateRect(frame, FALSE);
		}
	}
	return CDialogEx::OnCommand(wParam, lParam);
}

void CThemedDialog::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDis)
{
	auto it = m_buttons.find(nIDCtl);
	if (it == m_buttons.end() || lpDis->CtlType != ODT_BUTTON)
	{
		CDialogEx::OnDrawItem(nIDCtl, lpDis);   // vd listbox owner-draw: MFC chuyen ve cho control tu ve
		return;
	}

	CDC dc;
	dc.Attach(lpDis->hDC);
	CRect rc(lpDis->rcItem);
	bool pressed = (lpDis->itemState & ODS_SELECTED) != 0;
	bool disabled = (lpDis->itemState & ODS_DISABLED) != 0;

	CString text;
	GetDlgItem(nIDCtl)->GetWindowText(text);

	// Nen cua dialog phia sau nut
	dc.FillSolidRect(rc, theme::Bg);
	dc.SetBkMode(TRANSPARENT);

	switch (it->second)
	{
	case BTN_PRIMARY:
		theme::FillRound(dc, rc, S(10), disabled ? theme::Disabled : (pressed ? theme::PrimaryDark : theme::Primary));
		dc.SelectObject(&m_fontBold);
		dc.SetTextColor(RGB(255, 255, 255));
		break;
	case BTN_GHOST:
		theme::FillRound(dc, rc, S(10), pressed ? theme::Border : theme::Field);
		dc.SelectObject(&m_fontBold);
		dc.SetTextColor(theme::Text);
		break;
	case BTN_LINK:
		dc.SelectObject(&m_fontBold);
		dc.SetTextColor(disabled ? theme::Disabled : (pressed ? theme::PrimaryDark : theme::Primary));
		break;
	}
	dc.DrawText(text, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	dc.Detach();
}

// Con tro hinh ban tay khi re chuot len nut
BOOL CThemedDialog::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (pWnd && m_buttons.count(pWnd->GetDlgCtrlID()) && pWnd->IsWindowEnabled())
	{
		::SetCursor(::LoadCursor(NULL, IDC_HAND));
		return TRUE;
	}
	return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}
