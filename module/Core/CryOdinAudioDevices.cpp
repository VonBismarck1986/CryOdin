#include "StdAfx.h"
#include "CryOdinAudioDevices.h"
#include "Utils/MiniAudioHelpers.h"

#include <CryAction.h>
#include "Plugin.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
	namespace Odin
	{

		constexpr int MAX_MA_SOUNDS = 150;
		int counter = 0;

		std::unique_ptr<CCryOdinAudioSystem> g_pAudioSystem;
		ma_engine m_engine;
		ma_resource_manager m_resourceManager;

		OdinMediaStreamHandle output_streams[512];
		OdinRoomHandle m_room;
		size_t output_streams_len = 0; // This is a counter of player, so if no players connect besides main user then 0, when someone connects it goes to 1 and soon on
		
		struct SOdinMASound
		{
			SOdinMASound()
			{
				sound = ma_sound();
			}
			ma_sound sound;
			Vec3 position = ZERO;
			EntityId m_id = INVALID_ENTITYID;
		};

		std::vector<SOdinMASound> m_sounds;


		static ma_pcm_rb m_ringBuffer;

		static ma_allocation_callbacks MemoryCallback = {
			nullptr,
			Utils::Allocator::Alloc,
			Utils::Allocator::ReAlloc,
			Utils::Allocator::DeAlloc,
		};


		CCryOdinAudioSystem::CCryOdinAudioSystem()
		{
			if (!g_pAudioSystem)
			{
				g_pAudioSystem.reset(this);
			}
			ODIN_LOG("CryOdinAudio System");
		}

		bool CCryOdinAudioSystem::Init()
		{
			ma_result result;

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

			result = ma_device_init(NULL, &input_config, &m_audioDeviceConfig.input);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Input Device.");
				return false;
			}

			result = ma_pcm_rb_init(m_audioDeviceConfig.output.playback.format, m_audioDeviceConfig.output.playback.channels, m_audioDeviceConfig.output.playback.internalPeriodSizeInFrames * 5, NULL, NULL, &m_ringBuffer);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize the ring buffer.");
				return false;
			}

			ma_pcm_rb_set_sample_rate(&m_ringBuffer, m_audioDeviceConfig.output.sampleRate);


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

		}

		void CCryOdinAudioSystem::OnUpdate(float frameTime)
		{
			/* right now I'm updating all positions at once, this isn't really best practice */
			UpdateClientListener();

			for (auto& it : m_sounds)
			{
				if (it.m_id != INVALID_ENTITYID)
				{
					auto pEntity = gEnv->pEntitySystem->GetEntity(it.m_id);
					ma_sound_set_position(&it.sound, pEntity->GetWorldPos().x, pEntity->GetWorldPos().y, pEntity->GetWorldPos().z);
					ma_sound_set_direction(&it.sound, pEntity->GetForwardDir().x, pEntity->GetForwardDir().y, pEntity->GetForwardDir().z);

					it.position = Vec3(ma_sound_get_position(&it.sound).x, ma_sound_get_position(&it.sound).y, ma_sound_get_position(&it.sound).z);
				}
			}

			DrawDebug(frameTime);
		}

		void CCryOdinAudioSystem::AddSoundSource(OdinMediaStreamHandle stream, EntityId entityID, OdinRoomHandle room)
		{
			m_room = room;
			InsertOutputStream(stream);
			ma_result result;

			auto pEntity = gEnv->pEntitySystem->GetEntity(entityID);

			for (int i{ 0 }; i < MAX_MA_SOUNDS; ++i)
			{
				if (i == counter)
				{
					SOdinMASound sound{};
					m_sounds.push_back(sound);

					m_sounds[i].m_id = entityID;
					m_sounds[i].position = pEntity->GetWorldPos();

					result = ma_sound_init_from_data_source(&m_engine, &m_ringBuffer, 0, NULL, &m_sounds[i].sound);
					if (result != MA_SUCCESS) {
						ODIN_LOG("Failed to initialize the sound.");
						break;
					}
					ma_sound_set_looping(&m_sounds[i].sound, MA_TRUE);
					ma_sound_set_pinned_listener_index(&m_sounds[i].sound, 0);
					ma_sound_set_positioning(&m_sounds[i].sound, ma_positioning_relative);

					ma_sound_start(&m_sounds[i].sound);

					ODIN_LOG("Start sound: ( %d ) - location: %.2f,%.2f,%.2f", i, m_sounds[i].position.x, m_sounds[i].position.y, m_sounds[i].position.z);
				}
				break;
			}

			counter++;
		}

		void CCryOdinAudioSystem::RemoveSoundSource(OdinMediaStreamHandle stream, EntityId entityID)
		{

		}

		void CCryOdinAudioSystem::InsertOutputStream(OdinMediaStreamHandle stream)
		{
			output_streams[output_streams_len] = stream;
			output_streams_len += 1;
		}

		void CCryOdinAudioSystem::RemoveOutputStream(size_t index)
		{
			output_streams_len -= 1;
			output_streams[index] = output_streams[output_streams_len];
			output_streams[output_streams_len] = 0;
		}

		void CCryOdinAudioSystem::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			g_pAudioSystem->process_audio_data(pDevice, pOutput, pInput, frameCount);
		}

		void CCryOdinAudioSystem::process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			if (pDevice->type == ma_device_type_capture && m_user.inputStream)
			{
				// Push audio buffer from miniaudio callback to ODIN input stream
				int sample_count = frameCount * m_audioDeviceConfig.input.capture.channels;
				odin_audio_push_data(m_user.inputStream, (const float*)pInput, sample_count);
			}
			else if (pDevice->type == ma_device_type_playback && output_streams_len)
			{
				ma_result result;
				ma_uint32 framesWritten;

				int sample_count = frameCount * m_audioDeviceConfig.output.playback.channels;
				odin_audio_mix_streams(m_room, output_streams, output_streams_len, (float*)pOutput, sample_count);

				framesWritten = 0;
				while (framesWritten < frameCount)
				{
					void* pMappedBuffer;
					ma_uint32 framesToWrite = frameCount - framesWritten;

					result = ma_pcm_rb_acquire_write(&m_ringBuffer, &framesToWrite, &pMappedBuffer);
					if (result != MA_SUCCESS)
					{
						break;
					}

					if (framesToWrite == 0) {
						break;
					}

					ma_copy_pcm_frames(pMappedBuffer, ma_offset_pcm_frames_ptr_f32((float*)pOutput,framesWritten,pDevice->playback.channels), framesToWrite,pDevice->playback.format,pDevice->playback.channels);

					result = ma_pcm_rb_commit_write(&m_ringBuffer, framesToWrite);
					if (result != MA_SUCCESS) {
						break;
					}

					framesWritten += framesToWrite;
				}
			}

			(void*)pOutput;
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

		void CCryOdinAudioSystem::DrawDebug(float frameTime)
		{
			if (m_sounds.empty())
				return;

			for (int i{ 0 }; i < counter; ++i)
			{
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_sounds[i].position, 1.75f, Col_Blue);

				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_user.listenerPosition, 0.75f, Col_Red);
			
			}
		}

		void CCryOdinAudioSystem::UpdateClientListener()
		{
			if (!m_user.m_pEntity)
				return;

			ma_engine_listener_set_position(&m_engine, 0, m_user.m_pEntity->GetWorldPos().x, m_user.m_pEntity->GetWorldPos().y, m_user.m_pEntity->GetWorldPos().z);
			ma_engine_listener_set_direction(&m_engine, 0, m_user.m_pEntity->GetForwardDir().x, m_user.m_pEntity->GetForwardDir().y, m_user.m_pEntity->GetForwardDir().z);
			
			//ma_engine_listener_set_velocity();

			m_user.listenerPosition = Vec3(ma_engine_listener_get_position(&m_engine, 0).x, ma_engine_listener_get_position(&m_engine, 0).y, ma_engine_listener_get_position(&m_engine, 0).z);
		}





	}
}