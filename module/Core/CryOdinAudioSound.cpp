#include "StdAfx.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
	namespace Odin
	{
		ma_sound m_sound;

		ma_sound* CCryOdinAudioSound::GetSound()
		{
			return &m_sound;
		}

		void CCryOdinAudioSound::SetSoundVolume(float fAmount)
		{
			ma_sound_set_volume(&m_sound, ma_volume_db_to_linear(fAmount));
		}

		float CCryOdinAudioSound::GetSoundVolume() const
		{
			return ma_sound_get_volume(&m_sound);
		}

		void CCryOdinAudioSound::OnUpdate(float const fFrameTime)
		{
			/* Might be wondering why updating per-frame , since this locked to person and said can move at anytime best to keep "synced" */
			m_transform = m_user.m_pEntity->GetWorldTM();

			ma_sound_set_position(&m_sound, m_transform.GetPosition().x, m_transform.GetPosition().y, m_transform.GetPosition().z);
			ma_sound_set_direction(&m_sound, m_transform.GetForward().x, m_transform.GetForward().y, m_transform.GetForward().z);

			DebugDraw(fFrameTime);
		}

		Vec3 CCryOdinAudioSound::GetSoundPosition() const
		{
			return m_transform.GetPosition();
		}

		void CCryOdinAudioSound::InitSound(ma_engine* engine, OdinDataSource* pDataSource)
		{
			ma_sound_init_from_data_source(engine, pDataSource, 0, NULL, &m_sound);

			ma_sound_set_looping(&m_sound, 1);
			ma_sound_set_attenuation_model(&m_sound, ma_attenuation_model_linear);
			ma_sound_set_directional_attenuation_factor(&m_sound, 2.5f);
			ma_sound_set_cone(&m_sound, 0.5f, 1.5f, 0.95f);

			ma_sound_set_position(&m_sound, m_transform.GetPosition().x, m_transform.GetPosition().y, m_transform.GetPosition().z);
			ma_sound_set_direction(&m_sound, m_transform.GetForward().x, m_transform.GetForward().y, m_transform.GetForward().z);

			ODIN_LOG("Sound Finished");
		}

		void CCryOdinAudioSound::PlaySound()
		{
			ma_sound_start(&m_sound);
		}
		void CCryOdinAudioSound::StopSound()
		{
			ma_sound_stop(&m_sound);
		}

		void CCryOdinAudioSound::DebugDraw(float const frameTime)
		{
			// Position
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_transform.GetPosition(), 1.f, Col_Blue);

			// Direction
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_transform.GetPosition(), Col_BlueViolet, m_transform.GetForward() * 1.2f, Col_BlueViolet,1.5f);
		}
	}
}