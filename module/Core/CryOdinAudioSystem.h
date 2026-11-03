#pragma once

#include <ICryOdinAudioSystem.h>
#include <miniaudio.h>

#include "CryOdinAudioDevices.h"
#include "CryOdinAudioOcclusion.h"
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
		class CCryOdinSound;

		// A room's encoder as seen by the audio threads. The room keeps the pointers valid
		// until after UnregisterCaptureTarget has returned.
		struct SCryOdinCaptureTarget
		{
			OdinEncoder* pEncoder = nullptr;
			OdinRoom* pRoom = nullptr;
			const OdinPipeline* pPipeline = nullptr;
			uint32 apmEffectId = 0;

			std::atomic<bool> bTransmit{ true };
			std::atomic<bool> bGated{ true };     // Voice detection or the volume gate is on, so silence sends nothing
			std::atomic<bool> bEchoCanceller{ false };
			std::atomic<int> echoDelayMs{ 10 };
			std::atomic<bool> bSilent{ true };
			std::atomic<bool> bPublishing{ true }; // False when the game has cleared all channels. Odin would still send.
			std::atomic<OdinDecoder*> pLoopbackDecoder{ nullptr };
		};

		class CCryOdinAudioSystem final : public ICryOdinAudioSystem
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinAudioSystem)
			CCryOdinAudioSystem();
			virtual ~CCryOdinAudioSystem();

			bool InitAudioSystem();
			void ShutdownAudioSystem();
			void OnUpdate(float frameTime, int occlusionRaysPerFrame, float fOcclusionPerHit, bool& outInputChanged, bool& outOutputChanged);

			// Rooms call these so the devices only run while they're needed.
			void AcquireDevices();
			void ReleaseDevices();

			void RegisterCaptureTarget(const std::shared_ptr<SCryOdinCaptureTarget>& pTarget);
			void UnregisterCaptureTarget(const std::shared_ptr<SCryOdinCaptureTarget>& pTarget);
			// Call after anything a capture target points to was replaced, before the old one is freed.
			void WaitForAudioThreads();

			// The HRTF player setting, also while no audio system exists. Main thread.
			static void SetHrtfSetting(bool bEnabled) { s_bHrtfEnabled = bEnabled; }
			static bool GetHrtfSetting() { return s_bHrtfEnabled; }

			// Voices register themselves, so each update can place them. Main thread.
			void RegisterSound(CCryOdinSound* pSound);
			void UnregisterSound(CCryOdinSound* pSound);
			// Where a voice attaches its send to a reverb slot, null if the slot couldn't be made.
			ma_node* GetReverbSlotNode(int slot) { return m_reverb.GetSlotNode(slot); }

			const ma_allocation_callbacks& GetAllocationCallbacks() const { return m_allocation; }
			CCryOdinOcclusion& GetOcclusion() { return m_occlusion; }
			CCryOdinAudioDevice* GetAudioDevices() { return m_pAudioDevice.get(); }

			// Audio threads
			void OnCaptureData(const float* pInput, uint32 frameCount);
			void OnPlaybackData(float* pOutput, uint32 frameCount);

			// ICryOdinAudioSystem
			virtual ma_engine* GetAudioEngine() override { return &m_engine; }
			virtual ICryOdinAudioDevice* GetDevices() override { return m_pAudioDevice.get(); }

			virtual void SetMasterVolume(float fVolume) override;
			virtual float GetMasterVolume() const override { return m_fMasterVolume; }

			virtual void SetMicrophoneMuted(bool bMuted) override { m_bMicMuted.store(bMuted, std::memory_order_relaxed); }
			virtual bool IsMicrophoneMuted() const override { return m_bMicMuted.load(std::memory_order_relaxed); }

			virtual void SetMicrophoneGain(float fGain) override { m_fMicGain.store(std::max(fGain, 0.f), std::memory_order_relaxed); }
			virtual float GetMicrophoneGain() const override { return m_fMicGain.load(std::memory_order_relaxed); }

			virtual float GetInputLevel() const override { return m_fInputLevel.load(std::memory_order_relaxed); }
			virtual float GetOutputLevel() const override { return m_fOutputLevel.load(std::memory_order_relaxed); }

			virtual void SetListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up, EntityId ignoreEntity) override;
			virtual Vec3 GetListenerPosition() const override { return m_listener.position; }

			virtual void SetHrtfEnabled(bool bEnabled) override { s_bHrtfEnabled = bEnabled; }
			virtual bool IsHrtfEnabled() const override { return s_bHrtfEnabled; }

			virtual void SetReverbEnabled(bool bEnabled) override { CCryOdinReverbManager::SetEnabled(bEnabled); }
			virtual bool IsReverbEnabled() const override { return CCryOdinReverbManager::IsEnabled(); }
			virtual void SetEnvironmentReverb(const char* szEnvironmentName, const SCryOdinReverbSettings& settings) override { CCryOdinReverbManager::SetEnvironmentReverb(szEnvironmentName, settings); }
			virtual void RemoveEnvironmentReverb(const char* szEnvironmentName) override { CCryOdinReverbManager::RemoveEnvironmentReverb(szEnvironmentName); }
			// ~ICryOdinAudioSystem

		private:
			using TCaptureTargets = std::vector<std::shared_ptr<SCryOdinCaptureTarget>>;

			struct SListenerState
			{
				Vec3 position = ZERO;
				Vec3 forward = Vec3(0.f, 1.f, 0.f);
				Vec3 up = Vec3(0.f, 0.f, 1.f);
			};

			static void Logging(void* pUserData, ma_uint32 level, const char* pMessage);

			void PublishCaptureTargets(std::shared_ptr<const TCaptureTargets> pTargets);
			void SendCaptureToTarget(SCryOdinCaptureTarget& target, const float* pSamples, uint32 frameCount, bool bMuted);
			void UpdateListenerFromCamera();
			void UpdateSounds(float frameTime);

			ma_allocation_callbacks m_allocation;
			ma_log m_log;
			ma_engine m_engine;
			bool m_bLogReady = false;
			bool m_bEngineReady = false;

			std::unique_ptr<CCryOdinAudioDevice> m_pAudioDevice;
			CCryOdinOcclusion m_occlusion;
			CCryOdinReverbManager m_reverb;
			int m_deviceUsers = 0;
			float m_fMasterVolume = 1.f;

			// Main thread listener state
			SListenerState m_listener;
			EntityId m_listenerIgnoreEntity = INVALID_ENTITYID;
			bool m_bListenerSetThisFrame = false;
			// A player setting, so it outlives a shutdown of the audio system. Main thread.
			static bool s_bHrtfEnabled;
			std::vector<CCryOdinSound*> m_sounds;

			// Shared with the audio threads
			std::shared_ptr<const TCaptureTargets> m_pCaptureTargets;
			CCryOdinRtSection m_captureSection;
			CCryOdinRtSection m_playbackSection;
			TCryOdinRtMailbox<SListenerState> m_listenerMailbox;
			std::atomic<bool> m_bMicMuted{ false };
			std::atomic<float> m_fMicGain{ 1.f };
			std::atomic<float> m_fInputLevel{ 0.f };
			std::atomic<float> m_fOutputLevel{ 0.f };

			// Playback thread only
			uint32 m_listenerVersion = 0;
		};
	}
}
