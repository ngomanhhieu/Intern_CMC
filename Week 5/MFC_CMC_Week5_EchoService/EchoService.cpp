// EchoService.cpp : Windows service mo TCP cong 9999, gui tra lai (echo) moi du lieu client gui len.
//
// Chay trong Visual Studio: bam F5 -> tu chay o che do console (khong can cai service),
// go q + Enter de dung. Cung mot code server voi khi chay that duoi dang service.
//
// Chay that duoi dang service (cmd chay quyen Administrator):
//   EchoService.exe install     -> dang ky service voi SCM
//   sc start CMCEchoService     -> chay service
//   sc stop  CMCEchoService     -> dung service (dung ngay, khong treo)
//   EchoService.exe uninstall   -> dung (neu dang chay) va xoa service
//
// Log ghi ra file EchoService.log canh file exe (service khong co cua so console).

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <set>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#pragma comment(lib, "ws2_32.lib")

#define SERVICE_NAME   L"CMCEchoService"
#define DISPLAY_NAME   L"CMC Echo Service"
#define DESCRIPTION    L"Mo TCP cong 9999, gui tra lai moi tin nhan nhan duoc cho client (bai tap tuan 5)."
#define ECHO_PORT      9999
#define STOP_TIMEOUT   5000     // ms, thoi gian toi da cho cac client thread thoat

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------
static std::mutex g_logMutex;
static bool g_consoleMode = false;

static void Log(const char* fmt, ...)
{
	char msg[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char line[600];
	sprintf_s(line, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, msg);

	std::lock_guard<std::mutex> lk(g_logMutex);
	if (g_consoleMode)
		fputs(line, stdout);

	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	wchar_t* slash = wcsrchr(path, L'\\');
	if (slash) wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"EchoService.log");

	FILE* f = nullptr;
	if (_wfopen_s(&f, path, L"a") == 0 && f)
	{
		fputs(line, f);
		fclose(f);
	}
}

// ---------------------------------------------------------------------------
// TCP echo server
// ---------------------------------------------------------------------------
static SOCKET g_listen = INVALID_SOCKET;
static std::thread g_acceptThread;
static std::atomic<bool> g_stopping(false);

// Danh sach socket client dang mo, StopServer() cho toi khi danh sach nay rong.
static std::mutex g_clientMutex;
static std::condition_variable g_clientCv;
static std::set<SOCKET> g_clients;

// Cho toi khi socket co du lieu de doc. Cu 200ms kiem tra co stop mot lan,
// vi tren Windows shutdown() tu thread khac KHONG lam recv() dang block tra ve.
static bool WaitReadable(SOCKET s)
{
	while (!g_stopping)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		timeval tv = { 0, 200000 };
		int r = select(0, &fds, NULL, NULL, &tv);
		if (r == SOCKET_ERROR) return false;
		if (r > 0) return true;
	}
	return false;
}

// Moi client 1 thread: nhan bao nhieu byte thi gui lai dung bay nhieu byte.
// Khong can biet dinh dang tin nhan nen tin dai bao nhieu, tieng Viet hay khong deu echo dung.
static void ClientThread(SOCKET s, std::string peer)
{
	// Client khong chiu doc ma ta cu gui -> send() co the block mai; gioi han 2 giay
	DWORD sendTimeout = 2000;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sendTimeout, sizeof(sendTimeout));

	char buf[4096];
	long long total = 0;
	bool ok = true;

	while (ok)
	{
		if (!WaitReadable(s)) break;             // dang stop hoac loi
		int n = recv(s, buf, sizeof(buf), 0);
		if (n <= 0) break;                       // 0: client dong ket noi, <0: loi

		int sent = 0;
		while (sent < n)
		{
			int k = send(s, buf + sent, n - sent, 0);
			if (k <= 0) { ok = false; break; }
			sent += k;
		}
		total += n;
	}

	Log("Client %s ngat ket noi (da echo %lld byte)", peer.c_str(), total);

	{
		std::lock_guard<std::mutex> lk(g_clientMutex);
		g_clients.erase(s);
		closesocket(s);
	}
	g_clientCv.notify_all();
}

