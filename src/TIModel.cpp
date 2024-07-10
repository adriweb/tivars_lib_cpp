/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/TIs_lib
 * License: MIT
 */

#include "TIModel.h"
#include "TIModels.h"

#include <stdexcept>

namespace tivars
{

    bool TIModel::supportsType(const TIVarType& type) const
    {
        const std::vector<std::string>& exts = type.getExts();
        return this->orderID >= 0 && this->orderID < (int)exts.size() && !exts[this->orderID].empty();
    }

    /*** "Constructors" ***/

    TIModel::TIModel(const std::string& name)
    {
        if (TIModels::isValidName(name))
        {
            *this = TIModels::fromName(name);
        } else
        {
            throw std::invalid_argument("Invalid model name");
        }
    }
}

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    using namespace emscripten;
    EMSCRIPTEN_BINDINGS(_timodel) {
            class_<tivars::TIModel>("TIModel")
                    .constructor<>()
                    .constructor<const char*>()
                    .constructor<int, const std::string&, uint32_t, const std::string&, uint8_t>()

                    .function("getOrderId"  , &tivars::TIModel::getOrderId)
                    .function("getProductId", &tivars::TIModel::getProductId)
                    .function("getName"     , &tivars::TIModel::getName)
                    .function("getFlags"    , &tivars::TIModel::getFlags)
                    .function("getSig"      , &tivars::TIModel::getSig)
                    .function("supportsType", &tivars::TIModel::supportsType)
            ;
    }
#endif
