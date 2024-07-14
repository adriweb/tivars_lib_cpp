/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
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

        static void initTIVarTypesArray();
        static const std::unordered_map<std::string, TIVarType>& all();

        static bool isValidName(const std::string& name);
        static bool isValidID(uint8_t id);

    private:
        static void insertType(const std::string& name, int id, const std::vector<std::string>& exts, const TypeHandlers::handler_pair_t& handlers = make_handler_pair(TypeHandlers::DummyHandler));

    };
}

#endif //TIVARTYPES_H
