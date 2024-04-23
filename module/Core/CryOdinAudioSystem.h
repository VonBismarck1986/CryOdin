#pragma once

#include "Utils/OdinHelpers.h"
#include <ICryOdinAudioSystem.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioDevice;
		class CCryOdinSound;
		class CCryOdinUser;

		class CCryOdinAudioSystem final : public ICryOdinAudioSystem
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinAudioSystem)
			CCryOdinAudioSystem();
			virtual ~CCryOdinAudioSystem() = default;

			static CCryOdinAudioSystem& Get();

			virtual bool InitAudioSystem() override;
			virtual void ShutdownAudioSystem() override;
			virtual ma_engine* GetAudioEngine() override;

			virtual void CreateAudioObject(const ICryOdinUser& ref) override;
			virtual void OnUpdate(float const frameTime) override;

			void SetInputHandle(OdinMediaStreamHandle inputHandle);
			void SetLocalUser(CCryOdinUser* pUser);
		protected:
			static CCryOdinAudioSystem* s_instance;


			static void Logging(void* pUserData, ma_uint32 level, const char* pMessage);
			void LogMiniAudio(void* pUserData, ma_uint32 level, const char* pMessage);

			void UpdateLocalUserListeners(float const frameTime);
		private:
			static ma_engine m_engine;
			static ma_log m_log;
			static ma_resource_manager m_resourceManager;
			static ma_resource_manager_config m_resourceManagerConfig;

			std::unique_ptr<CCryOdinAudioDevice> m_pAudioDevice = nullptr;
			std::vector<std::unique_ptr<CCryOdinSound>> m_sounds;

			CCryOdinUser* m_pUser;
		};
	}
}