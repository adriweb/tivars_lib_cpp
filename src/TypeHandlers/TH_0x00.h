/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TH_0x00_H
#define TH_0x00_H

#include "ITIVarTypeHandler.h"

namespace tivars
{

    // Type Handler for type 0x00: Real
    class TH_0x00 : public ITIVarTypeHandler
    {
    public:

        static std::vector<uint> makeDataFromString(const std::string& str, const options_t options);

        static std::string makeStringFromData(const std::vector<uint>& data, const options_t options);

    };
}

#endif