#pragma once
#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_EnginePlugin
#define GAME_API   DLL_EXPORT

#include <CryCore/Platform/platform.h>
#include <CrySystem/ISystem.h>
#include <CryNetwork/ISerialize.h>

#pragma warning( push )
#pragma warning( disable : 4244)
#include <CrySystem/Profilers/ICryProfilingSystem.h>
#include <Cry3DEngine/I3DEngine.h>
#pragma  pop

#include <odin.h>

#define CHAR8_T_DEFINED 1
using char8_t = char;

#define ODIN_LOG(...) CryLogAlways("[CryOdin] " __VA_ARGS__);
#define ODIN_WARNING(...) CryLogAlways("[CryOdin] [WARNING] " __VA_ARGS__);

static void print_error(OdinReturnCode error, const char* text)
{
    char buffer[512];
    odin_error_format(error, buffer, sizeof(buffer));
    ODIN_WARNING("%s; %s\n", text, buffer);
}

#define DEFAULT_ODIN_URL "gateway.odin.4players.io"


#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Utils/EnumFlags.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/Elements/EnvFunction.h>
#include <CrySchematyc/Env/Elements/EnvSignal.h>
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CryCore/StaticInstanceList.h>