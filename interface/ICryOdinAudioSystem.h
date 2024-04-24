#pragma once

#include <miniaudio.h>
#include <odin.h>

namespace Cry
{
	namespace Odin
	{
		struct ICryOdinUser;
		class CCryOdinUserComponent;

		struct ICryOdinAudioSystem
		{
			virtual ~ICryOdinAudioSystem() {}

			virtual bool InitAudioSystem() = 0;
			virtual void ShutdownAudioSystem() = 0;
			virtual ma_engine* GetAudioEngine() = 0;

			virtual void CreateAudioObject(const CCryOdinUserComponent& ref) = 0;
			virtual void DestroyAudioObject(const CCryOdinUserComponent& ref) = 0;

			virtual void OnUpdate(float const frameTime) = 0;

		};


	}
}