// CMainDlg.cpp : implementation file
#include "pch.h"
#include "MFC_CMC_Week2.h"
#include "afxdialogex.h"
#include "CMainDlg.h"
#include <atlimage.h>

#define CHAT_TITLE_PREFIX _T("CMC Chat - ")
#define ROW_HEIGHT        26
#define BUBBLE_WIDTH      220
#define BUBBLE_PAD        10
#define TIME_GAP_MINUTES  5
#define TIME_GUTTER       44

struct FindData
{
	CString title;
	HWND exclude;
	HWND found;
};

static BOOL CALLBACK EnumChatProc(HWND hwnd, LPARAM lParam)
{
	FindData* fd = (FindData*)lParam;
	if (hwnd == fd->exclude) return TRUE;

	TCHAR buf[256];
	GetWindowText(hwnd, buf, 256);
	if (fd->title == buf)
	{
		fd->found = hwnd;
		return FALSE;
	}
	return TRUE;
}

HWND CMainDlg::FindChatWindow(const CString& user, HWND exclude)
{
	FindData fd;
	fd.title = CString(CHAT_TITLE_PREFIX) + user;
	fd.exclude = exclude;
	fd.found = NULL;
	EnumWindows(EnumChatProc, (LPARAM)&fd);
	return fd.found;
}

static void WrapText(CDC& dc, const CString& text, int maxWidth, std::vector<CString>& lines)
{
	lines.clear();
	CString line;
	int lastSpace = -1;

	for (int i = 0; i < text.GetLength(); i++)
	{
		TCHAR ch = text[i];
		if (ch == _T('\r')) continue;
		if (ch == _T('\n'))
		{
			lines.push_back(line);
			line.Empty();
			lastSpace = -1;
			continue;
		}

		line += ch;
		if (ch == _T(' ')) lastSpace = line.GetLength() - 1;

		if (dc.GetTextExtent(line).cx > maxWidth && line.GetLength() > 1)
		{
			if (lastSpace > 0)
			{
				lines.push_back(line.Left(lastSpace));
				line = line.Mid(lastSpace + 1);
			}
			else
			{
				lines.push_back(line.Left(line.GetLength() - 1));
				line = line.Right(1);
			}
			lastSpace = line.ReverseFind(_T(' '));
		}
	}
	if (!line.IsEmpty() || lines.empty())
		lines.push_back(line);
}

IMPLEMENT_DYNAMIC(CMainDlg, CDialogEx)

CMainDlg::CMainDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MAIN_DIALOG, pParent)
	, m_strMessage(_T(""))
{
}

CMainDlg::~CMainDlg()
{
}

void CMainDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_CHAT, m_listChat);
	DDX_Control(pDX, IDC_LIST_USERS, m_listUsers);
	DDX_Text(pDX, IDC_EDIT_MESSAGE, m_strMessage);
}

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SEND, &CMainDlg::OnClickedBtnSend)
	ON_WM_DRAWITEM()
	ON_WM_COPYDATA()
	ON_WM_MOUSEACTIVATE()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_USERS, &CMainDlg::OnLvnItemchangedListUsers)
END_MESSAGE_MAP()

int CMainDlg::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	return MA_ACTIVATE;
}

int CMainDlg::AddAvatar(const CString& name)
{
	const int S = 44;

	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);
	CString dir(exePath);
	dir = dir.Left(dir.ReverseFind(_T('\\')));
	CString file = dir + _T("\\avatars\\") + name + _T(".png");

	CClientDC dcScreen(this);
	CDC dc;
	dc.CreateCompatibleDC(&dcScreen);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dcScreen, S, S);
	CBitmap* pOldBmp = dc.SelectObject(&bmp);

	dc.FillSolidRect(0, 0, S, S, RGB(255, 255, 255));

	CRgn rgn;
	rgn.CreateEllipticRgn(0, 0, S, S);
	dc.SelectClipRgn(&rgn);

	CImage img;
	if (SUCCEEDED(img.Load(file)))
	{
		dc.SetStretchBltMode(HALFTONE);
		img.Draw(dc.GetSafeHdc(), 0, 0, S, S);
	}
	else
	{
		dc.FillSolidRect(0, 0, S, S, RGB(0, 132, 255));
		dc.SetTextColor(RGB(255, 255, 255));
		dc.SetBkMode(TRANSPARENT);
		CFont* pOldFont = dc.SelectObject(&m_fontHeader);
		CString letter = name.Left(1);
		letter.MakeUpper();
		CRect r(0, 0, S, S);
		dc.DrawText(letter, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOldFont);
	}

	dc.SelectClipRgn(NULL);
	dc.SelectObject(pOldBmp);

	return m_imgListUsers.Add(&bmp, (CBitmap*)NULL);
}

