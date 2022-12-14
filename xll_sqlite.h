// xll_sqlite.h
#pragma once
#pragma warning(disable : 5103)
#pragma warning(disable : 5105)
#include <algorithm>
#include <charconv>
#include <iterator>
#include <numeric>
#include "fms_sqlite/fms_sqlite.h"
#include "xll_mem_oper.h"
#include "xll/xll/splitpath.h"
#include "xll/xll/xll.h"
#include "xll_text.h"

#ifndef CATEGORY
#define CATEGORY "SQL"
#endif

// common arguments
inline const auto Arg_db = xll::Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database.");
inline const auto Arg_stmt = xll::Arg(XLL_HANDLEX, "stmt", "is a handle to a sqlite statement.");
inline const auto Arg_sql = xll::Arg(XLL_LPOPER, "sql", "is a SQL query to execute.");
inline const auto Arg_bind = xll::Arg(XLL_LPOPER4, "_bind", "is an optional array of values to bind.");
inline const auto Arg_nh = xll::Arg(XLL_BOOL, "no_headers", "is a optional boolean value indicating not to return headers. Default is FALSE.");
inline const auto Arg_table = xll::Arg(XLL_LPOPER4, "name", "the name of a table.");

// xltype to sqlite type
#define XLL_SQLITE_TYPE(X) \
X(xltypeInt,     SQLITE_INTEGER, "INTEGER") \
X(xltypeBool,    SQLITE_BOOLEAN, "BOOLEAN") \
X(xltypeNum,     SQLITE_FLOAT,   "FLOAT")   \
X(xltypeStr,     SQLITE_TEXT,    "TEXT")    \
X(xltypeBigData, SQLITE_BLOB,    "BLOB")    \
X(xltypeNil,     SQLITE_NULL,    "NULL")    \

namespace xll {

	inline const struct {
		int xltype; int sqltype;
	} xl_sql_type[] = {
#define XL_SQL_TYPE(a,b,c) { a, b }, 
		XLL_SQLITE_TYPE(XL_SQL_TYPE)
#undef XL_SQL_TYPE
	};

	// Excel type to extended SQLite type.
	inline constexpr int sqltype(int xltype)
	{
		for (auto [x, s] : xl_sql_type) {
#define XS_TYPE(a, b, c) if (xltype == x) return s;
			XLL_SQLITE_TYPE(XS_TYPE)
#undef XS_TYPE
		}
		return SQLITE_UNKNOWN;
	}

	// Extended SQLite type to Excel type.
	inline constexpr int xltype(int sqltype)
	{
		for (auto [x, s] : xl_sql_type) {
#define SX_TYPE(a, b, c) if (sqltype == s) return x;
			XLL_SQLITE_TYPE(SX_TYPE)
#undef SX_TYPE
		}
		return xltypeErr;
	}

	// time_t to Excel Julian date
	inline double to_excel(time_t t)
	{
		return static_cast<double>(25569. + t / 86400.);
	}
	// Gregorian to Excel
	inline double to_excel(double d)
	{
		return (d - 2440587.5);
	}
	// Excel Julian date to time_t
	inline time_t to_time_t(double d)
	{
		return static_cast<time_t>((d - 25569) * 86400);
	}

	inline auto view(const XLOPER& o)
	{
		ensure(xltypeStr == type(o));

		return fms::view<char>(o.val.str + 1, o.val.str[0]);
	}
	inline auto view(const XLOPER12& o)
	{
		ensure(xltypeStr == type(o));

		return fms::view<XCHAR>(o.val.str + 1, o.val.str[0]);
	}

	inline auto string_view(const XLOPER& o)
	{
		ensure(xltypeStr == type(o));

		return std::string_view(o.val.str + 1, o.val.str[0]);
	}
	inline auto string_view(const XLOPER12& o)
	{
		ensure(xltypeStr == type(o));

		return std::wstring_view(o.val.str + 1, o.val.str[0]);
	}

	// heuristic to detect Excel date type
	// if number in [1970, 3000] then possible date
	template<class X>
	inline bool possibly_num_date(const XOPER<X>& x)
	{
		static const double _1970 = 25569.00;
		static const double _3000 = 401769.00;
		
		return (x.type() == xltypeNum) and _1970 <= x.val.num and x.val.num <= _3000;
	}

