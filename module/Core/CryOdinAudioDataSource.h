#pragma once

#include <miniaudio.h>
#include <odin.h>

namespace Cry
{
	namespace Odin
	{

		struct OdinDataSourceConfig
		{
			OdinMediaStreamHandle media_handle = 0;
			ma_format format = ma_format_f32;
			ma_uint32 channels = 2;
			OdinRoomHandle room;

			bool usingNode = false;
			ma_node_config n_config;
		};

		struct OdinDataSource
		{
			ma_data_source_base base;
			OdinDataSourceConfig config;
		};

		struct OdinDataSourceNode
		{
			ma_data_source_node node;

			OdinDataSource odin;
			OdinDataSourceConfig config;
		};

		static void onProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut);
		static ma_result onGetRequiredInputFrameCount(ma_node* pNode, ma_uint32 outputFrameCount, ma_uint32* pInputFrameCount) {} //TODO:: create



		static ma_result  odin_read_pcm_frames(OdinDataSource* pOdinDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
		static ma_result  odin_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
		static ma_result  odin_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex);
		static ma_result  onGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);

		ma_result odin_data_source_init(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource);
		ma_result odin_data_source_uninit(OdinDataSource* pDataSource);
		ma_result odin_data_source_update_config(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource);


		ma_result odin_data_node_init(const ma_node_config* pConfig, OdinDataSource* pDataSource);

		ma_result odin_data_node_init_ex(ma_node_graph* nodeGraph, const OdinDataSourceConfig* pConfig, OdinDataSourceNode* pDataSource);










#define LPF_BIAS            0.9f    /* Higher values means more bias towards the low pass filter (the low pass filter will be more audible). Lower values means more bias towards the echo. Must be between 0 and 1. */
#define LPF_CUTOFF_FACTOR   80      /* High values = more filter. */
#define LPF_ORDER           8


		struct OdinEffectNoiseNode
		{
			ma_data_source_node node;
			ma_noise noise;
		};

		struct OdinEffectLowPass
		{
			ma_data_source_node node;
			ma_lpf1 lowpass;
		};

		struct OdinEffectHightPass
		{
			ma_data_source_node node;
			ma_hpf1 highpass;
		};

	}
}