// xll_is_as.h - is_xxx/as_xxx for XOPER's
#pragma once
#include <type_traits>
#include <string_view>
#include "xll/xll/xloper.h"

namespace xll {

	template<class X>
	inline bool is_double(const X& x)
	{
		return xltypeNum == type(x);
	}
	template<class X>
	inline double as_double(const X& x)
	{
		return x.val.num;
	}

	template<class X>
	inline bool is_text(const X& x)
	{
		return xltypeStr == type(x) and std::is_same_v<traits<X>::xchar, char>;
	}
	template<class X>
	inline std::string_view as_text(const X& x)
	{
		if constexpr (std::is_same_v<traits<X>::xchar, char>) {
			return std::string_view(x.val.str + 1, x.val.str[0]);
		}
		else {
			return std::string_view{};
		}
	}
	template<class X>
	inline bool is_text16(const X& x)
	{
		return xltypeStr == type(x) and std::is_same_v<traits<X>::xchar, wchar_t>;
	}
	template<class X>
	inline std::wstring_view as_text12(const X& x)
	{
		if constexpr (std::is_same_v<traits<X>::xchar, wchar_t>) {
			return std::wstring_view(x.val.str + 1, x.val.str[0]);
		}
		else {
			return std::wstring_view{};
		}
	}

	template<class X>
	inline bool is_int(const X& x)
	{
		return xltypeInt == type(x);
	}
	template<class X>
	inline double as_int(const X& x)
	{
		return x.val.w;
	}

	template<class X>
	inline bool is_int64(const X& x)
	{
		return xltypeInt == type(x);
	}
	template<class X>
	inline double as_int64(const X& x)
	{
		return x.val.w;
	}

	template<class X>
	inline bool is_null(const X& x)
	{
		return xltypeNil == type(x);
	}

	template<class X>
	inline bool is_blob(const X& x)
	{
		return xltypeBigData == type(x);
	}
	template<class X>
	inline auto as_blob(const X& x)
	{
		return std::basic_string_view(x.val.bigdata.h.lpbData, x.val.bigdata.cbData);
	}

} // namespace xll
