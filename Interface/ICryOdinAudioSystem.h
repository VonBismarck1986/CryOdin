#pragma once

#include "ICryOdinTypes.h"
#include "ICryOdinAudioDevices.h"

#include <CryEntitySystem/IEntityBasicTypes.h>

namespace Cry
{
	namespace Odin
	{
		//! Global voice audio settings. Call from the main thread only.
		struct ICryOdinAudioSystem
		{
			virtual ~ICryOdinAudioSystem() {}

			//! The miniaudio engine playing all voices. Include <miniaudio.h> to use it.
			virtual ma_engine* GetAudioEngine() = 0;
			virtual ICryOdinAudioDevice* GetDevices() = 0;

			//! Volume of all voices.
			virtual void SetMasterVolume(float fVolume) = 0;
			virtual float GetMasterVolume() const = 0;

			//! Stops sending the microphone to every room.
			virtual void SetMicrophoneMuted(bool bMuted) = 0;
			virtual bool IsMicrophoneMuted() const = 0;

			//! Linear gain on the microphone before any processing.
			virtual void SetMicrophoneGain(float fGain) = 0;
			virtual float GetMicrophoneGain() const = 0;

			//! Levels from 0 to 1, for voice meters.
			virtual float GetInputLevel() const = 0;
			virtual float GetOutputLevel() const = 0;

			//! Places the listener. Call each frame, or use CCryOdinComponentListener.
			//! When not called for a frame, the listener follows the view camera.
			//! @param ignoreEntity Ignored by occlusion ray casts, normally the player.
			virtual void SetListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up, EntityId ignoreEntity = INVALID_ENTITYID) = 0;
			virtual Vec3 GetListenerPosition() const = 0;
		};
	}
}
