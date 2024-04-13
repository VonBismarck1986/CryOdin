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

		class CCryOdin final : public ICryOdin
		{
		public:
			CCryOdin();
			virtual ~CCryOdin();


			// Start Up Odin, pass Accesskey and OdinApmConfig
			virtual void Init(const char* user_name, const char* accessKey, OdinApmConfig config) override;
			// Shutdown Odin
			virtual void Shutdown() override;

			// Set Current Access Key, you don't have to if already set key via Init()
			// @param const char* - Pass Access Key 
			virtual void SetAccessKey(const char* accessKey) override;

			// Generate a new Access Key - Note use this for only debugging or if you want to generate to save later to file
			virtual const char* GenerateAccessKey() const override;

			// Generate Room Access Token - you must generate per room creation
			//@param const char* - RoomID this is a string ( i.e. "DefaultRoom" or "LobbyRoom" etc )
			virtual const char* GenerateRoomToken(const char* roomID, const char* userName) const override;

			// Get the current Room Token being used
			//@return const char of token
			virtual const char* GetCurrentRoomToken() const override;

			// Set up APM Config for Odin
			//@param OdinApmConfig - base struct for APM settings
			virtual void SetOdinApmConfig(OdinApmConfig config) override;

			// Return current APM Settings for Odin
			//@return OdinApmConfig - struct params
			virtual OdinApmConfig GetCurrentOdinApmConfig() const override;


			// Connections

			// Connect based Client to Odin Network ( This must be Local Client not Client's that are connecting to your server/game connecting )
			// before using this function make sure to Access Key has been set if set no need to input username
			// @param IEntity - Passing refs of Entity
			// @param user_name - Pass user's name this used internally on Odin network.
			virtual void ConnectUserToOdin(ICryOdinUser& pEntity, const char* user_name = nullptr) override;

			// Remove user from a Odin Room, must pass Odin Room Handle and EntityID 
			virtual void RemoveUserFromOdinRoom(const char* room_name, const EntityId entityId) override;

			// Connect a user to a room, must pass Odin Room Handle and Entity ID
			// @param 
			virtual void ConnectUserToOdinRoom(const char* room_name, const EntityId entityId, const char* user_name = nullptr) override;

			// Use this function to return Odin peer id of a user 
			// @param EntityId - pass EntityID 
			// @return uint64_t - Odin peerID
			virtual uint64_t GetOdinUserId(const EntityId entityId) override;

			virtual OdinMediaStreamHandle GetDefaultUserInputStream() const override { return m_pMainClient.inputStream; }

		protected:
			static void handle_odin_event(OdinRoomHandle room, const OdinEvent* event, void* data);
			void odinEvent(OdinRoomHandle room, const OdinEvent* event, void* data);

			const char* get_name_from_connection_state_change_reason(OdinRoomConnectionStateChangeReason reason);
			const char* get_name_from_connection_state(OdinRoomConnectionState state);
			uint16_t get_media_id_from_handle(OdinMediaStreamHandle handle);

		public:
			void OnUpdate();

		private:
			CryFixedStringT<128> m_AccessKey; // this key is for testing
			CryFixedStringT<512> m_RoomToken;
			OdinApmConfig m_ApmConfig;
			OdinRoomHandle m_room;
			ICryOdinUser m_pMainClient;

			CCryOdinAudioSystem* m_pAudioSystem = nullptr;
		};
	}
}