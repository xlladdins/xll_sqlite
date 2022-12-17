// xll_sqlite_stmt.cpp - Sqlite3 bindings.
#include <thread>
#include "xll_sqlite.h"

using namespace xll;

AddIn xai_sqlite_stmt(
	Function(XLL_HANDLEX, "xll_sqlite_stmt", "\\" CATEGORY ".STMT")
	.Arguments({Arg_db})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Prepare a statement.")
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

		result = stmt_->sql(expanded);
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

		stmt_->reset();
		stmt_->clear_bindings();
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
		Arg_nh
		})
	.Category(CATEGORY)
	.FunctionHelp("Step throught a sqlite statement.")
	.HelpTopic("https://www.sqlite.org/c3ref/exec.html")
);
LPOPER12 WINAPI xll_sqlite_stmt_exec(HANDLEX stmt, BOOL no_headers)
{
#pragma XLLEXPORT
	static mem::OPER<XLOPER12> result;

	try {
		result.reset();
		result = ErrNA12;
		handle<sqlite::stmt> stmt_(stmt);
		ensure(stmt_);

		stmt_->reset();
		result = sqlite_exec_mem<XLOPER12>(*stmt_, no_headers);
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
		Arg_nh,
		Arg_bind
		})
	.Category(CATEGORY)
	.FunctionHelp("Return result of executing sql with optional binding.")
	.HelpTopic("https://www.sqlite.org/c3ref/query.html")
);
LPXLOPER12 WINAPI xll_sqlite_query(HANDLEX db, const LPOPER12 psql, BOOL no_headers, const LPOPER4 pval)
{
#pragma XLLEXPORT
	static mem::OPER<XLOPER12> result;

	try {
		result.reset();
		result = ErrNA;
		handle<sqlite::db> db_(db);
		ensure(db_);

		sqlite::stmt stmt(*db_);
		std::string sql = to_string(*psql, " ", " ");
		stmt.prepare(sql);
		sqlite_bind(stmt, *pval);
		
		result = sqlite_exec_mem<XLOPER12>(stmt, no_headers);
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
		Arg_nh,
		})
	.Category(CATEGORY)
	.FunctionHelp("Explain the query plan of a prepared statement.")
	.HelpTopic("https://www.sqlite.org/eqp.html")
);
LPOPER WINAPI xll_sqlite_stmt_explain(HANDLEX stmt, BOOL no_headers)
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
		result = sqlite_exec(eqp, no_headers);
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