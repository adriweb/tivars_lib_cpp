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
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string normalizeExactFractionInput(std::string str)
    {
        std::erase_if(str, [](unsigned char ch) { return std::isspace(ch) != 0; });

        if (str.empty())
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid Exact Real Fraction");
        }

        return str;
    }

    std::string fractionToDecimalString(const std::string& str)
    {
        const size_t slashPos = str.find('/');
        if (slashPos == std::string::npos)
        {
            return str;
        }
        if (str.find('/', slashPos + 1) != std::string::npos)
        {
            throw std::invalid_argument("Invalid fraction string");
        }

        const long double numerator = std::stold(str.substr(0, slashPos));
        const long double denominator = std::stold(str.substr(slashPos + 1));
        if (denominator == 0.0L)
        {
            throw std::invalid_argument("Fraction denominator must be nonzero");
        }

        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
               << std::defaultfloat
               << (numerator / denominator);
        return stream.str();
    }
}

namespace tivars::TypeHandlers
{

    data_t STH_ExactFraction::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const std::string normalized = normalizeExactFractionInput(str);
        return STH_FP::makeDataFromString(fractionToDecimalString(normalized), options, _ctx);
    }

    std::string STH_ExactFraction::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }

        return dec2frac(stod(STH_FP::makeStringFromData(data, options)));
    }

    uint8_t STH_ExactFraction::getMinVersionFromData(const data_t& data)
    {
        // handled in TH_GenericXXX
        (void)data;
        return 0;
    }
}
