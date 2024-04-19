#pragma once
#include <ICryOdinAudioDevices.h>


namespace Cry
{
	namespace Odin
	{

		class CCryOdinAudioSystem final : public ICryOdinAudioSystem
		{
		public:
			CCryOdinAudioSystem();
			virtual ~CCryOdinAudioSystem() = default;

			virtual bool Init() override;
			virtual void Shutdown() override;
			virtual void OnUpdate(float frameTime) override;

			// Input/Output Devices

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const override { return m_audioDeviceConfig; }

			virtual ma_device_info* GetAllOutputDevices() const override { return m_audioDeviceConfig.input_devices; }
			virtual ma_device_info* GetAllInputDevices() const override { return m_audioDeviceConfig.output_devices; }

			virtual ma_device GetCurrentInputDevice() const override { return m_audioDeviceConfig.input; }
			virtual ma_device GetCurrentOutDevice() const override { return m_audioDeviceConfig.output; }

			virtual const char* GetCurrentInputDeviceName() const override { return m_audioDeviceConfig.input.capture.name; }
			virtual const char* GetCurrentOutputDeviceName() const override { return m_audioDeviceConfig.output.playback.name; }

			virtual void SelectInputDevice(int index) override {}
			virtual void SelectOutputDevice(int index) override {}

			virtual int GetNumberOfInputDevices() const override { return m_audioDeviceConfig.input_devices_count; }
			virtual int GetNumberOfOutputDevices() const override { return m_audioDeviceConfig.output_devices_count; }

			virtual void AddSoundSource(OdinMediaStreamHandle stream, EntityId entityID, OdinRoomHandle room) override;
			virtual void RemoveSoundSource(OdinMediaStreamHandle stream, EntityId entityID) override;

			// this portion is Sound / Volume controls

			virtual float GetSoundVolumeFromPlayer(uint64_t peerID) const override { return 0.0f; }
			virtual void MutePlayer(uint64_t peerID) override {}
			virtual void SetVolumeForPlayer(uint64_t peerID, float fAmount) override {}

			void AddLocalPlayer(const IUser& user) { m_user = user; }

			/// DEBUG / MISC
			virtual void CreateSoundSource(const EntityId pEntity, OdinMediaStreamHandle stream, OdinRoomHandle room) override {}


			void InsertOutputStream(OdinMediaStreamHandle stream);
			void RemoveOutputStream(size_t index);

		protected:
			static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			void process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

			void GetAudioDevices(SCryOdinAudioDevicesConfig* devices);
			void FreeAudioDevices(SCryOdinAudioDevicesConfig* devices);

			void DrawDebug(float frameTime);
			void UpdateClientListener();
		private:
			SCryOdinAudioDevicesConfig m_audioDeviceConfig = SCryOdinAudioDevicesConfig();
			IUser m_user;
		};
	}
}