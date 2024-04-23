#include "StdAfx.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
	namespace Odin
	{
		ma_sound CCryOdinSound::CCryOdinSound::m_sound;

		CCryOdinSound::CCryOdinSound(ma_engine* engine, OdinDataSource* datasource, const CryAudio::CTransformation& transform)
			: m_transform(transform)
		{
			if (m_pEntity)
			{
				m_transform = CryAudio::CTransformation(m_pEntity->GetWorldTM());
			}

			ma_sound_init_from_data_source(engine, datasource, 0, NULL, &m_sound);
			ma_sound_set_position(&m_sound, m_transform.GetPosition().x, m_transform.GetPosition().y, m_transform.GetPosition().z);
			ma_sound_set_direction(&m_sound, m_transform.GetForward().x, m_transform.GetForward().y, m_transform.GetForward().z);
			ma_sound_set_looping(&m_sound, MA_TRUE);
			ma_sound_set_volume(&m_sound, ma_volume_db_to_linear(10.f));
		}

		void CCryOdinSound::StartSound()
		{
			ma_sound_start(&m_sound);
		}

		void CCryOdinSound::StopSound()
		{
			ma_sound_stop(&m_sound);
		}

		void CCryOdinSound::SetSoundVolume(float fAmount)
		{
			ma_sound_set_volume(&m_sound, fAmount);
		}

		float CCryOdinSound::GetSoundVolume() const
		{
			m_volume = ma_sound_get_volume(&m_sound);
			return m_volume;
		}

		void CCryOdinSound::OnUpdate(float const frameTime)
		{
			if (m_pEntity)
			{
				m_transform = CryAudio::CTransformation(m_pEntity->GetWorldTM());

				ma_sound_set_position(&m_sound, m_transform.GetPosition().x, m_transform.GetPosition().y, m_transform.GetPosition().z);
				ma_sound_set_direction(&m_sound, m_transform.GetForward().x, m_transform.GetForward().y, m_transform.GetForward().z);

				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_transform.GetPosition(), 1.2f, Col_Blue);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_transform.GetForward(), Col_Red, m_transform.GetForward() * 2, Col_Red, 2.5f);
			}
		}

		void CCryOdinSound::Destory()
		{
			ma_sound_uninit(&m_sound);
		}



	}
}