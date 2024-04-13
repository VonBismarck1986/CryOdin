#include "StdAfx.h"
#include "Plugin.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "Core/CryOdin.h"
#include "Core/CryOdinAudioDevices.h"
#include "Core/CryOdinUser.h"

#include <CryCore/Platform/platform_impl.inl>
//#include "Core/Implementation.h"
#include "CrySchematyc/Env/EnvPackage.h"
#include "CrySchematyc/ICore.h"
#include "CrySchematyc/Env/IEnvRegistry.h"

#include "IGameObject.h"
#include <CryGame/IGameFramework.h>


Cry::Odin::CCryOdinPlugin::~CCryOdinPlugin()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (gEnv->pSchematyc)
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(Cry::Odin::CCryOdinPlugin::GetCID());
	}

	if (m_pOdin)
	{
		m_pOdin->Shutdown();

		m_pOdin.release();
	}
}

bool Cry::Odin::CCryOdinPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "Odin_Plugin");

	m_pOdin.reset(new CCryOdin());
	
	return true;
}

void Cry::Odin::CCryOdinPlugin::MainUpdate(float frameTime)
{
	if (m_pOdin)
	{
		m_pOdin->OnUpdate();
	}
}

void Cry::Odin::CCryOdinPlugin::UpdateBeforeRender()
{
}

void Cry::Odin::CCryOdinPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparm, UINT_PTR lparam)
{
	switch (event)
	{
		case ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV:
		{
			// Register all components that belong to this plug-in
			auto staticAutoRegisterLambda = [](Schematyc::IEnvRegistrar& registrar)
				{
					// Call all static callback registered with the CRY_STATIC_AUTO_REGISTER_WITH_PARAM
					Detail::CStaticAutoRegistrar<Schematyc::IEnvRegistrar&>::InvokeStaticCallbacks(registrar);
				};

			if (gEnv->pSchematyc)
			{
				gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(
					stl::make_unique<Schematyc::CEnvPackage>(
						CCryOdinPlugin::GetCID(),
						"CryOdin",
						"",
						"Audio",
						staticAutoRegisterLambda
					)
				);
			}
		}
		break;
		case ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE:
		{

			EnableUpdate(IEnginePlugin::EUpdateStep::BeforeRender, true);
			EnableUpdate(IEnginePlugin::EUpdateStep::MainUpdate, true);

			OdinApmConfig conifg{};
			
			IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

			// Create the action map in which our action will reside
			IActionMap* pActionMap = pActionMapManager->CreateActionMap(m_szMyActionMapName);
			// Register a listener to receive callbacks when actions in our action map are triggered
			pActionMapManager->AddExtraActionListener(this, m_szMyActionMapName);

			// Register the action in the group
			pActionMap->CreateAction(m_myActionId);

			// Now define the first input with which the user can trigger the input
			SActionInput input;
			// Define that this input is triggered with the keyboard or mouse
			input.inputDevice = eAID_KeyboardMouse;
			// Set the input to "enter"
			// defaultInput is used in case of future rebinding by the end-user at runtime
			input.input = input.defaultInput = "enter";
			// Determine the activation modes we want to listen for
			input.activationMode = eIS_Pressed | eIS_Released;

			// Now bind the input to the action we created earlier
			pActionMap->AddAndBindActionInput(m_myActionId, input);

			// Make sure that the action map is enabled by default
			// This function can also be used to toggle action maps at runtime, for example to disable vehicle inputs when exiting
			pActionMap->Enable(true);
		}
		break;
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			gEnv->pGameFramework->GetIActionMapManager()->RemoveExtraActionListener(this, m_szMyActionMapName);
			//CCryOdin::GetOdin()->Shutdown();
		}
		break;
	}

}

void Cry::Odin::CCryOdinPlugin::OnAction(const ActionId& action, int activationMode, float value)
{
	const bool isInputPressed = (activationMode & eIS_Pressed) != 0;
	const bool isInputReleased = (activationMode & eIS_Released) != 0;

	// Check if the triggered action
	if (action == m_myActionId)
	{
		if (isInputPressed)
		{
			CryLogAlways("Action pressed!");
			//m_pTemp->ToggleEcho();
			
		}
		else if (isInputReleased)
		{
			CryLogAlways("Action released!");
		}
	}
}

CRYREGISTER_SINGLETON_CLASS(Cry::Odin::CCryOdinPlugin)