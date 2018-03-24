/*
 * Part of tivars_lib_cpp
 * (C) 2015-2018 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../utils.h"
#include <regex>

using namespace std;

namespace tivars
{

    data_t TH_0x1B::makeDataFromString(const string& str, const options_t& options)
    {
        (void)options;

        throw runtime_error("Unimplemented");

        if (str.empty() || !is_numeric(str))
        {
            throw invalid_argument("Invalid input string. Needs to be a valid Exact Complex Fraction");
        }
    }

    static string makeStringFromComp(data_t data, const options_t& options)
    {
        data_t::reference type = data[0];
        if ((type & ~0x80) != 0x1B)
        {
            throw invalid_argument("Unknown type");
        }
        type &= 0x80;
        return dec2frac(stod(TH_0x00::makeStringFromData(data, options)));
    }

    string TH_0x1B::makeStringFromData(const data_t& data, const options_t& options)
    {
        return makeStringFromComplex(data, options, makeStringFromComp, makeStringFromComp);
    }

}
