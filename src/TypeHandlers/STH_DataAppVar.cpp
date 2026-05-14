/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"

#include <stdexcept>

namespace tivars::TypeHandlers
{
    data_t STH_DataAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        if (str.empty())
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid hex data block");
        }

        data_t payload;
        try
        {
            payload = hex_string_to_bytes(str, "input string");
        }
        catch (const std::invalid_argument&)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid hex data block");
        }

        const size_t bytes = payload.size();
        if (bytes > 0xFFFF)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid hex data block");
        }

        data_t data = { (uint8_t)(bytes & 0xFF), (uint8_t)((bytes >> 8) & 0xFF) };
        vector_append(data, payload);
        return data;
    }

    std::string STH_DataAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const size_t byteCount = data.size();
        if (byteCount < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes");
        }

        const size_t lengthExp = (size_t) ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        const size_t lengthDat = byteCount - 2;

        if (lengthExp != lengthDat)
        {
            throw std::invalid_argument("Invalid data array. Expected " + std::to_string(lengthExp) + " bytes, got " + std::to_string(lengthDat));
        }

        std::string str;

        for (size_t i=2; i<byteCount; i++)
        {
            str += dechex(data[i]);
        }

        return str;
    }

    TIVarFileMinVersionByte STH_DataAppVar::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return VER_NONE;
    }
}
