// RegisterDlg.cpp : man hinh tao tai khoan
// Kiem tra du lieu o client truoc, server kiem tra lai
#include "pch.h"
#include "ChatClient.h"
#include "RegisterDlg.h"
#include "Theme.h"

using theme::S;

static const int W = 400;
static const int H = 630;
static const int X0 = 40, X1 = 360;
static const CRect RC_USER(X0, 140, X1, 184);      // goi y o duoi: 188-206
static const CRect RC_DISPLAY(X0, 240, X1, 284);   // goi y o duoi: 288-306
static const CRect RC_PASS(X0, 342, X1, 386);
static const CRect RC_PASS2(X0, 420, X1, 464);
static const CRect RC_STATUS(X0, 470, X1, 508);
static const CRect RC_REGISTER(X0, 512, X1, 558);
static const CRect RC_BACK(X0, 570, X1, 600);

CRegisterDlg::CRegisterDlg(const CString& server, int port, CWnd* pParent)
	: CThemedDialog(IDD_REGISTER, pParent), m_server(server), m_port(port)
{
}

BEGIN_MESSAGE_MAP(CRegisterDlg, CThemedDialog)
	ON_BN_CLICKED(IDC_BTN_REGISTER, &CRegisterDlg::OnClickedRegister)
	ON_BN_CLICKED(IDC_BTN_BACK, &CRegisterDlg::OnClickedBack)
	ON_MESSAGE(WM_NET_EVENT, &CRegisterDlg::OnNetEvent)
END_MESSAGE_MAP()

BOOL CRegisterDlg::OnInitDialog()
{
	CThemedDialog::OnInitDialog();

	SetClientSize(W, H);
	SetField(IDC_REG_USER, RC_USER);
	SetField(IDC_REG_DISPLAY, RC_DISPLAY);
	SetField(IDC_REG_PASS, RC_PASS);
	SetField(IDC_REG_PASS2, RC_PASS2);
	MoveCtrl(IDC_BTN_REGISTER, RC_REGISTER);
	MoveCtrl(IDC_BTN_BACK, RC_BACK);
	SetButtonStyle(IDC_BTN_REGISTER, BTN_PRIMARY);
	SetButtonStyle(IDC_BTN_BACK, BTN_LINK);

	((CEdit*)GetDlgItem(IDC_REG_USER))->SetCueBanner(_T("vd: hieu_ngo"));
	((CEdit*)GetDlgItem(IDC_REG_DISPLAY))->SetCueBanner(_T("vd: Ngô Mạnh Hiếu"));
	((CEdit*)GetDlgItem(IDC_REG_PASS))->SetCueBanner(_T("Ít nhất 6 ký tự"));
	((CEdit*)GetDlgItem(IDC_REG_PASS2))->SetCueBanner(_T("Nhập lại mật khẩu"));
	((CEdit*)GetDlgItem(IDC_REG_USER))->SetLimitText(20);
	((CEdit*)GetDlgItem(IDC_REG_DISPLAY))->SetLimitText(32);
	((CEdit*)GetDlgItem(IDC_REG_PASS))->SetLimitText(64);
	((CEdit*)GetDlgItem(IDC_REG_PASS2))->SetLimitText(64);

	GotoDlgCtrl(GetDlgItem(IDC_REG_USER));
	return FALSE;
}

