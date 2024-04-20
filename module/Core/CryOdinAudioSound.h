#pragma once

#include <ICryOdinAudioSound.h>

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryAction.h>
#include <ICryOdinUser.h>

namespace Cry
{
	namespace Odin
	{
		struct OdinDataSource;

		class CCryOdinAudioSound final : public ICryOdinAudioSound
		{
		public:
			CCryOdinAudioSound() = delete;
			virtual ~CCryOdinAudioSound() = default;

			explicit CCryOdinAudioSound(const IUser& user, CryAudio::CTransformation const& tranform)
				: m_user(user)
				, m_transform(tranform)
				, m_sound()
			{
				ma_sound sound{};
				m_sound = sound;
			}


			virtual ma_sound* GetSound() override;
			virtual void SetSoundVolume(float fAmount) override;
			virtual float GetSoundVolume() const override;
			virtual void OnUpdate(float const fFrameTime) override;
			virtual OdinMediaStreamHandle GetMediaHandle() const override { return m_user.mediaStream; }
			virtual uint64_t GetSoundID() const override { return m_user.peerID; }

			virtual void InitSound(ma_engine* engine, OdinDataSource* pDataSource) override;

			Vec3 GetSoundPosition() const;

			virtual void PlaySound() override;
			virtual void StopSound() override;
		private:
			ma_sound m_sound{};
			IUser m_user;
			CryAudio::CTransformation const m_transform;
		};
	}
}