#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Utils/SharedString.h>

#include <ICryOdin.h>

#include <vector>

namespace Cry
{
	namespace Odin
	{
		//! The local user. Add it to the local player to join a room, send voice and text, and get
		//! Odin events in Schematyc. Use one per room to be in several rooms, each with its own Room Id.
		class CCryOdinUserComponent
			: public IEntityComponent
			, public ICryOdin::IListener
			, public ICryOdin::IPushToTalkListener
		{
		public:
			CCryOdinUserComponent() = default;
			virtual ~CCryOdinUserComponent();

		protected:
			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;
			virtual void OnShutDown() override;

			// ICryOdin::IListener
			virtual void OnJoinedRoom(ICryOdinRoom& room) override;
			virtual void OnLeftRoom(ICryOdinRoom& room, const char* szReason) override;
			virtual void OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user) override;
			virtual void OnPeerLeft(ICryOdinRoom& room, ICryOdinUser& user) override;
			virtual void OnPeerUserDataChanged(ICryOdinRoom& room, ICryOdinUser& user) override;
			virtual void OnPeerTalkingChanged(ICryOdinRoom& room, ICryOdinUser& user, bool bTalking) override;
			virtual void OnTextMessage(ICryOdinRoom& room, ICryOdinUser* pSender, const char* szText, const char* szChannel) override;
			// ~ICryOdin::IListener

