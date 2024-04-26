#include "StdAfx.h"
#include "CryOdinAudioSystem.h"
#include "Utils/MiniAudioHelpers.h"


#include "CryOdinAudioDevices.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"
#include "CryOdinUser.h"
#include "CryOdinAudioOcclusion.h"
#include "ICryOdinUser.h"

#include "OdinComponents/CryOdinUserComponent.h"
#include "OdinComponents/CryOdinComponentSound.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
	namespace Odin
	{
		constexpr int max_data_obj = 25000; // I do not know max id number of Mediahandles so 25000 should be enough....
		int data_counter = 0; // Global Counter

		OdinDataSource odinData[max_data_obj]; //TODO: make this as a array
		OdinDataSourceConfig odinConfig[max_data_obj]; //TODO: make this as array

		// Ignore below node graph method testing
		//static ma_node_graph    g_nodeGraph;
		//OdinDataSourceNode      g_nodes[max_data_obj];
		//static ma_splitter_node g_splitterNode;
		//static OdinEffectNoiseNode g_noise;
		//static ma_lpf_node      g_lpfNode;

		// resuing this function - TODO:: make it global 
		static uint16_t get_media_id_from_handle(OdinMediaStreamHandle handle)
		{
			uint16_t media_id;
			int error = odin_media_stream_media_id(handle, &media_id);
			return odin_is_error(error) ? 0 : media_id;
		}

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
			m_pAudioDevice.reset(new CCryOdinAudioDevice(&m_engine, EOdinEffectsFlags::NONE));

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
			ma_engine_listener_set_cone(&m_engine, 0, 10.f, 45.f, 0.25f);

			m_pOcclusion.reset(new CCryOdinOcclusion());

			return true;
		}

		void CCryOdinAudioSystem::ShutdownAudioSystem()
		{
			m_pOcclusion.release();

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

			ma_engine_uninit(&m_engine);

			m_pAudioDevice->ShutdownAudioDevices();
			m_pAudioDevice.release();

			ma_resource_manager_uninit(&m_resourceManager);

			for (int i{ 0 }; i < data_counter; ++i)
			{
				odin_data_source_uninit(&odinData[i]);
			}
			data_counter = 0;

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

		void CCryOdinAudioSystem::CreateAudioObject(const CCryOdinUserComponent& ref)
		{
			data_counter += 1;
			
			// We based sound ID and DataSource on media handle
			uint16_t handle = get_media_id_from_handle(ref.GetOdinUser()->GetMediaHandle(EAudioHandleType::eAHT_Output));

			odinConfig[handle].channels = 2;
			odinConfig[handle].format = ma_format_f32;
			odinConfig[handle].media_handle = ref.GetOdinUser()->GetMediaHandle(EAudioHandleType::eAHT_Output);
			odinConfig[handle].room = ref.GetOdinUser()->GetRoomHandle();
			odinConfig[handle].usingNode = true;


			odin_data_source_init(&odinConfig[handle], &odinData[handle]);

			//TODO:: come back to node graph method for adding effects...

			//ma_data_source_node_config dataSourceNodeConfig = ma_data_source_node_config_init(&g_nodes[handle].odin);
			//ma_result res = odin_data_node_init_ex(&g_nodeGraph, &odinConfig[handle], &g_nodes[handle]);
			//if (res != MA_SUCCESS)
			//{
			//	ODIN_LOG("Failed");
			//}
			//
			//ma_node_attach_output_bus(&g_nodes[handle].node, 0, &g_splitterNode, 0);






			auto temp = std::make_unique<CCryOdinSound>(&m_engine, &odinData[handle], CryAudio::CTransformation::GetEmptyObject(), handle);

			temp->SetEntity(ref.GetEntity());
			temp->StartSound();

			m_pOcclusion->AddSound(temp.get());

			//m_pAudioDevice->SoundStarted(true);

			m_sounds.push_back(std::move(temp));

#if _PROFILE || _DEBUG
			ODIN_LOG("Counter: %i", data_counter);
#endif // _PROFILE || _DEBUG
		}

		void Cry::Odin::CCryOdinAudioSystem::DestroyAudioObject(const CCryOdinUserComponent& ref)
		{
		
			for (auto& sound : m_sounds)
			{
				if (sound->GetAttachedEntity()->GetId() == ref.GetEntityId())
				{
					sound->StopSound();
					sound->Destory();
					int soundID = sound->GetSoundId();
					sound.release();

					odin_data_source_uninit(&odinData[soundID]);
				}
			}

			data_counter -= 1;
#if _PROFILE || _DEBUG
			ODIN_LOG("Counter: %i", data_counter);
#endif // _PROFILE || _DEBUG
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
			auto pEntity = m_pUser->GetEntity();
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

		void CCryOdinAudioSystem::SetLocalUser(CCryOdinUserComponent* odinComponent)
		{
			m_pUser = odinComponent;
		}


		// FOR LOGGING MINIAUDIO BELOW
		void CCryOdinAudioSystem::Logging(void* pUserData, ma_uint32 level, const char* pMessage)
		{
			auto enumLevel = static_cast<ma_log_level>(level);

			switch (enumLevel)
			{
			case MA_LOG_LEVEL_DEBUG:
			{
				ODIN_LOG("[DEBUG] %s", pMessage);
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