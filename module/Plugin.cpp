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
		m_pOdin->OnUpdate(frameTime);
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
			
			IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

			IActionMap* pActionMap = pActionMapManager->CreateActionMap(m_szMyActionMapName);
			pActionMapManager->AddExtraActionListener(this, m_szMyActionMapName);

			// Register the action in the group
			pActionMap->CreateAction(m_myActionId);

			SActionInput input;
			input.inputDevice = eAID_KeyboardMouse;
			input.input = input.defaultInput = "enter";
			input.activationMode = eIS_Pressed | eIS_Released;

			pActionMap->AddAndBindActionInput(m_myActionId, input);
			pActionMap->Enable(true);
		}
		break;
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			gEnv->pGameFramework->GetIActionMapManager()->RemoveExtraActionListener(this, m_szMyActionMapName);
			
			if (m_pOdin)
			{
				m_pOdin->Shutdown();
			
				m_pOdin.release();
			}
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