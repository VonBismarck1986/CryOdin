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

Right now `CryOdin` doesn't use CryAudio System ( Cryengine audio system ). Instead it uses [Miniaudio](https://miniaud.io/index.html). This isn't too much of issue but, it does mean this plugin will take a while to be up to par with CryAudio. Right now the API of `CryOdin` handles most use cases of this. So it shouldn't be problem to plug CryOdin and get striaght into it.

If you want to build `CryOdin` you'll need to have Odin SDK from [OdinSDK](https://github.com/4Players/odin-sdk) and create a folder in `Cryengine SDKs` directory called  `Odin`. Once done you'll need to add `CryOdin` plugin into the `CryPlugins folders`. From there you'll add this line of code into the CMakeList.txt that is located in the `CryPlugins folder`.

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

### Connecting with CRYENGINE Source Code

When you're able to build `CryOdin` with `CRYENGINE`, adding it with your own project is striaght forward. In your Game.cryproject you'll need add this with plugins 
```json
{ "guid": "", "type": "EType::Native", "path": "CryOdin" },
``` 
Once add you should be run your project with CryOdin loaded in.

### Using Binary instead of building.
If you just want to use the plugin without building it you'll still need follow the directions above but, instead of running `cry_cmake.exe` you can just build and run your project like you'll normally do. Just make sure to go to [Release](https://github.com/VonBismarck1986/CryOdin/releases) and download the Binary - CryOdin.dll and put inside of `Cryengine/bin/win_x64` folder.

Remember Not only does CryOdin.dll must in `bin\win_x64` of `CRYENGINE` so do `odin.dll` and `odin_crypto.dll` from [OdinSDK](https://github.com/4Players/odin-sdk). The plugin won't load without both of them!

# CryOdin API
CryOdin gives you voice and text through 4Players ODIN SDK 2.x, and plays voices through [miniaudio](https://miniaud.io/index.html).

Add `Code/CryPlugins/CryOdin/Interface` to your game project's include paths. That's all you need to use the plugin. You don't link against it, and you don't need the Odin or miniaudio headers unless you want the raw handles.

Call everything from the main thread, and all events arrive on the main thread. The only exception is `ICryOdinAudioEffect::Process`, which runs on an audio thread.

## Access keys and tokens
To enter a room you need a room token, signed with an access key. There are three ways to get one:

| Way | Use for |
|---|---|
| Your CRYENGINE dedicated server holds the key in `dedicated.cfg`, and `OdinNetworkComponent` asks it for each token. See [Dedicated server](#dedicated-server) | **A shipped multiplayer game.** No game code needed |
| Your own server signs a token and gives it to the client: `SCryOdinRoomParams::szToken`, or `SetRoomToken` on the component | **A shipped game** with a login or token server of its own. 4Players asks you to never ship an access key in a client |
| The CMake option `ODIN_ACCESS_KEY`, `ICryOdin::SetAccessKey`, or the `odin_set_key` console command | Development. The CMake key is only compiled into Debug and Profile builds |
| Nothing. A temporary key is created | Trying it alone. **Clients with different keys can't meet in a room**, so this doesn't work with two clients |

## Using components
Add these to your entities in the Sandbox or from code. They're in the `Odin` category. In Schematyc you get their properties, signals and main functions. The full API is in C++.

| Component | Add it to | What it does |
|---|---|---|
| `OdinUserComponent` | The local player | Joins a room, sends the microphone and text, and sends events to Schematyc. Use one per room. For an entity placed in the level, turn off Local Player Only, as your game has to spawn the local player as one |
| `OdinComponentSound` | Each remote player | Plays that player's voice from their entity, with its own volume, 3D settings, occlusion and radio |
| `OdinComponentListener` | The local player | Voices are heard from this entity. Without it, they're heard from the view camera |
| `OdinNetworkComponent` | Every player entity, on the server and the clients | Gets tokens from the dedicated server, and tells every client which voice belongs to the entity. See [Dedicated server](#dedicated-server) |

For a shipped game, turn on Require Token on `OdinUserComponent`. Then tokens come only from the server, through `OdinNetworkComponent`, or from `SetRoomToken` when your own server replies. With Auto Join on, either one joins the room.

`OdinComponentSound` needs to know who the entity is. With `OdinNetworkComponent` on the same entity, that happens by itself. Otherwise each player gets a User Id from `OdinUserComponent` (the property, `SetUserId`, or a random one). Send that id over your game's network, then call `BindToUserId` on that player's entity on the other clients. Voices that aren't bound to an entity still play, in 2D.

## Dedicated server
A CRYENGINE dedicated server can hold the access key, so clients never get it. The server signs a token for each player who asks, for the room they ask for, with a user id the server picks.

**1. Put the key in `dedicated.cfg`** on the server machine, in the project's folder or its `config` folder. Lines starting with `--` or `;` are comments.
```
-- CryOdin
odin_serverAccessKey = YOUR_ACCESS_KEY
-- Optional, empty or missing allows any room
odin_serverAllowedRooms = lobby,team_red,team_blue
```
The engine doesn't load `dedicated.cfg` by itself, so CryOdin reads it on a dedicated server, and only the lines starting with `odin_`. If your game loads the file too, that's fine. You can also set `odin_serverAccessKey` in any config the server loads, or from C++ with `SetAccessKey`. The console never shows the key's value. Some things still put it in a log or a file:
- **The command line.** It's written to the log, so don't pass the key there.
- **A line without `=`.** The engine logs such a line whole, key included, so always write the `=`.
- **Dumping all variables to a file.** The engine's dump commands include it.

With `odin_serverAllowedRooms` empty, the server logs at start that any room is allowed. Players could then also ask for another team's room, so list your rooms, or check them in a validator.

**2. Set up the player entity.** It needs these components, on the server and on every client alike:
- `OdinNetworkComponent`: a networked component, so it must be on the same entity everywhere.
- `OdinUserComponent` and `OdinComponentSound`: ClientOnly, so a dedicated server leaves them out by itself.

Your game has to spawn one entity per player on the server, give it the player's channel and bind it to the network, as the C++ templates do in `OnClientConnectionReceived`. `OdinNetworkComponent` doesn't bind the entity itself, so it can't bind it before your own components are added:
```c++
pPlayerEntity->GetNetEntity()->SetChannelId(channelId);
// and in the player component's Initialize:
m_pEntity->GetNetEntity()->BindToNetwork();
```
Turn on Require Token on `OdinUserComponent`, so a client never signs with a key of its own.

**3. What happens when a client joins**
1. `OdinUserComponent` sees `OdinNetworkComponent` and no token, so Join asks the server. Auto Join works the same way.
2. The server checks the request:
   - it came from that entity's own player;
   - the room is in `odin_serverAllowedRooms`;
   - the player isn't asking too often (10 in 10 seconds);
   - your validator, if you set one, agrees.
3. The server signs a 5 minute token for the player's user id and sends it only to that player. The client then joins the room.
4. A refusal, or no answer within 15 seconds, sends On Join Failed with the reason and logs a warning on both sides. If you retry from On Join Failed, wait a few seconds first, or the rate limit refuses the retries too.
5. Every client asks the server for each player entity's user id. The entity's `OdinComponentSound` then plays that player's voice, unless your game gave it a User Id itself. Turn off Bind Voice on `OdinNetworkComponent` to do it yourself.

In single player, in the Sandbox, and for the host of a listen server, the same components sign the token right away with the key that machine has.

**User ids.** The server gives each player entity a random GUID. To use your own, such as an account id, call `SetOdinUserId` on the server before the player joins. Clients are told about the change, but a player already in a room stays there under the old id until they join again, and until then the others can't match their voice. `GetOdinUserId` gives the id on any machine.

**Your own rules.** On the server, game code can refuse a request or change how long the token lasts. The room and user id can't be changed there, as the player joins with the ones asked for:
```c++
struct CMyValidator : public Cry::Odin::ICryOdin::IServerTokenValidator
{
	virtual bool ValidateServerTokenRequest(Cry::Odin::ICryOdin::SServerTokenRequest& request, string& refuseReason) override
	{
		// request.channelId, request.entityId, request.szRoomId, request.szUserId
		if (strcmp(request.szRoomId, "team_red") == 0 && !IsOnRedTeam(request.channelId))
		{
			refuseReason = "not on the red team";
			return false;
		}
		request.lifetimeSeconds = 60;
		return true;
	}
};
pOdin->SetServerTokenValidator(&myValidator); // nullptr removes it
```

**Keep the key off clients.** A shipped client must not have the CMake `ODIN_ACCESS_KEY` (it's only in Debug and Profile builds anyway) or `odin_serverAccessKey` in any of its config files. With `OdinNetworkComponent` on its entity, a client connected to a server always asks that server, and Require Token stops it from signing a token itself when the component is missing. In single player that client is its own server and signs with a temporary key, which only puts it in a room alone.

## Using C++
```c++
#include <ICryOdin.h>
#include <OdinComponents/CryOdinComponentSound.h>

auto* pPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::Odin::ICryOdinPlugin>();
Cry::Odin::ICryOdin* pOdin = pPlugin->GetOdin();

Cry::Odin::SCryOdinRoomParams params;
params.szRoomId = "lobby";
params.szUserName = "Player";
params.szUserId = "account-1234";
params.szToken = tokenFromYourServer;
params.processing.noiseSuppression = Cry::Odin::ECryOdinNoiseSuppression::High;

Cry::Odin::ICryOdinRoom* pRoom = pOdin->JoinRoom(params);
```

Implement `ICryOdin::IListener` for events. Override only the ones you need, then register it with `pOdin->AddListener(*this, "MyListener")`.

```c++
struct CMyListener : public Cry::Odin::ICryOdin::IListener
{
	virtual void OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user) override
	{
		// Plays this voice from the player's entity
		pPlayerEntity->GetOrCreateComponent<CCryOdinComponentSound>()->BindToUser(user);
	}

	virtual void OnTextMessage(ICryOdinRoom& room, ICryOdinUser* pSender, const char* szText, const char* szChannel) override
	{
		CryLogAlways("%s: %s", pSender ? pSender->GetName() : "?", szText);
	}
};
```

| Event | When |
|---|---|
| `OnRoomStatusChanged` | The room is joining, joined, closing or closed |
| `OnJoinedRoom`, `OnLeftRoom` | The room is ready, or it has ended. A failed join also ends with `OnLeftRoom`. If Odin reconnects after a lost connection, status goes back to Joining |
| `OnPeerJoined`, `OnPeerLeft` | A user came or went. `OnPeerLeft` also comes for the local user when the connection ends |
| `OnPeerUserDataChanged` | A user's data was changed |
| `OnPeerTalkingChanged` | A user started or stopped talking, including the local user |
| `OnTextMessage`, `OnDataMessage`, `OnSocketMessage` | Something was received |
| `OnRoomError`, `OnRawRpc` | A server error, and every raw event from Odin as JSON |
| `OnTransmitChanged` | `SetTransmitEnabled` turned sending to a room on or off, for example by push to talk |
| `OnEncoderChanged` | `SetEncoderSettings` replaced a room's encoder. Only matters if you use `GetEncoder` |
| `OnAudioDeviceChanged` | A device was changed or replaced |

Pointers to rooms and users stay valid until `OnLeftRoom` and `OnPeerLeft` have returned, so clear your own there.

## Text and data
```c++
pRoom->SendText("Hello");                       // To everyone
pRoom->SendText("Enemy spotted", "team");       // With a channel of your choice
pRoom->SendText("Hi", nullptr, &peerId, 1);     // To one user
pRoom->SendData(bytes, size);                   // Your own data, reliable and in order
pRoom->SetUserData("{\"name\":\"Player\",\"team\":\"red\"}"); // Synced to all users
```

For high frequency data, `OpenSocket` gives you a reliable or unreliable data channel to one user or to all of them.

## Radio overlay
The radio makes a voice sound like it's coming over a radio: band limited, distorted, with static and a squelch burst when a transmission starts and ends. It's applied on the listening client, per voice.

```c++
// One voice
pUser->GetSound()->SetRadioSettings(GetCryOdinRadioPreset(ECryOdinRadioPreset::Military));
pSoundComponent->SetRadioPreset(ECryOdinRadioPreset::Handheld);

// Every voice in a room, a good fit for a separate team radio room
params.voice.radio = GetCryOdinRadioPreset(ECryOdinRadioPreset::Military);
```

The presets are `Clean`, `Handheld`, `Military`, `Aircraft` and `Damaged`. For your own sound, use the `Custom` preset and set the values in `SCryOdinRadioSettings`. These are also properties on the components. By default a radio voice ignores position, since a radio is heard in the ear. Turn off `bDisableSpatialization` to hear it from the speaker's location.

To be in both a nearby voice room and a team radio room, join two rooms, each with its own settings. Use `SetTransmitEnabled` on each room to choose where the microphone goes.

## Push to talk
Set Transmit Mode on `OdinUserComponent` to Push To Talk. The microphone is then only sent while the key is held. The key is in the component's Push To Talk Key property:

| Property | What it does |
|---|---|
| Use Built In Key | On: the plugin listens for the keys below. Off: call `SetPushToTalk(true/false)` from your own input code instead |
| Toggle | Press once to talk and again to stop, instead of holding the key |
| Action Name | `odin_push_to_talk` by default. Give each room its own name to have a key per room, such as V for nearby voice and B for team radio |
| Keyboard Or Mouse | An engine input name, such as `v`, `b`, `capslock`, `lalt`, `mouse4` or `mouse5`. Empty for none |
| Gamepad | Such as `xi_shoulderl` or `xi_dpad_down`, on any connected Xbox pad. Use a button. Triggers and sticks are analog and don't send presses. PlayStation `pad_` names aren't known by the engine on Windows |

The key is a normal action in the engine's action map manager, in the action map `odin`. So:
- **Players can rebind it** in your options menu, like your own actions: `pActionMapManager->ReBindActionInput("odin", "odin_push_to_talk", "v", "b")`. The game saves it with the player's other bindings (`SaveRebindDataToXML`).
- **For rebinding that's saved between sessions**, add the action to the action map file your game loads with `InitActionMaps`. The plugin then uses your inputs rather than the property's, and the player's saved bindings load onto it as usual:
  ```xml
  <actionmap name="odin">
    <action name="odin_push_to_talk" onPress="1" onRelease="1" keyboard="v" xboxpad="xi_shoulderl" />
  </actionmap>
  ```
- **To change it from code**, call `SetPushToTalkInputs("b", "xi_dpad_down")` or `SetPushToTalkKey` on the component. These replace the action's inputs, including ones from your XML or rebound by the player.
- **If your game turns off all action maps**, for example in a menu, turn `odin` back on afterwards with `EnableActionMap("odin", true)`. The log says when it finds it turned off.

It doesn't get stuck on. If the key is let go without the action seeing it, for example while the console is open or after the window lost focus, talking stops anyway. In Toggle mode it keeps going until pressed again. With both a keyboard key and a gamepad button bound, talking goes on until both are let go.

For a talk icon or a click sound, use the component's On Push To Talk Changed signal, or `OnTransmitChanged` on `ICryOdin::IListener`.

For rooms you join from C++ without the component, implement `ICryOdin::IPushToTalkListener` and call `pOdin->SubscribePushToTalk(listener, "Radio", key)`. Turn sending on and off yourself in `OnPushToTalkInput`, with `pRoom->SetTransmitEnabled(bDown)`.

## Voice quality
Each room has its own encoder settings, so a team radio room can use less bandwidth than nearby voice. On `OdinUserComponent` they're in the Voice Quality property:

| Setting | What it does |
|---|---|
| Quality | Low (16 kbps), Standard (32 kbps, the default), High (64 kbps), Very High (128 kbps), or Custom |
| Custom Bitrate | Bits per second when Quality is Custom, 6000 to 512000 |
| Expected Packet Loss | 0 to 100, default 15. Higher keeps the voice clearer on a bad connection, at some cost to quality |
| Voice Optimized | On tunes the codec for speech. Turn it off for music or other sounds |

In C++, `SCryOdinEncoderSettings` starts as Custom with `iBitrateBps = 32000`, so setting `iBitrateBps` alone works. Set `quality` to use a preset.

Each listener receives the bitrate once for every speaker they hear. So 64 kbps with 8 people talking at once is about 512 kbps down for each listener.

Players can change it at any time, for example from an options menu: `SetVoiceQuality`, `SetVoiceBitrate`, `SetExpectedPacketLoss` in Schematyc, or `SetEncoderSettings` on the component or on `ICryOdinRoom` in C++. Before joining it's simply used on join. While joined, the room makes a new encoder about 0.3 s after the last change, so dragging a slider gives one change, not dozens. What you notice then:
- A short gap in your voice if you're talking at that moment.
- The microphone processing of that room starts over: the echo canceller takes a second or two to adapt again, the gain controller starts at its default level, and noise suppression learns the background again.
- With Voice Isolation on, its model loads again.

The same rebuild makes **Send Position work when turned on after joining**: the room gets a new encoder that sends your position while you're silent.

## Your own audio effects
Implement `ICryOdinAudioEffect` and add it to a voice with `AddEffect`, or to the microphone with `ICryOdinRoom::AddCaptureEffect`. `Process` runs on an audio thread, so don't allocate, block or call the engine from it.

## Audio settings
| To change | Use |
|---|---|
| Microphone processing | `SCryOdinAudioProcessing` when joining, or `ICryOdinRoom::SetAudioProcessing` |
| Bitrate and codec | Voice Quality on the component, or `SCryOdinEncoderSettings` and `ICryOdinRoom::SetEncoderSettings`, see [Voice quality](#voice-quality) |
| Push to talk | Transmit Mode and Push To Talk Key on the component, see [Push to talk](#push-to-talk). Or `ICryOdinRoom::SetTransmitEnabled` |
| Mute, gain, master volume, levels | `ICryOdinAudioSystem` |
| One voice's volume, 3D, occlusion | `SCryOdinVoiceSettings` and `ICryOdinSound`, or `OdinComponentSound` |
| Microphone and speakers | `ICryOdinAudioDevice`. They only open while you're in a room |
| Skipping far away voices on the server | Send Position on the component, or `ICryOdinRoom::SetPosition` and `SetChannelMasks` |
| End to end encryption | `SCryOdinRoomParams::szPassword` |
| Raw access | `GetAudioEngine`, `GetMASoundHandle`, `GetDecoder`, `GetEncoder`, `GetNativeHandle` |

#### Microphone processing, from the Odin docs
- **Voice Activity Detection:** analyzes the input to find speech. You set the probability required to start and stop sending.
- **Volume Gate:** only sends when the input is loud enough. You set the dBFS to start and stop sending.
- **Echo Canceller:** removes the voices played on your speakers from the microphone. CryOdin gives it the mixed output.
- **Noise Suppression:** removes background noise. Higher levels distort speech more.
- **High Pass Filter:** removes low frequency rumble.
- **Transient Suppressor:** attenuates keyboard clicks.
- **Gain Controller:** keeps the input at a steady level.
- **Voice Isolation:** a deep learning model that removes everything except the voice. It loads the model when first used.

## Console
These work without any game code, which makes them useful for testing. Join a room first, as the other commands apply to rooms you're already in. Set `odin_debug 1` to see received text in the console.

To test two clients on one PC, set `ed_keepEditorActive 1` in the Sandbox. Otherwise it stops updating when another window has focus, and users who join meanwhile can't be heard until you click back into it. Both clients also need the same access key.

| Command | What it does |
|---|---|
| `odin_join <room> [name] [user id]` | Joins a room |
| `odin_leave [room]` | Leaves one room, or all of them |
| `odin_say <text>` | Sends text to all rooms |
| `odin_loopback [0\|1]` | Plays your own encoded voice back to you, to test without a second client |
| `odin_radio <off\|clean\|handheld\|military\|aircraft\|damaged>` | Sets the radio on all voices |
| `odin_bitrate <low\|standard\|high\|veryhigh\|kbps> [room]` | Sets the bitrate of your voice in all rooms, or one |
| `odin_mute [0\|1]` | Mutes the microphone |
| `odin_devices`, `odin_set_input <index>`, `odin_set_output <index>` | Lists and selects devices. -1 is the system default |
| `odin_set_key <key>` | Sets the access key. Not in Release builds |

| CVar | What it does |
|---|---|
| `odin_debug` | 1 draws voice information on screen. 2 also logs the Odin SDK's own messages, set it before Odin starts |
| `odin_occlusionRaysPerFrame` | How many occlusion rays voices can cast each frame |
| `odin_occlusionPerHit` | How much each surface between a voice and the listener adds to its occlusion |
| `odin_serverAccessKey` | The access key the server signs tokens with. Hidden in the console, set it in `dedicated.cfg` |
| `odin_serverAllowedRooms` | Rooms the server signs tokens for, separated by commas. Empty allows any room |
