// Database.h : luu user va tin nhan bang SQLite (file C:\ProgramData\CMCChat\chat.db)
//
// Bang users   : id, username, display_name, password_hash, salt, created_at, avatar, last_seen
// Bang messages: id, sender_id, receiver_id (-> users.id), content, created_at, is_read
//
// SQL dung prepared statement (chong SQL injection), mat khau luu bang PBKDF2 + salt.
// Moi ham co khoa mutex vi nhieu thread goi cung luc.

#pragma once
#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

struct UserRow
{
	long long id = 0;
	std::string username;
	std::string displayName;
};

struct UserSummary              // 1 dong trong danh sach nguoi dung
{
	UserRow user;
	bool hasAvatar = false;
	long long lastSeen = 0;     // lan cuoi online
	int unread = 0;             // so tin chua doc
	std::string lastFrom;       // ai gui tin cuoi
	std::string lastContent;
	long long lastTime = 0;
};

struct MessageRow
{
	long long id = 0;
	std::string from;           // username
	std::string to;
	std::string content;
	long long time = 0;
};

class Database
{
public:
	~Database();

	bool Open(const std::wstring& path);
	void Close();

	// Tra ve "" neu thanh cong, nguoc lai la ly do loi
	std::string Register(const std::string& username, const std::string& passwordHash, const std::string& displayName);
	bool CheckLogin(const std::string& username, const std::string& passwordHash, UserRow& out);
	bool FindUser(const std::string& username, UserRow& out);

	std::vector<UserSummary> ListUsers(long long meId);
	std::vector<UserRow> AllUsers();
	long long SaveMessage(long long fromId, long long toId, const std::string& content, long long time);
	std::vector<MessageRow> History(long long aId, long long bId, int limit);
	long long MarkRead(long long readerId, long long peerId);   // tra ve id tin moi nhat vua danh dau (0 = khong co gi moi)
	long long SeenId(long long senderId, long long receiverId); // id tin moi nhat nguoi nhan da xem
	void SetLastSeen(long long userId, long long time);
	long long LastSeen(long long userId);

	bool SetAvatar(long long userId, const std::string& image);
	bool GetAvatar(const std::string& username, std::string& image);
	bool HasAvatar(long long userId);

private:
	bool Exec(const char* sql);

	sqlite3* m_db = nullptr;
	std::mutex m_mtx;
};
