#include "StdAfx.h"
#include "OdinComponents/CryOdinComponentListener.h"

#include "Core/CryOdin.h"
#include "Core/CryOdinAudioSystem.h"

#include <CryEntitySystem/IEntity.h>
#include <Cry3DEngine/I3DEngine.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			static void RegisterOdinListenerComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinComponentListener));
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentListener::SetActive, "{CB4D471E-A26F-4C00-8DED-3CFE9FF20023}"_cry_guid, "SetActive");
						pFunction->SetDescription("Turns this entity being the voice listener on or off");
						pFunction->BindInput(1, 'actv', "Active");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentListener::SetUseViewCamera, "{6CF6081D-42DA-4F89-B419-8FE5469C1506}"_cry_guid, "SetUseViewCamera");
						pFunction->SetDescription("Listens from the camera instead of this entity");
						pFunction->BindInput(1, 'cam', "Use View Camera");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentListener::SetHrtfEnabled, "{7F274418-122E-4228-97A4-740EC60A9ED3}"_cry_guid, "SetHrtfEnabled");
						pFunction->SetDescription("On places 3D voices around the head, for headphones. Off uses stereo panning, for speakers. For all voices");
						pFunction->BindInput(1, 'hrtf', "Enabled");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentListener::IsHrtfEnabled, "{6A864894-A573-4D73-A482-D19C9DF927A8}"_cry_guid, "IsHrtfEnabled");
						pFunction->SetDescription("True while voices use HRTF");
						pFunction->BindOutput(0, 'hrtf', "Enabled");
						componentScope.Register(pFunction);
					}
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinListenerComponent);
		}

		void CCryOdinComponentListener::Initialize()
		{
		}

		void CCryOdinComponentListener::SetHrtfEnabled(bool bEnabled)
		{
			// Kept until voices play, without starting Odin for it.
			CCryOdinAudioSystem::SetHrtfSetting(bEnabled);
		}

		bool CCryOdinComponentListener::IsHrtfEnabled() const
		{
			return CCryOdinAudioSystem::GetHrtfSetting();
		}

		Cry::Entity::EventFlags CCryOdinComponentListener::GetEventMask() const
		{
			return Cry::Entity::EEvent::Update;
		}

		void CCryOdinComponentListener::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event != Cry::Entity::EEvent::Update || !m_bActive || gEnv->IsEditing())
			{
				return;
			}

			if (m_bLocalPlayerOnly && (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) == 0)
			{
				return;
			}

			CCryOdin* pOdin = CCryOdin::Get();
			CCryOdinAudioSystem* pAudioSystem = pOdin != nullptr ? pOdin->PeekAudioSystem() : nullptr;
			if (pAudioSystem == nullptr)
			{
				return;
			}

			Vec3 position;
			Vec3 forward;
			Vec3 up;
			if (m_bUseViewCamera)
			{
				const CCamera& camera = gEnv->pSystem->GetViewCamera();
				const Matrix34& matrix = camera.GetMatrix();
				position = camera.GetPosition();
				forward = matrix.GetColumn1();
				up = matrix.GetColumn2();
			}
			else
			{
				const Matrix34& matrix = m_pEntity->GetWorldTM();
				position = matrix.GetTranslation() + Vec3(0.f, 0.f, m_fEarHeight);
				forward = matrix.GetColumn1();
				up = matrix.GetColumn2();
			}

			pAudioSystem->SetListenerTransform(position, forward, up, m_pEntity->GetId());
		}
	}
}
