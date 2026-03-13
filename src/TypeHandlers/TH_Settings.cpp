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
        struct settings_field_t
        {
            const char* name;
            bool integer;
            uint8_t type;
        };

        const settings_field_t window_fields[] = {
            {"Xmin", false, 0x00}, {"Xmax", false, 0x00}, {"Xscl", false, 0x00}, {"Ymin", false, 0x00}, {"Ymax", false, 0x00}, {"Yscl", false, 0x00},
            {"Thetamin", false, 0x00}, {"Thetamax", false, 0x00}, {"Thetastep", false, 0x00}, {"Tmin", false, 0x00}, {"Tmax", false, 0x00}, {"Tstep", false, 0x00},
            {"PlotStart", true, 0x00}, {"nMax", true, 0x00}, {"unMin0", false, 0x0E}, {"vnMin0", false, 0x0E}, {"nMin", true, 0x00}, {"unMin1", false, 0x0E},
            {"vnMin1", false, 0x0E}, {"wnMin0", false, 0x0E}, {"PlotStep", true, 0x00}, {"Xres", true, 0x00}, {"wnMin1", false, 0x0E},
        };

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

        json real_string_to_json_value(const std::string& value, bool integer)
        {
            if (integer)
            {
                return static_cast<int>(std::stod(value));
            }

            if (value.find_first_of("eE") == std::string::npos && value.length() <= 6)
            {
                return std::stod(value);
            }

            return value;
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
        if (type == 0x0F || type == 0x10)
        {
            const json settings = json::parse(str);
            if (!settings.is_object())
            {
                throw std::invalid_argument((type == 0x0F ? "WindowSettings" : "RecallWindow") + std::string(" JSON must be an object"));
            }

            data_t data = (type == 0x0F) ? data_t{0xD0, 0x00, 0x00} : data_t{0xCF, 0x00};
            for (const auto& field : window_fields)
            {
                const data_t fieldData = TH_GenericReal::makeDataFromString(json_number_to_string(settings.at(field.name)), {{"_type", field.type}});
                data.insert(data.end(), fieldData.begin(), fieldData.end());
            }
            return data;
        }

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

        if (data.size() == 210 || data.size() == 209)
        {
            const bool isWindowSettings = data.size() == 210;
            if ((isWindowSettings && (data[0] != 0xD0 || data[1] != 0x00 || data[2] != 0x00))
                || (!isWindowSettings && (data[0] != 0xCF || data[1] != 0x00)))
            {
                throw std::invalid_argument(std::string("Invalid ") + (isWindowSettings ? "WindowSettings" : "RecallWindow") + " data header");
            }

            json settings;
            size_t offset = isWindowSettings ? 3 : 2;
            for (const auto& field : window_fields)
            {
                settings[field.name] = real_string_to_json_value(TH_GenericReal::makeStringFromData(data_t(data.begin() + offset, data.begin() + offset + 9)), field.integer);
                offset += 9;
            }
            return settings.dump(4);
        }

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
