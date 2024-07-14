/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"

#include <stdexcept>

namespace tivars::TypeHandlers
{

    data_t STH_ExactFractionPi::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        throw std::runtime_error("Unimplemented");

        if (str.empty() || !is_numeric(str))
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid Exact Real Pi Fraction");
        }
    }

    std::string STH_ExactFractionPi::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }

        return dec2frac(stod(STH_FP::makeStringFromData(data, options)), "π");
    }

}
