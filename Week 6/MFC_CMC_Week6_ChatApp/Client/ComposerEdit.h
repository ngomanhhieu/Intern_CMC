// ComposerEdit.h : o nhap tin nhan dung RichEdit
// Dung RichEdit vi EDIT thuong khong tu xuong dong khi go bang bo go tieng Viet
#pragma once

// Bao cua so cha tinh lai chieu cao o nhap (sau moi phim)
#define WM_COMPOSER_RECALC (WM_APP + 30)

class CComposerEdit : public CRichEditCtrl
{
public:
	// Font, mau, chu goi y
	void Setup(CFont* font, COLORREF bg, COLORREF text, COLORREF cueColor, const CString& cue);

	CString GetPlainText();           // noi dung, xuong dong la '\n'
	bool IsEmpty() { return GetWindowTextLength() == 0; }
	void RefreshCue();                // goi khi noi dung doi: ve lai / xoa chu goi y

protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()

private:
	CFont* m_font = nullptr;
	COLORREF m_cueColor = 0;
	CString m_cue;
	bool m_wasEmpty = true;
};
