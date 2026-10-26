// ThemedDialog.h : lop nen cho cac dialog (o nhap bo goc, nut xanh, nut link)
#pragma once
#include <map>
#include <vector>

class CThemedDialog : public CDialogEx
{
public:
	CThemedDialog(UINT id, CWnd* parent = nullptr);

protected:
	enum BtnStyle { BTN_PRIMARY, BTN_LINK, BTN_GHOST };

	void SetButtonStyle(UINT id, BtnStyle style);
	// Dat khung o nhap (px = true: toa do da la pixel that)
	void SetField(UINT editId, const CRect& frame, bool px = false);
	void SetClientSize(int w, int h);                    // kich thuoc vung client (pixel 96 DPI)
	void MoveCtrl(UINT id, const CRect& rc);
	CRect FieldFrame(UINT editId) const;

	// Lop con ve them noi dung
	virtual void PaintContent(CDC& dc, const CRect& client) {}

	CFont m_fontNormal;     // 15px
	CFont m_fontBold;       // 15px dam
	CFont m_fontSmall;      // 13px
	CFont m_fontTitle;      // 28px dam
	CFont m_fontAvatar;     // chu cai trong avatar
	int m_lineHeight = 0;   // chieu cao 1 dong chu (font 15px), pixel that

	virtual BOOL OnInitDialog();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDis);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	DECLARE_MESSAGE_MAP()

private:
	struct Field
	{
		UINT id;
		CRect frame;
	};
	std::vector<Field> m_fields;
	std::map<UINT, BtnStyle> m_buttons;
	CBrush m_brField;
	CBrush m_brBg;
};