BOOL CMainDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(CString(CHAT_TITLE_PREFIX) + m_username);

	CRect rect;
	m_listChat.GetClientRect(&rect);
	m_listChat.InsertColumn(0, _T(""), LVCFMT_LEFT, rect.Width() - GetSystemMetrics(SM_CXVSCROLL) - 4);
	m_imgListHeight.Create(1, ROW_HEIGHT, ILC_COLOR32, 1, 1);
	m_listChat.SetImageList(&m_imgListHeight, LVSIL_SMALL);

	m_listUsers.SetExtendedStyle(m_listUsers.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	m_listUsers.GetClientRect(&rect);
	m_listUsers.InsertColumn(0, _T(""), LVCFMT_LEFT, rect.Width() - 4);
	m_imgListUsers.Create(44, 44, ILC_COLOR32, 3, 3);
	m_listUsers.SetImageList(&m_imgListUsers, LVSIL_SMALL);

	m_fontList.CreateFont(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));
	m_listUsers.SetFont(&m_fontList);

	m_fontHeader.CreateFont(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));
	GetDlgItem(IDC_STATIC_PEER)->SetFont(&m_fontHeader);

	m_fontSmall.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("Segoe UI"));

	for (int i = 0; i < ACCOUNT_COUNT; i++)
	{
		CString name = g_accounts[i].user;
		if (name == m_username) continue;

		m_peers.push_back(name);
		m_unread.push_back(false);
		int img = AddAvatar(name);
		m_listUsers.InsertItem((int)m_peers.size() - 1, name, img);
	}

	if (!m_peers.empty())
		m_listUsers.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	return TRUE;
}

void CMainDlg::OnOK()
{
	if (::GetFocus() == GetDlgItem(IDC_EDIT_MESSAGE)->GetSafeHwnd())
		OnClickedBtnSend();
}

void CMainDlg::OnLvnItemchangedListUsers(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;
	if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_SELECTED))
		SelectPeer(pNMLV->iItem);
}

void CMainDlg::SelectPeer(int index)
{
	if (index < 0 || index >= (int)m_peers.size()) return;

	m_currentPeer = m_peers[index];
	m_unread[index] = false;
	UpdatePeerLabel(index);

	SetDlgItemText(IDC_STATIC_PEER, m_currentPeer);

	m_rows.clear();
	m_hasLastTime = false;
	m_listChat.DeleteAllItems();
	std::vector<ChatMessage>& hist = m_history[m_currentPeer];
	for (size_t i = 0; i < hist.size(); i++)
		AppendRows(hist[i]);
	if (m_listChat.GetItemCount() > 0)
		m_listChat.EnsureVisible(m_listChat.GetItemCount() - 1, FALSE);
}

void CMainDlg::UpdatePeerLabel(int index)
{
	CString label = m_peers[index];
	if (m_unread[index]) label += _T("  (mới)");
	m_listUsers.SetItemText(index, 0, label);
}

void CMainDlg::AppendRows(const ChatMessage& msg)
{
	if (!m_hasLastTime || (msg.time - m_lastTime).GetTotalMinutes() >= TIME_GAP_MINUTES)
	{
		CTime now = CTime::GetCurrentTime();
		bool today = (msg.time.GetYear() == now.GetYear()
			&& msg.time.GetMonth() == now.GetMonth()
			&& msg.time.GetDay() == now.GetDay());

		ChatRow tr;
		tr.lineIndex = 0;
		tr.lineCount = 1;
		tr.isMe = false;
		tr.isTime = true;
		tr.text = msg.time.Format(today ? _T("%H:%M") : _T("%H:%M  %d/%m/%Y"));
		m_rows.push_back(tr);
		m_listChat.InsertItem((int)m_rows.size() - 1, _T(""));
	}
	m_lastTime = msg.time;
	m_hasLastTime = true;

	CClientDC dc(&m_listChat);
	CFont* pFont = m_listChat.GetFont();
	CFont* pOldFont = pFont ? dc.SelectObject(pFont) : NULL;

	std::vector<CString> lines;
	WrapText(dc, msg.text, BUBBLE_WIDTH - 2 * BUBBLE_PAD, lines);

	if (pOldFont) dc.SelectObject(pOldFont);

	CString timeStr = msg.time.Format(_T("%H:%M"));
	for (int i = 0; i < (int)lines.size(); i++)
	{
		ChatRow row;
		row.lineIndex = i;
		row.lineCount = (int)lines.size();
		row.isMe = msg.isMe;
		row.isTime = false;
		row.text = lines[i];
		row.timeStr = timeStr;
		m_rows.push_back(row);
		m_listChat.InsertItem((int)m_rows.size() - 1, _T(""));
	}
}

void CMainDlg::AddMessage(const CString& peer, const CString& text, bool isMe)
{
	ChatMessage msg;
	msg.text = text;
	msg.isMe = isMe;
	msg.time = CTime::GetCurrentTime();
	m_history[peer].push_back(msg);

	if (peer == m_currentPeer)
	{
		AppendRows(msg);
		m_listChat.EnsureVisible(m_listChat.GetItemCount() - 1, FALSE);
	}
	else
	{
		for (int i = 0; i < (int)m_peers.size(); i++)
		{
			if (m_peers[i] == peer)
			{
				m_unread[i] = true;
				UpdatePeerLabel(i);
			}
		}
	}
}

