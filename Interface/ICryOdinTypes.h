#pragma once

#include <CryMath/Cry_Math.h>
#include <CryString/CryString.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Utils/SharedString.h>

// Third party types are only forward declared so that code using this plugin doesn't need the
// miniaudio or Odin headers. Include <miniaudio.h> or <odin.h> yourself to use the raw handles.
struct ma_engine;
struct ma_sound;
struct OdinRoom;
struct OdinEncoder;
struct OdinDecoder;
struct OdinPipeline;
struct OdinSocket;

namespace Cry
{
	namespace Odin
	{
		//! Connection state of a room.
		enum class ECryOdinRoomStatus
		{
			None = 0,
			Joining,
			Joined,
			Closing,
			Closed
		};

		enum class ECryOdinNoiseSuppression
		{
			None = 0,
			Low,      //!< 6 dB
			Moderate, //!< 12 dB
			High,     //!< 18 dB
			VeryHigh  //!< 21 dB
		};

		enum class ECryOdinGainController
		{
			Disabled = 0,
			V1,
			V2
		};

		//! How a local user's microphone is sent to a room.
		enum class ECryOdinTransmitMode
		{
			VoiceActivity = 0, //!< Sends when the audio processing settings detect voice.
			PushToTalk,        //!< Sends only while the talk key is held. Voice detection and the volume gate are turned off.
			Continuous         //!< Always sends. Voice detection and the volume gate are turned off.
		};

		enum class ECryOdinVoicePositioning
		{
			Global2D = 0, //!< Heard equally everywhere, ignores positions.
			Spatial3D     //!< Panned and attenuated from the position given to the voice. Attenuation None still pans.
		};

		enum class ECryOdinAttenuationModel
		{
			None = 0,
			Inverse,
			Linear,
			Exponential
		};

		enum class ECryOdinRadioPreset
		{
			Custom = 0, //!< Keeps the current values.
			Clean,
			Handheld,
			Military,
			Aircraft,
			Damaged
		};

