// Database.cpp : cai dat lop Database bang SQLite co san trong Windows (winsqlite3.dll)

#include <windows.h>
#include <bcrypt.h>
#include <winsqlite/winsqlite3.h>
#include "Database.h"
#pragma comment(lib, "winsqlite3.lib")
#pragma comment(lib, "bcrypt.lib")

static const ULONG PBKDF2_ITERATIONS = 100000;
static const int   SALT_BYTES = 16;
static const int   HASH_BYTES = 32;

// Bam mat khau de luu: PBKDF2-SHA256 + salt, 100000 vong (co y cham de kho do mat khau)
static bool StretchHash(const std::string& clientHash, const std::string& salt, std::string& out)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
		return false;

	out.assign(HASH_BYTES, '\0');
	NTSTATUS st = BCryptDeriveKeyPBKDF2(alg,
		(PUCHAR)clientHash.data(), (ULONG)clientHash.size(),
		(PUCHAR)salt.data(), (ULONG)salt.size(),
		PBKDF2_ITERATIONS, (PUCHAR)&out[0], HASH_BYTES, 0);
	BCryptCloseAlgorithmProvider(alg, 0);
	return BCRYPT_SUCCESS(st);
}

// So sanh 2 chuoi byte (khong dung giua chung)
static bool SameBytes(const std::string& a, const std::string& b)
{
	if (a.size() != b.size()) return false;
	unsigned char diff = 0;
	for (size_t i = 0; i < a.size(); i++)
		diff |= (unsigned char)(a[i] ^ b[i]);
	return diff == 0;
}

// ---- Kiem tra du lieu dang ky ----
static bool ValidUsername(const std::string& u)
{
	if (u.size() < 3 || u.size() > 20) return false;
	for (unsigned char c : u)
		if (!isalnum(c) && c != '_' && c != '.') return false;
	return true;
}

static size_t Utf8Length(const std::string& s)
{
	size_t n = 0;
	for (unsigned char c : s)
		if ((c & 0xC0) != 0x80) n++;
	return n;
}

static std::string Trim(const std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

// Prepared statement: SQL co dau ?, gia tri truyen qua Bind() -> chong SQL injection
class Stmt
{
public:
	Stmt(sqlite3* db, const char* sql) { if (db) sqlite3_prepare_v2(db, sql, -1, &m_st, NULL); }   // DB da dong -> Ok() = false
	~Stmt() { if (m_st) sqlite3_finalize(m_st); }
	bool Ok() const { return m_st != NULL; }

	void Bind(int i, const std::string& v) { sqlite3_bind_text(m_st, i, v.data(), (int)v.size(), SQLITE_TRANSIENT); }
	void BindBlob(int i, const std::string& v) { sqlite3_bind_blob(m_st, i, v.data(), (int)v.size(), SQLITE_TRANSIENT); }
	void Bind(int i, long long v) { sqlite3_bind_int64(m_st, i, v); }

	int Step() { return sqlite3_step(m_st); }

	std::string Text(int col)
	{
		const char* p = (const char*)sqlite3_column_text(m_st, col);
		return p ? std::string(p, sqlite3_column_bytes(m_st, col)) : std::string();
	}
	std::string Blob(int col)
	{
		const char* p = (const char*)sqlite3_column_blob(m_st, col);
		return p ? std::string(p, sqlite3_column_bytes(m_st, col)) : std::string();
	}
	long long Int(int col) { return sqlite3_column_int64(m_st, col); }

private:
	sqlite3_stmt* m_st = NULL;
};

// ---- Database ----
Database::~Database()
{
	Close();
}

bool Database::Exec(const char* sql)
{
	return sqlite3_exec(m_db, sql, NULL, NULL, NULL) == SQLITE_OK;
}

bool Database::Open(const std::wstring& path)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	if (sqlite3_open16(path.c_str(), &m_db) != SQLITE_OK)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
		return false;
	}

	sqlite3_busy_timeout(m_db, 3000);
	Exec("PRAGMA journal_mode=WAL;");
	Exec("PRAGMA foreign_keys=ON;");

	bool ok = Exec(
		"CREATE TABLE IF NOT EXISTS users ("
		"  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  username      TEXT    NOT NULL UNIQUE COLLATE NOCASE,"
		"  display_name  TEXT    NOT NULL,"
		"  password_hash BLOB    NOT NULL,"
		"  salt          BLOB    NOT NULL,"
		"  created_at    INTEGER NOT NULL);"
		"CREATE TABLE IF NOT EXISTS messages ("
		"  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  sender_id   INTEGER NOT NULL REFERENCES users(id),"
		"  receiver_id INTEGER NOT NULL REFERENCES users(id),"
		"  content     TEXT    NOT NULL,"
		"  created_at  INTEGER NOT NULL,"
		"  is_read     INTEGER NOT NULL DEFAULT 0);"
		"CREATE INDEX IF NOT EXISTS idx_messages_pair ON messages(sender_id, receiver_id, id);");

	// DB cu chua co cot avatar thi them vao (co roi thi lenh nay loi, bo qua)
	Exec("ALTER TABLE users ADD COLUMN avatar BLOB;");
	Exec("ALTER TABLE users ADD COLUMN last_seen INTEGER NOT NULL DEFAULT 0;");
	return ok;
}

