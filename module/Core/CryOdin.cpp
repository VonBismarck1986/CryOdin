#include "StdAfx.h"
#include "CryOdin.h"
#include "CryOdinAudioDevices.h"

#include "Plugin.h"

#include <cstdlib> // maybe?


#define DEFAULT_ROOM "Cryengine-Room"

namespace Cry
{
	namespace Odin
	{

		static std::unique_ptr<CCryOdin> g_pOdin;

		CCryOdin::CCryOdin()
			: m_AccessKey("AX907gsvRoGiz9qqhupOleCfR8q1BYE+8LcKwfgbGFqk") // for testing
			, m_RoomToken("")
			, m_ApmConfig(OdinApmConfig())
			, m_pMainClient(ICryOdinUser())
			, m_room(0)
		{
			if (!g_pOdin)
			{
				g_pOdin = std::make_unique<CCryOdin>(*this);
			}
			ODIN_LOG(" CryOdin System");
			m_pAudioSystem = new CCryOdinAudioSystem();
			m_pAudioSystem->Init();

		}

		CCryOdin::~CCryOdin()
		{
			Shutdown();
		}

		void CCryOdin::Init(const char* accessKey, OdinApmConfig config)
		{
			char gen_access_key[128];

			m_ApmConfig = config;

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
		}

		void CCryOdin::Shutdown()
		{
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

		const char* CCryOdin::GetCurrentRoomToken() const
		{
			return m_RoomToken.c_str();
		}

		void CCryOdin::SetOdinApmConfig(OdinApmConfig config)
		{
			m_ApmConfig = config;
		}

		OdinApmConfig CCryOdin::GetCurrentOdinApmConfig() const
		{
			return m_ApmConfig;
		}

		void CCryOdin::ConnectUserToOdin(ICryOdinUser& pEntity, const char* user_name)
		{

			//TODO:: Clean up below..
			OdinReturnCode error;

			m_room = odin_room_create();

			error = odin_room_set_event_callback(m_room, handle_odin_event, NULL);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set room event callback");
			}

			string user_id = string().Format("%d", pEntity.pUserEntity->GetId());
			const char* userID = user_id.c_str();

			string userName = string().Format("{\"name\":\"%s\"}", user_name);

			m_pMainClient = pEntity;
			m_pMainClient.roomToken = GenerateRoomToken(DEFAULT_ROOM, userID);
			m_pMainClient.userName = userName.c_str();
			//m_pMainClient.OdinID = pEntity.pUserEntity->GetId();


			error = odin_room_update_peer_user_data(m_room, (uint8_t*)m_pMainClient.userName, strlen(m_pMainClient.userName));
			if (odin_is_error(error))
			{
				print_error(error, "Failed to set user data");
			}

			error = odin_room_configure_apm(m_room, m_ApmConfig);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to configure room audio processing options");
			}

			ODIN_LOG("Joining room '%s' using '%s' with token '%s'\n", DEFAULT_ROOM, DEFAULT_ODIN_URL, m_pMainClient.roomToken.c_str());
			error = odin_room_join(m_room, DEFAULT_ODIN_URL, m_pMainClient.roomToken.c_str());

			if (odin_is_error(error))
			{
				print_error(error, "Failed to join room");
			}

			OdinAudioStreamConfig streamConfig;
			streamConfig.channel_count = 1;
			streamConfig.sample_rate = 48000;

			m_pMainClient.inputStream = odin_audio_stream_create(streamConfig);

			error = odin_room_add_media(m_room, m_pMainClient.inputStream);
			if (odin_is_error(error))
			{
				print_error(error, "Failed to add media stream");
			}

			m_pAudioSystem->AddLocalPlayer(m_pMainClient);

			ODIN_LOG("Success");
		}

		void CCryOdin::RemoveUserFromOdinRoom(const char* room_name, const EntityId entityId)
		{
		}

		void CCryOdin::ConnectUserToOdinRoom(const char* room_name, const EntityId entityId, const char* user_name)
		{
		}

