#include "StdAfx.h"
#include "CryOdinAudioSystem.h"
#include "Utils/MiniAudioHelpers.h"

#include <cmath>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr uint32 kCaptureChunkFrames = 2048;
			constexpr uint32 kMaxDatagramSize = 2048;
			constexpr float kLevelFall = 0.8f;
		}

		CCryOdinAudioSystem::CCryOdinAudioSystem()
		{
			m_allocation.pUserData = nullptr;
			m_allocation.onMalloc = Utils::Allocator::Alloc;
			m_allocation.onRealloc = Utils::Allocator::ReAlloc;
			m_allocation.onFree = Utils::Allocator::DeAlloc;

			ODIN_LOG("Audio System Starting");
		}

		CCryOdinAudioSystem::~CCryOdinAudioSystem()
		{
			ShutdownAudioSystem();
		}

		bool CCryOdinAudioSystem::InitAudioSystem()
		{
			ma_result result = ma_log_init(&m_allocation, &m_log);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to initialize the audio log; %s", ma_result_description(result));
				return false;
			}
			m_bLogReady = true;
			ma_log_register_callback(&m_log, ma_log_callback_init(Logging, nullptr));

			// The engine has no device of its own. OnPlaybackData reads it, so devices can be changed freely
			// and the mixed output is available for the echo canceller.
			ma_engine_config engineConfig = ma_engine_config_init();
			engineConfig.noDevice = MA_TRUE;
			engineConfig.channels = kOdinOutputChannels;
			engineConfig.sampleRate = kOdinSampleRate;
			engineConfig.listenerCount = 1;
			engineConfig.allocationCallbacks = m_allocation;
			engineConfig.pLog = &m_log;

			result = ma_engine_init(&engineConfig, &m_engine);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to initialize the audio engine; %s", ma_result_description(result));
				return false;
			}
			m_bEngineReady = true;

			// The CRYENGINE world has Z going up, Y forward and is right handed, like miniaudio's default.
			ma_engine_listener_set_world_up(&m_engine, 0, 0.f, 0.f, 1.f);
			ma_engine_listener_set_direction(&m_engine, 0, 0.f, 1.f, 0.f);

			m_pAudioDevice = stl::make_unique<CCryOdinAudioDevice>(*this);
			if (!m_pAudioDevice->Init(&m_log))
			{
				return false;
			}

			return true;
		}

		void CCryOdinAudioSystem::ShutdownAudioSystem()
		{
			// Stops the device threads, nothing reads the engine after this.
			m_pAudioDevice.reset();
			m_deviceUsers = 0;

			std::atomic_store(&m_pCaptureTargets, std::shared_ptr<const TCaptureTargets>());

			if (m_bEngineReady)
			{
				ma_engine_uninit(&m_engine);
				m_bEngineReady = false;
			}
			if (m_bLogReady)
			{
				ma_log_uninit(&m_log);
				m_bLogReady = false;
			}
		}

		void CCryOdinAudioSystem::OnUpdate(float frameTime, int occlusionRaysPerFrame, float fOcclusionPerHit, bool& outInputChanged, bool& outOutputChanged)
		{
			outInputChanged = false;
			outOutputChanged = false;

			if (m_pAudioDevice)
			{
				m_pAudioDevice->OnUpdate(frameTime, outInputChanged, outOutputChanged);
			}

			if (!m_bListenerSetThisFrame)
			{
				UpdateListenerFromCamera();
			}
			m_bListenerSetThisFrame = false;

			if (m_deviceUsers > 0)
			{
				m_occlusion.Update(m_listener.position, m_listenerIgnoreEntity, occlusionRaysPerFrame, fOcclusionPerHit);
			}
		}

		void CCryOdinAudioSystem::AcquireDevices()
		{
			if (m_pAudioDevice && ++m_deviceUsers == 1)
			{
				m_pAudioDevice->StartOutput();
				m_pAudioDevice->StartInput();
			}
		}

		void CCryOdinAudioSystem::ReleaseDevices()
		{
			if (m_pAudioDevice && m_deviceUsers > 0 && --m_deviceUsers == 0)
			{
				m_pAudioDevice->StopInput();
				m_pAudioDevice->StopOutput();
				m_fInputLevel.store(0.f, std::memory_order_relaxed);
				m_fOutputLevel.store(0.f, std::memory_order_relaxed);
			}
		}

		void CCryOdinAudioSystem::PublishCaptureTargets(std::shared_ptr<const TCaptureTargets> pTargets)
		{
			// Hold the old list past the wait. The audio threads must not be the ones to delete it.
			const std::shared_ptr<const TCaptureTargets> pOldTargets = std::atomic_exchange(&m_pCaptureTargets, std::move(pTargets));
			WaitForAudioThreads();
		}

		void CCryOdinAudioSystem::WaitForAudioThreads()
		{
			m_captureSection.WaitIdle();
			m_playbackSection.WaitIdle();
		}

		void CCryOdinAudioSystem::RegisterCaptureTarget(const std::shared_ptr<SCryOdinCaptureTarget>& pTarget)
		{
			auto pNewTargets = std::make_shared<TCaptureTargets>();
			if (const std::shared_ptr<const TCaptureTargets> pOldTargets = std::atomic_load(&m_pCaptureTargets))
			{
				*pNewTargets = *pOldTargets;
			}
			pNewTargets->push_back(pTarget);
			PublishCaptureTargets(std::move(pNewTargets));
		}

		void CCryOdinAudioSystem::UnregisterCaptureTarget(const std::shared_ptr<SCryOdinCaptureTarget>& pTarget)
		{
			auto pNewTargets = std::make_shared<TCaptureTargets>();
			if (const std::shared_ptr<const TCaptureTargets> pOldTargets = std::atomic_load(&m_pCaptureTargets))
			{
				*pNewTargets = *pOldTargets;
			}
			pNewTargets->erase(std::remove(pNewTargets->begin(), pNewTargets->end(), pTarget), pNewTargets->end());
			PublishCaptureTargets(std::move(pNewTargets));
		}

		void CCryOdinAudioSystem::SetMasterVolume(float fVolume)
		{
			m_fMasterVolume = std::max(fVolume, 0.f);
			if (m_bEngineReady)
			{
				ma_engine_set_volume(&m_engine, m_fMasterVolume);
			}
		}

		void CCryOdinAudioSystem::SetListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up, EntityId ignoreEntity)
		{
			if (!IsFiniteVec3(position) || !IsFiniteVec3(forward) || !IsFiniteVec3(up))
			{
				return;
			}

			m_listener.position = position;
			m_listener.forward = forward;
			m_listener.up = up;
			m_listenerIgnoreEntity = ignoreEntity;
			m_bListenerSetThisFrame = true;

			m_listenerMailbox.Write(m_listener);
		}

		void CCryOdinAudioSystem::UpdateListenerFromCamera()
		{
			if (gEnv->IsDedicated() || gEnv->pSystem == nullptr)
			{
				return;
			}

			const CCamera& camera = gEnv->pSystem->GetViewCamera();
			const Matrix34& matrix = camera.GetMatrix();

			m_listener.position = camera.GetPosition();
			m_listener.forward = matrix.GetColumn1();
			m_listener.up = matrix.GetColumn2();
			m_listenerIgnoreEntity = INVALID_ENTITYID;

			m_listenerMailbox.Write(m_listener);
		}

		void CCryOdinAudioSystem::OnCaptureData(const float* pInput, uint32 frameCount)
		{
			CCryOdinRtScope rtScope(m_captureSection);

			if (pInput == nullptr)
			{
				return;
			}

			const std::shared_ptr<const TCaptureTargets> pTargets = std::atomic_load(&m_pCaptureTargets);
			const bool bMuted = m_bMicMuted.load(std::memory_order_relaxed);
			const float fGain = m_fMicGain.load(std::memory_order_relaxed);

			float buffer[kCaptureChunkFrames];
			float fPeakRms = 0.f;

			uint32 framesLeft = frameCount;
			const float* pSource = pInput;
			while (framesLeft > 0)
			{
				const uint32 chunk = std::min(framesLeft, kCaptureChunkFrames);

				float fSum = 0.f;
				if (bMuted)
				{
					memset(buffer, 0, chunk * sizeof(float));
				}
				else
				{
					for (uint32 i = 0; i < chunk; ++i)
					{
						// The encoder needs samples from -1 to 1, and gain can take them past that.
						buffer[i] = crymath::clamp(pSource[i] * fGain, -1.f, 1.f);
						fSum += buffer[i] * buffer[i];
					}
				}
				fPeakRms = std::max(fPeakRms, sqrtf(fSum / static_cast<float>(chunk)));

				if (pTargets)
				{
					for (const std::shared_ptr<SCryOdinCaptureTarget>& pTarget : *pTargets)
					{
						SendCaptureToTarget(*pTarget, buffer, chunk, bMuted);
					}
				}

				pSource += chunk;
				framesLeft -= chunk;
			}

			const float fLastLevel = m_fInputLevel.load(std::memory_order_relaxed);
			m_fInputLevel.store(std::max(fPeakRms, fLastLevel * kLevelFall), std::memory_order_relaxed);
		}

		void CCryOdinAudioSystem::SendCaptureToTarget(SCryOdinCaptureTarget& target, const float* pSamples, uint32 frameCount, bool bMuted)
		{
			const bool bPublishing = target.bPublishing.load(std::memory_order_relaxed);
			const bool bTransmit = !bMuted && bPublishing && target.bTransmit.load(std::memory_order_relaxed);
			const bool bGated = target.bGated.load(std::memory_order_relaxed);

			if (!bPublishing || (!bTransmit && !bGated))
			{
				// Without a gate, even silence would be sent as voice.
				target.bSilent.store(true, std::memory_order_relaxed);
				return;
			}

			static const float s_silence[kCaptureChunkFrames] = {};
			odin_encoder_push(target.pEncoder, bTransmit ? pSamples : s_silence, frameCount);

			// Sequentially consistent, as DestroyLoopback relies on it with the WaitIdle protocol.
			OdinDecoder* pLoopback = target.pLoopbackDecoder.load(std::memory_order_seq_cst);

			// Pop always drains everything, otherwise datagrams build up in the encoder.
			for (;;)
			{
				uint8_t datagram[kMaxDatagramSize];
				uint32_t datagramLength = sizeof(datagram);
				if (odin_encoder_pop(target.pEncoder, datagram, &datagramLength) != ODIN_ERROR_SUCCESS)
				{
					break;
				}

				odin_room_send_datagram(target.pRoom, datagram, datagramLength);
				if (pLoopback != nullptr)
				{
					odin_decoder_push(pLoopback, datagram, datagramLength);
				}
			}

			target.bSilent.store(!bTransmit || odin_encoder_is_silent(target.pEncoder), std::memory_order_relaxed);
		}

		void CCryOdinAudioSystem::OnPlaybackData(float* pOutput, uint32 frameCount)
		{
			CCryOdinRtScope rtScope(m_playbackSection);

			if (!m_bEngineReady)
			{
				return;
			}

			// Some listener setters aren't safe while the graph is being read, so they all go here.
			SListenerState listener;
			if (m_listenerMailbox.TryRead(listener, m_listenerVersion))
			{
				ma_engine_listener_set_position(&m_engine, 0, listener.position.x, listener.position.y, listener.position.z);
				ma_engine_listener_set_direction(&m_engine, 0, listener.forward.x, listener.forward.y, listener.forward.z);
				ma_engine_listener_set_world_up(&m_engine, 0, listener.up.x, listener.up.y, listener.up.z);
			}

			ma_engine_read_pcm_frames(&m_engine, pOutput, frameCount, nullptr);

			const uint32 sampleCount = frameCount * kOdinOutputChannels;

			// The mix can go past full scale. Clip here instead of leaving it to the device, as the echo
			// canceller needs samples from -1 to 1 that match what the speakers really play.
			float fSum = 0.f;
			for (uint32 i = 0; i < sampleCount; ++i)
			{
				pOutput[i] = crymath::clamp(pOutput[i], -1.f, 1.f);
				fSum += pOutput[i] * pOutput[i];
			}
			const float fRms = sampleCount > 0 ? sqrtf(fSum / static_cast<float>(sampleCount)) : 0.f;
			const float fLastLevel = m_fOutputLevel.load(std::memory_order_relaxed);
			m_fOutputLevel.store(std::max(fRms, fLastLevel * kLevelFall), std::memory_order_relaxed);

			// The echo canceller compares the microphone with what was just played.
			if (const std::shared_ptr<const TCaptureTargets> pTargets = std::atomic_load(&m_pCaptureTargets))
			{
				for (const std::shared_ptr<SCryOdinCaptureTarget>& pTarget : *pTargets)
				{
					if (pTarget->apmEffectId != 0 && pTarget->bEchoCanceller.load(std::memory_order_relaxed))
					{
						odin_pipeline_update_apm_playback(pTarget->pPipeline, pTarget->apmEffectId, pOutput, sampleCount, static_cast<uint64_t>(pTarget->echoDelayMs.load(std::memory_order_relaxed)));
					}
				}
			}
		}

		// FOR LOGGING MINIAUDIO BELOW
		void CCryOdinAudioSystem::Logging(void* pUserData, ma_uint32 level, const char* pMessage)
		{
			// This can run on an audio thread, so only problems are logged.
			switch (level)
			{
			case MA_LOG_LEVEL_WARNING:
				ODIN_LOG("[miniaudio warning] %s", pMessage);
				break;
			case MA_LOG_LEVEL_ERROR:
				ODIN_LOG("[miniaudio error] %s", pMessage);
				break;
			default:
				break;
			}
		}
	}
}
