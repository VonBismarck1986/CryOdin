#include "StdAfx.h"
#include "OdinComponents/CryOdinUserComponent.h"

#include "Core/CryOdinAudioSystem.h"

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			static void RegisterOdinUserComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinUserComponent));
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinUserComponent);
		}

		void CCryOdinUserComponent::Initialize()
		{
			m_UserID = (uint64_t)gEnv->pNetContext->GetNetID(m_pEntity->GetId()).GetAsUint32();
		}

		Cry::Entity::EventFlags CCryOdinUserComponent::GetEventMask() const
		{
			return EEntityEvent::Update | EEntityEvent::Reset;
		}

		void CCryOdinUserComponent::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
				case EEntityEvent::Update:
				{
					if (m_gameplayStarted)
					{

					}
				}
				break;
				case EEntityEvent::Reset:
				{
					m_gameplayStarted = event.nParam[0] != 0;
				}
				break;
			}
		}

		uint64_t CCryOdinUserComponent::GetUserId() const
		{
			return (uint64_t)gEnv->pNetContext->GetNetID(m_pEntity->GetId()).GetAsUint32();
		}

		OdinMediaStreamHandle CCryOdinUserComponent::GetMediaHandle(EAudioHandleType type) const
		{
			auto index = static_cast<size_t>(type);
			return m_mediaHandles[index];
		}

		string CCryOdinUserComponent::ToStringDebug() const
		{
			static stack_string debugStr;
			string temp1 = string().Format("Username: %s\nPeerID: (%d)\nUserID: (%d)\nRoom: %s\nInputHandle: (%d)\nOutputHandle: (%d)\n", m_UserName, m_PeerID, m_UserID, m_RoomName.c_str(), m_mediaHandles[1], m_mediaHandles[2]);
			string temp2 = string().Format("OnlineStatus: ( %s )", m_OnlineStatus);
			debugStr.Format("%s%s", temp1, temp2);
			return debugStr;
		}

		void CCryOdinUserComponent::SetMediaHandle(EAudioHandleType type, OdinMediaStreamHandle handle)
		{
			auto index = static_cast<size_t>(type);
			m_mediaHandles[index] = handle;
		}

		void CCryOdinUserComponent::SetRoomHandle(OdinRoomHandle room)
		{
			m_room = room;
		}

	}
}