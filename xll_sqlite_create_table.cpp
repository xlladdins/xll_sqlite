// xll_sqlite_create_table.cpp
#include "xll_sqlite.h"

using namespace xll;

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
		ensure(pdata && *pdata);
		ensure(!pcolumns || pcolumns->is_missing() || columns(*pdata) == size(*pcolumns));
		ensure(!ptypes || ptypes->is_missing() 
			|| (columns(*pdata) == size(*ptypes) && rows(*pdata) > 1));

		handle<sqlite::db> db_(db);
		ensure(db_);

		const auto dtie = std::string("DROP TABLE IF EXISTS [") + table + "]";
		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, dtie.c_str(), 0, 0, 0));

		const OPER& data = *pdata;
		const OPER& column = *pcolumns;
		const OPER& type = *ptypes;

		OPER schema(data.columns(), 2);
		for (unsigned j = 0; j < schema.rows(); ++j) {
			schema(j, 0) = OPER("[") & (column ? column[j] : data(0, j)) & OPER("]");
			if (type) {
				const OPER& typej = Excel(xlfUpper, type[j]);
				schema(j, 1) = typej ? typej : type_name(data(1, j));
			}
			else {
				schema(j, 1) = type_name(data(1, j));
			}
		}
		const auto ct = std::string("CREATE TABLE [") + table + "]("
			+ to_string(schema, " ", ", ") + ")";

		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, ct.c_str(), 0, 0, 0));

		sqlite3_exec(*db_, "BEGIN TRANSACTION", NULL, NULL, NULL);

		auto ii = std::string("INSERT INTO [") + table + "] VALUES(?1";
		for (unsigned i = 2; i <= data.columns(); ++i) {
			char buf[4];
			ii += ", ?";
			ii += itoa(i, buf, 10);
		}
		ii += ")";
		sqlite::stmt stmt(*db_);
		stmt.prepare(ii.c_str(), (int)ii.length());

		for (unsigned i = column.is_missing(); i < data.rows(); ++i) {			
			for (unsigned j = 0; j < data.columns(); ++j) {
				const OPER& oij = data(i, j);

				if (oij.is_nil() || oij.is_str() && oij.val.str[0] == 0) {
					stmt.bind(j + 1);
				}
				else if (oij.is_str()) {
					auto v = view(oij);
					tm tm;
					if (fms::parse_tm(v, &tm)) {
						time_t t = _mkgmtime(&tm);
						stmt.bind(j + 1, t);
					}
					stmt.bind(j + 1, oij.val.str + 1, 2 * oij.val.str[0], SQLITE_STATIC);
				}
				else {
					if (Excel(xlfLeft, schema(j, 1), OPER(4)) == "DATE") {
						stmt.bind(j + 1, to_time(oij.as_num()));
					}
					else {
						stmt.bind(j + 1, oij.as_num());
					}
				}
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

		const std::string cta = std::string("CREATE TABLE [") + table + "] AS " + select;
		FMS_SQLITE_OK(*db_, sqlite3_exec(*db_, cta.c_str(), 0, 0, 0));
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		db = INVALID_HANDLEX;
	}

	return db;
}

