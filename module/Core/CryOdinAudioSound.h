#pragma once

#include <ICryOdinAudioSound.h>
#include <CryAudio/IAudioInterfacesCommonData.h>


namespace Cry
{
	namespace Odin
	{
		struct OdinDataSource;

		class CCryOdinSound final : public ICryOdinSound
		{
		public:
			CCryOdinSound() = delete;
			explicit CCryOdinSound(ma_engine* engine, OdinDataSource* datasource, const CryAudio::CTransformation& transform, uint16_t id);
			virtual ~CCryOdinSound() = default;

			virtual ma_sound* GetMASoundHandle() override { return &m_sound; }

			virtual void StartSound() override;
			virtual void StopSound() override;

			virtual void SetSoundVolume(float fAmount) override;
			virtual float GetSoundVolume() const override;
			virtual CryAudio::CTransformation GetTransform() override { return m_transform; }
			virtual uint16_t GetSoundId() const override { return m_id; }

			void OnUpdate(float const frameTime);
			void SetEntity(IEntity* entity) { m_pEntity = entity; }
			IEntity* GetAttachedEntity() { return m_pEntity; }

			void Destory();
		private:
			static ma_sound m_sound;
			IEntity* m_pEntity = nullptr;

			CryAudio::CTransformation m_transform;
			mutable float m_volume = 1.f;

			uint16_t m_id;
		};




	}
}