#include "StdAfx.h"
#include "CryOdinAudioDataSource.h"

namespace Cry
{
	namespace Odin
	{

		static ma_result odin_read_pcm_frames(OdinDataSource* pOdinDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
		{
			ma_uint32 framesWritten = 0;

			OdinReturnCode error;

			float* framesout = (float*)pFramesOut;

			if (frameCount == 0) {
				return MA_INVALID_ARGS;
			}

			if (pOdinDataSource == NULL) {
				return MA_INVALID_ARGS;
			}

			if (pFramesOut != NULL)
			{

				while (framesWritten < frameCount)
				{
					ma_uint32 framesToWrite = (frameCount * 2) - framesWritten;

					if (framesToWrite == 0)
						break;

					if (pOdinDataSource->config.media_handle == NULL)
						break;

					error = odin_audio_process_reverse(pOdinDataSource->config.room, framesout, framesToWrite);
					if (odin_is_error(error))
					{
						print_error(error, "Error with proccess reverse");
						break;
					}

					error = odin_audio_read_data(pOdinDataSource->config.media_handle, framesout, framesToWrite);
					if (odin_is_error(error))
					{
						print_error(error,"Error with audio read");
						break;
					}

					framesWritten += framesToWrite;
				}
			}

			if (pFramesRead != NULL) {
				*pFramesRead = framesWritten;
			}

			return MA_SUCCESS;
		}

		ma_result odin_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
		{
			return odin_read_pcm_frames((OdinDataSource*)pDataSource, pFramesOut, frameCount, pFramesRead);
		}

		ma_result odin_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
		{
			return MA_SUCCESS;
		}

		ma_result odin_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
		{
			OdinDataSource* pOdin = (OdinDataSource*)pDataSource;

			*pFormat = pOdin->config.format;
			*pChannels = pOdin->config.channels;
			*pSampleRate = 48000;  /* we will use the default rate */
			ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pOdin->config.channels);

			return MA_SUCCESS;
		}

		ma_result odin_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
		{
			return MA_SUCCESS;
		}

		ma_result odin_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
		{
			//OdinDataSource* pOdin = (OdinDataSource*)pDataSource;
			//
			//odin_audio_data_len(pOdin->config.media_handle);

			return MA_SUCCESS;
		}

		ma_result odin_data_source_onSetLooping(ma_data_source* pDataSource, ma_bool32 isLooping)
		{
			return MA_SUCCESS;
		}

		static ma_data_source_vtable g_my_data_source_vtable =
		{
			odin_data_source_read,
			NULL,
			odin_data_source_get_data_format,
			NULL,
			NULL,   /* onGetLength. There's no notion of a length. */
			NULL,   /* onSetLooping */
			0
		};

		ma_result odin_data_source_init(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource)
		{
			ma_result result;
			ma_data_source_config baseConfig;

			baseConfig = ma_data_source_config_init();
			baseConfig.vtable = &g_my_data_source_vtable;

			result = ma_data_source_init(&baseConfig, &pDataSource->base);
			if (result != MA_SUCCESS) {
				return result;
			}

			if (pConfig != NULL)
			{
				pDataSource->config = *pConfig;
			}

			return MA_SUCCESS;
		}



		ma_result odin_data_source_uninit(OdinDataSource* pDataSource)
		{
			pDataSource->config.room = 0;
			pDataSource->config.media_handle = 0;
			
			ma_data_source_uninit(&pDataSource->base);
			return MA_SUCCESS;
		}

		ma_result odin_data_source_update_config(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource)
		{
			if (pConfig != NULL)
			{
				pDataSource->config = *pConfig;
			}

			return MA_SUCCESS;
		}

	}
}