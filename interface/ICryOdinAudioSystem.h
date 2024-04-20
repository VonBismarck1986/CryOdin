#pragma once

#include <CryAction.h>
#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{

		struct SCryOdinAudioDevicesConfig;

		struct ICryOdinAudioSystem
		{
			virtual ~ICryOdinAudioSystem() {}

			virtual bool Init() = 0;
			virtual void Shutdown() = 0;
			virtual void OnUpdate(float frameTime) = 0;

			virtual ma_engine* GetAudioEngine() = 0;

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const = 0;

			virtual void CreateSoundObject(const IUser& user) = 0;
		};
	}
}