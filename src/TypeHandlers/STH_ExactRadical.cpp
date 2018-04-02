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

    data_t STH_ExactRadical::makeDataFromString(const string& str, const options_t& options)
    {
        (void)options;

        throw runtime_error("Unimplemented");

        if (str.empty() || !is_numeric(str))
        {
            throw invalid_argument("Invalid input string. Needs to be a valid Exact Real Radical");
        }
    }

    // TODO: handle sign bit?
    string STH_ExactRadical::makeStringFromData(const data_t& data, const options_t& options)
    {
        (void)options;

        if (data.size() != 9)
        {
            throw invalid_argument("Invalid data array. Needs to contain 9 bytes");
        }

        string dataStr;
        for (uint i = 0; i < 9; i++)
        {
            dataStr += (data[i] < 0x10 ? "0" : "") + dechex(data[i]); // zero left pad
        }

        int type = hexdec(dataStr.substr(0, 2));
        type &= ~0x80; // sign bit discarded
        if (type != 0x1C && type != 0x1D) // real or complex
        {
            throw invalid_argument("Invalid data bytes - invalid vartype: " + to_string(type));
        }

        int variant = hexdec(dataStr.substr(2, 1));
        if (variant < 0 || variant > 3)
        {
            throw invalid_argument("Invalid data bytes - unknown type variant: " + to_string(variant));
        }

        const vector<string> parts = {
            (variant == 1 || variant == 3 ? "-" : "") + trimZeros(dataStr.substr(9, 3)),
            trimZeros(dataStr.substr(15, 3)),
            (variant == 2 || variant == 3 ? "-" : "+") + trimZeros(dataStr.substr(6, 3)),
            trimZeros(dataStr.substr(12, 3)),
            trimZeros(dataStr.substr(3, 3))
        };

        string str = "(" + parts[0] + "*√(" + parts[1] + ")" + parts[2] + "*√(" + parts[3]  + "))/" + parts[4];

        // Improve final display
        str = regex_replace(str, regex("\\+1\\*"), "+");  str = regex_replace(str, regex("\\(1\\*"),  "(");
        str = regex_replace(str, regex("-1\\*"),   "-");  str = regex_replace(str, regex("\\(-1\\*"), "(-");
        str = regex_replace(str, regex("\\+-"),    "-");

        return str;
    }

}