#pragma once

#include <ICryOdinUser.h>
#include "Utils/OdinHelpers.h"

#include <memory>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinRoom;
		class CCryOdinSound;
		struct SCryOdinDecoder;

		class CCryOdinUser final : public ICryOdinUser
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinUser)
			CCryOdinUser(CCryOdinRoom& room, uint32 peerId, bool bLocal, const char* szUserId);
			virtual ~CCryOdinUser();

			// ICryOdinUser
			virtual const char* GetName() const override { return m_name.empty() ? m_userId.c_str() : m_name.c_str(); }
			virtual const char* GetUserId() const override { return m_userId.c_str(); }
			virtual uint32 GetPeerId() const override { return m_peerId; }
			virtual const char* GetUserData() const override { return m_userData.c_str(); }

			virtual ICryOdinRoom& GetRoom() const override;
			virtual bool IsLocal() const override { return m_bLocal; }
			virtual bool IsTalking() const override { return m_bTalking; }

			virtual ICryOdinSound* GetSound() const override;

			virtual bool HasPasswordMismatch() const override { return m_bPasswordMismatch; }

			virtual string ToStringDebug() const override;
			// ~ICryOdinUser

			void SetName(const char* szName) { m_name = szName; }
			void SetUserId(const char* szUserId) { m_userId = szUserId; }
			void SetUserData(const char* szUserData) { m_userData = szUserData; }
			void SetPasswordMismatch(bool bMismatch) { m_bPasswordMismatch = bMismatch; }

			// Must be destroyed before the audio system
			void SetVoice(std::shared_ptr<SCryOdinDecoder> pDecoder, std::unique_ptr<CCryOdinSound> pSound);
			void ClearVoice();
			CCryOdinSound* GetVoice() const { return m_pSound.get(); }
			const std::shared_ptr<SCryOdinDecoder>& GetDecoder() const { return m_pDecoder; }

			// Applies a short hold on the end of talking, so that gaps in speech don't flicker.
			// Returns true when the talking state changed.
			bool UpdateTalking(bool bSignalTalking, float frameTime);

			// Out of the room's lists, and waiting to be deleted. Listeners must not be told it's there.
			// The voice stops but stays, as a listener can still use it until its OnPeerLeft has returned.
			void MarkRetired();
			bool IsRetired() const { return m_bRetired; }

			// OnPeerLeft only goes with an OnPeerJoined.
			void SetAnnounced() { m_bAnnounced = true; }
			bool IsAnnounced() const { return m_bAnnounced; }

		private:
			CCryOdinRoom& m_room;
			const uint32 m_peerId;
			const bool m_bLocal;

			string m_userId;
			string m_name;
			string m_userData;

			std::shared_ptr<SCryOdinDecoder> m_pDecoder;
			std::unique_ptr<CCryOdinSound> m_pSound;

			bool m_bTalking = false;
			float m_fTalkHoldTime = 0.f;
			bool m_bPasswordMismatch = false;
			bool m_bRetired = false;
			bool m_bAnnounced = false;
		};
	}
}
