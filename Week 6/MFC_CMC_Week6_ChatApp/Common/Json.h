// Json.h : thu vien JSON nho (UTF-8) dung chung cho Client va Server
// Tao: Value::Object().Set("key", gia_tri) -> Dump(). Doc: Parse(text, v) -> v["key"].Str()
// Doc khoa khong co -> tra ve Null, khong loi.

#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdio>
#include <cstdlib>

namespace json
{
	class Value
	{
	public:
		enum Type { Null, Bool, Number, String, Array, Obj };

		Value() {}
		Value(bool b) : m_type(Bool), m_bool(b) {}
		Value(int n) : m_type(Number), m_num(n) {}
		Value(long long n) : m_type(Number), m_num(n) {}
		Value(const char* s) : m_type(String), m_str(s) {}
		Value(const std::string& s) : m_type(String), m_str(s) {}

		static Value Object() { Value v; v.m_type = Obj; return v; }
		static Value List() { Value v; v.m_type = Array; return v; }

		Type GetType() const { return m_type; }
		bool IsNull() const { return m_type == Null; }

		// ---- Ghi ----
		Value& Set(const std::string& key, const Value& v)
		{
			m_type = Obj;
			for (auto& kv : m_obj)
				if (kv.first == key) { kv.second = v; return *this; }
			m_obj.push_back(std::make_pair(key, v));
			return *this;
		}
		Value& Push(const Value& v)
		{
			m_type = Array;
			m_arr.push_back(v);
			return *this;
		}

		// ---- Doc ----
		const Value& operator[](const std::string& key) const
		{
			for (const auto& kv : m_obj)
				if (kv.first == key) return kv.second;
			return NullValue();
		}
		const Value& operator[](size_t i) const { return i < m_arr.size() ? m_arr[i] : NullValue(); }
		size_t Size() const { return m_type == Array ? m_arr.size() : m_obj.size(); }

		std::string Str() const { return m_type == String ? m_str : std::string(); }
		long long Int() const { return m_type == Number ? m_num : 0; }
		bool AsBool() const { return m_type == Bool ? m_bool : false; }

		// ---- Chuyen thanh chuoi JSON ----
		std::string Dump() const
		{
			std::string out;
			DumpTo(out);
			return out;
		}

		// ---- Phan tich chuoi JSON. Sai cu phap -> false ----
		static bool Parse(const std::string& text, Value& out)
		{
			size_t pos = 0;
			out = Value();
			if (!ParseValue(text, pos, out, 0)) return false;
			SkipSpace(text, pos);
			return pos == text.size();
		}

	private:
		Type m_type = Null;
		bool m_bool = false;
		long long m_num = 0;
		std::string m_str;
		std::vector<Value> m_arr;
		std::vector<std::pair<std::string, Value>> m_obj;

		static const Value& NullValue() { static const Value v; return v; }

		static void Escape(const std::string& s, std::string& out)
		{
			out += '"';
			for (unsigned char c : s)
			{
				switch (c)
				{
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default:
					if (c < 0x20)
					{
						char buf[8];
						sprintf_s(buf, "\\u%04x", c);
						out += buf;
					}
					else
						out += (char)c;   // byte UTF-8 giu nguyen
				}
			}
			out += '"';
		}

		void DumpTo(std::string& out) const
		{
			switch (m_type)
			{
			case Null:   out += "null"; break;
			case Bool:   out += m_bool ? "true" : "false"; break;
			case Number: out += std::to_string(m_num); break;
			case String: Escape(m_str, out); break;
			case Array:
				out += '[';
				for (size_t i = 0; i < m_arr.size(); i++)
				{
					if (i) out += ',';
					m_arr[i].DumpTo(out);
				}
				out += ']';
				break;
			case Obj:
				out += '{';
				for (size_t i = 0; i < m_obj.size(); i++)
				{
					if (i) out += ',';
					Escape(m_obj[i].first, out);
					out += ':';
					m_obj[i].second.DumpTo(out);
				}
				out += '}';
				break;
			}
		}

