// fms_sqlite.h
#pragma once
#include <map>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>
#include "sqlite-amalgamation-3390400/sqlite3.h"

// call OP and throw on error
#define FMS_SQLITE_OK(DB, OP) { int status = OP; if (SQLITE_OK != status) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

// SQL, affinity, SQLITE, bind
#define SQLITE_DATA_TYPE(X) \
X("INT", _INTEGER, _INTEGER, _int) \
X("INTEGER", _INTEGER, _INTEGER, _int) \
X("TINYINT", _INTEGER, _INTEGER, _int) \
X("SMALLINT", _INTEGER, _INTEGER, _int) \
X("MEDIUMINT", _INTEGER, _INTEGER, _int) \
X("BIGINT", _INTEGER, _INTEGER, _int64) \
X("UNSIGNED BIG INT", _INTEGER, _INTEGER, _int64) \
X("INT2", _INTEGER, _INTEGER, _int) \
X("INT8", _INTEGER, _INTEGER, _int64) \
X("CHARACTER", _TEXT, _TEXT, _text) \
X("VARCHAR", _TEXT, _TEXT, _text) \
X("VARYING CHARACTER", _TEXT, _TEXT, _text) \
X("NCHAR", _TEXT, _TEXT, _text16) \
X("NATIVE CHARACTER", _TEXT, _TEXT, _text16) \
X("NVARCHAR", _TEXT, _TEXT, _text16) \
X("TEXT", _TEXT, _TEXT, _text) \
X("CLOB", _TEXT, _TEXT, _text) \
X("BLOB", _BLOB, _BLOB, _blob) \
X("REAL", _REAL, _FLOAT, _double) \
X("DOUBLE", _REAL, _FLOAT, _double) \
X("DOUBLE PRECISION", _REAL, _FLOAT, _double) \
X("FLOAT", _REAL, _FLOAT, _double) \
X("NUMERIC", _NUMERIC, _FLOAT, _double) \
X("DECIMAL", _NUMERIC, _FLOAT, _double) \
X("BOOLEAN", _NUMERIC, _INTEGER, _int) \
X("DATE", _NUMERIC, _INTEGER, _int64) \
X("DATETIME", _NUMERIC, _INTEGER, _int64) \

// DATE and DATETIME are 64-bit time_t

#define SQLITE_NAME(a,b) {a, b},
inline const std::map<int, const char*> sqlite_type_name = {
	SQLITE_TYPE_ENUM(SQLITE_NAME)
};
#undef SQLITE_NAME

#define SQLITE_TYPE(a,b) {b, a},
inline const std::map<std::string, int> sqlite_type_value = {
	SQLITE_TYPE_ENUM(SQLITE_TYPE)
};
#undef SQLITE_TYPE

// bind and retrieve sqlite data
#define SQL_DATA_TYPE(a,b,c,d) { a, SQLITE##c, sqlite3_bind##d, sqlite3_column##d }, 
inline const struct sqlite_datum {
	const char* sql; int type; void* bind; void* column;
} sqlite_data[] = {
	SQLITE_DATA_TYPE(SQL_DATA_TYPE)
};
#undef SQL_DATA_TYPE

inline sqlite_datum lookup_datum(const char* sql)
{
	for (sqlite_datum sd : sqlite_data) {
		size_t n = strlen(sd.sql);
		if (0 == strncmp(sd.sql, sql, n)) {
			return sd;
		}
	}

	throw std::runtime_error(__FUNCTION__ ": lookup failed");
}

// phony types
#define SQLITE_UNKNOWN 0
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3

