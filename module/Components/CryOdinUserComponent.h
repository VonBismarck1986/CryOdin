#pragma once

#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinUserComponent : public ICryOdinUserComponent
		{
		public:
			CCryOdinUserComponent() = delete;
			explicit CCryOdinUserComponent(ICryOdinUser* ref)
				: m_pOdinUser(ref)
			{
				ODIN_LOG("Odin User copied");
			}

			virtual ~CCryOdinUserComponent() = default;

			virtual void Initialize() override {}


			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinUserComponent>& desc)
			{
				desc.SetGUID("{071AE72F-0316-478A-869A-DE6BDA1AAF8D}"_cry_guid);
				desc.SetLabel("OdinUserComponent");
				desc.SetEditorCategory("Odin");

			}

			virtual ICryOdinUser* GetOdinUser() override { return m_pOdinUser; }
		private:
			ICryOdinUser* m_pOdinUser = nullptr;
		};
	}
}