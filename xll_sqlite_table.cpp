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
#endif // 0

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

		sqlite::stmt stmt(*db_);
		stmt.prepare(sql);
		xll::iterable<XLOPER12> i(o);
		xll::copy(i, stmt);
	/*
		if (o.size() == 1 && o.is_num()) {
			handle<sqlite::cursor> cur_(o.as_num());
			ensure(cur_);
			//sqlite::insert(*db_, table + 1, table[0], *cur_);
		}
		else {
		*/
		/*}*/
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
		if (!types.is_missing()) {
			ct.append(" ");
			ct.append(types[j].to_string());
		}
		comma = ", ";
	}
	ct.append(")");

	return ct;
}
inline std::string create_table(const OPER& data, OPER columns, OPER types)
{
	if (columns.is_missing()) {
		columns.resize(1, data.columns());
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
		types[j] = types[j] ? types[j] : sqlite::sqlname(guess_sqltype(data(1, j)));
	}

	return create_table(columns, types);
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
		OPER column(1, data.columns());
		OPER type(1, data.columns());
		unsigned row = 0; // first row of data

		if (pcolumns->is_missing()) {
			row = 1; // first row has column names
		}

		auto ct = std::string("CREATE TABLE ")
			+ sqlite::table_name(table);
		if (pdata->is_missing()) {
			ct.append(create_table(*pcolumns, *ptypes));
		}
		else {
			ct.append(create_table(*pdata, *pcolumns, *ptypes));
		}

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

			//sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
	
			xll::iterable i(*pdata, row);
			copy(i, stmt);

			//sqlite3_exec(*db_, "COMMIT TRANSACTION", NULL, NULL, NULL);
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

