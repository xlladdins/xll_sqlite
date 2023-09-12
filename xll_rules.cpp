// xll_rules.cpp - create SQL for rules
#include "xll/xll/xll.h"

#ifndef CATEGORY
#define CATEGORY "SQL"
#endif

using namespace xll;

namespace fms {

	template<class T>
	inline std::string quote(const T& t)
	{
		return std::format("{}", t);
	}
	template<>
	inline std::string quote(const std::string& t)
	{
		return std::format("'{}'", t);
	}
	inline std::string quote(const char* t)
	{
		return std::format("'{}'", t);
	}

	using vs = std::string; // std::vector<std::string>;

	struct rule {
		virtual ~rule() = default;

		// Fields given the portfolioId
		vs query()
		{
			return _query();
		}
		vs bind()
		{
			return _bind();
		}
		vs select(const std::string& value)
		{
			return _select(value);
		}
		vs as()
		{
			return _as();
		}
		vs where()
		{
			return _where();
		}
	private:
		virtual vs _query() = 0;
		virtual vs _bind() = 0;
		virtual vs _select(const std::string&) = 0;
		virtual vs _where() = 0;
		virtual vs _as() = 0;
	};

	static inline std::map<std::string, std::string> rels{
		{ "eq", "=" },
		{ "ne", "<>" },
		{ "lt", "<" },
		{ "le", "<=" },
		{ "gt", ">" },
		{ "ge", ">=" },
	};

	// name rel
	// BIND @name value
	// SELECT name as name
	// WHERE @name rel name
	struct logical : public rule {
		std::string name, rel;

		logical(const char* name, const char* rel)
			: name(name), rel(rel)
		{
			if (rels.find(rel) == rels.end()) {
				throw std::invalid_argument("fms::logical: invalid relation");
			}
		}

		vs _query() override
		{
			return name;
		}
		vs _bind() override
		{
			return std::format("@{}", name);
		}
		vs _select(const std::string&) override
		{
			return name;
		}
		vs _as() override
		{
			return name;
		}
		vs _where() override
		{
			return std::format("(@{} {} {})", name, rels[rel], name);
		}
	};

	// measure(@name, name) rel value
	// BIND @name value
	struct measure : public rule {
		static inline std::map<std::string, std::string> funs{
			{ "absolute", "ABS({0} - {1})" },
			{ "relative", "ABS(({0} - {1})/@{0})" },
		};
		std::string fun, name, rel;

		measure(const char* fun, const char* name, const char* rel)
			: fun(fun), name(name), rel(rel)
		{
			if (funs.find(fun) == funs.end()) {
				throw std::invalid_argument("fms::measure: invalid function");
			}
		}

		vs _query() override
		{
			return name;
		}
		vs _bind() override
		{
			return std::format("@{}", _as());
		}
		vs _select(const std::string& value) override
		{
			return std::vformat(funs[fun], std::make_format_args(value, name));
		}
		vs _as() override
		{
			return std::format("{}_{}_{}", fun, name, rel);
		}
		vs _where() override
		{
			return std::format("({} {} @{})", _as(), rels[rel], _as());
		}
	};

	// category(name, value)
	// BIND @name value
	struct category : public rule {
		std::string name, cat;

		category(const char* name, const char* cat)
			: name(name), cat(cat)
		{
		}

		vs _query() override
		{
			return name;
		}
		vs _bind() override
		{
			return std::format("@{}", _as());
		}
		vs _select(const std::string& /*value*/) override
		{
			return "";// std::format("{} <> {} OR {} = {}", ;
		}
		vs _as() override
		{
			return ""; // std::format("{}_{}_{}", fun, name, rel);
		}
		vs _where() override
		{
			return ""; // std::format("({} {} @{})", _as(), rels[rel], _as());
		}
	};


} // namespace fms