	template<class X>
	inline bool is_str_date(const XOPER<X>& x, tm* ptm)
	{
		if (x.type() != xltypeStr) {
			return false;
		}

		auto vdt = view(x);
		if (vdt.len == 0) {
			return false;
		}

		return fms::parse_tm(vdt, ptm);
	}

	// bool < int < float < text
	template<class X>
	inline int guess_one_sqltype(const XOPER<X>& x)
	{
		if (!x) return SQLITE_NULL;

		if (x.type() == xltypeStr) {
			if (x.val.str[0] == 0) {
				return SQLITE_NULL;
			}
			struct tm tm;
			if (is_str_date(x, &tm)) {
				return SQLITE_DATETIME;
			}
			else {
				return SQLITE_TEXT;
			}
		}

		if (possibly_num_date(x)) {
			return SQLITE_DATETIME;
		}

		return sqltype(x.type());
	}

	template<class X>
	inline int guess_sqltype(const XOPER<X>& x)
	{
		int type = guess_one_sqltype(x[0u]);
		bool isdate = type == SQLITE_DATETIME;
		bool notdate = !isdate;

		if (x[0u].is_str() and isdate) {
			return type; // override all other types
		}

		for (unsigned i = 1; i < x.size(); ++i) {
			int ti = guess_one_sqltype(x[i]);

			if (ti == SQLITE_NULL) {
				continue;
			}

			if (type != ti) {
				if (type == SQLITE_BOOLEAN) {
					type = ti;
					notdate |= true;
				}
				if (type == SQLITE_INTEGER) {
					type = ti;
					notdate |= true;
				}
				if (ti == SQLITE_FLOAT) {
					type = ti;
					if (possibly_num_date(x[i])) {
						isdate |= true;
					}
					else {
						notdate |= true;
					}
				}
				if (ti == SQLITE_DATETIME) {
					type = ti;
					isdate |= true;
				}
			}
		}

		return (isdate or !notdate) ? SQLITE_DATETIME : type;
	}

	template<class X>
	inline void bind(sqlite::stmt& stmt, int j, const X& x)
	{
		switch (type(x)) {
		case xltypeNum:
			stmt.bind(j, x.val.num);
			break;
		case xltypeInt:
			stmt.bind(j, x.val.w);
			break;
		case xltypeStr: {
			tm tm;
			auto sv = xll::view(x);
			if (0 == sv.len) {
				stmt.bind(j);
			}
			if (fms::parse_tm(sv, &tm)) {
				stmt.bind(j, _mkgmtime(&tm));
			}
			else {
				stmt.bind(j, string_view(x));
			}
		}
			break;
		case xltypeBool:
			stmt.bind(j, x.val.xbool != 0);
			break;
		default:
			stmt.bind(j);
		}
	}

	template<class X>
	inline auto as_oper(const sqlite::value& v)
	{
		switch (v.sqltype()) {
		case SQLITE_INTEGER:
			return XOPER<X>(v.as_int());
		case SQLITE_FLOAT:
			return XOPER<X>(v.as_float());
		case SQLITE_TEXT: {
			auto str = v.as_text();
			return XOPER<X>(str.data(), (int)str.size());
		}
			//case SQLITE_BLOB:
		case SQLITE_BOOLEAN:
			return XOPER<X>(v.as_boolean());
		case SQLITE_DATETIME: {
			auto dt = v.as_datetime();
			if (SQLITE_TEXT == dt.type) {
				fms::view vdt(dt.value.t);
				if (vdt.len == 0) {
					return XOPER<X>("");
				}
				struct tm tm;
				if (fms::parse_tm(vdt, &tm)) {
					return XOPER<X>(to_excel(_mkgmtime(&tm)));
				}
				else {
					return XOPER<X>(to_excel(dt.value.f));
				}
			}
			else if (SQLITE_INTEGER == dt.type) {
				return XOPER<X>(to_excel(dt.value.i));
			}
			else if (SQLITE_FLOAT == dt.type) {
				return XOPER<X>(to_excel(dt.value.f));
			}
			else {
				return XOPER<X>(XOPER<X>::Err::NA);
			}
			break;
		}
		case SQLITE_NULL:
			return XOPER<X>("");
		default: 
			return XOPER<X>(XOPER<X>::Err::NA);
		}
	}

