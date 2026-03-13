/*
 * Part of tivars_lib_cpp
 * (C) 2015-2025 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../json.hpp"

#include <stdexcept>

using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        std::string json_number_to_string(const json& value)
        {
            if (value.is_string())
            {
                return value.get<std::string>();
            }
            if (value.is_number_integer())
            {
                return std::to_string(value.get<long long>());
            }
            if (value.is_number_unsigned())
            {
                return std::to_string(value.get<unsigned long long>());
            }
            if (value.is_number_float())
            {
                return value.dump();
            }

            throw std::invalid_argument("Expected a JSON string or number for a settings value");
        }
    }

    data_t TH_Settings::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        const auto typeIter = options.find("_type");
        if (typeIter == options.end())
        {
            throw std::runtime_error("Needs _type in options for TH_Settings::makeDataFromString");
        }

        const uint8_t type = static_cast<uint8_t>(typeIter->second);
        if (type != 0x11)
        {
            throw std::runtime_error("Unsupported settings type for TH_Settings::makeDataFromString: " + std::to_string(type));
        }

        const json settings = json::parse(str);
        if (!settings.is_object())
        {
            throw std::invalid_argument("TableRange JSON must be an object");
        }

        data_t data = {0x12, 0x00};
        const std::string tblMin = json_number_to_string(settings.at("TblMin"));
        const std::string deltaTbl = json_number_to_string(settings.at("DeltaTbl"));

        const data_t tblMinData = TH_GenericReal::makeDataFromString(tblMin, {{"_type", 0x00}});
        const data_t deltaTblData = TH_GenericReal::makeDataFromString(deltaTbl, {{"_type", 0x00}});

        data.insert(data.end(), tblMinData.begin(), tblMinData.end());
        data.insert(data.end(), deltaTblData.begin(), deltaTblData.end());
        return data;
    }

    std::string TH_Settings::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        if (data.size() != 20)
        {
            throw std::invalid_argument("Unsupported settings data size: " + std::to_string(data.size()));
        }
        if (data[0] != 0x12 || data[1] != 0x00)
        {
            throw std::invalid_argument("Invalid TableRange data header");
        }

        json settings;
        settings["TblMin"] = static_cast<int>(std::stod(TH_GenericReal::makeStringFromData(data_t(data.begin() + 2, data.begin() + 11))));
        settings["DeltaTbl"] = static_cast<int>(std::stod(TH_GenericReal::makeStringFromData(data_t(data.begin() + 11, data.begin() + 20))));
        return settings.dump(4);
    }

    uint8_t TH_Settings::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