		static void SkipSpace(const std::string& t, size_t& p)
		{
			while (p < t.size() && (t[p] == ' ' || t[p] == '\t' || t[p] == '\n' || t[p] == '\r')) p++;
		}

		static void AppendUtf8(std::string& out, unsigned cp)
		{
			if (cp < 0x80) out += (char)cp;
			else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
			else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
			else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
		}

		static bool ParseHex4(const std::string& t, size_t& p, unsigned& cp)
		{
			if (p + 4 > t.size()) return false;
			cp = 0;
			for (int i = 0; i < 4; i++)
			{
				char c = t[p++];
				cp <<= 4;
				if (c >= '0' && c <= '9') cp |= c - '0';
				else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
				else return false;
			}
			return true;
		}

		static bool ParseString(const std::string& t, size_t& p, std::string& out)
		{
			if (p >= t.size() || t[p] != '"') return false;
			p++;
			while (p < t.size())
			{
				char c = t[p++];
				if (c == '"') return true;
				if (c != '\\') { out += c; continue; }
				if (p >= t.size()) return false;
				char e = t[p++];
				switch (e)
				{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					unsigned cp;
					if (!ParseHex4(t, p, cp)) return false;
					// Cap surrogate (emoji...): 😀
					if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= t.size() && t[p] == '\\' && t[p + 1] == 'u')
					{
						p += 2;
						unsigned lo;
						if (!ParseHex4(t, p, lo)) return false;
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					AppendUtf8(out, cp);
					break;
				}
				default: return false;
				}
			}
			return false;
		}

		static bool ParseValue(const std::string& t, size_t& p, Value& out, int depth)
		{
			if (depth > 32) return false;   // chong goi tin long qua sau
			SkipSpace(t, p);
			if (p >= t.size()) return false;
			char c = t[p];

			if (c == '"')
			{
				out.m_type = String;
				return ParseString(t, p, out.m_str);
			}
			if (c == '{')
			{
				out.m_type = Obj;
				p++;
				SkipSpace(t, p);
				if (p < t.size() && t[p] == '}') { p++; return true; }
				for (;;)
				{
					SkipSpace(t, p);
					std::string key;
					if (!ParseString(t, p, key)) return false;
					SkipSpace(t, p);
					if (p >= t.size() || t[p] != ':') return false;
					p++;
					Value v;
					if (!ParseValue(t, p, v, depth + 1)) return false;
					out.m_obj.push_back(std::make_pair(key, v));
					SkipSpace(t, p);
					if (p < t.size() && t[p] == ',') { p++; continue; }
					if (p < t.size() && t[p] == '}') { p++; return true; }
					return false;
				}
			}
			if (c == '[')
			{
				out.m_type = Array;
				p++;
				SkipSpace(t, p);
				if (p < t.size() && t[p] == ']') { p++; return true; }
				for (;;)
				{
					Value v;
					if (!ParseValue(t, p, v, depth + 1)) return false;
					out.m_arr.push_back(v);
					SkipSpace(t, p);
					if (p < t.size() && t[p] == ',') { p++; continue; }
					if (p < t.size() && t[p] == ']') { p++; return true; }
					return false;
				}
			}
			if (t.compare(p, 4, "true") == 0) { out = Value(true); p += 4; return true; }
			if (t.compare(p, 5, "false") == 0) { out = Value(false); p += 5; return true; }
			if (t.compare(p, 4, "null") == 0) { out = Value(); p += 4; return true; }
			if (c == '-' || (c >= '0' && c <= '9'))
			{
				size_t start = p;
				if (t[p] == '-') p++;
				while (p < t.size() && ((t[p] >= '0' && t[p] <= '9') || t[p] == '.' || t[p] == 'e' || t[p] == 'E' || t[p] == '+' || t[p] == '-')) p++;
				out = Value((long long)strtod(t.substr(start, p - start).c_str(), nullptr));
				return true;
			}
			return false;
		}
	};
}
