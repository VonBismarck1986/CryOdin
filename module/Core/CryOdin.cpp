#include "StdAfx.h"
#include "CryOdin.h"

#include "CryOdinAudioSystem.h"
#include "CryOdinInput.h"
#include "CryOdinRoom.h"
#include "Utils/OdinJson.h"

#include <CrySystem/ConsoleRegistration.h>
#include <CrySystem/IConsole.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include <ctime>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			int g_cvarDebug = 0;
			int g_cvarOcclusionRaysPerFrame = 2;
			float g_cvarOcclusionPerHit = 0.5f;

			constexpr uint32 kAccessKeyBufferSize = 128;
			constexpr uint32 kTokenBufferSize = 1024;
			constexpr int kOdinLogVerbosityNeedsDebugLevel = 2;
			constexpr const char* kServerConfigFile = "dedicated.cfg";
			constexpr const char* kServerConfigPrefix = "odin_";
			// Each player gets this many token requests in each window.
			constexpr int kMaxServerRequestsPerWindow = 10;
			constexpr float kServerRequestWindowSeconds = 10.f;

			// Passes on only the odin_ lines of a config file. The game may load the same file itself, and
			// anything else in it mustn't happen twice.
			struct SOdinConfigSink final : public ILoadConfigurationEntrySink
			{
				virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) override
				{
					if (gEnv->pConsole != nullptr && strnicmp(szKey, kServerConfigPrefix, strlen(kServerConfigPrefix)) == 0)
					{
						gEnv->pConsole->LoadConfigVar(szKey, szValue);
						++count;
					}
				}

				int count = 0;
			};

			void CmdJoin(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin == nullptr || pArgs->GetArgCount() < 2)
				{
					ODIN_LOG("Usage: odin_join <room> [user name] [user id]");
					return;
				}

				SCryOdinRoomParams params;
				params.szRoomId = pArgs->GetArg(1);
				params.szUserName = pArgs->GetArgCount() > 2 ? pArgs->GetArg(2) : "";
				params.szUserId = pArgs->GetArgCount() > 3 ? pArgs->GetArg(3) : nullptr;
				pOdin->JoinRoom(params);
			}

			void CmdLeave(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin == nullptr)
				{
					return;
				}

				if (pArgs->GetArgCount() > 1)
				{
					pOdin->LeaveRoom(pOdin->FindRoom(pArgs->GetArg(1)));
					return;
				}
				for (int i = 0; i < pOdin->GetRoomCount(); ++i)
				{
					pOdin->LeaveRoom(pOdin->GetRoom(i));
				}
			}

			void CmdSay(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin == nullptr || pArgs->GetArgCount() < 2)
				{
					ODIN_LOG("Usage: odin_say <text>");
					return;
				}

				string text;
				for (int i = 1; i < pArgs->GetArgCount(); ++i)
				{
					if (i > 1)
					{
						text += " ";
					}
					text += pArgs->GetArg(i);
				}
				for (int i = 0; i < pOdin->GetRoomCount(); ++i)
				{
					ICryOdinRoom* pRoom = pOdin->GetRoom(i);
					ODIN_LOG("Text to room '%s': %s", pRoom->GetRoomId(), pRoom->SendText(text.c_str()) ? "sent" : "failed");
				}
				if (pOdin->GetRoomCount() == 0)
				{
					ODIN_LOG("No rooms joined, use odin_join first");
				}
			}

			void CmdDevices(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				ICryOdinAudioDevice* pDevices = pOdin != nullptr ? pOdin->GetAudioDevices() : nullptr;
				if (pDevices == nullptr)
				{
					return;
				}

				pDevices->RefreshDevices();
				ODIN_LOG("Input devices (selected %d):", pDevices->GetInputDeviceIndex());
				for (int i = 0; i < pDevices->GetNumberOfInputDevices(); ++i)
				{
					const SCryOdinAudioDeviceInfo info = pDevices->GetInputDeviceInfo(i);
					ODIN_LOG("  %d: %s%s", i, info.name.c_str(), info.bDefault ? " (default)" : "");
				}
				ODIN_LOG("Output devices (selected %d):", pDevices->GetOutputDeviceIndex());
				for (int i = 0; i < pDevices->GetNumberOfOutputDevices(); ++i)
				{
					const SCryOdinAudioDeviceInfo info = pDevices->GetOutputDeviceInfo(i);
					ODIN_LOG("  %d: %s%s", i, info.name.c_str(), info.bDefault ? " (default)" : "");
				}
			}

			void CmdSetInput(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				ICryOdinAudioDevice* pDevices = pOdin != nullptr ? pOdin->GetAudioDevices() : nullptr;
				if (pDevices != nullptr && pArgs->GetArgCount() > 1)
				{
					pDevices->ChangeInputDevice(atoi(pArgs->GetArg(1)));
				}
			}

			void CmdSetOutput(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				ICryOdinAudioDevice* pDevices = pOdin != nullptr ? pOdin->GetAudioDevices() : nullptr;
				if (pDevices != nullptr && pArgs->GetArgCount() > 1)
				{
					pDevices->ChangeOutputDevice(atoi(pArgs->GetArg(1)));
				}
			}

			void CmdMute(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				ICryOdinAudioSystem* pAudio = pOdin != nullptr ? pOdin->GetAudioSystem() : nullptr;
				if (pAudio != nullptr)
				{
					pAudio->SetMicrophoneMuted(pArgs->GetArgCount() > 1 ? atoi(pArgs->GetArg(1)) != 0 : !pAudio->IsMicrophoneMuted());
					ODIN_LOG("Microphone %s", pAudio->IsMicrophoneMuted() ? "muted" : "on");
				}
			}

			void CmdLoopback(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin != nullptr)
				{
					const bool bEnabled = pArgs->GetArgCount() < 2 || atoi(pArgs->GetArg(1)) != 0;
					for (int i = 0; i < pOdin->GetRoomCount(); ++i)
					{
						pOdin->GetRoom(i)->SetLocalLoopback(bEnabled);
					}
					if (pOdin->GetRoomCount() == 0)
					{
						ODIN_LOG("No rooms joined, use odin_join first");
					}
				}
			}

			// odin_radio <preset, or off>. Applies to all voices, including one from odin_loopback.
			void CmdRadio(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin == nullptr || pArgs->GetArgCount() < 2)
				{
					ODIN_LOG("Usage: odin_radio <off|clean|handheld|military|aircraft|damaged>");
					return;
				}

				const char* szPreset = pArgs->GetArg(1);
				SCryOdinRadioSettings settings;
				if (stricmp(szPreset, "off") == 0)
				{
					settings.bEnabled = false;
				}
				else if (stricmp(szPreset, "clean") == 0)
				{
					settings = GetCryOdinRadioPreset(ECryOdinRadioPreset::Clean);
				}
				else if (stricmp(szPreset, "handheld") == 0)
				{
					settings = GetCryOdinRadioPreset(ECryOdinRadioPreset::Handheld);
				}
				else if (stricmp(szPreset, "military") == 0)
				{
					settings = GetCryOdinRadioPreset(ECryOdinRadioPreset::Military);
				}
				else if (stricmp(szPreset, "aircraft") == 0)
				{
					settings = GetCryOdinRadioPreset(ECryOdinRadioPreset::Aircraft);
				}
				else if (stricmp(szPreset, "damaged") == 0)
				{
					settings = GetCryOdinRadioPreset(ECryOdinRadioPreset::Damaged);
				}
				else
				{
					ODIN_LOG("Unknown radio preset '%s'", szPreset);
					return;
				}

				if (pOdin->GetRoomCount() == 0)
				{
					ODIN_LOG("No rooms joined, use odin_join first");
				}
				for (int i = 0; i < pOdin->GetRoomCount(); ++i)
				{
					ICryOdinRoom* pRoom = pOdin->GetRoom(i);

					SCryOdinVoiceSettings defaults = pRoom->GetDefaultVoiceSettings();
					defaults.radio = settings;
					pRoom->SetDefaultVoiceSettings(defaults);

					// This is a test command, so it changes every voice. Only the radio, so that 3D voices stay 3D.
					for (int peer = 0; peer < pRoom->GetPeerCount(); ++peer)
					{
						if (ICryOdinSound* pSound = pRoom->GetPeer(peer)->GetSound())
						{
							pSound->SetRadioSettings(settings);
						}
					}
					if (ICryOdinUser* pLocal = pRoom->GetLocalUser())
					{
						if (ICryOdinSound* pSound = pLocal->GetSound())
						{
							pSound->SetRadioSettings(settings);
						}
					}
				}
			}

			// odin_bitrate <low|standard|high|veryhigh|kbps> [room]. A test command for all rooms, or one.
			void CmdBitrate(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin == nullptr || pArgs->GetArgCount() < 2)
				{
					ODIN_LOG("Usage: odin_bitrate <low|standard|high|veryhigh|kbps> [room]");
					return;
				}

				const char* szValue = pArgs->GetArg(1);
				ECryOdinVoiceQuality quality = ECryOdinVoiceQuality::Custom;
				int kbps = 0;
				if (stricmp(szValue, "low") == 0)
				{
					quality = ECryOdinVoiceQuality::Low;
				}
				else if (stricmp(szValue, "standard") == 0)
				{
					quality = ECryOdinVoiceQuality::Standard;
				}
				else if (stricmp(szValue, "high") == 0)
				{
					quality = ECryOdinVoiceQuality::High;
				}
				else if (stricmp(szValue, "veryhigh") == 0)
				{
					quality = ECryOdinVoiceQuality::VeryHigh;
				}
				else
				{
					kbps = atoi(szValue);
					if (kbps <= 0)
					{
						ODIN_LOG("Unknown bitrate '%s'", szValue);
						return;
					}
				}

				const char* szRoom = pArgs->GetArgCount() > 2 ? pArgs->GetArg(2) : nullptr;
				int changed = 0;
				for (int i = 0; i < pOdin->GetRoomCount(); ++i)
				{
					ICryOdinRoom* pRoom = pOdin->GetRoom(i);
					if (szRoom != nullptr && strcmp(pRoom->GetRoomId(), szRoom) != 0)
					{
						continue;
					}

					SCryOdinEncoderSettings settings = pRoom->GetEncoderSettings();
					settings.quality = quality;
					if (quality == ECryOdinVoiceQuality::Custom)
					{
						settings.iBitrateBps = SCryOdinEncoderSettings::KbpsToBps(kbps);
					}
					pRoom->SetEncoderSettings(settings);
					ODIN_LOG("Room '%s' will send at %d kbps", pRoom->GetRoomId(), settings.GetBitrateBps() / 1000);
					++changed;
				}
				if (changed == 0)
				{
					ODIN_LOG("No rooms joined, use odin_join first");
				}
			}

			void CmdSetKey(IConsoleCmdArgs* pArgs)
			{
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin != nullptr && pArgs->GetArgCount() > 1)
				{
					pOdin->SetAccessKey(pArgs->GetArg(1));
					ODIN_LOG("Access key set");
				}
			}
		}

		CCryOdin* CCryOdin::s_instance = nullptr;

		CCryOdin::CCryOdin()
		{
			s_instance = this;
			m_pInput = stl::make_unique<CCryOdinInput>();

			ODIN_LOG("Starting CryOdin");
			RegisterConsole();
		}

		CCryOdin::~CCryOdin()
		{
			Shutdown();
			UnregisterConsole();

			if (s_instance == this)
			{
				s_instance = nullptr;
			}
		}

		CCryOdin* CCryOdin::Get()
		{
			return s_instance;
		}

		void CCryOdin::RegisterConsole()
		{
			if (gEnv->pConsole == nullptr)
			{
				return;
			}

			REGISTER_CVAR2("odin_debug", &g_cvarDebug, 0, VF_NULL, "Draws Odin voice information on screen. 0: off, 1: on, 2: also passes the Odin SDK's own log to the console, set before Odin starts");
			REGISTER_CVAR2("odin_occlusionRaysPerFrame", &g_cvarOcclusionRaysPerFrame, 2, VF_NULL, "How many occlusion rays Odin voices can cast each frame");
			REGISTER_CVAR2("odin_occlusionPerHit", &g_cvarOcclusionPerHit, 0.5f, VF_NULL, "How much occlusion (0-1) each surface between a voice and the listener adds");
			// Invisible, so the console never shows the key. Never net synced, clients must not get it.
			m_pServerAccessKey = REGISTER_STRING("odin_serverAccessKey", "", VF_INVISIBLE, "Access key the server signs room tokens with, set in dedicated.cfg. Never put it in a client's config");
			m_pServerAllowedRooms = REGISTER_STRING("odin_serverAllowedRooms", "", VF_NULL, "Rooms the server signs tokens for, separated by commas. Empty allows any room");

			REGISTER_COMMAND("odin_join", &CmdJoin, VF_NULL, "Joins an Odin room: odin_join <room> [user name] [user id]");
			REGISTER_COMMAND("odin_leave", &CmdLeave, VF_NULL, "Leaves one Odin room, or all of them: odin_leave [room]");
			REGISTER_COMMAND("odin_say", &CmdSay, VF_NULL, "Sends text to all joined Odin rooms: odin_say <text>");
			REGISTER_COMMAND("odin_devices", &CmdDevices, VF_NULL, "Lists audio devices");
			REGISTER_COMMAND("odin_set_input", &CmdSetInput, VF_NULL, "Selects the microphone: odin_set_input <index>, -1 is the system default");
			REGISTER_COMMAND("odin_set_output", &CmdSetOutput, VF_NULL, "Selects the speakers: odin_set_output <index>, -1 is the system default");
			REGISTER_COMMAND("odin_mute", &CmdMute, VF_NULL, "Mutes the microphone: odin_mute [0|1]");
			REGISTER_COMMAND("odin_loopback", &CmdLoopback, VF_NULL, "Plays your own encoded voice back to you: odin_loopback [0|1]");
			REGISTER_COMMAND("odin_radio", &CmdRadio, VF_NULL, "Sets the radio effect of all voices: odin_radio <off|clean|handheld|military|aircraft|damaged>");
			REGISTER_COMMAND("odin_bitrate", &CmdBitrate, VF_NULL, "Sets the bitrate of your voice: odin_bitrate <low|standard|high|veryhigh|kbps> [room]");
#if !defined(_RELEASE)
			REGISTER_COMMAND("odin_set_key", &CmdSetKey, VF_NULL, "Sets the access key used to sign room tokens, for development only");
#endif

			m_bConsoleRegistered = true;
		}

		void CCryOdin::UnregisterConsole()
		{
			if (!m_bConsoleRegistered || gEnv->pConsole == nullptr)
			{
				return;
			}

			gEnv->pConsole->UnregisterVariable("odin_debug", true);
			gEnv->pConsole->UnregisterVariable("odin_occlusionRaysPerFrame", true);
			gEnv->pConsole->UnregisterVariable("odin_occlusionPerHit", true);
			gEnv->pConsole->UnregisterVariable("odin_serverAccessKey", true);
			gEnv->pConsole->UnregisterVariable("odin_serverAllowedRooms", true);
			m_pServerAccessKey = nullptr;
			m_pServerAllowedRooms = nullptr;

			gEnv->pConsole->RemoveCommand("odin_join");
			gEnv->pConsole->RemoveCommand("odin_leave");
			gEnv->pConsole->RemoveCommand("odin_say");
			gEnv->pConsole->RemoveCommand("odin_devices");
			gEnv->pConsole->RemoveCommand("odin_set_input");
			gEnv->pConsole->RemoveCommand("odin_set_output");
			gEnv->pConsole->RemoveCommand("odin_mute");
			gEnv->pConsole->RemoveCommand("odin_loopback");
			gEnv->pConsole->RemoveCommand("odin_radio");
			gEnv->pConsole->RemoveCommand("odin_bitrate");
#if !defined(_RELEASE)
			gEnv->pConsole->RemoveCommand("odin_set_key");
#endif

			m_bConsoleRegistered = false;
		}

		bool CCryOdin::IsDebugEnabled() const
		{
			return g_cvarDebug > 0;
		}

		void CCryOdin::OnEditorGameModeEnded()
		{
			// Nothing else would turn this off, and the next session would be silent with no sign why.
			if (m_pAudioSystem)
			{
				m_pAudioSystem->SetMicrophoneMuted(false);
			}
		}

		void CCryOdin::ShutdownInput()
		{
			m_pInput->Shutdown();
		}

		void CCryOdin::SubscribePushToTalk(IPushToTalkListener& listener, const char* szName, const SCryOdinPushToTalkKey& key, bool bReplaceInputs)
		{
			m_pInput->Subscribe(listener, szName, key, bReplaceInputs);
		}

		void CCryOdin::UnsubscribePushToTalk(IPushToTalkListener& listener)
		{
			m_pInput->Unsubscribe(listener);
		}

		void CCryOdin::NotifyTransmitChanged(ICryOdinRoom& room, bool bTransmitting)
		{
			// A listener asking for a shutdown has to wait, the room is on the call stack.
			++m_eventDepth;
			ForEachListener([&room, bTransmitting](IListener& listener) { listener.OnTransmitChanged(room, bTransmitting); });
			--m_eventDepth;
			if (m_bShutdownPending && m_eventDepth == 0)
			{
				ShutdownNow();
			}
		}

		void CCryOdin::OnOdinLog(const char* szJson)
		{
			// Runs on an Odin thread, which the log system allows.
			ODIN_LOG("[sdk] %s", szJson);
		}

		bool CCryOdin::EnsureStarted()
		{
			return m_bInitialized || (!m_bShutDown && InitOdin(nullptr));
		}

		bool CCryOdin::InitOdin(const char* accessKey)
		{
			if (accessKey != nullptr && accessKey[0] != '\0')
			{
				SetAccessKey(accessKey);
			}

			// It's still running but about to stop, so it can't be said to be started.
			if (m_bShuttingDown || m_bShutdownPending)
			{
				return false;
			}

			if (m_bInitialized)
			{
				m_bShutDown = false;
				return true;
			}

			ODIN_LOG("Initializing ODIN runtime %s", ODIN_VERSION);
			const OdinError error = odin_initialize(ODIN_VERSION);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to initialize the Odin runtime");
				return false;
			}

			if (g_cvarDebug >= kOdinLogVerbosityNeedsDebugLevel)
			{
				odin_debug_set_logging_hook(2, &CCryOdin::OnOdinLog);
			}

			// A dedicated server can sign tokens, but has no one to talk or listen.
			if (!gEnv->IsDedicated())
			{
				m_pAudioSystem = stl::make_unique<CCryOdinAudioSystem>();
				if (!m_pAudioSystem->InitAudioSystem())
				{
					m_pAudioSystem.reset();
					odin_debug_set_logging_hook(0, nullptr);
					odin_shutdown();
					return false;
				}
			}

			m_bInitialized = true;
			m_bShutDown = false;
			return true;
		}

		void CCryOdin::Shutdown()
		{
			m_bShutDown = true;

			// A room's events are on the call stack, so it can't be deleted yet.
			if (m_eventDepth > 0)
			{
				m_bShutdownPending = true;
				return;
			}

			ShutdownNow();
		}

		void CCryOdin::ShutdownNow()
		{
			m_bShutdownPending = false;

			if (!m_bInitialized || m_bShuttingDown)
			{
				return;
			}
			m_bShuttingDown = true;

			// Everyone holding a room or a user finds out now. By index, as new rooms can't be
			// made here but listeners can do anything else.
			for (size_t i = 0; i < m_rooms.size(); ++i)
			{
				m_rooms[i]->CloseNow("shutdown");
			}

			m_bInitialized = false;

			// Rooms use the audio system, and the Odin runtime is needed by both.
			m_rooms.clear();
			m_pAudioSystem.reset();

			odin_debug_set_logging_hook(0, nullptr);
			odin_shutdown();

			m_bShuttingDown = false;

			ODIN_LOG("Shut down");
		}

		string CCryOdin::GenerateAccessKey()
		{
			if (!EnsureStarted())
			{
				return string();
			}

			OdinTokenGenerator* pGenerator = nullptr;
			OdinError error = odin_token_generator_create(nullptr, &pGenerator);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to generate an access key");
				return string();
			}

			char accessKey[kAccessKeyBufferSize];
			uint32_t length = sizeof(accessKey);
			error = odin_token_generator_get_access_key(pGenerator, accessKey, &length);
			odin_token_generator_free(pGenerator);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to read the new access key");
				return string();
			}

			m_accessKey = accessKey;
			m_bTemporaryAccessKey = false;
			return m_accessKey;
		}

		void CCryOdin::SetAccessKey(const char* accessKey)
		{
			m_accessKey = accessKey != nullptr ? accessKey : "";
			m_bTemporaryAccessKey = false;
		}

		string CCryOdin::GetSigningKey()
		{
			if (!m_accessKey.empty() && !m_bTemporaryAccessKey)
			{
				return m_accessKey;
			}

			const char* szServerKey = m_pServerAccessKey != nullptr ? m_pServerAccessKey->GetString() : "";
			if (szServerKey != nullptr && szServerKey[0] != '\0')
			{
				return string(szServerKey);
			}

#if !defined(_RELEASE) && defined(ODIN_ACCESS_KEY)
			return string(ODIN_ACCESS_KEY);
#else
			if (m_accessKey.empty())
			{
				if (gEnv->IsDedicated())
				{
					ODIN_WARNING("No access key set, put odin_serverAccessKey in %s. Signing with a temporary key until then.", kServerConfigFile);
				}
				else
				{
					ODIN_LOG("No access key set, creating a temporary one. Clients with different keys can't meet in a room, and a shipped game should get its tokens from a server.");
				}
				if (GenerateAccessKey().empty())
				{
					return string();
				}
				m_bTemporaryAccessKey = true;
			}
			return m_accessKey;
#endif
		}

		void CCryOdin::LoadServerConfig()
		{
			if (gEnv->pSystem == nullptr)
			{
				return;
			}

			SOdinConfigSink sink;
			gEnv->pSystem->LoadConfiguration(kServerConfigFile, &sink, eLoadConfigInit, ELoadConfigurationFlags::SuppressConfigNotFoundWarning);

			// Never the key itself, logs get shared.
			const bool bHasKey = m_pServerAccessKey != nullptr && m_pServerAccessKey->GetString()[0] != '\0';
			if (bHasKey)
			{
				ODIN_LOG("Server signs room tokens with odin_serverAccessKey, %d odin setting(s) read from %s", sink.count, kServerConfigFile);
			}
			else
			{
				ODIN_WARNING("No odin_serverAccessKey in %s (or the file wasn't found next to the project or in its config folder). Players can't get room tokens signed with your key.", kServerConfigFile);
			}

			const bool bAllRooms = m_pServerAllowedRooms == nullptr || m_pServerAllowedRooms->GetString()[0] == '\0';
			if (bAllRooms)
			{
				ODIN_LOG("odin_serverAllowedRooms is empty, so players get tokens for any room they ask for, team rooms included. List your rooms there, or check them with SetServerTokenValidator.");
			}
		}

		bool CCryOdin::IsRoomAllowedOnServer(const char* szRoomId) const
		{
			const char* szAllowed = m_pServerAllowedRooms != nullptr ? m_pServerAllowedRooms->GetString() : "";
			if (szAllowed == nullptr || szAllowed[0] == '\0')
			{
				return true;
			}

			// Room names are compared exactly, as Odin does.
			const string allowed = szAllowed;
			int position = 0;
			string room = allowed.Tokenize(",", position);
			while (!room.empty())
			{
				room.Trim();
				if (room == szRoomId)
				{
					return true;
				}
				room = allowed.Tokenize(",", position);
			}
			return false;
		}

		bool CCryOdin::SignServerToken(SServerTokenRequest& request, string& outToken, string& outRefuseReason)
		{
			outToken.clear();
			outRefuseReason.clear();

			if (request.szRoomId == nullptr || request.szRoomId[0] == '\0')
			{
				outRefuseReason = "no room name";
				return false;
			}
			if (!IsRoomAllowedOnServer(request.szRoomId))
			{
				outRefuseReason.Format("room '%s' isn't in odin_serverAllowedRooms", request.szRoomId);
				return false;
			}
			// The token is signed for what was asked, whatever the validator does to these. The client
			// joins that room with that id.
			const string roomId = request.szRoomId;
			const string userId = request.szUserId != nullptr ? request.szUserId : "";
			if (m_pServerTokenValidator != nullptr && !m_pServerTokenValidator->ValidateServerTokenRequest(request, outRefuseReason))
			{
				if (outRefuseReason.empty())
				{
					outRefuseReason = "refused by the game";
				}
				return false;
			}

			outToken = GenerateRoomToken(roomId.c_str(), userId.c_str(), request.lifetimeSeconds);
			if (outToken.empty())
			{
				outRefuseReason = "the server couldn't sign the token";
				return false;
			}
			return true;
		}

		bool CCryOdin::AllowServerRequest(int channelId, bool& outLogRefusal)
		{
			const float now = gEnv->pTimer->GetAsyncCurTime();
			SRequestWindow& window = m_serverRequestWindows[channelId];
			if (window.count == 0 || now - window.start > kServerRequestWindowSeconds || now < window.start)
			{
				window.start = now;
				window.count = 0;
			}
			++window.count;
			outLogRefusal = window.count == kMaxServerRequestsPerWindow + 1;
			return window.count <= kMaxServerRequestsPerWindow;
		}

		string CCryOdin::GenerateRoomToken(const char* szRoomId, const char* szUserId, int lifetimeSeconds)
		{
			if (szRoomId == nullptr || szUserId == nullptr || !EnsureStarted())
			{
				return string();
			}

			const string signingKey = GetSigningKey();
			if (signingKey.empty())
			{
				return string();
			}

			OdinTokenGenerator* pGenerator = nullptr;
			OdinError error = odin_token_generator_create(signingKey.c_str(), &pGenerator);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to start signing tokens; invalid access key");
				return string();
			}

			const int64 now = static_cast<int64>(std::time(nullptr));

			OdinJson claims = OdinJson::object();
			claims["rid"] = szRoomId;
			claims["uid"] = szUserId;
			claims["nbf"] = now;
			claims["exp"] = now + std::max(lifetimeSeconds, 1);

			char token[kTokenBufferSize];
			uint32_t length = sizeof(token);
			const std::string body = OdinJsonDump(claims);
			error = odin_token_generator_sign(pGenerator, body.c_str(), token, &length);
			odin_token_generator_free(pGenerator);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to sign the room token");
				return string();
			}

			return string(token);
		}

		ICryOdinRoom* CCryOdin::JoinRoom(const SCryOdinRoomParams& params)
		{
			if (params.szRoomId == nullptr || params.szRoomId[0] == '\0')
			{
				ODIN_WARNING("Can't join a room without a name");
				return nullptr;
			}

			if (ICryOdinRoom* pExisting = FindRoom(params.szRoomId))
			{
				return pExisting;
			}

			if (m_bShuttingDown || m_bShutdownPending || !EnsureStarted())
			{
				return nullptr;
			}

			if (!m_pAudioSystem)
			{
				ODIN_LOG("Rooms can't be joined on a dedicated server");
				return nullptr;
			}

			// The Sandbox stops updating when another window has focus. Events would wait, and a user
			// joining meanwhile couldn't be heard. Easy to run into when testing two clients on one PC.
			if (gEnv->IsEditor() && gEnv->pConsole != nullptr)
			{
				const ICVar* pKeepActive = gEnv->pConsole->GetCVar("ed_keepEditorActive");
				if (pKeepActive != nullptr && pKeepActive->GetIVal() == 0)
				{
					ODIN_LOG("The Sandbox pauses voice events when it loses focus. Set ed_keepEditorActive 1 when testing with another client on this PC.");
				}
			}

			string userId = params.szUserId != nullptr ? params.szUserId : "";
			if (userId.empty())
			{
				userId = CryGUID::Create().ToString();
			}

			string token = params.szToken != nullptr ? params.szToken : "";
			if (token.empty())
			{
				token = GenerateRoomToken(params.szRoomId, userId.c_str(), 300);
				if (token.empty())
				{
					ODIN_WARNING("Can't join room '%s' without a token", params.szRoomId);
					return nullptr;
				}
			}

			SCryOdinRoomParams joinParams = params;
			joinParams.szUserId = userId.c_str();

			auto pRoom = stl::make_unique<CCryOdinRoom>(*this, *m_pAudioSystem, joinParams);
			if (!pRoom->Join(joinParams, token))
			{
				return nullptr;
			}

			CCryOdinRoom* pNewRoom = pRoom.get();
			m_rooms.push_back(std::move(pRoom));

			// Only now can a listener store it, or leave it.
			++m_eventDepth;
			pNewRoom->NotifyJoining();
			--m_eventDepth;
			if (m_bShutdownPending && m_eventDepth == 0)
			{
				ShutdownNow();
				return nullptr;
			}

			return pNewRoom;
		}

		void CCryOdin::LeaveRoom(ICryOdinRoom* pRoom)
		{
			for (const std::unique_ptr<CCryOdinRoom>& pExisting : m_rooms)
			{
				if (pExisting.get() == pRoom)
				{
					// Leaving sends events, and a listener asking for a shutdown there has to wait for this to end.
					++m_eventDepth;
					pExisting->Leave();
					--m_eventDepth;
					if (m_bShutdownPending && m_eventDepth == 0)
					{
						ShutdownNow();
					}
					return;
				}
			}
		}

		ICryOdinRoom* CCryOdin::FindRoom(const char* szRoomId) const
		{
			if (szRoomId != nullptr)
			{
				for (const std::unique_ptr<CCryOdinRoom>& pRoom : m_rooms)
				{
					// A room on its way out doesn't count, a new one can be joined with the same name.
					const ECryOdinRoomStatus status = pRoom->GetStatus();
					if (status != ECryOdinRoomStatus::Closing && status != ECryOdinRoomStatus::Closed && strcmp(pRoom->GetRoomId(), szRoomId) == 0)
					{
						return pRoom.get();
					}
				}
			}
			return nullptr;
		}

		ICryOdinRoom* CCryOdin::GetRoom(int index) const
		{
			return (index >= 0 && index < GetRoomCount()) ? m_rooms[index].get() : nullptr;
		}

		ICryOdinAudioSystem* CCryOdin::GetAudioSystem()
		{
			EnsureStarted();
			return m_pAudioSystem.get();
		}

		ICryOdinAudioDevice* CCryOdin::GetAudioDevices()
		{
			EnsureStarted();
			return m_pAudioSystem ? m_pAudioSystem->GetDevices() : nullptr;
		}

		void CCryOdin::OnUpdate(float const frameTime)
		{
			// Also before joining, so a key pressed then still works once joined.
			m_pInput->Update();

			// Nothing to do on a dedicated server, which never has rooms.
			if (!m_bInitialized || !m_pAudioSystem)
			{
				return;
			}

			// While this is above 0, a listener asking for a shutdown has it delayed until the end.
			++m_eventDepth;

			// By index, as a listener can join a room while this runs.
			for (size_t i = 0; i < m_rooms.size(); ++i)
			{
				CCryOdinRoom* pRoom = m_rooms[i].get();
				pRoom->OnUpdate(frameTime);
			}

			m_rooms.erase(std::remove_if(m_rooms.begin(), m_rooms.end(), [](const std::unique_ptr<CCryOdinRoom>& pRoom) { return pRoom->IsFinished(); }), m_rooms.end());

			bool bInputChanged = false;
			bool bOutputChanged = false;
			m_pAudioSystem->OnUpdate(frameTime, g_cvarOcclusionRaysPerFrame, g_cvarOcclusionPerHit, bInputChanged, bOutputChanged);
			if (bInputChanged)
			{
				ForEachListener([](IListener& listener) { listener.OnAudioDeviceChanged(true); });
			}
			if (bOutputChanged)
			{
				ForEachListener([](IListener& listener) { listener.OnAudioDeviceChanged(false); });
			}

			--m_eventDepth;

			if (m_bShutdownPending)
			{
				ShutdownNow();
				return;
			}

			if (g_cvarDebug > 0)
			{
				DrawDebug();
			}
		}

		void CCryOdin::DrawDebug()
		{
			if (gEnv->IsDedicated() || gEnv->pRenderer == nullptr)
			{
				return;
			}

			float fY = 60.f;
			const float fLine = 14.f;
			const float fSize = 1.3f;
			const ColorF white(1.f, 1.f, 1.f, 1.f);
			const ColorF green(0.4f, 1.f, 0.4f, 1.f);

			const CCryOdinAudioDevice* pDevices = m_pAudioSystem->GetAudioDevices();
			IRenderAuxText::Draw2dLabel(20.f, fY, fSize, white, false, "[Odin] in: %s (%.3f)%s  out: %s (%.3f)",
				pDevices->GetInputDeviceName().c_str(), m_pAudioSystem->GetInputLevel(), m_pAudioSystem->IsMicrophoneMuted() ? " MUTED" : "",
				pDevices->GetOutputDeviceName().c_str(), m_pAudioSystem->GetOutputLevel());
			fY += fLine;

			for (const std::unique_ptr<CCryOdinRoom>& pRoom : m_rooms)
			{
				SCryOdinConnectionStats stats;
				pRoom->GetConnectionStats(stats);

				static const char* s_statusNames[] = { "none", "joining", "joined", "closing", "closed" };
				IRenderAuxText::Draw2dLabel(20.f, fY, fSize, white, false, "Room '%s': %s, %d peers, rtt %.0f ms, tx %s, %d kbps",
					pRoom->GetRoomId(), s_statusNames[static_cast<int>(pRoom->GetStatus())], pRoom->GetPeerCount(), stats.fRoundTripMs, pRoom->IsTransmitEnabled() ? "on" : "off",
					pRoom->GetEncoderSettings().GetBitrateBps() / 1000);
				fY += fLine;

				if (ICryOdinUser* pLocal = pRoom->GetLocalUser())
				{
					IRenderAuxText::Draw2dLabel(30.f, fY, fSize, pLocal->IsTalking() ? green : white, false, "%s", pLocal->ToStringDebug().c_str());
					fY += fLine;
				}

				for (int i = 0; i < pRoom->GetPeerCount(); ++i)
				{
					ICryOdinUser* pUser = pRoom->GetPeer(i);
					const ICryOdinSound* pSound = pUser->GetSound();
					IRenderAuxText::Draw2dLabel(30.f, fY, fSize, pUser->IsTalking() ? green : white, false, "%s level %.3f occlusion %.2f%s",
						pUser->ToStringDebug().c_str(), pSound ? pSound->GetLevel() : 0.f, pSound ? pSound->GetOcclusion() : 0.f, (pSound && pSound->IsRadioEnabled()) ? " radio" : "");
					fY += fLine;
				}
			}
		}
	}
}
