// ContactList.h : danh sach nguoi dung tu ve
#pragma once
#include <deque>
#include "ChatTypes.h"

class CContactList : public CListBox
{
public:
	// itemData cua moi dong = chi so trong 'contacts'
	void Init(std::deque<Contact>* contacts, CFont* name, CFont* nameBold, CFont* smallFont, CFont* smallBoldFont, CFont* avatar);
	int ContactAt(int row) const { return row < 0 ? -1 : (int)GetItemData(row); }

	virtual void DrawItem(LPDRAWITEMSTRUCT lpDis);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT) {}

private:
	std::deque<Contact>* m_contacts = nullptr;
	CFont* m_fontName = nullptr;
	CFont* m_fontNameBold = nullptr;
	CFont* m_fontSmall = nullptr;
	CFont* m_fontSmallBold = nullptr;
	CFont* m_fontAvatar = nullptr;
};
