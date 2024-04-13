#pragma once

#include <miniaudio.h>
#include <odin.h>
#include <map>
#include <CryMath/Cry_Math.h>
#include <CryAction.h>

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

		//TODO:: Might remove this
		struct SCryOdinSounds
		{
			ma_sound sound; // Miniaudio sound 
			Vec3     position{ ZERO }; // Cryengine Vec3 of current location of sound
			uint16_t peerID = 0;
			IEntity& pEntity; 
		};

		struct ICryOdinAudioSystem
		{
			virtual ~ICryOdinAudioSystem() {}

			virtual bool Init() = 0;
			virtual void Shutdown() = 0;
			virtual void OnUpdate(float frameTime) = 0;

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const = 0;

			virtual ma_device_info* GetAllOutputDevices() const = 0;
			virtual ma_device_info* GetAllInputDevices() const = 0;

			virtual ma_device GetCurrentInputDevice() const = 0;
			virtual ma_device GetCurrentOutDevice() const = 0;

			virtual void SelectInputDevice(int index) = 0;
			virtual void SelectOutputDevice(int index) = 0;

			virtual int GetNumberOfInputDevices() const = 0;
			virtual int GetNumberOfOutputDevices() const = 0;

			virtual void SetInputStreamHandle(OdinMediaStreamHandle handle) = 0;
			virtual void ResetInputStreamHandle() = 0;


			virtual void AddSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room) = 0;
			virtual void RemoveSoundSource(OdinMediaStreamHandle stream, uint16_t peerID, OdinRoomHandle room) = 0;


			// this portion here is for setting up listener to default player 

			virtual void SetListenerPosition(const IEntity& pEntity) = 0; 
			virtual void SetListenerPosition(const Vec3& position) = 0;

			virtual void SetListenerDirection(const IEntity& pEntity) = 0;
			virtual void SetListenerDirection(const Vec3& direction) = 0;

			virtual Vec3 GetListenerPosition() const = 0;
			virtual Vec3 GetListenerDirection() const = 0;


			// this portion is Sound / Volume controls

			virtual float GetMicVolume() const = 0;
			virtual void SetMicVolume(float fAmount) = 0;
			virtual bool IsTalking() const = 0;

			virtual float GetSoundVolumeFromPlayer(uint16_t peerID) = 0;
			virtual void MutePlayer(uint16_t peerID) = 0;
			virtual void SetVolumeForPlayer(uint16_t peerID) = 0;
			virtual bool IsPlayerTalking() = 0;




		};


	}
}