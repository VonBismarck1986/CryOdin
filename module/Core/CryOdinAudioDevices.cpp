#include "StdAfx.h"
#include "CryOdinAudioDevices.h"

#include "CryOdinAudioDataSource.h"
#include "Utils/MiniAudioHelpers.h"

namespace Cry
{
	namespace Odin
	{
		CCryOdinAudioDevice* CCryOdinAudioDevice::CCryOdinAudioDevice::s_instance = nullptr;

		static ma_allocation_callbacks MemoryCallbacks = {
			nullptr,
			Utils::Allocator::Alloc,
			Utils::Allocator::ReAlloc,
			Utils::Allocator::DeAlloc,
		};

		CCryOdinAudioDevice::CCryOdinAudioDevice(ma_engine* engine, OdinDataFlags flags)
			: m_config(SCryOdinAudioDevicesConfig())
			, m_flags(flags)
		{
			if (!s_instance)
			{
				s_instance = this;
			}

			ma_result result;

			GetAudioDevices(&m_config);

			ma_device_config output_config = ma_device_config_init(ma_device_type_playback);

			output_config.playback.pDeviceID = &m_config.output_devices[0].id;
			output_config.playback.format = ma_format_f32;
			output_config.playback.channels = 0;
			output_config.sampleRate = 48000;
			output_config.dataCallback = data_callback;

			result = ma_device_init(NULL, &output_config, &m_config.output);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Output Device.");
			}
			else {
				ODIN_LOG("Speakers set to %s", m_config.output.playback.name);
			}

			ma_device_config input_config = ma_device_config_init(ma_device_type_capture);

			input_config.capture.pDeviceID = &m_config.input_devices[0].id;
			input_config.capture.format = ma_format_f32;
			input_config.capture.channels = 1;
			input_config.sampleRate = 48000;
			input_config.dataCallback = data_callback;

			result = ma_device_init(NULL, &input_config, &m_config.input);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Input Device.");
			}
			else {
				ODIN_LOG("Microphone set to %s", m_config.input.capture.name);
			}

			if (m_flags == EOdinEffectsFlags::RADIO)
			{
				g_noiseConfig.channels = 2;
				g_noiseConfig.duplicateChannels = MA_TRUE;
				g_noiseConfig.format = ma_format_f32;
				g_noiseConfig.type = ma_noise_type_pink;
				g_noiseConfig.amplitude = 0.2f;

				result = ma_noise_init(&g_noiseConfig, &MemoryCallbacks, &g_noise);
				if (result != MA_SUCCESS) {
					ODIN_LOG("Failed to initialize Radio Effect.");
				}
				ma_noise_set_seed(&g_noise, 10);
			}

			ma_device_start(&m_config.input);
			ma_device_start(&m_config.output);

			ODIN_LOG("Finished Audio Device Setup");
		}

		CCryOdinAudioDevice::~CCryOdinAudioDevice()
		{
		}

		CCryOdinAudioDevice& CCryOdinAudioDevice::Get()
		{
			return *s_instance;
		}

		int CCryOdinAudioDevice::GetNumberOfInputDevices() const
		{
			return m_config.input_devices_count;
		}

		int CCryOdinAudioDevice::GetNumberOfOutputDevices() const
		{
			return m_config.output_devices_count;
		}
		
		void CCryOdinAudioDevice::ChangeInputDevice(int index)
		{
		}

		void CCryOdinAudioDevice::ChangeOutputDevice(int index)
		{
		}

		void CCryOdinAudioDevice::ShutdownAudioDevices()
		{
			ma_device_uninit(&m_config.output);
			ma_device_uninit(&m_config.input);

			FreeAudioDevices(&m_config);

			if (s_instance)
			{
				SAFE_DELETE(s_instance);
				s_instance = nullptr;
			}
		}

		void CCryOdinAudioDevice::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			s_instance->process_audio_data(pDevice,pOutput,pInput,frameCount);
		}

		void CCryOdinAudioDevice::process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{	
			if (pDevice->type == ma_device_type_capture && m_inputHandle)
			{
				// Push audio buffer from miniaudio callback to ODIN input stream
				int sample_count = frameCount * 1;
				odin_audio_push_data(m_inputHandle, (const float*)pInput, sample_count);
			}

			//if (m_flags == EOdinDataSourceFlags::RADIO && m_bSoundStarted)
			//{
			//	ma_node_graph_read_pcm_frames(&m_nodeGraph, pOutput, frameCount, NULL);
			//}
		}

		void CCryOdinAudioDevice::GetAudioDevices(SCryOdinAudioDevicesConfig* devices)
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

			//ma_context_enumerate_devices(); TODO:: Change to this 
		}

		void CCryOdinAudioDevice::FreeAudioDevices(SCryOdinAudioDevicesConfig* devices)
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

	}
}