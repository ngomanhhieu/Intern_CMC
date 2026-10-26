// ChatView.h : khung hien thi tin nhan (bong bong chat, cuon, chuot phai de sao chep)
// Layout() tinh vi tri bong bong, OnPaint() chi ve phan dang thay
// Tin lien tiep cua 1 nguoi duoc gom nhom, cach nhau lau thi chen vach thoi gian
#pragma once
#include <vector>
#include "ChatTypes.h"

class CChatView : public CWnd
{
public:
	BOOL Create(CWnd* parent, UINT id);

	void SetContact(Contact* c);              // nullptr = chua chon ai
	void Refresh(bool scrollToBottom);        // goi sau khi danh sach tin thay doi
	bool IsAtBottom() const;

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

private:
	struct Item
	{
		bool separator = false;
		CString text;
		CRect rc;                 // bong bong (hoac vach thoi gian), toa do trong trang
		bool isMe = false;
		bool showAvatar = false;  // tin cuoi nhom cua nguoi kia
		bool showTime = false;    // tin cuoi nhom
		bool seen = false;        // tin cuoi cua minh va nguoi kia da xem
		bool typing = false;      // bong bong "dang nhap..."
		long long time = 0;
	};

	void Layout();
	void UpdateScrollBar();
	void ScrollTo(int pos);
	int MaxScroll() const;
	int ViewOffset() const;       // toa do trang -> toa do man hinh: y + ViewOffset()
	void PaintEmpty(CDC& dc, const CRect& client);

	Contact* m_contact = nullptr;
	std::vector<Item> m_items;
	int m_total = 0;
	int m_scroll = 0;

	CFont m_font;
	CFont m_fontSmall;
	CFont m_fontSmallBold;
	CFont m_fontAvatar;
	CFont m_fontBigAvatar;
	CFont m_fontTitle;
};
