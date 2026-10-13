
#include "pch.h"
#include "framework.h"
#include "Task1_CMC.h"
#include "Task1_CMCDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include <ShlObj.h> //thu vien de mo folder
#include <atlconv.h>

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
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


// CTask1CMCDlg dialog



CTask1CMCDlg::CTask1CMCDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TASK1_CMC_DIALOG, pParent)
	, m_strContent(_T(""))
	, m_strPath(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTask1CMCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_CONTENT, m_strContent);
	DDX_Text(pDX, IDC_EDIT_PATH, m_strPath);
}

BEGIN_MESSAGE_MAP(CTask1CMCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_EN_CHANGE(IDC_EDIT_CONTENT, &CTask1CMCDlg::OnEnChangeEditContent)
	ON_BN_CLICKED(IDC_BTN_CHOOSE, &CTask1CMCDlg::OnClickedBtnChoose)
	ON_BN_CLICKED(IDC_BTN_LOAD, &CTask1CMCDlg::OnClickedBtnLoad)
	ON_BN_CLICKED(IDC_BTN_SAVE, &CTask1CMCDlg::OnClickedBtnSave)
	ON_BN_CLICKED(IDCANCEL, &CTask1CMCDlg::OnBnClickedCancel)
END_MESSAGE_MAP()




BOOL CTask1CMCDlg::OnInitDialog()
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

void CTask1CMCDlg::OnSysCommand(UINT nID, LPARAM lParam)
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



void CTask1CMCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}


HCURSOR CTask1CMCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}






void CTask1CMCDlg::OnEnChangeEditContent()
{
	
}

void CTask1CMCDlg::OnClickedBtnChoose()
{	
	UpdateData(TRUE);
	BROWSEINFO bi = { 0 };
	bi.hwndOwner = this->GetSafeHwnd();
	bi.lpszTitle = _T("Chọn thư mục lưu file");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl != NULL)
	{
		TCHAR path[MAX_PATH];
		SHGetPathFromIDList(pidl, path);
		m_folderPath = path;
		m_strPath = path;
		UpdateData(FALSE);
		CoTaskMemFree(pidl);
	}
}

void CTask1CMCDlg::OnClickedBtnLoad()
{
	if (m_folderPath.IsEmpty())
	{
		MessageBox(_T("Chưa chọn folder chứa file!"), _T("Lỗi"), MB_ICONWARNING);
		return;
	}
	CString fullPath = m_folderPath + _T("\\output.txt");

	HANDLE hFile = CreateFile(
		fullPath,
		GENERIC_READ,               // mở để đọc
		0,
		NULL,
		OPEN_EXISTING,               // chỉ mở nếu file đã tồn tại
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD size = GetFileSize(hFile, NULL);
		CStringA buffer;
		DWORD bytesRead;
		ReadFile(hFile, buffer.GetBuffer(size), size, &bytesRead, NULL);
		buffer.ReleaseBuffer(bytesRead);
		CloseHandle(hFile);

		m_strContent = CA2T(buffer, CP_UTF8);
		UpdateData(FALSE);
	}
	else
	{
		MessageBox(_T("Không tìm thấy file!"), _T("Lỗi"), MB_ICONERROR);
	}
}

void CTask1CMCDlg::OnClickedBtnSave()
{
	UpdateData(TRUE);
	if (m_folderPath.IsEmpty())
	{
		MessageBox(_T("Chưa chọn folder lưu file!"), _T("Lỗi"), MB_ICONWARNING);
		return;
	}
	CString fullPath = m_folderPath + _T("\\output.txt");

	HANDLE hFile = CreateFile(
		fullPath,                  // đường dẫn file
		GENERIC_WRITE,              // mở để ghi
		0,                          // không chia sẻ
		NULL,                       // security mặc định
		CREATE_ALWAYS,              // tạo mới / ghi đè nếu đã có
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		CT2CA utf8content(m_strContent, CP_UTF8);
		DWORD bytesWritten;
		WriteFile(hFile, (LPCSTR)utf8content, (DWORD)strlen((LPCSTR)utf8content), &bytesWritten, NULL);
		CloseHandle(hFile);
		MessageBox(_T("Đã lưu file thành công!"), _T("Thông báo"), MB_ICONINFORMATION);
	}
	else
	{
		MessageBox(_T("Không thể lưu file!"), _T("Lỗi"), MB_ICONERROR);
	}
}

void CTask1CMCDlg::OnBnClickedCancel()
{
	
	CDialogEx::OnCancel();
}

