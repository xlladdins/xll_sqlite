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
//#include "xll24/splitpath.h"
#include "xll24/include/xll.h"
#include "xll_text.h"

#ifndef CATEGORY
#define CATEGORY "SQL"
#endif

// common arguments
inline const auto Arg_db = xll::Arg(xll::XLL_HANDLEX, "db", "is a handle to a sqlite database.");
inline const auto Arg_stmt = xll::Arg(xll::XLL_HANDLEX, "stmt", "is a handle to a sqlite statement.");
inline const auto Arg_sql = xll::Arg(xll::XLL_LPOPER, "sql", "is a SQL query to execute.");
inline const auto Arg_bind = xll::Arg(xll::XLL_LPOPER4, "_bind", "is an optional array of values to bind.");
inline const auto Arg_table = xll::Arg(xll::XLL_LPOPER4, "name", "the name of a table.");

// xltype to sqlite type
#define XLL_SQLITE_TYPE(X) \
X(xltypeInt,     SQLITE_INTEGER, "INTEGER") \
X(xltypeBool,    SQLITE_BOOLEAN, "BOOLEAN") \
X(xltypeNum,     SQLITE_FLOAT,   "FLOAT")   \
X(xltypeStr,     SQLITE_TEXT,    "TEXT")    \
X(xltypeBigData, SQLITE_BLOB,    "BLOB")    \
X(xltypeNil,     SQLITE_NULL,    "NULL")    \
X(xltypeErr,     SQLITE_NULL,    "NULL")    \

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
			if (xltype == x) return s;
		}
		return SQLITE_UNKNOWN;
	}

	// Extended SQLite type to Excel type.
	inline constexpr int xltype(int sqltype)
	{
		for (auto [x, s] : xl_sql_type) {
			if (sqltype == s) return x;
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
	inline bool possibly_num_date(const OPER& x)
	{
		static const double _1970 = 25569;
		//static const double _3000 = 401769;
		//static const double _2038 = 50424; // 2038/1/19
		static const double _2123 = 81723; // 2123/9/30

		return isNum(x) and _1970 <= x.val.num and x.val.num <= _2123;
	}

	inline bool is_str_date(const OPER& x, tm* ptm)
	{
		if (!isStr(x)) {
			return false;
		}

		auto vdt = fms::view(x.val.str+1,x.val.str[0]);
		if (vdt.len < 8) {
			return false;
		}

		return fms::parse_tm(vdt, ptm);
	}
#ifdef _DEBUG
	inline int test_is_str_date() {
		try {
			tm tm;
			ensure(is_str_date(OPER("1970-1-1"), &tm));
			ensure(is_str_date(OPER("1970/1/1"), &tm));
			ensure(!is_str_date(OPER("1970-1/1"), &tm));
			ensure(!is_str_date(OPER("1970/1-1"), &tm));
			ensure(is_str_date(OPER("1970/1/1"), &tm));
			ensure(is_str_date(OPER("1970-1-1T00:00:00.0"), &tm));
			ensure(is_str_date(OPER("1970-1-1 00:00:00"), &tm));
			ensure(is_str_date(OPER("1970-1-1 00:00:00.000"), &tm));
			ensure(is_str_date(OPER("1970-1-1 00:00:00.000000"), &tm));
			ensure(is_str_date(OPER("1970-1-1 00:00:00.000000000"), &tm));
			ensure(is_str_date(OPER("1970-1-1 00:00:00.0"), &tm));
			ensure(!is_str_date(OPER("1-1-1"), &tm));
		}
		catch (const std::exception& ex) {
			XLL_ERROR(ex.what());

			return FALSE;
		}

		return TRUE;
	}
#endif // _DEBUG

	// bool < int < float < text
	inline int guess_one_sqltype(const OPER& x)
	{
		ensure(size(x) <= 1);

		if (isNum(x)) {
			if (possibly_num_date(x)) {
				return SQLITE_DATETIME;
			}
			else if (x.val.num == std::floor(x.val.num) && std::fabs(x.val.num) < std::pow(2.,31)) {
				return SQLITE_INTEGER;
			}
			else {
				return SQLITE_FLOAT;
			}
		}

		if (isBool(x)) return SQLITE_BOOLEAN;

		if (isStr(x)) {
			if (x.val.str[0] == 0) {
				return SQLITE_NULL;
			}
			if (x.val.str[0] == 1 && strchr("YyNnTtFf", x.val.str[1])) {
				return SQLITE_BOOLEAN;
			}
			struct tm tm;
			if (is_str_date(x, &tm)) {
				return SQLITE_DATETIME;
			}
			else {
				return SQLITE_TEXT;
			}
		}

		if (!x) return SQLITE_NULL;

		return sqltype(type(x));
	}
#ifdef _DEBUG
	inline int test_guess_one_sqlite_type()
	{
		try {
			ensure(SQLITE_FLOAT == guess_one_sqltype(OPER(1.23)));
			ensure(SQLITE_FLOAT == guess_one_sqltype(OPER(std::pow(2,31))));
			ensure(SQLITE_INTEGER == guess_one_sqltype(OPER(123)));
			ensure(SQLITE_INTEGER == guess_one_sqltype(OPER(std::pow(2, 31)-1)));
			ensure(SQLITE_BOOLEAN == guess_one_sqltype(OPER(true)));
			ensure(SQLITE_NULL == guess_one_sqltype(OPER()));
			ensure(SQLITE_NULL == guess_one_sqltype(OPER("")));
			ensure(SQLITE_TEXT == guess_one_sqltype(OPER(" ")));
			ensure(SQLITE_DATETIME == guess_one_sqltype(OPER("1970-1-1")));
			ensure(SQLITE_NULL == guess_one_sqltype((OPER)ErrNA));
		}
		catch (const std::exception& ex) {
			XLL_ERROR(ex.what());

			return FALSE;
		}

		return TRUE;
	}
#endif // _DEBUG
	inline int guess_sqltype(const OPER& x, int col, int rows = -1, int off = 0)
	{
		ensure(col < columns(x));

		std::set<int> types;
		if (rows == -1) {
			rows = xll::rows(x);
		}
		for (int i = off; i < rows; ++i) {
			auto ti = guess_one_sqltype(x(i, col));
			ensure(ti != SQLITE_UNKNOWN);
			if (ti != SQLITE_NULL) {
				types.insert(ti);
			}
		}

		if (types.size() == 0) {
			return SQLITE_NULL;
		}
		if (types.size() > 1) {
			if (types.contains(SQLITE_TEXT)) {
				return SQLITE_TEXT;
			}
			if (types.contains(SQLITE_FLOAT)) {
				return SQLITE_FLOAT;
			}
			if (types.contains(SQLITE_INTEGER)) {
				return SQLITE_INTEGER;
			}
			if (types.contains(SQLITE_BOOLEAN)) {
				return SQLITE_BOOLEAN;
			}
		}

		return *types.begin();
	}

	// 1-based
	inline int bind_parameter_index(sqlite3_stmt* stmt, const OPER& o)
	{
		int i = 0; // not found

		if (isNum(o)) {
			i = (int)asNum(o);
		}
		else if (isStr(o)) {
			i = sqlite3_bind_parameter_index(stmt, to_string(o).c_str());
		}
		else {
			ensure(!__FUNCTION__ ": index must be integer or string");
		}

		return i;
	}

	// Convert Excel type to double.
	inline double to_float(const OPER& x)
	{
		return isNum(x) ? asNum(x) : asNum(Excel(xlfValue, x));	
	}

	// Convert Excel type to SQLite type.
	inline long to_int(const OPER& x)
	{
		return static_cast<long>(isInt(x) ? asNum(x) : asNum(Excel(xlfValue, x)));
	}

	inline bool is_null(const OPER& x)
	{
		return isMissing(x) || isNil(x) || isErr(x);
	}

	// Bind OPER to 1-based SQLite statement column j based on sqlite extended type tj.
	inline void bind(sqlite::stmt& stmt, int j, const OPER& x, int tj = 0)
	{
		if (is_null(x)) {
			stmt.bind(j); // NULL
		
			return;
		}

		if (!tj) {
			tj = stmt.sqltype(j); /// extended SQLite type
		}

		// Special cases.
		if (tj == SQLITE_DATETIME) {
			if (isNum(x)) {
				if (x == 0) {
					stmt.bind(j);
				}
				else if (possibly_num_date(x)) {
					stmt.bind(j, to_time_t(x.val.num));
				}
				else {
					stmt.bind(j, asNum(x)); // SQLITE_FLOAT Gregorian?
				}
			}
			else if (isStr(x)) {
				auto sv = fms::view(x.val.str + 1, x.val.str[0]);
				struct tm tm;
				if (x == "") {
					stmt.bind(j);
				}
				else if (fms::parse_tm(sv, &tm)) {
					stmt.bind(j, _mkgmtime(&tm));
				}
				else {
					ensure (!__FUNCTION__ ": invalid date string: ");
				}
			}
			else {
				ensure(!__FUNCTION__ ": date must be number or string");
			}
		}
		else if (tj == SQLITE_BOOLEAN) {
			bool b;
			if (isStr(x)) {
				ensure(x.val.str[0] == 1);
				ensure(strchr("YyTtNnFf", x.val.str[1]));
				b = (0 != strchr("YyTt", x.val.str[1]));
			}
			else {
				b = asNum(x) != 0;
			}
			stmt.bind(j, b);
		}
		else { // flexible SQLite type
			switch (type(x)) {
			case xltypeNum:
				stmt.bind(j, asNum(x));
				break;
			case xltypeInt:
			case xltypeBool:
				stmt.bind(j, (int)asNum(x));
				break;
			case xltypeStr:
				stmt.bind(j, to_string(x));
				break;
			default:
				ensure(!__FUNCTION__ ": invalid type");
			}
		}
	}

	// Bind keys to values. 
	inline void sqlite_bind(sqlite::stmt& stmt, const OPER& key, const OPER& val)
	{
		size_t n = size(key);
		ensure(size(val) == n);

		for (int i = 0; i < n; ++i) {
			int pi = 1 + i; // bind position
			if (isStr(key[i])) {
				const auto ki = key[i].to_string();
				pi = stmt.bind_parameter_index(ki.c_str());
				if (!pi) {
					XLL_ERROR((std::string(__FUNCTION__) + ": parameter index not found: " + ki).c_str());
				}
			}

			bind(stmt, pi, val[i]);
		}
	}

	// Convert SQLite value to OPER.
	inline auto as_oper(const sqlite::value& v)
	{
		switch (v.sqltype()) {
		case SQLITE_INTEGER:
			return OPER(v.as_int());
		case SQLITE_FLOAT:
			return OPER(v.as_float());
		case SQLITE_TEXT: {
			const auto str = v.as_text();
			return OPER(str.data(), (int)str.size());
		}
		//case SQLITE_BLOB:
		case SQLITE_BOOLEAN:
			return OPER(v.as_boolean());
		case SQLITE_DATETIME: {
			auto dt = v.as_datetime();
			if (SQLITE_TEXT == dt.type) {
				fms::view vdt(dt.value.t);
				if (vdt.len == 0) {
					return OPER("");
				}
				struct tm tm;
				if (fms::parse_tm(vdt, &tm)) {
					return OPER(to_excel(_mkgmtime(&tm)));
				}
				else {
					return OPER(to_excel(dt.value.f));
				}
			}
			else if (SQLITE_INTEGER == dt.type) {
				return OPER(to_excel(dt.value.i));
			}
			else if (SQLITE_FLOAT == dt.type) {
				return OPER(to_excel(dt.value.f));
			}
			else {
				return OPER(OPER::Err::NA);
			}
			break;
		}
		case SQLITE_NULL:
			return OPER("");
		default:
			return OPER(OPER::Err::NA);
		}
	}

	template<class O, class X = O::value_type>
	inline auto headers(sqlite::stmt& stmt, O& o)
	{
		for (int i = 0; i < stmt.column_count(); ++i) {
			o.push_back(OPER(stmt[i].name()));
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
		const OPER& o;
		unsigned row;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = X;

		iterable(const OPER& o, unsigned row = 0)
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
		//const OPER& x{ _x };
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

	/* !!!
	template<class X>
	inline void bind(sqlite3_stmt* stmt, int i, const OPER& o, int type = 0)
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
