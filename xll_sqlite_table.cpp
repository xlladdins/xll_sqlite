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
	.FunctionHelp("Guess sqlite types of columns.")
);
LPOPER WINAPI xll_sqlite_types(const LPOPER po, long column)
{
#pragma XLLEXPORT
	static OPER o;

	int type = guess_sqltype(*po, column);
	o = sqlite::sqlname(type);

	return &o;
}

AddIn xai_sqlite_insert_table(
	Function(XLL_HANDLEX, "xll_sqlite_insert_table", CATEGORY ".INSERT_TABLE")
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

		const OPER& o = *po;
		auto sql = std::string("INSERT INTO ")
			+ sqlite::table_name(table)
			+ " VALUES (?";
		for (unsigned i = 1; i < o.columns(); ++i) {
			sql.append(", ?");
		}
		sql.append(")");

		sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);

		sqlite::stmt stmt(*db_);
		stmt.prepare(sql);
		
		try {
			xll::iterable<XLOPER12> i(o);
			xll::copy(i, stmt);
			sqlite3_exec(*db_, "COMMIT TRANSACTION", NULL, NULL, NULL);
		}
		catch (const std::exception& ex) {
			sqlite3_exec(*db_, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
			XLL_ERROR(ex.what());
		}
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
/*
inline std::string create_table(const OPER& data, OPER columns, OPER types)
{
	unsigned off = 0;
	if (columns.is_missing()) {
		columns.resize(1, data.columns());
		off = 1;
	}
	else {
		ensure(data.columns() == columns.size());
	}
	if (types.is_missing()) {
		types.resize(1, data.columns());
	}
	else {
		ensure(data.columns() == types.size());
	}

	for (unsigned j = 0; j < data.columns(); ++j) {
		columns[j] = columns[j] ? columns[j] : data(0, j); // first row has column names
		if (columns[j].val.str[0] != 0 && columns[j].val.str[1] != '[') {
			columns[j] = OPER("[") & columns[j] & OPER("]");
		}
		types[j] = types[j] ? types[j] : sqlite::sqlname(guess_sqltype(data, j, data.rows() - off, off));
	}

	return create_table(columns, types);
}
*/
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

		if (pcolumns->is_missing()) {
			row = 1; // first row has column names
			column.resize(1, data.columns());
			for (unsigned j = 0; j < data.columns(); ++j) {
				column[j] = data(0, j);
			}
		}
		for (unsigned j = 0; j < column.size(); ++j) {
			if (column[j].val.str[0] != 0 && column[j].val.str[1] != '[') {
				column[j] = OPER("[") & column[j] & OPER("]");
			}
		}

		if (ptypes->is_missing()) {
			type.resize(1, data.columns());
			for (unsigned j = 0; j < data.columns(); ++j) {
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
			auto sql = std::string("INSERT INTO ")
				+ sqlite::table_name(table)
				+ " VALUES (?";
			for (unsigned j = 1; j < pdata->columns(); ++j) {
				sql.append(", ?");
			}
			sql.append(")");
			stmt.prepare(sql);

			sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
	
			try {
				for (unsigned i = row; i < pdata->rows(); ++i) {
					for (unsigned j = 0; j < pdata->columns(); ++j) {
						int tt;
						tt = stmt.sqltype(j + 1);
						bind(stmt, j + 1, (*pdata)(i, j), type[j].as_int());
					}
					stmt.step();
					stmt.reset();
				}
				sqlite3_exec(*db_, "COMMIT TRANSACTION", NULL, NULL, NULL);
			}
			catch (const std::exception& ex) {
				sqlite3_exec(*db_, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
				XLL_ERROR(ex.what());
			}
		}

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

