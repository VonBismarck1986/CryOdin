#pragma once

#include <odin.h>
#include <miniaudio.h>
#include <CryEntitySystem/IEntity.h>

namespace Cry
{
	namespace Odin
	{
		//TODO:: Rewrite most of this if/when needed should be simple enough to pass to and from Odin and Cryengine

		struct ICryOdinUser
		{

			IEntity* pUserEntity;
			const char* userName;
			uint16_t OdinID;
			bool     isTalking;
			bool     isMuted;
			OdinMediaStreamHandle inputStream;
			CryFixedStringT<512> roomToken;
			OdinApmConfig apmConfig;

			ICryOdinUser(IEntity* pEntity,uint16_t odinID, const char* user_name, const char* room_token) 
				: pUserEntity(pEntity)
				, userName(user_name)
				, OdinID(odinID)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, roomToken(room_token)
				, apmConfig(OdinApmConfig())
			{
			}

			ICryOdinUser(IEntity* pEntity, uint16_t odinID, const char* user_name)
				: pUserEntity(pEntity)
				, userName(user_name)
				, OdinID(odinID)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, roomToken(nullptr)
				, apmConfig(OdinApmConfig())
			{
			}

			ICryOdinUser() 
				: pUserEntity(nullptr)
				, userName(nullptr)
				, OdinID(-1)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, roomToken(nullptr)
				, apmConfig(OdinApmConfig())
			{
			}

			virtual ~ICryOdinUser() {}

			// INLINE ??

			ILINE bool IsTalking() const { return isTalking; }
			ILINE bool IsMuted() const { return isMuted; }
			ILINE uint16_t GetOdinID() const { return OdinID; }
			ILINE const char* GetUserName() const { return userName; }
		};
	}
}