#pragma once

#include "ICryOdinTypes.h"

namespace Cry
{
	namespace Odin
	{
		//! A data channel to one peer, or to all peers when the remote peer id is 0.
		//! Sockets belong to the room and end when it disconnects, so open them again after OnJoinedRoom.
		//! Drop your pointers when the status of the room leaves Joined, or in OnPeerLeft of the local user.
		//! Call from the main thread only.
		struct ICryOdinSocket
		{
			virtual ~ICryOdinSocket() {}

			virtual ECryOdinSocketKind GetKind() const = 0;
			virtual uint32 GetRemotePeerId() const = 0;
			virtual int GetLabel() const = 0;
			virtual int GetPriority() const = 0;
			virtual bool IsInbound() const = 0;
			virtual uint32 GetUnsentBytes() const = 0;

			virtual bool Send(const void* pData, uint32 size) = 0;
			//! Stops sending but keeps receiving. Messages already queued are still sent.
			virtual void CloseSending() = 0;

			//! The raw Odin socket. Include <odin.h> to use it.
			virtual OdinSocket* GetNativeHandle() = 0;
		};
	}
}
