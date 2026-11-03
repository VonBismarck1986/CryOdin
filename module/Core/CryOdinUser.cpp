#include "StdAfx.h"
#include "CryOdinUser.h"

#include "CryOdinRoom.h"
#include "CryOdinAudioSound.h"

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr float kTalkHoldSeconds = 0.25f;
		}

		CCryOdinUser::CCryOdinUser(CCryOdinRoom& room, uint32 peerId, bool bLocal, const char* szUserId)
			: m_room(room)
			, m_peerId(peerId)
			, m_bLocal(bLocal)
			, m_userId(szUserId)
		{
		}

		CCryOdinUser::~CCryOdinUser()
		{
			ClearVoice();
		}

		ICryOdinRoom& CCryOdinUser::GetRoom() const
		{
			return m_room;
		}

		ICryOdinSound* CCryOdinUser::GetSound() const
		{
			return m_pSound.get();
		}

		string CCryOdinUser::ToStringDebug() const
		{
			string debug;
			debug.Format("%s (peer %u, user '%s')%s%s", GetName(), m_peerId, m_userId.c_str(), m_bLocal ? " [local]" : "", m_bTalking ? " [talking]" : "");
			return debug;
		}

		void CCryOdinUser::SetVoice(std::shared_ptr<SCryOdinDecoder> pDecoder, std::unique_ptr<CCryOdinSound> pSound)
		{
			m_pSound = std::move(pSound);
			m_pDecoder = std::move(pDecoder);
		}

		void CCryOdinUser::ClearVoice()
		{
			// The sound reads the decoder, so it has to go first.
			m_pSound.reset();
			m_pDecoder.reset();
		}

		void CCryOdinUser::MarkRetired()
		{
			m_bRetired = true;
			if (m_pSound)
			{
				m_pSound->StopSound();
			}
		}

		bool CCryOdinUser::UpdateTalking(bool bSignalTalking, float frameTime)
		{
			if (bSignalTalking)
			{
				m_fTalkHoldTime = kTalkHoldSeconds;
			}
			else
			{
				m_fTalkHoldTime = std::max(m_fTalkHoldTime - frameTime, 0.f);
			}

			const bool bTalking = m_fTalkHoldTime > 0.f;
			if (bTalking != m_bTalking)
			{
				m_bTalking = bTalking;
				return true;
			}
			return false;
		}
	}
}
