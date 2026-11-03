#include "StdAfx.h"
#include "CryOdinAudioHrtf.h"

#include "Utils/OdinHelpers.h"

#include <cmath>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// I could of use Cryengine's math but forgot...
			constexpr float kPi = 3.14159265358979f;
			constexpr float kDegToRad = kPi / 180.f;
			constexpr float kSampleRate = static_cast<float>(kOdinSampleRate);

			// An average head, as in the paper.
			constexpr float kHeadRadius = 0.0875f;  // Meters
			constexpr float kSpeedOfSound = 343.f;  // Meters per second
			constexpr float kAlphaMin = 0.1f;       // Head shadow at its deepest...
			constexpr float kThetaMin = 150.f * kDegToRad; // ...this far from the ear.

			// Head shadow: a one-pole, one-zero filter. Only the zero moves with the angle, so the pole is fixed.
			constexpr float kBeta = 2.f * kSpeedOfSound / kHeadRadius;
			constexpr float kTwoFs = 2.f * kSampleRate;
			constexpr float kShadowNorm = 1.f / (kTwoFs + kBeta);
			constexpr float kShadowA1 = (kBeta - kTwoFs) * kShadowNorm;

			// Outer ear echoes, Brown and Duda's table I. Delays are in samples at 44.1 kHz.
			// The strength is below the paper's full echoes; tune by ear.
			constexpr float kPinnaRho[CCryOdinHrtf::kPinnaEchoes] = { 0.5f, -1.f, 0.5f, -0.25f, 0.25f };
			constexpr float kPinnaA[CCryOdinHrtf::kPinnaEchoes] = { 1.f, 5.f, 5.f, 5.f, 5.f };
			constexpr float kPinnaB[CCryOdinHrtf::kPinnaEchoes] = { 2.f, 4.f, 7.f, 11.f, 13.f };
			constexpr float kPinnaD[CCryOdinHrtf::kPinnaEchoes] = { 1.f, 0.5f, 0.5f, 0.5f, 0.5f };
			constexpr float kPinnaStrength = 0.6f;
			constexpr float kPinnaRateScale = kSampleRate / 44100.f;

			// A source straight behind loses this much of its highs, from a one-pole low pass at 5 kHz.
			constexpr float kBackCut = 0.6f;
			const float kBackCoef = 1.f - expf(-2.f * kPi * 5000.f / kSampleRate);
		}

		void CCryOdinHrtf::Reset()
		{
			m_bPrimed = false;
		}

		void CCryOdinHrtf::ComputeEar(const Vec3& direction, bool bRight, SEarParams& outParams)
		{
			// Seen from this ear, a source to the other side is negative.
			const float side = bRight ? direction.x : -direction.x;
			const float theta = acosf(crymath::clamp(side, -1.f, 1.f)); // 0: straight into this ear

			// Time to reach the ear, around the head when it's on the far side. Shifted to be never negative.
			const float aOverC = kHeadRadius / kSpeedOfSound;
			const float seconds = theta < kPi * 0.5f ? aOverC * (1.f - cosf(theta)) : aOverC * (1.f + theta - kPi * 0.5f);
			outParams.delay = seconds * kSampleRate;

			// Highs are boosted facing the ear and cut on the far side.
			const float alpha = 1.f + kAlphaMin * 0.5f + (1.f - kAlphaMin * 0.5f) * cosf(theta / kThetaMin * kPi);
			outParams.b0 = (kTwoFs * alpha + kBeta) * kShadowNorm;
			outParams.b1 = (kBeta - kTwoFs * alpha) * kShadowNorm;

			outParams.back = crymath::clamp(-direction.y, 0.f, 1.f) * kBackCut;

			// Outer ear: azimuth with this ear's side positive, and elevation, both in degrees.
			const float azimuth = atan2f(side, direction.y) / kDegToRad;
			const float elevation = asinf(crymath::clamp(direction.z, -1.f, 1.f)) / kDegToRad;
			for (int k = 0; k < kPinnaEchoes; ++k)
			{
				const float tau = kPinnaA[k] * cosf(azimuth * 0.5f * kDegToRad) * sinf(kPinnaD[k] * (90.f - elevation) * kDegToRad) + kPinnaB[k];
				outParams.pinna[k] = crymath::clamp(tau * kPinnaRateScale, 0.f, static_cast<float>(kBufferSize - 2));
			}
		}

		float CCryOdinHrtf::ReadHistory(float fDelay) const
		{
			return ReadDelayLine(m_history, m_writePos, fDelay);
		}

		float CCryOdinHrtf::ReadDelayLine(const float* pLine, int writePos, float fDelay)
		{
			const int whole = static_cast<int>(fDelay);
			const float fraction = fDelay - static_cast<float>(whole);
			const float s0 = pLine[(writePos - whole) & kBufferMask];
			const float s1 = pLine[(writePos - whole - 1) & kBufferMask];
			return s0 + fraction * (s1 - s0);
		}

		void CCryOdinHrtf::Process(const float* pIn, float* pOut, uint32 frameCount, const Vec3& direction)
		{
			if (frameCount == 0)
			{
				return;
			}

			SEarParams targets[2];
			ComputeEar(direction, false, targets[0]);
			ComputeEar(direction, true, targets[1]);

			if (!m_bPrimed)
			{
				for (int ear = 0; ear < 2; ++ear)
				{
					m_ears[ear] = SEarState();
					m_ears[ear].params = targets[ear];
				}
				memset(m_history, 0, sizeof(m_history));
				m_bPrimed = true;
			}

			// Everything glides to the new direction over the block, so a moving voice doesn't click.
			const float fStep = 1.f / static_cast<float>(frameCount);
			for (uint32 frame = 0; frame < frameCount; ++frame)
			{
				const float x = 0.5f * (pIn[frame * 2] + pIn[frame * 2 + 1]);
				m_history[m_writePos] = x;
				const float t = static_cast<float>(frame + 1) * fStep;

				for (int ear = 0; ear < 2; ++ear)
				{
					SEarState& state = m_ears[ear];
					const SEarParams& from = state.params;
					const SEarParams& to = targets[ear];

					float echoes = 0.f;
					for (int k = 0; k < kPinnaEchoes; ++k)
					{
						echoes += kPinnaRho[k] * ReadHistory(from.pinna[k] + (to.pinna[k] - from.pinna[k]) * t);
					}
					const float s = x + kPinnaStrength * echoes;

					const float b0 = from.b0 + (to.b0 - from.b0) * t;
					const float b1 = from.b1 + (to.b1 - from.b1) * t;
					float y = b0 * s + b1 * state.shadowX1 - kShadowA1 * state.shadowY1;
					state.shadowX1 = s;
					state.shadowY1 = y;

					state.backLowPass += kBackCoef * (y - state.backLowPass);
					const float back = from.back + (to.back - from.back) * t;
					y += back * (state.backLowPass - y);

					state.delayLine[m_writePos] = y;
					pOut[frame * 2 + ear] = ReadDelayLine(state.delayLine, m_writePos, from.delay + (to.delay - from.delay) * t);
				}

				m_writePos = (m_writePos + 1) & kBufferMask;
			}

			m_ears[0].params = targets[0];
			m_ears[1].params = targets[1];
		}
	}
}
