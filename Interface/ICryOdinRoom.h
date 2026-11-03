#pragma once

#include "ICryOdinTypes.h"
#include "ICryOdinAudioEffect.h"

namespace Cry
{
	namespace Odin
	{
		struct ICryOdinUser;
		struct ICryOdinSocket;

		//! A joined or joining Odin room. Get it from ICryOdin::JoinRoom.
		//! Call from the main thread only. Valid until OnLeftRoom has returned.
		struct ICryOdinRoom
		{
			virtual ~ICryOdinRoom() {}

			virtual const char* GetRoomId() const = 0;
			virtual ECryOdinRoomStatus GetStatus() const = 0;

			// Users
			///////////////////////////////////////////////////////////////

			//! Null until joined.
			virtual ICryOdinUser* GetLocalUser() const = 0;
			//! Number of remote users. Users can only be found once the room is joined.
			virtual int GetPeerCount() const = 0;
			virtual ICryOdinUser* GetPeer(int index) const = 0;
			virtual ICryOdinUser* FindPeer(uint32 peerId) const = 0;
			virtual ICryOdinUser* FindPeerByUserId(const char* szUserId) const = 0;

			//! Replaces the local user data and syncs it to all users.
			//! @param szJson A JSON object. Add "name" to keep the name visible to others.
			virtual bool SetUserData(const char* szJson) = 0;

			// Text and data
			///////////////////////////////////////////////////////////////

			//! Sends text to users in the room, received with OnTextMessage.
			//! @param szChannel Free text passed to the receiver, such as "team". Can be null.
			//! @param pPeerIds Who receives it. Null sends to everyone else.
			virtual bool SendText(const char* szText, const char* szChannel = nullptr, const uint32* pPeerIds = nullptr, uint32 peerCount = 0) = 0;
			//! Sends bytes reliably and in order, received with OnDataMessage.
			virtual bool SendData(const void* pData, uint32 size, const uint32* pPeerIds = nullptr, uint32 peerCount = 0) = 0;
			//! Sends any Odin client command as JSON.
			virtual bool SendRpc(const char* szJson) = 0;

			//! Opens a data channel, messages arrive with OnSocketMessage.
			//! @param remotePeerId The peer to connect to, 0 for all peers.
			//! @return The socket, owned by the room. Null on failure or when not joined.
			virtual ICryOdinSocket* OpenSocket(ECryOdinSocketKind kind, uint32 remotePeerId, int label = 0, int priority = 0) = 0;

			// Microphone
			///////////////////////////////////////////////////////////////

			//! Turns sending to this room on or off. Use it for push to talk.
			virtual void SetTransmitEnabled(bool bEnabled) = 0;
			virtual bool IsTransmitEnabled() const = 0;

			//! Changes the microphone processing of this room, also while joined.
			virtual void SetAudioProcessing(const SCryOdinAudioProcessing& processing) = 0;
			virtual const SCryOdinAudioProcessing& GetAudioProcessing() const = 0;

			//! Adds your own effect to the microphone, after all other processing. Others hear the result.
			//! It always receives 960 stereo frames.
			virtual void AddCaptureEffect(const ICryOdinAudioEffectPtr& pEffect) = 0;
			virtual void RemoveCaptureEffect(const ICryOdinAudioEffectPtr& pEffect) = 0;

			//! Changes how your voice is encoded for this room, such as its bitrate. While joined, the
			//! encoder is made again a moment later. That gives a short gap in your voice and restarts
			//! the microphone processing of this room, such as the echo canceller, for a second or two.
			//! Changes in quick succession, such as from a slider, become one. Before joining, or with
			//! settings that encode the same, it only stores them.
			virtual void SetEncoderSettings(const SCryOdinEncoderSettings& settings) = 0;
			virtual const SCryOdinEncoderSettings& GetEncoderSettings() const = 0;

			//! Plays your own encoded voice back to you with the local user's voice settings.
			//! This is for testing the audio path without a second client.
			virtual void SetLocalLoopback(bool bEnabled) = 0;

			// Voices
			///////////////////////////////////////////////////////////////

			//! The settings used for voices of users who join later.
			//! @param bApplyToExistingVoices Also changes the voices already playing, except those
			//! bound to a CCryOdinComponentSound, which have their own settings.
			virtual void SetDefaultVoiceSettings(const SCryOdinVoiceSettings& settings, bool bApplyToExistingVoices = false) = 0;
			virtual const SCryOdinVoiceSettings& GetDefaultVoiceSettings() const = 0;

			// Server side filtering
			///////////////////////////////////////////////////////////////

			//! Chooses which peers and channels the server sends to you.
			//! @param bReset True replaces all existing masks, false merges them.
			virtual bool SetChannelMasks(const SCryOdinChannelMask* pMasks, uint32 count, bool bReset) = 0;
			//! Sets the position used by the server to skip voices too far away. It's kept for reconnects.
			//! A new room sends on channel 1 only. The first call to this or ClearPosition replaces that,
			//! after which you send on exactly the channels you've given a position.
			//! @param position Scaled so that the culling radius is 1.0. Odin stores it in steps of 1/16.
			//! @param channelMask The channels to send on with this position, up to 12 channels in total.
			virtual bool SetPosition(const Vec3& position, uint64 channelMask = 1) = 0;
			//! Stops sending on these channels. When none are left, nothing is sent to this room.
			virtual bool ClearPosition(uint64 channelMask) = 0;

			// Debug and raw access
			///////////////////////////////////////////////////////////////

			//! @return False until there's a connection.
			virtual bool GetConnectionStats(SCryOdinConnectionStats& stats) const = 0;

			//! The raw handles. Include <odin.h> to use them.
			virtual OdinRoom* GetNativeHandle() = 0;
			//! Null when not joined. It's replaced on each join and by SetEncoderSettings, so get it again
			//! after OnJoinedRoom and OnEncoderChanged.
			virtual OdinEncoder* GetEncoder() = 0;
		};
	}
}
