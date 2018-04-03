/*
 * Part of tivars_lib_cpp
 * (C) 2015-2018 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../utils.h"

using namespace std;

namespace tivars
{
    
    data_t TH_ComplexList::makeDataFromString(const string& str, const options_t& options)
    {
        (void)options;

        data_t data(2); // reserve 2 bytes for size fields

        auto arr = explode(trim(str, "{}"), ',');
        size_t numCount = arr.size();

        for (auto& numStr : arr)
        {
            numStr = trim(numStr);
        }
        if (str.empty() || arr.empty() || numCount > 999)
        {
            throw invalid_argument("Invalid input string. Needs to be a valid complex list");
        }

        data[0] = (uchar) (numCount & 0xFF);
        data[1] = (uchar) ((numCount >> 8) & 0xFF);

        for (const auto& numStr : arr)
        {
            const auto& tmp = TH_GenericComplex::makeDataFromString(numStr, { {"_type", 0x0C} });
            data.insert(data.end(), tmp.begin(), tmp.end());
        }

        return data;
    }

    string TH_ComplexList::makeStringFromData(const data_t& data, const options_t& options)
    {
        (void)options;

        string str;

        size_t byteCount = data.size();
        size_t numCount = (size_t) ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        if (byteCount < 2+TH_GenericComplex::dataByteCount || ((byteCount - 2) % TH_GenericComplex::dataByteCount != 0)
            || (numCount != (size_t)((byteCount - 2) / TH_GenericComplex::dataByteCount)) || numCount > 999)
        {
            throw invalid_argument("Invalid data array. Needs to contain 2+" + to_string(TH_GenericComplex::dataByteCount) + "*n bytes");
        }

        str = "{";

        for (size_t i = 2, num = 0; i < byteCount; i += TH_GenericComplex::dataByteCount, num++)
        {
            str += TH_GenericComplex::makeStringFromData(data_t(data.begin()+i, data.begin()+i+TH_GenericComplex::dataByteCount));
            if (num < numCount - 1) // not last num
            {
                str += ',';
            }
        }

        str += "}";
        
        return str;
    }
}
