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
			virtual void SetMediaHandle(OdinMediaStreamHandle handleID) = 0;
			virtual uint64_t GetSoundID() const = 0;

			virtual void SetRoomHandle(OdinRoomHandle room) = 0;
			virtual OdinRoomHandle GetRoomHandle() = 0;

			virtual void InitSound(ma_engine* engine, OdinDataSource* pDataSource) = 0;
			virtual void PlaySound() = 0;
			virtual void StopSound() = 0;
		};

		using Sounds = std::vector<ICryOdinAudioSound*>;


		struct ICryOdinAudioListener
		{
			virtual ~ICryOdinAudioListener() {}

			virtual ICryOdinAudioListener& GetListener() = 0;

			virtual void UpdatePosition(Vec3 position) = 0;
			virtual Vec3 GetListenerPosition() = 0;

			virtual void UpdateDirection(Vec3 direction) = 0;
			virtual Vec3 GetListenerDirection() = 0;
		};
	}
}