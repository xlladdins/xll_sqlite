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

// xltype to sqlite type
#define XLL_SQLITE_TYPE(X) \
X(xltypeInt,     SQLITE_INTEGER, "INTEGER") \
X(xltypeBool,    SQLITE_BOOLEAN, "BOOLEAN") \
X(xltypeNum,     SQLITE_FLOAT,   "FLOAT")   \
X(xltypeStr,     SQLITE_TEXT,    "TEXT")    \
X(xltypeBigData, SQLITE_BLOB,    "BLOB")    \
X(xltypeNil,     SQLITE_NULL,    "NULL")    \

namespace xll {

	// common arguments
	static const auto Arg_db   = Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database.");
	static const auto Arg_stmt = Arg(XLL_HANDLEX, "stmt", "is a handle to a sqlite statement.");
	static const auto Arg_sql  = Arg(XLL_LPOPER, "sql", "is a SQL query to execute.");
	static const auto Arg_bind = Arg(XLL_LPOPER4, "_bind", "is an optional array of values to bind.");
	static const auto Arg_nh   = Arg(XLL_BOOL, "no_headers", "is a optional boolean value indicating not to return headers. Default is FALSE.");

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
#if 0
	inline std::string to_chars(int i)
	{
		std::string str(10, 0);
		auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.length(), i);
		if (ec == std::errc{}) {
			str.resize(ptr - str.data());
		}
		else {
			throw std::runtime_error(std::make_error_code(ec).message());
		}

		return str;
	}

	template<class X>
	class cursor : public sqlite::cursor {
		unsigned row;
		const XOPER<X>& o;
		std::vector<sqlite::cursor::type> _type;
		std::vector<std::string> _name;
	public:
		cursor(const XOPER<X>& o, unsigned row = 0)
			: row{ row }, o{ o }, _type(o.columns()), _name(o.columns())
		{
			for (unsigned i = 0; i < o.columns(); ++i) {
				if (row == 0) {
					_type[i] = sqlite_type[o(0, i).type()];
					_name[i] = to_chars(i);
				}
				else if (row == 1) {
					_type[i] = sqlite_type[o(1, i).type()];
					_name[i] = o(0, i).to_string();
				}
				else {
					if (o(0, i) && o(0,i).is_str()) {
						_type[i] = sqlite::type(o(0, i).val.str + 1);
					}
					else {
						_type[i] = sqlite_type[o(1, i).type()];
					}
					_name[i] = o(1, i).to_string();
				}
			}
		}
		cursor(const cursor&) = delete;
		cursor& operator=(const cursor&) = delete;
		~cursor()
		{ }
		int _column_count() const override
		{
			return o.columns();
		}
		int _column_type(int i) const override
		{
			switch (o(row, i).type()) {
			case xltypeNum:
				return SQLITE_FLOAT;
			case xltypeStr:
				return SQLITE_TEXT;
			case xltypeInt:
				return SQLITE_INTEGER;
				/*
			case xltypeBigData:
				return SQLITE_BLOB;
				*/
			case xltypeNil:
				return SQLITE_NULL;
			default:
				ensure(!__FUNCTION__ ": unknown type");
			}

			return SQLITE_UNKNOWN;
		}
		const char* _column_name(int /*i*/) const override
		{
			return nullptr;
		}
		bool _done() const override
		{
			return row >= o.rows();
		}
		void _step() override
		{
			++row;
		}

		/*
		std::span<void*> _as_blob(int i) const override
		{
			auto blob = o(row, i).val.bigdata;
			void* p = blob.h.lpData;

			return std::span<void*>(p, p + blob.cbData);
		}
		*/
		double _as_double(int i) const override
		{
			return o(row, i).as_num();
		}
		int _as_int(int i) const override
		{
			return o(row, i).as_int();
		}
		std::string_view _as_text(int i) const override
		{
			const auto& str = o(row, i).val.str;

			if constexpr (std::is_same_v<traits<X>::xchar, char>) {
				return std::string_view(str + 1, str[0]);
			}
			else {
				return std::string_view{};
			}
		}
		std::wstring_view _as_text16(int i) const override
		{
			const auto& str = o(row, i).val.str;

			if constexpr (std::is_same_v<traits<X>::xchar, wchar_t>) {
				return std::wstring_view(str + 1, str[0]);
			}
			else {
				return std::wstring_view{};
			}
		}
	};
#endif // 0

	// time_t to Excel Julian date
	inline double to_excel(time_t t)
	{
		return static_cast<double>(25569. + t / 86400.);
	}
	// Excel Julian date to time_t
	inline time_t to_time_t(double d)
	{
		return static_cast<time_t>((d - 25569) * 86400);
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
	inline OPER column(const sqlite::stmt& stmt, int i)
	{
		switch (stmt.type(i)) {
		case SQLITE_NULL:
			return OPER("");
		case SQLITE_INTEGER:
			return OPER(stmt.column_int(i));
		case SQLITE_FLOAT:
		case SQLITE_NUMERIC:
			return OPER(stmt.column_double(i));
		case SQLITE_TEXT: {
			auto text = stmt.column_text(i);
			return OPER(text.data(), static_cast<char>(text.size()));
		}
		case SQLITE_BOOLEAN:
			return OPER(stmt.columns_boolean(i));
		case SQLITE_DATETIME:
			auto dt = stmt.column_datetime(i);
			if (SQLITE_TEXT == dt.type) {
				auto vt = stmt.column_text(i);
				if (vt.size() == 0)
					return OPER("");
				fms::view v(vt.data(), (int)vt.size());
				struct tm tm;
				if (fms::parse_tm(v, &tm)) {
					return OPER(to_excel(_mkgmtime(&tm)));
				}
				else {
					return ErrValue;
				}
			}
			if (SQLITE_INTEGER == dt.type) {
				time_t t = dt.value.i;
				if (t) {
					return OPER(to_excel(t));
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
			ensure(result.size() % c == 0);
			result.reshape(result.size() / c, c);
		}
		else {
			FMS_SQLITE_OK(sqlite3_db_handle(stmt), ret);
		}

		return result;
	}


} // namespace xll
