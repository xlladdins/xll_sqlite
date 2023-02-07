// xll_sqlite_stmt.cpp - Sqlite3 bindings.
//#include <thread>
#include "xll_sqlite.h"

using namespace xll;

AddIn xai_sqlite_stmt(
	Function(XLL_HANDLEX, "xll_sqlite_stmt", "\\" CATEGORY ".STMT")
	.Arguments({Arg_db})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Create a statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/prepare.html")
);
HANDLEX WINAPI xll_sqlite_stmt(HANDLEX db)
{
#pragma XLLEXPORT
	HANDLEX result = INVALID_HANDLEX;

	try {
		handle<sqlite::db> db_(db);
		ensure(db_);
		handle<sqlite::stmt> stmt_(new sqlite::stmt(*db_));
		ensure(stmt_);

		result = stmt_.get();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_stmt_sql(
	Function(XLL_LPOPER, "xll_sqlite_stmt_sql", CATEGORY ".SQL")
	.Arguments({
		Arg_stmt,
		Arg(XLL_BOOL, "_expanded", "is an optional value indicating expanded sql.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the sql associated with a statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/expanded_sql.html")
);
LPOPER WINAPI xll_sqlite_stmt_sql(HANDLEX stmt, BOOL expanded)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		if (expanded) {
			result = stmt_->expanded_sql();
		}
		else {
			result = stmt_->sql();
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_stmt_db_handle(
	Function(XLL_HANDLEX, "xll_sqlite_stmt_db_handle", CATEGORY ".DB_HANDLE")
	.Arguments({
		Arg_stmt,
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the database handle associated with a statement.")
	.HelpTopic("https://sqlite.org/c3ref/db_handle.html")
);
HANDLEX WINAPI xll_sqlite_stmt_db_handle(HANDLEX stmt)
{
#pragma XLLEXPORT
	HANDLEX result = INVALID_HANDLEX;

	try {
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		result = to_handle<sqlite3>(stmt_->db_handle());
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_stmt_errmsg(
	Function(XLL_LPOPER, "xll_sqlite_stmt_errmsg", CATEGORY ".ERRMSG")
	.Arguments({
		Arg_stmt,
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the error message associated with a statement.")
	.HelpTopic("https://sqlite.org/c3ref/errcode.html")
);
LPOPER WINAPI xll_sqlite_stmt_errmsg(HANDLEX stmt)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		result = stmt_->errmsg();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_stmt_column_count(
	Function(XLL_DOUBLEX, "xll_sqlite_stmt_column_count", CATEGORY ".COLUMN_COUNT")
	.Arguments({
		Arg_stmt,
		})
		.Category(CATEGORY)
	.FunctionHelp("Return the error message associated with a statement.")
	.HelpTopic("https://sqlite.org/c3ref/column_count.html")
);
double WINAPI xll_sqlite_stmt_column_count(HANDLEX stmt)
{
#pragma XLLEXPORT
	double result = INVALID_HANDLEX;

	try {
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		result = stmt_->column_count();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_stmt_column_name(
	Function(XLL_LPOPER, "xll_sqlite_stmt_column_name", CATEGORY ".COLUMN_NAME")
	.Arguments({
		Arg_stmt,
		Arg(XLL_LPOPER, "index", "is an optional 0-based column index.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the column name at column index for a statement or all column names if not specified.")
	.HelpTopic("https://sqlite.org/c3ref/column_name.html")
);
LPOPER WINAPI xll_sqlite_stmt_column_name(HANDLEX stmt, LPOPER pi)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		if (pi->is_missing()) {
			sqlite::iterator it(*stmt_);
			result = OPER{};
			sqlite::map(it, std::back_inserter(result), 
				[](const sqlite::value& v) { return OPER(v.name()); });
		}
		else {
			result = (*stmt_)[pi->as_int()].name();
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_stmt_column_type(
	Function(XLL_LPOPER, "xll_sqlite_stmt_column_type", CATEGORY ".COLUMN_TYPE")
	.Arguments({
		Arg_stmt,
		Arg(XLL_LPOPER, "index", "is an optional 0-based column index.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the fundamental sqlite type at column index for a statement or all column names if not specified.")
	.HelpTopic("https://sqlite.org/c3ref/column_blob.html")
);
LPOPER WINAPI xll_sqlite_stmt_column_type(HANDLEX stmt, LPOPER pi)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		if (pi->is_missing()) {
			sqlite::iterator it(*stmt_);
			result = OPER{};
			sqlite::map(it, std::back_inserter(result), 
				[](const sqlite::value& v) { return OPER(sqlite::sqlname(v.column_type())); });
		}
		else {
			result = sqlite::sqlname((*stmt_)[pi->as_int()].column_type());
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_stmt_column_sqltype(
	Function(XLL_LPOPER, "xll_sqlite_stmt_column_sqltype", CATEGORY ".COLUMN_SQLTYPE")
	.Arguments({
		Arg_stmt,
		Arg(XLL_LPOPER, "index", "is an optional 0-based column index.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return the column SQL type at index for a statement or all column sql types if not specified.")
	.HelpTopic("https://sqlite.org/c3ref/column_blob.html")
);
LPOPER WINAPI xll_sqlite_stmt_column_sqltype(HANDLEX stmt, LPOPER pi)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		if (pi->is_missing()) {
			sqlite::iterator it(*stmt_);
			result = OPER{};
			sqlite::map(it, std::back_inserter(result),
				[](const sqlite::value& v) { return OPER(v.column_decltype()); });
		}
		else {
			result = (*stmt_)[pi->as_int()].column_decltype();
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sqlite_stmt_prepare(
	Function(XLL_HANDLEX, "xll_sqlite_stmt_prepare", CATEGORY ".PREPARE")
	.Arguments({
		Arg_stmt,
		Arg_sql
		})
	.Category(CATEGORY)
	.FunctionHelp("Prepare a statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/prepare.html")
);
HANDLEX WINAPI xll_sqlite_stmt_prepare(HANDLEX stmt, const LPOPER psql)
{
#pragma XLLEXPORT
	HANDLEX result = INVALID_HANDLEX;

	try {
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		std::string sql = to_string(*psql, " ", " ");
		stmt_->prepare(sql);

		result = stmt;
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_stmt_bind(
	Function(XLL_HANDLEX, "xll_sqlite_stmt_bind", CATEGORY ".BIND")
	.Arguments({
		Arg_stmt,
		Arg_bind,
		})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("bind a value to indices in a statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/bind_blob.html")
);
HANDLEX WINAPI xll_sqlite_stmt_bind(HANDLEX stmt, const LPOPER4 pval)
{
#pragma XLLEXPORT
	HANDLEX result = INVALID_HANDLEX;

	try {
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		//stmt_->reset();
		//stmt_->clear_bindings();
		sqlite_bind(*stmt_, *pval);
		
		result = stmt;
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return result;
}

AddIn xai_sqlite_stmt_exec(
	Function(XLL_LPOPER12, "xll_sqlite_stmt_exec", CATEGORY ".EXEC")
	.Arguments({
		Arg_stmt,
		})
	.Category(CATEGORY)
	.FunctionHelp("Step throught a sqlite statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/exec.html")
);
LPOPER12 WINAPI xll_sqlite_stmt_exec(HANDLEX stmt)
{
#pragma XLLEXPORT
	static mem::XOPER<XLOPER12> result;

	try {
		//result = ErrNA12;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		result.reset();
		stmt_->reset();
		xll::headers(*stmt_, result);
		xll::map(*stmt_, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return (LPOPER12)&result;
}

AddIn xai_sqlite_query(
	Function(XLL_LPXLOPER12, "xll_sqlite_query", CATEGORY ".QUERY")
	.Arguments({
		Arg_db,
		Arg_sql,
		Arg_bind
		})
	.Category(CATEGORY)
	.FunctionHelp("Return result of executing sql with optional binding.")
	.HelpTopic("https://www.sqlite.org/c3ref/query.html")
);
LPXLOPER12 WINAPI xll_sqlite_query(HANDLEX db, const LPOPER12 psql, const LPOPER4 pval)
{
#pragma XLLEXPORT
	static mem::XOPER<XLOPER12> result;

	try {
		result = ErrNA;
		handle<sqlite::db> db_(db);
		ensure(db_);

		sqlite::stmt stmt(*db_);
		std::string sql = to_string(*psql, " ", " ");
		stmt.prepare(sql);
		sqlite_bind(stmt, *pval);
		
		result.reset();
		xll::headers(stmt, result);
		xll::map(stmt, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return (LPXLOPER12)&result;
}

AddIn xai_sqlite_stmt_explain(
	Function(XLL_LPOPER, "xll_sqlite_stmt_explain", CATEGORY ".EXPLAIN")
	.Arguments({
		Arg_stmt,
		})
	.Category(CATEGORY)
	.FunctionHelp("Explain the query plan of a prepared statement.")
	.HelpTopic("https://www.sqlite.org/eqp.html")
);
LPOPER WINAPI xll_sqlite_stmt_explain(HANDLEX stmt)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = ErrNA;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		auto sql = std::string("EXPLAIN QUERY PLAN ") + stmt_->sql();
		sqlite::stmt eqp(stmt_->db_handle());
		eqp.prepare(sql);

		result = OPER{};
		xll::map(eqp, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

#if 0
void execa(std::stop_token token, OPER12 x, OPER12 h)
{
	//std::this_thread::sleep_for(std::chrono::seconds(px->as_int()));
	Sleep(1000 * x.as_int());
	x.val.num *= 2;

	int result;
	XLOPER12 ret;
	result = ::Excel12(xlAsyncReturn, &ret, 2, &h, &x);
	result = result;
}

AddIn xai_execa(
	Function(XLL_VOID, "xll_execa", "EXECA")
	.Arguments({
		Arg(XLL_LPOPER12, "x", "is x"),
		})
	.Asynchronous()
	.ThreadSafe()
);
void WINAPI xll_execa(LPOPER12 px, LPOPER12 ph)
{
#pragma XLLEXPORT
	std::jthread jt(execa, std::stop_token{}, *px, *ph);
}

#endif // 0