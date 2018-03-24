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

    data_t TH_0x1F::makeDataFromString(const string& str, const options_t& options)
    {
        (void)options;

        throw runtime_error("Unimplemented");

        if (str.empty() || !is_numeric(str))
        {
            throw invalid_argument("Invalid input string. Needs to be a valid Exact Complex Pi Fraction");
        }
    }

    static string makeStringFromComp(data_t data, const options_t& options)
    {
        data_t::reference type = data[0];
        if ((type & ~0x81) != 0x1E)
        {
            throw invalid_argument("Unknown type");
        }
        bool frac = type & 1;
        type &= 0x80;
        string str = TH_0x00::makeStringFromData(data, options);
        return frac ? dec2frac(stod(str), "π") : multiple(stoi(str), "π");
    }

    string TH_0x1F::makeStringFromData(const data_t& data, const options_t& options)
    {
        return makeStringFromComplex(data, options, makeStringFromComp, makeStringFromComp);
    }

}
