#pragma once

// The engine is built without exceptions, nlohmann would abort the process where it normally throws.
// So only use json::parse with allow_exceptions = false, and only read values through these helpers.
#ifndef JSON_NOEXCEPTION
	#define JSON_NOEXCEPTION
#endif

#pragma warning( push )
#pragma warning( disable : 4244 4267)
#include <nlohmann/json.hpp>
#pragma warning( pop )

#include <string>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		using OdinJson = nlohmann::json;

		// Returns a discarded value on invalid text, check with is_discarded.
		inline OdinJson OdinJsonParse(const char* szText)
		{
			return OdinJson::parse(szText, nullptr, false);
		}

		// dump with error replacement, invalid UTF-8 would otherwise abort the process.
		inline std::string OdinJsonDump(const OdinJson& value)
		{
			return value.dump(-1, ' ', false, OdinJson::error_handler_t::replace);
		}

		inline const OdinJson* OdinJsonFind(const OdinJson& object, const char* szKey)
		{
			if (!object.is_object())
			{
				return nullptr;
			}
			const auto it = object.find(szKey);
			return it != object.end() ? &(*it) : nullptr;
		}

		inline std::string OdinJsonString(const OdinJson& object, const char* szKey, const char* szDefault = "")
		{
			const OdinJson* pValue = OdinJsonFind(object, szKey);
			return (pValue && pValue->is_string()) ? pValue->get_ref<const std::string&>() : std::string(szDefault);
		}

		inline uint32 OdinJsonUInt32(const OdinJson& object, const char* szKey, uint32 defaultValue = 0)
		{
			const OdinJson* pValue = OdinJsonFind(object, szKey);
			return (pValue && pValue->is_number_unsigned()) ? pValue->get<uint32>() : defaultValue;
		}

		// Reads an array of numbers from 0 to 255. Returns false if it's anything else.
		inline bool OdinJsonBytes(const OdinJson& value, std::vector<uint8>& outBytes)
		{
			if (!value.is_array())
			{
				return false;
			}

			outBytes.clear();
			outBytes.reserve(value.size());
			for (const OdinJson& item : value)
			{
				if (!item.is_number_unsigned())
				{
					return false;
				}
				const uint64 byteValue = item.get<uint64>();
				if (byteValue > 255)
				{
					return false;
				}
				outBytes.push_back(static_cast<uint8>(byteValue));
			}
			return true;
		}

		inline OdinJson OdinJsonFromBytes(const void* pData, uint32 size)
		{
			OdinJson bytes = OdinJson::array();
			OdinJson::array_t& array = bytes.get_ref<OdinJson::array_t&>();
			array.reserve(size);
			const uint8* pBytes = static_cast<const uint8*>(pData);
			for (uint32 i = 0; i < size; ++i)
			{
				array.emplace_back(static_cast<uint64>(pBytes[i]));
			}
			return bytes;
		}
	}
}
