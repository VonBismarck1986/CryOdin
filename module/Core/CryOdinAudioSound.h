#pragma once

#include <ICryOdinAudioSound.h>
#include <miniaudio.h>

#include "CryOdinAudioDataSource.h"
#include "CryOdinAudioEffects.h"
#include "CryOdinAudioHrtf.h"
#include "CryOdinAudioReverb.h"
#include "Utils/OdinHelpers.h"
#include "Utils/OdinRealtime.h"

#include <atomic>
#include <memory>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioSystem;
		class CCryOdinSound;

		// ma_node_base must stay the first member, miniaudio casts to it.
		struct SCryOdinVoiceNode
		{
			ma_node_base base;
			CCryOdinSound* pSound = nullptr;
		};

		// The listener as each voice needs it, with a right vector that matches miniaudio's panning.
		struct SCryOdinListenerFrame
		{
			Vec3 position = ZERO;
			Vec3 right = Vec3(1.f, 0.f, 0.f);
			Vec3 forward = Vec3(0.f, 1.f, 0.f);
			Vec3 up = Vec3(0.f, 0.f, 1.f);
		};

		// Owns the decoder for as long as a voice or the network thread needs it.
		struct SCryOdinDecoder
		{
			explicit SCryOdinDecoder(OdinDecoder* pHandle) : pDecoder(pHandle) {}
			~SCryOdinDecoder() { odin_decoder_free(pDecoder); }
			DISABLE_COPY_AND_MOVE(SCryOdinDecoder)

			OdinDecoder* pDecoder;
		};

		class CCryOdinSound final : public ICryOdinSound
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinSound)
			CCryOdinSound(CCryOdinAudioSystem& audioSystem, std::shared_ptr<SCryOdinDecoder> pDecoder, uint32 peerId, const SCryOdinVoiceSettings& settings);
			virtual ~CCryOdinSound();

			bool IsValid() const { return m_bSoundReady; }

			// ICryOdinSound
			virtual ma_sound* GetMASoundHandle() override { return &m_sound; }
			virtual OdinDecoder* GetDecoder() override { return m_pDecoder->pDecoder; }
			virtual uint32 GetSoundId() const override { return m_id; }

			virtual void StartSound() override;
			virtual void StopSound() override;

			virtual void SetSoundVolume(float fAmount) override;
			virtual float GetSoundVolume() const override { return m_settings.fVolume; }

			virtual void SetMuted(bool bMuted) override;
			virtual bool IsMuted() const override { return m_settings.bMuted; }

			virtual void SetSettings(const SCryOdinVoiceSettings& settings) override;
			virtual const SCryOdinVoiceSettings& GetSettings() const override { return m_settings; }

			virtual void SetIgnoreRoomDefaults(bool bIgnore) override { m_bIgnoreRoomDefaults = bIgnore; }

			virtual void SetTransform(const Vec3& position, const Vec3& forward) override;
			virtual Vec3 GetPosition() const override { return m_position; }

			virtual void SetRadioEnabled(bool bEnabled) override;
			virtual bool IsRadioEnabled() const override { return m_settings.radio.bEnabled; }
			virtual void SetRadioSettings(const SCryOdinRadioSettings& settings) override;

			virtual float GetOcclusion() const override { return m_fOcclusionTarget.load(std::memory_order_relaxed); }
			virtual void SetOcclusionOverride(float fOcclusion) override;
			virtual void SetOcclusionIgnoreEntity(EntityId entityId) override { m_occlusionIgnoreEntity = entityId; }

			virtual void AddEffect(const ICryOdinAudioEffectPtr& pEffect) override;
			virtual void RemoveEffect(const ICryOdinAudioEffectPtr& pEffect) override;

			virtual bool IsTalking() const override { return IsSourceTalking(); }
			virtual float GetLevel() const override { return m_fLevel.load(std::memory_order_relaxed); }
			// ~ICryOdinSound

			bool IsIgnoringRoomDefaults() const { return m_bIgnoreRoomDefaults; }

			// Main thread, each update: distance, speaker direction, the direction for HRTF, and how much
			// of the voice goes to each reverb slot.
			void UpdateSpatial(const SCryOdinListenerFrame& listener, bool bHrtfWanted, const float (&reverbSends)[kCryOdinReverbSlots]);
			// A 3D voice whose Reverb Send isn't 0.
			bool WantsReverb() const;

			// Used by the occlusion system on the main thread
			bool WantsOcclusionRays() const;
			void SetOcclusionFromRays(float fOcclusion);
			EntityId GetOcclusionIgnoreEntity() const { return m_occlusionIgnoreEntity; }

			// Audio thread. The first gets the mono voice before the spatializer, the second the stereo result.
			// ppOut[0] is the voice, ppOut[1 + slot] the send to each reverb slot.
			void ProcessSource(float* pFrames, uint32 frameCount, bool bSilent);
			void ProcessAudio(const float* pIn, float** ppOut, uint32 frameCount);

		private:
			using TEffectList = std::vector<ICryOdinAudioEffectPtr>;

			// What UpdateSpatial found, for the audio thread.
			struct SSpatialState
			{
				bool bValid = false;              // A 3D voice stays silent until its first update, which knows the distance.
				float fDistanceGain = 1.f;
				float fBehind = 0.f;              // 0 facing the listener, 1 fully behind the speaker.
				Vec3 direction = Vec3(0.f, 1.f, 0.f); // Listener space: x right, y front, z up.
				bool bHrtf = false;
				float reverbSends[kCryOdinReverbSlots] = {};
			};

			void ApplySpatialSettings();
			void PublishSettings();
			void PublishEffects(std::shared_ptr<const TEffectList> pEffects);
			bool IsSourceTalking() const;
			void UpdateRealtimeSettings();

			CCryOdinAudioSystem& m_audioSystem;
			std::shared_ptr<SCryOdinDecoder> m_pDecoder;
			const uint32 m_id;

			SCryOdinDataSource m_dataSource;
			SCryOdinVoiceNode m_node;
			ma_sound m_sound;
			bool m_bDataSourceReady = false;
			bool m_bNodeReady = false;
			bool m_bSoundReady = false;

			// Main thread state
			SCryOdinVoiceSettings m_settings;
			Vec3 m_position = ZERO;
			Vec3 m_forward = ZERO;
			bool m_bHrtfApplied = false; // miniaudio's panning is off while HRTF places the voice.
			EntityId m_occlusionIgnoreEntity = INVALID_ENTITYID;
			bool m_bHasOcclusionOverride = false;
			bool m_bIgnoreRoomDefaults = false;

			// Shared with the audio thread
			TCryOdinRtMailbox<SCryOdinVoiceSettings> m_settingsMailbox;
			TCryOdinRtMailbox<SSpatialState> m_spatialMailbox;
			CCryOdinRtSection m_rtSection;
			std::shared_ptr<const TEffectList> m_pEffects;
			std::atomic<float> m_fOcclusionTarget{ 0.f };
			std::atomic<float> m_fLevel{ 0.f };
			std::atomic<bool> m_bStopped{ false };

			// Audio thread state
			SCryOdinVoiceSettings m_rtSettings;
			uint32 m_settingsVersion = 0;
			CCryOdinRadioEffect m_radio;
			CCryOdinOcclusionFilter m_occlusionFilter;
			CCryOdinOcclusionFilter m_behindFilter;
			CCryOdinHrtf m_hrtf;
			SSpatialState m_rtSpatial;
			uint32 m_spatialVersion = 0;
			bool m_bRtHrtf = false;
			float m_fCurrentGain = 0.f;
			float m_fCurrentOcclusion = 0.f;
			float m_fCurrentBehind = 0.f;
			float m_fCurrentDistanceGain = 0.f;
			float m_fCurrentSends[kCryOdinReverbSlots] = {};
		};
	}
}
