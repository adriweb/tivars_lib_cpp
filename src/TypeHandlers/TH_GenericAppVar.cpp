/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include <stdexcept>
#include <cstring>

namespace tivars::TypeHandlers
{
    data_t TH_GenericAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        return STH_DataAppVar::makeDataFromString(str, options);
    }

    std::string TH_GenericAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        if (data.size() < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes");
        }

        if (data.size() >= 2 + sizeof(STH_PythonAppVar::ID_CODE)
            && memcmp(STH_PythonAppVar::ID_CODE, &(data[2]), strlen(STH_PythonAppVar::ID_CODE)) == 0)
        {
            try {
                return STH_PythonAppVar::makeStringFromData(data, options);
            } catch (...) {} // "fallthrough"
        }

        return STH_DataAppVar::makeStringFromData(data, options);
    }

    uint8_t TH_GenericAppVar::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
