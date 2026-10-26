// ChatServer.cpp : server chat, chay duoi dang Windows service
//
// F5 trong Visual Studio: chay console, go "users" xem tai khoan, "q" de dung.
// Service (cmd Admin): ChatServer.exe install | uninstall, sc start/stop CMCChatServer
// Du lieu va log: C:\ProgramData\CMCChat\

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <stdio.h>
#include <time.h>
#include <set>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "Database.h"
#include "../Common/Protocol.h"
#pragma comment(lib, "ws2_32.lib")

#define SERVICE_NAME   L"CMCChatServer"
#define DISPLAY_NAME   L"CMC Chat Server"
#define DESCRIPTION    L"Server chat CMC: dang ky, dang nhap, chuyen va luu tin nhan (TCP cong 8888)."
#define STOP_TIMEOUT   5000     // ms cho client thread thoat khi dung
#define HISTORY_LIMIT  300      // so tin toi da moi lan lay lich su

using json::Value;

static Database g_db;

// ---- Log ----
static std::mutex g_logMutex;
static bool g_consoleMode = false;

// C:\ProgramData\CMCChat\ - service (SYSTEM) va F5 (user thuong) deu doc/ghi duoc
static std::wstring DataDir()
{
	static const std::wstring dir = [] {
		std::wstring d = L"C:\\ProgramData\\CMCChat\\";
		PWSTR base = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &base)))
			d = std::wstring(base) + L"\\CMCChat\\";
		CoTaskMemFree(base);

		SECURITY_ATTRIBUTES sa = { sizeof(sa) };
		if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
				L"D:(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;0x1301bf;;;BU)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
		{
			CreateDirectoryW(d.c_str(), &sa);     // da ton tai thi bo qua
			LocalFree(sa.lpSecurityDescriptor);
		}
		else
			CreateDirectoryW(d.c_str(), NULL);
		return d;
	}();
	return dir;
}

