#include "StdAfx.h"
#include "CryOdinAudioDevices.h"
#include "Utils/MiniAudioHelpers.h"
#include "CryOdinAudioDataSource.h"

#include "CryOdin.h"

#include "Plugin.h"
#include <CryEntitySystem/IEntity.h>

namespace Cry
{
	namespace Odin
	{
		ma_engine m_engine;
		ma_resource_manager m_resourceManager;

		OdinDataSouce m_odinDataSource; // test


		std::unique_ptr<CCryOdinAudioSystem> g_pAudioSystem;

		static ma_allocation_callbacks MemoryCallback = {
			nullptr,
			Utils::Allocator::Alloc,
			Utils::Allocator::ReAlloc,
			Utils::Allocator::DeAlloc,
		};



		CCryOdinAudioSystem::CCryOdinAudioSystem()
			: m_user(ICryOdinUser())
		{
			if (!g_pAudioSystem)
			{
				g_pAudioSystem.reset(this);
			}
			ODIN_LOG("CryOdinAudio System");
		}

		bool CCryOdinAudioSystem::Init()
		{

			// First we setup miniaudio resources before launching the engine
			ma_result result;

			ma_resource_manager_config resourceManagerConfig;
			resourceManagerConfig = ma_resource_manager_config_init();
			resourceManagerConfig.decodedFormat = ma_format_f32;
			resourceManagerConfig.decodedChannels = 0;
			resourceManagerConfig.decodedSampleRate = 48000;
			resourceManagerConfig.allocationCallbacks = MemoryCallback;
			
			result = ma_resource_manager_init(&resourceManagerConfig, &m_resourceManager);
			if (result != MA_SUCCESS)
			{
				CryWarning(VALIDATOR_MODULE_AUDIO, VALIDATOR_ASSERT, "Unable to start Miniaudio resource manager retart Game!");
				return false;
			}

			// Once resource manager is started we get audio devices ready

			GetAudioDevices(&m_audioDeviceConfig);


			ma_device_config output_config = ma_device_config_init(ma_device_type_playback);

			output_config.playback.pDeviceID = &m_audioDeviceConfig.output_devices[1].id;
			output_config.playback.format = ma_format_f32;
			output_config.playback.channels = 2;
			output_config.sampleRate = 48000;
			output_config.dataCallback = data_callback;
			result = ma_device_init(NULL, &output_config, &m_audioDeviceConfig.output);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Output Device.");
				return false;
			}

			ma_device_config input_config = ma_device_config_init(ma_device_type_capture);
			input_config.capture.pDeviceID = &m_audioDeviceConfig.input_devices[0].id;
			input_config.capture.format = ma_format_f32;
			input_config.capture.channels = 1;
			input_config.sampleRate = 48000;
			input_config.dataCallback = data_callback;
			//input_config.notificationCallback = inputCallback; //TODO:: Re-add maybe developer would want to know if device is connected or got disconnected... 

			result = ma_device_init(NULL, &input_config, &m_audioDeviceConfig.input);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Input Device.");
				return false;
			}

			OdinDataSourceConfig configOdin;
			odin_data_source_init(&configOdin, &m_odinDataSource);

			ma_engine_config config;
			config = ma_engine_config_init();
			config.pDevice = NULL; // Capture device - Speakers
			config.allocationCallbacks = MemoryCallback;
			config.pResourceManager = &m_resourceManager;
			config.listenerCount = 1;


