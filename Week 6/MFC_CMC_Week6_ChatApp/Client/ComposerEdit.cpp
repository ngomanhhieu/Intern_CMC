#include "pch.h"
#include "ComposerEdit.h"

BEGIN_MESSAGE_MAP(CComposerEdit, CRichEditCtrl)
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CComposerEdit::Setup(CFont* font, COLORREF bg, COLORREF text, COLORREF cueColor, const CString& cue)
{
	m_font = font;
	m_cueColor = cueColor;
	m_cue = cue;

	// Chi nhan chu thuong (dan khong keo theo dinh dang)
	SendMessage(EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE);

	// Bat TSF: bo go Telex co san cua Windows chay qua TSF, RichEdit mac dinh tat -> go ra chu khong dau
	SendMessage(EM_SETEDITSTYLE, SES_USECTF, SES_USECTF);
	SetFont(font);
	SetBackgroundColor(FALSE, bg);

	CHARFORMAT2 cf;
	ZeroMemory(&cf, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = text;
	SetDefaultCharFormat(cf);

	// Bao khi noi dung / chieu cao noi dung doi
	SetEventMask(ENM_CHANGE | ENM_REQUESTRESIZE);
}

// Sau moi phim, dan, cat: bao cua so cha tinh lai chieu cao
LRESULT CComposerEdit::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT r = CRichEditCtrl::WindowProc(message, wParam, lParam);
	switch (message)
	{
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_IME_COMPOSITION:
	case WM_IME_ENDCOMPOSITION:
	case WM_PASTE:
	case WM_CUT:
	case WM_UNDO:
	case WM_SETTEXT:
		if (CWnd* parent = GetParent())
			parent->PostMessage(WM_COMPOSER_RECALC);
		break;
	}
	return r;
}

CString CComposerEdit::GetPlainText()
{
	CString s;
	GetWindowText(s);
	s.Replace(_T("\r\n"), _T("\n"));
	s.Replace(_T("\r"), _T("\n"));   // RichEdit dung '\r' lam ky tu xuong dong
	return s;
}

void CComposerEdit::RefreshCue()
{
	bool empty = IsEmpty();
	if (empty != m_wasEmpty)
	{
		m_wasEmpty = empty;
		Invalidate();                  // RichEdit chi ve lai cho vua doi -> phai ve lai ca o de xoa/hien chu goi y
	}
}

// Tu ve chu goi y khi o trong
void CComposerEdit::OnPaint()
{
	Default();                        // de RichEdit tu ve noi dung truoc
	if (!IsEmpty() || m_cue.IsEmpty()) return;

	CClientDC dc(this);
	CRect rc;
	GetRect(&rc);                     // vung chu cua RichEdit
	CFont* old = m_font ? dc.SelectObject(m_font) : nullptr;
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(m_cueColor);
	dc.DrawText(m_cue, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	if (old) dc.SelectObject(old);
}
