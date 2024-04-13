# CryOdin
This repository is first attempt at linking together Odin and CRYENGINE. Please note until other wise written 
using this Plugin for you're game wouldn't be recommend unless you know what you're doing. I will be polishing this 
Plugin to be able to used.

# Odin 
 Odin is made by `4Players` check out there website here [4Players GmbH](https://www.4players.io/) for more information on Odin and pricing.
 Right now they offer a free use for services and go to [4Players GmbH](https://www.4players.io/odin/faq/) for FAQ.

 # CRYENGINE 
 Cryengine is made and owned by [Crytek](https://www.cryengine.com/) 


# How to use plugin 

Right now `CryOdin` doesn't use CryAudio System ( Cryengine audio system ). Instead it uses [Miniaudio](https://miniaud.io/index.html). This is too much of issue but, it does mean there's a lot to know for you end-user in how to do Audio programming. Right the API of `CryOdin` handles most use cases of this. So it shouldn't be problem to plug in the CryOdin and get striaght into it.

If you want to build `CryOdin` on you're own you'll need to have Odin SDK from [OdinSDK](https://github.com/4Players/odin-sdk) and create a folder in `Cryengine SDKs` directory called  `Odin`. Once done you'll need to add `CryOdin` plugin into the `CryPlugins folders`. From there you'll add this line of code into the CMakeList.txt that is located in the `CryPlugins folder`.

### CMakeList.txt located in CryPlugins Folder 
```CMake
# Mandatory plugin, contains entities required by the engine
if (OPTION_ENGINE)
	add_subdirectory(CryDefaultEntities/Module)
	
	add_subdirectory(CryScaleformSchematyc/Module)
	add_subdirectory(CrySensorSystem/Module)
endif()

# VR plugins
if (PLUGIN_VR_OCULUS)
	add_subdirectory(VR/CryOculusVR/Module)
endif()
if (PLUGIN_VR_OPENVR)	
	add_subdirectory(VR/CryOpenVR/Module)
endif()	
if (PLUGIN_VR_OSVR)	
	add_subdirectory(VR/CryOSVR/Module)
endif()
if (PLUGIN_VR_EMULATOR)
	add_subdirectory(VR/CryEmulatorVR/Module)
endif()

# GamePlatform: Optional plugin; option PLUGIN_GAMEPLATFORM to enable/disable it resides in its own sub directory
add_subdirectory(CryGamePlatform)

# Order matters. Node plugin requires knowledge of PLUGIN_GAMEPLATFORM option defined above
add_subdirectory(CryGamePlatformNodes/Module)

# UQS: Optional plugin; option PLUGIN_CRYUQS to enable/disable it resides in its own sub directory
add_subdirectory(CryUQS)

add_subdirectory(CryHTTP/Module)
add_subdirectory(CryOdin/Module) ## this what you add 
```
You should be able to run now `cry_cmake.exe` and you'll see CryOdin.

### connecting with project

When you're able to build `CryOdin` with `CRYENGINE`, adding with your own project is striaght forward. In your Game.cryproject you'll need add this with plugins 
```json
{ "guid": "", "type": "EType::Native", "path": "CryOdin" },
``` 
Once add you should be run your project with CryOdin loaded in.

### Using Binary instead of building.
If you just want to use the plugin without building it you'll still need follow the directions above but, instead of running `cry_cmake.exe` you can just build and run your project like you'll normally do. Just make sure to go to [Release](https://github.com/VonBismarck1986/CryOdin/releases/tag/0.0.1) and download the Binary - CryOdin.dll and put inside of `Cryengine/bin/win_x64` folder



# CryOdin API
Now that project is setup and CryOdin is loaded how do we use it? 

#### Initializing plugin
First we get the plugin
```c++
// Headers to add
#include "ICryOdin.h"
//

auto pOdinPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::Odin::ICryOdinPlugin>();
```
The code above allows us to get CryOdin via Cryengine Plugin Manager 