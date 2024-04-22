#include "StdAfx.h"
#include "CryOdinAudioSystem.h"

#include "CryOdin.h"

#include "CryOdinAudioSound.h"
#include "CryOdinAudioOcclusion.h"
#include "CryOdinAudioDevices.h"
#include "CryOdinAudioDataSource.h"

#include "Utils/MiniAudioHelpers.h"

#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
	namespace Odin
	{
		std::unique_ptr<CCryOdinAudioDevice> g_pAudiodevices = nullptr;
		ma_engine g_engine;

		static ma_allocation_callbacks MemoryCallback = {
			nullptr,
			Utils::Allocator::Alloc,
			Utils::Allocator::ReAlloc,
			Utils::Allocator::DeAlloc,
		};

		CCryOdinAudioSystem* CCryOdinAudioSystem::CCryOdinAudioSystem::s_AudioSystem = nullptr;

		CCryOdinAudioSystem::CCryOdinAudioSystem()
		{
			if (!s_AudioSystem)
			{
				s_AudioSystem = this;
			}

			ODIN_LOG("CryOdinAudio System");
		}

		bool CCryOdinAudioSystem::Init()
		{
			g_pAudiodevices.reset(new CCryOdinAudioDevice());

			ma_result result;

			ma_engine_config config;
			config = ma_engine_config_init();
			config.pDevice = NULL; // Capture device - Speakers
			config.allocationCallbacks = MemoryCallback;
			config.listenerCount = 1;

			result = ma_engine_init(&config, &g_engine);
			if (result != MA_SUCCESS) {
				ODIN_LOG("Failed to initialize the engine.");
				return false;
			}
			else {
				ODIN_LOG("Initialize Audio Engine.");
			}

			ma_engine_listener_set_world_up(&g_engine, 0, 0.0f, 0.0f, 1.0f); // double check on this, make sure it's Cryengine world UP position
			ma_engine_listener_set_cone(&g_engine, 0, 10.f, 25.f, 0.25f);

			return true;
		}

		void CCryOdinAudioSystem::Shutdown()
		{
			g_pAudiodevices.release();
		}

		void CCryOdinAudioSystem::OnUpdate(float frameTime)
		{
			UpdateListener(frameTime);
			DrawDebug(frameTime);
		}

		ma_engine* CCryOdinAudioSystem::GetAudioEngine()
		{
			return &g_engine;
		}

		SCryOdinAudioDevicesConfig CCryOdinAudioSystem::GetAudioDeviceConfig() const
		{
			return g_pAudiodevices->GetDevice().GetAudioDeviceConfig();
		}

		void CCryOdinAudioSystem::CreateSoundObject(const IUser& user)
		{
		}

		void CCryOdinAudioSystem::AddLocalPlayer(const IUser& user)
		{
			m_user = user;
			g_pAudiodevices->GetDevice().AddLocalUser(m_user);
		}

		CCryOdinAudioSound* CCryOdinAudioSystem::CreateSound(const IUser& user)
		{
			auto& mat = user.m_pEntity->GetWorldTM();

			CCryOdinAudioSound* sound = new CCryOdinAudioSound(user, CryAudio::CTransformation(mat));
			if (sound)
			{
				sound->SetMediaHandle(user.mediaStream);
				sound->SetRoomHandle(user.room);

				ODIN_LOG("Media Handle for sound set to %d with room id of (%d)", sound->GetMediaHandle(), sound->GetRoomHandle());

				g_pAudiodevices->GetDevice().SoundCreated(&g_engine, sound);
			}

			return sound;
		}

		void CCryOdinAudioSystem::InitSound(CCryOdinAudioSound* sound)
		{
			if (sound)
			{
				//m_pAudiodevices->SoundCreated(&g_engine, sound);
			}
		}

		void CCryOdinAudioSystem::UpdateListener(float const frameTime)
		{
			if (!m_user.m_pEntity)
				return;

			auto dir = m_user.m_pEntity->GetWorldRotation().GetColumn1();

			auto pos = m_user.m_pEntity->GetWorldPos();
			pos.z = pos.z + 1.2f;

			ma_engine_listener_set_direction(&g_engine, 0, dir.x, dir.y, dir.z);
			ma_engine_listener_set_position(&g_engine, 0, pos.x, pos.y, pos.z);


		}

		void CCryOdinAudioSystem::DrawDebug(float const frameTime)
		{
			if (m_user.m_pEntity)
			{
				Vec3 pos = Vec3(ma_engine_listener_get_position(&g_engine,0).x, ma_engine_listener_get_position(&g_engine, 0).y, ma_engine_listener_get_position(&g_engine, 0).z);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.5f, Col_Red);

				Vec3 Dir = Vec3(ma_engine_listener_get_direction(&g_engine,0).x, ma_engine_listener_get_direction(&g_engine, 0).y, ma_engine_listener_get_direction(&g_engine, 0).z);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_user.m_pEntity->GetWorldPos(), Col_DarkOliveGreen, Dir, Col_DarkOliveGreen,1.2f);
			}
		}
	}
}