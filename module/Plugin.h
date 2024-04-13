#pragma once

#include <CrySystem/ICryPlugin.h>
#include "CrySystem/ISystem.h"
#include <CryInput/IInput.h>
#include <IActionMapManager.h>

#include "ICryOdin.h"

namespace Cry
{
	namespace Odin
	{
		class CCryOdin;

		class CCryOdinPlugin final
			: public Cry::Odin::ICryOdinPlugin
			, public ISystemEventListener
			, public IActionListener
		{
		public:

			CRYINTERFACE_BEGIN()
				CRYINTERFACE_ADD(Cry::Odin::ICryOdinPlugin)
				CRYINTERFACE_ADD(Cry::IEnginePlugin)
			CRYINTERFACE_END()

			CRYGENERATE_SINGLETONCLASS_GUID(CCryOdinPlugin, "CryOdin", "{629B0B36-6293-4A38-B2CC-6EDC04B41EFA}"_cry_guid)


			PLUGIN_FLOWNODE_REGISTER
			PLUGIN_FLOWNODE_UNREGISTER;

			virtual ~CCryOdinPlugin();

			virtual const char* GetName() const override { return "CryOdin"; }

			virtual const char* GetCategory() const override { return "Audio"; }

			virtual ICryOdin* GetOdin() const { return reinterpret_cast<ICryOdin*>(m_pOdin.get()); }

			//! This is called to initialize the new plugin.
			virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

			virtual void MainUpdate(float delta) override;

			virtual void UpdateBeforeRender() override;

			virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

			virtual void OnAction(const ActionId& action, int activationMode, float value) override;

		private:
			// Define the name of the action map in which our action will reside
			const char* m_szMyActionMapName = "MyActionGroup";
			// Define the identifier of the action we are registering, this should be a constant over the runtime of the application
			const ActionId m_myActionId = ActionId("MyAction");
			
			std::unique_ptr<CCryOdin> m_pOdin = nullptr;
		};
	}
}