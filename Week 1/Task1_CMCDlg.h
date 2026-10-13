
// Task1_CMCDlg.h : header file
//

#pragma once


// CTask1CMCDlg dialog
class CTask1CMCDlg : public CDialogEx
{
// Construction
public:
	CTask1CMCDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TASK1_CMC_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	
	afx_msg void OnEnChangeEditContent();
	CString m_strContent;
	CString m_strPath;
	CString m_folderPath;
	afx_msg void OnClickedBtnChoose();
	afx_msg void OnClickedBtnLoad();
	afx_msg void OnClickedBtnSave();
	afx_msg void OnBnClickedCancel();
};
