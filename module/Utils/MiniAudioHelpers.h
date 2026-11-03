#pragma once
#include "CryMemory/CryMemoryManager.h"

namespace Cry::Odin
{
	namespace Utils
	{
		struct Allocator
		{
			static void* Alloc(size_t sz, void* pUserData) {
				return CryModuleCRTMalloc(sz);
			}

			static void DeAlloc(void* p, void* pUserData) {
				CryModuleCRTFree(p);
			}

			static void* ReAlloc(void* p, size_t sz, void* pUserData) {
				return CryModuleCRTRealloc(p, sz);
			}
		};
	}
}