static void Log(const char* fmt, ...)
{
	char msg[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char line[1100];
	sprintf_s(line, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, msg);

	std::lock_guard<std::mutex> lk(g_logMutex);
	if (g_consoleMode)
		fputs(line, stdout);

	FILE* f = nullptr;
	if (_wfopen_s(&f, (DataDir() + L"ChatServer.log").c_str(), L"a") == 0 && f)
	{
		fputs(line, f);
		fclose(f);
	}
}

// ---- Session: 1 ket noi TCP cua 1 client ----
struct Session
{
	SOCKET sock = INVALID_SOCKET;
	std::string addr;            // ip:port
	UserRow user;                // id = 0: chua dang nhap
	std::mutex sendMtx;          // khoa khi gui
};
typedef std::shared_ptr<Session> SessionPtr;

static SOCKET g_listen = INVALID_SOCKET;
static std::thread g_acceptThread;
static std::atomic<bool> g_stopping(false);

static std::mutex g_sessMtx;
static std::condition_variable g_sessCv;
static std::set<SessionPtr> g_sessions;

// Gui 1 goi tin toi 1 client
static bool SendTo(const SessionPtr& s, uint32_t action, const Value& body)
{
	std::string data = proto::Build(action, body);
	std::lock_guard<std::mutex> lk(s->sendMtx);
	if (s->sock == INVALID_SOCKET) return false;     // session da dong
	return proto::SendAll(s->sock, data);
}

// Cac ket noi cua 1 user (userId = 0: tat ca user da dang nhap)
static std::vector<SessionPtr> SessionsOf(long long userId)
{
	std::vector<SessionPtr> out;
	std::lock_guard<std::mutex> lk(g_sessMtx);
	for (const SessionPtr& s : g_sessions)
		if (s->user.id != 0 && (userId == 0 || s->user.id == userId))
			out.push_back(s);
	return out;
}

static bool IsOnline(long long userId)
{
	return !SessionsOf(userId).empty();
}

// Bao cho moi nguoi biet u online/offline
static void BroadcastStatus(const UserRow& u, bool online)
{
	Value body = Value::Object()
		.Set("username", u.username)
		.Set("display_name", u.displayName)
		.Set("online", online)
		.Set("last_seen", g_db.LastSeen(u.id));
	for (const SessionPtr& s : SessionsOf(0))
		if (s->user.id != u.id)
			SendTo(s, proto::USER_STATUS, body);
}

// ---- Xu ly tung action ----
static void OnRegister(const SessionPtr& s, const Value& req)
{
	std::string hash;
	if (!proto::Base64Decode(req["password"].Str(), hash))
		hash.clear();

	std::string username = req["username"].Str();
	std::string err = g_db.Register(username, hash, proto::Clean(req["display_name"].Str()));
	if (!err.empty())
	{
		SendTo(s, proto::REGISTER, proto::Failed(err));
		return;
	}

	UserRow u;
	g_db.FindUser(username, u);
	Log("Dang ky moi: %s (id %lld) tu %s", u.username.c_str(), u.id, s->addr.c_str());
	SendTo(s, proto::REGISTER, proto::Ok());
	BroadcastStatus(u, false);           // moi nguoi thay user moi
}

static void OnLogin(const SessionPtr& s, const Value& req)
{
	{
		std::lock_guard<std::mutex> lk(g_sessMtx);
		if (s->user.id != 0)
		{
			SendTo(s, proto::LOGIN, proto::Failed("Kết nối này đã đăng nhập rồi."));
			return;
		}
	}

	std::string hash;
	UserRow u;
	if (!proto::Base64Decode(req["password"].Str(), hash) || !g_db.CheckLogin(req["username"].Str(), hash, u))
	{
		Log("Dang nhap sai: '%s' tu %s", req["username"].Str().c_str(), s->addr.c_str());
		SendTo(s, proto::LOGIN, proto::Failed("Sai tên đăng nhập hoặc mật khẩu."));
		return;
	}

	bool wasOnline = IsOnline(u.id);
	{
		std::lock_guard<std::mutex> lk(g_sessMtx);
		s->user = u;
	}
	Log("%s dang nhap tu %s", u.username.c_str(), s->addr.c_str());
	SendTo(s, proto::LOGIN, proto::Ok().Set("username", u.username).Set("display_name", u.displayName)
		.Set("has_avatar", g_db.HasAvatar(u.id)));
	if (!wasOnline)
		BroadcastStatus(u, true);
}

static void OnGetUsers(const SessionPtr& s, const UserRow& me)
{
	Value users = Value::List();
	for (const UserSummary& u : g_db.ListUsers(me.id))
	{
		users.Push(Value::Object()
			.Set("username", u.user.username)
			.Set("display_name", u.user.displayName)
			.Set("online", IsOnline(u.user.id))
			.Set("has_avatar", u.hasAvatar)
			.Set("last_seen", u.lastSeen)
			.Set("unread", u.unread)
			.Set("last_from", u.lastFrom)
			.Set("last_content", u.lastContent)
			.Set("last_time", u.lastTime));
	}
	SendTo(s, proto::GET_USERS, proto::Ok().Set("users", users));
}

static void OnGetMessages(const SessionPtr& s, const UserRow& me, const Value& req)
{
	UserRow peer;
	if (!g_db.FindUser(req["with"].Str(), peer))
	{
		SendTo(s, proto::GET_MESSAGES, proto::Failed("Người dùng không tồn tại."));
		return;
	}

	Value list = Value::List();
	for (const MessageRow& m : g_db.History(me.id, peer.id, HISTORY_LIMIT))
	{
		list.Push(Value::Object()
			.Set("id", m.id)
			.Set("from", m.from)
			.Set("to", m.to)
			.Set("content", m.content)
			.Set("time", m.time));
	}
	SendTo(s, proto::GET_MESSAGES, proto::Ok().Set("with", peer.username).Set("messages", list)
		.Set("seen_id", g_db.SeenId(me.id, peer.id)));
}

static void OnChat(const SessionPtr& s, const UserRow& me, const Value& req)
{
	UserRow to;
	std::string content = proto::Clean(req["content"].Str());
	if (!g_db.FindUser(req["to"].Str(), to))
	{
		SendTo(s, proto::CHAT, proto::Failed("Người nhận không tồn tại."));
		return;
	}
	if (content.find_first_not_of(" \t\n") == std::string::npos) return;   // tin rong
	if ((int)content.size() > proto::MAX_CONTENT)
	{
		SendTo(s, proto::CHAT, proto::Failed("Tin nhắn quá dài."));
		return;
	}

	long long now = (long long)time(NULL);      // UTC timestamp
	long long id = g_db.SaveMessage(me.id, to.id, content, now);
	if (id == 0)
	{
		SendTo(s, proto::CHAT, proto::Failed("Server không lưu được tin nhắn."));
		return;
	}

	// Gui tin cho ca nguoi nhan va nguoi gui
	Value msg = proto::Ok()
		.Set("id", id)
		.Set("from", me.username)
		.Set("to", to.username)
		.Set("content", content)
		.Set("time", now);
	std::set<SessionPtr> targets;
	for (const SessionPtr& t : SessionsOf(to.id)) targets.insert(t);
	for (const SessionPtr& t : SessionsOf(me.id)) targets.insert(t);
	for (const SessionPtr& t : targets) SendTo(t, proto::CHAT, msg);
}

// Danh dau da doc, roi bao cho nguoi gui biet "Da xem"
static void OnMarkRead(const UserRow& me, const Value& req)
{
	UserRow peer;
	if (!g_db.FindUser(req["with"].Str(), peer)) return;
	long long lastId = g_db.MarkRead(me.id, peer.id);
	if (lastId == 0) return;
	Value seen = Value::Object().Set("by", me.username).Set("last_id", lastId);
	for (const SessionPtr& t : SessionsOf(peer.id))
		SendTo(t, proto::SEEN, seen);
}

// Chuyen tin hieu "dang nhap..." cho nguoi nhan
static void OnTyping(const UserRow& me, const Value& req)
{
	UserRow to;
	if (!g_db.FindUser(req["to"].Str(), to)) return;
	Value typing = Value::Object().Set("from", me.username);
	for (const SessionPtr& t : SessionsOf(to.id))
		SendTo(t, proto::TYPING, typing);
}

static void OnGetAvatar(const SessionPtr& s, const Value& req)
{
	std::string username = req["username"].Str(), image;
	if (!g_db.GetAvatar(username, image))
	{
		SendTo(s, proto::GET_AVATAR, proto::Failed("").Set("username", username));
		return;
	}
	SendTo(s, proto::GET_AVATAR, proto::Ok().Set("username", username).Set("data", proto::Base64Encode(image)));
}

// Chi nhan anh PNG / JPG, toi da MAX_AVATAR
static void OnSetAvatar(const SessionPtr& s, const UserRow& me, const Value& req)
{
	std::string image;
	bool png = false, jpg = false;
	if (proto::Base64Decode(req["data"].Str(), image) && image.size() >= 4)
	{
		png = (unsigned char)image[0] == 0x89 && image.compare(1, 3, "PNG") == 0;
		jpg = (unsigned char)image[0] == 0xFF && (unsigned char)image[1] == 0xD8;
	}
	if ((!png && !jpg) || image.size() > proto::MAX_AVATAR)
	{
		SendTo(s, proto::SET_AVATAR, proto::Failed("Ảnh không hợp lệ (chỉ nhận PNG/JPG, tối đa 512 KB)."));
		return;
	}
	if (!g_db.SetAvatar(me.id, image))
	{
		SendTo(s, proto::SET_AVATAR, proto::Failed("Server không lưu được ảnh."));
		return;
	}

	Log("%s doi avatar (%d byte)", me.username.c_str(), (int)image.size());
	SendTo(s, proto::SET_AVATAR, proto::Ok());
	Value msg = proto::Ok().Set("username", me.username).Set("data", req["data"].Str());
	for (const SessionPtr& t : SessionsOf(0))     // moi nguoi cap nhat anh moi
		SendTo(t, proto::GET_AVATAR, msg);
}

static bool HandlePacket(const SessionPtr& s, const proto::Packet& p)
{
	if (p.body.GetType() != Value::Obj)
	{
		SendTo(s, proto::ERROR_MSG, Value::Object().Set("reason", "Gói tin không phải JSON hợp lệ."));
		return true;
	}

	// Chua can dang nhap
	if (p.action == proto::REGISTER) { OnRegister(s, p.body); return true; }
	if (p.action == proto::LOGIN)    { OnLogin(s, p.body); return true; }
	if (p.action == proto::LOGOUT)   return false;

	UserRow me;
	{
		std::lock_guard<std::mutex> lk(g_sessMtx);
		me = s->user;
	}
	if (me.id == 0)
	{
		SendTo(s, proto::ERROR_MSG, Value::Object().Set("reason", "Bạn chưa đăng nhập."));
		return true;
	}

	switch (p.action)
	{
	case proto::GET_USERS:    OnGetUsers(s, me); break;
	case proto::GET_MESSAGES: OnGetMessages(s, me, p.body); break;
	case proto::CHAT:         OnChat(s, me, p.body); break;
	case proto::MARK_READ:    OnMarkRead(me, p.body); break;
	case proto::GET_AVATAR:   OnGetAvatar(s, p.body); break;
	case proto::SET_AVATAR:   OnSetAvatar(s, me, p.body); break;
	case proto::TYPING:       OnTyping(me, p.body); break;
	default:
		SendTo(s, proto::ERROR_MSG, Value::Object().Set("reason", "Action không hợp lệ: " + std::to_string(p.action)));
	}
	return true;
}

// ---- Moi client 1 thread: doc goi tin -> xu ly ----
static void ClientThread(SessionPtr s)
{
	DWORD sendTimeout = 3000;    // send() khong bi treo qua 3 giay
	setsockopt(s->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sendTimeout, sizeof(sendTimeout));
	BOOL keepAlive = TRUE;       // phat hien client mat mang
	setsockopt(s->sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));

	proto::Packet p;
	while (proto::RecvPacket(s->sock, p, g_stopping))
	{
		if (!HandlePacket(s, p)) break;
	}

	// Ket noi cuoi cung cua user -> luu lan cuoi online (lam truoc khi xoa, luc nay DB chac chan con mo)
	UserRow u;
	bool lastSession = true;
	{
		std::lock_guard<std::mutex> lk(g_sessMtx);
		u = s->user;
		for (const SessionPtr& o : g_sessions)
			if (o != s && o->user.id == u.id) lastSession = false;
	}
	if (u.id != 0 && lastSession)
		g_db.SetLastSeen(u.id, (long long)time(NULL));

	// Xoa khoi danh sach roi dong socket
	{
		std::lock_guard<std::mutex> lk(g_sessMtx);
		g_sessions.erase(s);
	}
	{
		std::lock_guard<std::mutex> lk(s->sendMtx);
		closesocket(s->sock);
		s->sock = INVALID_SOCKET;
	}

	if (u.id != 0)
	{
		Log("%s ngat ket noi (%s)", u.username.c_str(), s->addr.c_str());
		if (!g_stopping && !IsOnline(u.id))
			BroadcastStatus(u, false);
	}
	g_sessCv.notify_all();
}

