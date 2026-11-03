#include "StdAfx.h"
#include "CryOdinAudioDataSource.h"

#include "CryOdinAudioSound.h"
#include "Utils/OdinHelpers.h"

namespace Cry
{
	namespace Odin
	{
		static ma_result odin_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
		{
			SCryOdinDataSource* pOdin = static_cast<SCryOdinDataSource*>(pDataSource);

			if (pFramesOut != nullptr && frameCount > 0)
			{
				// Odin fills the whole buffer, with zeros if there's nothing to play.
				float* pSamples = static_cast<float*>(pFramesOut);
				bool bSilent = true;
				if (odin_decoder_pop(pOdin->pDecoder, pSamples, static_cast<uint32_t>(frameCount), &bSilent) != ODIN_ERROR_SUCCESS)
				{
					memset(pSamples, 0, static_cast<size_t>(frameCount) * sizeof(float));
					bSilent = true;
				}
				pOdin->bSilent.store(bSilent, std::memory_order_relaxed);

				pOdin->pSound->ProcessSource(pSamples, static_cast<uint32>(frameCount), bSilent);
			}

			// Always report a full read. Reading fewer frames ends the sound.
			if (pFramesRead != nullptr)
			{
				*pFramesRead = frameCount;
			}
			return MA_SUCCESS;
		}

		static ma_result odin_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
		{
			return MA_SUCCESS;
		}

		static ma_result odin_data_source_get_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
		{
			*pFormat = ma_format_f32;
			*pChannels = 1;
			*pSampleRate = kOdinSampleRate;

			// miniaudio often passes no channel map here
			if (pChannelMap != nullptr && channelMapCap > 0)
			{
				ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, 1);
			}
			return MA_SUCCESS;
		}

		static ma_data_source_vtable g_odinDataSourceVtable =
		{
			odin_data_source_read,
			odin_data_source_seek,
			odin_data_source_get_format,
			nullptr, // onGetCursor
			nullptr, // onGetLength. There's no notion of a length.
			nullptr, // onSetLooping
			0
		};

		ma_result odin_data_source_init(OdinDecoder* pDecoder, CCryOdinSound* pSound, SCryOdinDataSource* pDataSource)
		{
			ma_data_source_config baseConfig = ma_data_source_config_init();
			baseConfig.vtable = &g_odinDataSourceVtable;

			pDataSource->pDecoder = pDecoder;
			pDataSource->pSound = pSound;
			return ma_data_source_init(&baseConfig, &pDataSource->base);
		}

		void odin_data_source_uninit(SCryOdinDataSource* pDataSource)
		{
			ma_data_source_uninit(&pDataSource->base);
			pDataSource->pDecoder = nullptr;
			pDataSource->pSound = nullptr;
		}
	}
}
