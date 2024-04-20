#include "StdAfx.h"
#include "CryOdinAudioSound.h"
#include "CryOdinAudioDataSource.h"

#include <CryRenderer/IRenderAuxGeom.h>

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
			ma_sound_set_looping(&m_sound, true);

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
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_transform.GetPosition(),1.f,Col_Blue);
		}
	}
}