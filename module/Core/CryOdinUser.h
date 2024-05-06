#pragma once

#include <ICryOdinUser.h>
#include "Utils/OdinHelpers.h"

namespace Cry
{
	namespace Odin
	{
		class CCryOdinUser : public ICryOdinUser
		{
		public:
			CCryOdinUser() = delete;
			explicit CCryOdinUser(string userName, uint64_t peer_id, uint64_t user_id)
				: m_UserName(userName)
				, m_PeerID(peer_id)
				, m_UserID(user_id)
				, m_mediaHandles()
				, m_room(0)
			{}
			explicit CCryOdinUser(uint64_t peer_id, uint64_t user_id)
				: m_PeerID(peer_id)
				, m_UserID(user_id)
				, m_room(0)
			{
				m_mediaHandles[1] = 0;
				m_mediaHandles[2] = 0;
				m_UserName = "N/A";
				m_RoomName = "Default";
			}

			virtual ~CCryOdinUser() = default;


			virtual string GetUserName() const override { return m_UserName; }
			virtual uint64_t GetUserId() const override { return m_UserID; }
			virtual uint64_t GetPeerId() const override { return m_PeerID; }
			virtual const char* RoomName() const override { return m_RoomName.c_str(); }
			virtual OdinMediaStreamHandle GetMediaHandle(EAudioHandleType type) const override;
			virtual OdinRoomHandle GetRoomHandle() const override { return m_room; }
			virtual string GetStatus() const override { return m_OnlineStatus; }

			virtual string ToStringDebug() const override;

			void SetPeerID(uint64_t id) { m_PeerID = id; }
			void SetUserID(uint64_t id) { m_UserID = id; }

			void SetRoomName(const char* name) { m_RoomName = name; }
			void SetOnlineStatus(string online) { m_OnlineStatus = online; }

			virtual void SetMediaHandle(EAudioHandleType type, OdinMediaStreamHandle handle) override;
			virtual void SetRoomHandle(OdinRoomHandle room) override { m_room = room; }
		private:
			mutable OdinMediaStreamHandle m_mediaHandles[3]; // 1 is input , 2 is output
			mutable OdinRoomHandle m_room;

			mutable string m_UserName;
			mutable uint64_t m_PeerID;
			mutable uint64_t m_UserID;

			CryFixedStringT<128> m_RoomName;
			string m_debugString;
			mutable string m_OnlineStatus;
		};
	}
}