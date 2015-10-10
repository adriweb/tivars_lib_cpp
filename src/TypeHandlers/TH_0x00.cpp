/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TH_0x00.h"
#include "../utils.h"
#include <regex>

using namespace std;

namespace tivars
{
    
    data_t TH_0x00::makeDataFromString(const string& str, const options_t options)
    {
        data_t data(9);

        if (str == "" || !is_numeric(str))
        {
            throw invalid_argument("Invalid input string. Needs to be a valid real number");
        }

        double number = atof(str.c_str());

        int exponent = (int)floor(log10(abs(number)));
        number *= pow(10.0, -exponent);
        char tmp[20];
        sprintf(tmp, "%0.14f", number);
        string newStr = stripchars(tmp, "-.");

        uint flags = 0;
        flags |= (number < 0) ? (1 << 7) : 0;
        flags |= (has_option(options, "seqInit") && options.at("seqInit") == 1) ? 1 : 0;

        data[0] = flags;
        data[1] = (uint)(exponent + 0x80);
        for (uint i = 2; i < 9; i++)
        {
            data[i] = hexdec(newStr.substr(2*(i-2), 2)) & 0xFF;
        }

        return data;
    }

    string TH_0x00::makeStringFromData(const data_t& data, const options_t options)
    {
        if (data.size() != 9)
        {
            throw invalid_argument("Invalid data array. Needs to contain 9 bytes");
        }
        uint flags      = data[0];
        bool isNegative = (flags >> 7 == 1);
//      bool isUndef    = (flags  & 1 == 1); // if true, "used for initial sequence values"
        uint exponent   = data[1] - 0x80;
        string number   = "";
        for (uint i = 2; i < 9; i++)
        {
            number += dechex(data[i]);
        }
        number = number.substr(0, 1) + "." + number.substr(1);
        float tmp = (float) ((isNegative ? -1.0 : 1.0) * powf(10, exponent) * atof(number.c_str()));

        return to_string(tmp);
    }
}

