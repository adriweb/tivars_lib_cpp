/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#pragma once

#include "../autoloader.h"

namespace tivars
{
    class ITIVarTypeHandler
    {
    public:
        static data_t makeDataFromString(const std::string& str, const options_t options);
        static std::string makeStringFromData(const data_t& data, const options_t options);
    };
}
