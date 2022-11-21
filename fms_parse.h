// fms_parse.h
#pragma once
#include <ctype.h>
#include <ctime>
#include <limits>
#include <algorithm>
#include <tuple>

namespace fms {
	// len < 0 is error
	template<class T>
	struct view {
		T* buf;
		int len;
		view(T* buf = nullptr, int len = 0)
			: buf(buf), len(len)
		{ }
		template<size_t N>
		view(T(&buf)[N])
			: view(buf, N - 1)
		{ }

		operator bool() const
		{
			return len > 0;
		}
		T& operator*()
		{
			return *buf;
		}
		T operator*() const
		{
			return *buf;
		}
		view& operator++()
		{
			if (len) {
				++buf;
				--len;
			}

			return *this;
		}
		view operator++(int)
		{
			auto t(*this);

			++t;

			return t;
		}

		bool equal(const view& w) const
		{
			return len >= 0 && w.len >= 0 && len == w.len
				&& 0 == memcmp(buf, w.buf, len * sizeof(T));
		}

		bool is_error() const
		{
			return len < 0;
		}
		view error() const
		{
			return view(buf, -abs(len));
		}
		// convert error to view
		view<T> error_msg() const
		{
			return view<T>(buf, abs(len));
		}

		view& drop(int n)
		{
			n = std::clamp(-len, n, len);

			if (n > 0) {
				buf += n;
				len -= n;
			}
			else if (n < 0) {
				len += n;
			}

			return *this;
		}
		view& take(int n) {
			n = std::clamp(-len, n, len);

			if (n >= 0) {
				len = n;
			}
			else {
				buf += len + n;
				len = -n;
			}

			return *this;
		}

		bool eat(T t)
		{
			if (len && *buf == t) {
				operator++();

				return true;
			}

			return false;
		}
		bool eat_all(const T* ts, int nts)
		{
			while (nts--) {
				if (!eat(*ts++)) return false;
			}

			return true;
		}
		// null terminated
		bool eat_all(const T* ts)
		{
			if (!ts || !*ts) {
				return true;
			}

			return eat(*ts) && eat_all(++ts);
		}

		bool eat_any(const T* ts, int nts)
		{
			while (nts--) {
				if (eat(*ts++)) return true;
			}

			return false;
		}
		bool eat_any(const T* ts)
		{
			if (!ts || !*ts) {
				return false;
			}

			return eat(*ts) || eat_any(++ts);
		}

		view& eat_ws()
		{
			while (len && isspace(*buf)) {
				operator++();
			}

			return *this;
		}
	};

	template<class T>
	inline view<T> drop(view<T> v, int n)
	{
		return v.drop(n);
	}
	template<class T>
	inline view<T> take(view<T> v, int n)
	{
		return v.take(n);
	}


	// parse at least min and at most max leading digits to int
	template<class T>
	inline int parse_int(view<T>& v, int min = 0, int max = INT_MAX)
	{
		int i = 0;
		int sgn = 1;

		if (v.len && *v == '-') {
			sgn = -1;
			v.eat('-');
		}
		v.eat('+');

		while (v.len && isdigit(*v)) {
			if (max <= 0) {
				v = v.error();

				return INT_MAX;
			}

			int c = *v - '0';
			if (c < 10) {
				i = 10 * i + (*v - '0');
			}
			else {
				v = v.error();

				return INT_MAX;
			}

			++v;
			--min;
			--max;
		}
		if (min > 0) {
			v = v.error();

			return INT_MIN;
		}

		return sgn*i;
	}

	// [+-]ddd[.ddd][eE][+-]ddd
	template<class T>
	double parse_double(view<T>& v)
	{
		static const double NaN = std::numeric_limits<double>::quiet_NaN();

		double d = parse_int(v);
		if (v.error()) return NaN;

		if (v.eat('.')) {
			double e = .1;
			while (isdigit(*v)) {
				d += (*v - '0') * e;
				e /= 10;
				++v;
			}
		}

		if (v.eat('e') || v.eat('E')) {
			// +|-ddd
			double sgn = 1;
			if (v.eat('-')) {
				sgn = -1;
			}
			else {
				v.eat('+');
			}
			int e = parse_int(v);
			if (v.error()) return NaN;

			d *= pow(10., sgn * e);
		}

		return d;
	}

	// yyyy-mm-dd or yyyy/mm/dd
	template<class T>
	inline std::tuple<int, int, int> parse_ymd(fms::view<T>& v)
	{
		int y = INT_MIN, m = INT_MIN, d = INT_MIN;
		std::remove_const_t<T> sep = 0;

		y = parse_int(v, 1, 4);
		if (!v) goto parse_ymd_done;

		sep = *v;
		if (sep != '-' && sep != '/') {
			v = v.error();
			goto parse_ymd_done;
		}
		if (!v.eat(sep)) {
			v = v.error();
			goto parse_ymd_done;
		}

		m = parse_int(v, 1, 2);
		if (v.error()) goto parse_ymd_done;

		if (!v.eat(sep)) {
			v = v.error();
			goto parse_ymd_done;
		}

		d = parse_int(v, 1, 2);

	parse_ymd_done:

		return { y, m, d };
	}

