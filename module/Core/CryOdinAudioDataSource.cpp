#include "StdAfx.h"
#include "CryOdinAudioDataSource.h"
#include "ICryOdinAudioDevices.h"

namespace Cry
{
	namespace Odin
	{
		OdinDataSourceConfig g_mSourceConfig; // Main handle for all audio comming in from Odin

		static ma_result cry_odin_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
		{

			if (g_mSourceConfig.output_streams_len)
			{
				ODIN_LOG("stream length: %d\n", g_mSourceConfig.output_streams_len);
				
				int sample_count = frameCount * 2;
				odin_audio_mix_streams(g_mSourceConfig.room[0], g_mSourceConfig.odin_stream, g_mSourceConfig.output_streams_len, (float*)pFramesOut, sample_count);
				return MA_SUCCESS;
			}

			return MA_SUCCESS;
		}

		static ma_result cry_odin_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
		{
			// Seek to a specific PCM frame here. Return MA_NOT_IMPLEMENTED if seeking is not supported.
			return MA_SUCCESS;
		}

		static ma_result cry_odin_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
		{
			// Return the format of the data here.
			return MA_SUCCESS;
		}

		static ma_result cry_odin_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
		{
			// Retrieve the current position of the cursor here. Return MA_NOT_IMPLEMENTED and set *pCursor to 0 if there is no notion of a cursor.
			return MA_SUCCESS;
		}

		static ma_result cry_odin_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
		{
			// Retrieve the length in PCM frames here. Return MA_NOT_IMPLEMENTED and set *pLength to 0 if there is no notion of a length or if the length is unknown.
			return MA_SUCCESS;
		}

		static ma_data_source_vtable g_my_data_source_vtable =
		{
			cry_odin_source_read,
			cry_odin_source_seek,
			cry_odin_source_get_data_format,
			cry_odin_source_get_cursor,
			cry_odin_source_get_length
		};

		ma_result odin_data_source_init(OdinDataSourceConfig* soundConfig, OdinDataSouce* pOdinSource)
		{
			ma_result result;
			ma_data_source_config baseConfig;

			baseConfig = ma_data_source_config_init();
			baseConfig.vtable = &g_my_data_source_vtable;

			result = ma_data_source_init(&baseConfig, &pOdinSource->base);
			if (result != MA_SUCCESS) {
				return result;
			}

			g_mSourceConfig.odin_stream[g_mSourceConfig.output_streams_len] = soundConfig->odin_stream_ref;
			g_mSourceConfig.output_streams_len += 1;


			return MA_SUCCESS;
		}

		ma_result Cry::Odin::odin_data_source_update(OdinDataSourceConfig* soundConfig)
		{
			g_mSourceConfig.odin_stream[g_mSourceConfig.output_streams_len] = soundConfig->odin_stream_ref;
			g_mSourceConfig.output_streams_len += 1;
			return MA_SUCCESS;
		}

		ma_result Cry::Odin::odin_data_source_remove_stream(OdinDataSourceConfig* soundConfig)
		{
			g_mSourceConfig.output_streams_len -= 1;
			g_mSourceConfig.odin_stream[soundConfig->peerID] = g_mSourceConfig.odin_stream[g_mSourceConfig.output_streams_len];
			g_mSourceConfig.odin_stream[g_mSourceConfig.output_streams_len] = 0;

			return MA_SUCCESS;
		}

		void odin_data_source_uninit(OdinDataSouce* pOdinSource)
		{
			ma_data_source_uninit(&pOdinSource->base);
		}


	}
}