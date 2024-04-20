#pragma once
#include "ICryOdin.h"

#include <ICryOdinAudioSound.h>
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
			virtual ~CCryOdin();

			static CCryOdin* Get();

			virtual bool Init(const char* accessKey) override;
			virtual void Shutdown() override;
			virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

			virtual void SetAccessKey(const char* accessKey) override;
			virtual const char* GenerateAccessKey() const override;
			virtual const char* GenerateRoomToken(const char* roomID, const char* user_id) const override;
			virtual void SetOdinApmConfig(OdinApmConfig config) override;
			virtual OdinApmConfig GetCurrentOdinApmConfig() const override;

			// Connections
	
			virtual bool SetupAndConnectLocalClient(const IUser& pUser) override;
			virtual void RemoveLocalClientFromOdinRoom(const IUser& pEntity) override;

			virtual void ConnectUserToOdinRoom(const char* room_name) override;
			virtual uint64_t GetOdinUserId(const IUser entityId) override;
			virtual void GetPeers(std::unordered_map<uint64_t, IUser>& map) const override;

			void OnUpdate(float frameTime);

			Sounds GetSounds() const { return m_sounds; }
		protected:
			static void handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data);
			void odinEvent(OdinRoomHandle room, const OdinEvent* event, void* data);

			const char* get_name_from_connection_state_change_reason(OdinRoomConnectionStateChangeReason reason);
			const char* get_name_from_connection_state(OdinRoomConnectionState state);
			uint16_t get_media_id_from_handle(OdinMediaStreamHandle handle);


			void StartUpSounds();
		private:
			CryFixedStringT<128> m_AccessKey; 
			CryFixedStringT<512> m_RoomToken;
			OdinRoomHandle m_room;
			IUser m_pCurrentUser;


			CCryOdinAudioSystem* m_pAudioSystem = nullptr;

			// peer_id , ICryOdinUser 
			std::unordered_map<uint64_t, IUser> m_usersMap; // turn into vector?


			Sounds m_sounds;
		};
	}
}