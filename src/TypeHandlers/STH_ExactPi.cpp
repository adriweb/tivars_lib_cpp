/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace
{
    std::string normalizePiCoefficient(std::string str)
    {
        static const std::string piSymbol = "π";

        std::erase_if(str, [](unsigned char ch) { return std::isspace(ch) != 0; });
        tivars::replace_all(str, "*", "");
        tivars::replace_all(str, "pi", piSymbol);

        if (str == "0" || str == "+0" || str == "-0")
        {
            return "0";
        }

        const size_t piPos = str.find(piSymbol);
        if (piPos == std::string::npos || str.find(piSymbol, piPos + piSymbol.size()) != std::string::npos)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid Exact Real Pi");
        }

        str.erase(piPos, piSymbol.size());
        if (str.empty() || str == "+" || str == "-")
        {
            str += '1';
        }

        return str;
    }
}

namespace tivars::TypeHandlers
{

    data_t STH_ExactPi::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const std::string coefficient = normalizePiCoefficient(str);
        size_t parsedChars = 0;
        (void)std::stoll(coefficient, &parsedChars);
        if (parsedChars != coefficient.size())
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid Exact Real Pi");
        }

        return STH_FP::makeDataFromString(coefficient, options, _ctx);
    }

    std::string STH_ExactPi::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }

        return multiple(stoi(STH_FP::makeStringFromData(data, options)), "π");
    }

    TIVarFileMinVersionByte STH_ExactPi::getMinVersionFromData(const data_t& data)
    {
        // handled in TH_GenericXXX
        (void)data;
        return VER_NONE;
    }
}
