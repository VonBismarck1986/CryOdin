#pragma once

#include "CrySystem/ICryPlugin.h"
#include "ICryOdinUser.h"
#include <miniaudio.h>
#include <odin.h>

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


		/* Probably shoud seprate these two in different files ( headers ) but, I'm too lazy... so yea :D */
		class CCryOdinUserComponent;

		struct ICryOdin
		{
			virtual ~ICryOdin() {}

			struct IListener
			{
				virtual void OnJoinedRoom(const char* room_name) = 0;
				virtual void OnPeerJoined(CCryOdinUserComponent* user) = 0;
				virtual void OnPeerMediaAdded(CCryOdinUserComponent* user) = 0;
				virtual void OnPeerLeft(CCryOdinUserComponent* user) = 0;
				virtual void OnPeerUserDataUpated(CCryOdinUserComponent* user) = 0;
			};

			//! Used to register Odin listeners
			virtual void AddListener(IListener& listener, const char* szName) = 0;
			//! Used to remove Odin listeners
			virtual void RemoveListener(IListener& listener) = 0;

			// Start Up Odin and pass your accessKey.
			// It's recommend to pass key from a rest-api 
			// @param const char* - your Odin Access Key
			virtual bool InitOdin(const char* accessKey) = 0;

			// Shutdown Odin
			virtual void Shutdown() = 0;

			// Set Current Access Key, you don't have to if already set key via Init(). Again if 
			// in release mode best to pass Access from a rest-api 
			// @param const char* - Pass Access Key 
			virtual void SetAccessKey(const char* accessKey) = 0;

			// Generate a new Access Key - Note use this for only debugging or if you want to generate to save later to file
			virtual const char* GenerateAccessKey() const = 0;

			// Generate Room Access Token - you must generate per room creation
			//@param const char* - RoomID this is a string ( i.e. "DefaultRoom" or "LobbyRoom" etc )
			//@param const char* - This Is actually EntityID best to create a string and format with EntityID and pass it with c_str ( example: string userID = string().Format("%s", EntityID);
			virtual const char* GenerateRoomToken(const char* roomID, const char* user_id) const = 0;

			// Set up APM Config for Odin and current user, note this effect current user APM settings / Room APM
			//@param OdinApmConfig - base struct for APM settings
			virtual void SetOdinApmConfig(OdinApmConfig config) = 0;



			virtual bool SetUpLocalUser(const char* user_name, EntityId entityId) = 0;

			virtual bool JoinRoom(const char* room_name, const OdinApmConfig& config, CCryOdinUserComponent* odinComponent) = 0;

		};



	}
}