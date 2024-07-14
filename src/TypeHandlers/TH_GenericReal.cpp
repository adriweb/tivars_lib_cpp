/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include <stdexcept>
#include <unordered_map>

namespace tivars::TypeHandlers
{
    static const std::unordered_map<uint8_t, TypeHandlersTuple> type2handlers = {
        { 0x00, SpecificHandlerTuple(STH_FP) },
        { 0x18, SpecificHandlerTuple(STH_ExactFraction) },
        { 0x1C, SpecificHandlerTuple(STH_ExactRadical) },
        { 0x20, SpecificHandlerTuple(STH_ExactPi) },
        { 0x21, SpecificHandlerTuple(STH_ExactFractionPi) },
    };

    // TODO: guess, by parsing, the type instead of reading it from the options
    data_t TH_GenericReal::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericReal::makeDataFromString");
        }
        const uint8_t type = (uint8_t)typeIter->second;
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericReal: " + std::to_string(type));
        }
        return std::get<0>(handlerIter->second)(str, options, _ctx);
    }

    std::string TH_GenericReal::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }
        const uint8_t type = (uint8_t)(data[0] & 0x7F);
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type in this TH_GenericReal data: " + std::to_string(type));
        }
        return std::get<1>(handlerIter->second)(data, options, _ctx);
    }

    uint8_t TH_GenericReal::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
