#pragma once

#include <CryCore/BaseTypes.h>
#include <memory>

namespace Cry
{
	namespace Odin
	{
		//! Your own audio effect. Add it to a voice with ICryOdinSound::AddEffect to change how one
		//! user is heard, or to a room with ICryOdinRoom::AddCaptureEffect to change the local microphone.
		//!
		//! Process runs on a real time audio thread. Don't allocate, block, log or use engine systems
		//! in it. Pass values to it with atomics.
		struct ICryOdinAudioEffect
		{
			virtual ~ICryOdinAudioEffect() {}

			//! @param pFrames Interleaved 32 bit float samples, changed in place.
			//! @param frameCount Number of frames. It varies between calls.
			//! @param channelCount Always 2 at the moment.
			//! @param sampleRate Always 48000 at the moment.
			//! @param bSilent True when the source has nothing to play.
			virtual void Process(float* pFrames, uint32 frameCount, uint32 channelCount, uint32 sampleRate, bool bSilent) = 0;
		};

		using ICryOdinAudioEffectPtr = std::shared_ptr<ICryOdinAudioEffect>;
	}
}
