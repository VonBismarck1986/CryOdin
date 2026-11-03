#pragma once

#include <CryMath/Cry_Math.h>

namespace Cry
{
	namespace Odin
	{
		// Places a mono voice around the listener's head for headphones, with Brown and Duda's structural
		// model ("A Structural Model for Binaural Sound Synthesis", 1998): a delay and a head shadow
		// filter for each ear, and short outer ear echoes that depend on azimuth and elevation. A gentle
		// high cut for sources behind helps tell front from back. No measured data is needed.
		// Runs on the audio thread only. Reset can be called there too.
		class CCryOdinHrtf
		{
		public:
			// The next block jumps to its direction instead of gliding from the last one.
			void Reset();

			// pIn and pOut are stereo, pIn holds the same mono voice on both channels.
			// direction is a unit vector in listener space: x right, y front, z up.
			void Process(const float* pIn, float* pOut, uint32 frameCount, const Vec3& direction);

			static constexpr int kPinnaEchoes = 5;

		private:
			static constexpr int kBufferSize = 64; // Longest delay is about 32 samples at 48 kHz.
			static constexpr int kBufferMask = kBufferSize - 1;

			struct SEarParams
			{
				float delay = 0.f;          // Samples, the time for the sound to reach this ear.
				float b0 = 1.f;             // Head shadow filter
				float b1 = 0.f;
				float back = 0.f;           // 0 to 1, high cut for sources behind.
				float pinna[kPinnaEchoes] = {}; // Delay of each outer ear echo, in samples.
			};

			struct SEarState
			{
				SEarParams params;
				float shadowX1 = 0.f;
				float shadowY1 = 0.f;
				float backLowPass = 0.f;
				float delayLine[kBufferSize] = {};
			};

			static void ComputeEar(const Vec3& direction, bool bRight, SEarParams& outParams);
			float ReadHistory(float fDelay) const;
			static float ReadDelayLine(const float* pLine, int writePos, float fDelay);

			SEarState m_ears[2]; // Left, right
			float m_history[kBufferSize] = {}; // The mono input, read by the outer ear echoes.
			int m_writePos = 0;
			bool m_bPrimed = false;
		};
	}
}
