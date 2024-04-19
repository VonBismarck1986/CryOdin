#pragma once

#include <miniaudio.h>
#include <odin.h>
#include <map>
#include <CryMath/Cry_Math.h>
#include <CryAction.h>
#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{
#define DEFAULT_ROOM_COUNT 5 // change this maybe to cvar

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

		struct ICryOdinAudioSystem
		{
			virtual ~ICryOdinAudioSystem() {}

			virtual bool Init() = 0;
			virtual void Shutdown() = 0;
			virtual void OnUpdate(float frameTime) = 0;

			// Input/Output Devices

			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const = 0;

			virtual ma_device_info* GetAllOutputDevices() const = 0;
			virtual ma_device_info* GetAllInputDevices() const = 0;

			virtual ma_device GetCurrentInputDevice() const = 0;
			virtual ma_device GetCurrentOutDevice() const = 0;

			virtual const char* GetCurrentInputDeviceName() const = 0;
			virtual const char* GetCurrentOutputDeviceName() const = 0;

			virtual void SelectInputDevice(int index) = 0;
			virtual void SelectOutputDevice(int index) = 0;

			virtual int GetNumberOfInputDevices() const = 0;
			virtual int GetNumberOfOutputDevices() const = 0;

			virtual void AddSoundSource(OdinMediaStreamHandle stream, EntityId entityID, OdinRoomHandle room) = 0;
			virtual void RemoveSoundSource(OdinMediaStreamHandle stream, EntityId entityID) = 0;

			// this portion is Sound / Volume controls

			virtual float GetSoundVolumeFromPlayer(uint64_t peerID) const = 0;
			virtual void MutePlayer(uint64_t peerID) = 0;
			virtual void SetVolumeForPlayer(uint64_t peerID, float fAmount) = 0;



			/// DEBUG / MISC
			virtual void CreateSoundSource(const EntityId pEntity, OdinMediaStreamHandle stream, OdinRoomHandle room) = 0;

		};
	}
}