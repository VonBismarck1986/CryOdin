#include "StdAfx.h"
#include "CryOdin.h"

#include "Plugin.h"

#include "CryOdinAudioSystem.h"
#include "CryOdinAudioSound.h"
#include "CryOdinUser.h"
#include "Components/CryOdinUserComponent.h"

#include "ICryOdinAudioHandle.h"

#include <CrySystem/ISystem.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/yasli/JSONIArchiveImpl.h>


namespace Cry
{
	namespace Odin
	{
		CCryOdin* CCryOdin::CCryOdin::s_instance = nullptr;

		CCryOdin::CCryOdin()
			: m_AccessKey(nullptr)
			, m_RoomToken(nullptr)
		{
			if (!s_instance)
			{
				s_instance = this;
			}

			ODIN_LOG("Starting CryOdin");

			m_pAudioSystem.reset(new CCryOdinAudioSystem());
			m_pAudioSystem->InitAudioSystem();

		}

		CCryOdin::~CCryOdin()
		{
		}

		CCryOdin* CCryOdin::Get()
		{
			return s_instance;
		}

		bool CCryOdin::InitOdin(const char* accessKey)
		{
			if (m_AccessKey.empty())
			{
				if (accessKey == nullptr)
				{
					char gen_access_key[128];

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
			auto started = odin_startup_ex(ODIN_VERSION, audio_output_config);
			return started;
		}

		void CCryOdin::Shutdown()
		{
			m_pAudioSystem->ShutdownAudioSystem();
			m_pAudioSystem.release();

			for (auto& it : m_userMap)
			{
				odin_media_stream_destroy(it.second.get()->GetOdinUser()->GetMediaHandle(EAudioHandleType::eAHT_Output));
			}

			if (m_localUser)
			{
				odin_media_stream_destroy(m_localUser->GetMediaHandle(EAudioHandleType::eAHT_Input));
				odin_room_close(m_room);
				odin_room_destroy(m_room);
				odin_shutdown();
			}

			if (s_instance)
			{
				SAFE_DELETE(s_instance);
				s_instance = nullptr;
			}
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

		}

		bool CCryOdin::SetUpLocalUser(const char* user_name, EntityId entityId)
		{
			//m_localUser = new CCryOdinUser(user_name, 0, entityId);
			//m_pAudioSystem->SetLocalUser(m_localUser);

			return true;
		}

		bool CCryOdin::JoinRoom(const char* room_name, const OdinApmConfig& config, CCryOdinUserComponent* odinComponent)
		{
			if (odinComponent->GetEntityId() != INVALID_ENTITYID)
			{
				if (odinComponent->GetEntityId() == LOCAL_PLAYER_ENTITY_ID)
				{
					odinComponent->SetOdinUser(new CCryOdinUser(odinComponent->GetName(), 0, odinComponent->GetEntityId()));
					m_localUser = static_cast<CCryOdinUser*>(odinComponent->GetOdinUser());
				}

				OdinReturnCode error;
				ODIN_LOG("Joining room with local user");

				m_room = odin_room_create();
				error = odin_room_set_event_callback(m_room, handle_odin_event, NULL);
				if (odin_is_error(error))
				{
					print_error(error, "Failed to set room event callback");
				}

				auto id = odinComponent->GetOdinUser()->GetUserId();
				auto user_id = string().Format("%i", id);
				string user_name = string().Format("{\"name\":\"%s\"}", odinComponent->GetOdinUser()->GetUserName().c_str());

				auto room_key = GenerateRoomToken(room_name, user_id.c_str());


				error = odin_room_update_peer_user_data(m_room, (uint8_t*)user_name.c_str(), strlen(user_name));
				if (odin_is_error(error))
				{
					print_error(error, "Failed to set user data");
					return false;
				}

				ODIN_LOG("Setting Username: %s", user_name);

				error = odin_room_configure_apm(m_room, config);
				if (odin_is_error(error))
				{
					print_error(error, "Failed to configure room audio processing options");
					return false;
				}

				ODIN_LOG("Joining room(%d) '%s' using '%s' with token '%s'\n", m_room, room_name, DEFAULT_ODIN_URL, room_key);
				error = odin_room_join(m_room, DEFAULT_ODIN_URL, room_key);
				if (odin_is_error(error))
				{
					print_error(error, "Failed to join room");
					return false;
				}

				OdinAudioStreamConfig streamConfig;
				streamConfig.channel_count = 1;
				streamConfig.sample_rate = 48000;

				odinComponent->GetOdinUser()->SetMediaHandle(EAudioHandleType::eAHT_Input, odin_audio_stream_create(streamConfig));


				error = odin_room_add_media(m_room, odinComponent->GetOdinUser()->GetMediaHandle(EAudioHandleType::eAHT_Input));
				if (odin_is_error(error))
				{
					print_error(error, "Failed to add media stream");
					return false;
				}

				m_pAudioSystem->SetLocalUser(odinComponent);

				m_pAudioSystem->SetInputHandle(odinComponent->GetOdinUser()->GetMediaHandle(EAudioHandleType::eAHT_Input));
				return true;
			}

			ODIN_LOG("Error unable to join room with local user make sure to setup local user");
			return false;
		}

		void CCryOdin::OnUpdate(float const frameTime)
		{
			m_pAudioSystem->OnUpdate(frameTime);
		}


		void CCryOdin::handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data)
		{
			s_instance->odinEvent(room, event, data);
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

					auto handle = m_localUser->GetMediaHandle(EAudioHandleType::eAHT_Input);

					// Handle connection timeouts and reconnects as we need to create a new input stream
					if (event->room_connection_state_changed.reason == OdinRoomConnectionStateChangeReason_ConnectionLost)
					{
						handle = 0;
						m_localUser->SetMediaHandle(EAudioHandleType::eAHT_Input, handle);

						if (event->room_connection_state_changed.state == OdinRoomConnectionState_Connected)
						{
							OdinAudioStreamConfig audio_output_config{};
							audio_output_config.channel_count = 1;
							audio_output_config.sample_rate = 48000;

							handle = odin_audio_stream_create(audio_output_config);
						}
					}

					// Stop client if the room handle gets disconnected (e.g. due to room close, kick/ban or connection issues)
					if (event->room_connection_state_changed.state == OdinRoomConnectionState_Disconnected)
					{
						if (event->room_connection_state_changed.reason != OdinRoomConnectionStateChangeReason_ClientRequested)
						{
							handle = 0;
							m_localUser->SetMediaHandle(EAudioHandleType::eAHT_Input, handle);
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
					uint64_t user_id = convertOdinUserIDToUint64(own_user_id);

					m_localUser->SetPeerID(peer_id);
					m_localUser->SetUserID(user_id);

					m_listeners.ForEach([&room_id](IListener* pListener) {
						pListener->OnJoinedRoom(room_id);
					});

					//m_userMap.emplace(std::make_pair(peer_id, std::move(localUser)));

					// Print information about joined room to the console
					ODIN_LOG("Room '%s' owned by '%s' joined successfully as Peer(%" PRIu64 ") with user ID '%d'\n", room_id, customer, peer_id, user_id);
					ODIN_LOG("OdinID set to (%" PRIu64 ")\n", m_localUser->GetPeerId());

				}
				break;
				case OdinEvent_PeerJoined:
				{
					//const char* user_id = event->peer_joined.user_id;
					uint64_t peer_id = event->peer_joined.peer_id;
					auto user_data = (char*)event->peer_joined.peer_user_data;
			
					// Create the object into which we will parse the data
					SNativeJsonOdinUser parsedObject;
					
					if (Serialization::LoadJsonBuffer(parsedObject, user_data, sizeof(user_data)))
					{
						auto id = convertOdinUserIDToUint64(parsedObject.userId.c_str());
						auto user = new CCryOdinUser(parsedObject.name, peer_id, id);

						user->SetRoomHandle(room);
						user->SetRoomName("fix-me");
						user->SetMicMuted(parsedObject.inputMuted);
						user->SetSpeakersMuted(parsedObject.outputMuted);
						user->SetOnlineStatus(parsedObject.status);

						auto pUserComponent = std::make_unique<CCryOdinUserComponent>();
						pUserComponent->SetOdinUser(std::move(user));

						m_listeners.ForEach([&pUserComponent](IListener* pListener) {
							pListener->OnPeerJoined(pUserComponent.get());
							});

						m_userMap.emplace(std::make_pair(peer_id, std::move(pUserComponent)));
					}
				}
				break;
				case OdinEvent_PeerLeft:
				{
					uint64_t peer_id = event->peer_left.peer_id;
					auto it = m_userMap.find(peer_id);
					if (it != m_userMap.end())
					{
						ODIN_LOG("Found Peer (%" PRIu64 ") removing from map ", peer_id);
						m_listeners.ForEach([&it](IListener* pListener) {
							pListener->OnPeerJoined(it->second.get());
						});
						m_userMap.erase(it);
					}
				}
				break;
				case OdinEvent_PeerUserDataChanged:
				{
					uint64_t peer_id = event->peer_user_data_changed.peer_id;
					auto data = event->peer_user_data_changed.peer_user_data;
					size_t peer_user_data_len = event->peer_user_data_changed.peer_user_data_len;

					// Print information about the peers user data to the console
					ODIN_LOG("Peer(%" PRIu64 ") user data updated with %zu bytes\n", peer_id, peer_user_data_len);
					ODIN_LOG("DataChange: %s", (const char*)data);
				}
				break;
				case OdinEvent_MediaAdded:
				{
					uint64_t peer_id = event->media_added.peer_id;
					uint16_t media_id = get_media_id_from_handle(event->media_added.media_handle);

					ODIN_LOG("Looking for (%d)...", peer_id);

					auto it = m_userMap.find(peer_id);

					if (it != m_userMap.end())
					{
						if (it->second)
						{
							auto user = it->second->GetOdinUser();
							if (user)
							{
								it->second->GetOdinUser()->SetMediaHandle(EAudioHandleType::eAHT_Output, event->media_added.media_handle);

								m_pAudioSystem->CreateAudioObject(*it->second);
								ODIN_LOG("Found Peer (%" PRIu64 ") and Added MediaHandle ( %d )", peer_id, media_id);
							}
						}
					
					}
					else {
						ODIN_LOG("Unable to find (%d)", peer_id);
					}
				}
				break;
				case OdinEvent_MediaActiveStateChanged:
				{
					//uint16_t media_id = get_media_id_from_handle(event->media_active_state_changed.media_handle);
					uint64_t peer_id = event->media_active_state_changed.peer_id;
					bool state = event->media_active_state_changed.active ? true : false;

					auto it = m_userMap.find(peer_id);
					if (it != m_userMap.end())
					{
						it->second->GetOdinUser()->Talking(state);
						ODIN_LOG("Peer (%" PRIu64 ") is %s", it->first, state ? "talking" : "stopped");
						break;
					}

					ODIN_LOG("Peer (%" PRIu64 ") is %s", peer_id, state ? "talking" : "stopped");

				}
				break;
				case OdinEvent_MediaRemoved:
				{
					uint64_t peer_id = event->media_removed.peer_id;
					//uint16_t media_id = get_media_id_from_handle(event->media_removed.media_handle);

					auto it = m_userMap.find(peer_id);
					if (it != m_userMap.end())
					{
						m_pAudioSystem->DestroyAudioObject(*it->second);
						it->second->GetOdinUser()->SetMediaHandle(EAudioHandleType::eAHT_Output, 0); // zero out.. maybe not needed

						//m_userMap.erase(it);
					}

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

	}
}