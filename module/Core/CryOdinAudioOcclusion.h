#pragma once

#include <miniaudio.h>
#include "Utils/OdinHelpers.h"
#include <vector>

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryPhysics/IPhysics.h>

//TODO:: we got to raycast based on listener position and sound object(s) position based if in depth of obstruction passing info
// to sound with highpass, could high jack CryAudio Occlusion or yonk some code from it :D

namespace Cry
{
	namespace Odin
	{
		class CCryOdinSound;

		class CCryOdinOcclusion final
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinOcclusion)
			CCryOdinOcclusion() = default;
			virtual ~CCryOdinOcclusion() = default;

			void AddSound(CCryOdinSound* sound);



		private:
			std::unordered_map<int, CCryOdinSound*> m_sounds; //NOTE:: ...hmm memory leak??
		};
	}
}