	template<class O, class X = O::value_type>
	inline auto headers(sqlite::stmt& stmt, O& o)
	{
		for (int i = 0; i < stmt.column_count(); ++i) {
			o.push_back(XOPER<X>(stmt[i].name()));
		}
	}

	template<class O>
	inline auto map(sqlite::stmt& stmt, O& o)
	{
		using X = typename O::value_type;
		sqlite::map(stmt, std::back_inserter(o), as_oper<X>);
		
		auto c = stmt.column_count();
		if (c != 0) {
			ensure(0 == o.size() % c);
			o.resize(o.size() / c, c);
		}
	}
	template<class O>
	inline auto map(sqlite::iterator& i, O& o)
	{
		using X = typename O::value_type;
		sqlite::map(i, std::back_inserter(o), as_oper<X>);
	}

	// iterate over rows and columns of XOPER
	template<class X>
	class iterable {
		const XOPER<X>& o;
		unsigned row;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = X;

		iterable(const XOPER<X>& o, unsigned row = 0)
			: o{ o }, row{ row }
		{ }

		explicit operator bool() const
		{
			return row < o.rows();
		}

		class iterator {
			X x;
			unsigned column;
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = X;

			iterator(const X& x, unsigned column = 0)
				: x{ x }, column{ column }
			{ }

			bool operator==(const iterator& i) const
			{
				return &x == &i.x and column == i.column;
			}

			int size() const
			{
				return xll::size(x);
			}
			const X& operator[](int i) const
			{
				return xll::index(x, i);
			}

			auto begin() const
			{
				return iterator(x, 0);
			}
			auto end() const
			{
				return iterator(x, column);
			}
			explicit operator bool() const
			{
				return column < columns(x);
			}
			const X& operator*() const
			{
				return index(x, 0, column);
			}
			iterator& operator++()
			{
				if (column < columns(x)) {
					++column;
				}

				return *this;
			}
		};

		iterator operator*() const
		{
			auto c = o.val.array.columns;

			X x = {
				.val = {.array = {
					.lparray = o.val.array.lparray + row * c,
					.rows = 1,
					.columns = c} },
				.xltype = xltypeMulti };

			return iterator(x);
		}

		iterable& operator++()
		{
			if (row < o.rows()) {
				++row;
			}

			return *this;
		}
	};

	// copy XLOPER array to iterator
	template<class X>
	inline auto copy(typename iterable<X>::iterator& x, sqlite::stmt& o)
	{
		//const XOPER<X>& x{ _x };
		//ensure(x.size() == o.column_count());

		for (int i = 0; i < x.size(); ++i) {
			bind<X>(o, i + 1, x[i]);
		}
	}
	template<class X>
	inline auto copy(xll::iterable<X>& i, sqlite::stmt& o)
	{
		while (i) {
			typename iterable<X>::iterator _i{ *i };
			xll::copy<X>(_i, o);
			o.step();
			o.reset();
			++i;
		}
	}

	// 1-based
	template<class X>
	inline int bind_parameter_index(sqlite3_stmt* stmt, const XOPER<X>& o)
	{
		int i = 0; // not found

		if (o.is_num()) {
			i = o.as_int();
		}
		else if (o.is_str()) {
			i = sqlite3_bind_parameter_index(stmt, to_string(o).c_str());
		}
		else {
			ensure(!__FUNCTION__ ": index must be integer or string");
		}

		return i;
	}

