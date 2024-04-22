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
				, m_room(0)
			{
			}

			virtual ma_sound* GetSound() override;
			virtual void SetSoundVolume(float fAmount) override;
			virtual float GetSoundVolume() const override;
			virtual void OnUpdate(float const fFrameTime) override;
			virtual OdinMediaStreamHandle GetMediaHandle() const override { return m_user.mediaStream; }
			virtual void SetMediaHandle(OdinMediaStreamHandle handleID) override { m_user.mediaStream = handleID; }

			virtual void SetRoomHandle(OdinRoomHandle room) override { m_room = room; }
			virtual OdinRoomHandle GetRoomHandle() override { return m_room; }

			virtual uint64_t GetSoundID() const override { return m_user.peerID; }

			virtual void InitSound(ma_engine* engine, OdinDataSource* pDataSource) override;

			Vec3 GetSoundPosition() const;

			virtual void PlaySound() override;
			virtual void StopSound() override;

		protected:
			void DebugDraw(float const frameTime);

		private:
			OdinRoomHandle m_room;
			IUser m_user;
			CryAudio::CTransformation m_transform;
		};
	}
}