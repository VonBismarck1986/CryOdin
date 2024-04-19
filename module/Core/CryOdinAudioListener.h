#pragma once
#include <miniaudio.h>
#include <odin.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

// Thinking on this... since we know we need only 1 listener maybe not create a whole for class to handle it idk

namespace Cry
{
	namespace Odin
	{

		class CCryOdinAudioListener final
		{
		public:
			CCryOdinAudioListener() = default;
			~CCryOdinAudioListener() = default;

			void CreateListener(ma_engine* engine, CryAudio::CTransformation const& transformation);
			void DestroyListener(ma_engine* engine, int listenerIndex);

			void SetOcclusion() {}

			void OnUpdate(float frameTime);
		private:

		};

	}
}