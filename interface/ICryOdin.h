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

		
		struct ICryOdin
		{
			virtual ~ICryOdin() {}


			// Start Up Odin and pass your accessKey
			// @param const char* - your Odin Access Key
			virtual bool Init(const char* accessKey) = 0;

			// Shutdown Odin
			virtual void Shutdown() = 0;

			// Set Current Access Key, you don't have to if already set key via Init()
			// @param const char* - Pass Access Key 
			virtual void SetAccessKey(const char* accessKey) = 0;

			// Generate a new Access Key - Note use this for only debugging or if you want to generate to save later to file
			virtual const char* GenerateAccessKey() const = 0;

			// Generate Room Access Token - you must generate per room creation
			//@param const char* - RoomID this is a string ( i.e. "DefaultRoom" or "LobbyRoom" etc )
			//@param const char* - This Is actually EntityID best to create a string and format with EntityID and pass it with c_str ( example: string userID = string().Format("%s", EntityID);
			virtual const char* GenerateRoomToken(const char* roomID, const char* user_id) const = 0;

			// Set up APM Config for Odin and current user, note this effect current user APM settings
			//@param OdinApmConfig - base struct for APM settings
			virtual void SetOdinApmConfig(OdinApmConfig config) = 0;

			// Return current APM Settings for Odin
			//@return OdinApmConfig - struct params
			virtual OdinApmConfig GetCurrentOdinApmConfig() const = 0;


			// Connections

			// Setup the local Client
			//@param ICryOdinUser - This is basic struct of details of the local user and all users from Odin to Cryengine
			virtual bool SetupAndConnectLocalClient(const IUser& pUser) = 0;

			// Use this function to remove local user ( current ) from a room
			//@param ICryOdinUser - This is basic struct of details of the local user and all users from Odin to Cryengine
			virtual void RemoveLocalClientFromOdinRoom(const IUser& pEntity) = 0;

			// Connect the local user to a room, must pass a room name this will become a new RoomHandle
			// @param const char* - Just pass a room name example "my_room"
			virtual void ConnectUserToOdinRoom(const char* room_name) = 0;

			// Use this function to return Odin peer id of a user 
			// @param IUser - pass Cry::Odin::IUser ( IUser ) 
			// @return uint64_t - Odin peerID
			virtual uint64_t GetOdinUserId(const IUser entityId) = 0;

			// All connected peers in current room
			//@param std::map - pass a empty map of uint64_t , IUser this will copy over to your map
			virtual void GetPeers(std::unordered_map<uint64_t, IUser>& map) const = 0;
		};

	}
}