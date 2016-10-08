/*
 * Part of tivars_lib_cpp
 * (C) 2015-2016 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../utils.h"

using namespace std;

namespace tivars
{

    data_t TH_0x1D::makeDataFromString(const string& str, const options_t& options)
    {
        (void)options;

        throw runtime_error("Unimplemented");

        if (str == "" || !is_numeric(str))
        {
            throw runtime_error("Invalid input string. Needs to be a valid Exact Complex Radical");
        }
    }

    string TH_0x1D::makeStringFromData(const data_t& data, const options_t& options)
    {
        (void)options;

        if (data.size() != TH_0x1D::dataByteCount)
        {
            throw invalid_argument("Empty data array. Needs to contain " + to_string(TH_0x1D::dataByteCount) + " bytes");
        }

        string coeffR = TH_0x1C::makeStringFromData(data_t(data.begin(), data.begin() + TH_0x1C::dataByteCount));
        string coeffI = TH_0x1C::makeStringFromData(data_t(data.begin() + TH_0x1C::dataByteCount, data.begin() + 2 * TH_0x1C::dataByteCount));

        string str = "(" + coeffR + ")+(" + coeffI + ")*i";
        
        str = regex_replace(str, regex("\\+-"), "-");

        return str;
    }

}