#pragma once

#include <miniaudio.h>

namespace Cry
{
	namespace Odin
	{
		struct SCryOdinAudioDevicesConfig
		{
			ma_device_info* output_devices;
			ma_device output;
			ma_uint32 output_devices_count;
			OdinAudioStreamConfig audio_output_config;

			ma_device_info* input_devices;
			ma_device input;
			ma_uint32 input_devices_count;
			OdinAudioStreamConfig audio_input_config;
		};

		struct ICryOdinAudioDevice
		{
			virtual ~ICryOdinAudioDevice() {}

			virtual ma_device GetDefaultInputDevice() const = 0;
			virtual ma_device GetDefaultOutputDevice() const = 0;

			virtual ma_device_info* GetAllInputDevices() const = 0;
			virtual ma_device_info* GetAllOutputDevices() const = 0;

			virtual int GetNumberOfInputDevices() const = 0;
			virtual int GetNumberOfOutputDevices() const = 0;

			virtual void ChangeInputDevice(int index) = 0;
			virtual void ChangeOutputDevice(int index) = 0;

			virtual void SetInputStreamConfig(const OdinAudioStreamConfig& config) = 0;
			virtual void SetOutputStreamConfig(const OdinAudioStreamConfig& config) = 0;

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const = 0;
		};
	}
}