void Database::Close()
{
	std::lock_guard<std::mutex> lk(m_mtx);
	if (m_db)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

std::string Database::Register(const std::string& username, const std::string& passwordHash, const std::string& displayIn)
{
	std::string display = Trim(displayIn);
	if (!ValidUsername(username))
		return "Tên đăng nhập phải dài 3-20 ký tự, chỉ gồm chữ không dấu, số, dấu _ hoặc dấu chấm.";
	if (display.empty() || Utf8Length(display) > 32)
		return "Tên hiển thị phải dài 1-32 ký tự.";
	if (passwordHash.size() != HASH_BYTES)
		return "Mật khẩu không hợp lệ.";

	std::string salt(SALT_BYTES, '\0');
	if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, (PUCHAR)&salt[0], SALT_BYTES, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
		return "Lỗi server (không tạo được salt).";
	std::string stored;
	if (!StretchHash(passwordHash, salt, stored))
		return "Lỗi server (không băm được mật khẩu).";

	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "INSERT INTO users(username, display_name, password_hash, salt, created_at) "
	              "VALUES(?, ?, ?, ?, strftime('%s','now'))");
	if (!st.Ok()) return "Lỗi database.";
	st.Bind(1, username);
	st.Bind(2, display);
	st.BindBlob(3, stored);
	st.BindBlob(4, salt);
	int r = st.Step();
	if (r == SQLITE_CONSTRAINT) return "Tên đăng nhập đã có người dùng.";
	if (r != SQLITE_DONE) return "Lỗi database.";
	return "";
}

bool Database::CheckLogin(const std::string& username, const std::string& passwordHash, UserRow& out)
{
	std::string salt, stored;
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		Stmt st(m_db, "SELECT id, username, display_name, password_hash, salt FROM users WHERE username = ?");
		if (!st.Ok()) return false;
		st.Bind(1, username);
		if (st.Step() != SQLITE_ROW) return false;
		out.id = st.Int(0);
		out.username = st.Text(1);
		out.displayName = st.Text(2);
		stored = st.Blob(3);
		salt = st.Blob(4);
	}

	// Bam ngoai khoa de khong chan client khac
	std::string hash;
	return passwordHash.size() == HASH_BYTES && StretchHash(passwordHash, salt, hash) && SameBytes(hash, stored);
}

bool Database::FindUser(const std::string& username, UserRow& out)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT id, username, display_name FROM users WHERE username = ?");
	if (!st.Ok()) return false;
	st.Bind(1, username);
	if (st.Step() != SQLITE_ROW) return false;
	out.id = st.Int(0);
	out.username = st.Text(1);
	out.displayName = st.Text(2);
	return true;
}

std::vector<UserSummary> Database::ListUsers(long long meId)
{
	std::vector<UserSummary> list;
	std::lock_guard<std::mutex> lk(m_mtx);
	// Cac user khac + so tin chua doc + tin cuoi giua 2 nguoi
	Stmt st(m_db,
		"SELECT u.id, u.username, u.display_name,"
		"  (SELECT COUNT(*) FROM messages m WHERE m.sender_id = u.id AND m.receiver_id = ?1 AND m.is_read = 0),"
		"  s.username, l.content, l.created_at, u.avatar IS NOT NULL, u.last_seen"
		" FROM users u"
		" LEFT JOIN messages l ON l.id = (SELECT MAX(id) FROM messages m"
		"     WHERE (m.sender_id = u.id AND m.receiver_id = ?1) OR (m.sender_id = ?1 AND m.receiver_id = u.id))"
		" LEFT JOIN users s ON s.id = l.sender_id"
		" WHERE u.id <> ?1 ORDER BY u.username");
	if (!st.Ok()) return list;
	st.Bind(1, meId);
	while (st.Step() == SQLITE_ROW)
	{
		UserSummary u;
		u.user.id = st.Int(0);
		u.user.username = st.Text(1);
		u.user.displayName = st.Text(2);
		u.unread = (int)st.Int(3);
		u.lastFrom = st.Text(4);
		u.lastContent = st.Text(5);
		u.lastTime = st.Int(6);
		u.hasAvatar = st.Int(7) != 0;
		u.lastSeen = st.Int(8);
		list.push_back(u);
	}
	return list;
}

