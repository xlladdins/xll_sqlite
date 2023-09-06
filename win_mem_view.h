// win_mem_view.cpp - memory mapped data
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memoryapi.h>
#include "xll/xll/ensure.h"

namespace Win {

	template<class T>
	class mem_view {
		HANDLE h;
		DWORD max_len;
	public:
		T* buf;
		DWORD len;

		/// <summary>
		/// Map file of temporary anonymous memory.
		/// </summary>
		/// <param name="h">optional handle to file</param>
		/// <param name="len">initial size of buffer</param>
		mem_view(HANDLE h_ = INVALID_HANDLE_VALUE, DWORD max_len = 1 << 25)
			: h(CreateFileMapping(h_, 0, PAGE_READWRITE, 0, max_len * sizeof(T), nullptr)), 
			  max_len(max_len), buf(nullptr), len(0)
		{
			if (h != NULL) {
				buf = (T*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, len * sizeof(T));
			}
		}
		mem_view(const mem_view&) = delete;
		mem_view(mem_view&& mv) noexcept
		{
			*this = std::move(mv);
		}
		mem_view& operator=(const mem_view&) = delete;
		mem_view& operator=(mem_view&& mv) noexcept
		{
			if (this != &mv) {
				h = std::exchange(mv.h, INVALID_HANDLE_VALUE);
				buf = std::exchange(mv.buf, nullptr);
				len = std::exchange(mv.len, 0);
			}

			return *this;
		}
		~mem_view()
		{
			if (buf) UnmapViewOfFile(buf);
			if (h) CloseHandle(h);
		}

		mem_view& reset(DWORD _len = 0)
		{
			len = _len;

			return *this;
		}

		operator T* ()
		{
			return buf;
		}
		operator const T* () const
		{
			return buf;
		}

		T* end()
		{
			return buf + len;
		}
		const T* end() const
		{
			return buf + len;
		}

		// Write to buffered memory.
		mem_view& append(const T* s, DWORD n)
		{
			if (len + n >= max_len) {
				n = n;
			}
			ensure(h && len + n < max_len);
			if (n) {
				std::copy(s, s + n, buf + len);
				len += n;
			}

			return *this;
		}
		mem_view& append(const T* b, const T* e)
		{
			return append(b, (DWORD)(e - b));
		}
		mem_view& append(T t)
		{
			return append(&t, 1);
		}
	};

	// class alocator...

} // namespace Win