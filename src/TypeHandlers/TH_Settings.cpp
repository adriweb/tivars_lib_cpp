/*
 * Part of tivars_lib_cpp
 * (C) 2015-2025 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../json.hpp"

#include <stdexcept>

using namespace std::string_literals;
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
            {"Xmin", false, TH_Settings::typeReal}, {"Xmax", false, TH_Settings::typeReal}, {"Xscl", false, TH_Settings::typeReal}, {"Ymin", false, TH_Settings::typeReal}, {"Ymax", false, TH_Settings::typeReal}, {"Yscl", false, TH_Settings::typeReal},
            {"Thetamin", false, TH_Settings::typeReal}, {"Thetamax", false, TH_Settings::typeReal}, {"Thetastep", false, TH_Settings::typeReal}, {"Tmin", false, TH_Settings::typeReal}, {"Tmax", false, TH_Settings::typeReal}, {"Tstep", false, TH_Settings::typeReal},
            {"PlotStart", true, TH_Settings::typeReal}, {"nMax", true, TH_Settings::typeReal}, {"unMin0", false, TH_Settings::typeUndefinedReal}, {"vnMin0", false, TH_Settings::typeUndefinedReal}, {"nMin", true, TH_Settings::typeReal}, {"unMin1", false, TH_Settings::typeUndefinedReal},
            {"vnMin1", false, TH_Settings::typeUndefinedReal}, {"wnMin0", false, TH_Settings::typeUndefinedReal}, {"PlotStep", true, TH_Settings::typeReal}, {"Xres", true, TH_Settings::typeReal}, {"wnMin1", false, TH_Settings::typeUndefinedReal},
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
        if (type == typeWindowSettings || type == typeRecallWindow)
        {
            const json settings = json::parse(str);
            if (!settings.is_object())
            {
                throw std::invalid_argument((type == typeWindowSettings ? "WindowSettings"s : "RecallWindow"s) + " JSON must be an object");
            }

            data_t data = (type == typeWindowSettings)
                        ? data_t(windowSettingsHeader, windowSettingsHeader + windowSettingsHeaderByteCount)
                        : data_t(recallWindowHeader, recallWindowHeader + recallWindowHeaderByteCount);
            for (const auto& field : window_fields)
            {
                const data_t fieldData = TH_GenericReal::makeDataFromString(json_number_to_string(settings.at(field.name)), {{"_type", field.type}});
                data.insert(data.end(), fieldData.begin(), fieldData.end());
            }
            return data;
        }

        if (type != typeTableRange)
        {
            throw std::runtime_error("Unsupported settings type for TH_Settings::makeDataFromString: " + std::to_string(type));
        }

        const json settings = json::parse(str);
        if (!settings.is_object())
        {
            throw std::invalid_argument("TableRange JSON must be an object");
        }

        data_t data(tableRangeHeader, tableRangeHeader + tableRangeHeaderByteCount);
        const std::string tblMin = json_number_to_string(settings.at("TblMin"));
        const std::string deltaTbl = json_number_to_string(settings.at("DeltaTbl"));

        const data_t tblMinData = TH_GenericReal::makeDataFromString(tblMin, {{"_type", typeReal}});
        const data_t deltaTblData = TH_GenericReal::makeDataFromString(deltaTbl, {{"_type", typeReal}});

        data.insert(data.end(), tblMinData.begin(), tblMinData.end());
        data.insert(data.end(), deltaTblData.begin(), deltaTblData.end());
        return data;
    }

    std::string TH_Settings::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        if (data.size() == windowSettingsDataByteCount || data.size() == recallWindowDataByteCount)
        {
            const bool isWindowSettings = data.size() == windowSettingsDataByteCount;
            if ((isWindowSettings && !std::equal(windowSettingsHeader, windowSettingsHeader + windowSettingsHeaderByteCount, data.begin()))
                || (!isWindowSettings && !std::equal(recallWindowHeader, recallWindowHeader + recallWindowHeaderByteCount, data.begin())))
            {
                throw std::invalid_argument("Invalid "s + (isWindowSettings ? "WindowSettings" : "RecallWindow") + " data header");
            }

            json settings;
            size_t offset = isWindowSettings ? windowSettingsHeaderByteCount : recallWindowHeaderByteCount;
            for (const auto& field : window_fields)
            {
                settings[field.name] = real_string_to_json_value(TH_GenericReal::makeStringFromData(data_t(data.begin() + offset, data.begin() + offset + realDataByteCount)), field.integer);
                offset += realDataByteCount;
            }
            return settings.dump(4);
        }

        if (data.size() != tableRangeDataByteCount)
        {
            throw std::invalid_argument("Unsupported settings data size: " + std::to_string(data.size()));
        }
        if (!std::equal(tableRangeHeader, tableRangeHeader + tableRangeHeaderByteCount, data.begin()))
        {
            throw std::invalid_argument("Invalid TableRange data header");
        }

        json settings;
        settings["TblMin"] = static_cast<int>(std::stod(TH_GenericReal::makeStringFromData(data_t(data.begin() + tableRangeHeaderByteCount, data.begin() + tableRangeHeaderByteCount + realDataByteCount))));
        settings["DeltaTbl"] = static_cast<int>(std::stod(TH_GenericReal::makeStringFromData(data_t(data.begin() + tableRangeHeaderByteCount + realDataByteCount, data.begin() + tableRangeHeaderByteCount + 2 * realDataByteCount))));
        return settings.dump(4);
    }

    TIVarFileMinVersionByte TH_Settings::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return VER_NONE;
    }
}
