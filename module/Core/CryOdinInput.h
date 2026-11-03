#pragma once
#include "Utils/OdinHelpers.h"

#include <ICryOdin.h>
#include <CryAction/IActionMapManager.h>
#include <CryString/CryString.h>

#include <vector>

namespace Cry
{
	namespace Odin
	{
		// Push to talk keys as actions of the engine's action map manager, in the action map
		// kCryOdinActionMapName. So they can be rebound like the game's own actions. Main thread only.
		//
		// The action map is only changed from Update and when the game loads its action maps, never
		// while the manager sends an action, as that would delete what it's working with.
		class CCryOdinInput final : public IActionListener, public IActionMapEventListener
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinInput)
			CCryOdinInput() = default;
			// Doesn't touch the action map manager, which may be gone by then. See Shutdown.
			virtual ~CCryOdinInput() = default;

			void Subscribe(ICryOdin::IPushToTalkListener& listener, const char* szName, const SCryOdinPushToTalkKey& key, bool bReplaceInputs);
			// The listener gets no more calls, not even a last key up.
			void Unsubscribe(ICryOdin::IPushToTalkListener& listener);

			void Update();
			// Call before CryAction shuts down. Nothing is registered with it after this.
			void Shutdown();

			// IActionListener
			virtual void OnAction(const ActionId& action, int activationMode, float value) override;
			// ~IActionListener

			// IActionMapEventListener
			virtual void OnActionMapEvent(const SActionMapEvent& event) override;
			// ~IActionMapEventListener

		private:
			struct SSubscriber
			{
				ICryOdin::IPushToTalkListener* pListener = nullptr;
				string name;
				ActionId action;
				string keyboard;
				string gamepad;
				bool bDown = false;
				bool bApplyPending = true;
				bool bReplacePending = false;
			};

			IActionMapManager* GetActionMapManager() const;
			void StartListening();
			void StopListening();
			void ApplyPendingInputs(IActionMapManager& manager);
			void ApplyInputs(SSubscriber& subscriber, IActionMapManager& manager);
			bool IsOwnAction(const ActionId& action) const;
			// @param bUnknownCountsAsHeld Used when every bound input has a name the input system doesn't know.
			bool IsActionHeld(IActionMapManager& manager, const ActionId& action, bool bUnknownCountsAsHeld) const;
			// Something deleted the action map or an action without the event, so it's made again.
			void FindLostActions(IActionMapManager& manager);
			void ReleaseKeysNoLongerHeld(IActionMapManager& manager);
			void CheckActionMapEnabled(IActionMapManager& manager);
			SSubscriber* FindSubscriber(const ICryOdin::IPushToTalkListener& listener);

			std::vector<SSubscriber> m_subscribers;
			// Made here rather than by the game's action map XML, so their inputs follow the properties.
			std::vector<ActionId> m_ownActions;
			bool m_bListening = false;
			bool m_bShutDown = false;
			bool m_bDispatching = false; // The manager is sending an action, so it can't be changed now.
			bool m_bMapDisabledLogged = false;
		};
	}
}
