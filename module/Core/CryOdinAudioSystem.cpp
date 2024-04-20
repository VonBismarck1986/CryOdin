#include "StdAfx.h"
#include "CryOdinAudioSystem.h"

#include "CryOdin.h"

#include "CryOdinAudioSound.h"
#include "CryOdinAudioOcclusion.h"
#include "CryOdinAudioDevices.h"
#include "CryOdinAudioDataSource.h"

#include "Utils/MiniAudioHelpers.h"

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
			ma_engine_listener_set_direction(&g_engine, 0, 0.0f, 1.0f, 0.0f);
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
			CCryOdinAudioSound* sound = new CCryOdinAudioSound(user, CryAudio::CTransformation::GetEmptyObject());
			if (sound)
			{
				if (g_pAudiodevices)
				{
					g_pAudiodevices->GetDevice().SoundCreated(&g_engine, sound);
				}
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

			auto dir = m_user.m_pEntity->GetForwardDir();
			auto pos = m_user.m_pEntity->GetWorldPos();

			ma_engine_listener_set_direction(&g_engine, 0, dir.x, dir.y, dir.z);
			ma_engine_listener_set_position(&g_engine, 0, pos.x, pos.y, pos.z);
		}
	}
}