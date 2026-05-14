/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"
#include "../TIVarFile.h"

#include <stdexcept>

namespace tivars::TypeHandlers
{
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

        const std::string inner = trim(trim(str), "{}");
        if (inner.empty())
        {
            return {0, 0};
        }

        auto arr = explode(inner, ',');
        const size_t numCount = arr.size();

        for (auto& numStr : arr)
        {
            numStr = trim(numStr);
        }
        if (str.empty() || arr.empty() || numCount > 999)
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid real or complex list");
        }

        const bool isRealList = type == TIVarType{"Real"}.getId();
        const auto handler = isRealList ? &TH_GenericReal::makeDataFromString : &TH_GenericComplex::makeDataFromString;
        const size_t valueByteCount = isRealList ? TH_GenericReal::dataByteCount : TH_GenericComplex::dataByteCount;

        data_t data;
        data.reserve(2 + numCount * valueByteCount);
        data.resize(2); // leave 2 bytes for item count

        data[0] = (uint8_t) (numCount & 0xFF);
        data[1] = (uint8_t) ((numCount >> 8) & 0xFF);

        options_t memberOptions = options;
        memberOptions["_autoType"] = 1;
        const auto stringHandler = isRealList ? &TH_GenericReal::makeStringFromData : &TH_GenericComplex::makeStringFromData;
        const data_t* previous = nullptr;
        if (_ctx && !_ctx->isEvoFormat())
        {
            const auto entryOption = options.find("_entryIdx");
            const size_t entryIdx = entryOption == options.end() ? 0 : static_cast<size_t>(entryOption->second);
            const auto& entries = _ctx->getVarEntries();
            if (entryIdx < entries.size() && entries[entryIdx]._type.getName() == (isRealList ? "RealList" : "ComplexList"))
            {
                const auto& oldData = entries[entryIdx].data;
                if (oldData.size() >= 2 && oldData.size() == 2 + (oldData[0] + (size_t(oldData[1]) << 8)) * valueByteCount)
                {
                    previous = &oldData;
                }
            }
        }

        for (const auto& numStr : arr)
        {
            data_t tmp;
            const size_t offset = data.size();
            if (previous && offset + valueByteCount <= previous->size())
            {
                const data_t oldMember(previous->begin() + offset, previous->begin() + offset + valueByteCount);
                // Text alone cannot identify an integer's exact subtype. Keep unchanged members verbatim.
                try
                {
                    const bool supported = detail::supportsNumericSubtype(oldMember[0] & 0x3F, _ctx)
                        && (isRealList || detail::supportsNumericSubtype(oldMember[9] & 0x3F, _ctx));
                    if (supported && stringHandler(oldMember, options, _ctx) == numStr)
                    {
                        tmp = oldMember;
                    }
                }
                catch (const std::exception&)
                {
                    // An unsupported old member must not prevent replacing it with valid text.
                }
            }
            if (tmp.empty())
            {
                tmp = handler(numStr, memberOptions, _ctx);
            }
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
        if (numCount == 0 && byteCount == 2)
        {
            return "{}";
        }

        const bool isRealList    = (numCount == (byteCount - 2) / TH_GenericReal::dataByteCount);
        const bool isComplexList = (numCount == (byteCount - 2) / TH_GenericComplex::dataByteCount);

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

    TIVarFileMinVersionByte TH_GenericList::getMinVersionFromData(const data_t& data)
    {
        TIVarFileMinVersionByte version = VER_NONE;
        for (size_t offset = 2; offset < data.size(); offset += 9) {
            const uint8_t internalType = data[offset] & 0x3F;
            if (internalType > 0x1B) { // exact complex frac
                version = VER_CE_EXACTONLY;
                break;
            } else if (internalType == 0x1B) { // exact complex frac
                if (version < VER_CE_ALL) version = VER_CE_ALL;
            } else if (internalType == 0x18 || internalType == 0x19) { // real/mixed frac
                if (version < VER_84P_253MP) version = VER_84P_253MP;
            }
        }
        return version;
    }
}
