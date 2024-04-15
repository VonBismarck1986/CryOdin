#pragma once
#include "ICryOdin.h"

#include <CryAction.h> // for entities and other stuff
#include <CryString/CryString.h>
#include <map>


namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioSystem;

		class CCryOdin final 
			: public ICryOdin
			, public ISystemEventListener
		{
		public:
			CCryOdin();
			virtual ~CCryOdin() = default;

			static CCryOdin* Get();

			virtual void Init(const char* accessKey) override;
			virtual void Shutdown() override;
			virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

			virtual void SetAccessKey(const char* accessKey) override;
			virtual const char* GenerateAccessKey() const override;
			virtual const char* GenerateRoomToken(const char* roomID, const char* user_id) const override;
			virtual const char* GetCurrentRoomToken() const override;
			virtual void SetOdinApmConfig(OdinApmConfig config) override;
			virtual OdinApmConfig GetCurrentOdinApmConfig() const override;

			virtual void SetupLocalClient(ICryOdinUser& pEntity) override;
			virtual void RemoveUserFromOdinRoom(const ICryOdinUser& pEntity) override;
			virtual void ConnectUserToOdinRoom(const char* room_name) override;
			virtual uint64_t GetOdinUserId(const EntityId entityId) override;

			virtual OdinMediaStreamHandle GetDefaultUserInputStream() const override { return m_pMainClient.inputStream; }

			virtual void GetPeers(std::unordered_map<uint64_t, ICryOdinUser>& map) const override;

		protected:
			static void handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data);
			void odinEvent(OdinRoomHandle room, const OdinEvent* event, void* data);

			const char* get_name_from_connection_state_change_reason(OdinRoomConnectionStateChangeReason reason);
			const char* get_name_from_connection_state(OdinRoomConnectionState state);
			uint16_t get_media_id_from_handle(OdinMediaStreamHandle handle);

			void ChangeGameStatus();
		public:
			void OnUpdate();
			bool GameplayStarted() const { return m_bGamePlayStarted; }

		private:
			CryFixedStringT<128> m_AccessKey; // this key is for testing
			CryFixedStringT<512> m_RoomToken;
			OdinApmConfig m_ApmConfig;
			OdinRoomHandle m_room;
			ICryOdinUser m_pMainClient;

			CCryOdinAudioSystem* m_pAudioSystem = nullptr;

			// peer_id , ICryOdinUser 
			std::unordered_map<uint64_t, ICryOdinUser> m_usersMap; // turn into vector?



			bool m_bGamePlayStarted;
		};
	}
}