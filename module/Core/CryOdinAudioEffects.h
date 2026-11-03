#pragma once

#include <ICryOdinTypes.h>
#include <miniaudio.h>

namespace Cry
{
	namespace Odin
	{
		// The radio overlay for a mono voice. It runs before the spatializer, so that distance and
		// panning apply to the whole radio sound. Init and Uninit run on the main thread, while no
		// audio thread is using it. Everything else runs on the audio thread.
		class CCryOdinRadioEffect
		{
		public:
			bool Init(const ma_allocation_callbacks* pAllocation);
			void Uninit(const ma_allocation_callbacks* pAllocation);

			void Configure(const SCryOdinRadioSettings& settings);
			// Forgets the old signal. Call when the effect is turned on.
			void Reset(bool bTalking);
			void Process(float* pFrames, uint32 frameCount, bool bTalking);

		private:
			void ProcessChunk(float* pFrames, uint32 frameCount, bool bTransmitting);
			float NextRandom();

			SCryOdinRadioSettings m_settings;
			ma_hpf m_highPass;
			ma_lpf m_lowPass;
			ma_noise m_noise;
			bool m_bHighPassReady = false;
			bool m_bLowPassReady = false;
			bool m_bNoiseReady = false;

			uint32 m_holdFrames = 0;    // Short gaps in speech don't end a transmission
			bool m_bTransmitting = false;

			float m_fNoiseEnvelope = 0.f;
			uint32 m_squelchFrames = 0;
			uint32 m_squelchLength = 1;

			uint32 m_holdCounter = 0;
			float m_fHeldSample = 0.f;

			uint32 m_dropoutFrames = 0;
			uint32 m_dropoutCheckFrames = 0;
			uint32 m_randomState = 0x1F3A7C51;
		};

		// The muffled sound of a voice behind geometry, on the stereo output of a voice.
		class CCryOdinOcclusionFilter
		{
		public:
			bool Init(const ma_allocation_callbacks* pAllocation);
			void Uninit(const ma_allocation_callbacks* pAllocation);

			// fAmount is from 0 (no change) to 1 (fully at the low pass frequency).
			void Process(float* pFrames, uint32 frameCount, float fAmount, float fLowPassHz);

		private:
			void SetCutoff(float fCutoffHz);

			ma_lpf m_lowPass;
			bool m_bReady = false;
			float m_fCurrentCutoff = 0.f;
		};
	}
}