// Thread lang nghe. accept() block cho toi khi co client moi,
// hoac khi StopServer() dong g_listen thi accept() tra ve loi va vong lap thoat.
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
			Log("accept loi %d", WSAGetLastError());
			Sleep(100);                          // tranh vong lap ban CPU neu loi lien tuc
			continue;
		}

		char ip[INET_ADDRSTRLEN] = "?";
		inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
		std::string peer = std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));

		{
			std::lock_guard<std::mutex> lk(g_clientMutex);
			if (g_stopping)                      // vua accept dung luc dang stop
			{
				closesocket(c);
				break;
			}
			g_clients.insert(c);
		}
		Log("Client %s ket noi", peer.c_str());
		std::thread(ClientThread, c, peer).detach();
	}
}

static bool StartServer()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Log("WSAStartup loi");
		return false;
	}

	g_stopping = false;
	g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_listen == INVALID_SOCKET)
	{
		Log("socket() loi %d", WSAGetLastError());
		WSACleanup();
		return false;
	}

	// Khong cho process khac chiem cung cong
	int yes = 1;
	setsockopt(g_listen, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&yes, sizeof(yes));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(ECHO_PORT);
	if (bind(g_listen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
		listen(g_listen, SOMAXCONN) == SOCKET_ERROR)
	{
		Log("Khong mo duoc cong %d (loi %d)", ECHO_PORT, WSAGetLastError());
		closesocket(g_listen);
		g_listen = INVALID_SOCKET;
		WSACleanup();
		return false;
	}

	g_acceptThread = std::thread(AcceptLoop);
	Log("Dang lang nghe TCP cong %d", ECHO_PORT);
	return true;
}

static void StopServer()
{
	g_stopping = true;

	// 1. Dong socket lang nghe -> accept() tra ve -> AcceptLoop thoat
	closesocket(g_listen);
	g_listen = INVALID_SOCKET;
	if (g_acceptThread.joinable()) g_acceptThread.join();

	// 2. Client thread thay g_stopping trong vong <= 200ms thi tu dong socket va thoat.
	// 3. Cho cac client thread don dep xong (co gioi han thoi gian, khong bao gio treo)
	std::unique_lock<std::mutex> lk(g_clientMutex);
	bool allDone = g_clientCv.wait_for(lk, std::chrono::milliseconds(STOP_TIMEOUT),
		[] { return g_clients.empty(); });
	lk.unlock();

	if (allDone)
		WSACleanup();
	Log("Da dung server%s", allDone ? "" : " (con client thread chua thoat kip)");
}


// Windows service

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

	// Dang khoi dong / dang dung thi khong nhan them lenh
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

// SCM goi ham nay (tren thread rieng) khi co lenh stop/shutdown.
// Chi bao STOP_PENDING va bat event, viec don dep lam o ServiceMain -> handler tra ve ngay.
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

// Phan chay chung cho ca che do service va console (g_stopEvent da duoc tao o wmain)
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

// Ctrl+C / dong cua so console -> dung server giong nhu lenh stop cua SCM
static BOOL WINAPI ConsoleHandler(DWORD)
{
	SetEvent(g_stopEvent);
	return TRUE;
}

// Che do console: bam F5 trong Visual Studio la chay duoc, debug duoc, khong can cai service.
// Go q + Enter (hoac Ctrl+C, hoac dong cua so) -> dung server theo dung duong stop cua service.
static int RunConsole()
{
	g_consoleMode = true;
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	printf("Chay o che do console. Go q + Enter de dung server.\n");

	std::thread([] {
		char line[64];
		while (fgets(line, sizeof(line), stdin))
		{
			// Bo qua BOM UTF-8 (EF BB BF) va khoang trang o dau dong
			const unsigned char* p = (const unsigned char*)line;
			if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) p += 3;
			while (*p == ' ' || *p == '\t') p++;
			if (*p == 'q' || *p == 'Q')
			{
				Log("Nhan lenh dung tu ban phim");
				SetEvent(g_stopEvent);
				return;
			}
		}
	}).detach();

	DWORD err = RunServer();
	if (err != NO_ERROR)
		printf("Khong chay duoc server (cong %d dang bi chiem? service co dang chay khong?)\n", ECHO_PORT);
	return (int)err;
}

// ---------------------------------------------------------------------------
// Cai dat / go bo service
// ---------------------------------------------------------------------------
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

	printf("Da cai service. Chay bang: sc start CMCEchoService\n");
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

	// Dang chay thi dung truoc, cho toi da ~10 giay
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

	// Event bao dung, dung chung cho ca service (SCM stop) va console (q / Ctrl+C)
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
