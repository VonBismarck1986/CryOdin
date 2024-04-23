#pragma once
#include <cstdlib>


#define DISABLE_COPY(T)					\
		explicit T(const T&) = delete; \
		T& operator=(const T&) = delete; 


#define DISABLE_MOVE(T)				\
		explicit T(T&&) = delete;	\
		T& operator=(T&&) = delete;	

#define DISABLE_COPY_AND_MOVE(T) DISABLE_COPY(T) DISABLE_MOVE(T)




namespace Cry
{
	namespace Odin
	{

		static uint64_t convertOdinUserIDToUint64(const char* str) {
			char* endPtr; // Pointer to character immediately following the last valid character
			return std::strtoull(str, &endPtr, 10); // 10 indicates base 10 conversion
		}

	}
}