#pragma once

#include "CrySystem/ICryPlugin.h"
#include "CryEntitySystem/IEntityBasicTypes.h"

#include "ICryOdinTypes.h"
#include "ICryOdinRoom.h"
#include "ICryOdinUser.h"
#include "ICryOdinSocket.h"
#include "ICryOdinAudioEffect.h"
#include "ICryOdinAudioSound.h"
#include "ICryOdinAudioDevices.h"
#include "ICryOdinAudioSystem.h"

namespace Cry
{
	namespace Odin
	{
		struct ICryOdin;

		struct ICryOdinPlugin : public IEnginePlugin
		{
			CRYINTERFACE_DECLARE_GUID(ICryOdinPlugin, "{71A991C6-8E56-4BA2-9F23-68CCC301BF71}"_cry_guid);

			virtual ICryOdin* GetOdin() const = 0;
		};

		struct ICryOdin
		{
			virtual ~ICryOdin() {}

			//! All events are called on the main thread. Implement the ones you need.
			struct IListener
			{
				virtual ~IListener() {}

				virtual void OnRoomStatusChanged(ICryOdinRoom& room, ECryOdinRoomStatus status, const char* szMessage) {}
				//! The room is ready, all users already there have had OnPeerJoined.
				virtual void OnJoinedRoom(ICryOdinRoom& room) {}
				//! The room has ended, also on ICryOdin::Shutdown. It's deleted after this call.
				virtual void OnLeftRoom(ICryOdinRoom& room, const char* szReason) {}
				virtual void OnRoomError(ICryOdinRoom& room, const char* szMessage) {}

				virtual void OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user) {}
				//! The user is deleted after this call. Also called for the local user when the
				//! connection ends, even though OnPeerJoined isn't. Check IsLocal.
				virtual void OnPeerLeft(ICryOdinRoom& room, ICryOdinUser& user) {}
				virtual void OnPeerUserDataChanged(ICryOdinRoom& room, ICryOdinUser& user) {}
				//! Also called for the local user, check IsLocal.
				virtual void OnPeerTalkingChanged(ICryOdinRoom& room, ICryOdinUser& user, bool bTalking) {}

				//! @param pSender Null if the sender isn't known.
				//! @param szChannel Empty when none was given.
				virtual void OnTextMessage(ICryOdinRoom& room, ICryOdinUser* pSender, const char* szText, const char* szChannel) {}
				//! @param pSender Null if the sender isn't known.
				virtual void OnDataMessage(ICryOdinRoom& room, ICryOdinUser* pSender, const uint8* pData, uint32 size) {}
				virtual void OnSocketMessage(ICryOdinRoom& room, ICryOdinSocket& socket, const uint8* pData, uint32 size) {}
				//! Every event from the Odin server as JSON, including ones this plugin doesn't handle.
				virtual void OnRawRpc(ICryOdinRoom& room, const char* szJson) {}

				//! ICryOdinRoom::SetTransmitEnabled changed whether the microphone is sent to the room,
				//! for example by push to talk. Use it for a talk icon or a radio click.
				virtual void OnTransmitChanged(ICryOdinRoom& room, bool bTransmitting) {}
				//! ICryOdinRoom::SetEncoderSettings replaced the encoder. GetEncoder returns the new one, and
				//! the old one is gone.
				//! @param bSucceeded False if no new one could be made, so nothing is sent to the room until
				//! it's joined again.
				virtual void OnEncoderChanged(ICryOdinRoom& room, bool bSucceeded) {}

				//! The selected or active device changed, for example when one was unplugged.
				virtual void OnAudioDeviceChanged(bool bInput) {}
			};

			//! Receives a push to talk key, see SubscribePushToTalk. Called on the main thread.
			struct IPushToTalkListener
			{
				virtual ~IPushToTalkListener() {}

				//! The key went down or up. Up also comes when the key was let go without the action
				//! seeing it, for example while the console was open or after the window lost focus.
				virtual void OnPushToTalkInput(bool bDown) = 0;
			};

			//! A player asked the server for a room token through CCryOdinNetworkComponent.
			struct SServerTokenRequest
			{
				//! The game channel of the player, as given to the player entity with SetChannelId.
				int channelId = 0;
				//! The entity with the CCryOdinNetworkComponent.
				EntityId entityId = 0;
				const char* szRoomId = "";
				//! The id the token is signed for, picked by the server.
				const char* szUserId = "";
				//! How long the token can be used to join. The only value a validator can change: the token
				//! is always signed for the room and id above, as the player joins with those.
				int lifetimeSeconds = 300;
			};

