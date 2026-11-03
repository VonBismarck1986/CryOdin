#include "StdAfx.h"
#include "CryOdinAudioReverb.h"
#include "CryOdinAudioSound.h"

#include "OdinComponents/CryOdinReverbComponent.h"
#include "Utils/OdinHelpers.h"

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <algorithm>
#include <cmath>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// Freeverb's tunings, in samples at 44.1 kHz.
			constexpr int kCombTuning[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
			constexpr int kAllPassTuning[] = { 556, 441, 341, 225 };
			constexpr int kStereoSpread = 23;
			constexpr float kTuningScale = static_cast<float>(kOdinSampleRate) / 44100.f;

			constexpr float kFixedGain = 0.015f;
			constexpr float kScaleRoom = 0.28f;
			constexpr float kOffsetRoom = 0.7f;
			constexpr float kScaleDamp = 0.4f;
			// Freeverb scales by 3 for use as an insert. As a send next to the dry voice, a Wet Level of 1
			// already gives about as much reverb as voice.
			constexpr float kScaleWet = 1.f;
			constexpr float kAllPassFeedback = 0.5f;
			constexpr float kMaxPreDelayMs = 200.f;

			// Each voice's spaces are looked up this often.
			constexpr float kQueryInterval = 0.1f;
			// A slot nobody sent to for this long can go to another space. Most of the tail has died by then.
			constexpr float kSlotReleaseSeconds = 1.5f;
			// The engine writes every environment found without checking the size given, so this is generous.
			constexpr size_t kMaxAudioAreas = 64;
			// Longest a slot waits for its tail to die before going to another space.
			constexpr float kSlotReleaseMaxSeconds = 6.f;

			// Space keys: an environment with the entity providing it (low half never 0, as environment ids
			// aren't), or a box component's entity (low half 0). 0 is a free slot.
			uint64 MakeEnvironmentKey(uint32 environmentId, EntityId providerId) { return (static_cast<uint64>(providerId) << 32) | environmentId; }
			uint64 MakeBoxKey(EntityId entityId) { return static_cast<uint64>(entityId) << 32; }

			// About how long a reverb takes to die away, from its feedback and the combs' average length.
			float GetReleaseSeconds(const SCryOdinReverbSettings& settings)
			{
				const float fFeedback = settings.fRoomSize * kScaleRoom + kOffsetRoom;
				const float fLoopSeconds = 1356.f / 44100.f;
				const float fSeconds = fLoopSeconds * logf(0.001f) / logf(std::max(fFeedback, 0.01f));
				return crymath::clamp(fSeconds, kSlotReleaseSeconds, kSlotReleaseMaxSeconds);
			}

			void ReverbNodeProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut);

			ma_node_vtable g_reverbNodeVtable =
			{
				ReverbNodeProcess,
				nullptr, // onGetRequiredInputFrameCount
				1,       // Input buses
				1,       // Output buses
				MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_ALLOW_NULL_INPUT // The tail goes on after the voices stop
			};

			float Sanitize(float fValue, float fDefault, float fMin, float fMax)
			{
				return std::isfinite(fValue) ? crymath::clamp(fValue, fMin, fMax) : fDefault;
			}
		}

		// ---------------------------------------------------------------------------------------------
		// CCryOdinReverb

		void CCryOdinReverb::Init()
		{
			for (int channel = 0; channel < 2; ++channel)
			{
				const int spread = channel == 1 ? kStereoSpread : 0;
				for (int i = 0; i < kCombs; ++i)
				{
					m_combs[channel][i].buffer.assign(static_cast<size_t>((kCombTuning[i] + spread) * kTuningScale), 0.f);
				}
				for (int i = 0; i < kAllPasses; ++i)
				{
					m_allPasses[channel][i].buffer.assign(static_cast<size_t>((kAllPassTuning[i] + spread) * kTuningScale), 0.f);
				}
			}
			m_preDelay.assign(static_cast<size_t>(kMaxPreDelayMs * 0.001f * kOdinSampleRate) + 1, 0.f);
		}

		void CCryOdinReverb::Configure(const SCryOdinReverbSettings& settings)
		{
			m_fFeedback = settings.fRoomSize * kScaleRoom + kOffsetRoom;
			m_fDamp = settings.fDamping * kScaleDamp;
			const float fWet = settings.fWetLevel * kScaleWet;
			m_fWet1 = fWet * (settings.fWidth * 0.5f + 0.5f);
			m_fWet2 = fWet * ((1.f - settings.fWidth) * 0.5f);
			m_preDelaySamples = crymath::clamp(static_cast<int>(settings.fPreDelayMs * 0.001f * kOdinSampleRate), 0, static_cast<int>(m_preDelay.size()) - 1);
		}

		void CCryOdinReverb::Clear()
		{
			for (int channel = 0; channel < 2; ++channel)
			{
				for (SComb& comb : m_combs[channel])
				{
					std::fill(comb.buffer.begin(), comb.buffer.end(), 0.f);
					comb.store = 0.f;
				}
				for (SAllPass& allPass : m_allPasses[channel])
				{
					std::fill(allPass.buffer.begin(), allPass.buffer.end(), 0.f);
				}
			}
			std::fill(m_preDelay.begin(), m_preDelay.end(), 0.f);
			m_fCurrentWet1 = 0.f;
			m_fCurrentWet2 = 0.f;
		}

		void CCryOdinReverb::Process(const float* pIn, float* pOut, uint32 frameCount)
		{
			if (frameCount == 0 || m_preDelay.empty())
			{
				memset(pOut, 0, frameCount * 2 * sizeof(float));
				return;
			}

			const int preDelaySize = static_cast<int>(m_preDelay.size());
			const float fDamp1 = m_fDamp;
			const float fDamp2 = 1.f - m_fDamp;
			const float fStep = 1.f / static_cast<float>(frameCount);

			for (uint32 frame = 0; frame < frameCount; ++frame)
			{
				const float input = pIn != nullptr ? (pIn[frame * 2] + pIn[frame * 2 + 1]) * kFixedGain : 0.f;

				m_preDelay[m_preDelayPosition] = input;
				int readPosition = m_preDelayPosition - m_preDelaySamples;
				if (readPosition < 0)
				{
					readPosition += preDelaySize;
				}
				const float delayed = m_preDelay[readPosition];
				if (++m_preDelayPosition >= preDelaySize)
				{
					m_preDelayPosition = 0;
				}

				float outputs[2];
				for (int channel = 0; channel < 2; ++channel)
				{
					float sum = 0.f;
					for (SComb& comb : m_combs[channel])
					{
						const float out = comb.buffer[comb.position];
						comb.store = out * fDamp2 + comb.store * fDamp1;
						if (fabsf(comb.store) < 1e-20f)
						{
							comb.store = 0.f; // No denormals
						}
						comb.buffer[comb.position] = delayed + comb.store * m_fFeedback;
						if (++comb.position >= static_cast<int>(comb.buffer.size()))
						{
							comb.position = 0;
						}
						sum += out;
					}
					for (SAllPass& allPass : m_allPasses[channel])
					{
						const float buffered = allPass.buffer[allPass.position];
						float stored = sum + buffered * kAllPassFeedback;
						if (fabsf(stored) < 1e-20f)
						{
							stored = 0.f; // No denormals
						}
						allPass.buffer[allPass.position] = stored;
						if (++allPass.position >= static_cast<int>(allPass.buffer.size()))
						{
							allPass.position = 0;
						}
						sum = buffered - sum;
					}
					outputs[channel] = sum;
				}

				const float t = static_cast<float>(frame + 1) * fStep;
				const float fWet1 = m_fCurrentWet1 + (m_fWet1 - m_fCurrentWet1) * t;
				const float fWet2 = m_fCurrentWet2 + (m_fWet2 - m_fCurrentWet2) * t;
				pOut[frame * 2] = outputs[0] * fWet1 + outputs[1] * fWet2;
				pOut[frame * 2 + 1] = outputs[1] * fWet1 + outputs[0] * fWet2;
			}

			m_fCurrentWet1 = m_fWet1;
			m_fCurrentWet2 = m_fWet2;
		}

		// ---------------------------------------------------------------------------------------------
		// CCryOdinReverbSlot

		namespace
		{
			void ReverbNodeProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
			{
				CCryOdinReverbSlot::SNode* pSlotNode = static_cast<CCryOdinReverbSlot::SNode*>(pNode);
				pSlotNode->pSlot->Process(ppFramesIn != nullptr ? ppFramesIn[0] : nullptr, ppFramesOut[0], *pFrameCountOut);
			}
		}

		bool CCryOdinReverbSlot::Init(ma_engine* pEngine, const ma_allocation_callbacks* pAllocation)
		{
			m_reverb.Init();

			memset(&m_node.base, 0, sizeof(m_node.base));
			m_node.pSlot = this;

			const ma_uint32 channels[1] = { kOdinOutputChannels };
			ma_node_config nodeConfig = ma_node_config_init();
			nodeConfig.vtable = &g_reverbNodeVtable;
			nodeConfig.pInputChannels = channels;
			nodeConfig.pOutputChannels = channels;

			const ma_result result = ma_node_init(ma_engine_get_node_graph(pEngine), &nodeConfig, pAllocation, &m_node);
			m_bNodeReady = result == MA_SUCCESS;
			if (!m_bNodeReady)
			{
				ODIN_WARNING("Failed to create a reverb; %s", ma_result_description(result));
				return false;
			}
			ma_node_attach_output_bus(&m_node, 0, ma_engine_get_endpoint(pEngine), 0);
			return true;
		}

		void CCryOdinReverbSlot::Uninit(const ma_allocation_callbacks* pAllocation)
		{
			if (m_bNodeReady)
			{
				ma_node_uninit(&m_node, pAllocation);
				m_bNodeReady = false;
			}
		}

		void CCryOdinReverbSlot::SetSettings(const SCryOdinReverbSettings& settings, bool bNewSpace)
		{
			if (!bNewSpace && settings == m_published.settings)
			{
				return;
			}
			if (bNewSpace)
			{
				++m_published.space;
			}
			m_published.settings = settings;
			m_mailbox.Write(m_published);
		}

		void CCryOdinReverbSlot::Process(const float* pIn, float* pOut, uint32 frameCount)
		{
			SPublished published;
			if (m_mailbox.TryRead(published, m_version))
			{
				if (published.space != m_space)
				{
					// What's left of the last space fades out over this block, then it's cleared.
					m_space = published.space;
					m_bClearPending = true;
					m_reverb.FadeOut();
				}
				if (m_bClearPending)
				{
					m_pendingSettings = published.settings;
				}
				else
				{
					m_reverb.Configure(published.settings);
				}
			}

			m_reverb.Process(pIn, pOut, frameCount);

			if (m_bClearPending)
			{
				m_bClearPending = false;
				m_reverb.Clear();
				m_reverb.Configure(m_pendingSettings);
			}
		}

		// ---------------------------------------------------------------------------------------------
		// CCryOdinReverbManager

		bool CCryOdinReverbManager::s_bEnabled = true;
		std::unordered_map<uint32, SCryOdinReverbSettings> CCryOdinReverbManager::s_environmentReverbs;
		std::vector<CCryOdinReverbComponent*> CCryOdinReverbManager::s_components;

		bool CCryOdinReverbManager::Init(ma_engine* pEngine, const ma_allocation_callbacks* pAllocation)
		{
			bool bAll = true;
			for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
			{
				m_slots[slot] = stl::make_unique<CCryOdinReverbSlot>();
				if (!m_slots[slot]->Init(pEngine, pAllocation))
				{
					m_slots[slot].reset();
					bAll = false;
				}
				m_slotStates[slot] = SSlotState();
			}
			return bAll;
		}

		void CCryOdinReverbManager::Uninit(const ma_allocation_callbacks* pAllocation)
		{
			for (std::unique_ptr<CCryOdinReverbSlot>& pSlot : m_slots)
			{
				if (pSlot)
				{
					pSlot->Uninit(pAllocation);
					pSlot.reset();
				}
			}
			m_voices.clear();
		}

		void CCryOdinReverbManager::SetEnvironmentReverb(const char* szName, const SCryOdinReverbSettings& settings)
		{
			if (szName != nullptr && szName[0] != '\0')
			{
				s_environmentReverbs[CryAudio::StringToId(szName)] = settings;
			}
		}

		void CCryOdinReverbManager::RemoveEnvironmentReverb(const char* szName)
		{
			if (szName != nullptr)
			{
				s_environmentReverbs.erase(CryAudio::StringToId(szName));
			}
		}

		void CCryOdinReverbManager::RegisterComponent(CCryOdinReverbComponent* pComponent)
		{
			stl::push_back_unique(s_components, pComponent);
		}

		void CCryOdinReverbManager::UnregisterComponent(CCryOdinReverbComponent* pComponent)
		{
			stl::find_and_erase(s_components, pComponent);
		}

		SCryOdinReverbSettings CCryOdinReverbManager::Resolve(const SCryOdinReverbSettings& settings)
		{
			SCryOdinReverbSettings resolved = settings.preset != ECryOdinReverbPreset::Custom ? GetCryOdinReverbPreset(settings.preset) : settings;
			const SCryOdinReverbSettings defaults;
			resolved.fRoomSize = Sanitize(resolved.fRoomSize, defaults.fRoomSize, 0.f, 1.f);
			resolved.fDamping = Sanitize(resolved.fDamping, defaults.fDamping, 0.f, 1.f);
			resolved.fWidth = Sanitize(resolved.fWidth, defaults.fWidth, 0.f, 1.f);
			resolved.fPreDelayMs = Sanitize(resolved.fPreDelayMs, defaults.fPreDelayMs, 0.f, kMaxPreDelayMs);
			resolved.fWetLevel = Sanitize(resolved.fWetLevel, defaults.fWetLevel, 0.f, 1.f);
			return resolved;
		}

		bool CCryOdinReverbManager::FindSettingsForEnvironment(uint32 environmentId, EntityId providerId, SCryOdinReverbSettings& outSettings) const
		{
			// A component on the entity with the Environment wins, also when it's turned off.
			if (const IEntity* pProvider = gEnv->pEntitySystem->GetEntity(providerId))
			{
				if (const CCryOdinReverbComponent* pComponent = pProvider->GetComponent<CCryOdinReverbComponent>())
				{
					if (pComponent->GetShape() == ECryOdinReverbShape::EngineEnvironment)
					{
						if (!pComponent->IsActive())
						{
							return false;
						}
						outSettings = Resolve(pComponent->GetReverbSettings());
						return true;
					}
				}
			}

			const auto it = s_environmentReverbs.find(environmentId);
			if (it != s_environmentReverbs.end())
			{
				outSettings = Resolve(it->second);
				return true;
			}
			return false;
		}

		void CCryOdinReverbManager::QuerySpaces(const Vec3& position, std::vector<SSpaceAmount>& outSpaces) const
		{
			outSpaces.clear();
			if (gEnv->pEntitySystem == nullptr)
			{
				return;
			}

			// The same areas and fades CryAudio gives its own sounds. The area manager locks for this.
			if (IAreaManager* pAreaManager = gEnv->pEntitySystem->GetAreaManager())
			{
				SAudioAreaInfo areas[kMaxAudioAreas];
				size_t areaCount = 0;
				pAreaManager->QueryAudioAreas(position, areas, kMaxAudioAreas, areaCount);
				for (size_t i = 0; i < areaCount && i < kMaxAudioAreas; ++i)
				{
					const SAudioAreaInfo& area = areas[i];
					SSpaceAmount space;
					if (area.amount > 0.f && area.audioEnvironmentId != CryAudio::InvalidEnvironmentId
						&& FindSettingsForEnvironment(area.audioEnvironmentId, area.envProvidingEntityId, space.settings))
					{
						// Areas sharing an environment can have different components, so the provider counts too.
						space.key = MakeEnvironmentKey(area.audioEnvironmentId, area.envProvidingEntityId);
						space.amount = std::min(area.amount, 1.f);
						outSpaces.push_back(space);
					}
				}
			}

			for (const CCryOdinReverbComponent* pComponent : s_components)
			{
				if (pComponent->IsActive() && pComponent->GetShape() == ECryOdinReverbShape::Box)
				{
					const float fAmount = pComponent->GetBoxAmount(position);
					if (fAmount > 0.f)
					{
						SSpaceAmount space;
						space.key = MakeBoxKey(pComponent->GetEntityId());
						space.amount = fAmount;
						space.settings = Resolve(pComponent->GetReverbSettings());
						outSpaces.push_back(space);
					}
				}
			}
		}

		void CCryOdinReverbManager::Update(float frameTime, const std::vector<CCryOdinSound*>& sounds)
		{
			// Each voice's spaces, looked up a few times a second and spread over frames.
			for (CCryOdinSound* pSound : sounds)
			{
				SVoiceSpaces& voice = m_voices[pSound];
				if (!s_bEnabled || !pSound->WantsReverb())
				{
					voice.spaces.clear();
					voice.fNextQuery = 0.f;
					continue;
				}

				voice.fNextQuery -= frameTime;
				if (voice.fNextQuery <= 0.f)
				{
					QuerySpaces(pSound->GetPosition(), voice.spaces);
					m_fQueryStagger = m_fQueryStagger >= kQueryInterval ? 0.f : m_fQueryStagger + 0.013f;
					voice.fNextQuery = kQueryInterval + m_fQueryStagger;
				}
			}

			// Slots keep their space while any voice is in it.
			bool bUsed[kCryOdinReverbSlots] = {};
			std::vector<SSpaceAmount> unplaced;
			for (const auto& voicePair : m_voices)
			{
				for (const SSpaceAmount& space : voicePair.second.spaces)
				{
					bool bPlaced = false;
					for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
					{
						if (m_slots[slot] && m_slotStates[slot].key == space.key)
						{
							bUsed[slot] = true;
							m_slotStates[slot].settings = space.settings;
							bPlaced = true;
							break;
						}
					}
					if (!bPlaced && std::none_of(unplaced.begin(), unplaced.end(), [&space](const SSpaceAmount& other) { return other.key == space.key; }))
					{
						unplaced.push_back(space);
					}
				}
			}

			for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
			{
				if (!m_slots[slot])
				{
					continue;
				}
				if (bUsed[slot])
				{
					m_slotStates[slot].fUnusedTime = 0.f;
					m_slots[slot]->SetSettings(m_slotStates[slot].settings, false);
				}
				else
				{
					m_slotStates[slot].fUnusedTime += frameTime;
				}
			}

			// New spaces take a free slot, or one that has been quiet long enough. Others wait.
			for (const SSpaceAmount& space : unplaced)
			{
				int best = -1;
				for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
				{
					if (!m_slots[slot] || bUsed[slot])
					{
						continue;
					}
					const bool bFree = m_slotStates[slot].key == 0;
					if (bFree || m_slotStates[slot].fUnusedTime >= GetReleaseSeconds(m_slotStates[slot].settings))
					{
						if (best < 0 || bFree || m_slotStates[slot].fUnusedTime > m_slotStates[best].fUnusedTime)
						{
							best = slot;
						}
						if (bFree)
						{
							break;
						}
					}
				}
				if (best >= 0)
				{
					m_slotStates[best].key = space.key;
					m_slotStates[best].fUnusedTime = 0.f;
					m_slotStates[best].settings = space.settings;
					m_slots[best]->SetSettings(space.settings, true);
					bUsed[best] = true;
				}
			}

			// Each voice sends to the slots of its spaces, as much as it's in each.
			for (auto& voicePair : m_voices)
			{
				SVoiceSpaces& voice = voicePair.second;
				for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
				{
					voice.sends[slot] = 0.f;
					if (m_slotStates[slot].key == 0)
					{
						continue;
					}
					for (const SSpaceAmount& space : voice.spaces)
					{
						if (space.key == m_slotStates[slot].key)
						{
							voice.sends[slot] = std::max(voice.sends[slot], space.amount);
						}
					}
				}
			}
		}

		void CCryOdinReverbManager::GetSends(const CCryOdinSound* pSound, float (&outSends)[kCryOdinReverbSlots]) const
		{
			const auto it = m_voices.find(pSound);
			for (int slot = 0; slot < kCryOdinReverbSlots; ++slot)
			{
				outSends[slot] = it != m_voices.end() ? it->second.sends[slot] : 0.f;
			}
		}
	}
}
