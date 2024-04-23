#pragma once

#include <ICryOdinAudioDevices.h>
#include "Utils/OdinHelpers.h"

namespace Cry
{
	namespace Odin
	{
		struct OdinDataSource;


		class CCryOdinAudioDevice final : public ICryOdinAudioDevice
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinAudioDevice)
			CCryOdinAudioDevice(ma_engine* engine);
			virtual ~CCryOdinAudioDevice();

			static CCryOdinAudioDevice& Get();

			virtual ma_device GetDefaultInputDevice()  override { return m_config.input; }
			virtual ma_device GetDefaultOutputDevice()  override { return m_config.output; }

			virtual ma_device_info* GetAllInputDevices() const override { return m_config.input_devices; }
			virtual ma_device_info* GetAllOutputDevices() const override { return m_config.output_devices; }

			virtual int GetNumberOfInputDevices() const override;
			virtual int GetNumberOfOutputDevices() const override;

			virtual void ChangeInputDevice(int index) override;
			virtual void ChangeOutputDevice(int index) override;

			virtual void SetInputStreamConfig(const OdinAudioStreamConfig& config) override { m_config.audio_input_config = config; }
			virtual void SetOutputStreamConfig(const OdinAudioStreamConfig& config) override { m_config.audio_output_config = config; }

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const override { return m_config; }


			void SetInputHandle(OdinMediaStreamHandle inputHandle) { m_inputHandle = inputHandle; }
			void AddDataSource(OdinDataSource* dataSource) { m_dataSources.push_back(dataSource); }
		protected:
			static CCryOdinAudioDevice* s_instance;

			static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			void process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);


			void GetAudioDevices(SCryOdinAudioDevicesConfig* devices);
			void FreeAudioDevices(SCryOdinAudioDevicesConfig* devices);
		private:
			mutable SCryOdinAudioDevicesConfig m_config;
			std::vector<OdinDataSource*> m_dataSources;

			OdinMediaStreamHandle m_inputHandle;
		};
	}
}