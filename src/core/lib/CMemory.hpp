#ifndef HYPERION_V2_LIB_C_MEMORY_HPP
#define HYPERION_V2_LIB_C_MEMORY_HPP

#include <Types.hpp>

#include <cstring>

namespace hyperion
{

class Memory
{
public:
	static Int Compare(const void *lhs, const void *rhs, SizeType size)
	{
		return std::memcmp(lhs, rhs, size);
	}

	static char *CopyString(char *dest, const char *src, SizeType length = 0)
	{
		if (length)
			return strncpy(dest, src, length);
		return std::strcpy(dest, src);
	}

	static inline void *Set(void *dest, int ch, SizeType length)
	{
		return std::memset(dest, ch, length);
	}

	static inline void *Copy(void *dest, const void *src, SizeType size) 
	{
		return std::memcpy(dest, src, size);
	}
};

}



#endif