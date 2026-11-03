#pragma once
#include <cstdlib>
#include <cmath>

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
		// The format all voice audio is processed in.
		constexpr uint32 kOdinSampleRate = 48000;
		constexpr uint32 kOdinOutputChannels = 2;

		// A NaN or infinite position aborts the process inside the Odin SDK.
		inline bool IsFiniteVec3(const Vec3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}
	}
}
