/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include <stdexcept>
#include <cstring>

// TODO: handle filename stuff
// TODO: handle UTF8 BOM stuff
// TODO: handle weird chars stuff
// TODO: handle full metadata format

namespace tivars::TypeHandlers
{
    data_t STH_PythonAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        const size_t length = str.size() + 4 + 1;

        if (length > 65490) // todo : compute actual max size
        {
            throw std::invalid_argument("Invalid input string. Too big?");
        }

        data_t data(2 + length);
        data[0] = (uint8_t)(length & 0xFF);
        data[1] = (uint8_t)((length >> 8) & 0xFF);
        memcpy(&data[2], &ID_CODE, sizeof(ID_CODE));
        memcpy(&data[2+sizeof(ID_CODE)], &str[0], str.size());

        return data;
    }

    std::string STH_PythonAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        const size_t byteCount = data.size();
        const size_t lengthDat = byteCount - 2;

        if (byteCount < 2 + sizeof(ID_CODE))
        {
            throw std::invalid_argument("Invalid data array. Need at least 6 bytes, got " + std::to_string(lengthDat));
        }

        const size_t lengthExp = (size_t) ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));

        if (lengthExp != lengthDat)
        {
            throw std::invalid_argument("Invalid data array. Expected " + std::to_string(lengthExp) + " bytes, got " + std::to_string(lengthDat));
        }

        if (memcmp(ID_CODE,   &(data[2]), strlen(ID_CODE))   != 0
         && memcmp(ID_SCRIPT, &(data[2]), strlen(ID_SCRIPT)) != 0)
        {
            throw std::invalid_argument("Invalid data array. Magic header 'PYCD' or 'PYSC' not found");
        }

        // We skip the file name field if it's present
        // data[6] indicates its len. If it's > 0, there is an extra byte (always == 1 ?).
        const size_t scriptOffset = data[6] + 1u;

        return std::string(data.begin() + 6 + scriptOffset, data.end());
    }
}
