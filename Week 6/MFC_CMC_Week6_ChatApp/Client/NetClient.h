// NetClient.h : ket noi TCP toi server
// Thread nen nhan goi tin -> hang doi -> bao WM_NET_EVENT, cua so goi Pop() de lay ra
// Mat ket noi: nhan goi NET_CLOSED

#pragma once
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include "../Common/Protocol.h"

#define WM_NET_EVENT  (WM_APP + 10)
#define NET_CLOSED    0xFFFFFFFFu

class CNetClient
{
public:
	~CNetClient();

	// Ket noi (toi da 3 giay)
	bool Connect(const CString& host, int port, CString& err);
	void Disconnect();

	bool Send(uint32_t action, const json::Value& body = json::Value::Object());

	void SetNotifyWindow(HWND hwnd);
	bool Pop(proto::Packet& pkt);   // false = hang doi rong

private:
	void RecvLoop();
	void Push(const proto::Packet& pkt);

	SOCKET m_sock = INVALID_SOCKET;
	std::thread m_thread;
	std::atomic<bool> m_stop{ false };
	std::atomic<bool> m_closed{ true };
	std::atomic<HWND> m_hwnd{ NULL };
	std::mutex m_sendMtx;
	std::mutex m_queueMtx;
	std::deque<proto::Packet> m_queue;
};