// Thread nhan client moi
static void AcceptLoop()
{
	for (;;)
	{
		sockaddr_in addr = {};
		int len = sizeof(addr);
		SOCKET c = accept(g_listen, (sockaddr*)&addr, &len);
		if (c == INVALID_SOCKET)
		{
			if (g_stopping) break;
			Sleep(100);
			continue;
		}

		char ip[INET_ADDRSTRLEN] = "?";
		inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

		SessionPtr s = std::make_shared<Session>();
		s->sock = c;
		s->addr = std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
		{
			std::lock_guard<std::mutex> lk(g_sessMtx);
			if (g_stopping)
			{
				closesocket(c);
				break;
			}
			g_sessions.insert(s);
		}
		std::thread(ClientThread, s).detach();
	}
}

static bool StartServer()
{
	std::wstring dbPath = DataDir() + L"chat.db";
	if (!g_db.Open(dbPath))
	{
		Log("Khong mo duoc database chat.db");
		return false;
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Log("WSAStartup loi");
		return false;
	}

	g_stopping = false;
	g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int yes = 1;
	setsockopt(g_listen, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&yes, sizeof(yes));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(proto::DEFAULT_PORT);
	if (g_listen == INVALID_SOCKET ||
		bind(g_listen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
		listen(g_listen, SOMAXCONN) == SOCKET_ERROR)
	{
		Log("Khong mo duoc cong %d (loi %d)", proto::DEFAULT_PORT, WSAGetLastError());
		if (g_listen != INVALID_SOCKET) closesocket(g_listen);
		g_listen = INVALID_SOCKET;
		WSACleanup();
		g_db.Close();
		return false;
	}

	g_acceptThread = std::thread(AcceptLoop);
	char dbPathA[MAX_PATH * 3] = {};
	WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, dbPathA, sizeof(dbPathA), NULL, NULL);
	Log("Server dang lang nghe TCP cong %d", proto::DEFAULT_PORT);
	Log("Database: %s (%d tai khoan)", dbPathA, (int)g_db.AllUsers().size());
	return true;
}