			//! Decides on the server which room tokens a player gets, see SetServerTokenValidator.
			struct IServerTokenValidator
			{
				virtual ~IServerTokenValidator() {}

				//! Called on the main thread of the server, after the request passed the plugin's own checks
				//! (the player's own entity, a room allowed by odin_serverAllowedRooms, not too often).
				//! @param refuseReason Sent to the player and logged when refused.
				//! @return False to refuse.
				virtual bool ValidateServerTokenRequest(SServerTokenRequest& request, string& refuseReason) = 0;
			};

			//! Used to register Odin listeners. Call from the main thread only.
			virtual void AddListener(IListener& listener, const char* szName) = 0;
			//! Used to remove Odin listeners. Call from the main thread only.
			virtual void RemoveListener(IListener& listener) = 0;

			//! Listens for a push to talk key: an action in the action map kCryOdinActionMapName of the
			//! engine's action map manager. CCryOdinUserComponent uses this, use it yourself for rooms you
			//! join from code. Calling it again changes the key. The action is set up during the next
			//! update. Call from the main thread only.
			//! @param szName Shown in log messages.
			//! @param key Its bEnabled and bToggle aren't used here. Toggling is up to the listener.
			//! @param bReplaceInputs Replaces the inputs the action already has, also ones from your action
			//! map XML or rebound by the player. Otherwise only an action made by this plugin gets new
			//! inputs, and only when they differ from the ones it was made with.
			virtual void SubscribePushToTalk(IPushToTalkListener& listener, const char* szName, const SCryOdinPushToTalkKey& key, bool bReplaceInputs = false) = 0;
			//! The listener gets no more calls, not even a last key up.
			virtual void UnsubscribePushToTalk(IPushToTalkListener& listener) = 0;

			//! Starts Odin. It's also done by JoinRoom, so this is only needed to set a key early.
			//! On a dedicated server only tokens can be made, there's no audio or rooms.
			//! @param accessKey Your access key, or null to use the one already set or create a temporary one.
			//! Don't ship an access key in a game client, get tokens from your server with
			//! SCryOdinRoomParams::szToken instead.
			virtual bool InitOdin(const char* accessKey = nullptr) = 0;

			//! Leaves all rooms, stops audio and shuts down Odin. All OnPeerLeft and OnLeftRoom events are
			//! sent first. If called from inside one of the events, it happens right after that event's
			//! update. Call InitOdin to start again.
			virtual void Shutdown() = 0;
			virtual bool IsInitialized() const = 0;

			//! Sets the access key used to sign room tokens. Without one, the CVar odin_serverAccessKey is
			//! used, which a dedicated server reads from dedicated.cfg.
			virtual void SetAccessKey(const char* accessKey) = 0;

			//! Creates a new access key and sets it. Use this for debugging, or to save a key for later.
			//! Clients with different keys can't meet in a room.
			virtual string GenerateAccessKey() = 0;

			//! Signs a room token with the access key. Tokens should normally be made on your server.
			//! @param szRoomId Name of the room
			//! @param szUserId Your own id for the user
			//! @param lifetimeSeconds How long the token can be used to join
			//! @return The token, empty on failure
			virtual string GenerateRoomToken(const char* szRoomId, const char* szUserId, int lifetimeSeconds = 300) = 0;

			//! Lets game code on the server refuse token requests from players, or shorten their tokens.
			//! Null removes it. The validator must stay valid until it's removed.
			virtual void SetServerTokenValidator(IServerTokenValidator* pValidator) = 0;

			//! Starts joining a room. Watch OnJoinedRoom for the result, or OnLeftRoom if it failed.
			//! You can be in many rooms, such as one for nearby players and one for team radio.
			//! @return The room, or the existing one if already in it, in which case params are ignored.
			//! Null if it couldn't be started.
			virtual ICryOdinRoom* JoinRoom(const SCryOdinRoomParams& params) = 0;

			//! Leaves a room. OnLeftRoom follows in a later frame.
			virtual void LeaveRoom(ICryOdinRoom* pRoom) = 0;

			virtual ICryOdinRoom* FindRoom(const char* szRoomId) const = 0;
			virtual int GetRoomCount() const = 0;
			virtual ICryOdinRoom* GetRoom(int index) const = 0;

			//! Starts Odin if needed. Null if it can't start, or on a dedicated server.
			virtual ICryOdinAudioSystem* GetAudioSystem() = 0;
			//! Starts Odin if needed. Null if it can't start, or on a dedicated server.
			virtual ICryOdinAudioDevice* GetAudioDevices() = 0;
		};
	}
}