		enum class ECryOdinSocketKind
		{
			Reliable = 0, //!< Guaranteed and in order, up to 10 MiB per message.
			Unreliable    //!< Lowest latency, may be lost or reordered, must fit one datagram.
		};

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinNoiseSuppression>& desc)
		{
			desc.SetGUID("{8881E9AB-F55E-49CD-BB7D-78B732C10E10}"_cry_guid);
			desc.SetLabel("Odin Noise Suppression");
			desc.SetDefaultValue(ECryOdinNoiseSuppression::Moderate);
			desc.AddConstant(ECryOdinNoiseSuppression::None, "None", "None");
			desc.AddConstant(ECryOdinNoiseSuppression::Low, "Low", "Low (6 dB)");
			desc.AddConstant(ECryOdinNoiseSuppression::Moderate, "Moderate", "Moderate (12 dB)");
			desc.AddConstant(ECryOdinNoiseSuppression::High, "High", "High (18 dB)");
			desc.AddConstant(ECryOdinNoiseSuppression::VeryHigh, "VeryHigh", "Very High (21 dB)");
		}

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinGainController>& desc)
		{
			desc.SetGUID("{C6C12004-E53F-4DDE-88CB-7B9EB6D6A5F0}"_cry_guid);
			desc.SetLabel("Odin Gain Controller");
			desc.SetDefaultValue(ECryOdinGainController::V2);
			desc.AddConstant(ECryOdinGainController::Disabled, "Disabled", "Disabled");
			desc.AddConstant(ECryOdinGainController::V1, "V1", "V1 (adaptive digital gain)");
			desc.AddConstant(ECryOdinGainController::V2, "V2", "V2 (digital and input volume)");
		}

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinTransmitMode>& desc)
		{
			desc.SetGUID("{65E4FEB7-B45E-4F12-AAB3-DEAC27541895}"_cry_guid);
			desc.SetLabel("Odin Transmit Mode");
			desc.SetDefaultValue(ECryOdinTransmitMode::VoiceActivity);
			desc.AddConstant(ECryOdinTransmitMode::VoiceActivity, "VoiceActivity", "Voice Activity");
			desc.AddConstant(ECryOdinTransmitMode::PushToTalk, "PushToTalk", "Push To Talk");
			desc.AddConstant(ECryOdinTransmitMode::Continuous, "Continuous", "Continuous");
		}

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinVoicePositioning>& desc)
		{
			desc.SetGUID("{9A27A075-19FF-4E46-A06C-241227D3661C}"_cry_guid);
			desc.SetLabel("Odin Voice Positioning");
			desc.SetDefaultValue(ECryOdinVoicePositioning::Spatial3D);
			desc.AddConstant(ECryOdinVoicePositioning::Global2D, "Global2D", "2D (Global)");
			desc.AddConstant(ECryOdinVoicePositioning::Spatial3D, "Spatial3D", "3D (Spatial)");
		}

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinAttenuationModel>& desc)
		{
			desc.SetGUID("{4121C20E-D204-4A15-BA90-9FB667278555}"_cry_guid);
			desc.SetLabel("Odin Attenuation Model");
			desc.SetDefaultValue(ECryOdinAttenuationModel::Inverse);
			desc.AddConstant(ECryOdinAttenuationModel::None, "None", "None");
			desc.AddConstant(ECryOdinAttenuationModel::Inverse, "Inverse", "Inverse");
			desc.AddConstant(ECryOdinAttenuationModel::Linear, "Linear", "Linear");
			desc.AddConstant(ECryOdinAttenuationModel::Exponential, "Exponential", "Exponential");
		}

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinRadioPreset>& desc)
		{
			desc.SetGUID("{29FFCCBF-5A41-4656-899D-5692EDA93EB0}"_cry_guid);
			desc.SetLabel("Odin Radio Preset");
			desc.SetDefaultValue(ECryOdinRadioPreset::Military);
			desc.AddConstant(ECryOdinRadioPreset::Custom, "Custom", "Custom");
			desc.AddConstant(ECryOdinRadioPreset::Clean, "Clean", "Clean");
			desc.AddConstant(ECryOdinRadioPreset::Handheld, "Handheld", "Handheld");
			desc.AddConstant(ECryOdinRadioPreset::Military, "Military", "Military");
			desc.AddConstant(ECryOdinRadioPreset::Aircraft, "Aircraft", "Aircraft");
			desc.AddConstant(ECryOdinRadioPreset::Damaged, "Damaged", "Damaged");
		}

		//! Microphone processing, applied before the voice is encoded and sent.
		struct SCryOdinAudioProcessing
		{
			bool operator==(const SCryOdinAudioProcessing& other) const
			{
				return bVoiceActivity == other.bVoiceActivity && fVoiceAttack == other.fVoiceAttack && fVoiceRelease == other.fVoiceRelease
					&& bVolumeGate == other.bVolumeGate && fGateAttackDb == other.fGateAttackDb && fGateReleaseDb == other.fGateReleaseDb
					&& bEchoCanceller == other.bEchoCanceller && iEchoDelayMs == other.iEchoDelayMs && bHighPassFilter == other.bHighPassFilter
					&& bTransientSuppressor == other.bTransientSuppressor && noiseSuppression == other.noiseSuppression
					&& gainController == other.gainController && bVoiceIsolation == other.bVoiceIsolation && fVoiceIsolationLimitDb == other.fVoiceIsolationLimitDb;
			}

			static void ReflectType(Schematyc::CTypeDesc<SCryOdinAudioProcessing>& desc)
			{
				desc.SetGUID("{959D620B-0179-4C61-A58D-89CC538A797A}"_cry_guid);
				desc.SetLabel("Odin Audio Processing");
				desc.AddMember(&SCryOdinAudioProcessing::bVoiceActivity, 'vad', "VoiceActivity", "Voice Activity Detection", "Only send when speech is detected", true);
				desc.AddMember(&SCryOdinAudioProcessing::fVoiceAttack, 'vada', "VoiceAttack", "Voice Attack", "Speech probability (0-1) to start sending", 0.9f);
				desc.AddMember(&SCryOdinAudioProcessing::fVoiceRelease, 'vadr', "VoiceRelease", "Voice Release", "Speech probability (0-1) to stop sending", 0.8f);
				desc.AddMember(&SCryOdinAudioProcessing::bVolumeGate, 'gate', "VolumeGate", "Volume Gate", "Only send when the input is loud enough", false);
				desc.AddMember(&SCryOdinAudioProcessing::fGateAttackDb, 'gata', "GateAttack", "Gate Attack (dBFS)", "Input loudness to start sending", -30.f);
				desc.AddMember(&SCryOdinAudioProcessing::fGateReleaseDb, 'gatr', "GateRelease", "Gate Release (dBFS)", "Input loudness to stop sending", -40.f);
				desc.AddMember(&SCryOdinAudioProcessing::bEchoCanceller, 'aec', "EchoCanceller", "Echo Canceller", "Removes this client's voice playback from the microphone", true);
				desc.AddMember(&SCryOdinAudioProcessing::iEchoDelayMs, 'aecd', "EchoDelay", "Echo Delay (ms)", "Estimated delay between playback and capture", 10);
				desc.AddMember(&SCryOdinAudioProcessing::bHighPassFilter, 'hpf', "HighPassFilter", "High Pass Filter", "Removes low frequency rumble", false);
				desc.AddMember(&SCryOdinAudioProcessing::bTransientSuppressor, 'trns', "TransientSuppressor", "Transient Suppressor", "Attenuates keyboard clicks", false);
				desc.AddMember(&SCryOdinAudioProcessing::noiseSuppression, 'nois', "NoiseSuppression", "Noise Suppression", "Higher levels remove more noise but distort speech more", ECryOdinNoiseSuppression::Moderate);
				desc.AddMember(&SCryOdinAudioProcessing::gainController, 'agc', "GainController", "Gain Controller", "Keeps the input at a steady level", ECryOdinGainController::V2);
				desc.AddMember(&SCryOdinAudioProcessing::bVoiceIsolation, 'vi', "VoiceIsolation", "Voice Isolation", "Deep learning noise removal, loads a model on first use", false);
				desc.AddMember(&SCryOdinAudioProcessing::fVoiceIsolationLimitDb, 'vilm', "VoiceIsolationLimit", "Voice Isolation Limit (dB)", "100 removes background sound entirely", 100.f);
			}

			bool  bVoiceActivity = true;
			float fVoiceAttack = 0.9f;
			float fVoiceRelease = 0.8f;
			bool  bVolumeGate = false;
			float fGateAttackDb = -30.f;
			float fGateReleaseDb = -40.f;
			bool  bEchoCanceller = true;
			int   iEchoDelayMs = 10;
			bool  bHighPassFilter = false;
			bool  bTransientSuppressor = false;
			ECryOdinNoiseSuppression noiseSuppression = ECryOdinNoiseSuppression::Moderate;
			ECryOdinGainController gainController = ECryOdinGainController::V2;
			bool  bVoiceIsolation = false;
			float fVoiceIsolationLimitDb = 100.f;
		};

		//! Action map of the push to talk keys of CCryOdinUserComponent, in the engine's action map manager.
		constexpr const char* kCryOdinActionMapName = "odin";
		//! Default action name of the push to talk key.
		constexpr const char* kCryOdinPushToTalkAction = "odin_push_to_talk";

		//! The push to talk key of a CCryOdinUserComponent. It's an action in the engine's action map
		//! manager, in the action map named kCryOdinActionMapName. Your game can rebind it with
		//! IActionMapManager::ReBindActionInput and save it with the player's other bindings. If your
		//! action map XML defines the action itself, its inputs are used instead of the ones here.
		struct SCryOdinPushToTalkKey
		{
			bool operator==(const SCryOdinPushToTalkKey& other) const
			{
				return bEnabled == other.bEnabled && bToggle == other.bToggle && action == other.action && keyboard == other.keyboard && gamepad == other.gamepad;
			}
			bool operator!=(const SCryOdinPushToTalkKey& other) const { return !(*this == other); }

			//! Input names as the engine uses them: trimmed and lower case.
			static string CleanInputName(const char* szInput)
			{
				string input = szInput != nullptr ? szInput : "";
				input.Trim();
				input.MakeLower();
				return input;
			}

			//! The action name used, with the default for an empty one.
			const char* GetActionName() const { return action.empty() ? kCryOdinPushToTalkAction : action.c_str(); }

			//! Same action and inputs, ignoring case and spaces in the input names.
			bool HasSameInputs(const SCryOdinPushToTalkKey& other) const
			{
				return strcmp(GetActionName(), other.GetActionName()) == 0
					&& CleanInputName(keyboard.c_str()) == CleanInputName(other.keyboard.c_str())
					&& CleanInputName(gamepad.c_str()) == CleanInputName(other.gamepad.c_str());
			}

			static void ReflectType(Schematyc::CTypeDesc<SCryOdinPushToTalkKey>& desc)
			{
				desc.SetGUID("{007C02F0-5A51-4F57-8EC8-14C902EE8ED5}"_cry_guid);
				desc.SetLabel("Odin Push To Talk Key");
				desc.AddMember(&SCryOdinPushToTalkKey::bEnabled, 'enab', "Enabled", "Use Built In Key", "Listens for the keys below while Transmit Mode is Push To Talk. Turn off to call SetPushToTalk from your own input code instead", true);
				desc.AddMember(&SCryOdinPushToTalkKey::bToggle, 'togl', "Toggle", "Toggle", "Press once to start talking and again to stop, instead of holding the key", false);
				desc.AddMember(&SCryOdinPushToTalkKey::action, 'actn', "Action", "Action Name", "Name of the action in the odin action map. Give each room that needs its own key a different one", kCryOdinPushToTalkAction);
				desc.AddMember(&SCryOdinPushToTalkKey::keyboard, 'keyb', "Keyboard", "Keyboard Or Mouse", "Engine input name, such as v, b, capslock, lalt, mouse4 or mouse5. Empty for none", "v");
				desc.AddMember(&SCryOdinPushToTalkKey::gamepad, 'pad', "Gamepad", "Gamepad", "Engine input name of a button, such as xi_shoulderl or xi_dpad_down. Empty for none", "");
			}

			bool bEnabled = true;
			bool bToggle = false;
			Schematyc::CSharedString action = kCryOdinPushToTalkAction;
			Schematyc::CSharedString keyboard = "v";
			Schematyc::CSharedString gamepad;
		};

		//! Settings of the radio overlay. Applied to a voice on the listening client.
		struct SCryOdinRadioSettings
		{
			bool operator==(const SCryOdinRadioSettings& other) const
			{
				return bEnabled == other.bEnabled && bDisableSpatialization == other.bDisableSpatialization && preset == other.preset && fHighPassHz == other.fHighPassHz
					&& fLowPassHz == other.fLowPassHz && fDrive == other.fDrive && fNoiseLevel == other.fNoiseLevel && fSquelchLevel == other.fSquelchLevel
					&& fSquelchMs == other.fSquelchMs && iBitDepth == other.iBitDepth && iSampleHold == other.iSampleHold
					&& fDropoutChance == other.fDropoutChance && fWetMix == other.fWetMix && fOutputGain == other.fOutputGain;
			}

			static void ReflectType(Schematyc::CTypeDesc<SCryOdinRadioSettings>& desc)
			{
				desc.SetGUID("{43A7B4C9-F702-4D99-BCE2-1ADEC74F3AF7}"_cry_guid);
				desc.SetLabel("Odin Radio Settings");
				desc.AddMember(&SCryOdinRadioSettings::bEnabled, 'enab', "Enabled", "Enabled", "Plays this voice through the radio overlay", false);
				desc.AddMember(&SCryOdinRadioSettings::bDisableSpatialization, 'no3d', "DisableSpatialization", "Disable Spatialization", "A radio is heard in the ear, not from the speaker's position", true);
				desc.AddMember(&SCryOdinRadioSettings::preset, 'pres', "Preset", "Preset", "Ready made sounds. Choose Custom to use the values below", ECryOdinRadioPreset::Custom);
				desc.AddMember(&SCryOdinRadioSettings::fHighPassHz, 'hpf', "HighPass", "High Pass (Hz)", "Removes everything below this frequency", 350.f);
				desc.AddMember(&SCryOdinRadioSettings::fLowPassHz, 'lpf', "LowPass", "Low Pass (Hz)", "Removes everything above this frequency", 3200.f);
				desc.AddMember(&SCryOdinRadioSettings::fDrive, 'driv', "Drive", "Drive", "Amount of distortion (0-1)", 0.35f);
				desc.AddMember(&SCryOdinRadioSettings::fNoiseLevel, 'nois', "NoiseLevel", "Noise Level", "Static heard while the speaker talks (0-1)", 0.02f);
				desc.AddMember(&SCryOdinRadioSettings::fSquelchLevel, 'sqlv', "SquelchLevel", "Squelch Level", "Burst of static when a transmission starts and ends (0-1)", 0.15f);
				desc.AddMember(&SCryOdinRadioSettings::fSquelchMs, 'sqms', "SquelchTime", "Squelch Time (ms)", "Length of the squelch burst", 70.f);
				desc.AddMember(&SCryOdinRadioSettings::iBitDepth, 'bits', "BitDepth", "Bit Depth", "Digital degradation, 0 is off", 0);
				desc.AddMember(&SCryOdinRadioSettings::iSampleHold, 'hold', "SampleHold", "Sample Hold", "Keeps each sample for this many frames, 1 is off", 1);
				desc.AddMember(&SCryOdinRadioSettings::fDropoutChance, 'drop', "DropoutChance", "Dropout Chance", "Chance per 10 ms of a short signal loss (0-1)", 0.f);
				desc.AddMember(&SCryOdinRadioSettings::fWetMix, 'wet', "WetMix", "Wet Mix", "Blend of radio (1) and original voice (0)", 1.f);
				desc.AddMember(&SCryOdinRadioSettings::fOutputGain, 'gain', "OutputGain", "Output Gain", "Volume after the effect, the band limits remove a lot of a voice's energy", 1.6f);
			}

			bool  bEnabled = false;
			bool  bDisableSpatialization = true;
			ECryOdinRadioPreset preset = ECryOdinRadioPreset::Custom; //!< When not Custom, the values below are replaced by the preset's.
			float fHighPassHz = 350.f;
			float fLowPassHz = 3200.f;
			float fDrive = 0.35f;
			float fNoiseLevel = 0.02f;
			float fSquelchLevel = 0.15f;
			float fSquelchMs = 70.f;
			int   iBitDepth = 0;
			int   iSampleHold = 1;
			float fDropoutChance = 0.f;
			float fWetMix = 1.f;
			float fOutputGain = 1.6f;
		};

		//! Returns ready made radio settings, with bEnabled set. Custom returns the default values.
		inline SCryOdinRadioSettings GetCryOdinRadioPreset(ECryOdinRadioPreset preset)
		{
			SCryOdinRadioSettings settings;
			settings.bEnabled = true;
			settings.preset = preset;

			switch (preset)
			{
			case ECryOdinRadioPreset::Clean:
				settings.fHighPassHz = 200.f;
				settings.fLowPassHz = 5000.f;
				settings.fDrive = 0.1f;
				settings.fNoiseLevel = 0.004f;
				settings.fSquelchLevel = 0.05f;
				settings.fSquelchMs = 40.f;
				settings.fOutputGain = 1.2f;
				break;
			case ECryOdinRadioPreset::Handheld:
				settings.fHighPassHz = 450.f;
				settings.fLowPassHz = 2800.f;
				settings.fDrive = 0.5f;
				settings.fNoiseLevel = 0.03f;
				settings.fSquelchLevel = 0.2f;
				settings.fSquelchMs = 90.f;
				settings.fOutputGain = 1.8f;
				break;
			case ECryOdinRadioPreset::Military:
				settings.fHighPassHz = 400.f;
				settings.fLowPassHz = 3000.f;
				settings.fDrive = 0.4f;
				settings.fNoiseLevel = 0.018f;
				settings.fSquelchLevel = 0.2f;
				settings.fSquelchMs = 70.f;
				settings.fOutputGain = 1.7f;
				break;
			case ECryOdinRadioPreset::Aircraft:
				settings.fHighPassHz = 500.f;
				settings.fLowPassHz = 2500.f;
				settings.fDrive = 0.65f;
				settings.fNoiseLevel = 0.05f;
				settings.fSquelchLevel = 0.12f;
				settings.fSquelchMs = 50.f;
				settings.fOutputGain = 2.f;
				break;
			case ECryOdinRadioPreset::Damaged:
				settings.fHighPassHz = 600.f;
				settings.fLowPassHz = 2200.f;
				settings.fDrive = 0.8f;
				settings.fNoiseLevel = 0.08f;
				settings.fSquelchLevel = 0.3f;
				settings.fSquelchMs = 120.f;
				settings.iBitDepth = 6;
				settings.iSampleHold = 3;
				settings.fDropoutChance = 0.015f;
				settings.fOutputGain = 2.f;
				break;
			default:
				break;
			}

			return settings;
		}

		//! How one remote voice is played on this client.
		struct SCryOdinVoiceSettings
		{
			bool operator==(const SCryOdinVoiceSettings& other) const
			{
				return fVolume == other.fVolume && bMuted == other.bMuted && positioning == other.positioning && attenuation == other.attenuation
					&& fMinDistance == other.fMinDistance && fMaxDistance == other.fMaxDistance && fRolloff == other.fRolloff
					&& fDirectional == other.fDirectional && bOcclusion == other.bOcclusion
					&& fOcclusionLowPassHz == other.fOcclusionLowPassHz && fOcclusionAttenuation == other.fOcclusionAttenuation && radio == other.radio;
			}

			static void ReflectType(Schematyc::CTypeDesc<SCryOdinVoiceSettings>& desc)
			{
				desc.SetGUID("{2676E870-C7FD-4443-A81C-B0BDCA461845}"_cry_guid);
				desc.SetLabel("Odin Voice Settings");
				desc.AddMember(&SCryOdinVoiceSettings::fVolume, 'volu', "Volume", "Volume", "Linear volume of this voice", 1.f);
				desc.AddMember(&SCryOdinVoiceSettings::bMuted, 'mute', "Muted", "Muted", "Silences this voice on this client", false);
				desc.AddMember(&SCryOdinVoiceSettings::positioning, 'posi', "Positioning", "Positioning", "2D is heard everywhere, 3D comes from the entity", ECryOdinVoicePositioning::Spatial3D);
				desc.AddMember(&SCryOdinVoiceSettings::attenuation, 'attn', "Attenuation", "Attenuation", "How volume drops with distance in 3D", ECryOdinAttenuationModel::Inverse);
				desc.AddMember(&SCryOdinVoiceSettings::fMinDistance, 'mind', "MinDistance", "Min Distance", "Distance where attenuation starts", 2.f);
				desc.AddMember(&SCryOdinVoiceSettings::fMaxDistance, 'maxd', "MaxDistance", "Max Distance", "Distance where attenuation stops", 40.f);
				desc.AddMember(&SCryOdinVoiceSettings::fRolloff, 'roll', "Rolloff", "Rolloff", "Speed of the attenuation", 1.f);
				desc.AddMember(&SCryOdinVoiceSettings::fDirectional, 'dirc', "Directional", "Directional Panning", "How much the speaker's side affects stereo panning (0-1)", 1.f);
				desc.AddMember(&SCryOdinVoiceSettings::bOcclusion, 'occl', "Occlusion", "Occlusion", "Muffles the voice when geometry is in the way", false);
				desc.AddMember(&SCryOdinVoiceSettings::fOcclusionLowPassHz, 'oclp', "OcclusionLowPass", "Occlusion Low Pass (Hz)", "Low pass when fully occluded", 900.f);
				desc.AddMember(&SCryOdinVoiceSettings::fOcclusionAttenuation, 'ocat', "OcclusionAttenuation", "Occlusion Attenuation", "Volume removed when fully occluded (0-1)", 0.5f);
				desc.AddMember(&SCryOdinVoiceSettings::radio, 'radi', "Radio", "Radio", "Radio overlay", SCryOdinRadioSettings());
			}

			float fVolume = 1.f;
			bool  bMuted = false;
			ECryOdinVoicePositioning positioning = ECryOdinVoicePositioning::Spatial3D;
			ECryOdinAttenuationModel attenuation = ECryOdinAttenuationModel::Inverse;
			float fMinDistance = 2.f;
			float fMaxDistance = 40.f;
			float fRolloff = 1.f;
			float fDirectional = 1.f;
			bool  bOcclusion = false;
			float fOcclusionLowPassHz = 900.f;
			float fOcclusionAttenuation = 0.5f;
			SCryOdinRadioSettings radio;
		};

		//! Ready made bitrates for the voice sent to a room. Each listener receives this much per speaker.
		enum class ECryOdinVoiceQuality
		{
			Custom = 0, //!< Uses SCryOdinEncoderSettings::iBitrateBps.
			Low,        //!< 16 kbps, for poor connections.
			Standard,   //!< 32 kbps, clear speech.
			High,       //!< 64 kbps, close to the original voice.
			VeryHigh    //!< 128 kbps, for music or other sounds, with Voice Optimized off.
		};

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinVoiceQuality>& desc)
		{
			desc.SetGUID("{AF412B38-B0EC-46E5-B76A-2713AE5188A0}"_cry_guid);
			desc.SetLabel("Odin Voice Quality");
			desc.SetDefaultValue(ECryOdinVoiceQuality::Standard);
			desc.AddConstant(ECryOdinVoiceQuality::Custom, "Custom", "Custom");
			desc.AddConstant(ECryOdinVoiceQuality::Low, "Low", "Low (16 kbps)");
			desc.AddConstant(ECryOdinVoiceQuality::Standard, "Standard", "Standard (32 kbps)");
			desc.AddConstant(ECryOdinVoiceQuality::High, "High", "High (64 kbps)");
			desc.AddConstant(ECryOdinVoiceQuality::VeryHigh, "VeryHigh", "Very High (128 kbps)");
		}

		//! Encoder settings of a room. Change them while joined with ICryOdinRoom::SetEncoderSettings.
		struct SCryOdinEncoderSettings
		{
			static constexpr int kMinBitrateBps = 6000;
			static constexpr int kMaxBitrateBps = 512000;

			bool operator==(const SCryOdinEncoderSettings& other) const
			{
				return quality == other.quality && iBitrateBps == other.iBitrateBps && iExpectedPacketLossPercent == other.iExpectedPacketLossPercent
					&& bVoiceOptimized == other.bVoiceOptimized && iPositionUpdateIntervalMs == other.iPositionUpdateIntervalMs;
			}
			bool operator!=(const SCryOdinEncoderSettings& other) const { return !(*this == other); }

			static void ReflectType(Schematyc::CTypeDesc<SCryOdinEncoderSettings>& desc)
			{
				desc.SetGUID("{5970404F-5D1B-4001-AA2D-78BF2C48EBD8}"_cry_guid);
				desc.SetLabel("Odin Voice Quality Settings");
				desc.AddMember(&SCryOdinEncoderSettings::quality, 'qual', "Quality", "Quality", "Bitrate of your voice. Each listener receives this much per speaker. Custom uses the bitrate below", ECryOdinVoiceQuality::Custom);
				desc.AddMember(&SCryOdinEncoderSettings::iBitrateBps, 'bitr', "Bitrate", "Custom Bitrate (bits/s)", "Used when Quality is Custom, 6000 to 512000", 32000);
				desc.AddMember(&SCryOdinEncoderSettings::iExpectedPacketLossPercent, 'loss', "ExpectedPacketLoss", "Expected Packet Loss (%)", "Higher keeps the voice clearer on a bad connection, at some cost to quality", 15);
				desc.AddMember(&SCryOdinEncoderSettings::bVoiceOptimized, 'voip', "VoiceOptimized", "Voice Optimized", "Tunes the codec for speech. Turn off for music or other sounds", true);
			}

			//! The bitrate sent, from the preset or iBitrateBps.
			int GetBitrateBps() const
			{
				switch (quality)
				{
				case ECryOdinVoiceQuality::Low:      return 16000;
				case ECryOdinVoiceQuality::Standard: return 32000;
				case ECryOdinVoiceQuality::High:     return 64000;
				case ECryOdinVoiceQuality::VeryHigh: return 128000;
				default:                             return crymath::clamp(iBitrateBps, kMinBitrateBps, kMaxBitrateBps);
				}
			}

			//! Kilobits to bits per second, limited to what Odin takes.
			static int KbpsToBps(int kbps) { return crymath::clamp(kbps, kMinBitrateBps / 1000, kMaxBitrateBps / 1000) * 1000; }

			//! The settings with a quality preset, as the component starts with.
			static SCryOdinEncoderSettings MakeWithQuality(ECryOdinVoiceQuality preset)
			{
				SCryOdinEncoderSettings settings;
				settings.quality = preset;
				return settings;
			}

			int GetExpectedPacketLossPercent() const { return crymath::clamp(iExpectedPacketLossPercent, 0, 100); }
			int GetPositionUpdateIntervalMs() const { return std::max(iPositionUpdateIntervalMs, 0); }

			//! Same encoder result, so changing between them needs no new encoder.
			bool EncodesSameAs(const SCryOdinEncoderSettings& other) const
			{
				return GetBitrateBps() == other.GetBitrateBps() && GetExpectedPacketLossPercent() == other.GetExpectedPacketLossPercent()
					&& bVoiceOptimized == other.bVoiceOptimized && GetPositionUpdateIntervalMs() == other.GetPositionUpdateIntervalMs();
			}

			ECryOdinVoiceQuality quality = ECryOdinVoiceQuality::Custom; //!< When not Custom, iBitrateBps isn't used.
			int  iBitrateBps = 32000;          //!< Bits per second, 6000 to 512000. Used while quality is Custom, the default.
			int  iExpectedPacketLossPercent = 15;
			bool bVoiceOptimized = true;       //!< Opus tuned for speech. Off for music or other sounds.
			int  iPositionUpdateIntervalMs = 0; //!< Sends the position while silent, needed for proximity culling. 0 is off.
		};

		//! Everything needed to join a room.
		struct SCryOdinRoomParams
		{
			const char* szRoomId = "";       //!< Name of the room.
			const char* szUserName = "";     //!< Shown to other users, stored as "name" in the user data.
			const char* szUserId = nullptr;  //!< Your own id for this user, such as an account id. A random one is used when empty.
			const char* szToken = nullptr;   //!< Room token from your server. Without it, one is signed with the local access key.
			const char* szGateway = nullptr; //!< Odin gateway or server address. Null uses the 4Players gateway.
			const char* szUserDataJson = nullptr;  //!< JSON object synced to all users, name is added to it.
			const char* szExtraAuthJson = nullptr; //!< JSON object merged into the authentication, such as channel_masks, tags or parameters.
			const char* szPassword = nullptr;      //!< Enables end to end encryption with this shared password.

			bool bTransmit = true; //!< Initial state of ICryOdinRoom::SetTransmitEnabled.

			SCryOdinAudioProcessing processing;
			SCryOdinEncoderSettings encoder;

			//! Used for each voice in this room. 2D by default, so voices work with no entity attached.
			//! CCryOdinComponentSound replaces it for the voice it's bound to.
			SCryOdinVoiceSettings voice = MakeDefaultVoice();

			static SCryOdinVoiceSettings MakeDefaultVoice()
			{
				SCryOdinVoiceSettings settings;
				settings.positioning = ECryOdinVoicePositioning::Global2D;
				return settings;
			}
		};

		//! An entry of the channel mask list. Peer id 0 is the global mask.
		struct SCryOdinChannelMask
		{
			uint32 peerId = 0;
			uint64 mask = ~uint64(0);
		};

		struct SCryOdinConnectionStats
		{
			uint64 udpTxDatagrams = 0;
			uint64 udpTxBytes = 0;
			uint64 udpRxDatagrams = 0;
			uint64 udpRxBytes = 0;
			float  fRoundTripMs = 0.f;
		};

		struct SCryOdinAudioDeviceInfo
		{
			string name;
			bool   bDefault = false;
		};
	}
}
