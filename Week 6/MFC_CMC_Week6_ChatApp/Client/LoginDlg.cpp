// LoginDlg.cpp : man hinh dang nhap
// Ket noi server -> gui LOGIN -> thanh cong thi giu ket noi cho man hinh chat
#include "pch.h"
#include "ChatClient.h"
#include "LoginDlg.h"
#include "RegisterDlg.h"
#include "Theme.h"

using theme::S;

// Bo cuc (pixel o 96 DPI)
static const int W = 400;
static const int H = 590;
static const int X0 = 40, X1 = 360;
static const CRect RC_SERVER(X0, 236, 268, 280);
static const CRect RC_PORT(278, 236, X1, 280);
static const CRect RC_USER(X0, 314, X1, 358);
static const CRect RC_PASS(X0, 392, X1, 436);
static const CRect RC_STATUS(X0, 442, X1, 468);
static const CRect RC_LOGIN(X0, 474, X1, 520);
static const CRect RC_REGISTER(X0, 532, X1, 562);

CLoginDlg::CLoginDlg(CWnd* pParent)
	: CThemedDialog(IDD_LOGIN, pParent)
{
}

BEGIN_MESSAGE_MAP(CLoginDlg, CThemedDialog)
	ON_BN_CLICKED(IDC_BTN_LOGIN, &CLoginDlg::OnClickedLogin)
	ON_BN_CLICKED(IDC_BTN_GOREGISTER, &CLoginDlg::OnClickedGoRegister)
	ON_MESSAGE(WM_NET_EVENT, &CLoginDlg::OnNetEvent)
END_MESSAGE_MAP()

BOOL CLoginDlg::OnInitDialog()
{
	CThemedDialog::OnInitDialog();

	SetClientSize(W, H);
	SetField(IDC_SERVER, RC_SERVER);
	SetField(IDC_PORT, RC_PORT);
	SetField(IDC_USERNAME, RC_USER);
	SetField(IDC_PASSWORD, RC_PASS);
	MoveCtrl(IDC_BTN_LOGIN, RC_LOGIN);
	MoveCtrl(IDC_BTN_GOREGISTER, RC_REGISTER);
	SetButtonStyle(IDC_BTN_LOGIN, BTN_PRIMARY);
	SetButtonStyle(IDC_BTN_GOREGISTER, BTN_LINK);

	((CEdit*)GetDlgItem(IDC_SERVER))->SetCueBanner(_T("Địa chỉ IP server"));
	((CEdit*)GetDlgItem(IDC_PORT))->SetCueBanner(_T("Cổng"));
	((CEdit*)GetDlgItem(IDC_USERNAME))->SetCueBanner(_T("Tên đăng nhập"));
	((CEdit*)GetDlgItem(IDC_PASSWORD))->SetCueBanner(_T("Mật khẩu"));

	// Dien san server va tai khoan lan truoc
	CWinApp* app = AfxGetApp();
	SetDlgItemText(IDC_SERVER, app->GetProfileString(_T("Login"), _T("Server"), _T("127.0.0.1")));
	SetDlgItemInt(IDC_PORT, app->GetProfileInt(_T("Login"), _T("Port"), proto::DEFAULT_PORT), FALSE);
	CString lastUser = app->GetProfileString(_T("Login"), _T("User"), _T(""));
	SetDlgItemText(IDC_USERNAME, lastUser);

	GotoDlgCtrl(GetDlgItem(lastUser.IsEmpty() ? IDC_USERNAME : IDC_PASSWORD));
	return FALSE;   // da tu dat focus
}