// declared type in CREATE TABLE
// type, affinity, enum, category
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
X("CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("VARCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("VARYING CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("NCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("NATIVE CHARACTER(?)", "TEXT", SQLITE_TEXT, 2) \
X("NVARCHAR(?)", "TEXT", SQLITE_TEXT, 2) \
X("TEXT", "TEXT", SQLITE_TEXT, 2) \
X("CLOB", "TEXT", SQLITE_TEXT, 2) \
X("BLOB", "BLOB", SQLITE_BLOB, 3) \
X("REAL", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE", "REAL", SQLITE_FLOAT, 4) \
X("DOUBLE PRECISION", "REAL", SQLITE_FLOAT, 4) \
X("FLOAT", "REAL", SQLITE_FLOAT, 4) \
X("NUMERIC", "NUMERIC", SQLITE_FLOAT, 5) \
X("DECIMAL(?,?)", "NUMERIC", SQLITE_FLOAT, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_BOOLEAN, 5) \
X("DATE", "NUMERIC", SQLITE_DATETIME, 5) \
X("DATETIME", "NUMERIC", SQLITE_DATETIME, 5) \


#define SQLITE_DECL(a,b,c,d) {a, c},
inline const std::map<std::string, int> sqlite_decltype_map = {
	SQLITE_DECLTYPE(SQLITE_DECL)
};
#undef SQLITE_DECL

inline int sqlite_decltype(const char* type)
{
	if (!type || !*type) {
		return SQLITE_TEXT;
	}

	const auto& t = sqlite_decltype_map.find(type);

	return t != sqlite_decltype_map.end() ? t->second : SQLITE_TEXT;
}

namespace sqlite {

	// sqlite datetime
	struct datetime {
		union {
			double f;
			sqlite3_int64 i;
			const unsigned char* t;
		} value; // SQLITE_FLOAT/INTEGER/TEXT
		int type;
	};

	// open/close a sqlite3 database
	class db {
		sqlite3* pdb;
	public:
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
	};

	// https://www.sqlite.org/lang.html
	class stmt {
		sqlite3* pdb;
		sqlite3_stmt* pstmt;
		const char* ptail;
	public:
		stmt(sqlite3* pdb)
			: pdb(pdb), pstmt(nullptr), ptail(nullptr)
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
				ptail = std::exchange(_stmt.ptail, nullptr);
			}

			return *this;
		}

		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
		}

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

		const char* tail() const
		{
			return ptail;
		}

		const char* sql(bool expanded = false) const
		{
			return expanded ? sqlite3_expanded_sql(pstmt) : sqlite3_sql(pstmt);
		}

		stmt& prepare(const std::string_view& sql)
		{
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, sql.data(), (int)sql.size(), &pstmt, &ptail));

			return *this;
		}

		// reset a prepared statement
		stmt& reset()
		{
			FMS_SQLITE_OK(pdb, sqlite3_reset(pstmt));

			return *this;
		}

		// clear any existing bindings
		stmt& clear_bindings()
		{
			FMS_SQLITE_OK(pdb, sqlite3_clear_bindings(pstmt));

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

		// null
		stmt& bind(int i)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_null(pstmt, i));

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

		int bind_parameter_index(const char* name) const
		{
			return sqlite3_bind_parameter_index(pstmt, name);
		}

		// int ret = step();  while (ret == SQLITE_ROW) { ...; ret = step()) { }
		// if !SQLITE_DONE then error
		int step()
		{
			return sqlite3_step(pstmt);
		}

		// number of columns returned
		int column_count() const
		{
			return sqlite3_column_count(pstmt);
		}
		// fundamental sqlite type
		int column_type(int i) const
		{
			return sqlite3_column_type(pstmt, i);
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
		// internal representation
		sqlite3_value* column_value(int i)
		{
			return sqlite3_column_value(pstmt, i);
		}

		// 0-based
		const char* column_name(int i) const
		{
			return sqlite3_column_name(pstmt, i);
		}
		const wchar_t* column_name16(int i) const
		{
			return (const wchar_t*)sqlite3_column_name16(pstmt, i);
		}
		// type specified in CREATE TABLE
		const char* column_decltype(int i) const
		{
			return sqlite3_column_decltype(pstmt, i);
		}

		int type(int i) const
		{
			return ::sqlite_decltype(column_decltype(i));
		}

		// 
		// Get column types and values.
		//
		bool is_null(int i) const
		{
			return SQLITE_NULL == column_type(i);
		}

		bool is_boolean(int i) const
		{
			return SQLITE_BOOLEAN == type(i);
		}
		bool columns_boolean(int i) const
		{
			return 0 != column_int(i);
		}

		bool is_int(int i) const
		{
			return SQLITE_INTEGER == type(i);
		}
		int column_int(int i) const
		{
			return sqlite3_column_int(pstmt, i);
		}
		sqlite3_int64 column_int64(int i) const
		{
			return sqlite3_column_int64(pstmt, i);
		}

		bool is_double(int i) const
		{
			return SQLITE_FLOAT == type(i);
		}
		double column_double(int i) const
		{
			return sqlite3_column_double(pstmt, i);
		}

		bool is_text(int i) const
		{
			return SQLITE_TEXT == type(i);
		}
		const std::string_view column_text(int i) const
		{
			const char* str = (const char*)sqlite3_column_text(pstmt, i);
			int len = column_bytes(i);

			return std::string_view(str, len);
		}
		const std::wstring_view column_text16(int i) const
		{
			const wchar_t* str = (const wchar_t*)sqlite3_column_text16(pstmt, i);
			int len = column_bytes16(i);

			return std::wstring_view(str, len);
		}

		bool is_blob(int i) const
		{
			return SQLITE_BLOB == type(i);
		}
		const void* column_blob(int i) const
		{
			return sqlite3_column_blob(pstmt, i);
		}

		bool is_datetime(int i) const
		{
			return SQLITE_DATETIME == type(i);
		}
		datetime column_datetime(int i) const
		{
			switch (column_type(i)) {
			case SQLITE_FLOAT:
				return datetime{ .value = {.f = column_double(i)}, .type = SQLITE_FLOAT };
			case SQLITE_INTEGER:
				return datetime{ .value = {.i = column_int64(i)}, .type = SQLITE_INTEGER };
			case SQLITE_TEXT:
				return datetime{ .value = {.t = sqlite3_column_text(pstmt, i)}, .type = SQLITE_TEXT };
			}

			return datetime{ .value = {.t = nullptr}, .type = SQLITE_UNKNOWN };
		}
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

	// V is a variant type with free functions 
	// `bool is_xxx(const V&)` and `xxx as_xxx(const V&)`
	// for xxx in `blob`, `double`, `int`, `int64`, `null`, `text` and `text16` 
	template<class V>
	struct table_info {

		std::vector<std::string> name;
		std::vector<std::string> type;
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
			name.emplace_back(_name);
			type.emplace_back(_type);
			notnull.push_back(_notnull);
			dflt_value.emplace_back(_dflt_value);
			pk.push_back(_pk);

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
		sqlite::stmt insert_values(sqlite3* db, const std::string_view& table) const
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

		int bind(sqlite3_stmt* stmt, int j, const V& v) const
		{
			if (is_text(v)) {
				const auto t = as_text(v);
				return sqlite3_bind_text(stmt, j, t.data(), (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (is_text16(v)) {
				const auto t = as_text16(v);
				return sqlite3_bind_text16(stmt, j, t.data(), 2 * (int)t.size(), SQLITE_TRANSIENT);
			}
			else if (is_double(v)) {
				return sqlite3_bind_double(stmt, j, as_double(v));
			}
			else if (is_int(v)) {
				return sqlite3_bind_int(stmt, j, as_int(v));
			}
			else if (is_int64(v)) {
				return sqlite3_bind_int64(stmt, j, as_int64(v));
			}
			else if (is_null(v)) {
				return sqlite3_bind_null(stmt, j);
			}
			else if (is_blob(v)) {
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

} // sqlite
