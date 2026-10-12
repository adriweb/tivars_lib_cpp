/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace tivars::TypeHandlers
{
    static const std::unordered_map<uint8_t, TypeHandlersTuple> type2handlers = {
        { 0x00, SpecificHandlerTuple(STH_FP) },
        { 0x0E, SpecificHandlerTuple(STH_FP) },
        { 0x18, SpecificHandlerTuple(STH_ExactFraction) },
        { 0x1C, SpecificHandlerTuple(STH_ExactRadical) },
        { 0x20, SpecificHandlerTuple(STH_ExactPi) },
        { 0x21, SpecificHandlerTuple(STH_ExactFractionPi) },
    };

    data_t TH_GenericReal::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericReal::makeDataFromString");
        }
        uint8_t type = (uint8_t)typeIter->second;
        if (const auto autoType = options.find("_autoType"); autoType != options.end() && autoType->second)
        {
            std::string normalized = str;
            std::erase_if(normalized, [](unsigned char ch) { return std::isspace(ch) != 0; });
            const bool hasSlash = normalized.find('/') != std::string::npos;
            const bool hasRadical = normalized.find("√") != std::string::npos || normalized.find("sqrt") != std::string::npos;
            const bool hasPi = normalized.find("π") != std::string::npos || normalized.find("pi") != std::string::npos;
            if (hasRadical && hasPi)
            {
                throw std::invalid_argument("Pi and radical factors cannot share an exact real member");
            }
            if (hasRadical)
            {
                type = 0x1C;
            }
            else if (hasPi)
            {
                // Exact pi-fraction output can use a decimal coefficient to retain every BCD digit.
                const bool fractionalCoefficient = hasSlash || normalized.find_first_of(".eE") != std::string::npos;
                type = fractionalCoefficient ? 0x21 : 0x20;
            }
            else
            {
                type = hasSlash ? 0x18 : 0x00;
            }
        }
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericReal: " + std::to_string(type));
        }
        data_t data = std::get<0>(handlerIter->second)(str, options, _ctx);
        data[0] = static_cast<uint8_t>((data[0] & 0xC0) | (type & 0x3F));
        return data;
    }

    std::string TH_GenericReal::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }
        const uint8_t type = static_cast<uint8_t>(data[0] & 0x3F);
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type in this TH_GenericReal data: " + std::to_string(type));
        }
        return std::get<1>(handlerIter->second)(data, options, _ctx);
    }

    TIVarFileMinVersionByte TH_GenericReal::getMinVersionFromData(const data_t& data)
    {
        const uint8_t internalType = (uint8_t)(data[0] & 0x3F);
        if (internalType == 0x00 || internalType == 0x0E)
        {
            return VER_NONE;
        }
        if (internalType == 0x18 || internalType == 0x19)
        {
            return VER_84P_253MP;
        }
        return VER_CE_EXACTONLY;
    }
}
