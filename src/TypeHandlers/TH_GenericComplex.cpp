/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include <cstdlib>
#include <stdexcept>
#include <regex>
#include <unordered_map>

namespace tivars::TypeHandlers
{
    static const std::unordered_map<uint8_t, TypeHandlersTuple> type2handlers = {
        { 0x0C, SpecificHandlerTuple(STH_FP) },
        { 0x1B, SpecificHandlerTuple(STH_ExactFraction) },
        { 0x1D, SpecificHandlerTuple(STH_ExactRadical) },
        { 0x1E, SpecificHandlerTuple(STH_ExactPi) },
        { 0x1F, SpecificHandlerTuple(STH_ExactFractionPi) },
    };
    static const std::unordered_map<uint8_t, const char*> type2patterns = {
        { 0x0C, STH_FP::validPattern              },
        { 0x1B, STH_ExactFraction::validPattern   },
        { 0x1D, STH_ExactRadical::validPattern    },
        { 0x1E, STH_ExactPi::validPattern         },
        { 0x1F, STH_ExactFractionPi::validPattern },
    };

    static bool checkValidStringAndGetMatches(const std::string& str, const char* typePattern, std::smatch& matches)
    {
        if (str.empty())
        {
            return false;
        }
        // Handle real only, real+imag, imag only.
        const bool isValid = regex_match(str, matches, std::regex(std::string("^")   + typePattern + "()$"))
                          || regex_match(str, matches, std::regex(std::string("^")   + typePattern + typePattern + "i$"))
                          || regex_match(str, matches, std::regex(std::string("^()") + typePattern + "i$"));
        return isValid;
    }

    // For this, we're going to assume that both members are of the same type...
    // TODO: guess, by parsing, the type instead of reading it from the options
    data_t TH_GenericComplex::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        constexpr size_t bytesPerMember = 9;
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericComplex::makeDataFromString");
        }
        const uint8_t type = (uint8_t)typeIter->second;
        const auto& handlerIter = type2handlers.find(type);
        if (handlerIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + std::to_string(type));
        }
        const auto& handler = std::get<0>(handlerIter->second);

        std::string newStr = str;
        newStr = std::regex_replace(newStr, std::regex(" "), "");
        newStr = std::regex_replace(newStr, std::regex("\\+i"), "+1i");
        newStr = std::regex_replace(newStr, std::regex("-i"), "-1i");

        std::smatch matches;
        const bool isValid = checkValidStringAndGetMatches(newStr, type2patterns.at(type), matches);
        if (!isValid || matches.size() != 3)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid complex subtype string");
        }

        data_t data;
        for (int i=0; i<2; i++)
        {
            std::string coeff = matches[i+1];
            if (coeff.empty())
            {
                coeff = "0";
            }

            const data_t& member_data = handler(coeff, options, _ctx);
            data.insert(data.end(), member_data.begin(), member_data.end());

            uint8_t flags = 0;
            flags |= (atof(coeff.c_str()) < 0) ? (1 << 7) : 0;
            flags |= (1 << 2); // Because it's a complex number
            flags |= (1 << 3); // Because it's a complex number

            data[i * bytesPerMember] = flags;
        }

        return data;
    }

    std::string TH_GenericComplex::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        constexpr size_t bytesPerMember = 9;

        if (data.size() != 2*bytesPerMember)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain 18 bytes");
        }

        // 0x1F because we discard the flag bits (see above)
        const uint8_t typeR = (uint8_t)(data[0] & 0x1F);
        const uint8_t typeI = (uint8_t)(data[bytesPerMember] & 0x1F);

        const auto& handlerRIter = type2handlers.find(typeR);
        if (handlerRIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + std::to_string(typeR));
        }
        const auto& handlerIIter = type2handlers.find(typeI);
        if (handlerIIter == type2handlers.end())
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + std::to_string(typeI));
        }

        const auto& handlerR = std::get<1>(handlerRIter->second);
        const auto& handlerI = std::get<1>(handlerIIter->second);

        const data_t::const_iterator mid = data.cbegin() + bytesPerMember;
        const std::string coeffR = handlerR(data_t(data.cbegin(), mid), options, _ctx);
        const std::string coeffI = handlerI(data_t(mid, data.cend()), options, _ctx);

        const bool coeffRZero = coeffR == "0";
        const bool coeffIZero = coeffI == "0";
        std::string str;
        str.reserve(coeffR.length() + 1 + coeffI.length() + 1);
        if (!coeffRZero || coeffIZero) {
            str += coeffR;
        }
        if (!coeffRZero && !coeffIZero && coeffI.front() != '-' && coeffI.front() != '+') {
            str += '+';
        }
        if (!coeffIZero) {
            if (coeffI != "1") {
                str += coeffI;
            }
            str += 'i';
        }
        return str;
    }

    uint8_t TH_GenericComplex::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
