#include "StdAfx.h"
#include "CryOdinAudioSound.h"

#include "CryOdinAudioSystem.h"
#include "CryOdinAudioOcclusion.h"

#include <cmath>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// Occlusion changes over about a quarter of a second.
			constexpr float kOcclusionSmoothing = 0.00008f;
			constexpr float kMaxVoiceGain = 16.f;

			inline bool IsSpatial(const SCryOdinVoiceSettings& settings)
			{
				return settings.positioning == ECryOdinVoicePositioning::Spatial3D
					&& !(settings.radio.bEnabled && settings.radio.bDisableSpatialization);
			}

			// A source straight ahead of a stereo listener is reduced by the directional panning.
			// This brings it back to the level of a 2D voice.
			inline float GetSpatialMakeupGain(const SCryOdinVoiceSettings& settings)
			{
				const float fDirectional = crymath::clamp(settings.fDirectional, 0.f, 1.f);
				return 1.f / ((1.f + (1.f - fDirectional) + fDirectional * 0.7071f) * 0.5f);
			}

			inline float SanitizeFloat(float fValue, float fDefault, float fMin, float fMax)
			{
				return std::isfinite(fValue) ? crymath::clamp(fValue, fMin, fMax) : fDefault;
			}

			// These values come from properties and game code, and all reach the audio mix.
			// A preset fills in the radio values, the switches stay as they were set.
			SCryOdinRadioSettings SanitizeRadio(const SCryOdinRadioSettings& settings)
			{
				SCryOdinRadioSettings radio = settings;
				if (settings.preset != ECryOdinRadioPreset::Custom)
				{
					radio = GetCryOdinRadioPreset(settings.preset);
					radio.bEnabled = settings.bEnabled;
					radio.bDisableSpatialization = settings.bDisableSpatialization;
				}

				const SCryOdinRadioSettings defaults;
				radio.fHighPassHz = SanitizeFloat(radio.fHighPassHz, defaults.fHighPassHz, 20.f, 20000.f);
				radio.fLowPassHz = SanitizeFloat(radio.fLowPassHz, defaults.fLowPassHz, 20.f, 20000.f);
				radio.fDrive = SanitizeFloat(radio.fDrive, defaults.fDrive, 0.f, 1.f);
				radio.fNoiseLevel = SanitizeFloat(radio.fNoiseLevel, defaults.fNoiseLevel, 0.f, 1.f);
				radio.fSquelchLevel = SanitizeFloat(radio.fSquelchLevel, defaults.fSquelchLevel, 0.f, 1.f);
				radio.fSquelchMs = SanitizeFloat(radio.fSquelchMs, defaults.fSquelchMs, 0.f, 2000.f);
				radio.iBitDepth = crymath::clamp(radio.iBitDepth, 0, 16);
				radio.iSampleHold = crymath::clamp(radio.iSampleHold, 1, 64);
				radio.fDropoutChance = SanitizeFloat(radio.fDropoutChance, defaults.fDropoutChance, 0.f, 1.f);
				radio.fWetMix = SanitizeFloat(radio.fWetMix, defaults.fWetMix, 0.f, 1.f);
				radio.fOutputGain = SanitizeFloat(radio.fOutputGain, defaults.fOutputGain, 0.f, kMaxVoiceGain);
				return radio;
			}

			SCryOdinVoiceSettings SanitizeVoice(const SCryOdinVoiceSettings& settings)
			{
				const SCryOdinVoiceSettings defaults;
				SCryOdinVoiceSettings voice = settings;
				voice.fVolume = SanitizeFloat(voice.fVolume, defaults.fVolume, 0.f, kMaxVoiceGain);
				voice.fMinDistance = SanitizeFloat(voice.fMinDistance, defaults.fMinDistance, 0.01f, 100000.f);
				voice.fMaxDistance = SanitizeFloat(voice.fMaxDistance, defaults.fMaxDistance, voice.fMinDistance, 100000.f);
				voice.fRolloff = SanitizeFloat(voice.fRolloff, defaults.fRolloff, 0.f, 100.f);
				voice.fDirectional = SanitizeFloat(voice.fDirectional, defaults.fDirectional, 0.f, 1.f);
				voice.fOcclusionLowPassHz = SanitizeFloat(voice.fOcclusionLowPassHz, defaults.fOcclusionLowPassHz, 20.f, 20000.f);
				voice.fOcclusionAttenuation = SanitizeFloat(voice.fOcclusionAttenuation, defaults.fOcclusionAttenuation, 0.f, 1.f);
				voice.fInnerAngle = SanitizeFloat(voice.fInnerAngle, defaults.fInnerAngle, 0.f, 360.f);
				voice.fOuterAngle = SanitizeFloat(voice.fOuterAngle, defaults.fOuterAngle, voice.fInnerAngle, 360.f);
				voice.fBehindVolume = SanitizeFloat(voice.fBehindVolume, defaults.fBehindVolume, 0.f, 1.f);
				voice.fBehindLowPassHz = SanitizeFloat(voice.fBehindLowPassHz, defaults.fBehindLowPassHz, 20.f, 20000.f);
				voice.fReverbSend = SanitizeFloat(voice.fReverbSend, defaults.fReverbSend, 0.f, 2.f);
				voice.radio = SanitizeRadio(voice.radio);
				return voice;
			}

			// The same curves as miniaudio's, which now only pans. Done here so HRTF and the reverb can use them.
			float GetDistanceGain(const SCryOdinVoiceSettings& settings, float fDistance)
			{
				const float fMin = settings.fMinDistance;
				const float fMax = settings.fMaxDistance;
				if (settings.attenuation == ECryOdinAttenuationModel::None || fMin >= fMax)
				{
					return 1.f;
				}

				const float fClamped = crymath::clamp(fDistance, fMin, fMax);
				float fGain = 1.f;
				switch (settings.attenuation)
				{
				case ECryOdinAttenuationModel::Inverse:
					fGain = fMin / (fMin + settings.fRolloff * (fClamped - fMin));
					break;
				case ECryOdinAttenuationModel::Linear:
					fGain = 1.f - settings.fRolloff * (fClamped - fMin) / (fMax - fMin);
					break;
				case ECryOdinAttenuationModel::Exponential:
					fGain = powf(fClamped / fMin, -settings.fRolloff);
					break;
				default:
					break;
				}
				return crymath::clamp(fGain, 0.f, 1.f);
			}

			// 0 inside the inner angle, 1 outside the outer one. The angles are full widths, as in miniaudio.
			float GetBehindAmount(const SCryOdinVoiceSettings& settings, const Vec3& speakerForward, const Vec3& toListener)
			{
				if (!settings.bSpeakerDirection || speakerForward.GetLengthSquared() < 1e-6f)
				{
					return 0.f;
				}

				const float fCos = crymath::clamp(speakerForward.GetNormalized().Dot(toListener), -1.f, 1.f);
				const float fAngle = acosf(fCos) * (180.f / 3.14159265f);
				const float fInner = settings.fInnerAngle * 0.5f;
				const float fOuter = settings.fOuterAngle * 0.5f;
				if (fAngle <= fInner)
				{
					return 0.f;
				}
				if (fAngle >= fOuter || fOuter - fInner < 1e-3f)
				{
					return 1.f;
				}
				return (fAngle - fInner) / (fOuter - fInner);
			}

			void VoiceNodeProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
			{
				SCryOdinVoiceNode* pVoiceNode = static_cast<SCryOdinVoiceNode*>(pNode);
				pVoiceNode->pSound->ProcessAudio(ppFramesIn[0], ppFramesOut, *pFrameCountOut);
			}

			constexpr ma_uint32 kVoiceOutputBuses = 1 + kCryOdinReverbSlots;

			ma_node_vtable g_voiceNodeVtable =
			{
				VoiceNodeProcess,
				nullptr,           // onGetRequiredInputFrameCount
				1,                 // Input buses
				kVoiceOutputBuses, // The voice, then a send to each reverb slot
				0                  // Flags
			};
		}

		CCryOdinSound::CCryOdinSound(CCryOdinAudioSystem& audioSystem, std::shared_ptr<SCryOdinDecoder> pDecoder, uint32 peerId, const SCryOdinVoiceSettings& settings)
			: m_audioSystem(audioSystem)
			, m_pDecoder(std::move(pDecoder))
			, m_id(peerId)
			, m_settings(SanitizeVoice(settings))
		{
			memset(&m_sound, 0, sizeof(m_sound));
			memset(&m_node.base, 0, sizeof(m_node.base));
			m_node.pSound = this;

			ma_engine* pEngine = m_audioSystem.GetAudioEngine();
			const ma_allocation_callbacks* pAllocation = &m_audioSystem.GetAllocationCallbacks();

			// Set up everything the audio thread uses before the node is connected.
			m_rtSettings = m_settings;
			m_radio.Init(pAllocation);
			m_radio.Configure(m_rtSettings.radio);
			m_occlusionFilter.Init(pAllocation);
			m_behindFilter.Init(pAllocation);

			ma_result result = odin_data_source_init(m_pDecoder->pDecoder, this, &m_dataSource);
			m_bDataSourceReady = result == MA_SUCCESS;
			if (!m_bDataSourceReady)
			{
				ODIN_WARNING("Failed to create the data source of voice %u; %s", m_id, ma_result_description(result));
				return;
			}

			const ma_uint32 inputChannels[1] = { kOdinOutputChannels };
			ma_uint32 outputChannels[kVoiceOutputBuses];
			for (ma_uint32& channelCount : outputChannels)
			{
				channelCount = kOdinOutputChannels;
			}
			ma_node_config nodeConfig = ma_node_config_init();
			nodeConfig.vtable = &g_voiceNodeVtable;
			nodeConfig.pInputChannels = inputChannels;
			nodeConfig.pOutputChannels = outputChannels;

			result = ma_node_init(ma_engine_get_node_graph(pEngine), &nodeConfig, pAllocation, &m_node);
			m_bNodeReady = result == MA_SUCCESS;
			if (!m_bNodeReady)
			{
				ODIN_WARNING("Failed to create the effect node of voice %u; %s", m_id, ma_result_description(result));
				return;
			}
			ma_node_attach_output_bus(&m_node, 0, ma_engine_get_endpoint(pEngine), 0);
			for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
			{
				if (ma_node* pReverb = m_audioSystem.GetReverbSlotNode(slot))
				{
					ma_node_attach_output_bus(&m_node, static_cast<ma_uint32>(1 + slot), pReverb, 0);
				}
			}

			// Without pitch the decoder is read at exactly real time. A live voice can't be sped up
			// or slowed down, which rules out pitch and doppler.
			ma_sound_config soundConfig = ma_sound_config_init_2(pEngine);
			soundConfig.pDataSource = &m_dataSource;
			soundConfig.pInitialAttachment = &m_node;
			soundConfig.initialAttachmentInputBusIndex = 0;
			soundConfig.flags = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_PITCH;

			result = ma_sound_init_ex(pEngine, &soundConfig, &m_sound);
			m_bSoundReady = result == MA_SUCCESS;
			if (!m_bSoundReady)
			{
				ODIN_WARNING("Failed to create the sound of voice %u; %s", m_id, ma_result_description(result));
				return;
			}

			ApplySpatialSettings();

			m_audioSystem.GetOcclusion().AddSound(this);
			m_audioSystem.RegisterSound(this);

			StartSound();
		}

		CCryOdinSound::~CCryOdinSound()
		{
			m_audioSystem.GetOcclusion().RemoveSound(this);
			m_audioSystem.UnregisterSound(this);

			const ma_allocation_callbacks* pAllocation = &m_audioSystem.GetAllocationCallbacks();

			// These calls wait for the audio thread. A sound must go before the node it's attached to.
			if (m_bSoundReady)
			{
				ma_sound_uninit(&m_sound);
			}
			if (m_bNodeReady)
			{
				ma_node_uninit(&m_node, pAllocation);
			}
			if (m_bDataSourceReady)
			{
				odin_data_source_uninit(&m_dataSource);
			}

			m_occlusionFilter.Uninit(pAllocation);
			m_behindFilter.Uninit(pAllocation);
			m_radio.Uninit(pAllocation);
		}

		void CCryOdinSound::StartSound()
		{
			if (m_bSoundReady)
			{
				m_bStopped.store(false, std::memory_order_relaxed);
				ma_sound_start(&m_sound);
			}
		}

		void CCryOdinSound::StopSound()
		{
			if (m_bSoundReady)
			{
				// The decoder isn't read while stopped, so its last state would stay forever.
				m_bStopped.store(true, std::memory_order_relaxed);
				ma_sound_stop(&m_sound);
			}
		}

		bool CCryOdinSound::IsSourceTalking() const
		{
			return !m_bStopped.load(std::memory_order_relaxed) && !m_dataSource.bSilent.load(std::memory_order_relaxed);
		}

		void CCryOdinSound::PublishSettings()
		{
			m_settingsMailbox.Write(m_settings);
		}

		void CCryOdinSound::SetSoundVolume(float fAmount)
		{
			m_settings.fVolume = SanitizeFloat(fAmount, m_settings.fVolume, 0.f, kMaxVoiceGain);
			PublishSettings();
		}

		void CCryOdinSound::SetMuted(bool bMuted)
		{
			m_settings.bMuted = bMuted;
			PublishSettings();
		}

		void CCryOdinSound::SetSettings(const SCryOdinVoiceSettings& settings)
		{
			const SCryOdinVoiceSettings sanitized = SanitizeVoice(settings);
			if (sanitized == m_settings)
			{
				return;
			}
			m_settings = sanitized;
			ApplySpatialSettings();
			PublishSettings();
		}

		void CCryOdinSound::SetTransform(const Vec3& position, const Vec3& forward)
		{
			if (!m_bSoundReady || !IsFiniteVec3(position))
			{
				return;
			}

			m_position = position;
			ma_sound_set_position(&m_sound, position.x, position.y, position.z);
			if (IsFiniteVec3(forward))
			{
				m_forward = forward;
				ma_sound_set_direction(&m_sound, forward.x, forward.y, forward.z);
			}
		}

		bool CCryOdinSound::WantsReverb() const
		{
			return m_bSoundReady && IsSpatial(m_settings) && m_settings.fReverbSend > 0.f;
		}

		void CCryOdinSound::UpdateSpatial(const SCryOdinListenerFrame& listener, bool bHrtfWanted, const float (&reverbSends)[kCryOdinReverbSlots])
		{
			if (!m_bSoundReady)
			{
				return;
			}

			SSpatialState state;
			state.bValid = true;
			const bool bSpatial = IsSpatial(m_settings);
			if (bSpatial)
			{
				const Vec3 offset = m_position - listener.position;
				const float fDistance = offset.GetLength();
				state.fDistanceGain = GetDistanceGain(m_settings, fDistance);

				if (fDistance > 1e-4f)
				{
					const Vec3 toSource = offset / fDistance;
					state.direction = Vec3(toSource.Dot(listener.right), toSource.Dot(listener.forward), toSource.Dot(listener.up));
					state.fBehind = GetBehindAmount(m_settings, m_forward, -toSource);
				}
				state.bHrtf = bHrtfWanted && m_settings.bHrtf;

				// The reverb falls off more slowly than the voice, so a far voice sounds farther away.
				const float fSendScale = m_settings.fReverbSend * sqrtf(state.fDistanceGain);
				for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
				{
					state.reverbSends[slot] = reverbSends[slot] * fSendScale;
				}
			}

			// HRTF places the voice itself, so miniaudio must stop panning it.
			if (state.bHrtf != m_bHrtfApplied)
			{
				m_bHrtfApplied = state.bHrtf;
				ApplySpatialSettings();
			}

			m_spatialMailbox.Write(state);
		}

		void CCryOdinSound::SetRadioEnabled(bool bEnabled)
		{
			if (m_settings.radio.bEnabled != bEnabled)
			{
				m_settings.radio.bEnabled = bEnabled;
				ApplySpatialSettings();
				PublishSettings();
			}
		}

		void CCryOdinSound::SetRadioSettings(const SCryOdinRadioSettings& settings)
		{
			m_settings.radio = SanitizeRadio(settings);
			ApplySpatialSettings();
			PublishSettings();
		}

		void CCryOdinSound::SetOcclusionOverride(float fOcclusion)
		{
			m_bHasOcclusionOverride = fOcclusion >= 0.f;
			if (m_bHasOcclusionOverride)
			{
				m_fOcclusionTarget.store(crymath::clamp(fOcclusion, 0.f, 1.f), std::memory_order_relaxed);
			}
			else if (!WantsOcclusionRays())
			{
				m_fOcclusionTarget.store(0.f, std::memory_order_relaxed);
			}
		}

		bool CCryOdinSound::WantsOcclusionRays() const
		{
			return m_settings.bOcclusion && !m_bHasOcclusionOverride && IsSpatial(m_settings);
		}

		void CCryOdinSound::SetOcclusionFromRays(float fOcclusion)
		{
			if (!m_bHasOcclusionOverride)
			{
				m_fOcclusionTarget.store(crymath::clamp(fOcclusion, 0.f, 1.f), std::memory_order_relaxed);
			}
		}

		void CCryOdinSound::AddEffect(const ICryOdinAudioEffectPtr& pEffect)
		{
			if (!pEffect)
			{
				return;
			}

			auto pNewList = std::make_shared<TEffectList>();
			if (const std::shared_ptr<const TEffectList> pOldList = std::atomic_load(&m_pEffects))
			{
				*pNewList = *pOldList;
			}
			pNewList->push_back(pEffect);
			PublishEffects(std::move(pNewList));
		}

		void CCryOdinSound::RemoveEffect(const ICryOdinAudioEffectPtr& pEffect)
		{
			const std::shared_ptr<const TEffectList> pOldList = std::atomic_load(&m_pEffects);
			if (!pOldList)
			{
				return;
			}

			auto pNewList = std::make_shared<TEffectList>(*pOldList);
			pNewList->erase(std::remove(pNewList->begin(), pNewList->end(), pEffect), pNewList->end());
			PublishEffects(std::move(pNewList));
		}

		void CCryOdinSound::PublishEffects(std::shared_ptr<const TEffectList> pEffects)
		{
			// Hold the old list past the wait. The audio thread must not be the one to delete it.
			const std::shared_ptr<const TEffectList> pOldEffects = std::atomic_exchange(&m_pEffects, std::move(pEffects));
			m_rtSection.WaitIdle();
		}

		void CCryOdinSound::ApplySpatialSettings()
		{
			if (!m_bSoundReady)
			{
				return;
			}

			// All of these are atomic in miniaudio. Volume and distance are applied in ProcessAudio, so
			// miniaudio only pans: a rolloff of 0 keeps panning with no attenuation, while its "none"
			// model would turn off panning too. With HRTF it doesn't pan either.
			ma_sound_set_spatialization_enabled(&m_sound, (IsSpatial(m_settings) && !m_bHrtfApplied) ? MA_TRUE : MA_FALSE);
			ma_sound_set_attenuation_model(&m_sound, ma_attenuation_model_inverse);
			ma_sound_set_min_distance(&m_sound, m_settings.fMinDistance);
			ma_sound_set_max_distance(&m_sound, m_settings.fMaxDistance);
			ma_sound_set_rolloff(&m_sound, 0.f);
			ma_sound_set_directional_attenuation_factor(&m_sound, m_settings.fDirectional);

			// Rays only keep this up to date while the voice wants them.
			if (!m_bHasOcclusionOverride && !WantsOcclusionRays())
			{
				m_fOcclusionTarget.store(0.f, std::memory_order_relaxed);
			}
		}

		void CCryOdinSound::UpdateRealtimeSettings()
		{
			const bool bWasRadioEnabled = m_rtSettings.radio.bEnabled;
			SCryOdinRadioSettings oldRadio = m_rtSettings.radio;

			if (m_settingsMailbox.TryRead(m_rtSettings, m_settingsVersion))
			{
				if (!(m_rtSettings.radio == oldRadio))
				{
					m_radio.Configure(m_rtSettings.radio);
				}
				if (m_rtSettings.radio.bEnabled && !bWasRadioEnabled)
				{
					m_radio.Reset(IsSourceTalking());
				}
			}
		}

		void CCryOdinSound::ProcessSource(float* pFrames, uint32 frameCount, bool bSilent)
		{
			UpdateRealtimeSettings();

			if (m_rtSettings.radio.bEnabled)
			{
				m_radio.Process(pFrames, frameCount, !bSilent);
			}
		}

		void CCryOdinSound::ProcessAudio(const float* pIn, float** ppOut, uint32 frameCount)
		{
			CCryOdinRtScope rtScope(m_rtSection);

			UpdateRealtimeSettings();
			m_spatialMailbox.TryRead(m_rtSpatial, m_spatialVersion);

			const bool bSpatial = IsSpatial(m_rtSettings);
			const bool bHrtf = bSpatial && m_rtSpatial.bHrtf;
			float* pOut = ppOut[0];

			// The reverb sends: the mono voice, before speaker direction and occlusion, as a room is still
			// heard around a wall. Ramped, so a voice walking into a space doesn't click. Panning lowers the
			// sum of a voice in front, so it gets the same make up as the voice.
			const float fPanMakeup = (bSpatial && !bHrtf) ? GetSpatialMakeupGain(m_rtSettings) : 1.f;
			const float fSendVolume = (m_rtSettings.bMuted ? 0.f : m_rtSettings.fVolume) * fPanMakeup;
			const float fFrameStep = 1.f / static_cast<float>(std::max<uint32>(frameCount, 1));
			for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
			{
				float* pSend = ppOut[1 + slot];
				const float fTarget = bSpatial ? m_rtSpatial.reverbSends[slot] * fSendVolume : 0.f;
				const float fStart = m_fCurrentSends[slot];
				if (fTarget <= 0.f && fStart <= 0.f)
				{
					memset(pSend, 0, frameCount * kOdinOutputChannels * sizeof(float));
					continue;
				}
				for (uint32 frame = 0; frame < frameCount; ++frame)
				{
					const float fGain = fStart + (fTarget - fStart) * static_cast<float>(frame + 1) * fFrameStep;
					const float fMono = 0.5f * (pIn[frame * 2] + pIn[frame * 2 + 1]) * fGain;
					pSend[frame * 2] = fMono;
					pSend[frame * 2 + 1] = fMono;
				}
				m_fCurrentSends[slot] = fTarget;
			}

			if (bHrtf && !m_bRtHrtf)
			{
				m_hrtf.Reset();
			}
			m_bRtHrtf = bHrtf;

			if (bHrtf)
			{
				m_hrtf.Process(pIn, pOut, frameCount, m_rtSpatial.direction);
			}
			else
			{
				memcpy(pOut, pIn, frameCount * kOdinOutputChannels * sizeof(float));
			}

			// Distance before the effects, as miniaudio did when it attenuated, so gates and compressors
			// see the same level as before.
			const float fDistanceTarget = bSpatial ? (m_rtSpatial.bValid ? m_rtSpatial.fDistanceGain : 0.f) : 1.f;
			const float fDistanceStart = m_fCurrentDistanceGain;
			if (fDistanceTarget != 1.f || fDistanceStart != 1.f)
			{
				for (uint32 frame = 0; frame < frameCount; ++frame)
				{
					const float fDistanceGain = fDistanceStart + (fDistanceTarget - fDistanceStart) * static_cast<float>(frame + 1) * fFrameStep;
					pOut[frame * 2] *= fDistanceGain;
					pOut[frame * 2 + 1] *= fDistanceGain;
				}
			}
			m_fCurrentDistanceGain = fDistanceTarget;

			if (const std::shared_ptr<const TEffectList> pEffects = std::atomic_load(&m_pEffects))
			{
				const bool bSilent = !IsSourceTalking();
				for (const ICryOdinAudioEffectPtr& pEffect : *pEffects)
				{
					pEffect->Process(pOut, frameCount, kOdinOutputChannels, kOdinSampleRate, bSilent);
				}
			}

			const float fOcclusionTarget = m_fOcclusionTarget.load(std::memory_order_relaxed);
			const float fSmoothing = std::min(1.f, kOcclusionSmoothing * frameCount);
			m_fCurrentOcclusion += (fOcclusionTarget - m_fCurrentOcclusion) * fSmoothing;
			m_occlusionFilter.Process(pOut, frameCount, m_fCurrentOcclusion, m_rtSettings.fOcclusionLowPassHz);

			// Facing away from the listener, eased like occlusion.
			const float fBehindTarget = bSpatial ? m_rtSpatial.fBehind : 0.f;
			m_fCurrentBehind += (fBehindTarget - m_fCurrentBehind) * fSmoothing;
			m_behindFilter.Process(pOut, frameCount, m_fCurrentBehind, m_rtSettings.fBehindLowPassHz);

			float fTargetGain = m_rtSettings.bMuted ? 0.f : m_rtSettings.fVolume;
			fTargetGain *= 1.f - m_rtSettings.fOcclusionAttenuation * m_fCurrentOcclusion;
			if (bSpatial)
			{
				fTargetGain *= 1.f + (m_rtSettings.fBehindVolume - 1.f) * m_fCurrentBehind;
				// Panning lowers a voice in front, HRTF doesn't.
				fTargetGain *= fPanMakeup;
			}

			// Ramp the volume over the block, so that changes and mute don't click.
			const float fGainStep = (fTargetGain - m_fCurrentGain) / static_cast<float>(std::max<uint32>(frameCount, 1));
			float fGain = m_fCurrentGain;
			float fSum = 0.f;
			for (uint32 frame = 0; frame < frameCount; ++frame)
			{
				fGain += fGainStep;
				float* pFrame = pOut + frame * kOdinOutputChannels;
				pFrame[0] *= fGain;
				pFrame[1] *= fGain;
				fSum += pFrame[0] * pFrame[0] + pFrame[1] * pFrame[1];
			}
			m_fCurrentGain = fTargetGain;

			const float fRms = frameCount > 0 ? sqrtf(fSum / static_cast<float>(frameCount * kOdinOutputChannels)) : 0.f;
			m_fLevel.store(std::min(fRms, 1.f), std::memory_order_relaxed);
		}
	}
}
