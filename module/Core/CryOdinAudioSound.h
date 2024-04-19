#pragma once

#include <miniaudio.h>
#include <odin.h>
#include <ICryOdinAudioDevices.h>

namespace Cry
{
	namespace Odin
	{
		class CCryAudioSound final
		{
		public:
			CCryAudioSound() = delete;
			CCryAudioSound(CCryAudioSound const&) = delete;
			CCryAudioSound(CCryAudioSound&&) = delete;
			CCryAudioSound& operator=(CCryAudioSound const&) = delete;
			CCryAudioSound& operator=(CCryAudioSound&&) = delete;

			explicit CCryAudioSound(const ma_sound& sound, int id)
				: m_sound(sound)
				, m_id(id)
			{}

			void SetVolume(float fAmount) { ma_sound_set_volume(&m_sound, fAmount); }
			float GetVolume() const { return ma_sound_get_volume(&m_sound); }
			ma_sound GetSound() const { return m_sound; }

			Vec3 GetSoundPosition() const { return m_soundPosition; }
			void SetSoundPosition(Vec3 position) { m_soundPosition = position; }

			ma_sound m_sound;
		private:
			int m_id;
			Vec3 m_soundPosition{ ZERO };
		};
	}
}