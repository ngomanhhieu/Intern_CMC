#include "pch.h"
#include "framework.h"
#include "MFC_CMC_Week2.h"
#include "MFC_CMC_Week2Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CMFCCMCWeek2Dlg dialog

CMFCCMCWeek2Dlg::CMFCCMCWeek2Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFC_CMC_WEEK2_DIALOG, pParent)
	, m_strUsername(_T(""))
	, m_strPassword(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCCMCWeek2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_USERNAME, m_strUsername);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_strPassword);
}

BEGIN_MESSAGE_MAP(CMFCCMCWeek2Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_MOUSEACTIVATE()
	ON_BN_CLICKED(IDC_BTN_LOGIN, &CMFCCMCWeek2Dlg::OnClickedBtnLogin)
	ON_BN_CLICKED(IDC_BTN_REGISTER, &CMFCCMCWeek2Dlg::OnClickedBtnRegister)
END_MESSAGE_MAP()

BOOL CMFCCMCWeek2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	return TRUE;
}

void CMFCCMCWeek2Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void CMFCCMCWeek2Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CMFCCMCWeek2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CMFCCMCWeek2Dlg::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	return MA_ACTIVATE;   // click đầu tiên vừa kích hoạt cửa sổ vừa thực hiện luôn
}

void CMFCCMCWeek2Dlg::OnClickedBtnLogin()
{
	UpdateData(TRUE);

	bool ok = false;
	for (int i = 0; i < ACCOUNT_COUNT; i++)
	{
		if (m_strUsername == g_accounts[i].user && m_strPassword == g_accounts[i].pass)
			ok = true;
	}
	if (!ok)
	{
		MessageBox(_T("Sai tên đăng nhập hoặc mật khẩu!"), _T("Lỗi"), MB_ICONWARNING);
		return;
	}

	ShowWindow(SW_HIDE);          // ẩn dialog đăng nhập

	CMainDlg dlgMain;
	dlgMain.m_username = m_strUsername;
	dlgMain.DoModal();

	if (dlgMain.m_startFailed)    // khong mo duoc cong TCP -> quay lai man hinh dang nhap
	{
		ShowWindow(SW_SHOW);
		return;
	}

	EndDialog(IDOK);              // đóng chat xong thì thoát luôn app
}

void CMFCCMCWeek2Dlg::OnClickedBtnRegister()
{
	MessageBox(_T("Chưa có database nên chưa đăng ký được.\nDùng tài khoản có sẵn: hieu / mai / nam (mật khẩu 123)."),
		_T("Thông báo"), MB_ICONINFORMATION);
}