// xll_rules.cpp - create SQL for rules
#include "xll/xll/xll.h"

#ifndef CATEGORY
#define CATEGORY "SQL"
#endif

using namespace xll;

/*
logical - and, or, not, // xor, implies, equiv
relation - =, <>, <, >, <=, >=
binop - binop('name, name)

name, logical('name) => "'name logical name"
binop, name, logical, value('name) => "binop('name, name) logical @value"
implies, name, value('name) => "'name <> value OR name = value"
*/

namespace xll {

	class slice {
		const XLOPER12* p;
		unsigned start, size, stride, i;
	public:
		slice(const XLOPER12* p, unsigned start, unsigned size, unsigned stride = 1, unsigned i = 0)
			: p(p), start(start), size(size), stride(stride), i(0)
		{
			ensure(stride > 0);
			ensure(i < size);
		}
		slice(const OPER& o, unsigned start, unsigned size, unsigned stride = 1)
			: slice(&o[0u], start, size, stride)
		{
			ensure(start + (size - 1) * stride < o.size());
		}
		slice(const OPER& o)
			: slice(o, 0, o.size(), 1)
		{ }
		slice(const slice&) = default;
		slice& operator=(const slice&) = default;
		~slice() = default;

		bool operator==(const slice&) const = default;

		auto begin() const
		{
			return slice(p, start, size, stride, 0);
		}
		auto end() const
		{
			return slice(p, start, size, stride, size);
		}

		explicit operator bool() const
		{
			return i < size;
		}
		const XLOPER12& operator*() const
		{
			return p[start + i * stride];
		}
		slice& operator++()
		{
			if (operator bool()) {
				++i;
			}

			return *this;
		}
		slice operator++(int)
		{
			slice s(*this);

			operator++();

			return s;
		}
	};

	inline auto row(OPER& o, unsigned i)
	{
		ensure(i < o.rows());

		return slice(o, i * o.columns(), o.columns(), 1);
	}

	inline auto column(OPER& o, unsigned j)
	{
		ensure(j < o.columns());

		return slice(o, j, o.rows(), o.columns());
	}


	template<class UnOp>
	inline void map(UnOp op, OPER& o)
	{
		for (unsigned j = 0; j < o.size(); ++j) {
			o[j] = op(o[j]);
		}
	}

	template<class UnOp, class S>
	class apply {
		UnOp op;
		S s;
	public:
		apply(UnOp op, S s)
			: op(op), s(s)
		{ }

		explicit operator bool() const
		{
			return s;
		}
		auto operator*() const
		{
			return op(*s);
		}
		apply& operator++()
		{
			++s;

			return *this;
		}
		apply operator++(int)
		{
			apply a(*this);

			operator++();

			return a;
		}
	};

	// convert to SQL type
	inline OPER sql(const OPER& o)
	{
		OPER o_(o);

		if (o.size() > 1) {
			map(sql, o_);
		}
		else {
			switch (o.type()) {
			case xltypeNum:
				o_ = Excel(xlfText, o, OPER("General"));
				break;
			case xltypeStr:
				// quote unless already quoted
				if (o.val.str[0] > 0 && (o.val.str[1] != '\'' && o.val.str[1] != '[')) {
					o_ = Excel(xlfSubstitute, o, OPER("\'"), OPER("\'\'"));
					o_ = OPER("\'") & o_ & OPER("\'");
				}
				break;
			case xltypeBool:
				o_ = OPER(o.val.xbool ? "TRUE" : "FALSE"); // SQL true and false
				break;
			default:
				o_ = OPER("");
			}
		}

		return o_;
	}

	inline bool empty(const XLOPER12& o)
	{
		return o.xltype == xltypeNil
			|| o.xltype == xltypeMissing
			|| o.xltype == xltypeErr
			|| (o.xltype == xltypeStr && o.val.str[0] == 0);
	}

	template<class P, class S>
	inline S skip(P p, S s)
	{
		while (s && p(*s)) {
			++s;
		}

		return s;
	}

	inline OPER join(slice s, const OPER& sep)
	{
		OPER o;

		if (s = skip(empty, s)) {
			o = *s;
			while (++s) {
				if (!empty(*s)) {
					o &= sep;
					o &= *s;
				}
			}
		}

		return o;
	}

} // namespace xll

AddIn xai_sql_to_sql(
	Function(XLL_LPOPER, "xll_sql_to_sql", CATEGORY ".TO_SQL")
	.Arguments({
		Arg(XLL_LPOPER, "range", "is a range."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Return convert Excel range to SQL strings.")
);
LPOPER WINAPI xll_sql_to_sql(const LPOPER pstmts)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = *pstmts;
		map(sql, result);
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

inline OPER infix(OPER o, const OPER& op, const OPER& l = OPER(""), const OPER& r = OPER(""))
{
	return l & join(slice(o), op) & r;
}

AddIn xai_sql_and(
	Function(XLL_LPOPER, "xll_sql_and", CATEGORY ".AND")
	.Arguments({
		Arg(XLL_LPOPER, "stmts", "is range of SQL statements."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Return AND of statements.")
);
LPOPER WINAPI xll_sql_and(const LPOPER pstmts)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = infix(*pstmts, OPER(L") AND ("), OPER(L"("), OPER(L")"));
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

AddIn xai_sql_or(
	Function(XLL_LPOPER, "xll_sql_or", CATEGORY ".OR")
	.Arguments({
		Arg(XLL_LPOPER, "stmts", "is range of SQL statements."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Return OR of statements.")
);
LPOPER WINAPI xll_sql_or(const LPOPER pstmts)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result = infix(*pstmts, OPER(") OR ("), OPER("("), OPER(")"));
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}

#if 0

AddIn xai_sql_cnf(
	Function(XLL_LPOPER, "xll_sql_cnf", CATEGORY ".CNF")
	.Arguments({
		Arg(XLL_LPOPER, "range", "is a two dimensional range of propositions.")
		})
	.Category(CATEGORY)
	.FunctionHelp("Return SQL conjective normal form.")
	.HelpTopic("https://en.wikipedia.org/wiki/Conjunctive_normal_form")
);
LPOPER WINAPI xll_sql_cnf(const LPOPER pw)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		result.resize(pw->rows(), 1);
		for (unsigned j = 0; j < pw->rows(); ++j) {
			result[j] = OPER("(") & join(row(*pw, j), OPER(") OR (")) & OPER(")");
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}


/**********************************

@quantity le 500      | @quantity ge 500
@quantity eq quantity | reldiff le 0.1
					  | quantity ge 500

**********************************/
AddIn xai_sql_cases(
	Function(XLL_LPOPER, "xll_rules", CATEGORY ".CASES")
	.Arguments({
		Arg(XLL_LPOPER, "Cases", "is a list of cases."),
		Arg(XLL_LPOPER, "Rules", "is a list of rules."),
		})
		.Category(CATEGORY)
	.FunctionHelp("Return SQL WHERE clause for query.")
);
LPOPER WINAPI xll_rules(LPOPER pcases, LPOPER prules)
{
#pragma XLLEXPORT
}
#endif // 0