static void StopServer()
{
	g_stopping = true;

	// Dong socket lang nghe -> AcceptLoop thoat
	closesocket(g_listen);
	g_listen = INVALID_SOCKET;
	if (g_acceptThread.joinable()) g_acceptThread.join();

	// Cho cac client thread tu thoat (toi da STOP_TIMEOUT)
	std::unique_lock<std::mutex> lk(g_sessMtx);
	bool allDone = g_sessCv.wait_for(lk, std::chrono::milliseconds(STOP_TIMEOUT),
		[] { return g_sessions.empty(); });
	lk.unlock();

	if (allDone)
	{
		WSACleanup();
		g_db.Close();
	}
	Log("Da dung server%s", allDone ? "" : " (con client thread chua thoat kip)");
}

// ---- Windows service ----
static SERVICE_STATUS g_status = {};
static SERVICE_STATUS_HANDLE g_statusHandle = NULL;
static HANDLE g_stopEvent = NULL;

static void ReportStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
	static DWORD checkPoint = 1;

	g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_status.dwCurrentState = state;
	g_status.dwWin32ExitCode = exitCode;
	g_status.dwWaitHint = waitHint;

	if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING)
		g_status.dwControlsAccepted = 0;
	else
		g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
		g_status.dwCheckPoint = 0;
	else
		g_status.dwCheckPoint = checkPoint++;

	if (g_statusHandle)
		SetServiceStatus(g_statusHandle, &g_status);
}

