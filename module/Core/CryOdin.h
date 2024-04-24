#pragma once 
#include "Utils/OdinHelpers.h"

#include <ICryOdin.h>
#include <CryAction.h> // for entities and other stuff
#include <CryString/CryString.h>
#include <map>


namespace Cry
{
	namespace Odin
	{
		class CCryOdinUser;
		class CCryOdinAudioSystem;

		class CCryOdin final : public ICryOdin
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdin)
			CCryOdin();
			virtual ~CCryOdin();

			static CCryOdin* Get();

			virtual void AddListener(IListener& listener, const char* szName) override { m_listeners.Add(&listener, szName); }
			virtual void RemoveListener(IListener& listener) override { m_listeners.Remove(&listener); }

			virtual bool InitOdin(const char* accessKey) override;
			virtual void Shutdown() override;
			virtual void SetAccessKey(const char* accessKey) override { m_AccessKey = accessKey; };

			virtual const char* GenerateAccessKey() const override;
			virtual const char* GenerateRoomToken(const char* roomID, const char* user_id) const override;
			virtual void SetOdinApmConfig(OdinApmConfig config) override;

			virtual bool SetUpLocalUser(const char* user_name, EntityId entityId) override;

			virtual bool JoinRoom(const char* room_name, const OdinApmConfig& config, CCryOdinUserComponent* odinComponent) override;


			void OnUpdate(float const frameTime);
		protected:
			static CCryOdin* s_instance;

			static void handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data);
			void odinEvent(OdinRoomHandle room, const OdinEvent* event, void* data);

			const char* get_name_from_connection_state_change_reason(OdinRoomConnectionStateChangeReason reason);
			const char* get_name_from_connection_state(OdinRoomConnectionState state);
			uint16_t get_media_id_from_handle(OdinMediaStreamHandle handle);

		private:
			CryFixedStringT<128> m_AccessKey;
			CryFixedStringT<512> m_RoomToken;
			OdinRoomHandle m_room;

			std::unique_ptr<CCryOdinAudioSystem> m_pAudioSystem = nullptr;
			std::unordered_map<uint64_t, std::unique_ptr<CCryOdinUserComponent>> m_userMap;

			CCryOdinUser* m_localUser;

			CListenerSet<IListener*> m_listeners = 1;

		protected:
			struct SNativeJsonOdinUser
			{
				string name;
				string userId;
				string status;
				int outputMuted;
				int inputMuted;

				// The Serialize function is required, and will be called when reading from the JSON
				void Serialize(Serialization::IArchive& ar)
				{
					// Serialize the string under the name "myString"
					// The second argument is the label, used for UI serialization
					ar(name, "name", "UserName");
					ar(userId, "userId", "UserID");
					ar(status, "status", "OnlineStatus");
					ar(outputMuted, "outputMuted", "outputMuted");
					ar(inputMuted, "inputMuted", "inputMuted");
					
				}
			};
		};
	}
}