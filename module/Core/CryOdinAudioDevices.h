#pragma once

#include "ICryOdinAudioDevices.h"
#include "ICryOdinUser.h"

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

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const override;

			virtual ma_device_info* GetAllOutputDevices() const override;
			virtual ma_device_info* GetAllInputDevices() const override;

			virtual ma_device GetCurrentInputDevice() const override;
			virtual ma_device GetCurrentOutDevice() const override;

			virtual void SelectInputDevice(int index) override { /* not used right now */ }
			virtual void SelectOutputDevice(int index) override { /* not used right now */ }

			virtual int GetNumberOfInputDevices() const override;
			virtual int GetNumberOfOutputDevices() const override;

			virtual void SetInputStreamHandle(OdinMediaStreamHandle handle) override;
			virtual void ResetInputStreamHandle() override;


			virtual void AddSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room) override;
			virtual void RemoveSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room) override;


			// this portion here is for setting up listener to default player 

			virtual void SetListenerPosition(const IEntity& pEntity) override;
			virtual void SetListenerPosition(const Vec3& position) override;

			virtual void SetListenerDirection(const IEntity& pEntity) override;
			virtual void SetListenerDirection(const Vec3& direction) override;

			virtual Vec3 GetListenerPosition() const override;
			virtual Vec3 GetListenerDirection() const override;


			// this portion is Sound / Volume controls

			virtual float GetMicVolume() const override;
			virtual void SetMicVolume(float fAmount) override;
			virtual bool IsTalking() const override;

			virtual float GetSoundVolumeFromPlayer(uint16_t peerID) override;
			virtual void MutePlayer(uint16_t peerID) override;
			virtual void SetVolumeForPlayer(uint16_t peerID) override;
			virtual bool IsPlayerTalking() override;

			void AddLocalPlayer(const ICryOdinUser& user) { m_user = user; }

		protected:
			static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			void process_audio_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

			
			void GetAudioDevices(SCryOdinAudioDevicesConfig* devices);



		private:
			SCryOdinAudioDevicesConfig m_audioDeviceConfig = SCryOdinAudioDevicesConfig();
			ICryOdinUser m_user;
			std::unordered_map<int, SCryOdinSounds> m_sounds;
		};
	}
}