			result = ma_engine_init(&config, &m_engine);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize the engine.");
				return false;
			}

			ma_engine_listener_set_world_up(&m_engine, 0, 0.0f, 0.0f, 1.0f); // double check on this, make sure it's Cryengine world UP position
			ma_engine_listener_set_direction(&m_engine, 0, 0.0f, 1.0f, 0.0f);
			ma_engine_listener_set_cone(&m_engine, 0, 10.f, 25.f, 0.25f);

			ma_device_start(&m_audioDeviceConfig.input);
			ma_device_start(&m_audioDeviceConfig.output);

			return true;
		}

		void CCryOdinAudioSystem::Shutdown()
		{
			// Sounds, engine, device, others 
			odin_media_stream_destroy(m_user.inputStream);
			m_user.inputStream = 0;

			ma_device_uninit(&m_audioDeviceConfig.input);
			ma_device_uninit(&m_audioDeviceConfig.output);

			FreeAudioDevices(&m_audioDeviceConfig);

			ma_engine_uninit(&m_engine);

			odin_data_source_uninit(&m_odinDataSource);

			ma_resource_manager_uninit(&m_resourceManager);

			if (g_pAudioSystem)
			{
				g_pAudioSystem.release();
			}
		}

		void CCryOdinAudioSystem::OnUpdate(float frameTime)
		{
			// We have to make sure current local player listener is being updated might want to change this to its own class... yup sigh
			if (m_user.pUserEntity != nullptr)
			{
				auto playerPos = m_user.pUserEntity->GetWorldPos();

				
				ma_engine_listener_set_position(&m_engine,0, -playerPos.x, playerPos.z, playerPos.y);

			}


			if (m_sounds.empty())
				return;

			for (auto& sound : m_sounds)
			{
				sound.second.position = sound.second.pEntity->GetWorldPos(); // Get Pos of Entity
				ma_sound_set_position(&sound.second.sound, sound.second.position.x, sound.second.position.y, sound.second.position.z); // Now Set it to sound
			}
		}

		SCryOdinAudioDevicesConfig CCryOdinAudioSystem::GetAudioDeviceConfig() const
		{
			return m_audioDeviceConfig;
		}

		ma_device_info* CCryOdinAudioSystem::GetAllOutputDevices() const
		{
			return m_audioDeviceConfig.output_devices;
		}

		ma_device_info* CCryOdinAudioSystem::GetAllInputDevices() const
		{
			return m_audioDeviceConfig.input_devices;
		}

		ma_device CCryOdinAudioSystem::GetCurrentInputDevice() const
		{
			return m_audioDeviceConfig.input;
		}

		ma_device CCryOdinAudioSystem::GetCurrentOutDevice() const
		{
			return m_audioDeviceConfig.output;
		}

		int CCryOdinAudioSystem::GetNumberOfInputDevices() const
		{
			return m_audioDeviceConfig.input_devices_count;
		}

		int CCryOdinAudioSystem::GetNumberOfOutputDevices() const
		{
			return m_audioDeviceConfig.output_devices_count;
		}

		void CCryOdinAudioSystem::SetInputStreamHandle(OdinMediaStreamHandle handle)
		{
		}

		void CCryOdinAudioSystem::ResetInputStreamHandle()
		{
		}

		void CCryOdinAudioSystem::AddSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room)
		{
			ma_result result;

			OdinDataSourceConfig config;
			config.peerID = peerID;
			config.odin_stream_ref = std::move(stream);
			config.room[0] = std::move(room); // set to 0 which is 1st

			SCryOdinSounds soundConfig;
			soundConfig.peerID = peerID;
			soundConfig.pEntity = gEnv->pEntitySystem->GetEntity(peerID);
			soundConfig.position = soundConfig.pEntity->GetWorldPos();

			// this is for odin 
			odin_data_source_update(&config);

			result = ma_sound_init_from_data_source(&m_engine, &m_odinDataSource, 0, NULL, &soundConfig.sound);
			if (result != MA_SUCCESS)
			{
				ODIN_LOG("Failed to initialize the sound.");
			}
			ma_sound_set_looping(&soundConfig.sound, MA_TRUE);
			ma_sound_start(&soundConfig.sound);

			m_sounds.emplace(std::make_pair(peerID, std::move(soundConfig)));
		}

		void CCryOdinAudioSystem::RemoveSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room)
		{
		}

		void CCryOdinAudioSystem::SetListenerPosition(const IEntity& pEntity)
		{
			if (pEntity.GetId() != INVALID_ENTITYID)
			{
				auto pos = pEntity.GetWorldPos();

				ma_engine_listener_set_position(&m_engine, 0, -pos.x, pos.z, pos.y);
			}
		}

		void CCryOdinAudioSystem::SetListenerPosition(const Vec3& position)
		{
			ma_engine_listener_set_position(&m_engine, 0, position.x, position.y, position.x);
		}

		void CCryOdinAudioSystem::SetListenerDirection(const IEntity& pEntity)
		{
			if (pEntity.GetId() != INVALID_ENTITYID)
			{
				auto posFwd = pEntity.GetForwardDir();

				ma_engine_listener_set_direction(&m_engine, 0, posFwd.x, posFwd.y, posFwd.x);
			}
		}

		void CCryOdinAudioSystem::SetListenerDirection(const Vec3& direction)
		{
			ma_engine_listener_set_position(&m_engine, 0, direction.x, direction.y, direction.x);
		}

		Vec3 CCryOdinAudioSystem::GetListenerPosition() const
		{
			auto listenPos = ma_engine_listener_get_position(&m_engine, 0);
			Vec3 position = Vec3(listenPos.x,listenPos.y,listenPos.z);

			return position;
		}

		Vec3 CCryOdinAudioSystem::GetListenerDirection() const
		{
			auto listenDir = ma_engine_listener_get_direction(&m_engine, 0);
			Vec3 direction = Vec3(listenDir.x, listenDir.y, listenDir.z);

			return direction;
		}

		float CCryOdinAudioSystem::GetMicVolume() const
		{
			return 0.0f;
		}

		void CCryOdinAudioSystem::SetMicVolume(float fAmount)
		{
		}

		bool CCryOdinAudioSystem::IsTalking() const
		{
			return false;
		}

		float CCryOdinAudioSystem::GetSoundVolumeFromPlayer(uint16_t peerID)
		{
			return 0.0f;
		}

		void CCryOdinAudioSystem::MutePlayer(uint16_t peerID)
		{
		}

		void CCryOdinAudioSystem::SetVolumeForPlayer(uint16_t peerID)
		{
		}

		bool CCryOdinAudioSystem::IsPlayerTalking()
		{
			return false;
		}

		void CCryOdinAudioSystem::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			g_pAudioSystem->process_audio_data(pDevice, pOutput, pInput, frameCount);
		}

		void CCryOdinAudioSystem::process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			if(pDevice->type == ma_device_type_capture && m_user.inputStream)
			{
				// Push audio buffer from miniaudio callback to ODIN input stream
				int sample_count = frameCount * 1;
				odin_audio_push_data(m_user.inputStream, (const float*)pInput, sample_count);
			}
			else if (pDevice->type == ma_device_type_playback)
			{
				cry_odin_source_read(&m_odinDataSource, pOutput, frameCount, NULL);
			}

		}

		void CCryOdinAudioSystem::GetAudioDevices(SCryOdinAudioDevicesConfig* devices)
		{
			ma_context context;
			ma_device_info* output_devices;
			ma_uint32 output_devices_count;
			ma_device_info* input_devices;
			ma_uint32 input_devices_count;
			ma_result result = ma_context_init(NULL, 0, NULL, &context);

			if (result == MA_SUCCESS)
			{
				if ((result = ma_context_get_devices(&context, &output_devices, &output_devices_count, &input_devices, &input_devices_count)) == MA_SUCCESS)
				{
					devices->output_devices = (ma_device_info*)malloc(sizeof(ma_device_info) * output_devices_count);
					memcpy(devices->output_devices, output_devices, sizeof(ma_device_info) * output_devices_count);
					devices->output_devices_count = output_devices_count;

					devices->input_devices = (ma_device_info*)malloc(sizeof(ma_device_info) * input_devices_count);
					memcpy(devices->input_devices, input_devices, sizeof(ma_device_info) * input_devices_count);
					devices->input_devices_count = input_devices_count;
				}
				ma_context_uninit(&context);
			}
		}

		void CCryOdinAudioSystem::FreeAudioDevices(SCryOdinAudioDevicesConfig* devices)
		{
			if (devices->output_devices != NULL)
			{
				free(devices->output_devices);
				devices->output_devices = NULL;
			}
			devices->output_devices_count = 0;
			if (devices->input_devices != NULL)
			{
				free(devices->input_devices);
				devices->input_devices = NULL;
			}
			devices->input_devices_count = 0;
		}

		void CCryOdinAudioSystem::SetTalking(bool bToggle)
		{

		}

		void CCryOdinAudioSystem::DebugDraw(float frameTime)
		{

		}


	}
}