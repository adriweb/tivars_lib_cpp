/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include "../json.hpp"
#include "../tivarslib_utils.h"

#include <stdexcept>
#include <cstring>

namespace tivars::TypeHandlers
{
    data_t TH_GenericAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        const std::string trimmed = trim(str);
        if (!trimmed.empty() && trimmed.front() == '{')
        {
            const nlohmann::json j = nlohmann::json::parse(trimmed);
            if (j.contains("typeName"))
            {
                const std::string typeName = j.at("typeName").get<std::string>();
                if (typeName == "PythonModuleAppVar"
                 || typeName == "PythonImageAppVar"
                 || typeName == "StudyCardsAppVar"
                 || typeName == "StudyCardsSetgsAppVar"
                 || typeName == "CellSheetAppVar"
                 || typeName == "CellSheetStateAppVar")
                {
                    return TH_StructuredAppVar::makeDataFromString(trimmed, options, _ctx);
                }
            }
        }

        return STH_DataAppVar::makeDataFromString(str, options);
    }

    std::string TH_GenericAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        if (data.size() < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes");
        }

        const std::string typeName = detectStructuredAppVarTypeName(data);
        if (typeName == "PythonAppVar")
        {
            try {
                return STH_PythonAppVar::makeStringFromData(data, options);
            } catch (...) {} // "fallthrough"
        }
        else if (typeName != "AppVar")
        {
            try {
                return TH_StructuredAppVar::makeStringFromData(data, options, _ctx);
            } catch (...) {} // "fallthrough"
        }

        return STH_DataAppVar::makeStringFromData(data, options);
    }

    uint8_t TH_GenericAppVar::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