		uint64_t CCryOdin::GetOdinUserId(const EntityId entityId)
		{
			return 0;
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
					m_pMainClient.inputStream = 0;

					if (event->room_connection_state_changed.state == OdinRoomConnectionState_Connected)
					{
						OdinAudioStreamConfig audio_output_config;
						audio_output_config.channel_count = 1;
						audio_output_config.sample_rate = 48000;

						m_pMainClient.inputStream = odin_audio_stream_create(audio_output_config);
						odin_room_add_media(room, m_pMainClient.inputStream);

						//CCryOdinAudioDevice::Get()->SetInputStreamHandle(input_stream);
					}
				}

				// Stop client if the room handle gets disconnected (e.g. due to room close, kick/ban or connection issues)
				if (event->room_connection_state_changed.state == OdinRoomConnectionState_Disconnected)
				{
					if (event->room_connection_state_changed.reason != OdinRoomConnectionStateChangeReason_ClientRequested)
					{
						//exit(1);
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
				m_pMainClient.OdinID = peer_id;

				// Print information about joined room to the console
				ODIN_LOG("Room '%s' owned by '%s' joined successfully as Peer(%" PRIu64 ") with user ID '%s'\n", room_id, customer, peer_id, own_user_id);
				ODIN_LOG("OdinID set to (%" PRIu64 ")\n", m_pMainClient.OdinID);

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

				// convert user_id to EntityID
				unsigned int var = std::atoi(user_id);
				IEntity* temp = gEnv->pEntitySystem->GetEntity(var);

				ICryOdinUser newUser(std::move(temp), peer_id, user_id);

				m_usersMap.emplace(std::make_pair(peer_id,newUser)); // Probably just use emplace and allow std::map make the key

			}
			break;
			case OdinEvent_PeerLeft:
			{
				uint64_t peer_id = event->peer_left.peer_id;
				m_usersMap.erase(peer_id);
				//
				//// Walk through our global list of output streams and destroy the remaining ones owned by the peer just to be sure
				//for (size_t i = 0; i < output_streams_len; ++i)
				//{
				//	uint64_t stream_peer_id;
				//	odin_media_stream_peer_id(output_streams[i], &stream_peer_id);
				//	if (stream_peer_id == peer_id)
				//	{
				//		remove_output_stream(i);
				//	}
				//}
				//
				//// Print information about the peer to the console
				//ODIN_LOG("Peer(%" PRIu64 ") left\n", peer_id);
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

				auto it = m_usersMap.find(peer_id);
				if (it != m_usersMap.end())
				{
					//it->second.OdinID
					// Add the new output stream to our global list for later playback mixing
					m_pAudioSystem->AddSoundSource(event->media_added.media_handle, it->second.OdinID, room);

					// Print information about the media to the console
					ODIN_LOG("Media(%d) added by Peer(%" PRIu64 ")\n", media_id, it->second.OdinID);
				}
			}
			break;
			case OdinEvent_MediaRemoved:
			{
				//uint64_t peer_id = event->media_removed.peer_id;
				//uint16_t media_id = get_media_id_from_handle(event->media_removed.media_handle);
				//
				//// Find the output stream in our global list and destroy it
				//for (size_t i = 0; i < output_streams_len; ++i)
				//{
				//	if (output_streams[i] == event->media_removed.media_handle)
				//	{
				//		remove_output_stream(i);
				//		break;
				//	}
				//}
				//
				//// Print information about the media to the console
				//ODIN_LOG("Media(%d) removed by Peer(%" PRIu64 ")\n", media_id, peer_id);
			}
			break;
			case OdinEvent_MediaActiveStateChanged:
			{
				uint16_t media_id = get_media_id_from_handle(event->media_active_state_changed.media_handle);
				uint64_t peer_id = event->media_active_state_changed.peer_id;
				const char* state = event->media_active_state_changed.active ? "started" : "stopped";

				// Print information about the media activity update to the console
				ODIN_LOG("Peer(%" PRIu64 ") %s sending data on Media(%d)\n", peer_id, state, media_id);
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

		void CCryOdin::OnUpdate()
		{
			m_pAudioSystem->OnUpdate(0.f); // TODO:: Change this 0.f to actual vaule when needed
		}
	}
}