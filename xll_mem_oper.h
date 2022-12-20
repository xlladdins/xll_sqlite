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
	class OPER : public X {
		static inline Win::mem_view<X> xloper;
		static inline Win::mem_view<T> str;
	public:
		using X::val;
		using X::xltype;
		using xrw = typename traits<X>::xrw;
		using xcol = typename traits<X>::xcol;
		using xchar = typename traits<X>::xchar;

		void reset(DWORD len = 0)
		{
			xloper.reset(len);
			str.reset(len);
			xltype = xltypeNil;
		}

		OPER()
			: X{.xltype = xltypeNil}
		{ }
		OPER(const OPER&) = delete;
		OPER(OPER&& o) noexcept
		{
			xltype = o.xltype;
			val = o.val;
		}
		OPER& operator=(const OPER&) = delete;
		OPER& operator=(OPER&& o) noexcept
		{
			if (this != &o) {
				xltype = o.xltype;
				val = o.val;
			}

			return *this;
		}
		~OPER()
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

		OPER& reshape(xrw r, xcol c)
		{
			if (xltype == xltypeMulti) {
				ensure(r * c == size());

				val.array.rows = r;
				val.array.columns = c;
			}

			return *this;
		}

		explicit OPER(const X& x)
		{
			if (x.xltype == xltypeStr) {
				*this = OPER(x.val.str + 1, x.val.str[0]);
			}
			else if (x.xltype == xltypeMulti) {
				*this = OPER(x.val.array.rows, x.val.array.columns, x.val.array.lparray);
			}
			else {
				xltype = x.xltype;
				val = x.val;
			}
		}
		OPER& operator=(const X& x)
		{
			if (this != &x) {
				if (x.xltype == xltypeStr) {
					*this = OPER(x.val.str + 1, x.val.str[0]);
				}
				else if (x.xltype == xltypeMulti) {
					*this = OPER(x.val.array.rows, x.val.array.columns, x.val.array.lparray);
				}
				else {
					xltype = x.xltype;
					val = x.val;
				}
			}

			return *this;
		}

		// Num
		explicit OPER(double num)
			: X{.val = {.num = num}, .xltype = xltypeNum}
		{ }
		OPER& operator=(double num)
		{
			*this = OPER(num);

			return *this;
		}
		// Str
		OPER(const xchar* _str, xchar _len)
			: X{ .val = {.str = str.end()}, .xltype = xltypeStr }
		{
			str.append(_len);
			str.append(_str, _len);
		}
		// counted Str
		explicit OPER(const xchar* _str)
			: OPER(_str + 1, _str[0])
		{ }
		explicit OPER(const std::span<xchar>& str)
			: OPER(str.data(), (xchar)str.size())
		{ }
		OPER& operator=(const std::span<xchar>& str)
		{
			*this = OPER(str);

			return *this;
		}
		// Bool
		explicit OPER(bool xbool)
			: X{ .val = {.xbool = xbool}, .xltype = xltypeBool }
		{ }
		// Err
		explicit OPER(typename traits<X>::xerr err)
			: X{ .val = {.err = err}, .xltype = xltypeErr }
		{ }
		// Multi
		OPER(xrw r, xcol c)
			: X{ .val = {.array = {.lparray = xloper.end(), .rows = r, .columns = c}}, .xltype = xltypeMulti }
		{
			for (int i = 0; i < r * c; ++i) {
				xloper.append(OPER{});
			}
		}
		OPER(xrw r, xcol c, const X* pa)
			: OPER(r, c)
		{
			for (int i = 0; i < r * c; ++i) {
				ensure(pa[i].xltype != xltypeMulti);
				val.array.lparray[i] = OPER(pa[i]);
			}
		}

		OPER& push_back(const X& x)
		{
			if (xltype == xltypeNil) {
				*this = OPER(1,1,&x);
			}
			else {
				ensure(xltype == xltypeMulti);
				xloper.append(OPER(x));

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

} // namespace xll::mem