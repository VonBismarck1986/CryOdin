#pragma once

#include <miniaudio.h>
#include <odin.h>


namespace Cry
{
	namespace Odin
	{
		struct OdinDataSource;

		struct ICryOdinAudioSound
		{
			virtual ~ICryOdinAudioSound() {}

			virtual ma_sound* GetSound() = 0;
			virtual void SetSoundVolume(float fAmount) = 0;
			virtual float GetSoundVolume() const = 0;
			virtual void OnUpdate(float const fFrameTime) = 0;
			virtual OdinMediaStreamHandle GetMediaHandle() const = 0;
			virtual uint64_t GetSoundID() const = 0;

			virtual void InitSound(ma_engine* engine, OdinDataSource* pDataSource) = 0;
			virtual void PlaySound() = 0;
			virtual void StopSound() = 0;
		};

		using Sounds = std::vector<ICryOdinAudioSound*>;
	}
}