#include "StdAfx.h"
#include "CryOdinRoom.h"

#include "CryOdin.h"
#include "CryOdinAudioSystem.h"
#include "CryOdinAudioSound.h"
#include "Utils/OdinJson.h"

#include <odin_crypto.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// Marks text sent with SendText, so that a receiver can tell it from SendData.
			constexpr const char* kTextEnvelopeKey = "cryodin_text";
			constexpr const char* kChannelEnvelopeKey = "channel";
			constexpr float kClosingTimeoutSeconds = 5.f;
			// Changes closer together than this, such as from a slider, make one new encoder.
			constexpr float kEncoderRebuildDelaySeconds = 0.3f;

			// Turns the user data of an event into JSON text, and finds the name in it. Odin
			// clients send an object, others may send the JSON as bytes or text.
			std::string ReadUserData(const OdinJson& event)
			{
				const OdinJson* pUserData = OdinJsonFind(event, "user_data");
				if (pUserData == nullptr || pUserData->is_null())
				{
					return std::string();
				}

				if (pUserData->is_string())
				{
					return pUserData->get_ref<const std::string&>();
				}

				std::vector<uint8> bytes;
				if (OdinJsonBytes(*pUserData, bytes))
				{
					return std::string(bytes.begin(), bytes.end());
				}

				return OdinJsonDump(*pUserData);
			}

			std::string ReadNameFromUserData(const std::string& userData)
			{
				if (userData.empty())
				{
					return std::string();
				}
				const OdinJson value = OdinJsonParse(userData.c_str());
				return value.is_discarded() ? std::string() : OdinJsonString(value, "name");
			}

			OdinJson MakePeerIdList(const uint32* pPeerIds, uint32 peerCount)
			{
				OdinJson peerIds = OdinJson::array();
				for (uint32 i = 0; i < peerCount; ++i)
				{
					peerIds.push_back(pPeerIds[i]);
				}
				return peerIds;
			}
		}

		// CCryOdinSocket
		///////////////////////////////////////////////////////////////

		CCryOdinSocket::CCryOdinSocket(OdinSocket* pSocket)
			: m_pSocket(pSocket)
		{
			OdinSocketInfo info;
			if (odin_socket_info(m_pSocket, &info) == ODIN_ERROR_SUCCESS)
			{
				m_kind = info.kind == ODIN_SOCKET_KIND_RELIABLE ? ECryOdinSocketKind::Reliable : ECryOdinSocketKind::Unreliable;
				m_remotePeerId = info.remote_peer_id;
				m_label = info.label;
				m_priority = info.priority;
				m_bInbound = info.is_inbound;
			}
		}

		uint32 CCryOdinSocket::GetUnsentBytes() const
		{
			OdinSocketInfo info;
			return (m_pSocket != nullptr && odin_socket_info(m_pSocket, &info) == ODIN_ERROR_SUCCESS) ? info.unsent_bytes : 0;
		}

		bool CCryOdinSocket::Send(const void* pData, uint32 size)
		{
			if (m_pSocket == nullptr)
			{
				return false;
			}

			const OdinError error = odin_socket_send(m_pSocket, static_cast<const uint8_t*>(pData), size);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to send on socket");
				return false;
			}
			return true;
		}

		void CCryOdinSocket::CloseSending()
		{
			if (m_pSocket != nullptr)
			{
				odin_socket_reset(m_pSocket);
			}
		}

		// CCryOdinRoom
		///////////////////////////////////////////////////////////////

		CCryOdinRoom::CCryOdinRoom(CCryOdin& odin, CCryOdinAudioSystem& audioSystem, const SCryOdinRoomParams& params)
			: m_odin(odin)
			, m_audioSystem(audioSystem)
			, m_roomId(params.szRoomId)
			, m_processing(params.processing)
			, m_encoderSettings(params.encoder)
			, m_bTransmit(params.bTransmit)
			, m_defaultVoice(params.voice)
		{
			m_pDecoders = std::make_shared<const TDecoderMap>();
		}

		CCryOdinRoom::~CCryOdinRoom()
		{
			ForceClose();
		}

		bool CCryOdinRoom::Join(const SCryOdinRoomParams& params, const string& token)
		{
			m_localUserId = params.szUserId != nullptr ? params.szUserId : "";

			OdinJson userData = OdinJson::object();
			if (params.szUserDataJson != nullptr && params.szUserDataJson[0] != '\0')
			{
				OdinJson parsed = OdinJsonParse(params.szUserDataJson);
				if (parsed.is_object())
				{
					userData = std::move(parsed);
				}
				else
				{
					ODIN_WARNING("Ignoring user data for room '%s', it isn't a JSON object", m_roomId.c_str());
				}
			}
			if (params.szUserName != nullptr && params.szUserName[0] != '\0')
			{
				userData["name"] = params.szUserName;
			}
			m_localUserData = OdinJsonDump(userData);

			OdinJson authentication = OdinJson::object();
			if (params.szExtraAuthJson != nullptr && params.szExtraAuthJson[0] != '\0')
			{
				OdinJson parsed = OdinJsonParse(params.szExtraAuthJson);
				if (parsed.is_object())
				{
					authentication = std::move(parsed);
				}
				else
				{
					ODIN_WARNING("Ignoring extra authentication for room '%s', it isn't a JSON object", m_roomId.c_str());
				}
			}
			authentication["token"] = token.c_str();
			authentication["room_id"] = m_roomId.c_str();
			authentication["user_data"] = userData;

			m_events.on_datagram = OnDatagramCallback;
			m_events.on_rpc = OnRpcCallback;
			m_events.on_socket = OnSocketCallback;
			m_events.user_data = this;

			// The room owns the cipher from here on, even when creating the room fails.
			OdinCipher* pCipher = nullptr;
			if (params.szPassword != nullptr && params.szPassword[0] != '\0')
			{
				pCipher = odin_crypto_create(ODIN_CRYPTO_VERSION);
				if (pCipher == nullptr)
				{
					ODIN_WARNING("Failed to create the encryption for room '%s'", m_roomId.c_str());
					return false;
				}
				odin_crypto_set_password(pCipher, reinterpret_cast<const uint8_t*>(params.szPassword), static_cast<uint32_t>(strlen(params.szPassword)));
			}
			m_pCipher = pCipher;

			const char* szGateway = (params.szGateway != nullptr && params.szGateway[0] != '\0') ? params.szGateway : DEFAULT_ODIN_URL;

			// Events can arrive before this returns. They only go in the queue, so that's fine.
			const std::string authenticationText = OdinJsonDump(authentication);
			const OdinError error = odin_room_create(szGateway, authenticationText.c_str(), &m_events, pCipher, &m_pRoom);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to create room");
				m_pRoom = nullptr;
				m_pCipher = nullptr;
				return false;
			}

			m_status = ECryOdinRoomStatus::Joining;
			m_audioSystem.AcquireDevices();
			m_bHoldingDevices = true;

			ODIN_LOG("Joining room '%s' at %s", m_roomId.c_str(), szGateway);
			return true;
		}

		void CCryOdinRoom::NotifyJoining()
		{
			// Skip it if a queued event can't have changed the status yet. Only Leave could have.
			if (m_status == ECryOdinRoomStatus::Joining)
			{
				m_odin.ForEachListener([this](ICryOdin::IListener& listener)
				{
					if (m_status == ECryOdinRoomStatus::Joining)
					{
						listener.OnRoomStatusChanged(*this, ECryOdinRoomStatus::Joining, "");
					}
				});
			}
		}

		void CCryOdinRoom::CloseNow(const char* szReason)
		{
			if (!m_bFinished)
			{
				HandleClosed(szReason);
			}
		}

		void CCryOdinRoom::Leave()
		{
			if (m_pRoom == nullptr || m_status == ECryOdinRoomStatus::Closing || m_status == ECryOdinRoomStatus::Closed)
			{
				return;
			}

			ODIN_LOG("Leaving room '%s'", m_roomId.c_str());
			ChangeStatus(ECryOdinRoomStatus::Closing, "");
			DropSessionState();
			// The closed event follows, and is handled in OnUpdate.
			odin_room_close(m_pRoom);
		}

		void CCryOdinRoom::ForceClose()
		{
			DestroyEncoder();
			DetachLoopback();

			m_sockets.clear();
			m_peers.clear();
			m_pLocalUser.reset();
			m_retiredUsers.clear();
			m_retiredSockets.clear();

			auto pNoDecoders = std::make_shared<const TDecoderMap>();
			std::atomic_store(&m_pDecoders, std::shared_ptr<const TDecoderMap>(std::move(pNoDecoders)));

			CloseHandle();

			{
				CryAutoLock<CryCriticalSection> lock(m_queueLock);
				m_queue.clear();
			}

			if (m_bHoldingDevices)
			{
				m_bHoldingDevices = false;
				m_audioSystem.ReleaseDevices();
			}

			m_status = ECryOdinRoomStatus::Closed;
			m_bFinished = true;
		}

		void CCryOdinRoom::CloseHandle()
		{
			if (m_pRoom != nullptr)
			{
				odin_room_close(m_pRoom);
				// This waits for a running callback, which only takes the queue lock for a moment.
				odin_room_free(m_pRoom);
				m_pRoom = nullptr;
				m_pCipher = nullptr;
			}
		}

		// Network thread
		///////////////////////////////////////////////////////////////

		void CCryOdinRoom::OnDatagramCallback(OdinRoom* pRoom, const OdinDatagramProperties* pProperties, const uint8_t* pBytes, uint32_t length, void* pUserData)
		{
			CCryOdinRoom* pThis = static_cast<CCryOdinRoom*>(pUserData);

			const std::shared_ptr<const TDecoderMap> pDecoders = std::atomic_load(&pThis->m_pDecoders);
			if (pDecoders)
			{
				const auto it = pDecoders->find(pProperties->peer_id);
				if (it != pDecoders->end())
				{
					// A bad packet returns an error here, and there's nothing else to do about it.
					odin_decoder_push(it->second->pDecoder, pBytes, length);
				}
			}
		}

		void CCryOdinRoom::OnRpcCallback(OdinRoom* pRoom, const char* szJson, void* pUserData)
		{
			CCryOdinRoom* pThis = static_cast<CCryOdinRoom*>(pUserData);

			SQueuedEvent event;
			event.rpcJson = szJson;

			CryAutoLock<CryCriticalSection> lock(pThis->m_queueLock);
			pThis->m_queue.push_back(std::move(event));
		}

		void CCryOdinRoom::OnSocketCallback(OdinSocket* pSocket, const uint8_t* pMessage, uint32_t length, void* pUserData)
		{
			CCryOdinRoom* pThis = static_cast<CCryOdinRoom*>(pUserData);

			SQueuedEvent event;
			event.pSocket = pSocket;
			if (length > 0)
			{
				event.socketData.assign(pMessage, pMessage + length);
			}

			CryAutoLock<CryCriticalSection> lock(pThis->m_queueLock);
			pThis->m_queue.push_back(std::move(event));
		}

		void CCryOdinRoom::CaptureEffectCallback(float* pSamples, uint32_t sampleCount, bool* pIsSilent, const void* pUserData)
		{
			const SCaptureEffect* pEffect = static_cast<const SCaptureEffect*>(pUserData);
			// Odin always hands custom effects interleaved stereo at 48 kHz.
			pEffect->pEffect->Process(pSamples, sampleCount / 2, 2, kOdinSampleRate, pIsSilent != nullptr && *pIsSilent);
		}

		// Main thread events
		///////////////////////////////////////////////////////////////

		void CCryOdinRoom::OnUpdate(float frameTime)
		{
			// Nothing up the call stack can be reading these here.
			m_retiredUsers.clear();
			m_retiredSockets.clear();

			{
				CryAutoLock<CryCriticalSection> lock(m_queueLock);
				m_processing_queue.swap(m_queue);
			}

			for (const SQueuedEvent& event : m_processing_queue)
			{
				if (m_bFinished)
				{
					break;
				}
				if (event.pSocket != nullptr)
				{
					HandleSocket(event.pSocket, event.socketData);
				}
				else
				{
					HandleRpc(event.rpcJson.c_str());
				}
			}
			m_processing_queue.clear();

			if (m_bFinished)
			{
				return;
			}

			UpdateTalking(frameTime);
			UpdatePendingEncoder(frameTime);
			if (m_bFinished)
			{
				return;
			}
			UpdatePositionKeepAlive(frameTime);

			// The closed event should come right away. Don't hold on to the room forever if it doesn't.
			if (m_status == ECryOdinRoomStatus::Closing)
			{
				m_fClosingTime += frameTime;
				if (m_fClosingTime > kClosingTimeoutSeconds)
				{
					ODIN_WARNING("Room '%s' didn't confirm closing", m_roomId.c_str());
					HandleClosed("timeout");
				}
			}
		}

		void CCryOdinRoom::HandleRpc(const char* szJson)
		{
			m_odin.ForEachListener([this, szJson](ICryOdin::IListener& listener) { listener.OnRawRpc(*this, szJson); });

			const OdinJson rpc = OdinJsonParse(szJson);
			if (rpc.is_discarded() || !rpc.is_object() || rpc.empty())
			{
				ODIN_WARNING("Ignoring an invalid event from room '%s'", m_roomId.c_str());
				return;
			}

			// Each event is an object with its name as the only key.
			const auto first = rpc.begin();
			const std::string& name = first.key();
			const OdinJson& event = first.value();

			if (name == "RoomStatusChanged")
			{
				const std::string status = OdinJsonString(event, "status");
				const std::string message = OdinJsonString(event, "message");
				HandleStatusChanged(status.c_str(), message.c_str());
			}
			else if (name == "Joined")
			{
				const std::string roomId = OdinJsonString(event, "room_id");
				HandleJoined(OdinJsonUInt32(event, "own_peer_id"), roomId.c_str());
			}
			else if (name == "Left")
			{
				const std::string reason = OdinJsonString(event, "reason");
				HandleLeft(reason.c_str());
			}
			else if (name == "PeerJoined")
			{
				const std::string userId = OdinJsonString(event, "user_id");
				HandlePeerJoined(OdinJsonUInt32(event, "peer_id"), userId.c_str(), ReadUserData(event));
			}
			else if (name == "PeerLeft")
			{
				HandlePeerLeft(OdinJsonUInt32(event, "peer_id"));
			}
			else if (name == "PeerChanged")
			{
				if (OdinJsonFind(event, "user_data") != nullptr)
				{
					HandlePeerChanged(OdinJsonUInt32(event, "peer_id"), ReadUserData(event));
				}
			}
			else if (name == "MessageReceived")
			{
				if (const OdinJson* pMessage = OdinJsonFind(event, "message"))
				{
					HandleMessage(OdinJsonUInt32(event, "sender_peer_id"), pMessage);
				}
			}
			else if (name == "Error")
			{
				const std::string message = OdinJsonString(event, "message");
				ODIN_WARNING("Server error in room '%s': %s", m_roomId.c_str(), message.c_str());
				m_odin.ForEachListener([this, &message](ICryOdin::IListener& listener) { listener.OnRoomError(*this, message.c_str()); });
			}
			// Anything else, such as NewReconnectToken, isn't needed. Listeners can see it in OnRawRpc.
		}

		void CCryOdinRoom::ChangeStatus(ECryOdinRoomStatus status, const char* szMessage)
		{
			if (status != m_status)
			{
				m_status = status;
				// A listener can change the status again, and the others shouldn't hear them out of order.
				m_odin.ForEachListener([this, status, szMessage](ICryOdin::IListener& listener)
				{
					if (m_status == status)
					{
						listener.OnRoomStatusChanged(*this, status, szMessage);
					}
				});
			}
		}

		void CCryOdinRoom::HandleStatusChanged(const char* szStatus, const char* szMessage)
		{
			if (szMessage[0] != '\0')
			{
				ODIN_LOG("Room '%s' is %s: %s", m_roomId.c_str(), szStatus, szMessage);
			}

			if (strcmp(szStatus, "closed") == 0)
			{
				HandleClosed(szMessage);
			}
			else if (strcmp(szStatus, "joining") == 0)
			{
				// A lost connection comes back through here, and everything belonged to that session.
				if (m_status != ECryOdinRoomStatus::Closing)
				{
					DropSessionState();
				}
				// A listener could have left the room during that.
				if (m_status != ECryOdinRoomStatus::Closing)
				{
					ChangeStatus(ECryOdinRoomStatus::Joining, szMessage);
				}
			}
			// "joined" is reported when the Joined event arrives, once the users are known.
		}

		void CCryOdinRoom::HandleJoined(uint32 ownPeerId, const char* szRoomId)
		{
			if (m_status == ECryOdinRoomStatus::Closing)
			{
				return;
			}

			// Odin announces this client as a peer as well, which isn't known until now.
			if (CCryOdinUser* pSelf = FindUser(ownPeerId))
			{
				for (auto it = m_peers.begin(); it != m_peers.end(); ++it)
				{
					if (it->get() == pSelf)
					{
						m_localUserId = pSelf->GetUserId();
						pSelf->MarkRetired();
						m_retiredUsers.push_back(std::move(*it));
						m_peers.erase(it);
						break;
					}
				}
				PublishDecoders();
			}

			m_pLocalUser = stl::make_unique<CCryOdinUser>(*this, ownPeerId, true, m_localUserId.c_str());
			ApplyUserDataToUser(*m_pLocalUser, m_localUserData);

			CreateEncoder(ownPeerId);

			m_bSessionJoined = true;
			ChangeStatus(ECryOdinRoomStatus::Joined, "");

			ODIN_LOG("Joined room '%s' as peer %u", m_roomId.c_str(), ownPeerId);
			if (m_audioSystem.IsMicrophoneMuted())
			{
				ODIN_LOG("The microphone is muted, nothing will be sent to room '%s'", m_roomId.c_str());
			}

			// Users who were there first are announced now, so that the room is complete for the game.
			for (size_t i = 0; i < m_peers.size(); ++i)
			{
				CCryOdinUser& user = *m_peers[i];
				user.SetAnnounced();
				m_odin.ForEachListener([this, &user](ICryOdin::IListener& listener)
				{
					if (!user.IsRetired())
					{
						listener.OnPeerJoined(*this, user);
					}
				});
			}
			if (m_status == ECryOdinRoomStatus::Joined)
			{
				m_odin.ForEachListener([this](ICryOdin::IListener& listener) { listener.OnJoinedRoom(*this); });
			}
		}

		void CCryOdinRoom::HandleLeft(const char* szReason)
		{
			ODIN_LOG("Removed from room '%s': %s", m_roomId.c_str(), szReason);
			m_leaveReason = szReason;
			// The closed event follows this.
			if (m_pRoom != nullptr)
			{
				odin_room_close(m_pRoom);
			}
		}

		CCryOdinUser* CCryOdinRoom::CreatePeer(uint32 peerId, const char* szUserId, const std::string& userData)
		{
			m_peers.push_back(stl::make_unique<CCryOdinUser>(*this, peerId, false, szUserId));
			CCryOdinUser* pUser = m_peers.back().get();
			ApplyUserDataToUser(*pUser, userData);

			UpdatePasswordStatus(*pUser);
			CreateVoice(*pUser);
			return pUser;
		}

		void CCryOdinRoom::UpdatePasswordStatus(CCryOdinUser& user)
		{
			if (m_pCipher == nullptr)
			{
				return;
			}

			// Keys are exchanged through the user data, so this can change after the user has joined.
			const bool bMismatch = odin_crypto_get_peer_status(m_pCipher, user.GetPeerId()) == ODIN_CRYPTO_PEER_STATUS_PASSWORD_MISSMATCH;
			if (bMismatch && !user.HasPasswordMismatch())
			{
				ODIN_WARNING("Can't hear peer %u in room '%s', the room password doesn't match", user.GetPeerId(), m_roomId.c_str());
			}
			user.SetPasswordMismatch(bMismatch);
		}

		void CCryOdinRoom::HandlePeerJoined(uint32 peerId, const char* szUserId, const std::string& userData)
		{
			if (m_status == ECryOdinRoomStatus::Closing)
			{
				return;
			}
			if (m_pLocalUser && peerId == m_pLocalUser->GetPeerId())
			{
				return;
			}
			// The same peer again replaces the old one
			HandlePeerLeft(peerId);

			CCryOdinUser* pUser = CreatePeer(peerId, szUserId, userData);
			PublishDecoders();

			// Before the Joined event these are peers already in the room. HandleJoined announces them.
			if (m_bSessionJoined)
			{
				ODIN_LOG("%s joined room '%s'", pUser->ToStringDebug().c_str(), m_roomId.c_str());
				pUser->SetAnnounced();
				m_odin.ForEachListener([this, pUser](ICryOdin::IListener& listener)
				{
					if (!pUser->IsRetired())
					{
						listener.OnPeerJoined(*this, *pUser);
					}
				});
			}
		}

		void CCryOdinRoom::HandlePeerLeft(uint32 peerId)
		{
			for (auto it = m_peers.begin(); it != m_peers.end(); ++it)
			{
				if ((*it)->GetPeerId() == peerId)
				{
					// Listeners still get to read the user. It's deleted on the next update.
					CCryOdinUser& user = **it;
					m_retiredUsers.push_back(std::move(*it));
					m_peers.erase(it);
					PublishDecoders();
					user.MarkRetired();

					if (user.IsAnnounced())
					{
						ODIN_LOG("%s left room '%s'", user.ToStringDebug().c_str(), m_roomId.c_str());
						m_odin.ForEachListener([this, &user](ICryOdin::IListener& listener) { listener.OnPeerLeft(*this, user); });
					}
					return;
				}
			}
		}

		void CCryOdinRoom::HandlePeerChanged(uint32 peerId, const std::string& userData)
		{
			if (CCryOdinUser* pUser = FindUser(peerId))
			{
				ApplyUserDataToUser(*pUser, userData);
				UpdatePasswordStatus(*pUser);
				if (m_bSessionJoined)
				{
					m_odin.ForEachListener([this, pUser](ICryOdin::IListener& listener)
					{
						if (!pUser->IsRetired())
						{
							listener.OnPeerUserDataChanged(*this, *pUser);
						}
					});
				}
			}
		}

		void CCryOdinRoom::ApplyUserDataToUser(CCryOdinUser& user, const std::string& userData)
		{
			user.SetUserData(userData.c_str());
			user.SetName(ReadNameFromUserData(userData).c_str());
		}

		void CCryOdinRoom::HandleMessage(uint32 senderPeerId, const void* pMessageJson)
		{
			// The server may echo a message back to its sender.
			if (m_pLocalUser && senderPeerId == m_pLocalUser->GetPeerId())
			{
				return;
			}

			const OdinJson& message = *static_cast<const OdinJson*>(pMessageJson);
			CCryOdinUser* pSender = m_bSessionJoined ? FindUser(senderPeerId) : nullptr;

			std::string text;
			std::string channel;
			std::vector<uint8> bytes;
			bool bIsText = false;

			auto readEnvelope = [&](const OdinJson& object) -> bool
			{
				const OdinJson* pText = OdinJsonFind(object, kTextEnvelopeKey);
				if (pText == nullptr || !pText->is_string())
				{
					return false;
				}
				text = pText->get_ref<const std::string&>();
				channel = OdinJsonString(object, kChannelEnvelopeKey);
				return true;
			};

			const OdinJson* pBytesField = OdinJsonFind(message, "bytes");
			if (OdinJsonBytes(message, bytes) || (pBytesField != nullptr && OdinJsonBytes(*pBytesField, bytes)))
			{
				// Bytes are normally what's sent. It could be text from SendText, which is a small JSON object.
				if (!bytes.empty() && bytes.front() == '{')
				{
					const std::string bytesText(bytes.begin(), bytes.end());
					const OdinJson parsed = OdinJsonParse(bytesText.c_str());
					if (!parsed.is_discarded())
					{
						bIsText = readEnvelope(parsed);
					}
				}
			}
			else if (message.is_object())
			{
				bIsText = readEnvelope(message);
				if (!bIsText)
				{
					// Structured data from another client, hand it over as JSON text.
					const std::string dumped = OdinJsonDump(message);
					bytes.assign(dumped.begin(), dumped.end());
				}
			}
			else if (message.is_string())
			{
				text = message.get_ref<const std::string&>();
				bIsText = true;
			}
			else
			{
				ODIN_WARNING("Ignoring a message in an unknown format from peer %u", senderPeerId);
				return;
			}

			if (bIsText)
			{
				if (m_odin.IsDebugEnabled())
				{
					ODIN_LOG("[%s] %s: %s", m_roomId.c_str(), pSender != nullptr ? pSender->GetName() : "?", text.c_str());
				}
				m_odin.ForEachListener([&](ICryOdin::IListener& listener) { listener.OnTextMessage(*this, (pSender != nullptr && !pSender->IsRetired()) ? pSender : nullptr, text.c_str(), channel.c_str()); });
			}
			else
			{
				m_odin.ForEachListener([&](ICryOdin::IListener& listener) { listener.OnDataMessage(*this, (pSender != nullptr && !pSender->IsRetired()) ? pSender : nullptr, bytes.data(), static_cast<uint32>(bytes.size())); });
			}
		}

		void CCryOdinRoom::HandleSocket(OdinSocket* pSocket, const std::vector<uint8>& data)
		{
			// A message that was still queued when the connection ended.
			if (!m_bSessionJoined)
			{
				return;
			}

			CCryOdinSocket* pOdinSocket = nullptr;
			for (const std::unique_ptr<CCryOdinSocket>& pExisting : m_sockets)
			{
				if (pExisting->GetNativeHandle() == pSocket)
				{
					pOdinSocket = pExisting.get();
					break;
				}
			}

			// The first message of an inbound socket is how it becomes known.
			if (pOdinSocket == nullptr)
			{
				m_sockets.push_back(stl::make_unique<CCryOdinSocket>(pSocket));
				pOdinSocket = m_sockets.back().get();
			}

			m_odin.ForEachListener([&](ICryOdin::IListener& listener) { listener.OnSocketMessage(*this, *pOdinSocket, data.data(), static_cast<uint32>(data.size())); });
		}

		void CCryOdinRoom::HandleClosed(const char* szReason)
		{
			DropSessionState();
			CloseHandle();

			if (m_bHoldingDevices)
			{
				m_bHoldingDevices = false;
				m_audioSystem.ReleaseDevices();
			}

			const string reason = !m_leaveReason.empty() ? m_leaveReason : string(szReason);
			ChangeStatus(ECryOdinRoomStatus::Closed, reason.c_str());

			ODIN_LOG("Left room '%s'", m_roomId.c_str());
			m_odin.ForEachListener([this, &reason](ICryOdin::IListener& listener) { listener.OnLeftRoom(*this, reason.c_str()); });

			m_bFinished = true;
		}

		void CCryOdinRoom::DropSessionState()
		{
			DestroyEncoder();
			DetachLoopback();

			// Peers leave the list first, so listeners can't reach a stale one, and listeners only hear about
			// peers they were told about. Users are deleted on the next update, as a listener up
			// the call stack could still be reading one.
			const bool bWasJoined = m_bSessionJoined;
			m_bSessionJoined = false;

			// Taken before any listener runs, so that a nested call finds nothing and this one tells everyone.
			std::vector<std::unique_ptr<CCryOdinUser>> peers = std::move(m_peers);
			m_peers.clear();
			std::unique_ptr<CCryOdinUser> pLocalUser = std::move(m_pLocalUser);
			PublishDecoders();

			for (std::unique_ptr<CCryOdinUser>& pUser : peers)
			{
				pUser->MarkRetired();
			}
			if (pLocalUser)
			{
				pLocalUser->MarkRetired();
			}

			for (const std::unique_ptr<CCryOdinUser>& pUser : peers)
			{
				if (pUser->IsAnnounced())
				{
					CCryOdinUser& user = *pUser;
					m_odin.ForEachListener([this, &user](ICryOdin::IListener& listener) { listener.OnPeerLeft(*this, user); });
				}
			}
			if (pLocalUser && bWasJoined)
			{
				CCryOdinUser& user = *pLocalUser;
				m_odin.ForEachListener([this, &user](ICryOdin::IListener& listener) { listener.OnPeerLeft(*this, user); });
			}

			for (std::unique_ptr<CCryOdinUser>& pUser : peers)
			{
				m_retiredUsers.push_back(std::move(pUser));
			}
			if (pLocalUser)
			{
				m_retiredUsers.push_back(std::move(pLocalUser));
			}

			// Sockets belong to the connection and the room handle owns them. Listeners were able to use
			// them until here. Anything still holding one gets a dead socket until the next update.
			for (std::unique_ptr<CCryOdinSocket>& pSocket : m_sockets)
			{
				pSocket->Invalidate();
				m_retiredSockets.push_back(std::move(pSocket));
			}
			m_sockets.clear();
		}

		// Users
		///////////////////////////////////////////////////////////////

		ICryOdinUser* CCryOdinRoom::GetPeer(int index) const
		{
			return (index >= 0 && index < GetPeerCount()) ? m_peers[static_cast<size_t>(index)].get() : nullptr;
		}

		CCryOdinUser* CCryOdinRoom::FindUser(uint32 peerId) const
		{
			for (const std::unique_ptr<CCryOdinUser>& pUser : m_peers)
			{
				if (pUser->GetPeerId() == peerId)
				{
					return pUser.get();
				}
			}
			return nullptr;
		}

		ICryOdinUser* CCryOdinRoom::FindPeerByUserId(const char* szUserId) const
		{
			if (szUserId != nullptr && m_bSessionJoined)
			{
				for (const std::unique_ptr<CCryOdinUser>& pUser : m_peers)
				{
					if (strcmp(pUser->GetUserId(), szUserId) == 0)
					{
						return pUser.get();
					}
				}
			}
			return nullptr;
		}

		bool CCryOdinRoom::CreateVoice(CCryOdinUser& user)
		{
			OdinDecoder* pDecoder = nullptr;
			const OdinError error = odin_decoder_create(kOdinSampleRate, false, &pDecoder);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to create decoder");
				return false;
			}

			auto pDecoderHandle = std::make_shared<SCryOdinDecoder>(pDecoder);
			auto pSound = stl::make_unique<CCryOdinSound>(m_audioSystem, pDecoderHandle, user.GetPeerId(), m_defaultVoice);
			if (!pSound->IsValid())
			{
				return false;
			}

			user.SetVoice(std::move(pDecoderHandle), std::move(pSound));
			return true;
		}

		void CCryOdinRoom::PublishDecoders()
		{
			auto pNewDecoders = std::make_shared<TDecoderMap>();
			for (const std::unique_ptr<CCryOdinUser>& pUser : m_peers)
			{
				if (pUser->GetDecoder())
				{
					pNewDecoders->emplace(pUser->GetPeerId(), pUser->GetDecoder());
				}
			}
			std::atomic_store(&m_pDecoders, std::shared_ptr<const TDecoderMap>(std::move(pNewDecoders)));
		}

		void CCryOdinRoom::UpdateTalking(float frameTime)
		{
			if (!m_bSessionJoined)
			{
				return;
			}

			// The signals only update while their devices run. Without one, the last state would stay forever.
			CCryOdinAudioDevice* pDevices = m_audioSystem.GetAudioDevices();
			const bool bOutputRunning = pDevices != nullptr && pDevices->IsOutputRunning();
			const bool bInputRunning = pDevices != nullptr && pDevices->IsInputRunning();

			auto notify = [this](CCryOdinUser& user)
			{
				const bool bTalking = user.IsTalking();
				m_odin.ForEachListener([this, &user, bTalking](ICryOdin::IListener& listener)
				{
					if (!user.IsRetired())
					{
						listener.OnPeerTalkingChanged(*this, user, bTalking);
					}
				});
			};

			// By index, a listener can change the list. It stops being joined if it leaves.
			for (size_t i = 0; m_bSessionJoined && i < m_peers.size(); ++i)
			{
				CCryOdinUser& user = *m_peers[i];
				const ICryOdinSound* pSound = user.GetSound();
				if (user.UpdateTalking(bOutputRunning && pSound != nullptr && pSound->IsTalking(), frameTime))
				{
					notify(user);
				}
			}

			// Without an encoder, such as after a failed new one, nothing is sent, so that's not talking.
			if (m_bSessionJoined && m_pLocalUser)
			{
				CCryOdinUser& user = *m_pLocalUser;
				const bool bSending = m_pCaptureTarget && bInputRunning && !m_pCaptureTarget->bSilent.load(std::memory_order_relaxed);
				if (user.UpdateTalking(bSending, frameTime))
				{
					notify(user);
				}
			}
		}

		void CCryOdinRoom::UpdatePositionKeepAlive(float frameTime)
		{
			if (m_pEncoder == nullptr || m_encoderSettings.GetPositionUpdateIntervalMs() <= 0)
			{
				// A new encoder without position updates mustn't keep the voice closed off.
				if (m_bPositionGateForced)
				{
					m_bPositionGateForced = false;
					ApplyAudioProcessing();
				}
				return;
			}

			CCryOdinAudioDevice* pDevices = m_audioSystem.GetAudioDevices();
			const bool bMicRunning = pDevices != nullptr && pDevices->IsInputRunning();

			// While not sending voice, the encoder still has to run for the server to get positions.
			// A closed gate lets through only those. Digital silence would otherwise be sent as voice.
			const bool bGateForced = !m_bTransmit || m_audioSystem.IsMicrophoneMuted() || !bMicRunning;
			if (bGateForced != m_bPositionGateForced)
			{
				m_bPositionGateForced = bGateForced;
				ApplyAudioProcessing();
			}

			// With no microphone there's no audio thread running the encoder, so do it here.
			if (!bMicRunning && frameTime > 0.f && m_pCaptureTarget && m_pCaptureTarget->bPublishing.load(std::memory_order_relaxed))
			{
				static const float s_silence[kOdinSampleRate / 50] = {};
				const uint32 samples = std::min<uint32>(static_cast<uint32>(frameTime * kOdinSampleRate), CRY_ARRAY_COUNT(s_silence));
				odin_encoder_push(m_pEncoder, s_silence, samples);

				for (;;)
				{
					uint8_t datagram[2048];
					uint32_t length = sizeof(datagram);
					if (odin_encoder_pop(m_pEncoder, datagram, &length) != ODIN_ERROR_SUCCESS)
					{
						break;
					}
					odin_room_send_datagram(m_pRoom, datagram, length);
				}
			}
		}

		void CCryOdinRoom::SetDefaultVoiceSettings(const SCryOdinVoiceSettings& settings, bool bApplyToExistingVoices)
		{
			m_defaultVoice = settings;

			if (bApplyToExistingVoices)
			{
				for (const std::unique_ptr<CCryOdinUser>& pUser : m_peers)
				{
					CCryOdinSound* pSound = pUser->GetVoice();
					if (pSound != nullptr && !pSound->IsIgnoringRoomDefaults())
					{
						pSound->SetSettings(settings);
					}
				}
				if (m_pLocalUser)
				{
					CCryOdinSound* pSound = m_pLocalUser->GetVoice();
					if (pSound != nullptr && !pSound->IsIgnoringRoomDefaults())
					{
						pSound->SetSettings(settings);
					}
				}
			}
		}

		bool CCryOdinRoom::SetUserData(const char* szJson)
		{
			if (m_pRoom == nullptr || szJson == nullptr)
			{
				return false;
			}

			const OdinJson userData = OdinJsonParse(szJson);
			if (userData.is_discarded() || !userData.is_object())
			{
				ODIN_WARNING("SetUserData needs a JSON object");
				return false;
			}

			OdinJson change = OdinJson::object();
			change["user_data"] = userData;
			OdinJson command = OdinJson::object();
			command["ChangeSelf"] = std::move(change);

			m_localUserData = OdinJsonDump(userData);
			if (m_pLocalUser)
			{
				ApplyUserDataToUser(*m_pLocalUser, m_localUserData);
			}

			return SendRpc(OdinJsonDump(command).c_str());
		}

		// Text and data
		///////////////////////////////////////////////////////////////

		bool CCryOdinRoom::SendRpc(const char* szJson)
		{
			if (m_pRoom == nullptr || szJson == nullptr)
			{
				return false;
			}

			const OdinError error = odin_room_send_rpc(m_pRoom, szJson);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to send to room");
				return false;
			}
			return true;
		}

		bool CCryOdinRoom::SendMessagePayload(const void* pData, uint32 size, const uint32* pPeerIds, uint32 peerCount)
		{
			OdinJson message = OdinJson::object();
			message["message"] = OdinJsonFromBytes(pData, size);
			// Leaving this out sends to everyone. A null value is rejected by Odin.
			if (pPeerIds != nullptr && peerCount > 0)
			{
				message["peer_ids"] = MakePeerIdList(pPeerIds, peerCount);
			}

			OdinJson command = OdinJson::object();
			command["SendMessage"] = std::move(message);
			return SendRpc(OdinJsonDump(command).c_str());
		}

		bool CCryOdinRoom::SendData(const void* pData, uint32 size, const uint32* pPeerIds, uint32 peerCount)
		{
			if (pData == nullptr || size == 0)
			{
				return false;
			}
			return SendMessagePayload(pData, size, pPeerIds, peerCount);
		}

		bool CCryOdinRoom::SendText(const char* szText, const char* szChannel, const uint32* pPeerIds, uint32 peerCount)
		{
			if (szText == nullptr || szText[0] == '\0')
			{
				return false;
			}

			OdinJson envelope = OdinJson::object();
			envelope[kTextEnvelopeKey] = szText;
			if (szChannel != nullptr && szChannel[0] != '\0')
			{
				envelope[kChannelEnvelopeKey] = szChannel;
			}

			const std::string payload = OdinJsonDump(envelope);
			return SendMessagePayload(payload.data(), static_cast<uint32>(payload.size()), pPeerIds, peerCount);
		}

		ICryOdinSocket* CCryOdinRoom::OpenSocket(ECryOdinSocketKind kind, uint32 remotePeerId, int label, int priority)
		{
			if (m_pRoom == nullptr || !m_bSessionJoined)
			{
				return nullptr;
			}

			OdinSocket* pSocket = nullptr;
			const OdinSocketKind odinKind = kind == ECryOdinSocketKind::Reliable ? ODIN_SOCKET_KIND_RELIABLE : ODIN_SOCKET_KIND_UNRELIABLE;
			const OdinError error = odin_socket_create(m_pRoom, odinKind, remotePeerId, label, priority, &pSocket);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to open socket");
				return nullptr;
			}

			m_sockets.push_back(stl::make_unique<CCryOdinSocket>(pSocket));
			return m_sockets.back().get();
		}

		// Microphone
		///////////////////////////////////////////////////////////////

		bool CCryOdinRoom::CreateEncoder(uint32 ownPeerId)
		{
			DestroyEncoder();

			// Despite its name, Odin's bitrate_kbps argument is in bits per second.
			OdinEncoder* pEncoder = nullptr;
			const OdinError error = odin_encoder_create_ex(ownPeerId, kOdinSampleRate, false, m_encoderSettings.bVoiceOptimized,
				static_cast<uint32_t>(m_encoderSettings.GetBitrateBps()),
				static_cast<uint32_t>(m_encoderSettings.GetExpectedPacketLossPercent()),
				static_cast<uint64_t>(m_encoderSettings.GetPositionUpdateIntervalMs()),
				&pEncoder);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to create encoder");
				return false;
			}

			m_pEncoder = pEncoder;
			m_pPipeline = odin_encoder_get_pipeline(pEncoder);
			m_apmEffectId = 0;
			m_vadEffectId = 0;
			m_viEffectId = 0;

			// Order from the Odin docs: APM needs the original signal, VI comes before VAD which works better on the clean voice.
			OdinError effectError = odin_pipeline_insert_apm_effect(m_pPipeline, odin_pipeline_get_effect_count(m_pPipeline), kOdinSampleRate, kOdinOutputChannels == 2, &m_apmEffectId);
			if (effectError != ODIN_ERROR_SUCCESS)
			{
				print_error(effectError, "Failed to add the audio processing effect");
				m_apmEffectId = 0;
			}
			effectError = odin_pipeline_insert_vad_effect(m_pPipeline, odin_pipeline_get_effect_count(m_pPipeline), &m_vadEffectId);
			if (effectError != ODIN_ERROR_SUCCESS)
			{
				print_error(effectError, "Failed to add the voice detection effect");
				m_vadEffectId = 0;
			}

			m_pCaptureTarget = std::make_shared<SCryOdinCaptureTarget>();
			m_pCaptureTarget->pEncoder = m_pEncoder;
			m_pCaptureTarget->pRoom = m_pRoom;
			m_pCaptureTarget->pPipeline = m_pPipeline;
			m_pCaptureTarget->apmEffectId = m_apmEffectId;
			m_pCaptureTarget->bTransmit.store(m_bTransmit, std::memory_order_relaxed);

			ApplyAudioProcessing();

			for (const std::unique_ptr<SCaptureEffect>& pEffect : m_captureEffects)
			{
				InsertCaptureEffect(*pEffect);
			}

			m_bEncoderHasDefaultChannel = true;
			ApplyPositionsToEncoder();

			if (m_bLoopback)
			{
				if (m_pLoopbackDecoder)
				{
					// A new encoder in the same session keeps the local user's voice, which game code can hold.
					m_pCaptureTarget->pLoopbackDecoder.store(m_pLoopbackDecoder->pDecoder, std::memory_order_seq_cst);
				}
				else
				{
					SetLocalLoopback(true);
				}
			}

			m_audioSystem.RegisterCaptureTarget(m_pCaptureTarget);

			// This one already has the latest settings, also when joining again with a change still waiting.
			m_builtEncoderSettings = m_encoderSettings;
			m_bEncoderRebuildPending = false;
			return true;
		}

		void CCryOdinRoom::SetEncoderSettings(const SCryOdinEncoderSettings& settings)
		{
			m_encoderSettings = settings;

			// Before joining it's used by the encoder made on joining.
			if (m_pEncoder == nullptr)
			{
				return;
			}

			// Compared with the encoder there is, so going back and forth within the delay changes nothing.
			if (settings.EncodesSameAs(m_builtEncoderSettings))
			{
				m_bEncoderRebuildPending = false;
				return;
			}

			// Done from the update, never from inside a caller's own event, and once for many changes.
			m_bEncoderRebuildPending = true;
			m_fEncoderRebuildDelay = kEncoderRebuildDelaySeconds;
		}

		void CCryOdinRoom::UpdatePendingEncoder(float frameTime)
		{
			if (!m_bEncoderRebuildPending)
			{
				return;
			}

			m_fEncoderRebuildDelay -= frameTime;
			if (m_fEncoderRebuildDelay > 0.f)
			{
				return;
			}
			m_bEncoderRebuildPending = false;

			// Left or reconnecting since. The next join makes the encoder with these settings anyway.
			if (m_pEncoder == nullptr || !m_bSessionJoined || !m_pLocalUser || m_encoderSettings.EncodesSameAs(m_builtEncoderSettings))
			{
				return;
			}

			// The new encoder starts without the gate that position updates use. The update sets it again.
			m_bPositionGateForced = false;

			const bool bSucceeded = CreateEncoder(m_pLocalUser->GetPeerId());
			if (bSucceeded)
			{
				ODIN_LOG("Room '%s' now sends voice at %d kbps%s", m_roomId.c_str(), m_encoderSettings.GetBitrateBps() / 1000,
					m_encoderSettings.bVoiceOptimized ? "" : ", not voice optimized");
			}
			else
			{
				ODIN_WARNING("Couldn't make a new encoder for room '%s', nothing is sent to it until it's joined again", m_roomId.c_str());
			}

			m_odin.ForEachListener([this, bSucceeded](ICryOdin::IListener& listener) { listener.OnEncoderChanged(*this, bSucceeded); });
		}

		void CCryOdinRoom::DestroyEncoder()
		{
			if (m_pCaptureTarget)
			{
				// This returns once the audio threads can't be using it.
				m_audioSystem.UnregisterCaptureTarget(m_pCaptureTarget);
				m_pCaptureTarget.reset();
			}
			if (m_pEncoder != nullptr)
			{
				odin_encoder_free(m_pEncoder);
				m_pEncoder = nullptr;
				m_pPipeline = nullptr;

				m_captureEffects.erase(std::remove_if(m_captureEffects.begin(), m_captureEffects.end(), [](const std::unique_ptr<SCaptureEffect>& pEffect) { return pEffect->bRemoved; }), m_captureEffects.end());
			}
		}

		void CCryOdinRoom::ApplyAudioProcessing()
		{
			if (m_pEncoder == nullptr)
			{
				return;
			}

			OdinApmConfig apmConfig;
			apmConfig.echo_canceller = m_processing.bEchoCanceller;
			apmConfig.high_pass_filter = m_processing.bHighPassFilter;
			apmConfig.transient_suppressor = m_processing.bTransientSuppressor;
			apmConfig.noise_suppression_level = static_cast<OdinNoiseSuppressionLevel>(m_processing.noiseSuppression);
			apmConfig.gain_controller_version = static_cast<OdinGainControllerVersion>(m_processing.gainController);
			if (m_apmEffectId != 0)
			{
				odin_pipeline_set_apm_config(m_pPipeline, m_apmEffectId, &apmConfig);
			}

			OdinVadConfig vadConfig;
			vadConfig.voice_activity.enabled = m_processing.bVoiceActivity;
			vadConfig.voice_activity.attack_threshold = m_processing.fVoiceAttack;
			vadConfig.voice_activity.release_threshold = m_processing.fVoiceRelease;
			vadConfig.volume_gate.enabled = m_processing.bVolumeGate;
			vadConfig.volume_gate.attack_threshold = m_processing.fGateAttackDb;
			vadConfig.volume_gate.release_threshold = m_processing.fGateReleaseDb;
			if (m_bPositionGateForced)
			{
				// Silence never gets above full scale, so only position updates come out.
				vadConfig.voice_activity.enabled = false;
				vadConfig.volume_gate.enabled = true;
				vadConfig.volume_gate.attack_threshold = 0.f;
				vadConfig.volume_gate.release_threshold = 0.f;
			}
			if (m_vadEffectId != 0)
			{
				odin_pipeline_set_vad_config(m_pPipeline, m_vadEffectId, &vadConfig);
			}

			// Voice isolation loads a model, so it's only added when it's asked for.
			if (m_processing.bVoiceIsolation && m_viEffectId == 0)
			{
				uint32_t index = odin_pipeline_get_effect_count(m_pPipeline);
				if (m_vadEffectId != 0)
				{
					odin_pipeline_get_effect_index(m_pPipeline, m_vadEffectId, &index);
				}
				const OdinError error = odin_pipeline_insert_vi_effect(m_pPipeline, index, &m_viEffectId);
				if (error != ODIN_ERROR_SUCCESS)
				{
					print_error(error, "Failed to add the voice isolation effect");
					m_viEffectId = 0;
				}
			}
			if (m_viEffectId != 0)
			{
				OdinViConfig viConfig;
				viConfig.enabled = m_processing.bVoiceIsolation;
				viConfig.attenuation_limit_db = m_processing.fVoiceIsolationLimitDb;
				odin_pipeline_set_vi_config(m_pPipeline, m_viEffectId, &viConfig);
			}

			if (m_pCaptureTarget)
			{
				const bool bGated = m_vadEffectId != 0 && (m_processing.bVoiceActivity || m_processing.bVolumeGate || m_bPositionGateForced);
				m_pCaptureTarget->bGated.store(bGated, std::memory_order_relaxed);
				m_pCaptureTarget->bEchoCanceller.store(m_processing.bEchoCanceller, std::memory_order_relaxed);
				m_pCaptureTarget->echoDelayMs.store(std::max(m_processing.iEchoDelayMs, 0), std::memory_order_relaxed);
			}
		}

		void CCryOdinRoom::SetAudioProcessing(const SCryOdinAudioProcessing& processing)
		{
			m_processing = processing;
			ApplyAudioProcessing();
		}

		void CCryOdinRoom::SetTransmitEnabled(bool bEnabled)
		{
			const bool bChanged = m_bTransmit != bEnabled;
			m_bTransmit = bEnabled;
			if (m_pCaptureTarget)
			{
				m_pCaptureTarget->bTransmit.store(bEnabled, std::memory_order_relaxed);
			}
			// Open a gate that was closed for positions right away, so the start of the speech isn't lost.
			UpdatePositionKeepAlive(0.f);

			// Last, as a listener can shut Odin down, which deletes this room.
			if (bChanged)
			{
				m_odin.NotifyTransmitChanged(*this, bEnabled);
			}
		}

		void CCryOdinRoom::InsertCaptureEffect(SCaptureEffect& effect)
		{
			effect.effectId = 0;
			if (m_pPipeline != nullptr)
			{
				const OdinError error = odin_pipeline_insert_custom_effect(m_pPipeline, odin_pipeline_get_effect_count(m_pPipeline), CaptureEffectCallback, &effect, &effect.effectId);
				if (error != ODIN_ERROR_SUCCESS)
				{
					print_error(error, "Failed to add a capture effect");
					effect.effectId = 0;
				}
			}
		}

		void CCryOdinRoom::AddCaptureEffect(const ICryOdinAudioEffectPtr& pEffect)
		{
			if (!pEffect)
			{
				return;
			}

			auto pCaptureEffect = stl::make_unique<SCaptureEffect>();
			pCaptureEffect->pEffect = pEffect;
			InsertCaptureEffect(*pCaptureEffect);
			m_captureEffects.push_back(std::move(pCaptureEffect));
		}

		void CCryOdinRoom::RemoveCaptureEffect(const ICryOdinAudioEffectPtr& pEffect)
		{
			for (auto it = m_captureEffects.begin(); it != m_captureEffects.end(); ++it)
			{
				if ((*it)->pEffect == pEffect && !(*it)->bRemoved)
				{
					if ((*it)->effectId != 0 && m_pPipeline != nullptr)
					{
						const OdinError error = odin_pipeline_remove_effect(m_pPipeline, (*it)->effectId);
						if (error != ODIN_ERROR_SUCCESS)
						{
							// The pipeline can still call it, so it has to stay in memory.
							print_error(error, "Failed to remove a capture effect");
							(*it)->bRemoved = true;
							return;
						}
						// It could be in use on the capture thread right now.
						m_audioSystem.WaitForAudioThreads();
					}
					m_captureEffects.erase(it);
					return;
				}
			}
		}

		void CCryOdinRoom::SetLocalLoopback(bool bEnabled)
		{
			m_bLoopback = bEnabled;

			if (!bEnabled)
			{
				DetachLoopback();
				if (m_pLocalUser)
				{
					m_pLocalUser->ClearVoice();
				}
				return;
			}

			if (!m_pLocalUser || !m_pCaptureTarget || m_pLoopbackDecoder)
			{
				return;
			}

			if (CreateVoice(*m_pLocalUser))
			{
				m_pLoopbackDecoder = m_pLocalUser->GetDecoder();
				m_pCaptureTarget->pLoopbackDecoder.store(m_pLoopbackDecoder->pDecoder, std::memory_order_seq_cst);
			}
		}

		void CCryOdinRoom::DetachLoopback()
		{
			if (m_pCaptureTarget)
			{
				m_pCaptureTarget->pLoopbackDecoder.store(nullptr, std::memory_order_seq_cst);
			}
			m_audioSystem.WaitForAudioThreads();

			m_pLoopbackDecoder.reset();
		}

		// Server side filtering
		///////////////////////////////////////////////////////////////

		bool CCryOdinRoom::SetChannelMasks(const SCryOdinChannelMask* pMasks, uint32 count, bool bReset)
		{
			OdinJson masks = OdinJson::array();
			for (uint32 i = 0; i < count; ++i)
			{
				OdinJson entry = OdinJson::array();
				entry.push_back(pMasks[i].peerId);
				entry.push_back(pMasks[i].mask);
				masks.push_back(std::move(entry));
			}

			OdinJson change = OdinJson::object();
			change["masks"] = std::move(masks);
			change["reset"] = bReset;
			OdinJson command = OdinJson::object();
			command["SetChannelMasks"] = std::move(change);
			return SendRpc(OdinJsonDump(command).c_str());
		}

		bool CCryOdinRoom::SetPosition(const Vec3& position, uint64 channelMask)
		{
			if (!IsFiniteVec3(position))
			{
				return false;
			}

			if (channelMask == 0)
			{
				return false;
			}

			// An encoder holds 12 channels at most. Checking first keeps this list what the encoder has.
			uint64 allChannels = channelMask;
			for (const SPositionEntry& entry : m_positions)
			{
				allChannels |= entry.channelMask;
			}
			int channelCount = 0;
			for (uint64 bits = allChannels; bits != 0; bits &= bits - 1)
			{
				++channelCount;
			}
			if (channelCount > 12)
			{
				ODIN_WARNING("Can't send on more than 12 channels in room '%s'", m_roomId.c_str());
				return false;
			}

			m_bHasCustomPositions = true;
			RemoveChannelsFromPositions(channelMask);
			m_positions.push_back({ channelMask, position });

			if (m_pEncoder == nullptr)
			{
				return true;
			}

			// The first position replaces the encoder's default of channel 1 at the origin.
			if (m_bEncoderHasDefaultChannel)
			{
				ApplyPositionsToEncoder();
				return true;
			}

			const OdinPosition odinPosition = { position.x, position.y, position.z };
			const OdinError error = odin_encoder_set_position(m_pEncoder, channelMask, &odinPosition);
			if (error != ODIN_ERROR_SUCCESS)
			{
				print_error(error, "Failed to set position");
				return false;
			}

			// All channels may have been cleared before this.
			if (m_pCaptureTarget)
			{
				m_pCaptureTarget->bPublishing.store(true, std::memory_order_relaxed);
			}
			return true;
		}

		void CCryOdinRoom::RemoveChannelsFromPositions(uint64 channelMask)
		{
			for (SPositionEntry& entry : m_positions)
			{
				entry.channelMask &= ~channelMask;
			}
			m_positions.erase(std::remove_if(m_positions.begin(), m_positions.end(), [](const SPositionEntry& entry) { return entry.channelMask == 0; }), m_positions.end());
		}

		void CCryOdinRoom::ApplyPositionsToEncoder()
		{
			if (m_pEncoder == nullptr || !m_bHasCustomPositions)
			{
				if (m_pCaptureTarget)
				{
					m_pCaptureTarget->bPublishing.store(true, std::memory_order_relaxed);
				}
				return;
			}

			odin_encoder_clear_position(m_pEncoder, ~uint64(0));
			m_bEncoderHasDefaultChannel = false;

			for (const SPositionEntry& entry : m_positions)
			{
				const OdinPosition position = { entry.position.x, entry.position.y, entry.position.z };
				const OdinError error = odin_encoder_set_position(m_pEncoder, entry.channelMask, &position);
				if (error != ODIN_ERROR_SUCCESS)
				{
					print_error(error, "Failed to set position");
				}
			}

			if (m_pCaptureTarget)
			{
				m_pCaptureTarget->bPublishing.store(!m_positions.empty(), std::memory_order_relaxed);
			}
		}

		bool CCryOdinRoom::ClearPosition(uint64 channelMask)
		{
			m_bHasCustomPositions = true;
			RemoveChannelsFromPositions(channelMask);
			ApplyPositionsToEncoder();
			return true;
		}

		bool CCryOdinRoom::GetConnectionStats(SCryOdinConnectionStats& stats) const
		{
			OdinConnectionStats odinStats;
			if (m_pRoom == nullptr || odin_room_get_connection_stats(m_pRoom, &odinStats) != ODIN_ERROR_SUCCESS)
			{
				return false;
			}

			stats.udpTxDatagrams = odinStats.udp_tx_datagrams;
			stats.udpTxBytes = odinStats.udp_tx_bytes;
			stats.udpRxDatagrams = odinStats.udp_rx_datagrams;
			stats.udpRxBytes = odinStats.udp_rx_bytes;
			stats.fRoundTripMs = odinStats.rtt;
			return true;
		}
	}
}
