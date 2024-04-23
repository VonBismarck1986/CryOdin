#include "StdAfx.h"
#include "CryOdinUser.h"

namespace Cry
{
	namespace Odin
	{
		OdinMediaStreamHandle CCryOdinUser::GetMediaHandle(EAudioHandleType type) const
		{
			auto index = static_cast<size_t>(type);
			return m_mediaHandles[index];
		}

		string CCryOdinUser::ToStringDebug() const
		{
			return string().Format("Username: %s\nPeerID: (%d)\nUserID: (%d)\nRoom: %s\nInputHandle: (%d)\nOutputHandle: (%d)", m_UserName, m_PeerID, m_UserID, m_RoomName.c_str(), m_mediaHandles[1], m_mediaHandles[2]);
		}

		void CCryOdinUser::SetMediaHandle(EAudioHandleType type, OdinMediaStreamHandle handle)
		{
			auto index = static_cast<size_t>(type);
			m_mediaHandles[index] = handle;
		}
	}
}