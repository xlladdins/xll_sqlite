// xll_mem_oper.h - in memory OPER
#pragma once
#include <span>
#include "win_mem_view.h"
#include "xll/xll/XLCALL.h"
#include "xll/xll/ensure.h"

namespace xll::mem {

	template<class X>
	struct traits {};

	template<>
	struct traits<XLOPER> {
		using xchar = CHAR;
		using xerr = WORD;
		using xrw = WORD;
		using xcol = WORD;
	};
	template<>
	struct traits<XLOPER12> {
		using xchar = XCHAR;
		using xerr = int;
		using xrw = RW;
		using xcol = COL;
	};

	template<class X, class T = typename traits<X>::xchar>
	class XOPER : public X {
		static inline Win::mem_view<X> xloper;
		static inline Win::mem_view<T> str;
	public:
		using X::val;
		using X::xltype;
		using value_type = typename XOPER<X>;
		using xrw = typename traits<X>::xrw;
		using xcol = typename traits<X>::xcol;
		using xchar = typename traits<X>::xchar;

		void reset(DWORD len = 0)
		{
			xloper.reset(len);
			str.reset(len);
			xltype = xltypeNil;
		}

		XOPER()
			: X{.xltype = xltypeNil}
		{ }
		XOPER(const XOPER&) = delete;
		XOPER(XOPER&& o) noexcept
		{
			xltype = o.xltype;
			val = o.val;
		}
		XOPER& operator=(const XOPER&) = delete;
		XOPER& operator=(XOPER&& o) noexcept
		{
			if (this != &o) {
				xltype = o.xltype;
				val = o.val;
			}

			return *this;
		}
		~XOPER()
		{ }

		operator X& ()
		{
			return *this;
		}
		operator const X& () const
		{
			return *this;
		}

		xrw rows() const
		{
			return xltype == xltypeMulti ? val.array.rows : 1;
		}
		xrw columns() const
		{
			return xltype == xltypeMulti ? val.array.columns : 1;
		}
		auto size() const
		{
			return rows() * columns();
		}

		XOPER& reshape(xrw r, xcol c)
		{
			if (xltype == xltypeMulti) {
				ensure(r * c == size());

				val.array.rows = r;
				val.array.columns = c;
			}

			return *this;
		}

		explicit XOPER(const X& x)
		{
			if (x.xltype == xltypeStr) {
				*this = XOPER<X>(x.val.str + 1, x.val.str[0]);
			}
			else if (x.xltype == xltypeMulti) {
				*this = XOPER<X>(x.val.array.rows, x.val.array.columns, x.val.array.lparray);
			}
			else {
				xltype = x.xltype;
				val = x.val;
			}
		}
		XOPER& operator=(const X& x)
		{
			if (this != &x) {
				if (x.xltype == xltypeStr) {
					*this = XOPER<X>(x.val.str + 1, x.val.str[0]);
				}
				else if (x.xltype == xltypeMulti) {
					*this = XOPER<X>(x.val.array.rows, x.val.array.columns, x.val.array.lparray);
				}
				else {
					xltype = x.xltype;
					val = x.val;
				}
			}

			return *this;
		}

		// Num
		explicit XOPER(double num)
			: X{.val = {.num = num}, .xltype = xltypeNum}
		{ }
		XOPER& operator=(double num)
		{
			*this = XOPER<X>(num);

			return *this;
		}
		// Str
		XOPER(const xchar* _str, xchar _len)
			: X{ .val = {.str = str.end()}, .xltype = xltypeStr }
		{
			str.append(_len);
			str.append(_str, _len);
		}
		// counted Str
		explicit XOPER(const xchar* _str)
			: XOPER(_str + 1, _str[0])
		{ }
		explicit XOPER(const std::span<xchar>& str)
			: XOPER(str.data(), (xchar)str.size())
		{ }
		XOPER& operator=(const std::span<xchar>& str)
		{
			*this = XOPER<X>(str);

			return *this;
		}
		// Bool
		explicit XOPER(bool xbool)
			: X{ .val = {.xbool = xbool}, .xltype = xltypeBool }
		{ }
		// Err
		explicit XOPER(typename traits<X>::xerr err)
			: X{ .val = {.err = err}, .xltype = xltypeErr }
		{ }
		// Multi
		XOPER(xrw r, xcol c)
			: X{ .val = {.array = {.lparray = xloper.end(), .rows = r, .columns = c}}, .xltype = xltypeMulti }
		{
			for (int i = 0; i < r * c; ++i) {
				xloper.append(XOPER<X>{});
			}
		}
		XOPER(xrw r, xcol c, const X* pa)
			: XOPER(r, c)
		{
			for (int i = 0; i < r * c; ++i) {
				ensure(pa[i].xltype != xltypeMulti);
				val.array.lparray[i] = XOPER<X>(pa[i]);
			}
		}

		XOPER& push_back(const X& x)
		{
			if (xltype == xltypeNil) {
				*this = XOPER<X>(1,1,&x);
			}
			else {
				ensure(xltype == xltypeMulti);
				xloper.append(XOPER<X>(x));

				if (val.array.rows == 1) {
					++val.array.columns;
				}
				else if (val.array.columns == 1) {
					++val.array.rows;
				}
				else {
					ensure(__FUNCTION__ ": not a vector");
				}
			}

			return *this;
		}
	};
	using OPER12 = XOPER<XLOPER12>;
	using OPER4 = XOPER<XLOPER>;
	using OPER = XOPER<XLOPER12>;


} // namespace xll::mem