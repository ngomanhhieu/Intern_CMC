// Theme.h : mau sac, font va cac ham ve dung chung cho moi man hinh
#pragma once
#include <string>
#include <gdiplus.h>

namespace theme
{
	// ---- Bang mau ----
	const COLORREF Primary      = RGB(0, 132, 255);     // xanh chu dao
	const COLORREF PrimaryDark  = RGB(0, 106, 214);     // nut dang nhan
	const COLORREF Bg           = RGB(255, 255, 255);
	const COLORREF Panel        = RGB(247, 248, 250);   // nen phu (khung chat trong)
	const COLORREF Field        = RGB(240, 242, 245);   // nen o nhap lieu, bong bong nguoi khac
	const COLORREF Border       = RGB(228, 230, 235);
	const COLORREF Text         = RGB(5, 5, 5);
	const COLORREF SubText      = RGB(101, 103, 107);
	const COLORREF Online       = RGB(49, 162, 76);
	const COLORREF Error        = RGB(220, 53, 69);
	const COLORREF Selected     = RGB(231, 243, 255);
	const COLORREF Disabled     = RGB(190, 195, 201);

	// ---- DPI: kich thuoc viet o 96 DPI, S() nhan he so ----
	void InitDpi();
	int S(int px);

	// ---- Font Segoe UI ----
	void MakeFont(CFont& font, int px, int weight = FW_NORMAL);

	// ---- Chuyen doi chuoi ----
	std::string ToUtf8(const CString& s);
	CString FromUtf8(const std::string& s);

	// ---- Thoi gian (giay tu 1970, UTC) ----
	CString FormatShortTime(long long t);    // danh sach lien he: "14:05" / "T2" / "12/10"
	CString FormatClock(long long t);        // "14:05"
	CString FormatLastSeen(long long t);     // "Hoạt động 5 phút trước"
	CString FormatSeparator(long long t);    // vach ngan trong khung chat: "14:05" / "14:05, 12/10/2026"
	bool SameDay(long long a, long long b);

	// ---- Ve ----
	CString Initial(const CString& displayName);
	COLORREF AvatarColor(const CString& key);
	void FillRound(CDC& dc, const CRect& rc, int radius, COLORREF fill, COLORREF border = CLR_NONE, int borderWidth = 1);
	void FillCircle(CDC& dc, const CRect& rc, COLORREF fill, COLORREF ring = CLR_NONE, int ringWidth = 0);
	void DrawAvatar(CDC& dc, const CRect& rc, const CString& display, const CString& key, CFont* font);

	// ---- Anh avatar (key = username). Chua co anh thi ve chu cai ----
	void SetAvatarImage(const CString& key, const std::string& bytes);
	bool HasAvatarImage(const CString& key);
	void ClearAvatarImages();
	bool MakeAvatarPng(const CString& file, std::string& png);   // doc anh, cat vuong, thu 160x160, nen PNG
	void DrawOnlineDot(CDC& dc, const CRect& avatar, int size);
}

// Bat/tat GDI+ (ve hinh tron, bo goc muot)
class CGdiplusInit
{
public:
	CGdiplusInit();
	~CGdiplusInit();
private:
	ULONG_PTR m_token = 0;
};
