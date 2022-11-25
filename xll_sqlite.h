// xll_sqlite.h
#pragma once
#include <numeric>
#include "fms_sqlite.h"
#include "xll_mem_oper.h"
#include "xll/xll/splitpath.h"
#include "xll/xll/xll.h"
#include "xll_text.h"

#ifndef CATEGORY
#define CATEGORY "SQL"
#endif

// xltype to sqlite type
#define XLL_SQLITE_TYPE(X) \
X(xltypeInt,     SQLITE_INTEGER, "INTEGER") \
X(xltypeBool,    SQLITE_INTEGER, "INTEGER") \
X(xltypeNum,     SQLITE_FLOAT,   "FLOAT")   \
X(xltypeStr,     SQLITE_TEXT,    "TEXT")    \
X(xltypeBigData, SQLITE_BLOB,    "BLOB")    \
X(xltypeNil,     SQLITE_NULL,    "NULL")    \

namespace xll {

	// common arguments
	static const auto Arg_db   = Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database.");
	static const auto Arg_stmt = Arg(XLL_HANDLEX, "stmt", "is a handle to a sqlite statement.");
	static const auto Arg_sql  = Arg(XLL_LPOPER12, "sql", "is a SQL query to execute.");
	static const auto Arg_bind = Arg(XLL_LPOPER4, "_bind", "is an optional array of values to bind.");
	static const auto Arg_nh   = Arg(XLL_BOOL, "no_headers", "is a optional boolean value indicating not to return headers. Default is FALSE.");

#define XLTYPE(a, b, c) {a, b},
	inline std::map<int, int> sqlite_type = {
		XLL_SQLITE_TYPE(XLTYPE)
	};
#undef XLTYPE

#define XLNAME(a, b, c) {a, c},
	inline std::map<int, const char*> sqlite_name = {
		XLL_SQLITE_TYPE(XLNAME)
	};
#undef XLNAME

	inline auto view(const OPER& o)
	{
		return fms::view(o.val.str + 1, o.val.str[0]);
	}

	// time_t to Excel Julian date
	inline double to_julian(time_t t)
	{
		return static_cast<double>(25569. + t / 86400.);
	}
	// Excel Julian date to time_t
	inline time_t to_time(double d)
	{
		return static_cast<time_t>((d - 25569) * 86400);
	}

	inline int bind_parameter_index(sqlite3_stmt* stmt, const OPER& o)
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

		return sqlite_name[o.xltype];
	}

	// convert column i to OPER
	inline OPER column(sqlite3_stmt* stmt, int i)
	{
		//int type = sqlite::type(sqlite3_column_decltype(stmt, i));
		int ctype = sqlite3_column_type(stmt, i);

		if (ctype == SQLITE_NULL) {
			return OPER("");
		}

		switch (ctype) {
		case SQLITE_INTEGER:
			return OPER(sqlite3_column_int(stmt, i));
		case SQLITE_FLOAT:
			return OPER(sqlite3_column_double(stmt, i));
		case SQLITE_TEXT:
			return OPER((const char*)sqlite3_column_text(stmt, i), sqlite3_column_bytes(stmt, i));
		case SQLITE_DATETIME:
			if (SQLITE_TEXT == ctype) {
				fms::view v(sqlite3_column_text(stmt, i), sqlite3_column_bytes(stmt, i));
				if (v.len == 0)
					return OPER("");
				struct tm tm;
				if (fms::parse_tm(v, &tm)) {
					return OPER(to_julian(_mkgmtime(&tm)));
				}
				else {
					return ErrValue;
				}
			}
			else if (SQLITE_INTEGER == ctype) {
				time_t t = sqlite3_column_int64(stmt, i);
				if (t) {
					return OPER(to_julian(t));
				}
			}
			else if (SQLITE_FLOAT == ctype) {
				double d = sqlite3_column_double(stmt, i);
				return OPER(d - 2440587.5); // Julian
			}
			else {
				return ErrNA; // can't happen
			}
		//case SQLITE_NUMERIC: 
		// case SQLITE_BLOB:
		}

		return ErrNA;
	}

	inline void sqlite_bind(sqlite3_stmt* stmt, const OPER4& val, sqlite3_destructor_type del = SQLITE_TRANSIENT)
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
				pi = sqlite3_bind_parameter_index(stmt, name.c_str());
			}
			else if (!val[i]) {
				continue;
			}

			const OPER4& vali = pi ? val(i, 1) : val[i];
			if (vali.is_num()) {
				sqlite3_bind_double(stmt, pi, vali.val.num);
			}
			else if (vali.is_str()) {
				sqlite3_bind_text(stmt, pi, vali.val.str + 1, vali.val.str[0], del);
			}
			else {
				ensure(!__FUNCTION__ ": value to bind must be number or string");
			}
		}
	}

	inline OPER sqlite_exec(sqlite::stmt& stmt, bool no_headers)
	{
		OPER result;

		int ret = stmt.step();

		if (SQLITE_DONE == ret) {
			result = to_handle<sqlite::stmt>(&stmt);
		}
		else if (SQLITE_ROW == ret) {
			OPER row(1, stmt.column_count());

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
			result.reshape(result.size() / c, c);
		}
		else {
			FMS_SQLITE_OK(sqlite3_db_handle(stmt), ret);
		}

		return result;
	}


} // namespace xll
