#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <Core/CryOdinAudioDataSource.h>
#include <ICryOdinAudioSound.h>

namespace Cry
{
	namespace Odin
	{
		// from FMOD Flags
		enum class EObjectFlags : CryAudio::EnumFlagsType
		{
			None = 0,
			MovingOrDecaying = BIT(0),
			TrackAbsoluteVelocity = BIT(1),
			TrackVelocityForDoppler = BIT(2),
			UpdateVirtualStates = BIT(3),
			IsVirtual = BIT(4),
		};
		CRY_CREATE_ENUM_FLAG_OPERATORS(EObjectFlags);

		struct OdinDataSource;

		class CCryOdinComponentSound final : public IEntityComponent
		{
		public:
			CCryOdinComponentSound() = default;
			virtual ~CCryOdinComponentSound() = default;

			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;

			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinComponentSound>& desc)
			{
				desc.SetGUID("{0DB50E3F-243A-4732-B582-3F143841667C}"_cry_guid);
				desc.SetLabel("OdinComponentSound");
				desc.SetEditorCategory("Odin");
			}

			void MutePlayer();
			bool IsPlayerMuted() const { return m_bMuted; }

			void SetVolume(float fAmount);
			float GetVolume() const { return m_fVolume; }

			bool IsTalking() const { return m_bTalking; }

		protected:
			ma_sound* GetMASound() { return &m_sound; }

			void CreateSoundInstance(ma_engine* engine, OdinDataSource* dataSource, const CryAudio::CTransformation& transform, uint16_t id);
			void UpdateSoundPosition(const CryAudio::CTransformation& transform);
			void UpdateSoundVelocity(float const fFrameTime);

			void ToggleTalking() const { m_bTalking ? true : false; }

		protected:
			ma_sound m_sound;
			OdinDataSource* m_pDataSource = nullptr;

			CryAudio::CTransformation m_transform = CryAudio::CTransformation::GetEmptyObject(); // since this is in a component propbably not needed
			float m_fVolume = 1.f;

			EObjectFlags m_flags;

			bool m_bGameStarted = false;
			bool m_bMuted = false;
			bool m_bTalking = false;
			float m_absoluteVelocity = 0.0f;
			float m_previousAbsoluteVelocity = 0.0f;
			Vec3 m_position = ZERO;
			Vec3 m_previousPosition = ZERO;
			Vec3 m_velocity = ZERO;
		};
	}
}