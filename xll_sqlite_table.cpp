// xll_sqlite_table.cpp
#pragma warning(disable : 5103)
#pragma warning(disable : 5105)
#include "xll_sqlite.h"

using namespace xll;

AddIn xai_sqlite_types(
	Function(XLL_LPOPER, "xll_sqlite_types", CATEGORY ".TYPE")
	.Arguments({
		Arg(XLL_LPOPER, "range", "is a range."),
		Arg(XLL_LONG, "column", "is the range column to check."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Guess sqlite type of column.")
);
LPOPER WINAPI xll_sqlite_types(const LPOPER po, long column)
{
#pragma XLLEXPORT
	static OPER o;

	int type = guess_sqltype(*po, column);
	o = sqlite::sqlname(type);

	return &o;
}

// types of table columns
inline std::map<std::string,std::pair<int,int>> sqlite_types(sqlite3* db, const char* table)
{
	std::map<std::string, std::pair<int,int>> ts;

	sqlite::stmt stmt(db);
	const auto q = std::string("select * from ") + sqlite::table_name(table);
	stmt.prepare(q.c_str());
	for (int i = 0; i < stmt.column_count(); ++i) {
		const auto ni = stmt.column_name(i);
		ts[stmt.column_name(i)] = std::make_pair(stmt.column_index(ni), stmt.sqltype(i));
	}

	return ts;
}

// insert row i
inline void sqlite_insert(sqlite::stmt& stmt, const OPER& data, int i, const std::vector<int>& type )
{
	for (unsigned j = 0; j < data.columns(); ++j) {
		xll::bind(stmt, j + 1, data(i, j), type[j]);
	}
	stmt.step();
	stmt.reset();
}

inline void sqlite_insert_into(sqlite3* db, const char* table, const OPER& data, unsigned off = 0)
{
	const auto nt = sqlite_types(db, table);
	
	auto sql = std::string("INSERT INTO ")
		+ sqlite::table_name(table)
		+ " VALUES (?";
	for (size_t i = 1; i < nt.size(); ++i) {
		sql.append(", ?");
	}
	sql.append(")");
	sqlite::stmt stmt(db);
	stmt.prepare(sql);

	std::vector<int> ts(nt.size());
	for (const auto& [ni, ti] : nt) {
		ts[ti.first] = ti.second;
	}

	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	try {
		for (unsigned i = off; i < data.rows(); ++i) {
			sqlite_insert(stmt, data, i, ts);
		}
		sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, NULL);
	}
	catch (const std::exception& ex) {
		sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
		XLL_ERROR(ex.what());
	}
	catch (...) {
		XLL_ERROR(__FUNCTION__ ": unknown exception");
	}
}

AddIn xai_sqlite_insert_table(
	Function(XLL_HANDLEX, "xll_sqlite_insert_table", CATEGORY ".INSERT_INTO")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_CSTRING4, "table", "is the name of the table."),
		Arg(XLL_LPOPER, "data", "is a range of data or a handle to a sqlite cursor."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Create a sqlite table in a database.")
	.HelpTopic("https://www.sqlite.org/lang_insert.html")
);
HANDLEX WINAPI xll_sqlite_insert_table(HANDLEX db, const char* table, const LPOPER po)
{
#pragma XLLEXPORT
	try {
		handle<sqlite::db> db_(db);
		ensure(db_);
	
		sqlite_insert_into(*db_, table, *po);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}

// " (column type, ...)"
inline std::string create_table(const OPER& columns, const OPER& types)
{
	const char* comma = "";
	auto ct = std::string(" (");
	for (unsigned j = 0; j < columns.size(); ++j) {
		ct.append(comma);
		ct.append(columns[j] ? columns[j].to_string() : (OPER("col") & OPER(j)).to_string());
		ct.append(" ");
		ct.append(sqlite::sqlname(types[j].as_int()));
		comma = ", ";
	}
	ct.append(")");

	return ct;
}

AddIn xai_sqlite_create_table(
	Function(XLL_HANDLEX, "xll_sqlite_create_table", CATEGORY ".CREATE_TABLE")
	.Arguments({
		Arg(XLL_HANDLEX, "db", "is a handle to a sqlite database."),
		Arg(XLL_CSTRING4, "name", "is the name of the table."),
		Arg(XLL_LPOPER, "data", "is a range of data."),
		Arg(XLL_LPOPER, "columns", "is an optional range of column names."),
		Arg(XLL_LPOPER, "types", "is an optional range of column types."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Create a sqlite table in a database and populate if data is not missing.")
	.HelpTopic("https://www.sqlite.org/lang_createtable.html")
);
HANDLEX WINAPI xll_sqlite_create_table(HANDLEX db, const char* table, LPOPER pdata, LPOPER pcolumns, LPOPER ptypes)
{
#pragma XLLEXPORT
	try {
		handle<sqlite::db> db_(db);
		ensure(db_);

		const OPER& data = *pdata;
		OPER column = *pcolumns;
		OPER type = *ptypes;
		unsigned row = 0; // first row of data

		if (column.is_missing()) {
			ensure(rows(*pdata) > 1 || !"table must have at least two rows");
			row = 1; // first row has column names
			column.resize(1, pdata->columns());
			for (unsigned j = 0; j < pdata->columns(); ++j) {
				column[j] = data(0, j);
				ensure(column[j].is_str());
			}
		}
		// quote column names
		for (unsigned j = 0; j < column.size(); ++j) {
			if (column[j].val.str[0] != 0 && column[j].val.str[1] != '[') {
				column[j] = OPER("[") & column[j] & OPER("]");
			}
		}

		if (ptypes->is_missing()) {
			type.resize(1, data.columns());
			for (unsigned j = 0; j < type.columns(); ++j) {
				type[j] = guess_sqltype(data, j, data.rows() - row, row);
			}
		}

		auto ct = std::string("CREATE TABLE ")
			+ sqlite::table_name(table);
		ct.append(create_table(column, type));

		sqlite::stmt stmt(*db_);
		stmt.exec(std::string("DROP TABLE IF EXISTS ") + sqlite::table_name(table));
		stmt.exec(ct);

		if (!pdata->is_missing()) {
			sqlite_insert_into(*db_, table, *pdata, row);
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}
	catch (...) {
		XLL_ERROR(__FUNCTION__ ": unknown exception");

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