			// ICryOdin::IPushToTalkListener
			virtual void OnPushToTalkInput(bool bDown) override;
			// ~ICryOdin::IPushToTalkListener

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinUserComponent>& desc)
			{
				desc.SetGUID("{16EB0B3D-747B-479A-BFF1-581881DFA9FC}"_cry_guid);
				desc.SetLabel("OdinUserComponent");
				desc.SetEditorCategory("Odin");
				desc.SetDescription("Joins an Odin room as the local user");
				desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly });
				desc.AddMember(&CCryOdinUserComponent::m_bAutoJoin, 'auto', "AutoJoin", "Auto Join", "Joins the room when gameplay starts", true);
				desc.AddMember(&CCryOdinUserComponent::m_bRequireToken, 'requ', "RequireToken", "Require Token", "For a shipped game. Tokens only come from SetRoomToken, or from the server through an OdinNetworkComponent on this entity. In single player and on a listen server's host, that server is this machine. Without either, Auto Join waits for SetRoomToken", false);
				desc.AddMember(&CCryOdinUserComponent::m_bLocalPlayerOnly, 'locl', "LocalPlayerOnly", "Local Player Only", "Only joins from the entity of the local player, which your game has to spawn as one. Turn off for an entity placed in the level", true);
				desc.AddMember(&CCryOdinUserComponent::m_roomId, 'room', "RoomId", "Room Id", "Name of the room to join", "");
				desc.AddMember(&CCryOdinUserComponent::m_userName, 'name', "UserName", "User Name", "Name shown to other users", "");
				desc.AddMember(&CCryOdinUserComponent::m_userId, 'user', "UserId", "User Id", "Your own id for this user, a random one is used when empty", "");
				desc.AddMember(&CCryOdinUserComponent::m_gateway, 'gate', "Gateway", "Gateway", "Odin gateway or server, the 4Players gateway is used when empty", "");
				desc.AddMember(&CCryOdinUserComponent::m_transmitMode, 'mode', "TransmitMode", "Transmit Mode", "When the microphone is sent", ECryOdinTransmitMode::VoiceActivity);
				desc.AddMember(&CCryOdinUserComponent::m_pushToTalkKey, 'ptt', "PushToTalkKey", "Push To Talk Key", "The key used while Transmit Mode is Push To Talk. It's the action odin_push_to_talk in the odin action map, which your game can rebind", SCryOdinPushToTalkKey());
				desc.AddMember(&CCryOdinUserComponent::m_processing, 'proc', "AudioProcessing", "Audio Processing", "Microphone processing", SCryOdinAudioProcessing());
				desc.AddMember(&CCryOdinUserComponent::m_encoder, 'enco', "VoiceQuality", "Voice Quality", "How your voice is encoded for this room. A change while joined gives a short gap and restarts the microphone processing for a second or two", SCryOdinEncoderSettings::MakeWithQuality(ECryOdinVoiceQuality::Standard));
				desc.AddMember(&CCryOdinUserComponent::m_roomVoice, 'voic', "RoomVoice", "Room Voice", "How voices in this room play when not bound to an entity, turn on the radio here for a radio room", SCryOdinRoomParams::MakeDefaultVoice());
				desc.AddMember(&CCryOdinUserComponent::m_bSendPosition, 'prox', "SendPosition", "Send Position", "Lets the server skip voices that are too far away, all users in the room must use it", false);
				desc.AddMember(&CCryOdinUserComponent::m_fProximityRadius, 'radi', "ProximityRadius", "Proximity Radius", "Distance where the server stops sending a voice", 100.f);
			}

			//! Joins a room with the settings of this component.
			//! @param szRoomId Name of the room, null or empty uses the Room Id property.
			virtual bool Join(const char* szRoomId = nullptr);
			//! Joins a room with your own settings, for this session only. The transmit mode is still used.
			//! Fails with a warning, staying in the current room, if something else is already in that
			//! room, as leaving it would end it for both.
			//! Without a token, and with an OdinNetworkComponent on this entity, it asks the server for one
			//! and returns true. The current room is left and the new one joined when the token arrives, with
			//! the user id the server picked. A refusal or no answer gives On Join Failed.
			virtual bool JoinWithParams(const SCryOdinRoomParams& params);
			//! Also stops waiting for a token from the server.
			virtual void Leave();
			virtual bool IsJoined() const;
			//! True while a token asked from the server hasn't arrived.
			virtual bool IsWaitingForToken() const { return m_pendingServerJoin.requestId != 0; }

			//! The room of this component, or null.
			virtual ICryOdinRoom* GetRoom() const { return m_pRoom; }
			//! The local user in the room of this component, or null.
			virtual ICryOdinUser* GetLocalUser() const;

			virtual bool SendText(const char* szText, const char* szChannel = nullptr);

			//! Talks or stops talking in Push To Talk mode. The Push To Talk Key property calls this by
			//! itself. Turn its Use Built In Key off to call this from your own input code instead.
			virtual void SetPushToTalk(bool bPressed);
			virtual bool IsPushToTalkPressed() const { return m_bPushToTalkPressed; }

			//! Changes the push to talk key. A changed action name or input replaces the inputs of that
			//! action, also ones from your action map XML or rebound by the player.
			virtual void SetPushToTalkKey(const SCryOdinPushToTalkKey& key);
			virtual const SCryOdinPushToTalkKey& GetPushToTalkKey() const { return m_pushToTalkKey; }
			//! Sets the inputs of the push to talk key, such as "v" and "xi_shoulderl". Empty for none.
			virtual void SetPushToTalkInputs(const char* szKeyboard, const char* szGamepad);
			//! True: press once to talk and again to stop. False: talk while the key is held.
			virtual void SetPushToTalkToggle(bool bToggle);

			virtual void SetTransmitMode(ECryOdinTransmitMode mode);
			virtual ECryOdinTransmitMode GetTransmitMode() const { return m_transmitMode; }

			virtual void SetMicrophoneMuted(bool bMuted);

			//! The room to join, used by the next Join.
			virtual void SetRoomId(const char* szRoomId);
			virtual const char* GetRoomId() const { return m_roomId.c_str(); }

			//! Your own id for this user, used by the next Join. Share it with other players through your
			//! game's network, so they can give it to CCryOdinComponentSound on this player's entity.
			virtual void SetUserId(const char* szUserId);
			//! The id in use once joined, including a random one. Otherwise the id that was set.
			virtual const char* GetUserId() const;

			//! The token from your server for the next Join. It's used once, so set a new one for each Join.
			//! Without a token, one is asked from the server when this entity has an OdinNetworkComponent.
			//! Otherwise one is signed with the local access key, which a shipped game shouldn't have.
			//! With Require Token and Auto Join on, setting it during gameplay joins the room.
			virtual void SetRoomToken(const char* szToken);

			// Used by OdinNetworkComponent when the server answers, not meant for game code.
			bool IsWaitingForServerToken(uint32 requestId) const { return requestId != 0 && requestId == m_pendingServerJoin.requestId; }
			void OnServerTokenReply(uint32 requestId, bool bGranted, const char* szRoomId, const char* szUserId, const char* szToken, const char* szReason);

			//! How voices in this room play when not bound to an entity, applied to those playing now as well.
			virtual void SetRoomVoiceSettings(const SCryOdinVoiceSettings& settings);
			virtual const SCryOdinVoiceSettings& GetRoomVoiceSettings() const { return m_roomVoice; }
			//! Turns the room into a radio room with a preset. Custom turns the radio off.
			virtual void SetRoomRadioPreset(ECryOdinRadioPreset preset);

			//! Lets the server skip voices too far away. All users in the room must use it. Turning it on
			//! while joined makes a new encoder for the room, with a short gap in your voice, so positions
			//! are also sent while you're silent.
			virtual void SetSendPosition(bool bSendPosition);
			virtual void SetProximityRadius(float fRadius);

			virtual void SetAudioProcessing(const SCryOdinAudioProcessing& processing);
			virtual const SCryOdinAudioProcessing& GetAudioProcessing() const { return m_processing; }

			//! Changes the bitrate and codec settings of your voice, also while joined. See
			//! ICryOdinRoom::SetEncoderSettings for what happens then. The position update interval is
			//! kept up by Send Position, so it doesn't need to be set here.
			virtual void SetEncoderSettings(const SCryOdinEncoderSettings& settings);
			virtual const SCryOdinEncoderSettings& GetEncoderSettings() const { return m_encoder; }
			virtual void SetVoiceQuality(ECryOdinVoiceQuality quality);
			//! Sets a custom bitrate in kilobits per second, 6 to 512.
			virtual void SetVoiceBitrateKbps(int kbps);
			//! 0 to 100. Higher keeps the voice clearer on a bad connection, at some cost to quality.
			virtual void SetExpectedPacketLoss(int percent);

			virtual bool IsTalking() const;

			//! The name shown to other users. Changing it while joined updates the user data.
			virtual void SetDisplayName(const char* szUserName);
			virtual const char* GetDisplayName() const { return m_userName.c_str(); }

			// Used by Schematyc
			void JoinSchematyc() { Join(); }
			void SendTextSchematyc(const Schematyc::CSharedString& text, const Schematyc::CSharedString& channel) { SendText(text.c_str(), channel.c_str()); }
			void SetPushToTalkSchematyc(bool bPressed) { SetPushToTalk(bPressed); }
			bool IsPushToTalkPressedSchematyc() const { return IsPushToTalkPressed(); }
			int GetVoiceBitrateKbpsSchematyc() const { return (GetRoom() != nullptr ? GetRoom()->GetEncoderSettings() : GetEncoderSettings()).GetBitrateBps() / 1000; }
			void SetPushToTalkInputsSchematyc(const Schematyc::CSharedString& keyboard, const Schematyc::CSharedString& gamepad) { SetPushToTalkInputs(keyboard.c_str(), gamepad.c_str()); }
			void SetPushToTalkToggleSchematyc(bool bToggle) { SetPushToTalkToggle(bToggle); }
			void SetUseBuiltInPushToTalkKeySchematyc(bool bEnabled)
			{
				SCryOdinPushToTalkKey key = GetPushToTalkKey();
				key.bEnabled = bEnabled;
				SetPushToTalkKey(key);
			}
			void SetRoomIdSchematyc(const Schematyc::CSharedString& roomId) { SetRoomId(roomId.c_str()); }
			void SetUserIdSchematyc(const Schematyc::CSharedString& userId) { SetUserId(userId.c_str()); }
			void SetDisplayNameSchematyc(const Schematyc::CSharedString& name) { SetDisplayName(name.c_str()); }
			void SetRoomTokenSchematyc(const Schematyc::CSharedString& token) { SetRoomToken(token.c_str()); }
			Schematyc::CSharedString GetUserIdSchematyc() const { return Schematyc::CSharedString(GetUserId()); }

		private:
			void StartGameplay();
			void TryAutoJoin();
			void LogWhyAutoJoinWaits() const;
			void StopGameplay();
			bool IsAllowedToJoin() const;
			void UpdateTransmit();
			// Listens for the push to talk key while it's wanted, and stops otherwise.
			void UpdatePushToTalkKey();
			SCryOdinAudioProcessing GetEffectiveProcessing() const;
			// The encoder settings for the room: the property, with position updates for Send Position.
			SCryOdinEncoderSettings GetEffectiveEncoder() const;
			// The starting point for changing one value: the room's settings while joined.
			SCryOdinEncoderSettings GetCurrentEncoderForChange() const;
			void ApplyEncoderSettings();
			void RegisterListener();
			void UnregisterListener();
			bool IsMyRoom(const ICryOdinRoom& room) const;
			// This entity can get tokens from the server.
			bool HasServerTokenSource() const;
			// Stops waiting for a token from the server, an answer that still comes is ignored.
			void CancelServerToken();
			void SendJoinFailed(const char* szRoomId, const char* szReason);

			// A join waiting for its token from the server. The params point into the strings here.
			struct SPendingServerJoin
			{
				uint32 requestId = 0;
				float fTimeLeft = 0.f;
				SCryOdinRoomParams params;
				string roomId;
				string userName;
				string gateway;
				string userDataJson;
				string extraAuthJson;
				string password;
			};

			bool m_bAutoJoin = true;
			bool m_bRequireToken = false;
			bool m_bLocalPlayerOnly = true;
			Schematyc::CSharedString m_roomId;
			Schematyc::CSharedString m_userName;
			Schematyc::CSharedString m_userId;
			Schematyc::CSharedString m_gateway;
			ECryOdinTransmitMode m_transmitMode = ECryOdinTransmitMode::VoiceActivity;
			SCryOdinPushToTalkKey m_pushToTalkKey;
			SCryOdinAudioProcessing m_processing;
			SCryOdinEncoderSettings m_encoder = SCryOdinEncoderSettings::MakeWithQuality(ECryOdinVoiceQuality::Standard);
			SCryOdinEncoderSettings m_appliedEncoder = m_encoder; // The property when it was last given to the room.
			bool m_bAppliedSendPosition = false;                  // Send Position when it was last given to the room.
			SCryOdinVoiceSettings m_roomVoice = SCryOdinRoomParams::MakeDefaultVoice();
			bool m_bSendPosition = false;
			float m_fProximityRadius = 100.f;

			ICryOdinRoom* m_pRoom = nullptr;
			std::vector<ICryOdinRoom*> m_leavingRooms; // Only compared, so that their last events are still seen.
			SCryOdinAudioProcessing m_sessionProcessing; // What the room was joined with, as properties stay as they are.
			string m_token;
			SPendingServerJoin m_pendingServerJoin;
			// Auto Join waits for the next update, when every component of the entity is initialized,
			// OdinNetworkComponent included.
			bool m_bAutoJoinPending = false;
			bool m_bDisplayNamePending = false;
			bool m_bBecameLocalPlayer = false;
			bool m_bEntityShuttingDown = false;
			bool m_bListening = false;
			bool m_bGameplayStarted = false;
			bool m_bPushToTalkPressed = false;
			bool m_bPushToTalkSignalled = false;      // Last value sent with On Push To Talk Changed.
			bool m_bPushToTalkSubscribed = false;
			bool m_bPushToTalkReplacePending = false; // SetPushToTalkKey changed the inputs, used once listening.
			SCryOdinPushToTalkKey m_subscribedKey;    // What the plugin listens for now.
			float m_fPositionTimer = 0.f;
		};
	}
}
