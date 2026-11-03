#include "StdAfx.h"
#include "CryOdinInput.h"

#include "CryOdin.h"

#include <CryGame/IGameFramework.h>
#include <CryInput/IInput.h>

#include <algorithm>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr uint32 kGamepadDevices = eAID_XboxPad | eAID_PS4Pad;

			EActionInputDevice GetGamepadDevice(const string& input)
			{
				// PlayStation inputs start with pad_, Xbox ones with xi_.
				return strncmp(input.c_str(), "pad_", 4) == 0 ? eAID_PS4Pad : eAID_XboxPad;
			}

			constexpr int kMaxGamepads = 4;

			enum class EInputHeld
			{
				No,
				Yes,
				Unknown // Not a name the input system knows, such as a PlayStation input on Windows.
			};

			EInputHeld IsInputHeld(const char* szInput)
			{
				if (gEnv->pInput == nullptr || szInput == nullptr || szInput[0] == '\0')
				{
					return EInputHeld::No;
				}

				const SInputSymbol* pSymbol = gEnv->pInput->GetSymbolByName(szInput);
				if (pSymbol == nullptr)
				{
					return EInputHeld::Unknown;
				}

				// Buttons are 1 while held. The input system sets 0 when the window loses focus.
				if (pSymbol->value > 0.f)
				{
					return EInputHeld::Yes;
				}

				// Each gamepad has its own symbols under the same names, and the lookup above only finds
				// the first pad. The action fires for any of them.
				if (pSymbol->deviceType == eIDT_Gamepad)
				{
					for (int i = 0; i < kMaxGamepads; ++i)
					{
						const SInputSymbol* pPadSymbol = gEnv->pInput->LookupSymbol(eIDT_Gamepad, i, pSymbol->keyId);
						if (pPadSymbol != nullptr && pPadSymbol->value > 0.f)
						{
							return EInputHeld::Yes;
						}
					}
				}
				return EInputHeld::No;
			}

			// The input the action was made with for these devices, empty if none.
			string GetDefaultInput(const IActionMapAction& action, uint32 devices)
			{
				for (int i = 0; i < action.GetNumActionInputs(); ++i)
				{
					const SActionInput* pInput = action.GetActionInput(i);
					if (pInput != nullptr && (static_cast<uint32>(pInput->inputDevice) & devices) != 0)
					{
						return string(pInput->defaultInput.c_str());
					}
				}
				return string();
			}

			bool AddInput(IActionMap& map, const ActionId& action, const string& input, EActionInputDevice device)
			{
				if (input.empty())
				{
					return true;
				}

				SActionInput actionInput;
				actionInput.input = input.c_str();
				actionInput.defaultInput = input.c_str();
				actionInput.inputDevice = device;
				// Hold would only repeat the press.
				actionInput.activationMode = eAAM_OnPress | eAAM_OnRelease;
				return map.AddAndBindActionInput(action, actionInput);
			}
		}

		IActionMapManager* CCryOdinInput::GetActionMapManager() const
		{
			if (m_bShutDown || gEnv->pGameFramework == nullptr)
			{
				return nullptr;
			}
			return gEnv->pGameFramework->GetIActionMapManager();
		}

		void CCryOdinInput::StartListening()
		{
			if (m_bListening)
			{
				return;
			}

			IActionMapManager* pManager = GetActionMapManager();
			if (pManager == nullptr)
			{
				return;
			}

			// For all action maps, as the game loading its action maps deletes the odin one, and with it
			// anything listening to it alone.
			pManager->AddExtraActionListener(this);
			pManager->RegisterActionMapEventListener(this);
			m_bListening = true;
		}

		void CCryOdinInput::StopListening()
		{
			if (!m_bListening)
			{
				return;
			}
			m_bListening = false;

			if (IActionMapManager* pManager = GetActionMapManager())
			{
				pManager->RemoveExtraActionListener(this);
				pManager->UnregisterActionMapEventListener(this);
			}
		}

		void CCryOdinInput::Shutdown()
		{
			StopListening();
			m_bShutDown = true;
			m_subscribers.clear();
			m_ownActions.clear();
		}

		CCryOdinInput::SSubscriber* CCryOdinInput::FindSubscriber(const ICryOdin::IPushToTalkListener& listener)
		{
			for (SSubscriber& subscriber : m_subscribers)
			{
				if (subscriber.pListener == &listener)
				{
					return &subscriber;
				}
			}
			return nullptr;
		}

		void CCryOdinInput::Subscribe(ICryOdin::IPushToTalkListener& listener, const char* szName, const SCryOdinPushToTalkKey& key, bool bReplaceInputs)
		{
			if (m_bShutDown)
			{
				return;
			}

			const string name = szName != nullptr ? szName : "";
			const ActionId action(key.GetActionName());
			const string keyboard = SCryOdinPushToTalkKey::CleanInputName(key.keyboard.c_str());
			const string gamepad = SCryOdinPushToTalkKey::CleanInputName(key.gamepad.c_str());

			if (keyboard.empty() && gamepad.empty())
			{
				ODIN_WARNING("The push to talk key of '%s' has no keyboard or gamepad input set", name.c_str());
			}
			if (!keyboard.empty() && gEnv->pInput != nullptr && gEnv->pInput->GetSymbolByName(keyboard.c_str()) == nullptr)
			{
				ODIN_WARNING("The push to talk key '%s' of '%s' isn't an input the engine knows. Use names such as v, capslock, lalt or mouse4", keyboard.c_str(), name.c_str());
			}
			// Xbox inputs are known even with no pad plugged in.
			if (!gamepad.empty() && gEnv->pInput != nullptr && gEnv->pInput->GetSymbolByName(gamepad.c_str()) == nullptr)
			{
				ODIN_WARNING("The push to talk gamepad input '%s' of '%s' isn't an input the engine knows on this platform. Use Xbox names such as xi_shoulderl or xi_dpad_down", gamepad.c_str(), name.c_str());
			}

			for (const SSubscriber& other : m_subscribers)
			{
				if (other.pListener != &listener && other.action == action && (other.keyboard != keyboard || other.gamepad != gamepad))
				{
					ODIN_WARNING("'%s' and '%s' both use the push to talk action '%s' with different keys, the keys of '%s' are used. Give them different Action Names to have two keys",
						other.name.c_str(), name.c_str(), action.c_str(), name.c_str());
				}
			}

			SSubscriber* pSubscriber = FindSubscriber(listener);
			if (pSubscriber == nullptr)
			{
				m_subscribers.emplace_back();
				pSubscriber = &m_subscribers.back();
				pSubscriber->pListener = &listener;
			}

			pSubscriber->name = name;
			pSubscriber->action = action;
			pSubscriber->keyboard = keyboard;
			pSubscriber->gamepad = gamepad;
			pSubscriber->bApplyPending = true;
			pSubscriber->bReplacePending = pSubscriber->bReplacePending || bReplaceInputs;

			// Registering and the action are both left to Update. This can be called while the manager
			// sends an action, and adding a listener then would change the list it's going through.
		}

		void CCryOdinInput::Unsubscribe(ICryOdin::IPushToTalkListener& listener)
		{
			m_subscribers.erase(std::remove_if(m_subscribers.begin(), m_subscribers.end(), [&listener](const SSubscriber& subscriber) { return subscriber.pListener == &listener; }), m_subscribers.end());
		}

		bool CCryOdinInput::IsOwnAction(const ActionId& action) const
		{
			return std::find(m_ownActions.begin(), m_ownActions.end(), action) != m_ownActions.end();
		}

		void CCryOdinInput::ApplyPendingInputs(IActionMapManager& manager)
		{
			if (m_bDispatching)
			{
				return;
			}

			for (SSubscriber& subscriber : m_subscribers)
			{
				if (subscriber.bApplyPending)
				{
					subscriber.bApplyPending = false;
					ApplyInputs(subscriber, manager);
					subscriber.bReplacePending = false;
				}
			}
		}

		void CCryOdinInput::ApplyInputs(SSubscriber& subscriber, IActionMapManager& manager)
		{
			IActionMap* pMap = manager.GetActionMap(kCryOdinActionMapName);
			if (pMap == nullptr)
			{
				pMap = manager.CreateActionMap(kCryOdinActionMapName);
				if (pMap == nullptr)
				{
					ODIN_WARNING("Couldn't create the action map '%s' for push to talk", kCryOdinActionMapName);
					return;
				}
			}

			const bool bOwn = IsOwnAction(subscriber.action);
			IActionMapAction* pAction = pMap->GetAction(subscriber.action);
			const bool bExisted = pAction != nullptr;
			if (pAction != nullptr)
			{
				// Compared with what it was made with, so a key the player rebound stays as it is.
				const bool bChanged = stricmp(GetDefaultInput(*pAction, eAID_KeyboardMouse).c_str(), subscriber.keyboard.c_str()) != 0
					|| stricmp(GetDefaultInput(*pAction, kGamepadDevices).c_str(), subscriber.gamepad.c_str()) != 0;

				if (subscriber.bReplacePending || (bOwn && bChanged))
				{
					pMap->RemoveAction(subscriber.action);
					pAction = nullptr;
				}
				else if (!bOwn && CCryOdin::Get() != nullptr && CCryOdin::Get()->IsDebugEnabled())
				{
					ODIN_LOG("Push to talk of '%s' uses the action '%s' from your action map XML", subscriber.name.c_str(), subscriber.action.c_str());
				}
			}

			if (pAction == nullptr)
			{
				if (!pMap->CreateAction(subscriber.action))
				{
					ODIN_WARNING("Couldn't create the push to talk action '%s'", subscriber.action.c_str());
					return;
				}

				const bool bKeyboard = AddInput(*pMap, subscriber.action, subscriber.keyboard, eAID_KeyboardMouse);
				const bool bGamepad = AddInput(*pMap, subscriber.action, subscriber.gamepad, GetGamepadDevice(subscriber.gamepad));
				if (!bKeyboard || !bGamepad)
				{
					ODIN_WARNING("Couldn't bind push to talk action '%s' to '%s' and '%s'", subscriber.action.c_str(), subscriber.keyboard.c_str(), subscriber.gamepad.c_str());
				}

				// One from the XML that code gave new inputs stays the game's, so the properties don't take it over.
				if (!bOwn && !bExisted)
				{
					m_ownActions.push_back(subscriber.action);
				}

				ODIN_LOG("Push to talk of '%s': action '%s', keyboard '%s', gamepad '%s'", subscriber.name.c_str(), subscriber.action.c_str(),
					subscriber.keyboard.empty() ? "none" : subscriber.keyboard.c_str(), subscriber.gamepad.empty() ? "none" : subscriber.gamepad.c_str());
			}

			// A new action map starts disabled. Also turned on again if the game turned it off, as a
			// component asking for its key now wants it to work.
			manager.EnableActionMap(kCryOdinActionMapName, true);
			m_bMapDisabledLogged = false;
		}

		bool CCryOdinInput::IsActionHeld(IActionMapManager& manager, const ActionId& action, bool bUnknownCountsAsHeld) const
		{
			IActionMap* pMap = manager.GetActionMap(kCryOdinActionMapName);
			if (pMap == nullptr || !pMap->Enabled())
			{
				return false;
			}

			const IActionMapAction* pAction = pMap->GetAction(action);
			if (pAction == nullptr)
			{
				return false;
			}

			bool bAnyKnown = false;
			for (int i = 0; i < pAction->GetNumActionInputs(); ++i)
			{
				const SActionInput* pInput = pAction->GetActionInput(i);
				const EInputHeld held = pInput != nullptr ? IsInputHeld(pInput->input.c_str()) : EInputHeld::No;
				if (held == EInputHeld::Yes)
				{
					return true;
				}
				bAnyKnown = bAnyKnown || held == EInputHeld::No;
			}
			// With only unknown inputs nothing can be checked, so it's left to the release of the action.
			return bUnknownCountsAsHeld && !bAnyKnown;
		}

		void CCryOdinInput::ReleaseKeysNoLongerHeld(IActionMapManager& manager)
		{
			// Collected first, a listener can subscribe or unsubscribe while it's told.
			std::vector<ICryOdin::IPushToTalkListener*> released;
			for (SSubscriber& subscriber : m_subscribers)
			{
				if (subscriber.bDown && !IsActionHeld(manager, subscriber.action, true))
				{
					subscriber.bDown = false;
					released.push_back(subscriber.pListener);
				}
			}

			for (ICryOdin::IPushToTalkListener* pListener : released)
			{
				if (FindSubscriber(*pListener) != nullptr)
				{
					pListener->OnPushToTalkInput(false);
				}
			}
		}

		void CCryOdinInput::CheckActionMapEnabled(IActionMapManager& manager)
		{
			const IActionMap* pMap = manager.GetActionMap(kCryOdinActionMapName);
			if (pMap != nullptr && !pMap->Enabled())
			{
				if (!m_bMapDisabledLogged)
				{
					m_bMapDisabledLogged = true;
					ODIN_LOG("The action map '%s' was turned off, so push to talk keys do nothing. Turn it on with IActionMapManager::EnableActionMap(\"%s\", true)", kCryOdinActionMapName, kCryOdinActionMapName);
				}
			}
			else
			{
				m_bMapDisabledLogged = false;
			}
		}

		void CCryOdinInput::FindLostActions(IActionMapManager& manager)
		{
			// Clear, RemoveAllActionMaps or a failed InitActionMaps delete everything without telling anyone.
			IActionMap* pMap = manager.GetActionMap(kCryOdinActionMapName);
			for (SSubscriber& subscriber : m_subscribers)
			{
				if (subscriber.bApplyPending)
				{
					continue;
				}
				if (pMap == nullptr || pMap->GetAction(subscriber.action) == nullptr)
				{
					stl::find_and_erase(m_ownActions, subscriber.action);
					subscriber.bApplyPending = true;
				}
			}
		}

		void CCryOdinInput::Update()
		{
			if (m_bShutDown || m_subscribers.empty())
			{
				return;
			}

			IActionMapManager* pManager = GetActionMapManager();
			if (pManager == nullptr)
			{
				return;
			}

			StartListening();
			FindLostActions(*pManager);
			ApplyPendingInputs(*pManager);
			ReleaseKeysNoLongerHeld(*pManager);
			CheckActionMapEnabled(*pManager);
		}

		void CCryOdinInput::OnAction(const ActionId& action, int activationMode, float value)
		{
			const bool bDown = (activationMode & eAAM_OnPress) != 0;
			const bool bUp = (activationMode & eAAM_OnRelease) != 0;
			if (bDown == bUp)
			{
				return;
			}

			// The engine sends a release for each bound input on its own. Letting go of V while the
			// gamepad button is still held mustn't stop talking. The released input is already 0 here.
			IActionMapManager* pManager = GetActionMapManager();
			const bool bOtherInputHeld = bUp && pManager != nullptr && IsActionHeld(*pManager, action, false);

			// Collected first, a listener can subscribe or unsubscribe while it's told.
			std::vector<ICryOdin::IPushToTalkListener*> changed;
			for (SSubscriber& subscriber : m_subscribers)
			{
				if (subscriber.action == action && subscriber.bDown != bDown && !(bUp && bOtherInputHeld))
				{
					subscriber.bDown = bDown;
					changed.push_back(subscriber.pListener);
				}
			}

			const bool bWasDispatching = m_bDispatching;
			m_bDispatching = true;
			for (ICryOdin::IPushToTalkListener* pListener : changed)
			{
				if (FindSubscriber(*pListener) != nullptr)
				{
					pListener->OnPushToTalkInput(bDown);
				}
			}
			m_bDispatching = bWasDispatching;
		}

		void CCryOdinInput::OnActionMapEvent(const SActionMapEvent& event)
		{
			if (event.event != SActionMapEvent::eActionMapManagerEvent_ActionMapsInitialized)
			{
				return;
			}

			// The game loaded its action maps, which deleted all others. Its XML may define the actions
			// now. Done right away, so the player's bindings the game loads next find them.
			m_ownActions.clear();
			for (SSubscriber& subscriber : m_subscribers)
			{
				subscriber.bApplyPending = true;
			}

			if (IActionMapManager* pManager = GetActionMapManager())
			{
				ApplyPendingInputs(*pManager);
			}
		}
	}
}
