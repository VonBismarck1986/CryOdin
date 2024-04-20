#include "StdAfx.h"
#include "CryOdinAudioDevices.h"
#include "CryOdinAudioDataSource.h"
#include "CryOdinAudioSound.h"

namespace Cry
{
	namespace Odin
	{
		static ma_pcm_rb m_ringBuffer;
		OdinDataSource datasource[200];
		std::unordered_map<uint64_t, OdinDataSource> g_datasource;

		CCryOdinAudioDevice* CCryOdinAudioDevice::CCryOdinAudioDevice::pAudioDevice = nullptr;

		CCryOdinAudioDevice::CCryOdinAudioDevice()
		{
			if (pAudioDevice == nullptr)
			{
				pAudioDevice = this;
			}

			ma_result result;

			GetAudioDevices(&m_config);

			ma_device_config output_config = ma_device_config_init(ma_device_type_playback);

			output_config.playback.pDeviceID = &m_config.output_devices[1].id;
			output_config.playback.format = ma_format_f32;
			output_config.playback.channels = 2;
			output_config.sampleRate = 48000;
			output_config.dataCallback = data_callback;
			result = ma_device_init(NULL, &output_config, &m_config.output);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize Output Device.");
			}

			ODIN_LOG("Speakers device set to %s", m_config.output.playback.name);

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

			ODIN_LOG("Microphone device set to %s", m_config.input.capture.name);

			result = ma_pcm_rb_init(m_config.output.playback.format, m_config.output.playback.channels, m_config.output.playback.internalPeriodSizeInFrames * 5, NULL, NULL, &m_ringBuffer);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize the ring buffer.");
			}

			ma_pcm_rb_set_sample_rate(&m_ringBuffer, m_config.output.sampleRate);

			ma_device_start(&m_config.input);
			ma_device_start(&m_config.output);

			ODIN_LOG("Finished Audio Device Setup");
		}

		ma_device CCryOdinAudioDevice::GetDefaultInputDevice() const
		{
			return m_config.input;
		}

		ma_device CCryOdinAudioDevice::GetDefaultOutputDevice() const
		{
			return m_config.output;
		}

		ma_device_info* CCryOdinAudioDevice::GetAllInputDevices() const
		{
			return m_config.input_devices;
		}

		ma_device_info* CCryOdinAudioDevice::GetAllOutputDevices() const
		{
			return m_config.output_devices;
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
			for (int i{ 0 }; i < m_config.input_devices_count; ++i)
			{
				if (i == index)
				{
				}
			}
		}

		void CCryOdinAudioDevice::ChangeOutputDevice(int index)
		{
			for (int i{ 0 }; i < m_config.output_devices_count; ++i)
			{
				if (i == index)
				{
				}
			}
		}

		void CCryOdinAudioDevice::SoundCreated(ma_engine* engine, CCryOdinAudioSound* sound)
		{
			auto count = sound->GetSoundID();

			OdinDataSourceConfig config{};
			config.channels = 2;
			config.format = ma_format_f32;
			config.media_handle = sound->GetMediaHandle();

			odin_data_source_init(&config, &datasource[count]);

			sound->InitSound(engine,&datasource[count]);
			sound->PlaySound();

			g_datasource.emplace(std::make_pair(count, datasource[count]));

			ODIN_LOG("Sound Started");
		}

		void CCryOdinAudioDevice::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			pAudioDevice->process_audio_data(pDevice, pOutput, pInput, frameCount);
		}

		void CCryOdinAudioDevice::process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			if (!m_user.m_pEntity)
				return;

			if (pDevice->type == ma_device_type_capture && m_user.inputStream)
			{
				// Push audio buffer from miniaudio callback to ODIN input stream
				int sample_count = frameCount * m_config.input.capture.channels;
				odin_audio_push_data(m_user.inputStream, (const float*)pInput, sample_count);
			}
			else if(pDevice->type == ma_device_type_playback)
			{
				for (auto& it : g_datasource)
				{
					if (it.second.config.media_handle != 0)
					{
						odin_read_pcm_frames(&it.second, pOutput, frameCount, NULL);
					}
				}
			}

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