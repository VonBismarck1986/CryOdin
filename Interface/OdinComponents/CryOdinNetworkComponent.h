#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Utils/SharedString.h>

#include <ICryOdin.h>

struct INetChannel;

namespace Cry
{
	namespace Odin
	{
		//! The network part of Odin for a player. The server holds the access key, and this component
		//! gets room tokens from it for OdinUserComponent on the same entity. The server also picks the
		//! player's Odin user id here, and tells every client, so OdinComponentSound on this entity plays
		//! the player's voice by itself.
		//! Add it to the entity your server spawns for each player, the one given the player's channel
		//! with GetNetEntity()->SetChannelId and bound to the network by your game. It has to be on that
		//! entity on the server and on every client alike, as the engine numbers an entity's network
		//! messages in order.
		// Not final. Calls on a final class skip the vtable, and a game DLL couldn't link them.
		class CCryOdinNetworkComponent : public IEntityComponent
		{
		public:
			CCryOdinNetworkComponent() = default;
			virtual ~CCryOdinNetworkComponent() = default;

			//! What RequestRoomToken did.
			enum class ETokenRequest
			{
				Failed, //!< Nothing was sent, see the reason.
				Signed, //!< Signed right here, on the server or in single player.
				Sent,   //!< Asked the server, the answer goes to OdinUserComponent::OnServerTokenReply.
			};

			// Sent by a client to the server.
			struct STokenRequestParams
			{
				uint32 requestId = 0;
				string roomId;

				void SerializeWith(TSerialize ser);
			};

			// Sent by the server to the client who asked.
			struct STokenReplyParams
			{
				uint32 requestId = 0;
				bool bGranted = false;
				string roomId;
				string userId;
				string token;
				string reason;

				void SerializeWith(TSerialize ser);
			};

			// Sent by a client that wants to know the user id of this entity's player.
			struct SUserIdQueryParams
			{
				bool bWanted = true;

				void SerializeWith(TSerialize ser);
			};

			// Sent by the server with the user id of this entity's player.
			struct SUserIdParams
			{
				string userId;

				void SerializeWith(TSerialize ser);
			};

		protected:
			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinNetworkComponent>& desc)
			{
				desc.SetGUID("{855425BF-A369-44F5-91DF-6021D212B6B0}"_cry_guid);
				desc.SetLabel("OdinNetworkComponent");
				desc.SetEditorCategory("Odin");
				desc.SetDescription("Gets room tokens from the server, which holds the access key. Add it to the player entity next to OdinUserComponent");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Singleton });
				desc.AddMember(&CCryOdinNetworkComponent::m_bBindVoice, 'bind', "BindVoice", "Bind Voice", "On the other players' clients, plays this player's voice from the OdinComponentSound on this entity, unless your game gave that one a User Id", true);
			}

			//! The Odin user id of this entity's player, picked by the server. Empty on a client until the
			//! first token arrives.
			virtual const char* GetOdinUserId() const { return m_odinUserId.c_str(); }
			//! On the server: sets the Odin user id of this player, such as an account id, instead of the
			//! random one. Set it before the player joins a room, the clients are told about the change.
			//! Ignored on clients.
			virtual void SetOdinUserId(const char* szUserId);

			virtual void SetBindVoice(bool bBindVoice);
			virtual bool GetBindVoice() const { return m_bBindVoice; }

			// Used by OdinUserComponent, not meant for game code.
			// @param outRequestId Set when Sent, to match the answer.
			// @param outToken, outUserId Set when Signed.
			ETokenRequest RequestRoomToken(const char* szRoomId, uint32& outRequestId, string& outToken, string& outUserId, string& outReason);

			// Used by Schematyc
			Schematyc::CSharedString GetOdinUserIdSchematyc() const { return Schematyc::CSharedString(GetOdinUserId()); }

		private:
			struct SRmis;

			bool ServerRequestToken(STokenRequestParams&& params, INetChannel* pNetChannel);
			bool ClientReceiveToken(STokenReplyParams&& params, INetChannel* pNetChannel);
			bool ServerRequestUserId(SUserIdQueryParams&& params, INetChannel* pNetChannel);
			bool ClientReceiveUserId(SUserIdParams&& params, INetChannel* pNetChannel);

			// On the server. bLocal: asked by the host of a listen server or in single player.
			void HandleTokenRequest(int senderChannelId, bool bLocal, const STokenRequestParams& request, STokenReplyParams& reply);
			void EnsureOdinUserId();
			// Takes back an id this component gave the sound, when leaving game mode in the Sandbox.
			void ReleaseBoundVoice();
			// The user id is known or changed here.
			void OnOdinUserIdChanged();
			// Points the entity's OdinComponentSound at the user id, see Bind Voice.
			void BindVoice();
			void UpdateEventMask();

			bool m_bBindVoice = true;

			string m_odinUserId;
			string m_boundVoiceUserId;          // What BindVoice gave the sound, so a change of id can follow it.
			bool m_bUserIdQueryPending = false; // A client asks once the entity is bound to the network.
			bool m_bBindVoicePending = false;   // The sound can be added after this component.
			uint32 m_nextRequestId = 0;
			bool m_bRmisRegistered = false;
		};
	}
}
