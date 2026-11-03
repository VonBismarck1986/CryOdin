#include "StdAfx.h"
#include "OdinComponents/CryOdinNetworkComponent.h"
#include "OdinComponents/CryOdinUserComponent.h"
#include "OdinComponents/CryOdinComponentSound.h"

#include "Core/CryOdin.h"

#include <CryEntitySystem/IEntity.h>
#include <CryNetwork/Rmi.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			// Room names longer than this are refused, they could only be an attempt to flood the log.
			constexpr size_t kMaxRoomIdLength = 128;
			// Network strings carry up to 4095 characters.
			constexpr size_t kMaxTokenLength = 4000;
			constexpr int kTokenLifetimeSeconds = 300;

			int GetSenderChannelId(INetChannel* pNetChannel)
			{
				return (pNetChannel != nullptr && gEnv->pGameFramework != nullptr) ? gEnv->pGameFramework->GetGameChannelId(pNetChannel) : 0;
			}

			static void RegisterOdinNetworkComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinNetworkComponent));
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinNetworkComponent::GetOdinUserIdSchematyc, "{BE458F51-EABF-4F73-BDA7-1EB1BFDBE658}"_cry_guid, "GetOdinUserId");
						pFunction->SetDescription("The Odin user id of this player, picked by the server. Empty on a client until the first token arrives");
						pFunction->BindOutput(0, 'user', "User Id");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinNetworkComponent::SetBindVoice, "{F8D470E9-A28B-4B61-80C9-E5D5D92EE825}"_cry_guid, "SetBindVoice");
						pFunction->SetDescription("On: other players hear this player's voice from the OdinComponentSound on this entity");
						pFunction->BindInput(1, 'bind', "Bind Voice");
						componentScope.Register(pFunction);
					}
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinNetworkComponent);
		}

		// A nested type, as the handlers are private.
		struct CCryOdinNetworkComponent::SRmis
		{
			using ServerRequestToken = SRmi<RMI_WRAP(&CCryOdinNetworkComponent::ServerRequestToken)>;
			using ClientReceiveToken = SRmi<RMI_WRAP(&CCryOdinNetworkComponent::ClientReceiveToken)>;
			using ServerRequestUserId = SRmi<RMI_WRAP(&CCryOdinNetworkComponent::ServerRequestUserId)>;
			using ClientReceiveUserId = SRmi<RMI_WRAP(&CCryOdinNetworkComponent::ClientReceiveUserId)>;
		};

		void CCryOdinNetworkComponent::SUserIdQueryParams::SerializeWith(TSerialize ser)
		{
			ser.Value("wanted", bWanted);
		}

		void CCryOdinNetworkComponent::SUserIdParams::SerializeWith(TSerialize ser)
		{
			ser.Value("user", userId);
		}

		void CCryOdinNetworkComponent::STokenRequestParams::SerializeWith(TSerialize ser)
		{
			ser.Value("id", requestId);
			ser.Value("room", roomId);
		}

		void CCryOdinNetworkComponent::STokenReplyParams::SerializeWith(TSerialize ser)
		{
			ser.Value("id", requestId);
			ser.Value("ok", bGranted);
			ser.Value("room", roomId);
			ser.Value("user", userId);
			ser.Value("token", token);
			ser.Value("reason", reason);
		}

		void CCryOdinNetworkComponent::Initialize()
		{
			// The same order on the server and the clients, the engine sends the index.
			SRmis::ServerRequestToken::Register(this, eRAT_NoAttach, true, eNRT_ReliableOrdered);
			SRmis::ClientReceiveToken::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
			SRmis::ServerRequestUserId::Register(this, eRAT_NoAttach, true, eNRT_ReliableOrdered);
			SRmis::ClientReceiveUserId::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
			m_bRmisRegistered = true;

			// Binding is left to the game. Binding here could come before the game's other components
			// are added, whose network data would then never be sent.

			if (gEnv->bServer)
			{
				EnsureOdinUserId();
			}
			else
			{
				// Asked once the entity is bound, which a client's copy may not be yet.
				m_bUserIdQueryPending = true;
				UpdateEventMask();
			}
		}

		Cry::Entity::EventFlags CCryOdinNetworkComponent::GetEventMask() const
		{
			Cry::Entity::EventFlags flags = Cry::Entity::EEvent::Reset;
			if (m_bUserIdQueryPending || m_bBindVoicePending)
			{
				flags |= Cry::Entity::EEvent::Update;
			}
			return flags;
		}

		void CCryOdinNetworkComponent::UpdateEventMask()
		{
			if (!m_pEntity->IsGarbage())
			{
				m_pEntity->UpdateComponentEventMask(this);
			}
		}

		void CCryOdinNetworkComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == Cry::Entity::EEvent::Reset)
			{
				// Leaving game mode in the Sandbox. The sound's User Id is a property that could be saved
				// with the level, and would then count as the game's own id.
				if (event.nParam[0] == 0)
				{
					ReleaseBoundVoice();
				}
				return;
			}

			if (event.event != Cry::Entity::EEvent::Update)
			{
				return;
			}

			if (m_bUserIdQueryPending && gEnv->pGameFramework != nullptr && m_pEntity->GetNetEntity()->IsBoundToNetwork())
			{
				m_bUserIdQueryPending = false;
				SRmis::ServerRequestUserId::InvokeOnServer(this, SUserIdQueryParams());
			}

			// Waits while editing, for the same reason.
			if (m_bBindVoicePending && !gEnv->IsEditing())
			{
				m_bBindVoicePending = false;
				BindVoice();
			}

			if (!m_bUserIdQueryPending && !m_bBindVoicePending)
			{
				UpdateEventMask();
			}
		}

		void CCryOdinNetworkComponent::SetOdinUserId(const char* szUserId)
		{
			if (!gEnv->bServer)
			{
				ODIN_WARNING("SetOdinUserId on entity '%s' is ignored on a client, the server picks the id", m_pEntity->GetName());
				return;
			}

			const string previous = m_odinUserId;
			m_odinUserId = szUserId != nullptr ? szUserId : "";
			EnsureOdinUserId();
			if (m_odinUserId != previous)
			{
				// Clients that asked before have the old one. Ones that join later ask by themselves.
				if (gEnv->pGameFramework != nullptr && m_pEntity->GetNetEntity()->IsBoundToNetwork())
				{
					SUserIdParams params;
					params.userId = m_odinUserId;
					SRmis::ClientReceiveUserId::InvokeOnRemoteClients(this, std::move(params));
				}
				OnOdinUserIdChanged();
			}
		}

		void CCryOdinNetworkComponent::SetBindVoice(bool bBindVoice)
		{
			m_bBindVoice = bBindVoice;
			if (m_bBindVoice)
			{
				BindVoice();
			}
		}

		void CCryOdinNetworkComponent::EnsureOdinUserId()
		{
			if (m_odinUserId.empty())
			{
				m_odinUserId = CryGUID::Create().ToString();
				OnOdinUserIdChanged();
			}
		}

		void CCryOdinNetworkComponent::OnOdinUserIdChanged()
		{
			// The host of a listen server plays voices too. A dedicated server has no sound to bind.
			if (gEnv->IsClient())
			{
				// The sound can be added to the entity after this component, so a frame later as well.
				BindVoice();
				m_bBindVoicePending = true;
				UpdateEventMask();
			}
		}

		void CCryOdinNetworkComponent::BindVoice()
		{
			// The local player hears itself only with odin_loopback, never from its own entity.
			if (!m_bBindVoice || m_odinUserId.empty() || gEnv->IsEditing() || (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0)
			{
				return;
			}

			CCryOdinComponentSound* pSound = m_pEntity->GetComponent<CCryOdinComponentSound>();
			if (pSound == nullptr)
			{
				return;
			}

			// An id the game gave the sound itself stays. One from here follows a change.
			const char* szCurrent = pSound->GetUserId();
			const bool bOurs = szCurrent[0] == '\0' || m_boundVoiceUserId == szCurrent;
			if (bOurs && m_odinUserId != szCurrent)
			{
				pSound->BindToUserId(m_odinUserId.c_str(), nullptr);
				m_boundVoiceUserId = m_odinUserId;
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin != nullptr && pOdin->IsDebugEnabled())
				{
					ODIN_LOG("Entity '%s' plays the voice of user '%s'", m_pEntity->GetName(), m_odinUserId.c_str());
				}
			}
		}

		void CCryOdinNetworkComponent::ReleaseBoundVoice()
		{
			if (m_boundVoiceUserId.empty())
			{
				return;
			}
			CCryOdinComponentSound* pSound = m_pEntity->GetComponent<CCryOdinComponentSound>();
			if (pSound != nullptr && m_boundVoiceUserId == pSound->GetUserId())
			{
				pSound->BindToUserId("", nullptr);
			}
			m_boundVoiceUserId.clear();
			// The next game mode binds again.
			if (!m_odinUserId.empty() && gEnv->IsClient())
			{
				m_bBindVoicePending = true;
				UpdateEventMask();
			}
		}

		// The engine doesn't check which side registered a message, so any message can arrive on either
		// side. Each handler first checks it's on the side it was meant for.

		bool CCryOdinNetworkComponent::ServerRequestUserId(SUserIdQueryParams&& params, INetChannel* pNetChannel)
		{
			if (!gEnv->bServer)
			{
				return true;
			}

			// Anyone may know whose voice is whose. Only the asker gets the answer.
			const int senderChannelId = GetSenderChannelId(pNetChannel);
			if (senderChannelId != 0)
			{
				EnsureOdinUserId();
				SUserIdParams reply;
				reply.userId = m_odinUserId;
				SRmis::ClientReceiveUserId::InvokeOnClient(this, std::move(reply), senderChannelId);
			}
			return true;
		}

		bool CCryOdinNetworkComponent::ClientReceiveUserId(SUserIdParams&& params, INetChannel* pNetChannel)
		{
			// Sent by a client to the server, it would let a player pick any user id.
			if (gEnv->bServer)
			{
				return true;
			}

			if (params.userId != m_odinUserId)
			{
				m_odinUserId = params.userId;
				OnOdinUserIdChanged();
			}
			return true;
		}

		CCryOdinNetworkComponent::ETokenRequest CCryOdinNetworkComponent::RequestRoomToken(const char* szRoomId, uint32& outRequestId, string& outToken, string& outUserId, string& outReason)
		{
			outRequestId = 0;
			outToken.clear();
			outUserId.clear();
			outReason.clear();

			STokenRequestParams request;
			request.requestId = ++m_nextRequestId;
			request.roomId = szRoomId != nullptr ? szRoomId : "";

			// The host of a listen server, or single player, holds the key itself.
			if (gEnv->bServer)
			{
				STokenReplyParams reply;
				HandleTokenRequest(m_pEntity->GetNetEntity()->GetChannelId(), true, request, reply);
				if (!reply.bGranted)
				{
					outReason = reply.reason;
					return ETokenRequest::Failed;
				}
				outToken = reply.token;
				outUserId = reply.userId;
				return ETokenRequest::Signed;
			}

			// Another component can join from its Initialize, before this one has registered its messages.
			if (!m_bRmisRegistered)
			{
				outReason.Format("OdinNetworkComponent on entity '%s' isn't initialized yet", m_pEntity->GetName());
				return ETokenRequest::Failed;
			}
			if (gEnv->pGameFramework == nullptr || !m_pEntity->GetNetEntity()->IsBoundToNetwork())
			{
				outReason.Format("entity '%s' isn't bound to the network, so it can't ask the server", m_pEntity->GetName());
				return ETokenRequest::Failed;
			}

			outRequestId = request.requestId;
			SRmis::ServerRequestToken::InvokeOnServer(this, std::move(request));
			return ETokenRequest::Sent;
		}

		bool CCryOdinNetworkComponent::ServerRequestToken(STokenRequestParams&& params, INetChannel* pNetChannel)
		{
			if (!gEnv->bServer)
			{
				return true;
			}

			const int senderChannelId = GetSenderChannelId(pNetChannel);

			STokenReplyParams reply;
			HandleTokenRequest(senderChannelId, false, params, reply);

			// Only the player who asked gets the answer. A token is only signed when that's the entity's owner.
			if (senderChannelId != 0)
			{
				CRY_ASSERT(!reply.bGranted || senderChannelId == m_pEntity->GetNetEntity()->GetChannelId());
				SRmis::ClientReceiveToken::InvokeOnClient(this, std::move(reply), senderChannelId);
			}

			// False would count as a broken message.
			return true;
		}

		void CCryOdinNetworkComponent::HandleTokenRequest(int senderChannelId, bool bLocal, const STokenRequestParams& request, STokenReplyParams& reply)
		{
			reply.requestId = request.requestId;
			reply.roomId = request.roomId;
			reply.bGranted = false;

			CCryOdin* pOdin = CCryOdin::Get();
			const int ownerChannelId = m_pEntity->GetNetEntity()->GetChannelId();

			// Counted for the sender before anything else, so a flood costs one log line each window,
			// and one player can't use up another player's requests.
			bool bLogRefusal = true;
			const bool bAllowed = pOdin == nullptr || pOdin->AllowServerRequest(senderChannelId, bLogRefusal);

			// Anything a client sent is cut short before it can reach the log.
			if (request.roomId.length() > kMaxRoomIdLength)
			{
				reply.reason.Format("the room name is longer than %d characters", static_cast<int>(kMaxRoomIdLength));
				reply.roomId = request.roomId.Left(kMaxRoomIdLength) + "...";
			}
			else if (!bAllowed)
			{
				reply.reason = "too many token requests, try again in a few seconds";
			}
			else if (!bLocal && ownerChannelId == 0)
			{
				reply.reason.Format("player entity '%s' has no channel, give it one with GetNetEntity()->SetChannelId when spawning it", m_pEntity->GetName());
			}
			else if (!bLocal && senderChannelId != ownerChannelId)
			{
				reply.reason.Format("channel %d asked for a token of entity '%s', which belongs to channel %d", senderChannelId, m_pEntity->GetName(), ownerChannelId);
			}
			else if (pOdin != nullptr)
			{
				EnsureOdinUserId();

				ICryOdin::SServerTokenRequest tokenRequest;
				tokenRequest.channelId = senderChannelId;
				tokenRequest.entityId = GetEntityId();
				tokenRequest.szRoomId = request.roomId.c_str();
				tokenRequest.szUserId = m_odinUserId.c_str();
				tokenRequest.lifetimeSeconds = kTokenLifetimeSeconds;

				string token;
				if (pOdin->SignServerToken(tokenRequest, token, reply.reason))
				{
					if (token.length() > kMaxTokenLength)
					{
						reply.reason = "the signed token is too long to send";
					}
					else
					{
						reply.bGranted = true;
						reply.userId = m_odinUserId;
						reply.token = std::move(token);
					}
				}
			}
			else
			{
				reply.reason = "CryOdin isn't running on the server";
			}

			// Never the token, logs get shared.
			if (reply.bGranted)
			{
				ODIN_LOG("Signed a token for room '%s', user '%s', channel %d", reply.roomId.c_str(), reply.userId.c_str(), senderChannelId);
			}
			else if (bAllowed || bLogRefusal)
			{
				ODIN_WARNING("Refused a token for room '%s' to channel %d: %s", reply.roomId.c_str(), senderChannelId, reply.reason.c_str());
			}
		}

		bool CCryOdinNetworkComponent::ClientReceiveToken(STokenReplyParams&& params, INetChannel* pNetChannel)
		{
			// Sent by a client to the server, it would let a player pick any user id.
			if (gEnv->bServer)
			{
				return true;
			}

			if (params.bGranted && !params.userId.empty() && params.userId != m_odinUserId)
			{
				m_odinUserId = params.userId;
				OnOdinUserIdChanged();
			}

			// Several user components, one for each room, can be waiting. Only the one that asked gets it,
			// and nothing is touched after its join, which can run any listener.
			DynArray<CCryOdinUserComponent*> users;
			m_pEntity->GetAllComponents<CCryOdinUserComponent>(users);
			for (CCryOdinUserComponent* pUser : users)
			{
				if (pUser->IsWaitingForServerToken(params.requestId))
				{
					pUser->OnServerTokenReply(params.requestId, params.bGranted, params.roomId.c_str(), params.userId.c_str(), params.token.c_str(), params.reason.c_str());
					break;
				}
			}
			return true;
		}
	}
}