void CMainDlg::OnClickedBtnSend()
{
	UpdateData(TRUE);
	if (m_strMessage.IsEmpty()) return;
	if (m_currentPeer.IsEmpty())
	{
		MessageBox(_T("Hãy chọn người để nhắn ở danh sách bên trái!"), _T("Thông báo"), MB_ICONINFORMATION);
		return;
	}

	CString text = m_strMessage;
	AddMessage(m_currentPeer, text, true);

	HWND hPeer = FindChatWindow(m_currentPeer, GetSafeHwnd());
	if (hPeer != NULL)
	{
		CString payload = m_username + _T("\t") + text;
		COPYDATASTRUCT cds;
		cds.dwData = 1;
		cds.cbData = (DWORD)((payload.GetLength() + 1) * sizeof(TCHAR));
		cds.lpData = (PVOID)(LPCTSTR)payload;
		::SendMessage(hPeer, WM_COPYDATA, (WPARAM)GetSafeHwnd(), (LPARAM)&cds);
	}
	else
	{
		MessageBox(_T("Người này chưa online (chưa mở cửa sổ đăng nhập) nên chưa nhận được tin."), _T("Thông báo"), MB_ICONINFORMATION);
	}

	m_strMessage = _T("");
	UpdateData(FALSE);
	GetDlgItem(IDC_EDIT_MESSAGE)->SetFocus();
}

BOOL CMainDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
	if (pCopyDataStruct->dwData == 1)
	{
		CString payload((LPCTSTR)pCopyDataStruct->lpData);
		int p = payload.Find(_T('\t'));
		if (p > 0)
		{
			CString sender = payload.Left(p);
			CString text = payload.Mid(p + 1);
			AddMessage(sender, text, false);
		}
	}
	return TRUE;
}

void CMainDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl != IDC_LIST_CHAT)
	{
		CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
		return;
	}

	int index = lpDrawItemStruct->itemID;
	if (index < 0 || index >= (int)m_rows.size()) return;

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	int saved = dc.SaveDC();

	CRect rcItem(lpDrawItemStruct->rcItem);
	dc.FillSolidRect(rcItem, RGB(255, 255, 255));
	dc.IntersectClipRect(rcItem);

	ChatRow& row = m_rows[index];
	CFont* pFont = m_listChat.GetFont();
	if (pFont) dc.SelectObject(pFont);
	dc.SetBkMode(TRANSPARENT);

	if (row.isTime)
	{
		dc.SetTextColor(RGB(140, 140, 140));
		dc.DrawText(row.text, &rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.RestoreDC(saved);
		dc.Detach();
		return;
	}

	int rowH = rcItem.Height();

	CRect rcBubble;
	rcBubble.top = rcItem.top - row.lineIndex * rowH + 3;
	rcBubble.bottom = rcItem.top + (row.lineCount - row.lineIndex) * rowH - 3;
	if (row.isMe)
	{
		rcBubble.right = rcItem.right - TIME_GUTTER;
		rcBubble.left = rcBubble.right - BUBBLE_WIDTH;
	}
	else
	{
		rcBubble.left = rcItem.left + TIME_GUTTER;
		rcBubble.right = rcBubble.left + BUBBLE_WIDTH;
	}

	COLORREF bubbleColor = row.isMe ? RGB(0, 132, 255) : RGB(230, 230, 230);
	COLORREF textColor = row.isMe ? RGB(255, 255, 255) : RGB(0, 0, 0);

	CBrush brush(bubbleColor);
	CPen pen(PS_SOLID, 1, bubbleColor);
	dc.SelectObject(&brush);
	dc.SelectObject(&pen);
	dc.RoundRect(rcBubble, CPoint(16, 16));

	dc.SetTextColor(textColor);
	CRect rcText(rcBubble.left + BUBBLE_PAD, rcItem.top, rcBubble.right - BUBBLE_PAD, rcItem.bottom);
	dc.DrawText(row.text, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (row.lineIndex == row.lineCount - 1)
	{
		dc.SelectObject(&m_fontSmall);
		dc.SetTextColor(RGB(140, 140, 140));

		CRect rcTime = rcItem;
		if (row.isMe)
		{
			rcTime.left = rcItem.right - TIME_GUTTER;
			rcTime.right = rcItem.right - 4;
			dc.DrawText(row.timeStr, &rcTime, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		else
		{
			rcTime.left = rcItem.left + 4;
			rcTime.right = rcItem.left + TIME_GUTTER;
			dc.DrawText(row.timeStr, &rcTime, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
	}

	dc.RestoreDC(saved);
	dc.Detach();
}