	// hh:mm:ss
	template<class T>
	inline std::tuple<int, int, int> parse_hms(fms::view<T>& v)
	{
		int h = 0, m = 0, s = 0;

		h = parse_int(v, 1, 2);
		if (!v) goto parse_hms_done;

		if (!v.eat(':')) {
			v = v.error();
			goto parse_hms_done;
		}

		m = parse_int(v, 1, 2);
		if (!v) goto parse_hms_done;

		if (!v.eat(':')) {
			v = v.error();
			goto parse_hms_done;
		}

		s = parse_int(v, 1, 2);

	parse_hms_done:

		return { h, m, s };
	}

	// parse datetime string into struct tm
	template<class T>
	inline bool parse_tm(fms::view<T>& v, tm* ptm)
	{
		ptm->tm_isdst = -1;
		ptm->tm_hour = 0;
		ptm->tm_min = 0;
		ptm->tm_sec = 0;

		auto [y, m, d] = parse_ymd(v);
		ptm->tm_year = y - 1900;
		ptm->tm_mon = m - 1;
		ptm->tm_mday = d;

		if (!v.len) return true;
		if (!v) return false;

		if (!v.eat(' ') && !v.eat('T')) {
			v = v.error();
			return false;
		}

		if (!v.len) return true;

		auto [hh, mm, ss] = parse_hms(v);
		ptm->tm_hour = hh;
		ptm->tm_min = mm;
		ptm->tm_sec = ss;
		if (v.is_error()) return false;

		return true;
	}


#ifdef _DEBUG
#include <cassert>

	template<class T>
	inline bool parse_test()
	{
		{
			view<T> v;
			assert(!v);
			view<T> v2{ v };
			assert(!v2);
			assert(v.equal(v2));
			v = v2;
			assert(!v);
			assert(v2.equal(v));
		}
		{
			T buf[] = { 'a', 'b', 'c' };
			view v(buf, 3);
			assert(v.len == 3);

			assert(drop(v, 0).equal(v));
			assert(!drop(v, 3));
			assert(drop(v, 1).equal(view(buf + 1, 2)));
			assert(drop(v, -1).equal(view(buf, 2)));

			assert(take(v, 3).equal(v));
			assert(!take(v, 0));
			assert(take(v, 1).equal(view(buf, 1)));
			assert(take(v, -1).equal(view(buf + 2, 1)));
		}
		{
			T buf[] = {'1', '2', '3'};
			view v(buf, 3);
			assert(123 == parse_int(v));
			assert(!v);
		}
		{
			T buf[] = { '-', '1' };
			view v(buf, 2);
			assert(-1 == parse_int(v));
			assert(!v);
		}
		{
			T buf[] = { '1', '2', '3', 'x'};
			view v(buf, 4);
			assert(123 == parse_int(v));
			assert(v);
			assert('x' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(12 == parse_int(v));
			assert(v && 'c' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(12 == parse_int(v, 2, 2));
			assert(v && 'c' == *v);
		}
		{
			T buf[] = { '1', '2', 'c' };
			view v(buf, 3);
			assert(INT_MAX == parse_int(v, 0, 1));
			assert(!v && '2' == *v.error_msg());
		}
		{
			const char date[] = "2022-01-02 3:04:5Z";
			view v(date, (int)strlen(date));
			auto [y, m, d] = parse_ymd<const char>(v);
			assert(2022 == y);
			assert(1 == m);
			assert(2 == d);
			v.eat_ws();
			auto [hh, mm, ss] = parse_hms<const char>(v);
			assert(3 == hh);
			assert(4 == mm);
			assert(5 == ss);
		}
		{
			time_t t = time(nullptr);
			tm tm;
			localtime_s(&tm, &t);
			char buf[256];
			strftime(buf, 256, "%Y-%m-%d %H:%M:%S", &tm);
			view<const char> v(buf, (int)strlen(buf));
			assert(parse_tm(v, &tm));
			time_t u = mktime(&tm);
			assert(t == u);
		}
		{
			const char* buf = "2022-1-1";
			view<const char> v(buf, (int)strlen(buf));
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime(&tm);
			tm = *gmtime(&u);
			assert(tm.tm_hour == 0);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}
		{
			const char* buf = "2022-1-1T0:0:0.0Z";
			view<const char> v(buf, (int)strlen(buf));
			tm tm;
			assert(parse_tm(v, &tm));
			time_t u = _mkgmtime(&tm);
			tm = *gmtime(&u);
			assert(tm.tm_hour == 0);
			assert(tm.tm_min == 0);
			assert(tm.tm_sec == 0);
		}

		return true;
	}

#endif // _DEBUG

} // namespace fms
