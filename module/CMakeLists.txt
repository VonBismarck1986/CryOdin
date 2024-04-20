option(PLUGIN_Odin "Enable the Odin VOIP plugin" ON)
if (NOT PLUGIN_Odin)
	return()
endif()

set(ODIN_SDK "${SDK_DIR}/Odin" CACHE PATH "Odin sdk path")

set(ODIN_ACCESS_KEY "" CACHE STRING "Access key from Odin website for testing not needed if you don't have one CryOdin will generate a temp one for you if that's the case")

if(NOT EXISTS ${ODIN_SDK})
	message("Odin sdk path not found at: ${ODIN_SDK}")
	return()
endif()


start_sources()
sources_platform(ALL)
add_sources("CryOdin.cpp"
	SOURCE_GROUP "Root"
	"Plugin.cpp"
	"Plugin.h"
	"StdAfx.h"
)

add_sources("CryOdin_Interface.cpp"
	SOURCE_GROUP "Interface"
	"../interface/ICryOdin.h"
	"../interface/ICryOdinAudioDevices.h"
	"../interface/ICryOdinUser.h"
	"../interface/ICryOdinAudioHandle.h"
	"../interface/ICryOdinAudioSystem.h"
	"../interface/ICryOdinAudioSound.h"
)

add_sources("CryOdin_Core.cpp"
	SOURCE_GROUP "Core"
	"Core/CryOdin.cpp"
	"Core/CryOdin.h"
	"Core/CryOdinAudioDevices.cpp"
	"Core/CryOdinAudioDevices.h"
	"Core/CryOdinAudioListener.cpp"
	"Core/CryOdinAudioListener.h"
	"Core/CryOdinUser.cpp"
	"Core/CryOdinUser.h"
	"Core/CryOdinAudioSound.cpp"
	"Core/CryOdinAudioSound.h"
	"Core/CryOdinAudioDataSource.cpp"
	"Core/CryOdinAudioDataSource.h"
	"Core/CryOdinAudioSystem.cpp"
	"Core/CryOdinAudioSystem.h"
	"Core/CryOdinAudioOcclusion.cpp"
	"Core/CryOdinAudioOcclusion.h"
)

add_sources("CryOdin_Utils.cpp"
	SOURCE_GROUP "Utils"
	"Utils/MiniAudioHelpers.h"
	"Utils/OdinHelpers.h"
)

add_sources("NoUberFile"
	SOURCE_GROUP "Root"
		"StdAfx.cpp"
		"resource.h"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CVarOverrides.h")
    add_sources("NoUberFile"
        PROJECTS Game
        SOURCE_GROUP "Root"
            "CVarOverrides.h"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/EngineDefineOverrides.h")
    add_sources("NoUberFile"
        PROJECTS Game
        SOURCE_GROUP "Root"
            "EngineDefineOverrides.h"
    )
endif()
end_sources()


CryEngineModule(CryOdin PCH "StdAfx.cpp" SOLUTION_FOLDER "CryPlugins")

target_include_directories(${THIS_PROJECT}
 PRIVATE
  "${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
  "${CRYENGINE_DIR}/Code/CryEngine/CryAction"
  "${CRYENGINE_DIR}/Code/CryEngine/CrySchematyc/Core/Interface"
  "${CRYENGINE_DIR}/Code/CryPlugins/CryDefaultEntities/Module"
  "${ODIN_SDK}/include"
  "../include"
 PUBLIC
  ../interface
)

unset(${OdinLib} CACHE)
find_library(OdinLib NAMES odin PATHS "${ODIN_SDK}/bin/windows-x86_64")
target_link_libraries(${THIS_PROJECT} PRIVATE "${OdinLib}")

#Enable cpp 17 support
set_target_properties(${THIS_PROJECT} PROPERTIES CXX_STANDARD 17)
#Enable edit and continue support
target_compile_options( ${THIS_PROJECT} PRIVATE /ZI )

target_compile_definitions(${THIS_PROJECT} PRIVATE ODIN_ACCESS_KEY=${ODIN_ACCESS_KEY}) 

deploy_runtime_files(${ODIN_SDK}/bin/windows-x86_64/odin.dll)