// fms_sqlite.h
#pragma once
#ifdef _DEBUG
#include <cassert>
#endif 
#include <map>
#include <string_view>
#include <stdexcept>
#include <vector>
#define SQLITE_ENABLE_NORMALIZE
#include "sqlite-amalgamation-3390400/sqlite3.h"
#include <format>

// call OP and throw on error
#define FMS_SQLITE_OK(DB, OP) { int status = OP; if (SQLITE_OK != status) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
// type, name
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

#define SQLITE_NAME(a,b) {a, b},
inline const std::map<int, const char*> sqlite_type_name = {
	SQLITE_TYPE_ENUM(SQLITE_NAME)
};
#undef SQLITE_NAME

#define SQLITE_TYPE(a,b) {b, a},
inline const std::map<std::string, int> sqlite_name_type = {
	SQLITE_TYPE_ENUM(SQLITE_TYPE)
};
#undef SQLITE_TYPE

// extended types
#define SQLITE_UNKNOWN 0
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3

// permitted types in CREATE TABLE
// SQL name, affinity, type, category
#define SQLITE_DECLTYPE(X) \
X("INT", "INTEGER", SQLITE_INTEGER, 1) \
X("INTEGER", "INTEGER", SQLITE_INTEGER, 1) \
X("TINYINT", "INTEGER", SQLITE_INTEGER, 1) \
X("SMALLINT", "INTEGER", SQLITE_INTEGER, 1) \
X("MEDIUMINT", "INTEGER", SQLITE_INTEGER, 1) \
X("BIGINT", "INTEGER", SQLITE_INTEGER, 1) \
X("UNSIGNED BIG INT", "INTEGER", SQLITE_INTEGER, 1) \
X("INT2", "INTEGER", SQLITE_INTEGER, 1) \
X("INT8", "INTEGER", SQLITE_INTEGER, 1) \
X("CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("VARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("VARYING CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NCHAR", "TEXT", SQLITE_TEXT, 2) \
X("NATIVE CHARACTER", "TEXT", SQLITE_TEXT, 2) \
X("NVARCHAR", "TEXT", SQLITE_TEXT, 2) \
X("TEXT", "TEXT", SQLITE_TEXT, 2) \
X("CLOB", "TEXT", SQLITE_TEXT, 2) \
X("BLOB", "BLOB", SQLITE_BLOB, 3) \
X("REAL", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE PRECISION", "REAL", SQLITE_FLOAT, 4) \
X("FLOAT", "REAL", SQLITE_FLOAT, 4) \
X("NUMERIC", "NUMERIC", SQLITE_FLOAT, 5) \
X("DECIMAL", "NUMERIC", SQLITE_FLOAT, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_BOOLEAN, 5) \
X("DATE", "NUMERIC", SQLITE_DATETIME, 5) \
X("DATETIME", "NUMERIC", SQLITE_DATETIME, 5) \

#define SQLITE_DECL(a,b,c,d) {a, c},
inline const std::map<std::string_view, int> sqlite_decltype_map = {
	SQLITE_DECLTYPE(SQLITE_DECL)
};
#undef SQLITE_DECL

inline int sqlite_type(std::string_view type)
{
	if (0 == type.size()) {
		return SQLITE_TEXT; // sqlite default type
	}

	// ignore size info not used by sqlite
	if (size_t pos = type.find('('); pos != std::string_view::npos) {
		type = type.substr(0, pos);
	}

	const auto& t = sqlite_decltype_map.find(type);
	if (t == sqlite_decltype_map.end()) {
		throw std::runtime_error(__FUNCTION__ ": unknown SQL type");
	}

	return t->second;
}

namespace sqlite {

	// https://sqlite.org/datatype3.html#determination_of_column_affinity"
	inline int affinity(const std::string_view& decl)
	{
		if (decl.contains("INT")) {
			return SQLITE_INTEGER;
		}
		if (decl.contains("CHAR") or decl.contains("CLOB") or decl.contains("TEXT")) {
			return SQLITE_TEXT;
		}
		if (decl.contains("BLOB") or decl.size() == 0) {
			return SQLITE_BLOB;
		}
		if (decl.contains("REAL") or decl.contains("FLOA") or decl.contains("DOUB")) {
			return SQLITE_FLOAT;
		}

		return SQLITE_NUMERIC;
	}

	// sqlite datetime
	struct datetime {
		union {
			double f;
			time_t i;
			const unsigned char* t;
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;
	};

	class string {
		const char* str;
	public:
		string()
			: str{nullptr}
		{ }
		string(const char* str)
			: str{ str }
		{ }
		string(const string&) = delete;
		string& operator=(const string&) = delete;
		string(string&& _str) noexcept
		{
			*this = std::move(_str);
		}
		string& operator=(string&& _str) noexcept
		{
			if (this != &_str) {
				str = std::exchange(_str.str, nullptr);
			}

			return *this;
		}
		~string()
		{
			if (str) {
				sqlite3_free((void*)str);
			}
		}

		operator const char* () const
		{
			return str;
		}

	};

	// open/close a sqlite3 database
	class db {
		sqlite3* pdb;
	public:
		// db("") for in-memory database
		db(const char* filename, int flags = 0)
			: pdb(nullptr)
		{
			if (!flags) {
				flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
			}
			if (!filename || !*filename) {
				flags |= SQLITE_OPEN_MEMORY;
			}

			FMS_SQLITE_OK(pdb, sqlite3_open_v2(filename, &pdb, flags, nullptr));
		}
		db(const db&) = delete;
		db& operator=(const db&) = delete;
		~db()
		{
			sqlite3_close(pdb);
		}

		operator sqlite3* ()
		{
			return pdb;
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(pdb);
		}

	};

	// https://www.sqlite.org/lang.html
	class stmt {
		sqlite3* pdb;
		sqlite3_stmt* pstmt;
	public:
		stmt(sqlite3* pdb)
			: pdb{ pdb }, pstmt{ nullptr }
		{
			if (!pdb) {
				throw std::runtime_error(__FUNCTION__ ": database handle must not be null");
			}
		}
		stmt(const stmt&) = delete;
		stmt& operator=(const stmt&) = delete;
		stmt(stmt&& _stmt) noexcept
		{
			*this = std::move(_stmt);
		}
		stmt& operator=(stmt&& _stmt) noexcept
		{
			if (this != &_stmt) {
				pdb = std::exchange(_stmt.pdb, nullptr);
				pstmt = std::exchange(_stmt.pstmt, nullptr);
			}

			return *this;
		}
		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
		}

		// for use in native sqlite3 C functions
		operator sqlite3_stmt* ()
		{
			return pstmt;
		}

		sqlite3* db_handle() const
		{
			return pstmt ? sqlite3_db_handle(pstmt) : nullptr;
		}

		const char* errmsg() const
		{
			return sqlite3_errmsg(pdb);
		}

		// https://www.sqlite.org/c3ref/expanded_sql.html
		const char* sql() const
		{
			return sqlite3_sql(pstmt);
		}
		const char* expanded_sql() const
		{
			return sqlite3_expanded_sql(pstmt);
		}
		string normalized_sql() const
		{
			return string(sqlite3_normalized_sql(pstmt));
		}

		// https://www.sqlite.org/c3ref/prepare.html
		int prepare(const std::string_view& sql)
		{
			int ret = sqlite3_prepare_v2(pdb, sql.data(), (int)sql.size(), &pstmt, 0);
			FMS_SQLITE_OK(pdb, ret);

			return ret;
		}
		// int ret = step();  while (ret == SQLITE_ROW) { ...; ret = step()) { }
		// if (!SQLITE_DONE) then error
		int step()
		{
			return sqlite3_step(pstmt);
		}

		// start iteration over result rows
		// stmt.prepare(); iterable i(stmt); while (i) { const stmt& stmt = *i; ...; ++i; }
		class iterable {
			sqlite3* pdb;
			sqlite3_stmt* pstmt;
			int ret;
		public:
			// assumes _stmt lifetime
			iterable(stmt& _stmt)
				: pdb(_stmt.db_handle()), pstmt{ _stmt }
			{
				ret = SQLITE_ROW;
				operator++();
			}
			explicit operator bool() const noexcept
			{
				return SQLITE_ROW == ret;
			}
			const stmt& operator*() noexcept
			{
				return *(const stmt*)this;
			}
			iterable& operator++()
			{
				if (SQLITE_ROW == ret) {
					ret = sqlite3_step(pstmt);
					if (!(SQLITE_ROW == ret or SQLITE_DONE == ret)) {
						throw std::runtime_error(sqlite3_errmsg(pdb));
					}
				}

				return *this;
			}
		};

		// reset a prepared statement
		int reset()
		{
			return sqlite3_reset(pstmt);
		}

		// clear any existing bindings
		int clear_bindings()
		{
			return sqlite3_clear_bindings(pstmt);
		}

		//
		// 1-based binding
		// 

		// null
		stmt& bind(int i)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_null(pstmt, i));

			return *this;
		}

		stmt& bind(int i, double d)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_double(pstmt, i, d));

			return *this;
		}

		// int
		stmt& bind(int i, int j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int(pstmt, i, j));

			return *this;
		}
		// int64
		stmt& bind(int i, int64_t j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}

		// text, make copy by default
		stmt& bind(int i, const std::string_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text(pstmt, i, str.data(),
				static_cast<int>(str.size()), cb));

			return *this;
		}
		// text16 with length in characters
		stmt& bind(int i, const std::wstring_view& str, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text16(pstmt, i, (const void*)str.data(),
				static_cast<int>(2 * str.size()), cb));

			return *this;
		}

		// datetime
		stmt& bind(int i, const datetime& dt)
		{
			switch (dt.type) {
			case SQLITE_FLOAT:
				bind(i, dt.value.f);
				break;
			case SQLITE_INTEGER:
				bind(i, dt.value.i);
				break;
			case SQLITE_TEXT:
				// cast from unsigned char
				bind(i, (const char*)dt.value.t);
				break;
			}

			return *this;
		}

		// https://www.sqlite.org/c3ref/bind_parameter_index.html
		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		// number of columns returned
		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}

		//
		// 0-base column data
		// 
		
		// fundamental sqlite type
		int column_type(int i) const
		{
			return sqlite3_column_type(pstmt, i);
		}
		// type specified in CREATE TABLE
		const char* column_decltype(int i) const
		{
			return sqlite3_column_decltype(pstmt, i);
		}
		// extended SQLITE_ type
		int sqltype(int i) const
		{
			const char* ctype = column_decltype(i);

			return ::sqlite_type(ctype);
		}

		// bytes, not chars
		int column_bytes(int i) const
		{
			return sqlite3_column_bytes(pstmt, i);
		}
		int column_bytes16(int i) const
		{
			return sqlite3_column_bytes16(pstmt, i);
		}

		const char* column_name(int i) const
		{
			return sqlite3_column_name(pstmt, i);
		}
		const wchar_t* column_name16(int i) const
		{
			// cast from void*
			return (const wchar_t*)sqlite3_column_name16(pstmt, i);
		}

		// 
		// Get 0-base column types and values.
		//

		bool is_null(int i) const
		{
			return SQLITE_NULL == column_type(i);
		}

		bool is_boolean(int i) const
		{
			return SQLITE_BOOLEAN == sqltype(i);
		}
		bool as_boolean(int i) const
		{
			return 0 != as_int(i);
		}

		bool is_int(int i) const
		{
			return SQLITE_INTEGER == sqltype(i);
		}
		int as_int(int i) const
		{
			return sqlite3_column_int(pstmt, i);
		}
		// bool is_int64???
		sqlite3_int64 as_int64(int i) const
		{
			return sqlite3_column_int64(pstmt, i);
		}

		// use sqlite type name
		bool is_float(int i) const
		{
			return SQLITE_FLOAT == sqltype(i);
		}
		double as_float(int i) const
		{
			return sqlite3_column_double(pstmt, i);
		}

		bool is_text(int i) const
		{
			return SQLITE_TEXT == sqltype(i);
		}
		const std::string_view as_text(int i) const
		{
			const char* str = (const char*)sqlite3_column_text(pstmt, i);
			int len = column_bytes(i);

			return std::string_view(str, len);
		}
		const std::wstring_view as_text16(int i) const
		{
			const wchar_t* str = (const wchar_t*)sqlite3_column_text16(pstmt, i);
			int len = column_bytes16(i);

			return std::wstring_view(str, len);
		}

		bool is_blob(int i) const
		{
			return SQLITE_BLOB == sqltype(i);
		}
		const std::basic_string_view<std::byte> as_blob(int i) const
		{
			const std::byte* blob = (std::byte*)sqlite3_column_blob(pstmt, i);
			int size = column_bytes(i);

			return std::basic_string_view<std::byte>(blob, size);
		}

		bool is_datetime(int i) const
		{
			return SQLITE_DATETIME == sqltype(i);
		}
		datetime as_datetime(int i) const
		{
			switch (column_type(i)) {
			case SQLITE_FLOAT:
				return datetime{ .value = {.f = as_float(i)}, .type = SQLITE_FLOAT };
			case SQLITE_INTEGER:
				return datetime{ .value = {.i = as_int64(i)}, .type = SQLITE_INTEGER };
			case SQLITE_TEXT:
				return datetime{ .value = {.t = sqlite3_column_text(pstmt, i)}, .type = SQLITE_TEXT };
			}

			return datetime{ .value = {.t = nullptr}, .type = SQLITE_UNKNOWN };
		}

		// internal representation
		sqlite3_value* as_value(int i)
		{
			return sqlite3_column_value(pstmt, i);
		}
