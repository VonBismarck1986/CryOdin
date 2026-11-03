#pragma once

#include "ICryOdinTypes.h"
#include "ICryOdinAudioEffect.h"

#include <CryEntitySystem/IEntityBasicTypes.h>

namespace Cry
{
	namespace Odin
	{
		//! The voice of one user in one room. Get it from ICryOdinUser::GetSound.
		//! Call from the main thread only. Valid until OnPeerLeft has returned, or for the local user's
		//! voice until SetLocalLoopback(false).
		struct ICryOdinSound
		{
			virtual ~ICryOdinSound() {}

			//! The miniaudio sound of this voice. Include <miniaudio.h> to use it.
			virtual ma_sound* GetMASoundHandle() = 0;
			//! The Odin decoder of this voice. Include <odin.h> to use it, for example to add pipeline effects.
			virtual OdinDecoder* GetDecoder() = 0;
			//! The peer id of the user this voice belongs to.
			virtual uint32 GetSoundId() const = 0;

			virtual void StartSound() = 0;
			virtual void StopSound() = 0;

			virtual void SetSoundVolume(float fAmount) = 0;
			virtual float GetSoundVolume() const = 0;

			virtual void SetMuted(bool bMuted) = 0;
			virtual bool IsMuted() const = 0;

			virtual void SetSettings(const SCryOdinVoiceSettings& settings) = 0;
			virtual const SCryOdinVoiceSettings& GetSettings() const = 0;

			//! When true, ICryOdinRoom::SetDefaultVoiceSettings doesn't change this voice.
			//! CCryOdinComponentSound sets it on the voice it's bound to.
			virtual void SetIgnoreRoomDefaults(bool bIgnore) = 0;

			//! Where the voice comes from when it's 3D.
			virtual void SetTransform(const Vec3& position, const Vec3& forward) = 0;
			virtual Vec3 GetPosition() const = 0;

			virtual void SetRadioEnabled(bool bEnabled) = 0;
			virtual bool IsRadioEnabled() const = 0;
			virtual void SetRadioSettings(const SCryOdinRadioSettings& settings) = 0;

			//! Occlusion from 0 (clear) to 1 (blocked), found by ray casts when enabled in the settings.
			virtual float GetOcclusion() const = 0;
			//! Sets the occlusion yourself, whatever the settings say, and stops the ray casts. A negative value goes back to normal.
			virtual void SetOcclusionOverride(float fOcclusion) = 0;
			//! The ray casts ignore this entity, normally the speaker's own.
			virtual void SetOcclusionIgnoreEntity(EntityId entityId) = 0;

			//! Effects run in the order added, on the stereo voice after the radio and spatializer, and before occlusion and volume.
			virtual void AddEffect(const ICryOdinAudioEffectPtr& pEffect) = 0;
			virtual void RemoveEffect(const ICryOdinAudioEffectPtr& pEffect) = 0;

			//! True while the decoder has voice to play.
			virtual bool IsTalking() const = 0;
			//! Current output level from 0 to 1, for a voice meter.
			virtual float GetLevel() const = 0;
		};
	}
}