AddIn xai_rule_query(
	Function(XLL_LPOPER, "xll_rule_query", CATEGORY ".RULE.QUERY")
	.Arguments({
		Arg(XLL_HANDLEX, "rule", "is a handle to a rule."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Return fields to query.")
);
LPOPER WINAPI xll_rule_query(HANDLEX rule)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		handle<fms::rule> r(rule);
		ensure(r);

		result = r->query().c_str();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

AddIn xai_rule_bind(
	Function(XLL_LPOPER, "xll_rule_bind", CATEGORY ".RULE.BIND")
	.Arguments({
		Arg(XLL_HANDLEX, "rule", "is a handle to a rule."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Return fields to bind.")
);
LPOPER WINAPI xll_rule_bind(HANDLEX rule)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		handle<fms::rule> r(rule);
		ensure(r);

		result = r->bind().c_str();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

AddIn xai_rule_select(
	Function(XLL_LPOPER, "xll_rule_select", CATEGORY ".RULE.SELECT")
	.Arguments({
		Arg(XLL_HANDLEX, "rule", "is a handle to a rule."),
		Arg(XLL_LPOPER, "value", "is a value to select."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Return fields to select.")
);
LPOPER WINAPI xll_rule_select(HANDLEX rule, const LPOPER pvalue)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		handle<fms::rule> r(rule);
		ensure(r);

		OPER value = Excel(xlfText, *pvalue, OPER("General"));
		result = r->select(value.to_string()).c_str();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

AddIn xai_rule_as(
	Function(XLL_LPOPER, "xll_rule_as", CATEGORY ".RULE.AS")
	.Arguments({
		Arg(XLL_HANDLEX, "rule", "is a handle to a rule."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Return fields to as.")
);
LPOPER WINAPI xll_rule_as(HANDLEX rule)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		handle<fms::rule> r(rule);
		ensure(r);

		result = r->as().c_str();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

AddIn xai_rule_where(
	Function(XLL_LPOPER, "xll_rule_where", CATEGORY ".RULE.WHERE")
	.Arguments({
		Arg(XLL_HANDLEX, "rule", "is a handle to a rule."),
		})
	.Category(CATEGORY)
	.FunctionHelp("Return fields to where.")
);
LPOPER WINAPI xll_rule_where(HANDLEX rule)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		handle<fms::rule> r(rule);
		ensure(r);

		result = r->where().c_str();
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
		result = ErrNA;
	}

	return &result;
}

AddIn xai_rule_logical(
	Function(XLL_HANDLEX, "xll_rule_logical", "\\" CATEGORY ".RULE_LOGICAL")
	.Arguments({
		Arg(XLL_CSTRING4, "name", "is a name."),
		Arg(XLL_CSTRING4, "rel", "is a relation."),
		})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Return a logical rule.")
);
HANDLEX WINAPI xll_rule_logical(const char* name, const char* rel)
{
#pragma XLLEXPORT
	handle<fms::rule> h(new fms::logical(name, rel));

	return h.get();
}

AddIn xai_rule_measure(
	Function(XLL_HANDLEX, "xll_rule_measure", "\\" CATEGORY ".RULE_MEASURE")
	.Arguments({
		Arg(XLL_CSTRING4, "function", "is a function."),
		Arg(XLL_CSTRING4, "name", "is a name."),
		Arg(XLL_CSTRING4, "rel", "is a relation."),
		})
	.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Return a measure rule.")
);
HANDLEX WINAPI xll_rule_measure(const char* function, const char* name, const char* rel)
{
#pragma XLLEXPORT
	handle<fms::rule> h(new fms::measure(function, name, rel));

	return h.get();
}

AddIn xai_rule_category(
	Function(XLL_HANDLEX, "xll_rule_category", "\\" CATEGORY ".RULE_CATEGORY")
	.Arguments({
		Arg(XLL_CSTRING4, "name", "is a function."),
		Arg(XLL_CSTRING4, "category", "is a name."),
		})
		.Uncalced()
	.Category(CATEGORY)
	.FunctionHelp("Return a category rule.")
);
HANDLEX WINAPI xll_rule_category(const char* name, const char* category)
{
#pragma XLLEXPORT
	handle<fms::rule> h(new fms::category(name, category));

	return h.get();
}

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