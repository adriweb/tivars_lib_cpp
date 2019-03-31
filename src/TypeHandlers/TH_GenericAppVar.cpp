/*
 * Part of tivars_lib_cpp
 * (C) 2015-2019 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../utils.h"

#include <cstring>

namespace tivars
{
    data_t TH_GenericAppVar::makeDataFromString(const std::string& str, const options_t& options)
    {
        return STH_DataAppVar::makeDataFromString(str, options);
    }

    std::string TH_GenericAppVar::makeStringFromData(const data_t& data, const options_t& options)
    {
        if (data.size() < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes");
        }


        return STH_DataAppVar::makeStringFromData(data, options);
    }
}
