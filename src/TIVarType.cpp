/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TIVarType.h"
#include "TIVarTypes.h"

#include <stdexcept>

namespace tivars
{
    /*** "Constructors" ***/

    TIVarType::TIVarType(uint8_t id)
    {
        if (TIVarTypes::isValidID(id))
        {
            *this = TIVarTypes::fromId(id);
        } else {
            throw std::invalid_argument("Invalid type ID");
        }
    }

    TIVarType::TIVarType(const std::string& name)
    {
        if (TIVarTypes::isValidName(name))
        {
            *this = TIVarTypes::fromName(name);
        } else {
            throw std::invalid_argument("Invalid type name");
        }
    }
}

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    using namespace emscripten;
    EMSCRIPTEN_BINDINGS(_tivartype) {
            class_<tivars::TIVarType>("TIVarType")
                    .constructor<>()
                    .constructor<const char*>()
                    .constructor<int, const std::string&, const std::vector<std::string>&, const tivars::handler_pair_t&>()

                    .function("getId"      , &tivars::TIVarType::getId)
                    .function("getName"    , &tivars::TIVarType::getName)
                    .function("getExts"    , &tivars::TIVarType::getExts)
                    .function("getHandlers", &tivars::TIVarType::getHandlers)

                    .class_function("createFromID",   &tivars::TIVarType::createFromID)
                    .class_function("createFromName", &tivars::TIVarType::createFromName)
            ;
    }
#endif