	/* !!! 
	template<class X>
	inline void bind(sqlite3_stmt* stmt, int i, const XOPER<X>& o, int type = 0)
	{
		if (o.is_nil()) {
			sqlite3_bind_null(stmt, i);
			return;
		}

		if (!type) {
			type = sqltype(o.type());
		}

		switch (o.type()) {
		case xltypeNum:
			if (type == SQLITE_DATETIME) {
				sqlite3_bind_int64(stmt, i, to_time_t(o.val.num));
			}
			else {
				sqlite3_bind_double(stmt, i, o.as_num());
			}
			return;
		case xltypeStr:
			if constexpr (std::is_same_v<char, typename traits<X>::xchar>) {
				sqlite3_bind_text(stmt, i, o.val.str + 1, o.val.str[0], SQLITE_TRANSIENT);
			}
			else {
				sqlite3_bind_text16(stmt, i, o.val.str + 1, 2*o.val.str[0], SQLITE_TRANSIENT);
			}
			return;
		case xltypeBool:
			sqlite3_bind_int(stmt, i, o.val.xbool);
			return;
		case xltypeInt:
			sqlite3_bind_int(stmt, i, o.val.w);
			return;
		}

		ensure(!__FUNCTION__ ": invalid type");
	}
	*/
	inline void sqlite_bind(sqlite::stmt& stmt, const OPER4& val, sqlite3_destructor_type del = SQLITE_TRANSIENT)
	{
		size_t n = val.columns() == 2 ? val.rows() : val.size();

		for (unsigned i = 0; i < n; ++i) {
			unsigned pi = i + 1; // bind position
			if (val.columns() == 2) {
				if (!val(i, 0)) {
					continue;
				}
				std::string name = to_string(val(i, 0));
				// check sql string for ':', '@', or '$'?
				if (name[0] != ':') {
					name.insert(0, 1, ':');
				}
				pi = stmt.bind_parameter_index(name.c_str());
				if (!pi) {
					XLL_WARNING((name + ": not found").c_str());
				}
			}
			else if (!val[i]) {
				continue;
			}

			const OPER4& vali = pi ? val(i, 1) : val[i];
			if (vali.is_num()) {
				stmt.bind(pi, vali.val.num);
			}
			else if (vali.is_str()) {
				stmt.bind(pi, std::string_view(vali.val.str + 1, vali.val.str[0]), del);
			}
			else if (vali.is_nil()) {
				stmt.bind(pi);
			}
			else if (vali.is_bool()) {
				stmt.bind(pi, vali.val.xbool);
			}
			else {
				ensure(!__FUNCTION__ ": value to bind must be number, string, or null");
			}
		}
	}
	/*
	inline OPER sqlite_exec(sqlite::stmt& stmt, bool no_headers)
	{
		OPER result;

		int ret = stmt.step();

		if (SQLITE_DONE == ret) {
			result = to_handle<sqlite::stmt>(&stmt);
		}
		else if (SQLITE_ROW == ret) {
			OPER row(1, stmt.as_count());

			if (!no_headers) {
				for (unsigned i = 0; i < row.columns(); ++i) {
					row[i] = stmt.column_name(i);
				}
				result.push_bottom(row);
			}

			while (SQLITE_ROW == ret) {
				for (unsigned i = 0; i < row.columns(); ++i) {
					row[i] = column(stmt, i);
				}
				result.push_bottom(row);
				ret = stmt.step();
			}
			ensure(SQLITE_DONE == ret || !__FUNCTION__ ": step not done");
		}
		else {
			FMS_SQLITE_OK(sqlite3_db_handle(stmt), ret);
		}

		return result;
	}
	template<class X>
	inline mem::OPER<X> sqlite_exec_mem(sqlite::stmt& stmt, bool no_headers)
	{
		using xchar = mem::traits<X>::xchar;
		mem::OPER<X> result;

		int ret = stmt.step();

		if (SQLITE_DONE == ret) {
			result = to_handle<sqlite::stmt>(&stmt);
		}
		else if (SQLITE_ROW == ret) {
			int c = stmt.column_count();

			if (!no_headers) {
				for (int i = 0; i < c; ++i) {
					result.push_back(OPER(stmt.column_name(i)));
				}
			}

			while (SQLITE_ROW == ret) {
				for (int i = 0; i < c; ++i) {
					result.push_back(column(stmt, i));
				}
				ret = stmt.step();
			}
			ensure(SQLITE_DONE == ret || !__FUNCTION__ ": step not done");
			ensure(result.size() % c == 0);
			result.reshape(result.size() / c, c);
		}
		else {
			FMS_SQLITE_OK(sqlite3_db_handle(stmt), ret);
		}

		return result;
	}
	*/


} // namespace xll
