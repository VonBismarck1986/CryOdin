#pragma once

#include <ICryOdinAudioSystem.h>
#include <ICryOdinAudioDevices.h>
#include <ICryOdinAudioSound.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinAudioDevice;
		class CCryOdinAudioSound;

		class CCryOdinAudioSystem final : public ICryOdinAudioSystem
		{
		public:
			CCryOdinAudioSystem();
			virtual ~CCryOdinAudioSystem() = default;

			static CCryOdinAudioSystem& GetAudioSystem() { return *s_AudioSystem; }

			virtual bool Init() override;
			virtual void Shutdown() override;
			virtual void OnUpdate(float frameTime) override;

			virtual ma_engine* GetAudioEngine() override;
			virtual SCryOdinAudioDevicesConfig GetAudioDeviceConfig() const override;
			virtual void CreateSoundObject(const IUser& user) override;

			void AddLocalPlayer(const IUser& user);
			CCryOdinAudioSound* CreateSound(const IUser& user);

			void InitSound(CCryOdinAudioSound* sound);
		protected:
			static CCryOdinAudioSystem* s_AudioSystem;

			void UpdateListener(float const frameTime);
			
		private:
			IUser m_user;
			std::vector<IUser> m_peers;
		};
	}
}