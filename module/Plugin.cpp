#include "StdAfx.h"
#include "Plugin.h"

#pragma warning( push )
#pragma warning( disable : 4244 4267)
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#pragma warning( pop )

#include "Core/CryOdin.h"

#include <CryCore/Platform/platform_impl.inl>
#include "CrySchematyc/Env/EnvPackage.h"
#include "CrySchematyc/ICore.h"
#include "CrySchematyc/Env/IEnvRegistry.h"

Cry::Odin::CCryOdinPlugin::~CCryOdinPlugin()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (gEnv->pSchematyc)
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(Cry::Odin::CCryOdinPlugin::GetCID());
	}

	m_pOdin.reset();
}

bool Cry::Odin::CCryOdinPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "Odin_Plugin");

	// Odin and the audio devices only start when they're first needed.
	m_pOdin = stl::make_unique<CCryOdin>();

	// The engine doesn't load dedicated.cfg by itself, only some games do.
	if (gEnv->IsDedicated())
	{
		m_pOdin->LoadServerConfig();
	}

	EnableUpdate(IEnginePlugin::EUpdateStep::MainUpdate, true);

	return true;
}

Cry::Odin::ICryOdin* Cry::Odin::CCryOdinPlugin::GetOdin() const
{
	return m_pOdin.get();
}

void Cry::Odin::CCryOdinPlugin::MainUpdate(float frameTime)
{
	if (m_pOdin)
	{
		// Real time, as voice keeps going when the game is paused. Limited, so that one long
		// stall such as a level load doesn't look like a timeout.
		m_pOdin->OnUpdate(std::min(gEnv->pTimer->GetRealFrameTime(), 0.25f));
	}
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
						"EntityComponents",
						"Bismarck",
						"Components",
						staticAutoRegisterLambda
					)
				);
			}
		}
		break;
		case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		{
			if (wparm == 0 && m_pOdin)
			{
				m_pOdin->OnEditorGameModeEnded();
			}
		}
		break;
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			// The plugin's destructor doesn't always run, as the process can end right after a fast shutdown.
			// The object stays, because entity components are destroyed after this and talk to it.
			if (m_pOdin)
			{
				// CryAction, which holds the push to talk actions, shuts down right after this event.
				m_pOdin->ShutdownInput();
				m_pOdin->Shutdown();
			}
		}
		break;
	}
}

CRYREGISTER_SINGLETON_CLASS(Cry::Odin::CCryOdinPlugin)
