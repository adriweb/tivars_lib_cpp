/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARTYPES_H
#define TIVARTYPES_H

#include "CommonTypes.h"
#include "TIVarType.h"
#include <unordered_map>

namespace tivars
{
    class TIVarTypes
    {

    public:
        static TIVarType fromName(const std::string& name);
        static TIVarType fromId(uint8_t id);

        static const std::unordered_map<std::string, TIVarType>& all();

        static bool isValidName(const std::string& name);
        static bool isValidID(uint8_t id);

    };
}

#endif //TIVARTYPES_H
