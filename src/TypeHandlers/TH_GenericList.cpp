/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"
#include "../TIVarTypes.h"

#include <stdexcept>

namespace tivars::TypeHandlers
{
    // TODO: also make it detect the type correctly...
    data_t TH_GenericList::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const auto& typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_GenericList::makeDataFromString");
        }

        const auto type = typeIter->second;
        if (type != TIVarType{"Real"}.getId() && type != TIVarType{"Complex"}.getId())
        {
            throw std::invalid_argument("Invalid type for given string");
        }

        auto arr = explode(trim(str, "{}"), ',');
        const size_t numCount = arr.size();

        for (auto& numStr : arr)
        {
            numStr = trim(numStr);
        }
        if (str.empty() || arr.empty() || numCount > 999)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid real or complex list");
        }

        const auto handler = (type == 0x00) ? &TH_GenericReal::makeDataFromString : &TH_GenericComplex::makeDataFromString;

        data_t data(2); // reserve 2 bytes for size fields

        data[0] = (uint8_t) (numCount & 0xFF);
        data[1] = (uint8_t) ((numCount >> 8) & 0xFF);

        for (const auto& numStr : arr)
        {
            const auto& tmp = handler(numStr, options, _ctx);
            data.insert(data.end(), tmp.begin(), tmp.end());
        }

        return data;
    }

    std::string TH_GenericList::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        const size_t byteCount = data.size();
        if (byteCount < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes");
        }

        const size_t numCount = (size_t) ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));

        const bool isRealList    = (numCount == (size_t)((byteCount - 2) / TH_GenericReal::dataByteCount));
        const bool isComplexList = (numCount == (size_t)((byteCount - 2) / TH_GenericComplex::dataByteCount));

        if (!(isRealList ^ isComplexList))
        {
            throw std::invalid_argument("Invalid data array. Needs to contain 2+9*n or 2+18*n bytes");
        }

        const size_t typeByteCount = isRealList ? TH_GenericReal::dataByteCount : TH_GenericComplex::dataByteCount;

        if (byteCount < 2+typeByteCount || ((byteCount - 2) % typeByteCount != 0) || numCount > 999)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain 2+" + std::to_string(typeByteCount) + "*n bytes");
        }

        const auto handler = isRealList ? &TH_GenericReal::makeStringFromData : &TH_GenericComplex::makeStringFromData;

        std::string str = "{";
        for (size_t i = 2, num = 0; i < byteCount; i += typeByteCount, num++)
        {
            str += handler(data_t(data.begin()+i, data.begin()+i+typeByteCount), options, _ctx);
            if (num < numCount - 1) // not last num
            {
                str += ',';
            }
        }
        str += "}";

        return str;
    }

    uint8_t TH_GenericList::getMinVersionFromData(const data_t& data)
    {
        uint8_t version = 0;
        for (size_t offset = 2; offset < data.size(); offset += 9) {
            uint8_t internalType = data[offset] & 0x3F;
            if (internalType > 0x1B) { // exact complex frac
                version = 0x10;
                break;
            } else if (internalType == 0x1B) { // exact complex frac
                if (version < 0x0B) version = 0x0B;
            } else if (internalType == 0x18 || internalType == 0x19) { // real/mixed frac
                if (version < 0x06) version = 0x06;
            }
        }
        return version;
    }
}
