#include "StdAfx.h"
#include "OdinComponents/CryOdinReverbComponent.h"

#include "Core/CryOdinAudioReverb.h"

#include <CryEntitySystem/IEntity.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			static void RegisterOdinReverbComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinReverbComponent));
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinReverbComponent::SetActive, "{D6BB26DE-A821-4DED-9DE0-688482F195F2}"_cry_guid, "SetActive");
						pFunction->SetDescription("Turns the reverb of this space on or off");
						pFunction->BindInput(1, 'actv', "Active");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinReverbComponent::SetReverbPreset, "{8C23374D-324E-4E84-95F6-EBC237752F9C}"_cry_guid, "SetReverbPreset");
						pFunction->SetDescription("Changes how this space sounds. Custom uses the values in the Reverb property");
						pFunction->BindInput(1, 'pres', "Preset");
						componentScope.Register(pFunction);
					}
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinReverbComponent);
		}

		CCryOdinReverbComponent::~CCryOdinReverbComponent()
		{
			if (m_bRegistered)
			{
				CCryOdinReverbManager::UnregisterComponent(this);
				m_bRegistered = false;
			}
		}

		void CCryOdinReverbComponent::Initialize()
		{
			if (!m_bRegistered)
			{
				CCryOdinReverbManager::RegisterComponent(this);
				m_bRegistered = true;
			}
		}

		void CCryOdinReverbComponent::OnShutDown()
		{
			if (m_bRegistered)
			{
				CCryOdinReverbManager::UnregisterComponent(this);
				m_bRegistered = false;
			}
		}

		float CCryOdinReverbComponent::GetBoxAmount(const Vec3& worldPosition) const
		{
			if (m_pEntity->IsHidden())
			{
				return 0.f;
			}

			// In the entity's own space, so the box turns with it.
			const Vec3 local = m_pEntity->GetWorldTM().GetInverted().TransformPoint(worldPosition);
			const Vec3 outside(
				std::max(fabsf(local.x) - m_boxSize.x * 0.5f, 0.f),
				std::max(fabsf(local.y) - m_boxSize.y * 0.5f, 0.f),
				std::max(fabsf(local.z) - m_boxSize.z * 0.5f, 0.f));
			const float fDistance = outside.GetLength();
			if (fDistance <= 0.f)
			{
				return 1.f;
			}
			if (m_fFadeDistance <= 0.f)
			{
				return 0.f;
			}
			return std::max(0.f, 1.f - fDistance / m_fFadeDistance);
		}
	}
}
