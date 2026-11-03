#pragma once

#include <ICryOdinAudioDevices.h>
#include <miniaudio.h>

#include "Utils/OdinHelpers.h"

#include <atomic>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioSystem;

		// The microphone and the speakers. Only the data callbacks run on another thread.
		class CCryOdinAudioDevice final : public ICryOdinAudioDevice
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinAudioDevice)
			explicit CCryOdinAudioDevice(CCryOdinAudioSystem& audioSystem);
			virtual ~CCryOdinAudioDevice();

			bool Init(ma_log* pLog);
			void ShutdownAudioDevices();

			bool StartInput();
			void StopInput();
			bool StartOutput();
			void StopOutput();

			// Restarts devices that stopped by themselves. Returns true in outInput / outOutput if a device changed.
			void OnUpdate(float frameTime, bool& outInputChanged, bool& outOutputChanged);

			// ICryOdinAudioDevice
			virtual void RefreshDevices() override;

			virtual int GetNumberOfInputDevices() const override { return static_cast<int>(m_inputDevices.size()); }
			virtual int GetNumberOfOutputDevices() const override { return static_cast<int>(m_outputDevices.size()); }

			virtual SCryOdinAudioDeviceInfo GetInputDeviceInfo(int index) const override;
			virtual SCryOdinAudioDeviceInfo GetOutputDeviceInfo(int index) const override;

			virtual int GetInputDeviceIndex() const override { return m_input.selected; }
			virtual int GetOutputDeviceIndex() const override { return m_output.selected; }

			virtual string GetInputDeviceName() const override;
			virtual string GetOutputDeviceName() const override;

			virtual bool ChangeInputDevice(int index) override;
			virtual bool ChangeOutputDevice(int index) override;

			virtual bool IsInputRunning() const override;
			virtual bool IsOutputRunning() const override;
			// ~ICryOdinAudioDevice

		private:
			struct SDeviceSlot
			{
				ma_device device;
				bool bOpen = false;     // Is ma_device_init done
				bool bWanted = false;   // Should it be running
				int selected = DefaultDevice;
				string selectedName;
				float fStoppedTime = 0.f;
				std::atomic<bool> bChanged{ false };
			};

			static void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
			static void NotificationCallback(const ma_device_notification* pNotification);

			bool OpenDevice(SDeviceSlot& slot, bool bInput);
			void CloseDevice(SDeviceSlot& slot);
			bool StartDevice(SDeviceSlot& slot, bool bInput);
			bool ChangeDevice(SDeviceSlot& slot, bool bInput, int index);
			void UpdateSlot(SDeviceSlot& slot, bool bInput, float frameTime, bool& outChanged);
			void ReselectByName(SDeviceSlot& slot, const std::vector<ma_device_info>& devices);

			CCryOdinAudioSystem& m_audioSystem;

			ma_context m_context;
			bool m_bContextReady = false;

			std::vector<ma_device_info> m_inputDevices;
			std::vector<ma_device_info> m_outputDevices;

			SDeviceSlot m_input;
			SDeviceSlot m_output;
		};
	}
}
