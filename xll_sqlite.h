// xll_sqlite.h
#pragma once
#pragma warning(disable : 5103)
#pragma warning(disable : 5105)
#include <charconv>
#include <numeric>
#include "fms_sqlite.h"
#include "fms_parse.h"
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

	// xltype to sqlite basic type
#define XLTYPE(a, b, c) {a, b},
	inline std::map<int, int> sqlite_type = {
		XLL_SQLITE_TYPE(XLTYPE)
	};
#undef XLTYPE

#define XLNAME(a, b, c) {a, c},
	inline std::map<int, const char*> sqlite_decltype = {
		XLL_SQLITE_TYPE(XLNAME)
	};
#undef XLNAME

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

	template<class X>
	class back_inserter {
		X& x;
	public:
		back_inserter(X& x)
			: x{ x }
		{ }
		void push_back(void)
		{
			x.push_back(X{});
		}
		template<class T>
		void push_back(const T& t)
		{
			x.push_back(X{ t });
		}
		template<>
		void push_back(const std::string_view& t)
		{
			x.push_back(X(t.data(), (int)t.size()));
		}
		template<>
		void push_back(const sqlite::datetime& t)
		{
			x.push_back(X(t.type));
		}
		template<>
		void push_back(const std::basic_string_view<std::byte>& t)
		{
			x.push_back(X(t.size()));
		}
	};

	// iterate over rows and columns of XOPER
	template<class X>
	class cursor {
		const XOPER<X>& o;
		unsigned row;
	public:
		using value_type = X;

		cursor(const XOPER<X>& o, unsigned row = 0)
			: o{ o }, row{row}
		{ }

		explicit operator bool() const
		{
			return row < o.rows();
		}

		class row_iter {
			X x;
			unsigned column;
		public:
			row_iter(const X& x, unsigned column = 0)
				: x{ x }, column{ column }
			{ }
			explicit operator bool() const
			{
				return column < columns(x);
			}
			const X& operator*() const
			{
				return index(x, 0, column);
			}
			row_iter& operator++()
			{
				if (column < columns(x)) {
					++column;
				}

				return *this;
			}
		};
		
		row_iter operator*() const
		{
			auto c = o.val.array.columns;
	
			X x = {
				.val = {.array = {
					.lparray = o.val.array.lparray + row * c,
					.rows = 1,
					.columns = c} },
				.xltype = xltypeMulti };

			return row_iter(x);
		}

		cursor& operator++()
		{
			if (row < o.rows()) {
				++row;
			}

			return *this;
		}
	};

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

	/* !!! */
	template<class X>
	inline void bind(sqlite3_stmt* stmt, int i, const XOPER<X>& o, int type = 0)
	{
		if (o.is_nil()) {
			sqlite3_bind_null(stmt, i);
			return;
		}

		if (!type) {
			type = sqlite_type[o.type()];
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

	// Detect sqlite type name
	inline const char* type_name(const OPER& o)
	{
		if (o.is_str()) {
			auto v = view(o);
			struct tm tm;
			if (fms::parse_tm(v, &tm)) {
				return "DATETIME";
			}
		}

		return sqlite_decltype[o.type()];
	}

	// convert column i to OPER
	inline OPER column(const sqlite::stmt& stmt, int i, int type)
	{
		switch (type) {
		case SQLITE_NULL:
			return OPER("");
		case SQLITE_INTEGER:
			return OPER(stmt.as_int(i));
		case SQLITE_FLOAT:
		case SQLITE_NUMERIC:
			return OPER(stmt.as_float(i));
		case SQLITE_TEXT: {
			auto text = stmt.as_text(i);
			return OPER(text.data(), static_cast<char>(text.size()));
		}
		case SQLITE_BOOLEAN:
			return OPER(stmt.as_boolean(i));
		case SQLITE_DATETIME:
			auto dt = stmt.as_datetime(i);
			if (SQLITE_TEXT == dt.type) {
				auto vt = stmt.as_text(i);
				if (vt.size() == 0)
					return OPER("");
				fms::view v(vt.data(), (int)vt.size());
				struct tm tm;
				if (fms::parse_tm(v, &tm)) {
					return OPER(0/*to_excel(_mkgmtime(&tm))*/);
				}
				else {
					return ErrValue;
				}
			}
			if (SQLITE_INTEGER == dt.type) {
				time_t t = dt.value.i;
				if (t) {
					return 0;//!!! OPER(to_excel(t));
				}
			}
			else if (SQLITE_FLOAT == dt.type) {
				double d = dt.value.f;
				return OPER(d - 2440587.5); // Julian
			}
			else {
				return ErrValue; // can't happen
			}
		//case SQLITE_NUMERIC: 
		// case SQLITE_BLOB:
		}

		return ErrValue;
	}

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
