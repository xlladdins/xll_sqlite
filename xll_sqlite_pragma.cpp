// xll_sqlite_pragma.cpp - Sqlite3 bindings.
#include "xll_sqlite.h"

using namespace xll;

AddIn xai_sqlite_schema(
	Function(XLL_LPOPER, "xll_sqlite_schema", CATEGORY ".SCHEMA")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg_nh
		})
	.Category(CATEGORY)
	.FunctionHelp("Return information from sqlite_schema.")
	.HelpTopic("https://www.sqlite.org/schematab.html")
);
LPOPER WINAPI xll_sqlite_schema(HANDLEX db, BOOL no_headers)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::db> db_(db);
		ensure(db_);

		sqlite::stmt stmt(*db_);
		stmt.prepare("select * from sqlite_schema");

		result = sqlite_exec(stmt, no_headers);
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
		Arg_nh
		})
	.Category(CATEGORY)
	.FunctionHelp("Call 'PRAGMA pragma' or return all pramas if omitted.")
	.HelpTopic("https://www.sqlite.org/pragma.html")
);
LPOPER WINAPI xll_sqlite_pragma(HANDLEX db, const char* pragma, BOOL no_headers)
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

		result = sqlite_exec(stmt, no_headers);
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
		Arg_nh
		})
	.Category(CATEGORY)
	.FunctionHelp("Call PRAGMA table_list.")
	.HelpTopic("https://www.sqlite.org/pragma.html#pragma_table_list")
);
LPOPER WINAPI xll_sqlite_table_list(HANDLEX db, bool no_headers)
{
#pragma XLLEXPORT

	return xll_sqlite_pragma(db, "table_list", no_headers);
}

AddIn xai_sqlite_table_info(
	Function(XLL_LPOPER, "xll_sqlite_table_info", CATEGORY ".TABLE_INFO")
	.Arguments({
		Arg_db,
		Arg(XLL_CSTRING4, "name", "is the name of the table."),
		Arg_nh
		})
	.Category(CATEGORY)
	.FunctionHelp("Call PRAGMA table_info(table).")
	.HelpTopic("https://www.sqlite.org/pragma.html#pragma_table_info")
);
LPOPER WINAPI xll_sqlite_table_info(HANDLEX db, const char* name, bool no_headers)
{
#pragma XLLEXPORT
	handle<sqlite::db> db_(db);

	auto pragma = std::string("table_info(") + sqlite::table_name(name) + ")";

	return xll_sqlite_pragma(db, pragma.c_str(), no_headers);
}
