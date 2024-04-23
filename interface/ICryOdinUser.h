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
			virtual void Talking(bool yesNo) = 0;
			virtual bool IsTalking() = 0;

			virtual void SetMediaHandle(EAudioHandleType type, OdinMediaStreamHandle handle) = 0;
			virtual void SetRoomHandle(OdinRoomHandle room) = 0;
		};


		// thinking how I should do this.. 
		struct ICryOdinUserComponent : public IEntityComponent
		{
			virtual ~ICryOdinUserComponent() {};

			virtual ICryOdinUser* GetOdinUser() = 0;
		};
	}
}