// Windows goi khi stop service: bao "dang dung" va bat event dung
static DWORD WINAPI CtrlHandler(DWORD ctrl, DWORD, LPVOID, LPVOID)
{
	switch (ctrl)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		Log("Nhan lenh dung service");
		ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, STOP_TIMEOUT + 2000);
		SetEvent(g_stopEvent);
		return NO_ERROR;
	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;
	}
	return ERROR_CALL_NOT_IMPLEMENTED;
}

// Dung chung cho ca che do service va console
static DWORD RunServer()
{
	if (!StartServer())
		return ERROR_NETWORK_UNREACHABLE;

	ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
	WaitForSingleObject(g_stopEvent, INFINITE);

	StopServer();
	return NO_ERROR;
}

static void WINAPI ServiceMain(DWORD, LPWSTR*)
{
	g_statusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, CtrlHandler, NULL);
	if (!g_statusHandle) return;

	Log("Service khoi dong");
	ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	DWORD err = RunServer();
	ReportStatus(SERVICE_STOPPED, err, 0);
}

// ---- Che do console (F5 trong Visual Studio) ----
static BOOL WINAPI ConsoleHandler(DWORD)
{
	SetEvent(g_stopEvent);
	return TRUE;
}

static int RunConsole()
{
	g_consoleMode = true;
	SetConsoleOutputCP(CP_UTF8);                 // in tieng Viet
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	printf("CMC Chat Server - che do console.\n  users + Enter: xem tai khoan trong database\n  q + Enter    : dung server\n");

	std::thread([] {
		char line[64];
		while (fgets(line, sizeof(line), stdin))
		{
			const char* p = line;
			if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;   // bo BOM
			while (*p == ' ' || *p == '\t') p++;

			if (_strnicmp(p, "users", 5) == 0)
			{
				std::vector<UserRow> users = g_db.AllUsers();
				printf("--- %d tai khoan trong database ---\n", (int)users.size());
				for (const UserRow& u : users)
					printf("  #%-4lld %-20s %s%s\n", u.id, u.username.c_str(), u.displayName.c_str(),
						IsOnline(u.id) ? "  (online)" : "");
			}
			else if (*p == 'q' || *p == 'Q')
			{
				Log("Nhan lenh dung tu ban phim");
				SetEvent(g_stopEvent);
				return;
			}
		}
	}).detach();

	DWORD err = RunServer();
	if (err != NO_ERROR)
		printf("Khong chay duoc server (cong %d dang bi chiem? service CMCChatServer co dang chay khong?)\n", proto::DEFAULT_PORT);
	return (int)err;
}

