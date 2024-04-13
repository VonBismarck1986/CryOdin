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

		// Forward Delcared
		struct ICryOdinAudioDevice;

		struct ICryOdin
		{
			virtual ~ICryOdin() {}

			// Basic setup

			// Start Up Odin, pass Accesskey, OdinApmConfig, and first user
			virtual void Init(const char* accessKey, OdinApmConfig config) = 0;
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

			// Get the current Room Token being used
			//@return const char of token
			virtual const char* GetCurrentRoomToken() const = 0;

			// Set up APM Config for Odin
			//@param OdinApmConfig - base struct for APM settings
			virtual void SetOdinApmConfig(OdinApmConfig config) = 0;

			// Return current APM Settings for Odin
			//@return OdinApmConfig - struct params
			virtual OdinApmConfig GetCurrentOdinApmConfig() const = 0;


			// Connections

			// Connect based Client to Odin Network ( This must be Local Client not Client's that are connecting to your server/game connecting )
			// before using this function make sure to Access Key has been set if set no need to input username
			// @param IEntity - Passing refs of Entity
			// @param user_name - Pass user's name this used internally on Odin network.
			virtual void ConnectUserToOdin(ICryOdinUser& pEntity, const char* user_name = nullptr) = 0;

			// Remove user from a Odin Room, must pass Odin Room Handle and EntityID 
			virtual void RemoveUserFromOdinRoom(const char* room_name, const EntityId entityId) = 0;

			// Connect a user to a room, must pass Odin Room Handle and Entity ID
			// @param 
			virtual void ConnectUserToOdinRoom(const char* room_name, const EntityId entityId, const char* user_name = nullptr) = 0;

			// Use this function to return Odin peer id of a user 
			// @param EntityId - pass EntityID 
			// @return uint64_t - Odin peerID
			virtual uint64_t GetOdinUserId(const EntityId entityId) = 0;



			// MISC

			virtual OdinMediaStreamHandle GetDefaultUserInputStream() const = 0;


		};

	}
}