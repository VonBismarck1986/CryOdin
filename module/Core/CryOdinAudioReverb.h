#pragma once

#include <ICryOdinTypes.h>
#include <CryEntitySystem/IEntityBasicTypes.h>
#include <miniaudio.h>

#include "Utils/OdinRealtime.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinReverbComponent;
		class CCryOdinSound;

		// How many spaces can ring at once. Each voice has a send to each.
		constexpr int kCryOdinReverbSlots = 3;

		// A stereo reverb after Jezar's Freeverb (public domain): eight damped combs and four all-passes
		// for each channel, plus a pre-delay. The buffers are made on the main thread, everything else
		// runs on the audio thread.
		class CCryOdinReverb
		{
		public:
			void Init();
			void Configure(const SCryOdinReverbSettings& settings);
			// The next Process ramps the output to silence, so a Clear after it doesn't click.
			void FadeOut() { m_fWet1 = 0.f; m_fWet2 = 0.f; }
			void Clear();
			// pIn and pOut are stereo. pIn holds the sends of all voices.
			void Process(const float* pIn, float* pOut, uint32 frameCount);

		private:
			static constexpr int kCombs = 8;
			static constexpr int kAllPasses = 4;

			struct SComb
			{
				std::vector<float> buffer;
				int position = 0;
				float store = 0.f;
			};

			struct SAllPass
			{
				std::vector<float> buffer;
				int position = 0;
			};

			SComb m_combs[2][kCombs];
			SAllPass m_allPasses[2][kAllPasses];
			std::vector<float> m_preDelay;
			int m_preDelayPosition = 0;
			int m_preDelaySamples = 0;

			float m_fFeedback = 0.84f;
			float m_fDamp = 0.2f;
			float m_fWet1 = 0.f;      // Targets
			float m_fWet2 = 0.f;
			float m_fCurrentWet1 = 0.f; // Ramped over each block
			float m_fCurrentWet2 = 0.f;
		};

		// One reverb in the voice mix. Voices attach their sends to its node.
		class CCryOdinReverbSlot
		{
		public:
			bool Init(ma_engine* pEngine, const ma_allocation_callbacks* pAllocation);
			void Uninit(const ma_allocation_callbacks* pAllocation);

			ma_node* GetNode() { return m_bNodeReady ? &m_node.base : nullptr; }

			// Main thread. A new space clears what the last one left behind.
			void SetSettings(const SCryOdinReverbSettings& settings, bool bNewSpace);

			// Audio thread
			void Process(const float* pIn, float* pOut, uint32 frameCount);

			// ma_node_base must stay the first member, miniaudio casts to it.
			struct SNode
			{
				ma_node_base base;
				CCryOdinReverbSlot* pSlot = nullptr;
			};

		private:
			struct SPublished
			{
				SCryOdinReverbSettings settings;
				uint32 space = 0;
			};

			SNode m_node;
			bool m_bNodeReady = false;
			CCryOdinReverb m_reverb;

			// Main thread
			SPublished m_published;

			// Shared
			TCryOdinRtMailbox<SPublished> m_mailbox;

			// Audio thread
			uint32 m_version = 0;
			uint32 m_space = 0;
			bool m_bClearPending = false;            // A new space: fade out this block, then clear.
			SCryOdinReverbSettings m_pendingSettings; // Used once cleared.
		};

		// Decides which spaces get the reverb slots and how much each voice sends to them. Main thread.
		class CCryOdinReverbManager
		{
		public:
			bool Init(ma_engine* pEngine, const ma_allocation_callbacks* pAllocation);
			void Uninit(const ma_allocation_callbacks* pAllocation);

			ma_node* GetSlotNode(int slot) { return m_slots[slot] ? m_slots[slot]->GetNode() : nullptr; }

			void RemoveSound(const CCryOdinSound* pSound) { m_voices.erase(pSound); }

			// Finds the spaces each voice is in, gives the slots out, and works out each voice's sends.
			void Update(float frameTime, const std::vector<CCryOdinSound*>& sounds);
			// What a voice sends to each slot, from 0 to 1, as found by the last Update.
			void GetSends(const CCryOdinSound* pSound, float (&outSends)[kCryOdinReverbSlots]) const;

			// Player setting and the name table outlive the audio system.
			static void SetEnabled(bool bEnabled) { s_bEnabled = bEnabled; }
			static bool IsEnabled() { return s_bEnabled; }
			static void SetEnvironmentReverb(const char* szName, const SCryOdinReverbSettings& settings);
			static void RemoveEnvironmentReverb(const char* szName);

			// CCryOdinReverbComponent adds itself while it exists.
			static void RegisterComponent(CCryOdinReverbComponent* pComponent);
			static void UnregisterComponent(CCryOdinReverbComponent* pComponent);

			// The settings with the preset's values filled in, and everything within range.
			static SCryOdinReverbSettings Resolve(const SCryOdinReverbSettings& settings);

		private:
			// A space: a CRYENGINE environment id, or a box component's entity with the top bit set.
			using TSpaceKey = uint64;

			struct SSpaceAmount
			{
				TSpaceKey key = 0;
				float amount = 0.f;
				SCryOdinReverbSettings settings;
			};

			struct SVoiceSpaces
			{
				float fNextQuery = 0.f;
				std::vector<SSpaceAmount> spaces;
				float sends[kCryOdinReverbSlots] = {};
			};

			struct SSlotState
			{
				TSpaceKey key = 0; // 0 is free
				float fUnusedTime = 0.f;
				SCryOdinReverbSettings settings;
			};

			void QuerySpaces(const Vec3& position, std::vector<SSpaceAmount>& outSpaces) const;
			bool FindSettingsForEnvironment(uint32 environmentId, EntityId providerId, SCryOdinReverbSettings& outSettings) const;

			std::unique_ptr<CCryOdinReverbSlot> m_slots[kCryOdinReverbSlots];
			SSlotState m_slotStates[kCryOdinReverbSlots];
			std::unordered_map<const CCryOdinSound*, SVoiceSpaces> m_voices;
			float m_fQueryStagger = 0.f;

			static bool s_bEnabled;
			static std::unordered_map<uint32, SCryOdinReverbSettings> s_environmentReverbs;
			static std::vector<CCryOdinReverbComponent*> s_components;
		};
	}
}
