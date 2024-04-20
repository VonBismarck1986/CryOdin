#include "StdAfx.h"
#include "CryOdin.h"

#include "CryOdinAudioSystem.h"
#include "CryOdinAudioSound.h"

#include "Plugin.h"

namespace Cry
{
	namespace Odin
	{
		static std::unique_ptr<CCryOdin> g_pOdin;

		CCryOdin::CCryOdin()
			: m_AccessKey("AX907gsvRoGiz9qqhupOleCfR8q1BYE+8LcKwfgbGFqk") // for testing
			, m_pCurrentUser(nullptr)
			, m_RoomToken("")
			, m_room(0)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "Odin_SDK");

			if (!g_pOdin)
			{
				g_pOdin = std::make_unique<CCryOdin>(*this);
			}

			m_pAudioSystem = new CCryOdinAudioSystem();
			m_pAudioSystem->Init();
			ODIN_LOG("CryOdin System Init");
		}

		CCryOdin::~CCryOdin()
		{
		}

		CCryOdin* CCryOdin::Get()
		{
			return g_pOdin.get();
		}

		bool CCryOdin::Init(const char* accessKey)
		{
			char gen_access_key[128];

			if (m_AccessKey.empty())
			{
				if (accessKey == nullptr)
				{
					odin_access_key_generate(gen_access_key, sizeof(gen_access_key));
					m_AccessKey = gen_access_key;
				}
				else {
					m_AccessKey = accessKey;
				}

				ODIN_LOG("Access Key: %s", m_AccessKey.c_str());
			}
			else {
				ODIN_LOG("Access Key: %s", m_AccessKey.c_str());
			}


			OdinAudioStreamConfig audio_output_config;
			audio_output_config.channel_count = 2;
			audio_output_config.sample_rate = 48000;

			ODIN_LOG("Initializing ODIN runtime %s\n", ODIN_VERSION);
			odin_startup_ex(ODIN_VERSION, audio_output_config);

			return true;
		}

		void CCryOdin::Shutdown()
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

			if (m_pAudioSystem)
			{
				m_pAudioSystem->Shutdown();
				SAFE_DELETE(m_pAudioSystem);
			}

			odin_room_close(m_room);
			odin_room_destroy(m_room);

			odin_shutdown();

			if (g_pOdin)
			{
				g_pOdin.release();
			}
		}

		void CCryOdin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			switch (event)
			{
				case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
				{

				}
				break;
				default:
					break;
			}
		}

		void CCryOdin::SetAccessKey(const char* accessKey)
		{
			m_AccessKey = accessKey;
		}

		const char* CCryOdin::GenerateAccessKey() const
		{
			odin_access_key_generate(m_AccessKey.m_str, sizeof(m_AccessKey));
			return m_AccessKey.c_str();
		}

		const char* CCryOdin::GenerateRoomToken(const char* roomID, const char* user_id) const
		{
			OdinReturnCode error;

			OdinTokenGenerator* generator = odin_token_generator_create(m_AccessKey);
			if (!generator)
			{
				ODIN_LOG("Failed to initialize token generator; invalid access key\n");
			}

			OdinTokenOptions options;
			options.customer = "<no customer>";
			options.audience = OdinTokenAudience_Gateway;
			options.lifetime = 300;

			error = odin_token_generator_create_token_ex(generator, roomID, user_id, &options, m_RoomToken.m_str, sizeof(m_RoomToken));
			if (odin_is_error(error))
			{
				print_error(error, "Failed to generate room token");
				odin_token_generator_destroy(generator);
			}
			ODIN_LOG("RoomToken: %s", m_RoomToken.c_str());
			odin_token_generator_destroy(generator);

			return m_RoomToken.c_str();
		}

		void CCryOdin::SetOdinApmConfig(OdinApmConfig config)
		{
			m_pCurrentUser.apmConfig = config;
		}

		OdinApmConfig CCryOdin::GetCurrentOdinApmConfig() const
		{
			if (m_pCurrentUser.m_pEntity)
			{
				return m_pCurrentUser.apmConfig;
			}

			// send a default
			OdinApmConfig apmConfig{};
			apmConfig.voice_activity_detection = true;
			apmConfig.voice_activity_detection_attack_probability = 0.9;
			apmConfig.voice_activity_detection_release_probability = 0.8;
			apmConfig.volume_gate = false;
			apmConfig.volume_gate_attack_loudness = -30;
			apmConfig.volume_gate_release_loudness = -40;
			apmConfig.echo_canceller = true;
			apmConfig.high_pass_filter = false;
			apmConfig.pre_amplifier = false;
			apmConfig.noise_suppression_level = OdinNoiseSuppressionLevel_Moderate;
			apmConfig.transient_suppressor = false;
			apmConfig.gain_controller = true;


			return apmConfig;
		}

		bool CCryOdin::SetupAndConnectLocalClient(const IUser& pUser)
		{
			OdinReturnCode error;

			m_pCurrentUser = pUser;


			m_room = odin_room_create();
			error = odin_room_set_event_callback(m_room, handle_odin_event, NULL);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set room event callback");
			}

			string user_id = string().Format("%i", m_pCurrentUser.m_pEntity->GetId());
			const char* userID = user_id.c_str();

			string userName = string().Format("{\"name\":\"%s\"}", m_pCurrentUser.m_pEntity->GetName());
			string defaultRoom = string().Format("%s_default_room", m_pCurrentUser.m_pEntity->GetName());

			m_RoomToken = GenerateRoomToken(defaultRoom.c_str(), userID);
			m_pCurrentUser.userName = userName.c_str();

			error = odin_room_update_peer_user_data(m_room, (uint8_t*)m_pCurrentUser.userName, strlen(m_pCurrentUser.userName));
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set user data");
				return false;
			}
			ODIN_LOG("Setting Username: %s", m_pCurrentUser.userName);

			error = odin_room_configure_apm(m_room, m_pCurrentUser.apmConfig);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to configure room audio processing options");
				return false;
			}

			ODIN_LOG("Joining room '%s' using '%s' with token '%s'\n", defaultRoom.c_str(), DEFAULT_ODIN_URL, m_RoomToken.c_str());
			error = odin_room_join(m_room, DEFAULT_ODIN_URL, m_RoomToken.c_str());
			if (odin_is_error(error))
			{
				print_error(error, "Failed to join room");
				return false;
			}

			OdinAudioStreamConfig streamConfig;
			streamConfig.channel_count = 1;
			streamConfig.sample_rate = 48000;

			m_pCurrentUser.inputStream = odin_audio_stream_create(streamConfig);

			error = odin_room_add_media(m_room, m_pCurrentUser.inputStream);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to add media stream");
				return false;
			}

			m_pAudioSystem->AddLocalPlayer(m_pCurrentUser);

			return true;
		}

		void CCryOdin::ConnectUserToOdinRoom(const char* room_name)
		{
			OdinReturnCode error;
			odin_room_destroy(m_room);
			odin_room_close(m_room);

			m_room = odin_room_create();
			error = odin_room_set_event_callback(m_room, handle_odin_event, NULL);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set room event callback");
			}


			string user_id = string().Format("%i", m_pCurrentUser.m_pEntity->GetId());
			const char* userID = user_id.c_str();

			string userName = string().Format("{\"name\":\"%s\"}", m_pCurrentUser.m_pEntity->GetName());

			m_RoomToken = GenerateRoomToken(room_name, userID);
			m_pCurrentUser.userName = userName.c_str();

			error = odin_room_update_peer_user_data(m_room, (uint8_t*)m_pCurrentUser.userName, strlen(m_pCurrentUser.userName));
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set user data");

			}
			ODIN_LOG("Setting Username: %s", m_pCurrentUser.userName);

			error = odin_room_configure_apm(m_room, m_pCurrentUser.apmConfig);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to configure room audio processing options");

			}

			ODIN_LOG("Joining room '%s' using '%s' with token '%s'\n", room_name, DEFAULT_ODIN_URL, m_RoomToken.c_str());
			error = odin_room_join(m_room, DEFAULT_ODIN_URL, m_RoomToken.c_str());
			if (odin_is_error(error))
			{
				print_error(error, "Failed to join room");
			}

			OdinAudioStreamConfig streamConfig;
			streamConfig.channel_count = 1;
			streamConfig.sample_rate = 48000;

			m_pCurrentUser.inputStream = odin_audio_stream_create(streamConfig);

			error = odin_room_add_media(m_room, m_pCurrentUser.inputStream);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to add media stream");

			}

			

			error = odin_room_set_event_callback(m_room, handle_odin_event, NULL);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set room event callback");
			}

			ODIN_LOG("Room %s Joined", room_name);
		}

		void CCryOdin::RemoveLocalClientFromOdinRoom(const IUser& pEntity)
		{
			if (pEntity.m_pEntity)
			{
				odin_room_close(m_room);
			}
		}

		uint64_t CCryOdin::GetOdinUserId(const IUser entityId)
		{
			if (m_pCurrentUser.m_pEntity)
			{
				return m_pCurrentUser.peerID;
			}

			return 0;
		}

		void CCryOdin::GetPeers(std::unordered_map<uint64_t, IUser>& map) const
		{
			map = m_usersMap;
		}

		void CCryOdin::OnUpdate(float frameTime)
		{
			if (m_pAudioSystem)
			{
				m_pAudioSystem->OnUpdate(frameTime);
			}

			if (!m_sounds.empty())
			{
				for (auto& it : m_sounds)
				{
					it->OnUpdate(frameTime);
				}
			}
		}

		void CCryOdin::handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data)
		{
			g_pOdin->odinEvent(room, event, data);
		}

		void CCryOdin::odinEvent(OdinRoomHandle room, const OdinEvent* event, void* data)
		{
			switch (event->tag)
			{
				case OdinEvent_RoomConnectionStateChanged:
				{
					const char* connection_state_name = get_name_from_connection_state(event->room_connection_state_changed.state);
					const char* connection_state_reason = get_name_from_connection_state_change_reason(event->room_connection_state_changed.reason);

					ODIN_LOG("Connection state changed to '%s' due to %s\n", connection_state_name, connection_state_reason);

					// Handle connection timeouts and reconnects as we need to create a new input stream
					if (event->room_connection_state_changed.reason == OdinRoomConnectionStateChangeReason_ConnectionLost)
					{
						m_pCurrentUser.inputStream = 0;

						ODIN_LOG("ODIN ROOM ADD MEDIA - inputStream 0");

						if (event->room_connection_state_changed.state == OdinRoomConnectionState_Connected)
						{
							OdinAudioStreamConfig audio_output_config{};
							audio_output_config.channel_count = 1;
							audio_output_config.sample_rate = 48000;

							m_pCurrentUser.inputStream = odin_audio_stream_create(audio_output_config);
							odin_room_add_media(room, m_pCurrentUser.inputStream);
							ODIN_LOG("ODIN ROOM ADD MEDIA");
						}
					}

					// Stop client if the room handle gets disconnected (e.g. due to room close, kick/ban or connection issues)
					if (event->room_connection_state_changed.state == OdinRoomConnectionState_Disconnected)
					{
						if (event->room_connection_state_changed.reason != OdinRoomConnectionStateChangeReason_ClientRequested)
						{
							// ... need to add something here
							m_pCurrentUser.inputStream = 0;
							ODIN_LOG("ODIN ROOM MEDIA REMOVE");
						}
					}
				}
				break;
				case OdinEvent_Joined:
				{
					const char* room_id = event->joined.room_id;
					const char* customer = event->joined.customer;
					const char* own_user_id = event->joined.own_user_id;
					uint64_t peer_id = event->joined.own_peer_id;
					m_pCurrentUser.peerID = peer_id;

					// Print information about joined room to the console
					ODIN_LOG("Room '%s' owned by '%s' joined successfully as Peer(%" PRIu64 ") with user ID '%s'\n", room_id, customer, peer_id, own_user_id);
					ODIN_LOG("OdinID set to (%" PRIu64 ")\n", m_pCurrentUser.peerID = peer_id);

				}
				break;
				case OdinEvent_PeerJoined:
				{
					const char* user_id = event->peer_joined.user_id;
					uint64_t peer_id = event->peer_joined.peer_id;
					size_t peer_user_data_len = event->peer_joined.peer_user_data_len;

					// Print information about the peer to the console
					ODIN_LOG("Peer(%" PRIu64 ") joined with user ID '%s'\n", peer_id, user_id);
					// Print information about the peers user data to the console
					ODIN_LOG("Peer(%" PRIu64 ") has user data with %zu bytes\n", peer_id, peer_user_data_len);

					auto pEntity = gEnv->pEntitySystem->FindEntityByName("test_person");
					if (pEntity)
					{
						IUser newUser(std::move(pEntity), peer_id, user_id);
						m_usersMap.emplace(std::make_pair(peer_id, newUser)); // Probably just use emplace and allow std::map make the key

						ODIN_LOG("Placed user in id(%d)", peer_id);
					}
					else {
						ODIN_LOG("Unable to find");
					}

				}
				break;
				case OdinEvent_PeerLeft:
				{
					uint64_t peer_id = event->peer_left.peer_id;

					for (auto& peer : m_usersMap)
					{
						if (peer.second.peerID == peer_id)
						{
							//m_pAudioSystem->RemoveSoundSource(peer.second.inputStream, peer.second.m_pEntity->GetId());
							m_usersMap.erase(peer.first);
							break;
						}
					}

				}
				break;
				case OdinEvent_PeerUserDataChanged:
				{
					uint64_t peer_id = event->peer_user_data_changed.peer_id;
					size_t peer_user_data_len = event->peer_user_data_changed.peer_user_data_len;

					// Print information about the peers user data to the console
					ODIN_LOG("Peer(%" PRIu64 ") user data updated with %zu bytes\n", peer_id, peer_user_data_len);
				}
				break;
				case OdinEvent_MediaAdded:
				{
					uint64_t peer_id = event->media_added.peer_id;
					uint16_t media_id = get_media_id_from_handle(event->media_added.media_handle);

					ODIN_LOG("Looking for (%d)...", peer_id);

					auto it = m_usersMap.find(peer_id);
					if (it != m_usersMap.end())
					{
						it->second.mediaStream = media_id;

						CCryOdinAudioSound* sound = m_pAudioSystem->CreateSound(it->second);
						//m_pAudioSystem->InitSound(*sound);
						m_sounds.push_back(std::move(sound));

						//StartUpSounds();
						//m_pAudioSystem->AddSoundSource(event->media_added.media_handle, it->second.m_pEntity->GetId(), room);
						ODIN_LOG("Media(%d) added by Peer(%" PRIu64 ") with EntityID (%i)\n", media_id, it->second.peerID, it->second.m_pEntity->GetId());
					}
					else {
						ODIN_LOG("Unable to find (%d)", peer_id);
					}
				}
				break;
				case OdinEvent_MediaRemoved:
				{
					uint64_t peer_id = event->media_removed.peer_id;
					uint16_t media_id = get_media_id_from_handle(event->media_removed.media_handle);

					auto it = m_usersMap.find(peer_id);
					if (it != m_usersMap.end())
					{
						//m_pAudioSystem->RemoveSoundSource(event->media_removed.media_handle, it->second.m_pEntity->GetId());
					}
					ODIN_LOG("Media(%d) removed by Peer(%" PRIu64 ")\n", media_id, peer_id);
				}
				break;
				case OdinEvent_MediaActiveStateChanged:
				{
					//uint16_t media_id = get_media_id_from_handle(event->media_active_state_changed.media_handle);
					uint64_t peer_id = event->media_active_state_changed.peer_id;
					//const char* state = event->media_active_state_changed.active ? "started" : "stopped";
					bool talking = event->media_active_state_changed.active ? true : false;

					auto it = m_usersMap.find(peer_id);
					if (it != m_usersMap.end())
					{
						it->second.isTalking = talking;
						it->second.isMuted = talking;

						ODIN_LOG("Peer(%" PRIu64 ") %s ", peer_id, it->second.isTalking ? "talking" : "stoppedTalked");
					}
					ODIN_LOG("Peer(%" PRIu64 ") %s ", peer_id,talking ? "talking" : "stoppedTalked");
				}
				break;
			}
		}

		const char* CCryOdin::get_name_from_connection_state_change_reason(OdinRoomConnectionStateChangeReason reason)
		{
			switch (reason)
			{
			case OdinRoomConnectionStateChangeReason_ClientRequested:
				return "client request";
			case OdinRoomConnectionStateChangeReason_ServerRequested:
				return "server request";
			case OdinRoomConnectionStateChangeReason_ConnectionLost:
				return "timeout";
			default:
				return "unknown";
			}
		}

		const char* CCryOdin::get_name_from_connection_state(OdinRoomConnectionState state)
		{
			switch (state)
			{
			case OdinRoomConnectionState_Connecting:
				return "connecting";
			case OdinRoomConnectionState_Connected:
				return "connected";
			case OdinRoomConnectionState_Disconnecting:
				return "disconnecting";
			case OdinRoomConnectionState_Disconnected:
				return "disconnected";
			default:
				return "unknown";
			}
		}

		uint16_t CCryOdin::get_media_id_from_handle(OdinMediaStreamHandle handle)
		{
			uint16_t media_id;
			int error = odin_media_stream_media_id(handle, &media_id);
			return odin_is_error(error) ? 0 : media_id;
		}

		void CCryOdin::StartUpSounds()
		{
			for (auto it : m_sounds)
			{
				if (it)
				{
					//m_pAudioSystem->InitSound(static_cast<CCryOdinAudioSound*>(it));
				}
			}
		}



	}
}