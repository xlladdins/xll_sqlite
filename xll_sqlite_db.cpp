// xll_sqlite_db.cpp - Sqlite3 bindings.
#include <format>
#include "xll_sqlite.h"

using namespace xll;

#if 0
// get full filename path and strip out Debug or Release and 64-bit builds
static const char* fullpath(const char* filename)
{
	static char full[_MAX_PATH] = { 0 };
	static path<char> base;

	ensure(0 == base.split(Excel4(xlGetName).to_string().c_str()));
	char* t;
	if (0 != (t = strstr(base.dir, "\\Debug"))) {
		*t = 0;
	}
	else if (0 != (t = strstr(base.dir, "\\Release"))) {
		*t = 0;
	}
	if (0 != (t = strstr(base.dir, "\\x64"))) {
		*t = 0;
	}

	path file(filename);
	if (!file) {
		strcpy_s(base.fname, file.fname);
		strcpy_s(base.ext, file.ext);
	}
	base.fname[0] = 0;
	base.ext[0] = 0;

	ensure(0 == base.make(full, _MAX_PATH));
	strcat(full, filename);

	//MessageBoxA(0, full,  "Full Path", MB_OK);

	return full;
};
#endif // 0

#define SQLITE_OPEN "https://www.sqlite.org/c3ref/c_open_autoproxy.html"

XLL_CONST(LONG, SQLITE_OPEN_READONLY, SQLITE_OPEN_READONLY, "The database is opened in read-only mode. If the database does not already exist, an error is returned.", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_READWRITE, SQLITE_OPEN_READWRITE, "The database is opened for reading and writing if possible, or reading only if the file is write protected by the operating system. In either case the database must already exist, otherwise an error is returned.", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_CREATE, SQLITE_OPEN_CREATE, "The database is opened for reading and writing, and is created if it does not already exist. This is the behavior that is always used for sqlite3_open() and sqlite3_open16().", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_URI, SQLITE_OPEN_URI, "The filename can be interpreted as a URI if this flag is set.", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_MEMORY, SQLITE_OPEN_MEMORY, "The database will be opened as an in-memory database. The database is named by the \"filename\" argument for the purposes of cache-sharing, if shared cache mode is enabled, but the \"filename\" is otherwise ignored.", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_NOMUTEX, SQLITE_OPEN_NOMUTEX, "The new database connection will use the \"multi-thread\" threading mode. This means that separate threads are allowed to use SQLite at the same time, as long as each thread is using a different database connection.", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_FULLMUTEX, SQLITE_OPEN_FULLMUTEX, "The new database connection will use the \"serialized\" threading mode. This means the multiple threads can safely attempt to use the same database connection at the same time. (Mutexes will block any actual concurrency, but in this mode there is no harm ", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_SHAREDCACHE, SQLITE_OPEN_SHAREDCACHE, "The database is opened shared cache enabled, overriding the default shared cache setting provided by sqlite3_enable_shared_cache().", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_PRIVATECACHE, SQLITE_OPEN_PRIVATECACHE, "The database is opened shared cache disabled, overriding the default shared cache setting provided by sqlite3_enable_shared_cache().", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_EXRESCODE, SQLITE_OPEN_EXRESCODE, "The database connection comes up in \"extended result code mode\". In other words, the database behaves has if sqlite3_extended_result_codes(db,1) where called on the database connection as soon as the connection is created. In addition to setting the ext", CATEGORY " Enum", SQLITE_OPEN);
XLL_CONST(LONG, SQLITE_OPEN_NOFOLLOW, SQLITE_OPEN_NOFOLLOW, "The database filename is not allowed to be a symbolic link", CATEGORY " Enum", SQLITE_OPEN);

#undef SQLITE_OPEN

AddIn xai_cwd(
	Function(XLL_CSTRING12, "xll_cwd", "SYS.CWD")
	.Arguments({
		Arg(XLL_CSTRING12, "dir", "is the directory to change to or missing.")}
		)
	.Category("SYS")
	.FunctionHelp("Get or change the current working directory.")
	.HelpTopic("https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/getcwd-wgetcwd?view=msvc-160")
);
wchar_t* WINAPI xll_cwd(wchar_t* dir)
{
#pragma XLLEXPORT
	static wchar_t cwd[MAX_PATH];

	cwd[0] = 0;
	if (*dir) {
		if (0 != _wchdir(dir)) {
			XLL_ERROR(__FUNCTION__ ": could not change to directory");
		}
	}
	else {
		if (nullptr == _wgetcwd(cwd, MAX_PATH)) {
			XLL_ERROR(__FUNCTION__ ": could not get current working directory");
		}
	}

	return cwd;
}

