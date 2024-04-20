#include "StdAfx.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"

namespace Cry
{
	namespace Odin
	{
	
		ma_sound* CCryOdinAudioSound::GetSound()
		{
			return &m_sound;
		}

		void CCryOdinAudioSound::SetSoundVolume(float fAmount)
		{

		}

		float CCryOdinAudioSound::GetSoundVolume() const
		{
			return ma_sound_get_volume(&m_sound);
		}

		void CCryOdinAudioSound::OnUpdate(float const fFrameTime)
		{
			ODIN_LOG("Updating sound");
		}

		Vec3 CCryOdinAudioSound::GetSoundPosition() const
		{
			return m_transform.GetPosition();
		}

		void CCryOdinAudioSound::InitSound(ma_engine* engine, OdinDataSource* pDataSource)
		{
			ma_sound_init_from_data_source(engine, pDataSource, 0, NULL, &m_sound);
			ma_sound_set_looping(&m_sound, true);
		}

		void CCryOdinAudioSound::PlaySound()
		{
			ma_sound_start(&m_sound);
		}
		void CCryOdinAudioSound::StopSound()
		{
			ma_sound_stop(&m_sound);
		}
	}
}