void CLoginDlg::PaintContent(CDC& dc, const CRect& client)
{
	// Logo
	CRect logo(S(W / 2 - 38), S(36), S(W / 2 + 38), S(112));
	theme::FillCircle(dc, logo, theme::Primary);
	CRect bubble(logo.CenterPoint().x - S(20), logo.CenterPoint().y - S(15), logo.CenterPoint().x + S(20), logo.CenterPoint().y + S(13));
	theme::FillRound(dc, bubble, S(12), RGB(255, 255, 255));
	POINT tail[3] = { { bubble.left + S(8), bubble.bottom - S(2) }, { bubble.left + S(4), bubble.bottom + S(8) }, { bubble.left + S(18), bubble.bottom - S(2) } };
	CBrush white(RGB(255, 255, 255));
	CPen nopen(PS_NULL, 0, RGB(0, 0, 0));
	CBrush* oldBr = dc.SelectObject(&white);
	CPen* oldPen = dc.SelectObject(&nopen);
	dc.Polygon(tail, 3);
	dc.SelectObject(oldBr);
	dc.SelectObject(oldPen);
	for (int i = -1; i <= 1; i++)
	{
		CPoint c(bubble.CenterPoint().x + i * S(10), bubble.CenterPoint().y);
		theme::FillCircle(dc, CRect(c.x - S(3), c.y - S(3), c.x + S(3), c.y + S(3)), theme::Primary);
	}

	dc.SelectObject(&m_fontTitle);
	dc.SetTextColor(theme::Text);
	CRect r(0, S(124), client.right, S(164));
	dc.DrawText(_T("CMC Chat"), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	dc.SelectObject(&m_fontNormal);
	dc.SetTextColor(theme::SubText);
	r = CRect(0, S(164), client.right, S(190));
	dc.DrawText(_T("Đăng nhập để bắt đầu trò chuyện"), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	// Nhan phia tren moi o
	dc.SelectObject(&m_fontBold);
	dc.SetTextColor(theme::Text);
	auto label = [&](const CRect& field, LPCTSTR text) {
		CRect lr(S(field.left), S(field.top - 24), S(field.right), S(field.top - 4));
		dc.DrawText(text, &lr, DT_LEFT | DT_SINGLELINE | DT_BOTTOM);
	};
	label(RC_SERVER, _T("Server"));
	label(RC_PORT, _T("Cổng"));
	label(RC_USER, _T("Tên đăng nhập"));
	label(RC_PASS, _T("Mật khẩu"));

	if (!m_status.IsEmpty())
	{
		dc.SelectObject(&m_fontSmall);
		dc.SetTextColor(m_statusError ? theme::Error : theme::SubText);
		CRect sr(S(RC_STATUS.left), S(RC_STATUS.top), S(RC_STATUS.right), S(RC_STATUS.bottom));
		dc.DrawText(m_status, &sr, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	}
}

void CLoginDlg::SetStatus(const CString& text, bool error)
{
	m_status = text;
	m_statusError = error;
	CRect sr(S(RC_STATUS.left), S(RC_STATUS.top), S(RC_STATUS.right), S(RC_STATUS.bottom));
	InvalidateRect(sr, FALSE);
	UpdateWindow();
}

void CLoginDlg::SetBusy(bool busy)
{
	m_busy = busy;
	UINT ids[] = { IDC_SERVER, IDC_PORT, IDC_USERNAME, IDC_PASSWORD, IDC_BTN_LOGIN, IDC_BTN_GOREGISTER };
	for (UINT id : ids)
		GetDlgItem(id)->EnableWindow(!busy);
}

void CLoginDlg::OnOK()
{
	OnClickedLogin();   // Enter = dang nhap
}

void CLoginDlg::OnClickedLogin()
{
	if (m_busy) return;

	CString server, user, pass;
	GetDlgItemText(IDC_SERVER, server);
	GetDlgItemText(IDC_USERNAME, user);
	GetDlgItemText(IDC_PASSWORD, pass);
	int port = (int)GetDlgItemInt(IDC_PORT, NULL, FALSE);
	server.Trim();
	user.Trim();

	if (server.IsEmpty() || port <= 0 || port > 65535) { SetStatus(_T("Hãy nhập địa chỉ server và cổng hợp lệ."), true); return; }
	if (user.IsEmpty()) { SetStatus(_T("Hãy nhập tên đăng nhập."), true); GotoDlgCtrl(GetDlgItem(IDC_USERNAME)); return; }
	if (pass.IsEmpty()) { SetStatus(_T("Hãy nhập mật khẩu."), true); GotoDlgCtrl(GetDlgItem(IDC_PASSWORD)); return; }

	SetBusy(true);
	SetStatus(_T("Đang kết nối tới server..."), false);

	CString err;
	{
		CWaitCursor wait;
		if (!theApp.m_net.Connect(server, port, err))
		{
			SetBusy(false);
			SetStatus(err, true);
			return;
		}
	}

	CWinApp* app = AfxGetApp();
	app->WriteProfileString(_T("Login"), _T("Server"), server);
	app->WriteProfileInt(_T("Login"), _T("Port"), port);

	// Chi gui SHA-256 cua mat khau
	theApp.m_net.SetNotifyWindow(m_hWnd);
	theApp.m_net.Send(proto::LOGIN, json::Value::Object()
		.Set("username", theme::ToUtf8(user))
		.Set("password", proto::HashPasswordForWire(theme::ToUtf8(pass))));
	SetStatus(_T("Đang đăng nhập..."), false);
}

LRESULT CLoginDlg::OnNetEvent(WPARAM, LPARAM)
{
	proto::Packet p;
	while (theApp.m_net.Pop(p))
	{
		if (p.action == proto::LOGIN && p.body["status"].Str() == "ok")
		{
			theApp.m_user = theme::FromUtf8(p.body["username"].Str());
			theApp.m_display = theme::FromUtf8(p.body["display_name"].Str());
			AfxGetApp()->WriteProfileString(_T("Login"), _T("User"), theApp.m_user);

			// Goi tin den sau de lai cho man hinh chat
			theApp.m_net.SetNotifyWindow(NULL);
			EndDialog(IDOK);
			return 0;
		}
		if (p.action == proto::LOGIN || p.action == NET_CLOSED)
		{
			theApp.m_net.Disconnect();
			SetBusy(false);
			SetStatus(p.action == NET_CLOSED ? CString(_T("Mất kết nối tới server.")) : theme::FromUtf8(p.body["reason"].Str()), true);
			GotoDlgCtrl(GetDlgItem(IDC_PASSWORD));
			((CEdit*)GetDlgItem(IDC_PASSWORD))->SetSel(0, -1);
			return 0;
		}
	}
	return 0;
}

void CLoginDlg::OnClickedGoRegister()
{
	CString server;
	GetDlgItemText(IDC_SERVER, server);
	int port = (int)GetDlgItemInt(IDC_PORT, NULL, FALSE);

	CRegisterDlg dlg(server.Trim(), port, this);
	if (dlg.DoModal() == IDOK)
	{
		SetDlgItemText(IDC_USERNAME, dlg.m_registeredUser);
		SetDlgItemText(IDC_PASSWORD, _T(""));
		SetStatus(_T("Tạo tài khoản thành công! Hãy nhập mật khẩu để đăng nhập."), false);
		GotoDlgCtrl(GetDlgItem(IDC_PASSWORD));
	}
}
