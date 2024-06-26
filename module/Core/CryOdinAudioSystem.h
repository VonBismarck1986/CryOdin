#pragma once

#include "Utils/OdinHelpers.h"
#include <ICryOdinAudioSystem.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinOcclusion;
		class CCryOdinAudioDevice;
		class CCryOdinSound;
		class CCryOdinUser;
		class CCryOdinComponentSound;

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

			virtual void CreateAudioObject(const CCryOdinUserComponent& ref) override;
			virtual CCryOdinComponentSound* CreateAudioComponent(const CCryOdinUserComponent& ref) override;

			virtual void DestroyAudioObject(const CCryOdinUserComponent& ref) override;

			virtual void OnUpdate(float const frameTime) override;

			void SetInputHandle(OdinMediaStreamHandle inputHandle);
			void SetLocalUser(CCryOdinUserComponent* odinComponent);

			CCryOdinAudioDevice* GetAudioDevices() { return m_pAudioDevice.get(); }
		protected:
			static CCryOdinAudioSystem* s_instance;

			static void Logging(void* pUserData, ma_uint32 level, const char* pMessage);
			void UpdateLocalUserListeners(float const frameTime);
		private:
			static ma_engine m_engine;
			static ma_log m_log;
			static ma_resource_manager m_resourceManager;
			static ma_resource_manager_config m_resourceManagerConfig;

			std::unique_ptr<CCryOdinAudioDevice> m_pAudioDevice = nullptr;
			std::unique_ptr<CCryOdinOcclusion> m_pOcclusion = nullptr;

			std::vector<std::unique_ptr<CCryOdinSound>> m_sounds;

			std::unordered_map<uint16_t,std::shared_ptr<CCryOdinComponentSound>> m_componentSounds;

			CCryOdinUserComponent* m_pUser = nullptr;
		};
	}
}