// ---- Cai dat / go bo service ----
static int InstallService()
{
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring cmd = std::wstring(L"\"") + path + L"\"";

	SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!scm)
	{
		printf("OpenSCManager loi %lu (hay chay cmd bang quyen Administrator)\n", GetLastError());
		return 1;
	}

	SC_HANDLE svc = CreateServiceW(scm, SERVICE_NAME, DISPLAY_NAME,
		SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL, cmd.c_str(), NULL, NULL, NULL, NULL, NULL);
	if (!svc)
	{
		DWORD e = GetLastError();
		printf(e == ERROR_SERVICE_EXISTS ? "Service da ton tai\n" : "CreateService loi %lu\n", e);
		CloseServiceHandle(scm);
		return 1;
	}

	SERVICE_DESCRIPTIONW desc = { (LPWSTR)DESCRIPTION };
	ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

	printf("Da cai service. Chay bang: sc start CMCChatServer\n");
	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
	return 0;
}

static int UninstallService()
{
	SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
	if (!scm)
	{
		printf("OpenSCManager loi %lu (hay chay cmd bang quyen Administrator)\n", GetLastError());
		return 1;
	}

	SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
	if (!svc)
	{
		printf("OpenService loi %lu (service chua duoc cai?)\n", GetLastError());
		CloseServiceHandle(scm);
		return 1;
	}

	SERVICE_STATUS st = {};
	if (ControlService(svc, SERVICE_CONTROL_STOP, &st))
	{
		printf("Dang dung service...\n");
		for (int i = 0; i < 20 && QueryServiceStatus(svc, &st) && st.dwCurrentState != SERVICE_STOPPED; i++)
			Sleep(500);
	}

	if (DeleteService(svc))
		printf("Da xoa service\n");
	else
		printf("DeleteService loi %lu\n", GetLastError());

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
	return 0;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc > 1)
	{
		if (_wcsicmp(argv[1], L"install") == 0)   return InstallService();
		if (_wcsicmp(argv[1], L"uninstall") == 0) return UninstallService();
	}

	g_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_stopEvent) return 1;

	if (argc > 1)
	{
		if (_wcsicmp(argv[1], L"console") == 0) return RunConsole();
		printf("Tham so: install | uninstall | console\n");
		return 1;
	}

	// Khong co tham so: thu ket noi voi SCM de chay nhu service
	SERVICE_TABLE_ENTRYW table[] = {
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ NULL, NULL }
	};
	if (!StartServiceCtrlDispatcherW(table))
	{
		// Khong phai SCM goi (vd bam F5 trong Visual Studio) -> chay o che do console
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			return RunConsole();
		return 1;
	}
	return 0;
}
