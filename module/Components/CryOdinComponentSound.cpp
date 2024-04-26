#include "StdAfx.h"
#include "OdinComponents/CryOdinComponentSound.h"

#include <Core/CryOdinAudioSystem.h>
#include <CryPhysics/IPhysics.h>

namespace Cry
{
	namespace Odin
	{
		constexpr float FloatEpsilon = 1.0e-3f;

		namespace
		{
			static void RegisterOdinSoundComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinComponentSound));
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinSoundComponent);
		}

		void Cry::Odin::CCryOdinComponentSound::Initialize()
		{
			m_transform = m_pEntity->GetWorldTM();
		}

		Cry::Entity::EventFlags CCryOdinComponentSound::GetEventMask() const
		{
			return EEntityEvent::Update | EEntityEvent::Reset;
		}

		void CCryOdinComponentSound::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
			case EEntityEvent::Update:
			{
				const float frameTime = event.fParam[0];

				if (m_bGameStarted)
				{
					UpdateSoundPosition(m_pEntity->GetWorldTM());
					UpdateSoundVelocity(frameTime);
				}
		
			}
			break;
			case EEntityEvent::Reset:
			{
				m_bGameStarted = event.nParam[0] != 0;
			}
			break;
			}
		}

		void CCryOdinComponentSound::MutePlayer()
		{
			ma_sound_set_volume(&m_sound, 0.0f);
		}

		void CCryOdinComponentSound::SetVolume(float fAmount)
		{
			ma_sound_set_volume(&m_sound, ma_volume_linear_to_db(fAmount));
			m_fVolume = ma_sound_get_volume(&m_sound);
		}

		void CCryOdinComponentSound::CreateSoundInstance(ma_engine* engine, OdinDataSource* dataSource, const CryAudio::CTransformation& transform, uint16_t id)
		{
			m_transform = transform;
			m_position = transform.GetPosition();

			m_pDataSource = dataSource;

			ma_sound_init_from_data_source(engine, dataSource, 0, NULL, &m_sound);
			ma_sound_set_position(&m_sound, m_transform.GetPosition().x, m_transform.GetPosition().y, m_transform.GetPosition().z);
			ma_sound_set_direction(&m_sound, m_transform.GetForward().x, m_transform.GetForward().y, m_transform.GetForward().z);
			ma_sound_set_looping(&m_sound, MA_TRUE);
			ma_sound_set_volume(&m_sound, ma_volume_db_to_linear(10.f));

			m_bGameStarted = true;
		}

		void CCryOdinComponentSound::UpdateSoundPosition(const CryAudio::CTransformation& transform)
		{

			m_position = transform.GetPosition();

			if (((m_flags & EObjectFlags::TrackAbsoluteVelocity) != EObjectFlags::None) || ((m_flags & EObjectFlags::TrackVelocityForDoppler) != EObjectFlags::None))
			{
				m_flags |= EObjectFlags::MovingOrDecaying;
			}
			else
			{
				m_previousPosition = m_position;
			}

			ma_sound_set_position(&m_sound, transform.GetPosition().x, transform.GetPosition().y, transform.GetPosition().z);
			ma_sound_set_direction(&m_sound, transform.GetForward().x, transform.GetForward().y, transform.GetForward().z);
		}

		void CCryOdinComponentSound::UpdateSoundVelocity(float const fFrameTime)
		{
			Vec3 const deltaPos(m_position - m_previousPosition);

			if (!deltaPos.IsZero())
			{
				m_velocity = deltaPos / fFrameTime;
				m_previousPosition = m_position;
			}
			else if (!m_velocity.IsZero())
			{
				// We did not move last frame, begin exponential decay towards zero.
				float const decay = std::max(1.0f - fFrameTime / 0.05f, 0.0f);
				m_velocity *= decay;

				if (m_velocity.GetLengthSquared() < FloatEpsilon)
				{
					m_velocity = ZERO;
					m_flags &= ~EObjectFlags::MovingOrDecaying;
				}

				if ((m_flags & EObjectFlags::TrackVelocityForDoppler) != EObjectFlags::None)
				{
					ma_sound_set_velocity(&m_sound, m_velocity.x, m_velocity.y, m_velocity.z);
				}
			}

			if ((m_flags & EObjectFlags::TrackAbsoluteVelocity) != EObjectFlags::None)
			{
				float const absoluteVelocity = m_velocity.GetLength();

				if (absoluteVelocity == 0.0f || fabs(absoluteVelocity - m_previousAbsoluteVelocity) > 0.1f)
				{
					m_previousAbsoluteVelocity = absoluteVelocity;
					m_absoluteVelocity = absoluteVelocity;
				}
			}
		}

	}
}