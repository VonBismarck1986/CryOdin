#pragma once

#include <ICryOdinAudioDevices.h>
#include <ICryOdinAudioSound.h>

namespace Cry
{
	namespace Odin
	{
		struct OdinDataSource;
		class CCryOdinAudioSound;

		class CCryOdinAudioDevice final : public ICryOdinAudioDevice
		{
		public:
			CCryOdinAudioDevice();
			virtual ~CCryOdinAudioDevice() = default;

			inline static CCryOdinAudioDevice& GetDevice() { return *pAudioDevice; };

			virtual ma_device GetDefaultInputDevice() const override;
			virtual ma_device GetDefaultOutputDevice() const override;

			virtual ma_device_info* GetAllInputDevices() const override;
			virtual ma_device_info* GetAllOutputDevices() const override;

			virtual int GetNumberOfInputDevices() const override;
			virtual int GetNumberOfOutputDevices() const override;

			virtual void ChangeInputDevice(int index) override;
			virtual void ChangeOutputDevice(int index) override;

			virtual void SetInputStreamConfig(const OdinAudioStreamConfig& config) override { m_config.audio_input_config = config; }
			virtual void SetOutputStreamConfig(const OdinAudioStreamConfig& config) override { m_config.audio_output_config = config; }
			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const override { return m_config; }

			void AddLocalUser(const IUser& user) { m_user = user; }
			void SoundCreated(ma_engine* engine, CCryOdinAudioSound* sound);

		protected:
			static CCryOdinAudioDevice* pAudioDevice;

			static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			void process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);


			void GetAudioDevices(SCryOdinAudioDevicesConfig* devices);
			void FreeAudioDevices(SCryOdinAudioDevicesConfig* devices);
		private:
			SCryOdinAudioDevicesConfig m_config;
			IUser m_user;
			std::unordered_map<int, OdinDataSource> m_dataSources;
		};
	}
}