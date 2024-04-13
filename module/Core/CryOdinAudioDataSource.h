#pragma once
#include <miniaudio.h>
#include <odin.h>

namespace Cry
{
	namespace Odin
	{

#define DEFAULT_ROOM_COUNT 5

		struct OdinDataSouce
		{
			ma_data_source_base base; // this is for miniaudio to allow to make a custom datasource
			ma_format format = ma_format_f32; // maps to Odin audio format 
			ma_uint32 channels = 2; // maps to speakers i.e. head phones
			ma_uint32 sampleRate = 48000; // maps to Odin sample rate
		};

		struct OdinDataSourceConfig
		{
			OdinMediaStreamHandle odin_stream[2048]; // Why 2048, cause having more then should be fine... I think
			OdinMediaStreamHandle odin_stream_ref; // This is a ref for passing to odin_stream[2048] 
			OdinRoomHandle room[DEFAULT_ROOM_COUNT]; // A player can be in more then one room right now 3
			size_t output_streams_len = 0; // This is a counter of player, so if no players connect besides main user then 0, when someone connects it goes to 1 and soon on
			uint16_t peerID; // peer id
		};

		ma_result odin_data_source_init(OdinDataSourceConfig* soundConfig, OdinDataSouce* pOdinSource);
		ma_result odin_data_source_update(OdinDataSourceConfig* soundConfig);
		ma_result odin_data_source_remove_stream(OdinDataSourceConfig* soundConfig);

		void odin_data_source_uninit(OdinDataSouce* pOdinSource);

		static ma_result cry_odin_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
		static ma_result cry_odin_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex);
		static ma_result cry_odin_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
		static ma_result cry_odin_source_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor);
		static ma_result cry_odin_source_get_length(ma_data_source* pDataSource, ma_uint64* pLength);

	}
}