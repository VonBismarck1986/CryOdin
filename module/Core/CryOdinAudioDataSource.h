#pragma once

#include <miniaudio.h>
#include <odin.h>
#include <atomic>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinSound;

		// A never ending miniaudio data source reading a mono Odin decoder.
		// ma_data_source_base must stay the first member, miniaudio casts to it.
		struct SCryOdinDataSource
		{
			ma_data_source_base base;
			OdinDecoder* pDecoder = nullptr;
			CCryOdinSound* pSound = nullptr;
			std::atomic<bool> bSilent{ true };
		};

		ma_result odin_data_source_init(OdinDecoder* pDecoder, CCryOdinSound* pSound, SCryOdinDataSource* pDataSource);
		void odin_data_source_uninit(SCryOdinDataSource* pDataSource);
	}
}
