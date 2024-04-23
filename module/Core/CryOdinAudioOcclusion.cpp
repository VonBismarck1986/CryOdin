#include "StdAfx.h"
#include "CryOdinAudioOcclusion.h"

#include "CryOdinAudioSound.h"


namespace Cry
{
	namespace Odin
	{

		void CCryOdinOcclusion::AddSound(CCryOdinSound* sound)
		{
			m_sounds.emplace(std::make_pair(sound->GetSoundId(), sound));
		}




	}
}