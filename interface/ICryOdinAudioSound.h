#pragma once

#include <miniaudio.h>
#include <odin.h>

namespace Cry
{
	namespace Odin
	{
		struct ICryOdinSound
		{
			virtual ~ICryOdinSound() {}

			virtual ma_sound* GetMASoundHandle() = 0;

			virtual void StartSound() = 0;
			virtual void StopSound() = 0;
			virtual void SetSoundVolume(float fAmount) = 0;
			virtual float GetSoundVolume() const = 0;

			virtual CryAudio::CTransformation GetTransform() = 0;
		};
	}
}