std::vector<UserRow> Database::AllUsers()
{
	std::vector<UserRow> list;
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT id, username, display_name FROM users ORDER BY id");
	if (!st.Ok()) return list;
	while (st.Step() == SQLITE_ROW)
	{
		UserRow u;
		u.id = st.Int(0);
		u.username = st.Text(1);
		u.displayName = st.Text(2);
		list.push_back(u);
	}
	return list;
}

long long Database::SaveMessage(long long fromId, long long toId, const std::string& content, long long time)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "INSERT INTO messages(sender_id, receiver_id, content, created_at) VALUES(?, ?, ?, ?)");
	if (!st.Ok()) return 0;
	st.Bind(1, fromId);
	st.Bind(2, toId);
	st.Bind(3, content);
	st.Bind(4, time);
	if (st.Step() != SQLITE_DONE) return 0;
	return sqlite3_last_insert_rowid(m_db);
}

std::vector<MessageRow> Database::History(long long aId, long long bId, int limit)
{
	std::vector<MessageRow> rows;
	std::lock_guard<std::mutex> lk(m_mtx);
	// 'limit' tin moi nhat giua 2 nguoi, xep theo thoi gian
	Stmt st(m_db,
		"SELECT m.id, s.username, r.username, m.content, m.created_at FROM ("
		"    SELECT * FROM messages"
		"    WHERE (sender_id = ?1 AND receiver_id = ?2) OR (sender_id = ?2 AND receiver_id = ?1)"
		"    ORDER BY id DESC LIMIT ?3) m"
		" JOIN users s ON s.id = m.sender_id"
		" JOIN users r ON r.id = m.receiver_id"
		" ORDER BY m.id ASC");
	if (!st.Ok()) return rows;
	st.Bind(1, aId);
	st.Bind(2, bId);
	st.Bind(3, (long long)limit);
	while (st.Step() == SQLITE_ROW)
	{
		MessageRow m;
		m.id = st.Int(0);
		m.from = st.Text(1);
		m.to = st.Text(2);
		m.content = st.Text(3);
		m.time = st.Int(4);
		rows.push_back(m);
	}
	return rows;
}

bool Database::SetAvatar(long long userId, const std::string& image)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "UPDATE users SET avatar = ? WHERE id = ?");
	if (!st.Ok()) return false;
	st.BindBlob(1, image);
	st.Bind(2, userId);
	return st.Step() == SQLITE_DONE;
}

bool Database::GetAvatar(const std::string& username, std::string& image)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT avatar FROM users WHERE username = ? AND avatar IS NOT NULL");
	if (!st.Ok()) return false;
	st.Bind(1, username);
	if (st.Step() != SQLITE_ROW) return false;
	image = st.Blob(0);
	return true;
}

bool Database::HasAvatar(long long userId)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT 1 FROM users WHERE id = ? AND avatar IS NOT NULL");
	if (!st.Ok()) return false;
	st.Bind(1, userId);
	return st.Step() == SQLITE_ROW;
}

long long Database::MarkRead(long long readerId, long long peerId)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "UPDATE messages SET is_read = 1 WHERE sender_id = ? AND receiver_id = ? AND is_read = 0");
	if (!st.Ok()) return 0;
	st.Bind(1, peerId);
	st.Bind(2, readerId);
	if (st.Step() != SQLITE_DONE || sqlite3_changes(m_db) == 0) return 0;

	Stmt q(m_db, "SELECT MAX(id) FROM messages WHERE sender_id = ? AND receiver_id = ?");
	if (!q.Ok()) return 0;
	q.Bind(1, peerId);
	q.Bind(2, readerId);
	return q.Step() == SQLITE_ROW ? q.Int(0) : 0;
}

long long Database::SeenId(long long senderId, long long receiverId)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT MAX(id) FROM messages WHERE sender_id = ? AND receiver_id = ? AND is_read = 1");
	if (!st.Ok()) return 0;
	st.Bind(1, senderId);
	st.Bind(2, receiverId);
	return st.Step() == SQLITE_ROW ? st.Int(0) : 0;
}

void Database::SetLastSeen(long long userId, long long time)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "UPDATE users SET last_seen = ? WHERE id = ?");
	if (!st.Ok()) return;
	st.Bind(1, time);
	st.Bind(2, userId);
	st.Step();
}

long long Database::LastSeen(long long userId)
{
	std::lock_guard<std::mutex> lk(m_mtx);
	Stmt st(m_db, "SELECT last_seen FROM users WHERE id = ?");
	if (!st.Ok()) return 0;
	st.Bind(1, userId);
	return st.Step() == SQLITE_ROW ? st.Int(0) : 0;
}
