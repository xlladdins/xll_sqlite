// fms_sqlite.h
#pragma once
#include <map>
#include <string>
#include <stdexcept>
#include "sqlite-amalgamation-3390400/sqlite3.h"
#include "fms_parse.h"

#define FMS_SQLITE_OK(DB, OP) { int status = OP; if (SQLITE_OK != status) \
	throw std::runtime_error(sqlite3_errmsg(DB)); }

// fundamental sqlite column types
#define SQLITE_TYPE_ENUM(X) \
X(SQLITE_INTEGER, "INTEGER") \
X(SQLITE_FLOAT,   "FLOAT")   \
X(SQLITE_TEXT,    "TEXT")    \
X(SQLITE_BLOB,    "BLOB")    \
X(SQLITE_NULL,    "NULL")    \

// phony types
#define SQLITE_NUMERIC -1
#define SQLITE_DATETIME -2
#define SQLITE_BOOLEAN -3
#define SQLITE_UNKNOWN -4

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
X("NUMERIC", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DECIMAL(?,?)", "NUMERIC", SQLITE_NUMERIC, 5) \
X("BOOLEAN", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DATE", "NUMERIC", SQLITE_NUMERIC, 5) \
X("DATETIME", "NUMERIC", SQLITE_NUMERIC, 5) \

namespace sqlite {

#define SQLITE_NAME(a,b) {a, b},
	inline const std::map<int, const char*> type_name = {
		SQLITE_TYPE_ENUM(SQLITE_NAME)
	};
#undef SQLITE_NAME

#define SQLITE_TYPE(a,b) {b, a},
	inline const std::map<std::string, int> type_value = {
		SQLITE_TYPE_ENUM(SQLITE_TYPE)
	};
#undef SQLITE_TYPE

	inline int type(const char* str)
	{
		if (!str || !*str) {
			return SQLITE_TEXT;
		}
		switch (*str++) {
		case 'B': 
			return *str == 'O' ? SQLITE_BOOLEAN : SQLITE_BLOB;
		case 'C': case 'V': 
			return SQLITE_TEXT;
		case 'D': 
			return *str == 'A' ? SQLITE_DATETIME : *str == 'E' ? SQLITE_NUMERIC : SQLITE_FLOAT;
		case 'F': case 'R': 
			return SQLITE_FLOAT;
		case 'I': case 'M': case 'S': case 'U': 
			return SQLITE_INTEGER;
		case 'N': 
			return *str == 'U' ? SQLITE_NUMERIC : SQLITE_TEXT;
		case 'T': 
			return *str == 'I' ? SQLITE_INTEGER : SQLITE_TEXT;
		}

		return SQLITE_UNKNOWN;
	}

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

	class stmt {
		sqlite3* pdb;
		sqlite3_stmt* pstmt;
		const char* ptail;
	public:
		stmt(sqlite3* pdb)
			: pdb(pdb), pstmt(nullptr), ptail(nullptr)
		{ }
		stmt(const stmt&) = delete;
		stmt& operator=(const stmt&) = delete;
		~stmt()
		{
			if (pstmt) {
				sqlite3_finalize(pstmt);
			}
			pstmt = nullptr;
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

		const char* sql(bool expanded = true) const
		{
			return expanded ? sqlite3_expanded_sql(pstmt) : sqlite3_sql(pstmt);
		}

		stmt& prepare(const char* sql, int nsql = -1)
		{
			FMS_SQLITE_OK(pdb, sqlite3_prepare_v2(pdb, sql, nsql, &pstmt, &ptail));

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
		stmt& bind(int i, int j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int(pstmt, i, j));

			return *this;
		}
		stmt& bind(int i, int64_t j)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_int64(pstmt, i, j));

			return *this;
		}
		stmt& bind(int i, const char* str, int len = -1, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text(pstmt, i, str, len, cb));

			return *this;
		}
		stmt& bind(int i, const wchar_t* str, int len = -1, void(*cb)(void*) = SQLITE_TRANSIENT)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_text16(pstmt, i, str, len, cb));

			return *this;
		}
		stmt& bind(int i)
		{
			FMS_SQLITE_OK(pdb, sqlite3_bind_null(pstmt, i));

			return *this;
		}

		int bind_parameter_index(const char* name)
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

		int column_type(int i) const
		{
			return sqlite3_column_type(pstmt, i);
		}

		int column_bytes(int i) const
		{
			return sqlite3_column_bytes(pstmt, i);
		}

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
		const char* column_decltype(int i) const
		{
			return sqlite3_column_decltype(pstmt, i);
		}
	};

} // sqlite
