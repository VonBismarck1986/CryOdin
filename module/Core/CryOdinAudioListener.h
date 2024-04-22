#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioListener
		{
		public:
			CCryOdinAudioListener() = delete;
			virtual ~CCryOdinAudioListener() = default;

			explicit CCryOdinAudioListener(CryAudio::CTransformation const& tranform)
				: m_transform(tranform)
			{

			}

			void UpdateListener(ma_engine* engine, Vec3 position, Vec3 direction) {}

		protected:
			CryAudio::CTransformation m_transform;

		};
	}
}