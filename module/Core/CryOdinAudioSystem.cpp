#include "StdAfx.h"
#include "CryOdinAudioSystem.h"
#include "Utils/MiniAudioHelpers.h"


#include "CryOdinAudioDevices.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"
#include "CryOdinUser.h"

namespace Cry
{
	namespace Odin
	{
		OdinDataSource odinData; //TODO: make this as a array
		OdinDataSourceConfig odinConfig; //TODO: make this as array

		static ma_allocation_callbacks MemoryCallback = {
			nullptr,
			Utils::Allocator::Alloc,
			Utils::Allocator::ReAlloc,
			Utils::Allocator::DeAlloc,
		};

		CCryOdinAudioSystem* CCryOdinAudioSystem::CCryOdinAudioSystem::s_instance = nullptr;
		ma_engine CCryOdinAudioSystem::CCryOdinAudioSystem::m_engine;
		ma_log CCryOdinAudioSystem::CCryOdinAudioSystem::m_log;
		ma_resource_manager CCryOdinAudioSystem::CCryOdinAudioSystem::m_resourceManager;
		ma_resource_manager_config CCryOdinAudioSystem::CCryOdinAudioSystem::m_resourceManagerConfig;


		CCryOdinAudioSystem::CCryOdinAudioSystem()
		{
			if (!s_instance)
			{
				s_instance = this;
			}

			if (!m_sounds.empty())
			{
				for (auto& sound : m_sounds)
				{
					if (sound)
					{
						sound.release();
					}
				}
			}

			ODIN_LOG("Audio System Starting");
		}

		CCryOdinAudioSystem& CCryOdinAudioSystem::Get()
		{
			return *s_instance;
		}

		bool CCryOdinAudioSystem::InitAudioSystem()
		{
			ma_result result;

			result = ma_log_init(&MemoryCallback, &m_log);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Logger.");
				return false;
			}

			ma_log_callback_init(Logging,NULL);

			//TODO:: Probably remove this
			m_resourceManagerConfig = ma_resource_manager_config_init();
			m_resourceManagerConfig.decodedFormat = ma_format_f32;
			m_resourceManagerConfig.decodedChannels = 0;               
			m_resourceManagerConfig.decodedSampleRate = 48000;          

			result = ma_resource_manager_init(&m_resourceManagerConfig, &m_resourceManager);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize resource manager.");
				return false;
			}

			// Start Devices
			m_pAudioDevice.reset(new CCryOdinAudioDevice(&m_engine));

			ma_engine_config engineConfig;
			engineConfig = ma_engine_config_init();
			engineConfig.pResourceManager = &m_resourceManager;
			engineConfig.allocationCallbacks = MemoryCallback;
			engineConfig.listenerCount = 1;
			engineConfig.pLog = &m_log;

			result = ma_engine_init(&engineConfig, &m_engine);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize engine.\n");
				return false;
			}

			ma_engine_listener_set_world_up(&m_engine, 0, 0.0f, 0.0f, 1.0f); // double check on this, make sure it's Cryengine world UP position
			ma_engine_listener_set_cone(&m_engine, 0, 10.f, 25.f, 0.25f);

			return true;
		}

		void CCryOdinAudioSystem::ShutdownAudioSystem()
		{
			if (!m_sounds.empty())
			{
				for (auto& sound : m_sounds)
				{
					if (sound)
					{
						sound->StopSound();
						sound.release();
					}
				}
			}

			if (s_instance)
			{
				SAFE_DELETE(s_instance);
				s_instance = nullptr;
			}

		}


		ma_engine* CCryOdinAudioSystem::GetAudioEngine()
		{
			return &m_engine;
		}

		void CCryOdinAudioSystem::CreateAudioObject(const ICryOdinUser& ref)
		{
			odinConfig.channels = 2;
			odinConfig.format = ma_format_f32;
			odinConfig.media_handle = ref.GetMediaHandle(EAudioHandleType::eAHT_Output);
			odinConfig.room = ref.GetRoomHandle();

			odin_data_source_init(&odinConfig, &odinData);

			auto temp = std::make_unique<CCryOdinSound>(&m_engine, &odinData, CryAudio::CTransformation::GetEmptyObject());

			//m_pAudioDevice->AddDataSource(std::move(&odinData));

			IEntity* entity = gEnv->pEntitySystem->GetEntity(ref.GetUserId());
			temp->SetEntity(entity);

			temp->StartSound();

			m_sounds.push_back(std::move(temp));


		}

		void CCryOdinAudioSystem::OnUpdate(float const frameTime)
		{
			for (auto& sound : m_sounds)
			{
				if (sound)
				{
					sound->OnUpdate(frameTime);
					UpdateLocalUserListeners(frameTime); // Why in here? well if there's no sound, no reason to listen
				}
			}


		}

		void CCryOdinAudioSystem::UpdateLocalUserListeners(float const frameTime)
		{
			auto pEntity = gEnv->pEntitySystem->GetEntity(m_pUser->GetUserId());
			if (pEntity)
			{
				auto dir = pEntity->GetForwardDir();
				auto pos = pEntity->GetWorldPos();

				ma_engine_listener_set_direction(&m_engine, 0, dir.x, dir.y, dir.z);
				ma_engine_listener_set_position(&m_engine, 0, pos.x, pos.y, pos.z);
			}
		}

		void CCryOdinAudioSystem::SetInputHandle(OdinMediaStreamHandle inputHandle)
		{
			m_pAudioDevice->SetInputHandle(inputHandle);
		}

		void CCryOdinAudioSystem::SetLocalUser(CCryOdinUser* pUser)
		{
			m_pUser = pUser;
		}


		// FOR LOGGING MINIAUDIO BELOW
		void CCryOdinAudioSystem::Logging(void* pUserData, ma_uint32 level, const char* pMessage)
		{
			s_instance->LogMiniAudio(pUserData, level, pMessage);
		}

		void CCryOdinAudioSystem::LogMiniAudio(void* pUserData, ma_uint32 level, const char* pMessage)
		{
			auto enumLevel = static_cast<ma_log_level>(level);

			switch (enumLevel)
			{
				case MA_LOG_LEVEL_DEBUG:
				{
					ODIN_LOG("[DEBUG] %s",pMessage);
				}
				break;
				case MA_LOG_LEVEL_INFO:
				{
					ODIN_LOG("[INFO] %s", pMessage);
				}
				break;
				case MA_LOG_LEVEL_WARNING:
				{
					ODIN_LOG("[WARNING] %s", pMessage);
				}
				break;
				case MA_LOG_LEVEL_ERROR:
				{
					ODIN_LOG("[ERROR] %s", pMessage);
				}
				break;
			}
		}



	}
}