#ifdef _DEBUG
		static int test()
		{
			int ret = SQLITE_OK;
			//const char* s = nullptr;
			
			try {
				db db("../tmp.db");
				stmt stmt(db);

				std::string_view sql("DROP TABLE IF EXISTS a");
				ret = stmt.prepare(sql);
				stmt.step();
				stmt.reset();

				sql = "CREATE TABLE a (b INT, c REAL, d TEXT, e DATETIME)";
				ret = stmt.prepare(sql);
				assert(sql == stmt.sql());				
				assert(SQLITE_DONE == stmt.step());
				assert(0 == stmt.column_count());

				stmt.reset();
				ret = stmt.prepare("INSERT INTO a VALUES (123, 1.23, 'foo', '2023-4-5')");
				ret = stmt.step();
				assert(SQLITE_DONE == ret);
				
				stmt.reset();
				stmt.prepare("SELECT * FROM a");
				int rows = 0;
				stmt::iterable i(stmt);
				while (i) {
					const sqlite::stmt& si = *i;
					assert(4 == si.column_count());
					assert(si.is_int(0));
					assert(123 == si.as_int(0));
					assert(si.is_float(1));
					assert(1.23 == si.as_float(1));
					assert(si.is_text(2));
					assert(si.as_text(2) == "foo");
					assert(si.is_datetime(3));
					auto dt = si.as_datetime(3);
					assert(SQLITE_TEXT == dt.type);
					assert(std::string("2023-4-5") == (char*)dt.value.t);
					++rows;
					++i;
				}
				assert(1 == rows);

				stmt.reset();
				stmt.prepare("SELECT e FROM a");
				stmt.step();
				auto dtv = stmt.as_value(0);
				auto dtt = sqlite3_value_type(dtv);
				auto dtct = stmt.sqltype(0);

				stmt.reset();
				ret = stmt.prepare("INSERT INTO a VALUES (?, ?, ?, ?)");
				int b = 2;
				double c = 2.34;
				char d[2] = { 'a', 0};
				time_t e = 1234567;
				for (int j = 0; j < 3; ++j) {
					stmt.reset();
					// 1-based
					stmt.bind(1, b);
					stmt.bind(2, c);
					stmt.bind(3, d);
					stmt.bind(4, e);
					stmt.step();

					++b;
					c += 0.01;
					d[0] = ++d[0];
					e += 86400;
				}

				stmt.reset();
				stmt.prepare("select count(*) from a");
				assert(SQLITE_ROW == stmt.step());
				assert(1 == stmt.column_count());
				assert(4 == stmt.as_int(0));
				assert(SQLITE_DONE == stmt.step());

			}
			catch (const std::exception& ex) {
				puts(ex.what());
			}

			return 0;
		}
