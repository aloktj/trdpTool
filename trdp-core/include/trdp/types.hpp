#pragma once

#include <cstdint>
#include <string>

namespace trdp
{

enum class TrdpType
{
    BOOL1,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    INT32,
    INT64,
    REAL32,
    REAL64,
    STRING,
    BYTES
};

inline std::string toString(TrdpType type)
{
    switch (type)
    {
    case TrdpType::BOOL1:
        return "BOOL1";
    case TrdpType::UINT8:
        return "UINT8";
    case TrdpType::UINT16:
        return "UINT16";
    case TrdpType::UINT32:
        return "UINT32";
    case TrdpType::UINT64:
        return "UINT64";
    case TrdpType::INT32:
        return "INT32";
    case TrdpType::INT64:
        return "INT64";
    case TrdpType::REAL32:
        return "REAL32";
    case TrdpType::REAL64:
        return "REAL64";
    case TrdpType::STRING:
        return "STRING";
    case TrdpType::BYTES:
        return "BYTES";
    }
    return "UNKNOWN";
}

inline std::size_t defaultElementSize(TrdpType type)
{
    switch (type)
    {
    case TrdpType::BOOL1:
    case TrdpType::UINT8:
        return 1;
    case TrdpType::UINT16:
        return 2;
    case TrdpType::UINT32:
    case TrdpType::INT32:
    case TrdpType::REAL32:
        return 4;
    case TrdpType::UINT64:
    case TrdpType::INT64:
    case TrdpType::REAL64:
        return 8;
    case TrdpType::STRING:
    case TrdpType::BYTES:
        return 0; // dynamic
    }
    return 0;
}

} // namespace trdp

