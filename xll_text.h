// xll_text.h - text functions
#pragma once
#include "xll24/xll.h"

namespace xll {

	inline std::string wcstombs(const wchar_t* ws, int wn = -1)
	{
		int n = WideCharToMultiByte(CP_UTF8, 0, ws, wn, NULL, 0, NULL, NULL);
		std::string s(n, '\0');

		int n_ = WideCharToMultiByte(CP_UTF8, 0, ws, wn, s.data(), n, NULL, NULL);
		ensure(n_ == n);
		if (wn == -1) {
			s.resize(--n); // remove terminating null
		}

		return s;
	}

	// quote("ab{c", '{', '}') => "{ab\{c}"
	inline std::string quote(std::string s, char lq = 0, char rq = 0)
	{
		if (lq) {
			std::string esc("\\");
			esc.push_back(lq);
			auto i = s.find(lq);
			while (i != std::string::npos) {
				s.replace(i, 1, esc);
				i = s.find(lq, i + 2);
			}
			s.insert(0, 1, lq);
		}

		if (rq || lq) {
			s.push_back(rq ? rq : lq);
		}

		return s;
	}

	// Similar to TEXTJOIN.
	inline OPER text_join(const OPER& o, const char* fs = 0, const char* rs = 0, const char* lq = 0, const char* rq = 0)
	{
		OPER O;
		OPER FS(fs);
		OPER RS(rs ? rs : fs);
		OPER LQ(lq ? lq : "");
		OPER RQ(rq ? rq : LQ);

		for (unsigned i = 0; i < o.rows(); ++i) {
			if (rs && i > 0) {
				O &= RS;
			}
			for (unsigned j = 0; j < o.columns(); ++j) {
				if (fs && j > 0) {
					O &= FS;
				}
				O &= LQ & o(i, j) & RQ;
			}
		}

		return O;
	}


	// Convert OPER to string for SQL query.
	inline std::string to_string(const OPER& o, const char* fs = 0, const char* rs = 0)
	{
		std::string s;

		if (o.is_multi()) {
			for (unsigned i = 0; i < o.rows(); ++i) {
				if (rs && i > 0) {
					s.append(rs);
				}
				for (unsigned j = 0; j < o.columns(); ++j) {
					if (fs && j > 0) {
						s.append(fs);
					}
					s.append(to_string(o(i, j)));
				}
			}
		}
		else if (o.is_str()) {
			s = wcstombs(o.val.str + 1, o.val.str[0]);
		}
		else if (o.is_bool()) {
			s = o.val.xbool ? "TRUE" : "FALSE";
		}
		else if (o.is_nil()) {
			s = "";
		}
		else if (o.is_err()) {
			s = xll_err_str[o.val.err];
		}
		else {
			const auto& v = Excel(xlfText, o, OPER("General"));
			s = wcstombs(v.val.str + 1, v.val.str[0]);
		}

		return s;
	}

#ifdef _DEBUG
	inline void to_string_test()
	{
		{
			std::string s = wcstombs(L"abc");
			ensure(3 == s.length());
			ensure(s == "abc");
		}
		{
			std::string s = "abc";
			auto qs = quote(s);
			ensure(qs == s);
		}
		{
			std::string s = "abc";
			auto qs = quote(s, '\'');
			ensure(qs == "'abc'");
		}
		{
			std::string s = "a{bc";
			auto qs = quote(s, '{', '}');
			ensure(qs == "{a\\{bc}");
		}
		{
			OPER o(1.23);
			std::string os = to_string(o);
			ensure(os == "1.23");
		}
		{
			OPER o("foo");
			std::string os = to_string(o);
			ensure(os == "foo");
		}
		{
			OPER o(false);
			std::string os = to_string(o);
			ensure(os == "FALSE");
		}
		{
			OPER o;
			std::string os = to_string(o);
			ensure(os == "");
		}
		{
			std::string os = to_string(ErrNA);
			ensure(os == "#N/A");
		}
		{
			OPER o({ OPER(1.23), OPER(false), OPER("foo"), ErrNA });
			const char* fs = ", ";
			const char* rs = ";";
			auto os = to_string(o, fs, rs);
			ensure(os == "1.23, FALSE, foo, #N/A");

			o.resize(2, 2);
			os = to_string(o, fs, rs);
			ensure(os == "1.23, FALSE;foo, #N/A");
		}
	}
#endif // _DEBUG

} // namespace xll
