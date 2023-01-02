// xll_sqlite_table.cpp
#pragma warning(disable : 5103)
#pragma warning(disable : 5105)
#include "xll_sqlite.h"

using namespace xll;

#if 0
const char* common_type(const char* a, const char* b)
{
	int ta = sqlite::type(a);
	int tb = sqlite::type(b);

	if (ta == tb) {
		return a;
	}

	switch (ta) {
	case SQLITE_INTEGER:
		switch (tb) {
		case SQLITE_FLOAT:
			return b;
		case SQLITE_TEXT:
			return b;
		case SQLITE_DATETIME:
			return b;
		default:
			return a;
		}
	case SQLITE_FLOAT:
		switch (tb) {
		case SQLITE_TEXT:
			return b;
		case SQLITE_DATETIME:
			return b;
		default:
			return a;
		}
	case SQLITE_TEXT:
		switch (tb) {
		case SQLITE_DATETIME:
			return b;
		default:
			return a;
		}
	}

	return a;
}
#endif // 9

AddIn xai_sqlite_types(
	Function(XLL_LPOPER, "xll_sqlite_types", CATEGORY ".TYPES")
	.Arguments({
		Arg(XLL_LPOPER, "range", "is a range."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Guess sqlite types of columns.")
);
LPOPER WINAPI xll_sqlite_types(const LPOPER po)
{
#pragma XLLEXPORT
	static OPER o;

	int type = guess_sqltype(*po);
	o = sqlite_name(sqlite_type[type]);

	return &o;
}
#if 0
AddIn xai_sqlite_insert_table(
	Function(XLL_HANDLEX, "xll_sqlite_insert_table", CATEGORY ".INSERT_TABLE")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_PSTRING4, "table", "is the name of the table."),
		Arg(XLL_LPOPER, "data", "is a range of data or a handle to a sqlite cursor."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Create a sqlite table in a database.")
	.HelpTopic("https://www.sqlite.org/lang_insert.html")
);
HANDLEX WINAPI xll_sqlite_insert_table(HANDLEX db, const char* table, const OPER& o)
{
#pragma XLLEXPORT
	try {
		handle<sqlite::db> db_(db);
		ensure(db_);
		if (o.size() == 1 && o.is_num()) {
			handle<sqlite::cursor> cur_(o.as_num());
			ensure(cur_);
			sqlite::insert(*db_, table + 1, table[0], *cur_);
		}
		else {
			xll::cursor<XLOPER12> cur(o);
			sqlite::insert(*db_, table + 1, table[0], cur);
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}
#endif // 0

AddIn xai_sqlite_create_table(
	Function(XLL_HANDLEX, "xll_sqlite_create_table", CATEGORY ".CREATE_TABLE")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_CSTRING4, "name", "is the name of the table."),
		Arg(XLL_LPOPER, "data", "is a range of data."),
		Arg(XLL_LPOPER, "_columns", "is an optional range of column names."),
		Arg(XLL_LPOPER, "_types", "is an optional range of column types."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Create a sqlite table in a database.")
	.HelpTopic("https://www.sqlite.org/lang_createtable.html")
);
HANDLEX WINAPI xll_sqlite_create_table(HANDLEX db, const char* table, LPOPER pdata, LPOPER pcolumns, LPOPER ptypes)
{
#pragma XLLEXPORT
	try {
		ensure(!pdata->is_missing());
		if (!pcolumns->is_missing()) {
			ensure(pdata->columns() == pcolumns->size());
		}
		if (!ptypes->is_missing()) {
			ensure(pdata->columns() == ptypes->size());
		}

		handle<sqlite::db> db_(db);
		ensure(db_);

		const OPER& data = *pdata;
		OPER column(1, data.columns());
		OPER type(1, data.columns());
		unsigned row = 0; // first row of data

		if (pcolumns->is_missing()) {
			row = 1; // first row has column names
			for (unsigned j = 0; j < data.columns(); ++j) {
				column[j] = data(0, j);
			}
		}
		else {
			for (unsigned j = 0; j < data.columns(); ++j) {
				column[j] = pcolumns->operator[](j);
			}
		}

		if (ptypes->is_missing()) {
			row = 1; // first row has column names
			for (unsigned j = 0; j < data.columns(); ++j) {
				type[j] = type_name(data(row, j));
			}
		}
		else {
			for (unsigned j = 0; j < data.columns(); ++j) {
				type[j] = ptypes->operator[](j);
			}
		}

		OPER ct = OPER("CREATE TABLE [") & OPER(table) & OPER("] (");
		const char* comma = "";
		for (unsigned j = 0; j < data.columns(); ++j) {
			ct &= comma;
			ct &= column[j];
			ct &= " ";
			ct &= type[j];
			comma = ", ";
		}
		ct &= ")";

		sqlite::stmt stmt(*db_);
		stmt.exec(ct.to_string());

		//sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);

		//xll::cursor

		//sqlite3_exec(*db_, "COMMIT TRANSACTION", NULL, NULL, NULL);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}

// !!!this should only take a stmt, create the table, and exec the stmt
AddIn xai_sqlite_create_table_as(
	Function(XLL_HANDLEX, "xll_sqlite_create_table_as", CATEGORY ".CREATE_TABLE_AS")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_CSTRING4, "table", "is the name of the table."),
		Arg(XLL_LPOPER, "select", "is a SELECT query."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Create a sqlite table in a database using SELECT.")
	.HelpTopic("https://www.sqlite.org/lang_createtable.html")
);
HANDLEX WINAPI xll_sqlite_create_table_as(HANDLEX db, const char* table, LPOPER pselect)
{
#pragma XLLEXPORT
	try {
		handle<sqlite::db> db_(db);
		ensure(db_);

		const auto dte = std::string("DROP TABLE IF EXISTS [") + table + "]";
		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, dte.c_str(), 0, 0, 0));

		// not optimal. exec stmt???
		std::string select;
		if (pselect->is_num()) {
			handle<sqlite::stmt> stmt_(pselect->as_num());
			ensure(stmt_);
			select = stmt_->sql();
		}
		else {
			select = to_string(*pselect, " ", " ");
		}

		const std::string cta = std::string("CREATE TABLE ") + sqlite::table_name(table) + " AS " + select;
		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, cta.c_str(), 0, 0, 0));
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}

