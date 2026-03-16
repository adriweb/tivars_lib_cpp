/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../TIVarFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace tivars::TypeHandlers
{
    namespace
    {
        constexpr size_t bytesPerMember = 9;

        using MakeDataFn = decltype(&DummyHandler::makeDataFromString);

        struct MemberParser
        {
            uint8_t subtype;
            MakeDataFn makeDataFromString;
        };

        const std::array<MemberParser, 5> realMemberParsers = {{
            {0x0C, &STH_FP::makeDataFromString},
            {0x1B, &STH_ExactFraction::makeDataFromString},
            {0x1D, &STH_ExactRadical::makeDataFromString},
            {0x1E, &STH_ExactPi::makeDataFromString},
            {0x1F, &STH_ExactFractionPi::makeDataFromString},
        }};

        const std::unordered_map<uint8_t, MemberParser> imaginaryMemberParsers = {
            {0x0C, {0x0C, &STH_FP::makeDataFromString}},
            {0x1B, {0x1B, &STH_ExactFraction::makeDataFromString}},
            {0x1D, {0x1D, &STH_ExactRadical::makeDataFromString}},
            {0x1E, {0x1E, &STH_ExactPi::makeDataFromString}},
            {0x1F, {0x1F, &STH_ExactFractionPi::makeDataFromString}},
        };

        struct ComplexParts
        {
            std::string real;
            std::string imag;
        };

        std::string normalizeComplexInput(std::string str)
        {
            std::erase_if(str, [](unsigned char ch) { return std::isspace(ch) != 0; });
            std::erase(str, '*');
            std::ranges::replace(str, 'j', 'i');
            return str;
        }

        ComplexParts splitComplexString(const std::string& str)
        {
            if (str.empty())
            {
                throw std::invalid_argument("Invalid input string. Needs to be a valid complex subtype string");
            }

            if (str.back() != 'i')
            {
                return {str, "0"};
            }

            const std::string imagExpr = str.substr(0, str.size() - 1);
            int depth = 0;
            size_t splitPos = std::string::npos;
            for (size_t i = 0; i < imagExpr.size(); i++)
            {
                const char ch = imagExpr[i];
                if (ch == '(')
                {
                    depth++;
                }
                else if (ch == ')')
                {
                    depth--;
                    if (depth < 0)
                    {
                        throw std::invalid_argument("Invalid input string. Needs to be a valid complex subtype string");
                    }
                }
                else if (i > 0 && depth == 0 && (ch == '+' || ch == '-')
                      && imagExpr[i - 1] != 'e' && imagExpr[i - 1] != 'E')
                {
                    splitPos = i;
                }
            }
            if (depth != 0)
            {
                throw std::invalid_argument("Invalid input string. Needs to be a valid complex subtype string");
            }

            ComplexParts parts;
            if (splitPos == std::string::npos)
            {
                parts.real = "0";
                parts.imag = imagExpr;
            }
            else
            {
                parts.real = imagExpr.substr(0, splitPos);
                parts.imag = imagExpr.substr(splitPos);
            }

            if (parts.real.empty())
            {
                parts.real = "0";
            }
            if (parts.imag.empty() || parts.imag == "+" || parts.imag == "-")
            {
                parts.imag += '1';
            }

            return parts;
        }

        uint8_t parseRealMember(const std::string& coeff, const options_t& options, const TIVarFile* _ctx, data_t& encodedData)
        {
            for (const auto& [subtype, makeDataFromString] : realMemberParsers)
            {
                try
                {
                    encodedData = makeDataFromString(coeff, options, _ctx);
                    return subtype;
                }
                catch (const std::exception&)
                {
                }
            }

            throw std::invalid_argument("Invalid input string. Needs to be a valid real subtype string");
        }
    }

    static const std::unordered_map<uint8_t, TypeHandlersTuple> type2handlers = {
        { 0x0C, SpecificHandlerTuple(STH_FP) },
        { 0x1B, SpecificHandlerTuple(STH_ExactFraction) },
        { 0x1D, SpecificHandlerTuple(STH_ExactRadical) },
        { 0x1E, SpecificHandlerTuple(STH_ExactPi) },
        { 0x1F, SpecificHandlerTuple(STH_ExactFractionPi) },
    };

    data_t TH_GenericComplex::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericComplex::makeDataFromString");
        }
        const uint8_t type = (uint8_t)typeIter->second;
        if (!imaginaryMemberParsers.contains(type))
        {
            throw std::runtime_error("Unknown/Invalid type for this TH_GenericComplex: " + std::to_string(type));
        }

        const auto& [real, imag] = splitComplexString(normalizeComplexInput(str));

        data_t data;

        {
            data_t realMemberData;
            const auto& subtype = parseRealMember(real, options, _ctx, realMemberData);
            realMemberData[0] = static_cast<uint8_t>((realMemberData[0] & 0x80) | (subtype & 0x1F));
            data.insert(data.end(), realMemberData.begin(), realMemberData.end());
        }

        {
            const auto& [subtype, makeDataFromString] = imaginaryMemberParsers.at(type);
            data_t imagMemberData = makeDataFromString(imag, options, _ctx);
            imagMemberData[0] = static_cast<uint8_t>((imagMemberData[0] & 0x80) | (subtype & 0x1F));
            data.insert(data.end(), imagMemberData.begin(), imagMemberData.end());
        }

        return data;
    }

    std::string TH_GenericComplex::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
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

        const auto mid = data.cbegin() + bytesPerMember;
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

    TIVarFileMinVersionByte TH_GenericComplex::getMinVersionFromData(const data_t& data)
    {
        const uint8_t maxInternalType = std::max<uint8_t>(static_cast<uint8_t>(data[0] & 0x1F), static_cast<uint8_t>(data[bytesPerMember] & 0x1F));
        if (maxInternalType == 0x0C) { // Complex
            return VER_NONE;
        } else if (maxInternalType == 0x1B) { // Fraction
            return VER_CE_ALL;
        } else {
            return VER_CE_EXACTONLY;
        }
    }
}
