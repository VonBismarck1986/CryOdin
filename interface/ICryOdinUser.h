#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <ICryOdinAudioHandle.h>
#include <odin.h>

namespace Cry
{
	namespace Odin
	{
		struct ICryOdinUser
		{
			virtual ~ICryOdinUser() {}

			virtual string GetUserName() const = 0;
			virtual uint64_t GetUserId() const = 0;
			virtual uint64_t GetPeerId() const = 0;
			virtual const char* RoomName() const = 0;

			virtual OdinMediaStreamHandle GetMediaHandle(EAudioHandleType type) const = 0;
			virtual OdinRoomHandle GetRoomHandle() const = 0;

			virtual string ToStringDebug() const = 0;
			virtual bool IsTalking() = 0;
		};


		// thinking how I should do this.. 
		struct ICryOdinUserComponent : public IEntityComponent
		{



		private:
			ICryOdinUser* m_pOdinUser;
		};
	}
}