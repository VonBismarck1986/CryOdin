#pragma once
#include "Utils/OdinHelpers.h"

#include <ICryOdin.h>
#include <CryCore/Containers/CryListenerSet.h>
#include <CryString/CryString.h>

#include <memory>
#include <unordered_map>
#include <vector>

struct ICVar;

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioSystem;
		class CCryOdinInput;
		class CCryOdinRoom;

		class CCryOdin final : public ICryOdin
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdin)
			CCryOdin();
			virtual ~CCryOdin();

			static CCryOdin* Get();

			// ICryOdin
			virtual void AddListener(IListener& listener, const char* szName) override { m_listeners.Add(&listener, szName); }
			virtual void RemoveListener(IListener& listener) override { m_listeners.Remove(&listener); }

			virtual void SubscribePushToTalk(IPushToTalkListener& listener, const char* szName, const SCryOdinPushToTalkKey& key, bool bReplaceInputs) override;
			virtual void UnsubscribePushToTalk(IPushToTalkListener& listener) override;

			virtual bool InitOdin(const char* accessKey) override;
			virtual void Shutdown() override;
			virtual bool IsInitialized() const override { return m_bInitialized; }

			virtual void SetAccessKey(const char* accessKey) override;
			virtual string GenerateAccessKey() override;
			virtual string GenerateRoomToken(const char* szRoomId, const char* szUserId, int lifetimeSeconds) override;
			virtual void SetServerTokenValidator(IServerTokenValidator* pValidator) override { m_pServerTokenValidator = pValidator; }

			virtual ICryOdinRoom* JoinRoom(const SCryOdinRoomParams& params) override;
			virtual void LeaveRoom(ICryOdinRoom* pRoom) override;

			virtual ICryOdinRoom* FindRoom(const char* szRoomId) const override;
			virtual int GetRoomCount() const override { return static_cast<int>(m_rooms.size()); }
			virtual ICryOdinRoom* GetRoom(int index) const override;

			virtual ICryOdinAudioSystem* GetAudioSystem() override;
			virtual ICryOdinAudioDevice* GetAudioDevices() override;
			// ~ICryOdin

			void OnUpdate(float const frameTime);

			// Each game mode session in the Sandbox starts clean.
			void OnEditorGameModeEnded();
			// Before CryAction shuts down, which owns the push to talk actions.
			void ShutdownInput();
			// Called by a room when SetTransmitEnabled changed it. The room can be deleted when this returns.
			void NotifyTransmitChanged(ICryOdinRoom& room, bool bTransmitting);
			// Extra logging is on, which shows received text in the console.
			bool IsDebugEnabled() const;

			// True when Odin may start by itself. False once it was shut down, until InitOdin is called.
			bool EnsureStarted();

			// Used by components that shouldn't start Odin just by existing.
			CCryOdinAudioSystem* PeekAudioSystem() const { return m_pAudioSystem.get(); }

			// Reads the odin_ settings from dedicated.cfg, only those, as the game may load the rest itself.
			void LoadServerConfig();
			// Checks a player's request against odin_serverAllowedRooms and the game's validator, and signs it.
			// The request's lifetime can be changed by the validator.
			bool SignServerToken(SServerTokenRequest& request, string& outToken, string& outRefuseReason);
			// Counts a token request from a player's channel. False once it asks too often.
			// @param outLogRefusal True for the first refusal in a window, so a flood logs once.
			bool AllowServerRequest(int channelId, bool& outLogRefusal);

			template<typename TFunction>
			void ForEachListener(const TFunction& function)
			{
				m_listeners.ForEach([&function](IListener* pListener) { function(*pListener); });
			}

		private:
			static void OnOdinLog(const char* szJson);

			void RegisterConsole();
			void UnregisterConsole();
			void DrawDebug();
			void ShutdownNow();
			// The key that signs tokens, see SetAccessKey. Creates a temporary one when there's none. Empty on failure.
			string GetSigningKey();
			bool IsRoomAllowedOnServer(const char* szRoomId) const;

			static CCryOdin* s_instance;

			bool m_bInitialized = false;
			bool m_bShutDown = false;      // Don't start again by itself once shut down.
			bool m_bShuttingDown = false;  // Listeners are being told about it.
			bool m_bShutdownPending = false;
			int m_eventDepth = 0;          // Rooms are on the call stack and can't be deleted now.
			string m_accessKey;
			bool m_bTemporaryAccessKey = false; // Made because no key was set, so odin_serverAccessKey still wins.
			ICVar* m_pServerAccessKey = nullptr;
			ICVar* m_pServerAllowedRooms = nullptr;
			IServerTokenValidator* m_pServerTokenValidator = nullptr;
			struct SRequestWindow
			{
				float start = 0.f;
				int count = 0;
			};
			std::unordered_map<int, SRequestWindow> m_serverRequestWindows; // By player channel.

			std::unique_ptr<CCryOdinInput> m_pInput;
			std::unique_ptr<CCryOdinAudioSystem> m_pAudioSystem;
			std::vector<std::unique_ptr<CCryOdinRoom>> m_rooms;

			CListenerSet<IListener*> m_listeners = 1;

			bool m_bConsoleRegistered = false;
		};
	}
}
