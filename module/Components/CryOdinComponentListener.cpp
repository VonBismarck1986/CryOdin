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
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinListenerComponent);
		}

		void CCryOdinComponentListener::Initialize()
		{
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
				position = matrix.GetTranslation();
				forward = matrix.GetColumn1();
				up = matrix.GetColumn2();
			}

			pAudioSystem->SetListenerTransform(position, forward, up, m_pEntity->GetId());
		}
	}
}
