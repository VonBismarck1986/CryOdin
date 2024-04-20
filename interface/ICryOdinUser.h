#pragma once

#include <odin.h>
#include <miniaudio.h>
#include <CryEntitySystem/IEntity.h>


namespace Cry
{
	namespace Odin
	{
		struct IUser
		{
			IUser()
				: m_pEntity(nullptr)
				, peerID(0)
				, userName(nullptr)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, mediaStream(0)
				, apmConfig(OdinApmConfig())
				, listenerPosition(CryAudio::CTransformation::GetEmptyObject())
			{}

			IUser(IEntity* pEntity, uint16_t peer_id, const char* user_name)
				: m_pEntity(pEntity)
				, peerID(peer_id)
				, userName(user_name)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, mediaStream(0)
				, apmConfig(OdinApmConfig())
				, listenerPosition(CryAudio::CTransformation::GetEmptyObject())
			{}

			IUser(IEntity* pEntity, uint16_t peer_id)
				: m_pEntity(pEntity)
				, peerID(peer_id)
				, userName(nullptr)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, mediaStream(0)
				, apmConfig(OdinApmConfig())
				, listenerPosition(CryAudio::CTransformation::GetEmptyObject())
			{}

			IUser(IEntity* pEntity)
				: m_pEntity(pEntity)
				, peerID(0)
				, userName(nullptr)
				, isTalking(false)
				, isMuted(false)
				, inputStream(0)
				, mediaStream(0)
				, apmConfig(OdinApmConfig())
				, listenerPosition(CryAudio::CTransformation::GetEmptyObject())
			{}


			ILINE const char* GetUserName() const { return userName; }
			ILINE bool IsTalking() const { return isTalking; }
			ILINE bool IsMuted() const { return isMuted; }
			ILINE uint64_t OdinPeerID() const { return peerID; }
			ILINE void SetTransformForListener(CryAudio::CTransformation& transform) { listenerPosition = transform; }

			IEntity* m_pEntity;
			const char* userName;
			bool isTalking;
			bool isMuted;
			CryAudio::CTransformation listenerPosition;

			uint64_t peerID; // Odin Peer ID
			OdinMediaStreamHandle inputStream; // default user input ( mic )
			OdinMediaStreamHandle mediaStream; // incoming media ( audio )
			OdinApmConfig apmConfig;
		};
	}
}