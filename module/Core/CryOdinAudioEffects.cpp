#include "StdAfx.h"
#include "CryOdinAudioEffects.h"

#include "Utils/OdinHelpers.h"

#include <cmath>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr uint32 kRadioFilterOrder = 4;
			constexpr uint32 kRadioChannels = 1;
			constexpr uint32 kEffectChunkFrames = 512;
			constexpr float kNoiseEnvelopeStep = 0.002f;
			constexpr float kOcclusionOpenCutoffHz = 20000.f;
			// How long the voice can be silent before it counts as the end of a transmission.
			constexpr uint32 kTransmissionHoldFrames = kOdinSampleRate / 4;
			// The level of normal speech, which the distortion keeps at about the same loudness.
			constexpr float kSpeechReferenceLevel = 0.1f;

			inline double ClampCutoff(float fHz)
			{
				return static_cast<double>(crymath::clamp(fHz, 20.f, kOdinSampleRate * 0.45f));
			}
		}

		bool CCryOdinRadioEffect::Init(const ma_allocation_callbacks* pAllocation)
		{
			const ma_hpf_config highPassConfig = ma_hpf_config_init(ma_format_f32, kRadioChannels, kOdinSampleRate, ClampCutoff(m_settings.fHighPassHz), kRadioFilterOrder);
			m_bHighPassReady = ma_hpf_init(&highPassConfig, pAllocation, &m_highPass) == MA_SUCCESS;

			const ma_lpf_config lowPassConfig = ma_lpf_config_init(ma_format_f32, kRadioChannels, kOdinSampleRate, ClampCutoff(m_settings.fLowPassHz), kRadioFilterOrder);
			m_bLowPassReady = ma_lpf_init(&lowPassConfig, pAllocation, &m_lowPass) == MA_SUCCESS;

			const ma_noise_config noiseConfig = ma_noise_config_init(ma_format_f32, kRadioChannels, ma_noise_type_pink, 4321, 1.0);
			m_bNoiseReady = ma_noise_init(&noiseConfig, pAllocation, &m_noise) == MA_SUCCESS;

			return m_bHighPassReady && m_bLowPassReady && m_bNoiseReady;
		}

		void CCryOdinRadioEffect::Uninit(const ma_allocation_callbacks* pAllocation)
		{
			if (m_bHighPassReady)
			{
				ma_hpf_uninit(&m_highPass, pAllocation);
				m_bHighPassReady = false;
			}
			if (m_bLowPassReady)
			{
				ma_lpf_uninit(&m_lowPass, pAllocation);
				m_bLowPassReady = false;
			}
			if (m_bNoiseReady)
			{
				ma_noise_uninit(&m_noise, pAllocation);
				m_bNoiseReady = false;
			}
		}

		void CCryOdinRadioEffect::Configure(const SCryOdinRadioSettings& settings)
		{
			m_settings = settings;

			// Reinit keeps the filter state and doesn't allocate, so it's safe here.
			if (m_bHighPassReady)
			{
				const ma_hpf_config config = ma_hpf_config_init(ma_format_f32, kRadioChannels, kOdinSampleRate, ClampCutoff(m_settings.fHighPassHz), kRadioFilterOrder);
				ma_hpf_reinit(&config, &m_highPass);
			}
			if (m_bLowPassReady)
			{
				const ma_lpf_config config = ma_lpf_config_init(ma_format_f32, kRadioChannels, kOdinSampleRate, ClampCutoff(m_settings.fLowPassHz), kRadioFilterOrder);
				ma_lpf_reinit(&config, &m_lowPass);
			}
		}

		void CCryOdinRadioEffect::Reset(bool bTalking)
		{
			m_bTransmitting = bTalking;
			m_holdFrames = bTalking ? kTransmissionHoldFrames : 0;
			m_fNoiseEnvelope = bTalking ? 1.f : 0.f;
			m_squelchFrames = 0;
			m_holdCounter = 0;
			m_fHeldSample = 0.f;
			m_dropoutFrames = 0;
			m_dropoutCheckFrames = 0;

			if (m_bHighPassReady)
			{
				for (ma_uint32 i = 0; i < m_highPass.hpf2Count; ++i)
				{
					ma_biquad_clear_cache(&m_highPass.pHPF2[i].bq);
				}
			}
			if (m_bLowPassReady)
			{
				ma_lpf_clear_cache(&m_lowPass);
			}
		}

		float CCryOdinRadioEffect::NextRandom()
		{
			// xorshift, cheap and without locks
			m_randomState ^= m_randomState << 13;
			m_randomState ^= m_randomState >> 17;
			m_randomState ^= m_randomState << 5;
			return static_cast<float>(m_randomState) / 4294967296.f;
		}

		void CCryOdinRadioEffect::Process(float* pFrames, uint32 frameCount, bool bTalking)
		{
			// One burst of static per transmission, not one per gap in the speech.
			if (bTalking)
			{
				m_holdFrames = kTransmissionHoldFrames;
			}
			else
			{
				m_holdFrames = m_holdFrames > frameCount ? m_holdFrames - frameCount : 0;
			}

			const bool bTransmitting = m_holdFrames > 0;
			if (bTransmitting != m_bTransmitting)
			{
				m_bTransmitting = bTransmitting;
				m_squelchLength = std::max<uint32>(1, static_cast<uint32>(m_settings.fSquelchMs * 0.001f * kOdinSampleRate));
				m_squelchFrames = m_settings.fSquelchLevel > 0.f ? m_squelchLength : 0;
			}

			uint32 framesLeft = frameCount;
			while (framesLeft > 0)
			{
				const uint32 chunk = std::min(framesLeft, kEffectChunkFrames);
				ProcessChunk(pFrames, chunk, bTransmitting);
				pFrames += chunk;
				framesLeft -= chunk;
			}
		}

		void CCryOdinRadioEffect::ProcessChunk(float* pFrames, uint32 frameCount, bool bTransmitting)
		{
			float dry[kEffectChunkFrames];
			float noise[kEffectChunkFrames];

			const bool bUseDry = m_settings.fWetMix < 1.f;
			if (bUseDry)
			{
				memcpy(dry, pFrames, frameCount * sizeof(float));
			}

			const bool bHasNoise = m_bNoiseReady && (m_settings.fNoiseLevel > 0.f || m_squelchFrames > 0 || m_fNoiseEnvelope > 0.f);
			if (bHasNoise)
			{
				ma_noise_read_pcm_frames(&m_noise, noise, frameCount, nullptr);
			}

			// Distortion, then static, then the band limits, which is the order on a real radio link.
			// The soft clip is scaled so that speech stays at about the same level, and blended in so
			// that a drive of 0 changes nothing. Turning the radio on doesn't make a voice much louder.
			const float fPreGain = 1.f + m_settings.fDrive * 9.f;
			const float fClipScale = (1.f + fPreGain * kSpeechReferenceLevel) / (1.f + fPreGain);
			const float fPostGain = ((1.f + fPreGain) / fPreGain) * fClipScale;
			const float fDriveMix = std::min(m_settings.fDrive * 4.f, 1.f);
			const float fTargetEnvelope = bTransmitting ? 1.f : 0.f;

			for (uint32 frame = 0; frame < frameCount; ++frame)
			{
				m_fNoiseEnvelope += crymath::clamp(fTargetEnvelope - m_fNoiseEnvelope, -kNoiseEnvelopeStep, kNoiseEnvelopeStep);

				float fNoiseGain = m_fNoiseEnvelope * m_settings.fNoiseLevel;
				if (m_squelchFrames > 0)
				{
					fNoiseGain += m_settings.fSquelchLevel * static_cast<float>(m_squelchFrames) / static_cast<float>(m_squelchLength);
					--m_squelchFrames;
				}

				const float fInput = pFrames[frame];
				const float fDriven = fInput * fPreGain;
				const float fClipped = (fDriven / (1.f + fabsf(fDriven))) * fPostGain;
				float fSample = fInput + (fClipped - fInput) * fDriveMix;
				if (bHasNoise)
				{
					fSample += noise[frame] * fNoiseGain;
				}
				pFrames[frame] = fSample;
			}

			if (m_bHighPassReady)
			{
				ma_hpf_process_pcm_frames(&m_highPass, pFrames, pFrames, frameCount);
			}
			if (m_bLowPassReady)
			{
				ma_lpf_process_pcm_frames(&m_lowPass, pFrames, pFrames, frameCount);
			}

			// Digital damage
			if (m_settings.iSampleHold > 1 || m_settings.iBitDepth > 0)
			{
				const float fSteps = m_settings.iBitDepth > 0 ? static_cast<float>(1 << (m_settings.iBitDepth - 1)) : 0.f;
				for (uint32 frame = 0; frame < frameCount; ++frame)
				{
					if (m_holdCounter == 0)
					{
						m_fHeldSample = pFrames[frame];
					}
					float fSample = m_settings.iSampleHold > 1 ? m_fHeldSample : pFrames[frame];
					if (fSteps > 0.f)
					{
						fSample = floorf(fSample * fSteps + 0.5f) / fSteps;
					}
					pFrames[frame] = fSample;
					m_holdCounter = (m_holdCounter + 1) % static_cast<uint32>(m_settings.iSampleHold);
				}
			}

			// Short signal losses
			for (uint32 frame = 0; frame < frameCount; ++frame)
			{
				if (m_settings.fDropoutChance > 0.f && m_dropoutCheckFrames == 0)
				{
					m_dropoutCheckFrames = kOdinSampleRate / 100;
					if (bTransmitting && m_dropoutFrames == 0 && NextRandom() < m_settings.fDropoutChance)
					{
						m_dropoutFrames = static_cast<uint32>((0.02f + NextRandom() * 0.06f) * kOdinSampleRate);
					}
				}
				if (m_dropoutCheckFrames > 0)
				{
					--m_dropoutCheckFrames;
				}

				const float fGain = m_dropoutFrames > 0 ? 0.f : m_settings.fOutputGain;
				if (m_dropoutFrames > 0)
				{
					--m_dropoutFrames;
				}

				float fSample = pFrames[frame] * fGain;
				if (bUseDry)
				{
					fSample = fSample * m_settings.fWetMix + dry[frame] * (1.f - m_settings.fWetMix);
				}
				pFrames[frame] = fSample;
			}
		}

		bool CCryOdinOcclusionFilter::Init(const ma_allocation_callbacks* pAllocation)
		{
			m_fCurrentCutoff = kOcclusionOpenCutoffHz;
			const ma_lpf_config config = ma_lpf_config_init(ma_format_f32, kOdinOutputChannels, kOdinSampleRate, m_fCurrentCutoff, 2);
			m_bReady = ma_lpf_init(&config, pAllocation, &m_lowPass) == MA_SUCCESS;
			return m_bReady;
		}

		void CCryOdinOcclusionFilter::Uninit(const ma_allocation_callbacks* pAllocation)
		{
			if (m_bReady)
			{
				ma_lpf_uninit(&m_lowPass, pAllocation);
				m_bReady = false;
			}
		}

		void CCryOdinOcclusionFilter::SetCutoff(float fCutoffHz)
		{
			// Limit the updates to changes that can be heard.
			if (fabsf(fCutoffHz - m_fCurrentCutoff) > m_fCurrentCutoff * 0.02f)
			{
				m_fCurrentCutoff = fCutoffHz;
				const ma_lpf_config config = ma_lpf_config_init(ma_format_f32, kOdinOutputChannels, kOdinSampleRate, m_fCurrentCutoff, 2);
				ma_lpf_reinit(&config, &m_lowPass);
			}
		}

		void CCryOdinOcclusionFilter::Process(float* pFrames, uint32 frameCount, float fAmount, float fLowPassHz)
		{
			if (!m_bReady)
			{
				return;
			}

			if (fAmount <= 0.001f)
			{
				// Keep the filter following the signal and throw its output away, so that it doesn't
				// click from old values when occlusion starts again.
				SetCutoff(kOcclusionOpenCutoffHz);

				float discard[kEffectChunkFrames * kOdinOutputChannels];
				uint32 framesLeft = frameCount;
				const float* pSource = pFrames;
				while (framesLeft > 0)
				{
					const uint32 chunk = std::min(framesLeft, kEffectChunkFrames);
					ma_lpf_process_pcm_frames(&m_lowPass, discard, pSource, chunk);
					pSource += chunk * kOdinOutputChannels;
					framesLeft -= chunk;
				}
				return;
			}

			// Blend on a log scale, which is how a change in frequency sounds.
			SetCutoff(kOcclusionOpenCutoffHz * powf(crymath::clamp(fLowPassHz, 20.f, kOcclusionOpenCutoffHz) / kOcclusionOpenCutoffHz, fAmount));
			ma_lpf_process_pcm_frames(&m_lowPass, pFrames, pFrames, frameCount);
		}
	}
}