#endif // _DEBUG
	};

	// scratch database for a single sqlite3_value
	// https://www.sqlite.org/c3ref/value_blob.html
	class value {
		static sqlite3* db()
		{
			static sqlite::db db("");
			static bool init = true;
			
			if (init) {
				sqlite::stmt stmt(db);
				stmt.prepare("CREATE TABLE IF NOT EXISTS v (value)");
				stmt.step();

				init = false;
			}

			return db;
		}
		mutable sqlite::stmt stmt;
		sqlite3_int64 rowid;
	public:
		value()
			: stmt(db()), rowid(0)
		{ }
		template<class T>
		value(const T& t)
			: value()
		{
			stmt.reset();
			stmt.prepare("INSERT INTO v VALUES (?)");
			stmt.bind(1, t);
			stmt.step();

			rowid = sqlite3_last_insert_rowid(db());
		}
		value(const value&) = delete;
		value& operator=(const value&) = delete;
		~value()
		{
			stmt.reset();
			stmt.prepare("DELETE FROM v WHERE rowid = ?");
			stmt.bind(1, rowid);
			stmt.step();
		}

		static int count()
		{
			sqlite::stmt stmt(db());

			stmt.prepare("SELECT COUNT(*) FROM v");
			stmt.step();

			return stmt.as_int(0);
		}


		// Internal fundamental sqlite type.
		int type() const
		{
			return sqlite3_value_type(select());
		}
		// Best numeric datatype of the value.
		int numeric_type() const
		{
			return sqlite3_value_numeric_type(select());
		}

		sqlite3_value* select() const
		{
			stmt.reset();
			stmt.prepare("SELECT value FROM v WHERE rowid = ?");
			stmt.bind(1, rowid);
			stmt.step();

			return stmt.as_value(0);
		}

		int bytes() const
		{
			return sqlite3_value_bytes(select());
		}
		int bytes16() const
		{
			return sqlite3_value_bytes16(select());
		}

		const void* as_blob() const
		{
			return sqlite3_value_blob(select());
		}
		const double as_double() const
		{
			return sqlite3_value_double(select());
		}
		const int as_int() const
		{
			return sqlite3_value_int(select());
		}
		const sqlite3_int64 as_int64() const
		{
			return sqlite3_value_int64(select());
		}
		const unsigned char* as_text() const
		{
			return sqlite3_value_text(select());
		}
		const void* as_text16() const
		{
			return sqlite3_value_text16(select());
		}

		// CAST (v AS type)
		sqlite3_value* cast(const char* type) const
		{
			stmt.reset();
			stmt.prepare("SELECT CAST (value AS ?) FROM v WHERE rowid = ?");
			stmt.bind(1, type);
			stmt.bind(2, rowid);
			auto sql = stmt.expanded_sql();
			stmt.step();

			return stmt.as_value(0);
		}
#ifdef _DEBUG
		static int test()
		{
			try {
				{
					value v(1.23);
					assert(SQLITE_FLOAT == v.type());
					assert(1.23 == v.as_double());
				}
				{
					value v(123);
					assert(SQLITE_INTEGER == v.type());
					assert(123 == v.as_int());
				}
				{
					value v("string");
					assert(SQLITE_TEXT == v.type());
					assert(0 == strcmp("string", (const char*)v.as_text()));
				}
			}
			catch (const std::exception& ex) {
				puts(ex.what());
			}

			return 0;
		}
#endif // _DEBUG
	};

	inline std::string quote(const std::string_view& s, char l, char r = 0)
	{
		std::string t(s);

		r = r ? r : l;

		if (!s.starts_with(l)) {
			t.insert(t.begin(), l);
		}
		if (!s.ends_with(r)) {
			t.insert(t.end(), r);
		}

		return t;
	}
	inline std::string table_name(const std::string_view& t)
	{
		return quote(t, '[', ']');
	}
	inline std::string variable_name(const std::string_view& v)
	{
		return quote(v, '\'');
	}

	template<class C>
	//	requires requires (C c) {
	//	c.push_back(); }
	inline auto copy(stmt& _stmt, C& c)
	{
		stmt::iterable i(_stmt);

		int col = _stmt.column_count();
		while (i) {
			const stmt& si = *i;
			
			// cache types
			std::vector<int> type(col);
			for (int j = 0; j < col; ++j) {
				type[j] = (*i).sqltype(j);
			}
			
			for (int j = 0; j < col; ++j) {
				switch (type[j]) {
				case SQLITE_NULL:
					c.push_back();
					break;
				case SQLITE_FLOAT:
					c.push_back(si.as_float(j));
					break;
				case SQLITE_BOOLEAN:
					c.push_back(si.as_boolean(j));
					break;
				case SQLITE_TEXT:
					c.push_back(si.as_text(j));
					break;
				case SQLITE_INTEGER:
					c.push_back(si.as_int(j));
					break;
				case SQLITE_DATETIME:
					auto dt = si.as_datetime(j);
					if (type[j] != SQLITE_INTEGER) {
						throw std::runtime_error(__FUNCTION__ ": datetime not stored as time_t");
					}
					c.push_back(dt.value.i);
					break;
					/*
				case SQLITE_BLOB:
					c.push_back(si.as_blob(j));
					break;
					*/
				default:
					c.push_back(); // null
				}
			}
			++i;
		}
		//c.resize(c.size() / col, col);
	}
