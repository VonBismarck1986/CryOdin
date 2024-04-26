#pragma once

#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinUserComponent : public IEntityComponent
		{
		public:
			CCryOdinUserComponent() = default;
			virtual ~CCryOdinUserComponent() = default;

			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;

			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinUserComponent>& desc)
			{
				desc.SetGUID("{16EB0B3D-747B-479A-BFF1-581881DFA9FC}"_cry_guid);
				desc.SetLabel("OdinUserComponent");
				desc.SetEditorCategory("Odin");
			}

			ICryOdinUser* GetOdinUser() const { return m_pOdinUser; }
			void SetOdinUser(ICryOdinUser* ref) { m_pOdinUser = ref; }

		
		private:
			mutable ICryOdinUser* m_pOdinUser = nullptr;
		};
	}
}