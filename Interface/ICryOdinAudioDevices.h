#pragma once

#include "ICryOdinTypes.h"

namespace Cry
{
	namespace Odin
	{
		//! Lists and selects the microphone and speakers. Call from the main thread only.
		struct ICryOdinAudioDevice
		{
			static constexpr int DefaultDevice = -1;

			virtual ~ICryOdinAudioDevice() {}

			//! Reads the device list again, for example after a headset was plugged in.
			virtual void RefreshDevices() = 0;

			virtual int GetNumberOfInputDevices() const = 0;
			virtual int GetNumberOfOutputDevices() const = 0;

			virtual SCryOdinAudioDeviceInfo GetInputDeviceInfo(int index) const = 0;
			virtual SCryOdinAudioDeviceInfo GetOutputDeviceInfo(int index) const = 0;

			//! The selected device, or DefaultDevice when following the system default.
			virtual int GetInputDeviceIndex() const = 0;
			virtual int GetOutputDeviceIndex() const = 0;

			//! Name of the device in use. Empty when it isn't open.
			virtual string GetInputDeviceName() const = 0;
			virtual string GetOutputDeviceName() const = 0;

			//! Selects a device. Use DefaultDevice to follow the system default. If the device is open it's switched now.
			virtual bool ChangeInputDevice(int index) = 0;
			virtual bool ChangeOutputDevice(int index) = 0;

			virtual bool IsInputRunning() const = 0;
			virtual bool IsOutputRunning() const = 0;
		};
	}
}