#if 0
	// V is a variant type with free functions 
	// `bool is_xxx(const V&)` and `xxx as_xxx(const V&)`
	// for xxx in `blob`, `double`, `int`, `int64`, `null`, `text`, `text16`, and `datetime` 
	template<class V>
	struct table_info {

		std::vector<std::string> name;
		std::vector<std::string> type;
		std::vector<int> sqltype; // SQLITE_XXX type
		std::vector<int> notnull;
		std::vector<std::string> dflt_value;
		std::vector<int> pk;

		table_info()
		{ }
		table_info(size_t n)
			: name(n), type(n), notnull(n), dflt_value(n), pk(n)
		{ }
		table_info(sqlite3* db, const std::string_view& table)
		{
			sqlite::stmt stmt(db);
			auto query = std::string("PRAGMA table_info(") + table_name(table) + ");";

			stmt.prepare(query.c_str());

			while (SQLITE_ROW == stmt.step()) {
				push_back(stmt.column_text(1), 
					stmt.column_text(2), 
					stmt.column_int(3),
					stmt.column_text(4),stmt.column_int(5));
			}
		}

		table_info& push_back(
			const std::string_view& _name,
			const std::string_view& _type,
			const int _notnull = 0,
			const std::string_view& _dflt_value = "",
			const int _pk = 0)
		{
			name.emplace_back(std::string(_name));
			type.emplace_back(std::string(_type));
			notnull.push_back(_notnull);
			dflt_value.emplace_back(std::string(_dflt_value));
			pk.push_back(_pk);
			sqltype.push_back(sqlite_type(_type));

			return *this;
		}

		size_t size() const
		{
			return name.size();
		}

		int parameter_index(const char* key) const
		{
			for (int i = 0; i < size(); ++i) {
				if (name[i] == key) {
					return i + 1;
				}
			}

			return 0;
		}

		// "(name type [modifiers], ...)"
		std::string schema() const
		{
			std::string sql("(");
			std::string comma("");
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append(name[i]);
				sql.append(" ");
				sql.append(type[i]);
				if (pk[i]) {
					sql.append(" PRIMARY KEY");
				}
				if (notnull[i]) {
					sql.append(" NOT NULL");
				}
				if (dflt_value[i] != "") {
					sql.append(" DEFAULT ('");
					sql.append(dflt_value[i]);
					sql.append("')");
				}
			}
			sql.append(")");

			return sql;
		}

		int drop_table(sqlite3* db, const std::string_view& table)
		{
			auto sql = std::string("DROP TABLE IF EXISTS ") + table_name(table);

			return sqlite3_exec(db, sql.data(), nullptr, nullptr, nullptr);
		}

		int create_table(sqlite3* db, const std::string_view& table)
		{
			drop_table(db, table);

			auto sql = std::string("CREATE TABLE ")
				+ table_name(table)
				+ schema();

			return sqlite3_exec(db, sql.data(), nullptr, nullptr, nullptr);
		}

		// prepare a statement for inserting values
		// stmt = "INSERT INTO table(key, ...) VALUES (@key, ...)"
		// for (row : rows) {
		//   for ([k, v]: row) {
		//     bind(stmt, k, v);
		//   }
		//   stmt.step();
		//   stmt.reset();
		// }
		sqlite::stmt insert_into(sqlite3* db, const std::string_view& table) const
		{
			sqlite::stmt stmt(db);

			auto sql = std::string(" INSERT INTO ") + table_name(table) + " (";
			std::string comma("");
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append(name[i]);
			}
			sql.append(")");
			sql.append(" VALUES (");
			comma = "";
			for (size_t i = 0; i < size(); ++i) {
				sql.append(comma);
				comma = ", ";
				sql.append("@");
				sql.append(name[i]);
			}
			sql.append(")");

			stmt.prepare(sql);

			return stmt;
		}

		template<class C, class X = typename C::value_type>
		int insert_values(stmt& stmt, C& cursor)
		{
			int count = 0;

			while (cursor) {
				auto row = *cursor;
				for (unsigned j = 0; row; ++j) {
					switch (sqltype[j]) {
					case SQLITE_NULL:
						stmt.bind(j + 1);
						break;
					case SQLITE_TEXT:
						stmt.bind(j + 1, as_text<X>(*row));
						break;
					case SQLITE_FLOAT:
					case SQLITE_NUMERIC:
						stmt.bind(j + 1, as_double<X>(*row));
						break;
					case SQLITE_INTEGER:
					case SQLITE_BOOLEAN:
						stmt.bind(j + 1, as_int<X>(*row));
						break;
					case SQLITE_DATETIME:
						stmt.bind(j + 1, as_datetime<X>(*row));
						break;
					}
					++row;
					++count;
				}
				stmt.step();
				stmt.reset();
				++cursor;
			}

			return count;
		}

		template<class C, class O>
		int exec(sqlite3_stmt* stmt, C& cursor, O& o, bool no_headers = false)
		{
			using X = typename O::value_type;

			while (cursor) {
				auto row = *cursor;
				for (unsigned j = 0; row; ++j) {
					switch (sqltype[j]) {
					case SQLITE_NULL:
						o.push_back(X{});
						break;
					case SQLITE_TEXT:
						o.push_back(stmt.column_text(j));
						break;
					case SQLITE_FLOAT:
						o.push_back(stmt.column_double(j));
						break;
					case SQLITE_INTEGER:
						o.push_back(stmt.column_int(j));
						break;
					}
					++row;
				}
				stmt.step();
				stmt.reset();
				++cursor;
			}
		}

		int bind(sqlite3_stmt* stmt, int j, const V& v) const
		{
			if (SQLITE_TEXT == sqltype[j]) {
				const auto t = as_text(v);
				return sqlite3_bind_text(stmt, j + 1, t.data(), (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (SQLITE_TEXT16 == sqltype[j]) {
				const auto t = as_text16(v);
				return sqlite3_bind_text16(stmt, j + 1, t.data(), 2 * (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (SQLITE_FLOAT == sqltype[j]) {
				return sqlite3_bind_double(stmt, j + 1, as_double(v));
			}
			else if (SQLITE_INTEGER == sqltype[j]) {
				return sqlite3_bind_int(stmt, j + 1, as_int(v));
			}
			else if (SQLITE_INT64 == sqltype[j]) {
				return sqlite3_bind_int64(stmt, j + 1, as_int64(v));
			}
			else if (SQLITE_NULL == sqltype[j]) { // ???
				return sqlite3_bind_null(stmt, j + 1);
			}
			else if (SQLITE_BLOB == sqltype[j]) {
				const auto& b = as_blob(v);
				return sqlite3_bind_blob(stmt, j, b.data(), (int)b.size(), nullptr);
			}

			return SQLITE_MISUSE;
		}
		template<class V>
		int bind(sqlite3_stmt* stmt, const char* key, const V& v) const
		{
			int j = parameter_index(key);
			if (j == 0) {
				throw std::runtime_error(__FUNCTION__ ": key not found");
			}

			return bind(stmt, j, v);
		}
	};
#endif // 0
} // sqlite
