# CryOdin
This repository is first attempt at linking together Odin and CRYENGINE. Please note until other wise written 
using this Plugin for you're game wouldn't be recommend unless you know what you're doing. I will be polishing this 
Plugin to be able to used without issues.

# Odin 
 Odin is made by `4Players` check out their website here [4Players GmbH](https://www.4players.io/) for more information on Odin and pricing.
 Right now they offer a free use for services and go to [4Players GmbH](https://www.4players.io/odin/faq/) for FAQ.

 # CRYENGINE 
 [CRYENGINE](https://www.cryengine.com/) is made and owned by [Crytek](https://www.crytek.com/) 


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
The code above allows us to get CryOdin via Cryengine Plugin Manager.

Next we setup Audio Processing Module (APM) `APM Config`, this config just Odin what features to use when a player is using their mic
```c++
OdinApmConfig apmConfig{};
apmConfig.voice_activity_detection = true;
apmConfig.voice_activity_detection_attack_probability = 0.9;
apmConfig.voice_activity_detection_release_probability = 0.8;
apmConfig.volume_gate = false;
apmConfig.volume_gate_attack_loudness = -30;
apmConfig.volume_gate_release_loudness = -40;
apmConfig.echo_canceller = true;
apmConfig.high_pass_filter = false;
apmConfig.pre_amplifier = false;
apmConfig.noise_suppression_level = OdinNoiseSuppressionLevel_Moderate;
apmConfig.transient_suppressor = false;
apmConfig.gain_controller = true;
```
From Odin Docs:

#### Voice Activity Detection (VAD)
When enabled, ODIN will analyze the audio input signal using smart voice detection algorithm to determine the presence of speech. You can define both the probability required to start and stop transmitting.

#### Input Volume Gate
When enabled, the volume gate will measure the volume of the input audio signal, thus deciding when a user is speaking loud enough to transmit voice data. You can define both the root mean square power (dBFS) for when the gate should engage and disengage.

#### Acoustic Echo Cancellation (AEC)
When enabled the echo canceller will try to subtract echoes, reverberation, and unwanted added sounds from the audio input signal. Note, that you need to process the reverse audio stream, also known as the loopback data to be used in the ODIN echo canceller.

#### Noise Suppression
When enbabled, the noise suppressor will remove distracting background noise from the input audio signal. You can control the aggressiveness of the suppression. Increasing the level will reduce the noise level at the expense of a higher speech distortion.

#### High-Pass Filter (HPF)
When enabled, the high-pass filter will remove low-frequency content from the input audio signal, thus making it sound cleaner and more focused.

#### Preamplifier
When enabled, the preamplifier will boost the signal of sensitive microphones by taking really weak audio signals and making them louder.

#### Transient Suppression
When enabled, the transient suppressor will try to detect and attenuate keyboard clicks.

#### Automatic Gain Control (AGC)
When enabled, the gain controller will bring the input audio signal to an appropriate range when it’s either too loud or too quiet.

Once you setup the APM next start Odin with
```c++
/// Continued from above code
pOdinPlugin->GetOdin()->Init(nullptr, apmConfig);
```
`Init(arg,arg2)` this Init is simple we pass AccessKey (const char*) which you get from Odin or allow the plugin to create one for you and APM config.
#### Please Note:
When use the plugin to create a AccessKey this key is limited to 25 users connected and so is the Free Key that Odin will provide from [Here](https://www.4players.io/odin/introduction/access-keys/) it's best use Odin Key Generator since the plugin right will just keep creating a new one each time. ( fixing this soon. )