#include "StdAfx.h"
#include "CryOdinAudioDataSource.h"

namespace Cry
{
	namespace Odin
	{
		void onProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
		{

		}

		static ma_result odin_read_pcm_frames(OdinDataSource* pOdinDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
		{
			
			if (frameCount == 0) {
				return MA_INVALID_ARGS;
			}

			if (pOdinDataSource == NULL) {
				return MA_INVALID_ARGS;
			}

			ma_uint32 framesWritten = 0;
			OdinReturnCode error;
			float* framesout = (float*)pFramesOut;

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

		ma_result onGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
		{
			OdinDataSource* pOdin = (OdinDataSource*)pDataSource;

			*pFormat = pOdin->config.format;
			*pChannels = pOdin->config.channels;
			*pSampleRate = 48000;  /* we will use the default rate */
			ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, pOdin->config.channels);

			return MA_SUCCESS;
		}

		static ma_data_source_vtable g_my_data_source_vtable =
		{
			odin_data_source_read,
			NULL,
			onGetDataFormat,
			NULL,
			NULL,   /* onGetLength. There's no notion of a length. */
			NULL,   /* onSetLooping */
			0
		};


		static ma_node_vtable g_odin_node_vtable = 
		{
			onProcess,
			NULL,
			1,
			1,
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

		ma_result odin_data_node_init(const ma_node_config* pConfig, OdinDataSource* pDataSource)
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
				pDataSource->config.n_config = *pConfig;
			}

			return MA_SUCCESS;
		}

		ma_result odin_data_node_init_ex(ma_node_graph* nodeGraph, const OdinDataSourceConfig* pConfig, OdinDataSourceNode* pDataSource)
		{
			if (pConfig->usingNode == true)
			{
				ma_result result;

				pDataSource->config = *pConfig;

				ma_uint32 inputChannels[1];     // Equal in size to the number of input channels specified in the vtable.

				ma_uint32 outputChannels[1];    // Equal in size to the number of output channels specified in the vtable.

				outputChannels[0] = pDataSource->config.channels;
				inputChannels[0] = pDataSource->config.channels;

				ma_node_config nodeConfig = ma_node_config_init();
				nodeConfig.vtable = &g_odin_node_vtable;
				nodeConfig.pInputChannels = inputChannels;
				nodeConfig.pOutputChannels = outputChannels;

				pDataSource->config.n_config = nodeConfig;

				result = ma_node_init(nodeGraph, &nodeConfig, NULL, &pDataSource->node);
				return result;
			}

			return MA_INVALID_ARGS;
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