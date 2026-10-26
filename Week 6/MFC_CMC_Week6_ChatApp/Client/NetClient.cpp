#include "pch.h"
#include <ws2tcpip.h>
#include "NetClient.h"
#include "Theme.h"
#pragma comment(lib, "ws2_32.lib")

CNetClient::~CNetClient()
{
	Disconnect();
}

bool CNetClient::Connect(const CString& host, int port, CString& err)
{
	Disconnect();

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* res = nullptr;
	std::string hostA = theme::ToUtf8(host);
	if (getaddrinfo(hostA.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
	{
		err = _T("Không tìm thấy địa chỉ server.");
		return false;
	}

	SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (s == INVALID_SOCKET)
	{
		freeaddrinfo(res);
		err = _T("Không tạo được socket.");
		return false;
	}

	// connect co timeout 3 giay
	u_long nb = 1;
	ioctlsocket(s, FIONBIO, &nb);
	bool ok = false;
	if (connect(s, res->ai_addr, (int)res->ai_addrlen) == 0)
		ok = true;
	else if (WSAGetLastError() == WSAEWOULDBLOCK)
	{
		fd_set wf, ef;
		FD_ZERO(&wf); FD_ZERO(&ef);
		FD_SET(s, &wf); FD_SET(s, &ef);
		timeval tv = { 3, 0 };
		if (select(0, NULL, &wf, &ef, &tv) > 0 && FD_ISSET(s, &wf))
			ok = true;
	}
	freeaddrinfo(res);

	if (!ok)
	{
		closesocket(s);
		err = _T("Không kết nối được tới server. Kiểm tra server đã chạy chưa, địa chỉ IP và cổng.");
		return false;
	}

	nb = 0;
	ioctlsocket(s, FIONBIO, &nb);
	DWORD sendTimeout = 5000;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&sendTimeout, sizeof(sendTimeout));
	BOOL keepAlive = TRUE;
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));

	{
		std::lock_guard<std::mutex> lk(m_queueMtx);
		m_queue.clear();
	}
	m_sock = s;
	m_stop = false;
	m_closed = false;
	m_thread = std::thread(&CNetClient::RecvLoop, this);
	return true;
}

void CNetClient::Disconnect()
{
	m_stop = true;
	if (m_thread.joinable()) m_thread.join();   // RecvAll kiem tra m_stop moi 200ms nen join nhanh
	if (m_sock != INVALID_SOCKET)
	{
		std::lock_guard<std::mutex> lk(m_sendMtx);
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	m_closed = true;
}

bool CNetClient::Send(uint32_t action, const json::Value& body)
{
	std::string data = proto::Build(action, body);
	std::lock_guard<std::mutex> lk(m_sendMtx);
	if (m_sock == INVALID_SOCKET || m_closed) return false;
	return proto::SendAll(m_sock, data);
}

void CNetClient::SetNotifyWindow(HWND hwnd)
{
	m_hwnd = hwnd;
	bool pending;
	{
		std::lock_guard<std::mutex> lk(m_queueMtx);
		pending = !m_queue.empty();
	}
	if (hwnd && pending)                          // con goi tin cho -> bao cua so moi xu ly luon
		::PostMessage(hwnd, WM_NET_EVENT, 0, 0);
}

bool CNetClient::Pop(proto::Packet& pkt)
{
	std::lock_guard<std::mutex> lk(m_queueMtx);
	if (m_queue.empty()) return false;
	pkt = std::move(m_queue.front());
	m_queue.pop_front();
	return true;
}

void CNetClient::Push(const proto::Packet& pkt)
{
	{
		std::lock_guard<std::mutex> lk(m_queueMtx);
		m_queue.push_back(pkt);
	}
	HWND h = m_hwnd;
	if (h) ::PostMessage(h, WM_NET_EVENT, 0, 0);
}

void CNetClient::RecvLoop()
{
	proto::Packet p;
	while (proto::RecvPacket(m_sock, p, m_stop))
		Push(p);

	m_closed = true;
	if (!m_stop)                                  // mat ket noi that (khong phai tu Disconnect)
	{
		proto::Packet closed;
		closed.action = NET_CLOSED;
		Push(closed);
	}
}
