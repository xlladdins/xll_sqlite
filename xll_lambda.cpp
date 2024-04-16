// xll_lambda.cpp - utility functions for lambda expressions
#include "xll24/xll.h"

using namespace xll;

AddIn xai_define_lambda(
	Macro("xll_define_lambda", "XLL.DEFINE.LAMBDA")
	//.FunctionHelp("Define lambda from selection. Delete if single cell.")
);
int WINAPI xll_define_lambda()
{
#pragma XLLEXPORT
	try {
		OPER s = Excel(xlfSelection);
		ensure(type(s) == xltypeSRef);

		REF r = reshape(SRef(s), 1, 1);

		if (columns(s) == 1) {
			for (unsigned i = 0; i < s.rows(); ++i) {
				OPER name = Excel(xlCoerce, OPER(r));
				if (name) {
					Excel(xlcDeleteName, name);
				}
				r = translate(r, 1, 0);
			}
		}
		else {
			for (unsigned i = 0; i < rows(s); ++i) {
				OPER name = Excel(xlCoerce, OPER(r));

				if (name) {
					REF r1 = translate(r, 0, 1);
					OPER formula = Excel(xlfGetFormula, OPER(r1));
					// remove trailing (...)
					XCHAR* e = formula.val.str + formula.val.str[0];
					ensure(*e == ')');
					while (e > formula.val.str && *--e != '(') {
						;
					}
					--e;
					formula.val.str[0] = (XCHAR)(e - formula.val.str);
					Excel(xlcDefineName, name, formula);
				}

				r = translate(r, 1, 0);
			}
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());

		return FALSE;
	}

	return TRUE;
}
// similar to Ctrl-Shift-F3
Auto <OpenAfter> xaoa_define_lambda([]() {
	Excel(xlcOnKey, OPER(ON_CTRL ON_ALT ON_F3), OPER("XLL.DEFINE.LAMBDA"));
	return TRUE;
});

AddIn xai_get_name(
	Function(XLL_LPOPER, "xll_get_name", "XLL.GET.NAME")
	.Arguments({
		Arg(XLL_LPOPER, "name", "is a defined name."),
		Arg(XLL_LPOPER, "_info", "is an optional boolean indicating if name is defined only for the sheet, not the entire workbook.")
		})
	.Uncalced()
	.FunctionHelp("Return formula for defined name.")
	.HelpTopic("https://xlladdins.github.io/Excel4Macros/get.name.html")
	.Category("XLL")
);
LPOPER WINAPI xll_get_name(LPOPER px, LPOPER pi)
{
#pragma XLLEXPORT
	static OPER result;

	try {
		OPER name = *px;
		ensure(type(name) == xltypeStr);
		ensure(name.val.str[0] > 0);
		if (name.val.str[1] != '!') {
			name = OPER("!") & name;
		}

		if (pi->is_missing()) {
			result = Excel(xlfGetName, name);
		}
		else {
			OPER two(2);
			result = Excel(xlfGetName, name, two);
		}
	}
	catch (const std::exception& ex) {
		XLL_ERROR(ex.what());
	}

	return &result;
}