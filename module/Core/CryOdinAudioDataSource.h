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
		};

		struct OdinDataSource
		{
			ma_data_source_base base;
			OdinDataSourceConfig config;
		};


		static ma_result  odin_read_pcm_frames(OdinDataSource* pOdinDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
		static ma_result  odin_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
		static ma_result  odin_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex);
		static ma_result  odin_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
		static ma_result  odin_data_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor);
		static ma_result  odin_data_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength);
		static ma_result  odin_data_source_onSetLooping(ma_data_source* pDataSource, ma_bool32 isLooping);

		ma_result odin_data_source_init(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource);
		ma_result odin_data_source_uninit(OdinDataSource* pDataSource);
		ma_result odin_data_source_sound_removed(OdinDataSource* pDataSource, size_t index);

		ma_result odin_data_source_update_config(const OdinDataSourceConfig* pConfig, OdinDataSource* pDataSource);
	}
}