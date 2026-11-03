#pragma once

#include "ICryOdinTypes.h"

namespace Cry
{
	namespace Odin
	{
		struct ICryOdinRoom;
		struct ICryOdinSound;

		//! A user in a room, either the local one or a remote peer.
		//! Call from the main thread only. Valid until OnPeerLeft has returned. That event also comes for
		//! the local user, when leaving the room or when the connection is lost.
		struct ICryOdinUser
		{
			virtual ~ICryOdinUser() {}

			//! The "name" field of the user data, or the user id when it has none.
			virtual const char* GetName() const = 0;
			//! The id given when joining, from the room token.
			virtual const char* GetUserId() const = 0;
			//! The id of this user in this room session. It can change after a reconnect.
			virtual uint32 GetPeerId() const = 0;
			//! The user data as JSON text.
			virtual const char* GetUserData() const = 0;

			virtual ICryOdinRoom& GetRoom() const = 0;
			virtual bool IsLocal() const = 0;
			virtual bool IsTalking() const = 0;

			//! The voice of this user. Null for the local user, unless loopback is on.
			virtual ICryOdinSound* GetSound() const = 0;

			//! True when an encrypted room's password doesn't match this user's, so they can't be heard.
			virtual bool HasPasswordMismatch() const = 0;

			virtual string ToStringDebug() const = 0;
		};
	}
}