AddIn xai_sqlite_db(
	Function(XLL_HANDLEX, "xll_sqlite_db", "\\" CATEGORY ".DB")
	.Arguments({
		Arg(XLL_CSTRING4, "filename", "is the optional name of a sqlite database. "
			"An in-memory database is used if filename is missing."),
		Arg(XLL_LONG, "flags", "an optional sum of flags from the SQLITE_OPEN_* enumeration."
			"Default is SQLITE_OPEN_READWRITE() + SQLITE_OPEN_CREATE().")
		})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Open a connection to a sqlite3 database.")
	.HelpTopic("https://www.sqlite.org/c3ref/open.html")
);
HANDLEX WINAPI xll_sqlite_db(const char* filename, LONG flags)
{
#pragma XLLEXPORT
	HANDLEX result = INVALID_HANDLEX;

	try {
		handle<sqlite::db> h(new sqlite::db(filename, flags));
		ensure(h);
		result = h.get();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_schema(
	Function(XLL_LPOPER, "xll_sqlite_schema", CATEGORY ".SCHEMA")
	.Arguments({
		Arg_db,
		Arg(XLL_CSTRING4, "_name", "is the optional table name.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return information from sqlite_schema and table if name is specified.")
	.HelpTopic("https://www.sqlite.org/schematab.html")
);
LPOPER WINAPI xll_sqlite_schema(HANDLEX db, const char* name)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::db> db_(db);
		ensure(db_);

		sqlite::stmt stmt(*db_);

		auto sql = std::string("SELECT * FROM sqlite_schema ")
			+ (*name ? std::string("WHERE name = ") + sqlite::variable_name(name) : " ")
			+ "ORDER BY tbl_name, type DESC, name";

		stmt.prepare(sql);

		result = OPER{};
		xll::headers(stmt, result);
		xll::map(stmt, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_pragma(
	Function(XLL_LPOPER, "xll_sqlite_pragma", CATEGORY ".PRAGMA")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_CSTRING4, "_pragma", "is an optional pragma name."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Call 'PRAGMA pragma' or return all pragmas if omitted.")
	.HelpTopic("https://www.sqlite.org/pragma.html")
);
LPOPER WINAPI xll_sqlite_pragma(HANDLEX db, const char* pragma)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::db> db_(db);
		ensure(db_);

		auto sql = std::string("PRAGMA ")
			+ (*pragma ? pragma : "pragma_list");

		sqlite::stmt stmt(*db_);
		stmt.prepare(sql);

		result = OPER{};
		xll::headers(stmt, result);
		xll::map(stmt, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_table_list(
	Function(XLL_LPOPER, "xll_sqlite_table_list", CATEGORY ".TABLE_LIST")
	.Arguments({
		Arg_db,
		})
	.Category(CATEGORY)
	.FunctionHelp("Call PRAGMA table_list.")
	.HelpTopic("https://www.sqlite.org/pragma.html#pragma_table_list")
);
LPOPER WINAPI xll_sqlite_table_list(HANDLEX db)
{
#pragma XLLEXPORT

	return xll_sqlite_pragma(db, "table_list");
}

AddIn xai_sqlite_table_info(
	Function(XLL_LPOPER, "xll_sqlite_table_info", CATEGORY ".TABLE_INFO")
	.Arguments({
		Arg_db,
		Arg(XLL_CSTRING4, "name", "is the name of the table."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Call PRAGMA table_info(table).")
	.HelpTopic("https://www.sqlite.org/pragma.html#pragma_table_info")
);
LPOPER WINAPI xll_sqlite_table_info(HANDLEX db, const char* table)
{
#pragma XLLEXPORT
	handle<sqlite::db> db_(db);

	auto pragma = std::string("table_info(") + sqlite::table_name(table) + ")";

	return xll_sqlite_pragma(db, pragma.c_str());
}
