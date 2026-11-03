#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

namespace Cry
{
	namespace Odin
	{
		//! Makes voices be heard from this entity. Add it to the local player.
		//! Without this component the listener follows the view camera.
		class CCryOdinComponentListener : public IEntityComponent
		{
		public:
			CCryOdinComponentListener() = default;
			virtual ~CCryOdinComponentListener() = default;

		protected:
			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinComponentListener>& desc)
			{
				desc.SetGUID("{96FD2723-9061-41BC-841D-539DE89224FD}"_cry_guid);
				desc.SetLabel("OdinComponentListener");
				desc.SetEditorCategory("Odin");
				desc.SetDescription("Voices are heard from this entity");
				desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::Singleton });
				desc.AddMember(&CCryOdinComponentListener::m_bActive, 'actv', "Active", "Active", "Moves the voice listener to this entity", true);
				desc.AddMember(&CCryOdinComponentListener::m_bUseViewCamera, 'cam', "UseViewCamera", "Use View Camera", "Listens from the camera instead of this entity, but still ignores it in occlusion ray casts", false);
				desc.AddMember(&CCryOdinComponentListener::m_bLocalPlayerOnly, 'locl', "LocalPlayerOnly", "Local Player Only", "Only active on the entity of the local player, which your game has to spawn as one. Turn off for an entity placed in the level", true);
			}

			virtual void SetActive(bool bActive) { m_bActive = bActive; }
			virtual bool IsActive() const { return m_bActive; }

			virtual void SetUseViewCamera(bool bUseViewCamera) { m_bUseViewCamera = bUseViewCamera; }
			virtual bool IsUsingViewCamera() const { return m_bUseViewCamera; }

		protected:
			bool m_bActive = true;
			bool m_bUseViewCamera = false;
			bool m_bLocalPlayerOnly = true;
		};
	}
}
