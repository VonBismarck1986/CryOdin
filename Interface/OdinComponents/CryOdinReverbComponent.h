#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <ICryOdin.h>

namespace Cry
{
	namespace Odin
	{
		//! Where the space of a CCryOdinReverbComponent is.
		enum class ECryOdinReverbShape
		{
			EngineEnvironment = 0, //!< The area and fade distance of the CRYENGINE Environment on this entity.
			Box                    //!< A box of its own, centered on the entity, for levels without audio environments.
		};

		inline void ReflectType(Schematyc::CTypeDesc<ECryOdinReverbShape>& desc)
		{
			desc.SetGUID("{78C006AA-08CF-4E7B-8E7E-DAF65052C341}"_cry_guid);
			desc.SetLabel("Odin Reverb Shape");
			desc.SetDefaultValue(ECryOdinReverbShape::EngineEnvironment);
			desc.AddConstant(ECryOdinReverbShape::EngineEnvironment, "EngineEnvironment", "Engine Audio Environment");
			desc.AddConstant(ECryOdinReverbShape::Box, "Box", "Box");
		}

		//! How Odin voices sound in a space. Put it on the entity of a CRYENGINE audio area, next to its
		//! Environment component, to give voices in that environment this reverb. Or use its own box.
		// Not final. Calls on a final class skip the vtable, and a game DLL couldn't link them.
		class CCryOdinReverbComponent : public IEntityComponent
		{
		public:
			CCryOdinReverbComponent() = default;
			virtual ~CCryOdinReverbComponent();

		protected:
			virtual void Initialize() override;
			virtual void OnShutDown() override;

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinReverbComponent>& desc)
			{
				desc.SetGUID("{99B71FC9-1644-44A3-9D58-0839605CD7B6}"_cry_guid);
				desc.SetLabel("OdinReverbComponent");
				desc.SetEditorCategory("Odin");
				desc.SetDescription("Reverb for Odin voices in an audio environment or a box");
				desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::Singleton });
				desc.AddMember(&CCryOdinReverbComponent::m_bActive, 'actv', "Active", "Active", "Voices in this space get the reverb", true);
				desc.AddMember(&CCryOdinReverbComponent::m_shape, 'shap', "Shape", "Shape", "Engine Audio Environment follows the CRYENGINE Environment component on this entity and its area. Box uses the size below", ECryOdinReverbShape::EngineEnvironment);
				desc.AddMember(&CCryOdinReverbComponent::m_reverb, 'revb', "Reverb", "Reverb", "How the space sounds", SCryOdinReverbSettings());
				desc.AddMember(&CCryOdinReverbComponent::m_boxSize, 'bsiz', "BoxSize", "Box Size", "Size of the box in meters, centered on the entity. Used with the Box shape", Vec3(10.f, 10.f, 4.f));
				desc.AddMember(&CCryOdinReverbComponent::m_fFadeDistance, 'fade', "FadeDistance", "Fade Distance", "Outside the box, the reverb fades out over this distance. Used with the Box shape", 2.f);
			}

			virtual void SetActive(bool bActive) { m_bActive = bActive; }
			virtual bool IsActive() const { return m_bActive; }

			virtual void SetReverbSettings(const SCryOdinReverbSettings& settings) { m_reverb = settings; }
			virtual const SCryOdinReverbSettings& GetReverbSettings() const { return m_reverb; }
			virtual void SetReverbPreset(ECryOdinReverbPreset preset) { m_reverb.preset = preset; }

			virtual ECryOdinReverbShape GetShape() const { return m_shape; }

			// Used by the plugin: how much of the box applies at a world position, 0 to 1.
			float GetBoxAmount(const Vec3& worldPosition) const;

		protected:
			bool m_bActive = true;
			ECryOdinReverbShape m_shape = ECryOdinReverbShape::EngineEnvironment;
			SCryOdinReverbSettings m_reverb;
			Vec3 m_boxSize = Vec3(10.f, 10.f, 4.f);
			float m_fFadeDistance = 2.f;
			bool m_bRegistered = false;
		};
	}
}