void CRegisterDlg::PaintContent(CDC& dc, const CRect& client)
{
	dc.SelectObject(&m_fontTitle);
	dc.SetTextColor(theme::Text);
	CRect r(0, S(28), client.right, S(68));
	dc.DrawText(_T("Tạo tài khoản"), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

	dc.SelectObject(&m_fontNormal);
	dc.SetTextColor(theme::SubText);
	r = CRect(0, S(68), client.right, S(94));
	dc.DrawText(_T("Server: ") + m_server, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

	dc.SelectObject(&m_fontBold);
	dc.SetTextColor(theme::Text);
	auto label = [&](const CRect& field, LPCTSTR text) {
		CRect lr(S(field.left), S(field.top - 24), S(field.right), S(field.top - 4));
		dc.DrawText(text, &lr, DT_LEFT | DT_SINGLELINE | DT_BOTTOM);
	};
	label(RC_USER, _T("Tên đăng nhập"));
	label(RC_DISPLAY, _T("Tên hiển thị"));
	label(RC_PASS, _T("Mật khẩu"));
	label(RC_PASS2, _T("Nhập lại mật khẩu"));

	// Goi y duoi o ten dang nhap / ten hien thi
	dc.SelectObject(&m_fontSmall);
	dc.SetTextColor(theme::SubText);
	auto hint = [&](const CRect& field, LPCTSTR text) {
		CRect hr(S(field.left + 2), S(field.bottom + 4), S(field.right), S(field.bottom + 22));
		dc.DrawText(text, &hr, DT_LEFT | DT_SINGLELINE | DT_TOP);
	};
	hint(RC_USER, _T("3-20 ký tự: chữ không dấu, số, dấu _ hoặc dấu chấm"));
	hint(RC_DISPLAY, _T("Tên mọi người sẽ thấy, có thể viết tiếng Việt"));

	if (!m_status.IsEmpty())
	{
		dc.SetTextColor(m_statusError ? theme::Error : theme::SubText);
		CRect sr(S(RC_STATUS.left), S(RC_STATUS.top), S(RC_STATUS.right), S(RC_STATUS.bottom));
		dc.DrawText(m_status, &sr, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	}
}

void CRegisterDlg::SetStatus(const CString& text, bool error)
{
	m_status = text;
	m_statusError = error;
	CRect sr(S(RC_STATUS.left), S(RC_STATUS.top), S(RC_STATUS.right), S(RC_STATUS.bottom));
	InvalidateRect(sr, FALSE);
	UpdateWindow();
}

void CRegisterDlg::SetBusy(bool busy)
{
	m_busy = busy;
	UINT ids[] = { IDC_REG_USER, IDC_REG_DISPLAY, IDC_REG_PASS, IDC_REG_PASS2, IDC_BTN_REGISTER, IDC_BTN_BACK };
	for (UINT id : ids)
		GetDlgItem(id)->EnableWindow(!busy);
}

// Tra ve "" neu hop le
CString CRegisterDlg::Validate(CString& user, CString& display, CString& pass)
{
	CString pass2;
	GetDlgItemText(IDC_REG_USER, user);
	GetDlgItemText(IDC_REG_DISPLAY, display);
	GetDlgItemText(IDC_REG_PASS, pass);
	GetDlgItemText(IDC_REG_PASS2, pass2);
	user.Trim();
	display.Trim();

	if (user.GetLength() < 3 || user.GetLength() > 20)
		return _T("Tên đăng nhập phải dài 3-20 ký tự.");
	for (int i = 0; i < user.GetLength(); i++)
	{
		TCHAR c = user[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
		if (!ok) return _T("Tên đăng nhập chỉ gồm chữ không dấu, số, dấu _ hoặc dấu chấm.");
	}
	if (display.IsEmpty())
		return _T("Hãy nhập tên hiển thị.");
	if (pass.GetLength() < 6)
		return _T("Mật khẩu phải có ít nhất 6 ký tự.");
	if (pass != pass2)
		return _T("Hai mật khẩu không khớp nhau.");
	return CString();
}

void CRegisterDlg::OnOK()
{
	OnClickedRegister();
}

void CRegisterDlg::OnClickedRegister()
{
	if (m_busy) return;

	CString user, display, pass;
	CString err = Validate(user, display, pass);
	if (!err.IsEmpty())
	{
		SetStatus(err, true);
		return;
	}

	SetBusy(true);
	SetStatus(_T("Đang kết nối tới server..."), false);
	{
		CWaitCursor wait;
		if (!m_net.Connect(m_server, m_port, err))
		{
			SetBusy(false);
			SetStatus(err, true);
			return;
		}
	}
	m_net.SetNotifyWindow(m_hWnd);
	m_net.Send(proto::REGISTER, json::Value::Object()
		.Set("username", theme::ToUtf8(user))
		.Set("password", proto::HashPasswordForWire(theme::ToUtf8(pass)))
		.Set("display_name", theme::ToUtf8(display)));
	SetStatus(_T("Đang tạo tài khoản..."), false);
	m_registeredUser = user;
}

LRESULT CRegisterDlg::OnNetEvent(WPARAM, LPARAM)
{
	proto::Packet p;
	while (m_net.Pop(p))
	{
		if (p.action == proto::REGISTER && p.body["status"].Str() == "ok")
		{
			m_net.Disconnect();
			EndDialog(IDOK);
			return 0;
		}
		if (p.action == proto::REGISTER || p.action == proto::ERROR_MSG || p.action == NET_CLOSED)
		{
			m_net.Disconnect();
			SetBusy(false);
			SetStatus(p.action == NET_CLOSED ? CString(_T("Mất kết nối tới server.")) : theme::FromUtf8(p.body["reason"].Str()), true);
			return 0;
		}
	}
	return 0;
}

void CRegisterDlg::OnClickedBack()
{
	m_net.Disconnect();
	EndDialog(IDCANCEL);
}
