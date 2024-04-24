#include "StdAfx.h"
#include "CryOdinUserComponent.h"

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			static void RegisterOdinUserComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinUserComponent));
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinUserComponent);
		}

		void CCryOdinUserComponent::Initialize()
		{
			ODIN_LOG("Odin Component Created");
		}

		Cry::Entity::EventFlags CCryOdinUserComponent::GetEventMask() const
		{
			return Cry::Entity::EventFlags();
		}

		void CCryOdinUserComponent::ProcessEvent(const SEntityEvent& event)
		{
		}

	}
}