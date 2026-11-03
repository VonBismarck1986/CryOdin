#pragma once

#include <ICryOdinRoom.h>
#include <ICryOdinSocket.h>

#include "CryOdinUser.h"
#include "Utils/OdinHelpers.h"

#include <CryThreading/CryThread.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		class CCryOdin;
		class CCryOdinAudioSystem;
		struct SCryOdinCaptureTarget;
		struct SCryOdinDecoder;

		class CCryOdinSocket final : public ICryOdinSocket
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinSocket)
			explicit CCryOdinSocket(OdinSocket* pSocket);

			// ICryOdinSocket
			virtual ECryOdinSocketKind GetKind() const override { return m_kind; }
			virtual uint32 GetRemotePeerId() const override { return m_remotePeerId; }
			virtual int GetLabel() const override { return m_label; }
			virtual int GetPriority() const override { return m_priority; }
			virtual bool IsInbound() const override { return m_bInbound; }
			virtual uint32 GetUnsentBytes() const override;

			virtual bool Send(const void* pData, uint32 size) override;
			virtual void CloseSending() override;

			virtual OdinSocket* GetNativeHandle() override { return m_pSocket; }
			// ~ICryOdinSocket

			// The room is done with it. Any later use does nothing.
			void Invalidate() { m_pSocket = nullptr; }

		private:
			OdinSocket* m_pSocket;
			ECryOdinSocketKind m_kind = ECryOdinSocketKind::Reliable;
			uint32 m_remotePeerId = 0;
			int m_label = 0;
			int m_priority = 0;
			bool m_bInbound = false;
		};

		class CCryOdinRoom final : public ICryOdinRoom
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinRoom)
			CCryOdinRoom(CCryOdin& odin, CCryOdinAudioSystem& audioSystem, const SCryOdinRoomParams& params);
			virtual ~CCryOdinRoom();

			// Starts connecting. On failure the room can just be deleted. The status is set here
			// and reported with NotifyJoining, once the room is in the list and can be left.
			bool Join(const SCryOdinRoomParams& params, const string& token);
			void NotifyJoining();
			// Ends the room now, sending all the usual events. Used on shutdown.
			void CloseNow(const char* szReason);
			// Starts leaving. The room reports IsFinished after the server has confirmed it.
			void Leave();
			// Ends everything now, without any events. Used on shutdown.
			void ForceClose();

			// Runs the queued server events and sends talking changes. Main thread.
			void OnUpdate(float frameTime);

			bool IsFinished() const { return m_bFinished; }

			// ICryOdinRoom
			virtual const char* GetRoomId() const override { return m_roomId.c_str(); }
			virtual ECryOdinRoomStatus GetStatus() const override { return m_status; }

			virtual ICryOdinUser* GetLocalUser() const override { return m_pLocalUser.get(); }
			virtual int GetPeerCount() const override { return m_bSessionJoined ? static_cast<int>(m_peers.size()) : 0; }
			virtual ICryOdinUser* GetPeer(int index) const override;
			virtual ICryOdinUser* FindPeer(uint32 peerId) const override { return m_bSessionJoined ? FindUser(peerId) : nullptr; }
			virtual ICryOdinUser* FindPeerByUserId(const char* szUserId) const override;

			virtual bool SetUserData(const char* szJson) override;

			virtual bool SendText(const char* szText, const char* szChannel, const uint32* pPeerIds, uint32 peerCount) override;
			virtual bool SendData(const void* pData, uint32 size, const uint32* pPeerIds, uint32 peerCount) override;
			virtual bool SendRpc(const char* szJson) override;

			virtual ICryOdinSocket* OpenSocket(ECryOdinSocketKind kind, uint32 remotePeerId, int label, int priority) override;

			virtual void SetTransmitEnabled(bool bEnabled) override;
			virtual bool IsTransmitEnabled() const override { return m_bTransmit; }

			virtual void SetAudioProcessing(const SCryOdinAudioProcessing& processing) override;
			virtual const SCryOdinAudioProcessing& GetAudioProcessing() const override { return m_processing; }

			virtual void AddCaptureEffect(const ICryOdinAudioEffectPtr& pEffect) override;
			virtual void RemoveCaptureEffect(const ICryOdinAudioEffectPtr& pEffect) override;

			virtual void SetEncoderSettings(const SCryOdinEncoderSettings& settings) override;
			virtual const SCryOdinEncoderSettings& GetEncoderSettings() const override { return m_encoderSettings; }

			virtual void SetLocalLoopback(bool bEnabled) override;

			virtual void SetDefaultVoiceSettings(const SCryOdinVoiceSettings& settings, bool bApplyToExistingVoices) override;
			virtual const SCryOdinVoiceSettings& GetDefaultVoiceSettings() const override { return m_defaultVoice; }

			virtual bool SetChannelMasks(const SCryOdinChannelMask* pMasks, uint32 count, bool bReset) override;
			virtual bool SetPosition(const Vec3& position, uint64 channelMask) override;
			virtual bool ClearPosition(uint64 channelMask) override;

			virtual bool GetConnectionStats(SCryOdinConnectionStats& stats) const override;

			virtual OdinRoom* GetNativeHandle() override { return m_pRoom; }
			virtual OdinEncoder* GetEncoder() override { return m_pEncoder; }
			// ~ICryOdinRoom

		private:
			// One queued event from the Odin network thread
			struct SQueuedEvent
			{
				string rpcJson;
				OdinSocket* pSocket = nullptr;
				std::vector<uint8> socketData;
			};

			// A capture effect has to stay in memory while it's in a pipeline.
			struct SCaptureEffect
			{
				ICryOdinAudioEffectPtr pEffect;
				uint32 effectId = 0;
				bool bRemoved = false; // Odin wouldn't give it up, so it stays until the encoder is gone.
			};

			using TDecoderMap = std::unordered_map<uint32, std::shared_ptr<SCryOdinDecoder>>;

			// Odin network thread
			static void OnDatagramCallback(OdinRoom* pRoom, const OdinDatagramProperties* pProperties, const uint8_t* pBytes, uint32_t length, void* pUserData);
			static void OnRpcCallback(OdinRoom* pRoom, const char* szJson, void* pUserData);
			static void OnSocketCallback(OdinSocket* pSocket, const uint8_t* pMessage, uint32_t length, void* pUserData);
			static void CaptureEffectCallback(float* pSamples, uint32_t sampleCount, bool* pIsSilent, const void* pUserData);

			// Main thread event handlers
			void HandleRpc(const char* szJson);
			void HandleStatusChanged(const char* szStatus, const char* szMessage);
			void HandleJoined(uint32 ownPeerId, const char* szRoomId);
			void HandleLeft(const char* szReason);
			void HandlePeerJoined(uint32 peerId, const char* szUserId, const std::string& userData);
			void HandlePeerLeft(uint32 peerId);
			void HandlePeerChanged(uint32 peerId, const std::string& userData);
			void HandleMessage(uint32 senderPeerId, const void* pMessageJson);
			void HandleSocket(OdinSocket* pSocket, const std::vector<uint8>& data);
			void HandleClosed(const char* szReason);

			void ChangeStatus(ECryOdinRoomStatus status, const char* szMessage);
			void UpdateTalking(float frameTime);
			void UpdatePositionKeepAlive(float frameTime);
			// Makes the encoder again once SetEncoderSettings has settled.
			void UpdatePendingEncoder(float frameTime);

			CCryOdinUser* FindUser(uint32 peerId) const;
			CCryOdinUser* CreatePeer(uint32 peerId, const char* szUserId, const std::string& userData);
			bool CreateVoice(CCryOdinUser& user);
			void PublishDecoders();

			bool CreateEncoder(uint32 ownPeerId);
			void DestroyEncoder();
			void ApplyAudioProcessing();
			void InsertCaptureEffect(SCaptureEffect& effect);
			void DetachLoopback();

			void DropSessionState();
			void CloseHandle();
			void ApplyPositionsToEncoder();
			void RemoveChannelsFromPositions(uint64 channelMask);

			bool SendMessagePayload(const void* pData, uint32 size, const uint32* pPeerIds, uint32 peerCount);
			void ApplyUserDataToUser(CCryOdinUser& user, const std::string& userData);
			void UpdatePasswordStatus(CCryOdinUser& user);

			CCryOdin& m_odin;
			CCryOdinAudioSystem& m_audioSystem;

			string m_roomId;
			string m_leaveReason;
			OdinRoom* m_pRoom = nullptr;
			OdinCipher* m_pCipher = nullptr; // Owned by the room handle
			OdinRoomEvents m_events;
			ECryOdinRoomStatus m_status = ECryOdinRoomStatus::None;
			bool m_bHoldingDevices = false;
			bool m_bFinished = false;
			bool m_bSessionJoined = false; // The Joined event arrived in this connection
			float m_fClosingTime = 0.f;

			// Network thread queue
			CryCriticalSection m_queueLock;
			std::vector<SQueuedEvent> m_queue;
			std::vector<SQueuedEvent> m_processing_queue;

			// Read by the network thread
			std::shared_ptr<const TDecoderMap> m_pDecoders;

			// Users
			std::unique_ptr<CCryOdinUser> m_pLocalUser;
			std::vector<std::unique_ptr<CCryOdinUser>> m_peers;
			std::vector<std::unique_ptr<CCryOdinUser>> m_retiredUsers;
			std::string m_localUserData;
			string m_localUserId;

			// Sockets, valid for one connection
			std::vector<std::unique_ptr<CCryOdinSocket>> m_sockets;
			std::vector<std::unique_ptr<CCryOdinSocket>> m_retiredSockets;

			// Microphone
			OdinEncoder* m_pEncoder = nullptr;
			const OdinPipeline* m_pPipeline = nullptr;
			std::shared_ptr<SCryOdinCaptureTarget> m_pCaptureTarget;
			SCryOdinAudioProcessing m_processing;
			SCryOdinEncoderSettings m_encoderSettings;
			SCryOdinEncoderSettings m_builtEncoderSettings; // What the current encoder was made with.
			bool m_bEncoderRebuildPending = false;
			float m_fEncoderRebuildDelay = 0.f;
			uint32 m_apmEffectId = 0;
			uint32 m_vadEffectId = 0;
			uint32 m_viEffectId = 0;
			std::vector<std::unique_ptr<SCaptureEffect>> m_captureEffects;
			bool m_bTransmit = true;
			bool m_bLoopback = false;
			std::shared_ptr<SCryOdinDecoder> m_pLoopbackDecoder;

			// Reapplied when the encoder is recreated. Until the game sets any, a new encoder keeps its default of channel 1.
			struct SPositionEntry
			{
				uint64 channelMask;
				Vec3 position;
			};
			std::vector<SPositionEntry> m_positions;
			bool m_bHasCustomPositions = false;
			bool m_bEncoderHasDefaultChannel = false;
			bool m_bPositionGateForced = false; // Keeps position updates going with the voice closed off

			SCryOdinVoiceSettings m_defaultVoice;
		};
	}
}
