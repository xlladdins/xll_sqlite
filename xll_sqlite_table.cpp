// xll_sqlite_table.cpp
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

const char* guess_type(const OPER& o)
{
	const char* type = sqlite_name[o.type()];
	if (xltypeStr == o.type()) {
		tm tm;
		fms::view<wchar_t> v(o.val.str + 1, o.val.str[0]);
		if (fms::parse_tm(v, &tm)) {
			return "DATETIME";
		}
	}

	return type;
}

const char* guess_type(const OPER& o, int col, int rows)
{
	const char* type = guess_type(o(0, col));

	for (int i = 1; i < rows; ++i) {
		const char* typei = sqlite_name[o(i, col).type()];
		if (0 != strcmp(type, typei)) {
			type = common_type(type, typei);
		}
	}

	return type;
}

AddIn xai_sqlite_types(
	Function(XLL_LPOPER, "xll_sqlite_types", CATEGORY ".TYPES")
	.Arguments({
		Arg(XLL_LPOPER, "range", "is a range."),
		Arg(XLL_USHORT, "_rows", "is an optional number of rows to scan. Default is all.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Guess sqlite types of columns.")
);
LPOPER WINAPI xll_sqlite_types(const LPOPER po, unsigned short rows)
{
#pragma XLLEXPORT
	static OPER o;

	if (rows == 0) {
		rows = (unsigned short)po->rows();
	}

	o.resize(1, po->size());
	for (unsigned j = 0; j < o.columns(); ++j) {
		o[j] = guess_type(o, j, rows);
	}

	return &o;
}

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
		Arg(XLL_CSTRING4, "table", "is the name of the table."),
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

		unsigned row = 0; // first row of data
		const OPER& data = *pdata;

		if (pcolumns->is_missing()) {
			row = 1; // first row has column names
		}

		sqlite::table_info ti(data.columns());		
		for (unsigned j = 0; j < data.columns(); ++j) {
			OPER namej = row == 0 ? (*pcolumns)[j] : data(0, j);
			namej = namej ? namej : OPER("col") & OPER(j);
			ti.name[j] = namej.to_string();
			
			if (ptypes->is_missing()) {
				ti.type[j] = type_name(data(row, j));
			}
			else {
				ti.type[j] = (*ptypes)[j].to_string();
			}
		}		

		handle<sqlite::db> db_(db);
		ensure(db_);

		sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);

		const auto dtie = std::string("DROP TABLE IF EXISTS ") + sqlite::table_name(table);
		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, dtie.c_str(), 0, 0, 0));

		const auto ct = std::string("CREATE TABLE ") 
			+ sqlite::table_name(table)
			+ ti.schema();

		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, ct.c_str(), 0, 0, 0));

		sqlite::stmt stmt(*db_);
		stmt.prepare(ti.insert_values(table));

		std::vector<int> decl(ti.size());
		for (unsigned j = 0; j < ti.size(); ++j) {
			decl[j] = ::sqlite_decltype(ti.type[j].c_str());
		}

		for (unsigned i = row; i < data.rows(); ++i) {			
			for (unsigned j = 0; j < data.columns(); ++j) {
				const OPER& oij = data(i, j);
				bind(stmt, j + 1, oij, decl[j]);
			}
			stmt.step();
			stmt.reset();
		}

		sqlite3_exec(*db_, "COMMIT TRANSACTION", NULL, NULL, NULL);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}

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

