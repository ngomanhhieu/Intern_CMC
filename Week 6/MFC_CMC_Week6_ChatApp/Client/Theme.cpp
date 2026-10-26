#include "pch.h"
#include <map>
#include <memory>
#include <vector>
#include <shlwapi.h>
#include "Theme.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

namespace theme
{
	static int g_dpi = 96;

	void InitDpi()
	{
		HDC hdc = ::GetDC(NULL);
		g_dpi = GetDeviceCaps(hdc, LOGPIXELSY);
		::ReleaseDC(NULL, hdc);
	}

	int S(int px)
	{
		return MulDiv(px, g_dpi, 96);
	}

	void MakeFont(CFont& font, int px, int weight)
	{
		if (font.GetSafeHandle()) font.DeleteObject();
		font.CreateFont(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));
	}

	std::string ToUtf8(const CString& s)
	{
		if (s.IsEmpty()) return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), NULL, 0, NULL, NULL);
		std::string out(n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(), &out[0], n, NULL, NULL);
		return out;
	}

	CString FromUtf8(const std::string& s)
	{
		if (s.empty()) return CString();
		int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
		CString out;
		MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.GetBuffer(n), n);
		out.ReleaseBuffer(n);
		return out;
	}

	bool SameDay(long long a, long long b)
	{
		CTime ta((time_t)a), tb((time_t)b);
		return ta.GetYear() == tb.GetYear() && ta.GetMonth() == tb.GetMonth() && ta.GetDay() == tb.GetDay();
	}

	CString FormatClock(long long t)
	{
		return CTime((time_t)t).Format(_T("%H:%M"));
	}

	CString FormatLastSeen(long long t)
	{
		if (t <= 0) return _T("Ngoại tuyến");
		long long d = (long long)time(NULL) - t;
		CString s;
		if (d < 60) s = _T("Hoạt động vừa xong");
		else if (d < 3600) s.Format(_T("Hoạt động %lld phút trước"), d / 60);
		else if (d < 24 * 3600) s.Format(_T("Hoạt động %lld giờ trước"), d / 3600);
		else if (d < 7 * 24 * 3600) s.Format(_T("Hoạt động %lld ngày trước"), d / (24 * 3600));
		else s = _T("Ngoại tuyến");
		return s;
	}

	CString FormatShortTime(long long t)
	{
		if (t <= 0) return CString();
		long long now = (long long)time(NULL);
		CTime ct((time_t)t);
		if (SameDay(t, now)) return ct.Format(_T("%H:%M"));
		if (now - t < 6 * 24 * 3600)
		{
			static const TCHAR* days[] = { _T("CN"), _T("T2"), _T("T3"), _T("T4"), _T("T5"), _T("T6"), _T("T7") };
			return days[ct.GetDayOfWeek() - 1];
		}
		return ct.Format(_T("%d/%m"));
	}

	CString FormatSeparator(long long t)
	{
		long long now = (long long)time(NULL);
		CTime ct((time_t)t);
		if (SameDay(t, now)) return ct.Format(_T("%H:%M"));
		if (SameDay(t, now - 24 * 3600)) return ct.Format(_T("%H:%M, Hôm qua"));
		return ct.Format(_T("%H:%M, %d/%m/%Y"));
	}

	// Chu cai dau cua tu cuoi ("Ngo Manh Hieu" -> "H")
	CString Initial(const CString& displayName)
	{
		CString s = displayName;
		s.Trim();
		if (s.IsEmpty()) return _T("?");
		int sp = s.ReverseFind(_T(' '));
		CString c = s.Mid(sp + 1, 1);
		c.MakeUpper();
		return c;
	}

	COLORREF AvatarColor(const CString& key)
	{
		static const COLORREF palette[] = {
			RGB(0, 132, 255), RGB(255, 107, 107), RGB(32, 201, 151), RGB(255, 159, 67),
			RGB(132, 94, 247), RGB(240, 101, 149), RGB(34, 184, 207), RGB(81, 207, 102)
		};
		unsigned h = 0;
		for (int i = 0; i < key.GetLength(); i++)
			h = h * 31 + (unsigned)towlower(key[i]);
		return palette[h % _countof(palette)];
	}

	static Gdiplus::Color ToColor(COLORREF c)
	{
		return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
	}

	static void RoundPath(Gdiplus::GraphicsPath& path, const CRect& rc, int radius)
	{
		float d = (float)radius * 2;
		if (d > rc.Width()) d = (float)rc.Width();
		if (d > rc.Height()) d = (float)rc.Height();
		float x = (float)rc.left, y = (float)rc.top, r = (float)rc.right - 1, b = (float)rc.bottom - 1;
		path.AddArc(x, y, d, d, 180, 90);
		path.AddArc(r - d, y, d, d, 270, 90);
		path.AddArc(r - d, b - d, d, d, 0, 90);
		path.AddArc(x, b - d, d, d, 90, 90);
		path.CloseFigure();
	}

	void FillRound(CDC& dc, const CRect& rc, int radius, COLORREF fill, COLORREF border, int borderWidth)
	{
		Gdiplus::Graphics g(dc.GetSafeHdc());
		g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		Gdiplus::GraphicsPath path;
		RoundPath(path, rc, radius);
		Gdiplus::SolidBrush brush(ToColor(fill));
		g.FillPath(&brush, &path);
		if (border != CLR_NONE)
		{
			Gdiplus::Pen pen(ToColor(border), (float)borderWidth);
			g.DrawPath(&pen, &path);
		}
	}

	void FillCircle(CDC& dc, const CRect& rc, COLORREF fill, COLORREF ring, int ringWidth)
	{
		Gdiplus::Graphics g(dc.GetSafeHdc());
		g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
		if (ring != CLR_NONE && ringWidth > 0)
		{
			Gdiplus::SolidBrush rb(ToColor(ring));
			g.FillEllipse(&rb, (float)rc.left - ringWidth, (float)rc.top - ringWidth,
				(float)rc.Width() + 2 * ringWidth, (float)rc.Height() + 2 * ringWidth);
		}
		Gdiplus::SolidBrush brush(ToColor(fill));
		g.FillEllipse(&brush, (float)rc.left, (float)rc.top, (float)rc.Width(), (float)rc.Height());
	}

	// ---- Anh avatar ----
	static std::map<CString, std::unique_ptr<Gdiplus::Bitmap>> g_avatars;

	static CString AvatarKey(const CString& key)
	{
		CString k = key;
		k.MakeLower();
		return k;
	}

	// Doc anh tu mang byte, copy sang Bitmap rieng (khong phu thuoc stream)
	static Gdiplus::Bitmap* LoadBitmap(const std::string& bytes)
	{
		IStream* stream = SHCreateMemStream((const BYTE*)bytes.data(), (UINT)bytes.size());
		if (!stream) return nullptr;
		Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromStream(stream);
		Gdiplus::Bitmap* copy = nullptr;
		if (src && src->GetLastStatus() == Gdiplus::Ok)
		{
			copy = new Gdiplus::Bitmap(src->GetWidth(), src->GetHeight(), PixelFormat32bppPARGB);
			Gdiplus::Graphics g(copy);
			g.DrawImage(src, 0, 0, src->GetWidth(), src->GetHeight());
		}
		delete src;
		stream->Release();
		return copy;
	}

	void SetAvatarImage(const CString& key, const std::string& bytes)
	{
		Gdiplus::Bitmap* bmp = LoadBitmap(bytes);
		if (bmp) g_avatars[AvatarKey(key)].reset(bmp);
	}

	bool HasAvatarImage(const CString& key)
	{
		return g_avatars.count(AvatarKey(key)) > 0;
	}

	void ClearAvatarImages()
	{
		g_avatars.clear();
	}

	static bool GetPngEncoder(CLSID& clsid)
	{
		UINT n = 0, size = 0;
		Gdiplus::GetImageEncodersSize(&n, &size);
		if (size == 0) return false;
		std::vector<BYTE> buf(size);
		Gdiplus::ImageCodecInfo* codecs = (Gdiplus::ImageCodecInfo*)buf.data();
		Gdiplus::GetImageEncoders(n, size, codecs);
		for (UINT i = 0; i < n; i++)
			if (wcscmp(codecs[i].MimeType, L"image/png") == 0) { clsid = codecs[i].Clsid; return true; }
		return false;
	}

	bool MakeAvatarPng(const CString& file, std::string& png)
	{
		const int SIZE = 160;
		Gdiplus::Bitmap src(file);
		if (src.GetLastStatus() != Gdiplus::Ok || src.GetWidth() == 0) return false;

		// Cat hinh vuong roi thu nho (anh doc: lay phan tren de giu khuon mat)
		int w = src.GetWidth(), h = src.GetHeight(), side = min(w, h);
		int top = h > w ? (h - side) * 15 / 100 : (h - side) / 2;
		Gdiplus::Bitmap dst(SIZE, SIZE, PixelFormat32bppARGB);
		Gdiplus::Graphics g(&dst);
		g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
		g.DrawImage(&src, Gdiplus::Rect(0, 0, SIZE, SIZE), (w - side) / 2, top, side, side, Gdiplus::UnitPixel);

		CLSID clsid;
		IStream* stream = SHCreateMemStream(NULL, 0);
		if (!stream || !GetPngEncoder(clsid) || dst.Save(stream, &clsid) != Gdiplus::Ok)
		{
			if (stream) stream->Release();
			return false;
		}
		STATSTG stat = {};
		stream->Stat(&stat, STATFLAG_NONAME);
		png.resize((size_t)stat.cbSize.QuadPart);
		LARGE_INTEGER zero = {};
		stream->Seek(zero, STREAM_SEEK_SET, NULL);
		ULONG read = 0;
		stream->Read(&png[0], (ULONG)png.size(), &read);
		stream->Release();
		return read == png.size();
	}

	void DrawAvatar(CDC& dc, const CRect& rc, const CString& display, const CString& key, CFont* font)
	{
		// Co anh: to hinh tron bang anh (vien muot)
		auto it = g_avatars.find(AvatarKey(key));
		if (it != g_avatars.end())
		{
			Gdiplus::Bitmap* bmp = it->second.get();
			Gdiplus::Graphics g(dc.GetSafeHdc());
			g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
			g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
			Gdiplus::TextureBrush brush(bmp, Gdiplus::WrapModeTileFlipXY);
			Gdiplus::Matrix m((float)rc.Width() / bmp->GetWidth(), 0, 0, (float)rc.Height() / bmp->GetHeight(),
				(float)rc.left, (float)rc.top);
			brush.SetTransform(&m);
			g.FillEllipse(&brush, (float)rc.left, (float)rc.top, (float)rc.Width(), (float)rc.Height());
			return;
		}

		FillCircle(dc, rc, AvatarColor(key));
		CFont* old = dc.SelectObject(font);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(RGB(255, 255, 255));
		CRect r = rc;
		dc.DrawText(Initial(display), &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.SelectObject(old);
	}

	void DrawOnlineDot(CDC& dc, const CRect& avatar, int size)
	{
		CRect dot(avatar.right - size, avatar.bottom - size, avatar.right, avatar.bottom);
		FillCircle(dc, dot, Online, RGB(255, 255, 255), S(2));
	}
}

CGdiplusInit::CGdiplusInit()
{
	Gdiplus::GdiplusStartupInput input;
	Gdiplus::GdiplusStartup(&m_token, &input, NULL);
}

CGdiplusInit::~CGdiplusInit()
{
	if (m_token) Gdiplus::GdiplusShutdown(m_token);
}
