/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "../CommonTypes.h"
#include "TypeHandlers.h"

#include <stdexcept>

namespace tivars::TypeHandlers
{
    data_t DummyHandler::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)str;
        (void)options;
        (void)_ctx;
        throw std::runtime_error("This type is not supported / implemented (yet?)");
    }

    std::string DummyHandler::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)data;
        (void)options;
        (void)_ctx;
        throw std::runtime_error("This type is not supported / implemented (yet?)");
    }

    uint8_t DummyHandler::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        throw std::runtime_error("This type is not supported / implemented (yet?)");
    }
}
