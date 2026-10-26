// ChatTypes.h : du lieu tin nhan va nguoi dung phia client
#pragma once
#include <vector>

struct ChatMsg
{
	long long id = 0;            // id do server cap (de khong hien trung tin)
	bool isMe = false;
	CString text;
	long long time = 0;          // giay tu 1970 (UTC)
};

struct Contact
{
	CString user;
	CString display;
	bool online = false;
	int unread = 0;
	CString lastText;            // tin cuoi (hien xem truoc trong danh sach)
	bool lastFromMe = false;
	long long lastTime = 0;
	bool historyRequested = false;
	bool historyLoaded = false;
	std::vector<ChatMsg> msgs;

	long long lastSeen = 0;        // lan cuoi online (UTC), 0 = chua biet
	long long seenId = 0;          // tin cua minh co id <= seenId thi nguoi kia da xem
	ULONGLONG typingUntil = 0;     // GetTickCount64() toi luc het "dang nhap..."

	bool IsTyping() const { return typingUntil > GetTickCount64(); }
};
