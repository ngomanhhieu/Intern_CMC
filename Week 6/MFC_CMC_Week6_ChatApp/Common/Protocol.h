// Protocol.h : giao thuc dung chung cho Client va Server
//
// Client -> Server qua TCP, moi tin nhan deu di qua Server (Server luu DB va chuyen tiep).
// Goi tin: [action u32][length u32][JSON UTF-8], 2 so dau la big-endian.
// Phan hoi luon co "status": "ok" / "failed" (+ "reason" khi loi).
// Mat khau gui di la base64(SHA-256), thoi gian la UTC timestamp.

#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <winsock2.h>
#include <bcrypt.h>
#include "Json.h"
#pragma comment(lib, "bcrypt.lib")

namespace proto
{
	enum Action : uint32_t
	{
		REGISTER     = 1,    // {username, password, display_name}
		LOGIN        = 2,    // {username, password} -> {username, display_name}
		LOGOUT       = 3,
		GET_USERS    = 4,    // -> {users: [...]}
		GET_MESSAGES = 5,    // {with} -> {with, messages: [...]}
		CHAT         = 6,    // {to, content} -> server gui {id, from, to, content, time} cho ca 2 ben
		MARK_READ    = 7,    // {with}: danh dau da doc
		USER_STATUS  = 8,    // server tu gui: {username, display_name, online}
		GET_AVATAR   = 9,    // {username} -> {username, data: base64 anh}
		SET_AVATAR   = 10,   // {data} -> server gui GET_AVATAR moi cho moi nguoi
		TYPING       = 11,   // {to} -> server chuyen {from} cho nguoi nhan
		SEEN         = 12,   // server gui: {by, last_id} - nguoi kia da xem toi tin last_id
		ERROR_MSG    = 99,   // {reason}
	};

	// Header 8 byte o dau moi goi tin
	struct PacketHeader
	{
		uint32_t action;
		uint32_t length;
	};

	// 1 goi tin da tach: loai lenh + noi dung JSON
	struct Packet
	{
		uint32_t action = 0;
		json::Value body;
	};

	static const int      DEFAULT_PORT   = 8888;
	static const uint32_t MAX_PAYLOAD    = 4 * 1024 * 1024;   // 4 MB / goi tin
	static const int      MAX_CONTENT    = 8000;              // do dai toi da 1 tin nhan (byte UTF-8)
	static const size_t   MAX_AVATAR     = 512 * 1024;        // anh avatar toi da 512 KB

	// ---- Dong goi / gui / nhan ----
	inline void PutU32(std::string& out, uint32_t v)
	{
		out.push_back((char)((v >> 24) & 0xFF));
		out.push_back((char)((v >> 16) & 0xFF));
		out.push_back((char)((v >> 8) & 0xFF));
		out.push_back((char)(v & 0xFF));
	}

	inline uint32_t GetU32(const unsigned char* p)
	{
		return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
	}

	// Ghep header + JSON thanh chuoi byte de gui
	inline std::string Build(uint32_t action, const json::Value& body)
	{
		std::string payload = body.Dump();
		std::string out;
		out.reserve(8 + payload.size());
		PutU32(out, action);
		PutU32(out, (uint32_t)payload.size());
		out += payload;
		return out;
	}

	// Gui du toan bo du lieu (send co the chi gui duoc 1 phan)
	inline bool SendAll(SOCKET s, const std::string& data)
	{
		size_t sent = 0;
		while (sent < data.size())
		{
			int n = send(s, data.data() + sent, (int)(data.size() - sent), 0);
			if (n <= 0) return false;
			sent += n;
		}
		return true;
	}

	// Doc du len byte, 200ms kiem tra co stop 1 lan (de dung thread khong bi treo)
	inline bool RecvAll(SOCKET s, char* buf, int len, const std::atomic<bool>& stop)
	{
		int got = 0;
		while (got < len)
		{
			if (stop) return false;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(s, &fds);
			timeval tv = { 0, 200000 };
			int r = select(0, &fds, NULL, NULL, &tv);
			if (r == SOCKET_ERROR) return false;
			if (r == 0) continue;
			int n = recv(s, buf + got, len - got, 0);
			if (n <= 0) return false;
			got += n;
		}
		return true;
	}

	// Doc 1 goi tin. false = mat ket noi hoac goi tin qua lon
	inline bool RecvPacket(SOCKET s, Packet& pkt, const std::atomic<bool>& stop)
	{
		unsigned char hdr[sizeof(PacketHeader)];
		if (!RecvAll(s, (char*)hdr, sizeof(hdr), stop)) return false;

		PacketHeader h;
		h.action = GetU32(hdr);
		h.length = GetU32(hdr + 4);
		if (h.length > MAX_PAYLOAD) return false;

		std::string payload(h.length, '\0');
		if (h.length > 0 && !RecvAll(s, &payload[0], (int)h.length, stop)) return false;

		pkt.action = h.action;
		if (!json::Value::Parse(payload, pkt.body))
			pkt.body = json::Value();
		return true;
	}

	// ---- Base64 ----
	inline std::string Base64Encode(const std::string& in)
	{
		static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		size_t i = 0;
		while (i + 2 < in.size())
		{
			uint32_t v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
			out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += tbl[v & 63];
			i += 3;
		}
		if (i + 1 == in.size())
		{
			uint32_t v = (unsigned char)in[i] << 16;
			out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += "==";
		}
		else if (i + 2 == in.size())
		{
			uint32_t v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8);
			out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += '=';
		}
		return out;
	}

	inline bool Base64Decode(const std::string& in, std::string& out)
	{
		auto val = [](char c) -> int {
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '+') return 62;
			if (c == '/') return 63;
			return -1;
		};
		out.clear();
		if (in.size() % 4 != 0) return false;
		for (size_t i = 0; i < in.size(); i += 4)
		{
			int a = val(in[i]), b = val(in[i + 1]);
			int c = in[i + 2] == '=' ? 0 : val(in[i + 2]);
			int d = in[i + 3] == '=' ? 0 : val(in[i + 3]);
			if (a < 0 || b < 0 || c < 0 || d < 0) return false;
			uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
			out += (char)((v >> 16) & 0xFF);
			if (in[i + 2] != '=') out += (char)((v >> 8) & 0xFF);
			if (in[i + 3] != '=') out += (char)(v & 0xFF);
		}
		return true;
	}

	// Client bam mat khau truoc khi gui: base64(SHA-256(mat khau))
	inline std::string HashPasswordForWire(const std::string& passwordUtf8)
	{
		unsigned char hash[32] = {};
		BCRYPT_ALG_HANDLE alg = NULL;
		if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
		{
			BCryptHash(alg, NULL, 0, (PUCHAR)passwordUtf8.data(), (ULONG)passwordUtf8.size(), hash, sizeof(hash));
			BCryptCloseAlgorithmProvider(alg, 0);
		}
		return Base64Encode(std::string((const char*)hash, sizeof(hash)));
	}

	// Bo ky tu dieu khien (giu xuong dong, tab)
	inline std::string Clean(const std::string& s)
	{
		std::string out;
		out.reserve(s.size());
		for (unsigned char c : s)
			if (c >= 0x20 || c == '\n' || c == '\t')
				out.push_back((char)c);
		return out;
	}

	// Phan hoi chuan ok / failed
	inline json::Value Ok() { return json::Value::Object().Set("status", "ok"); }
	inline json::Value Failed(const std::string& reason)
	{
		return json::Value::Object().Set("status", "failed").Set("reason", reason);
	}
}
