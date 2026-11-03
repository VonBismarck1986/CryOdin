#include "StdAfx.h"
#include "CryOdinAudioDevices.h"

#include "CryOdinAudioSystem.h"

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// A rerouted default device stops and starts by itself, so give it this long to come back.
			constexpr float kDeviceRestartDelay = 1.f;
			constexpr float kDeviceRestartRetry = 5.f;
		}

		CCryOdinAudioDevice::CCryOdinAudioDevice(CCryOdinAudioSystem& audioSystem)
			: m_audioSystem(audioSystem)
		{
		}

		CCryOdinAudioDevice::~CCryOdinAudioDevice()
		{
			ShutdownAudioDevices();
		}

		bool CCryOdinAudioDevice::Init(ma_log* pLog)
		{
			ma_context_config contextConfig = ma_context_config_init();
			contextConfig.pLog = pLog;
			contextConfig.allocationCallbacks = m_audioSystem.GetAllocationCallbacks();

			const ma_result result = ma_context_init(nullptr, 0, &contextConfig, &m_context);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to create the audio context; %s", ma_result_description(result));
				return false;
			}
			m_bContextReady = true;

			RefreshDevices();
			return true;
		}

		void CCryOdinAudioDevice::ShutdownAudioDevices()
		{
			m_input.bWanted = false;
			m_output.bWanted = false;
			CloseDevice(m_input);
			CloseDevice(m_output);

			m_inputDevices.clear();
			m_outputDevices.clear();

			if (m_bContextReady)
			{
				ma_context_uninit(&m_context);
				m_bContextReady = false;
			}
		}

		void CCryOdinAudioDevice::RefreshDevices()
		{
			if (!m_bContextReady)
			{
				return;
			}

			ma_device_info* pOutputs = nullptr;
			ma_uint32 outputCount = 0;
			ma_device_info* pInputs = nullptr;
			ma_uint32 inputCount = 0;

			const ma_result result = ma_context_get_devices(&m_context, &pOutputs, &outputCount, &pInputs, &inputCount);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to list audio devices; %s", ma_result_description(result));
				return;
			}

			// The arrays belong to the context and the next call replaces them, so keep a copy.
			m_inputDevices.assign(pInputs, pInputs + inputCount);
			m_outputDevices.assign(pOutputs, pOutputs + outputCount);

			ReselectByName(m_input, m_inputDevices);
			ReselectByName(m_output, m_outputDevices);
		}

		void CCryOdinAudioDevice::ReselectByName(SDeviceSlot& slot, const std::vector<ma_device_info>& devices)
		{
			if (slot.selected == DefaultDevice)
			{
				return;
			}

			// Positions in the list can change, names don't.
			slot.selected = DefaultDevice;
			for (size_t i = 0; i < devices.size(); ++i)
			{
				if (slot.selectedName == devices[i].name)
				{
					slot.selected = static_cast<int>(i);
					return;
				}
			}
		}

		SCryOdinAudioDeviceInfo CCryOdinAudioDevice::GetInputDeviceInfo(int index) const
		{
			SCryOdinAudioDeviceInfo info;
			if (index >= 0 && index < GetNumberOfInputDevices())
			{
				info.name = m_inputDevices[index].name;
				info.bDefault = m_inputDevices[index].isDefault != MA_FALSE;
			}
			return info;
		}

		SCryOdinAudioDeviceInfo CCryOdinAudioDevice::GetOutputDeviceInfo(int index) const
		{
			SCryOdinAudioDeviceInfo info;
			if (index >= 0 && index < GetNumberOfOutputDevices())
			{
				info.name = m_outputDevices[index].name;
				info.bDefault = m_outputDevices[index].isDefault != MA_FALSE;
			}
			return info;
		}

		string CCryOdinAudioDevice::GetInputDeviceName() const
		{
			return m_input.bOpen ? string(m_input.device.capture.name) : string();
		}

		string CCryOdinAudioDevice::GetOutputDeviceName() const
		{
			return m_output.bOpen ? string(m_output.device.playback.name) : string();
		}

		bool CCryOdinAudioDevice::IsInputRunning() const
		{
			return m_input.bOpen && ma_device_is_started(&m_input.device) != MA_FALSE;
		}

		bool CCryOdinAudioDevice::IsOutputRunning() const
		{
			return m_output.bOpen && ma_device_is_started(&m_output.device) != MA_FALSE;
		}

		bool CCryOdinAudioDevice::OpenDevice(SDeviceSlot& slot, bool bInput)
		{
			if (slot.bOpen)
			{
				return true;
			}
			if (!m_bContextReady)
			{
				return false;
			}

			const std::vector<ma_device_info>& devices = bInput ? m_inputDevices : m_outputDevices;
			const ma_device_id* pDeviceId = nullptr;
			if (slot.selected >= 0 && slot.selected < static_cast<int>(devices.size()))
			{
				pDeviceId = &devices[slot.selected].id;
			}

			ma_device_config config = ma_device_config_init(bInput ? ma_device_type_capture : ma_device_type_playback);
			config.sampleRate = kOdinSampleRate;
			config.dataCallback = DataCallback;
			config.notificationCallback = NotificationCallback;
			config.pUserData = this;

			if (bInput)
			{
				config.capture.pDeviceID = pDeviceId;
				config.capture.format = ma_format_f32;
				config.capture.channels = 1;
			}
			else
			{
				config.playback.pDeviceID = pDeviceId;
				config.playback.format = ma_format_f32;
				config.playback.channels = kOdinOutputChannels;
			}

			const ma_result result = ma_device_init(&m_context, &config, &slot.device);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to open the %s; %s", bInput ? "microphone" : "speakers", ma_result_description(result));
				return false;
			}

			slot.bOpen = true;
			ODIN_LOG("%s set to %s", bInput ? "Microphone" : "Speakers", bInput ? slot.device.capture.name : slot.device.playback.name);
			return true;
		}

		void CCryOdinAudioDevice::CloseDevice(SDeviceSlot& slot)
		{
			if (slot.bOpen)
			{
				// This stops the device and waits for its thread.
				ma_device_uninit(&slot.device);
				slot.bOpen = false;
			}
		}

		bool CCryOdinAudioDevice::StartDevice(SDeviceSlot& slot, bool bInput)
		{
			if (!OpenDevice(slot, bInput))
			{
				return false;
			}

			const ma_result result = ma_device_start(&slot.device);
			if (result != MA_SUCCESS)
			{
				ODIN_WARNING("Failed to start the %s; %s", bInput ? "microphone" : "speakers", ma_result_description(result));
				return false;
			}
			return true;
		}

		bool CCryOdinAudioDevice::StartInput()
		{
			m_input.bWanted = true;
			m_input.fStoppedTime = 0.f;
			return IsInputRunning() || StartDevice(m_input, true);
		}

		void CCryOdinAudioDevice::StopInput()
		{
			m_input.bWanted = false;
			// Closed and not just stopped, so that the system doesn't show the microphone in use.
			CloseDevice(m_input);
		}

		bool CCryOdinAudioDevice::StartOutput()
		{
			m_output.bWanted = true;
			m_output.fStoppedTime = 0.f;
			return IsOutputRunning() || StartDevice(m_output, false);
		}

		void CCryOdinAudioDevice::StopOutput()
		{
			m_output.bWanted = false;
			CloseDevice(m_output);
		}

		bool CCryOdinAudioDevice::ChangeDevice(SDeviceSlot& slot, bool bInput, int index)
		{
			const std::vector<ma_device_info>& devices = bInput ? m_inputDevices : m_outputDevices;
			if (index != DefaultDevice && (index < 0 || index >= static_cast<int>(devices.size())))
			{
				ODIN_WARNING("No %s device with index %d", bInput ? "input" : "output", index);
				return false;
			}

			slot.selected = index;
			slot.selectedName = index >= 0 ? devices[index].name : "";

			bool bResult = true;
			CloseDevice(slot);
			if (slot.bWanted)
			{
				bResult = StartDevice(slot, bInput);
			}
			slot.bChanged.store(true, std::memory_order_relaxed);
			return bResult;
		}

		bool CCryOdinAudioDevice::ChangeInputDevice(int index)
		{
			return ChangeDevice(m_input, true, index);
		}

		bool CCryOdinAudioDevice::ChangeOutputDevice(int index)
		{
			return ChangeDevice(m_output, false, index);
		}

		void CCryOdinAudioDevice::UpdateSlot(SDeviceSlot& slot, bool bInput, float frameTime, bool& outChanged)
		{
			outChanged = slot.bChanged.exchange(false, std::memory_order_relaxed);

			if (!slot.bWanted)
			{
				return;
			}

			const bool bRunning = slot.bOpen && ma_device_is_started(&slot.device) != MA_FALSE;
			if (bRunning)
			{
				slot.fStoppedTime = 0.f;
				return;
			}

			slot.fStoppedTime += frameTime;
			if (slot.fStoppedTime < kDeviceRestartDelay)
			{
				return;
			}

			// The selection is kept if the device is still in the list, and goes to the system default if not.
			ODIN_WARNING("The %s stopped, restarting it", bInput ? "microphone" : "speakers");
			CloseDevice(slot);
			RefreshDevices();

			bool bStarted = StartDevice(slot, bInput);
			if (!bStarted && slot.selected != DefaultDevice)
			{
				slot.selected = DefaultDevice;
				slot.selectedName = "";
				bStarted = StartDevice(slot, bInput);
			}

			if (bStarted)
			{
				slot.fStoppedTime = 0.f;
				outChanged = true;
			}
			else
			{
				slot.fStoppedTime = kDeviceRestartDelay - kDeviceRestartRetry;
			}
		}

		void CCryOdinAudioDevice::OnUpdate(float frameTime, bool& outInputChanged, bool& outOutputChanged)
		{
			UpdateSlot(m_input, true, frameTime, outInputChanged);
			UpdateSlot(m_output, false, frameTime, outOutputChanged);
		}

		void CCryOdinAudioDevice::DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
		{
			CCryOdinAudioDevice* pThis = static_cast<CCryOdinAudioDevice*>(pDevice->pUserData);

			if (pDevice->type == ma_device_type_capture)
			{
				pThis->m_audioSystem.OnCaptureData(static_cast<const float*>(pInput), frameCount);
			}
			else if (pDevice->type == ma_device_type_playback)
			{
				pThis->m_audioSystem.OnPlaybackData(static_cast<float*>(pOutput), frameCount);
			}
		}

		void CCryOdinAudioDevice::NotificationCallback(const ma_device_notification* pNotification)
		{
			// Runs on a device thread. Devices can't be changed from here, OnUpdate does that.
			if (pNotification->type == ma_device_notification_type_rerouted)
			{
				CCryOdinAudioDevice* pThis = static_cast<CCryOdinAudioDevice*>(pNotification->pDevice->pUserData);
				SDeviceSlot& slot = pNotification->pDevice->type == ma_device_type_capture ? pThis->m_input : pThis->m_output;
				slot.bChanged.store(true, std::memory_order_relaxed);
			}
		}
	}
}
