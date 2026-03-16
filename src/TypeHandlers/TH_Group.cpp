/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include "../json.hpp"
#include "../TIModels.h"
#include "../TIVarFile.h"
#include "../tivarslib_utils.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        uint16_t read_le16(const data_t& data, size_t offset)
        {
            if (offset + 1 >= data.size())
            {
                throw std::invalid_argument("Unexpected end of GroupObject data");
            }

            return static_cast<uint16_t>(data[offset])
                 | (static_cast<uint16_t>(data[offset + 1]) << 8);
        }

        data_t parse_hex(const std::string& hex)
        {
            if (hex.size() % 2 != 0)
            {
                throw std::invalid_argument("GroupObject hex strings must have an even number of digits");
            }

            data_t data;
            data.reserve(hex.size() / 2);
            for (size_t i = 0; i < hex.size(); i += 2)
            {
                data.push_back(hexdec(hex.substr(i, 2)));
            }
            return data;
        }

        std::string to_hex(const data_t& data)
        {
            std::string out;
            out.reserve(data.size() * 2);
            for (const uint8_t byte : data)
            {
                out += dechex(byte);
            }
            return out;
        }

        bool is_sized_group_entry_type(uint8_t typeId)
        {
            switch (typeId)
            {
                case 0x03: // Equation
                case 0x04: // String
                case 0x05: // Program
                case 0x06: // ProtectedProgram
                case 0x08: // GraphDataBase
                case 0x0B: // SmartEquation
                case 0x15: // AppVar / PythonAppVar
                case 0x17: // GroupObject
                    return true;
                default:
                    return false;
            }
        }

        TIVarType detect_full_type(uint8_t typeId, const data_t& entryData)
        {
            TIVarType type{typeId};
            if (type.getName() == "AppVar")
            {
                const std::string detectedTypeName = detectStructuredAppVarTypeName(entryData);
                if (detectedTypeName != "AppVar")
                {
                    return TIVarType{detectedTypeName};
                }
            }
            return type;
        }

        size_t get_group_entry_data_length(const TIVarType& type, const data_t& data, size_t offset)
        {
            const uint8_t typeId = static_cast<uint8_t>(type.getId());
            if (offset >= data.size())
            {
                throw std::invalid_argument("Unexpected end of GroupObject data");
            }

            if (is_sized_group_entry_type(typeId))
            {
                return static_cast<size_t>(2 + read_le16(data, offset));
            }

            switch (typeId)
            {
                case 0x00: // Real
                case 0x18: // RealFraction
                case 0x19: // MixedFraction
                case 0x1C: // ExactRealRadical
                case 0x20: // ExactRealPi
                case 0x21: // ExactRealPiFrac
                    return TH_GenericReal::dataByteCount;

                case 0x0C: // Complex
                case 0x1B: // ExactComplexFrac
                case 0x1D: // ExactComplexRadical
                case 0x1E: // ExactComplexPi
                case 0x1F: // ExactComplexPiFrac
                    return TH_GenericComplex::dataByteCount;

                case 0x01: // RealList
                    return 2 + read_le16(data, offset) * TH_GenericReal::dataByteCount;

                case 0x0D: // ComplexList
                    return 2 + read_le16(data, offset) * TH_GenericComplex::dataByteCount;

                case 0x02: // Matrix
                {
                    if (offset + 1 >= data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject matrix data");
                    }
                    return 2 + data[offset] * data[offset + 1] * TH_GenericReal::dataByteCount;
                }

                case 0x07: // Picture
                {
                    const size_t remaining = data.size() - offset;
                    return remaining >= TH_Picture::colorPictureDataByteCount
                         ? TH_Picture::colorPictureDataByteCount
                         : TH_Picture::monoPictureDataByteCount;
                }

                case 0x0F: // WindowSettings
                    return TH_Settings::windowSettingsDataByteCount;

                case 0x10: // RecallWindow
                    return TH_Settings::recallWindowDataByteCount;

                case 0x11: // TableRange
                    return TH_Settings::tableRangeDataByteCount;

                case 0x1A: // Image
                    return TH_Picture::imageDataByteCount;

                default:
                    throw std::runtime_error("Unsupported GroupObject entry type " + std::to_string(typeId));
            }
        }

        std::string grouped_entry_name_to_string(const TIVarType& type, const data_t& rawName)
        {
            return entry_name_to_string(type, rawName.data(), rawName.size());
        }

        data_t grouped_entry_name_from_json(const TIVarType& type, const json& entry)
        {
            if (entry.contains("nameHex"))
            {
                data_t rawName = parse_hex(entry.at("nameHex").get<std::string>());
                rawName.resize(8, 0x00);
                return rawName;
            }

            const std::string name = entry.contains("name")
                                   ? entry.at("name").get<std::string>()
                                   : "";

            static const char* const candidateModels[] = {"83PCE", "84+CE", "84+CSE", "84+", "83+", "83", "82"};
            for (const char* modelName : candidateModels)
            {
                try
                {
                    TIVarFile scratch = TIVarFile::createNew(type, name, TIModel{modelName});
                    const auto& scratchEntry = scratch.getVarEntries()[0];
                    return data_t(scratchEntry.varname, scratchEntry.varname + sizeof(scratchEntry.varname));
                }
                catch (const std::exception&)
                {
                }
            }

            throw std::runtime_error("Unable to derive a valid GroupObject entry name for type " + type.getName());
        }

        std::string json_value_to_string(const json& value)
        {
            if (value.is_string())
            {
                return value.get<std::string>();
            }
            return value.dump();
        }
    }

    data_t TH_Group::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const json j = json::parse(str);
        const json& entries = j.contains("entries") ? j.at("entries") : j;
        if (!entries.is_array())
        {
            throw std::invalid_argument("GroupObject JSON must be an array or an object with an entries array");
        }

        data_t data(minimumDataByteCount, 0x00);
        for (const json& entry : entries)
        {
            if (!entry.is_object())
            {
                throw std::invalid_argument("Each GroupObject entry must be a JSON object");
            }

            const uint8_t groupedTypeByte = static_cast<uint8_t>(entry.contains("groupTypeByte")
                                        ? entry.at("groupTypeByte").get<int>()
                                        : (entry.contains("typeId")
                                            ? entry.at("typeId").get<int>()
                                            : TIVarType{entry.at("typeName").get<std::string>()}.getId()));
            const TIVarType type = entry.contains("typeName")
                                 ? TIVarType{entry.at("typeName").get<std::string>()}
                                 : detect_full_type(groupedTypeByte & 0x3F, {});
            data_t entryData;
            if (entry.contains("rawDataHex"))
            {
                entryData = parse_hex(entry.at("rawDataHex").get<std::string>());
            }
            else if (entry.contains("readableContent"))
            {
                entryData = std::get<0>(type.getHandlers())(json_value_to_string(entry.at("readableContent")), {}, _ctx);
            }
            else
            {
                throw std::invalid_argument("Each GroupObject entry must contain rawDataHex or readableContent");
            }

            const data_t rawName = grouped_entry_name_from_json(type, entry);
            const uint8_t archivedFlag = entry.value("archived", false) ? archivedFlagValue : 0x00;
            const uint8_t version = entry.contains("version")
                                  ? static_cast<uint8_t>(entry.at("version").get<int>())
                                  : std::get<2>(type.getHandlers())(entryData);

            data.push_back(groupedTypeByte);
            data.push_back(0x00);
            data.push_back(version);
            data.push_back(0x00);
            data.push_back(0x00);
            data.push_back(archivedFlag);

            switch (groupedTypeByte & 0x3F)
            {
                case 0x05: // Program
                case 0x06: // ProtectedProgram
                case 0x15: // AppVar/PythonAppVar
                case 0x17: // GroupObject
                {
                    const size_t nameLen = std::ranges::find(rawName, 0x00) - rawName.begin();
                    data.push_back(static_cast<uint8_t>(nameLen));
                    data.insert(data.end(), rawName.begin(), rawName.begin() + static_cast<long>(nameLen));
                    break;
                }

                case 0x01: // RealList
                case 0x0D: // ComplexList
                {
                    const size_t nameLen = std::ranges::find(rawName, 0x00) - rawName.begin();
                    data.push_back(static_cast<uint8_t>(nameLen + 1));
                    data.insert(data.end(), rawName.begin(), rawName.begin() + static_cast<long>(nameLen));
                    data.push_back(0x00);
                    break;
                }

                default:
                    data.insert(data.end(), rawName.begin(), rawName.begin() + fixedNameByteCount);
                    break;
            }

            data.insert(data.end(), entryData.begin(), entryData.end());
        }

        const size_t length = data.size() - minimumDataByteCount;
        if (length > 0xFFFF)
        {
            throw std::invalid_argument("GroupObject data is too large");
        }
        data[0] = static_cast<uint8_t>(length & 0xFF);
        data[1] = static_cast<uint8_t>((length >> 8) & 0xFF);
        return data;
    }

    std::string TH_Group::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;

        if (data.size() < minimumDataByteCount)
        {
            throw std::invalid_argument("Invalid GroupObject data. Needs to contain at least 2 bytes");
        }

        const size_t expectedLength = read_le16(data, 0);
        if (expectedLength != data.size() - minimumDataByteCount)
        {
            throw std::invalid_argument("Invalid GroupObject data length");
        }

        json entries = json::array();
        size_t offset = minimumDataByteCount;
        while (offset < data.size())
        {
            if (offset + 3 > data.size())
            {
                throw std::invalid_argument("Unexpected end of GroupObject VAT data");
            }

            const uint8_t groupedTypeByte = data[offset++];
            offset++; // Unused byte
            const uint8_t version = data[offset++];
            const uint8_t typeId = groupedTypeByte & 0x3F;

            data_t rawName(8, 0x00);
            uint8_t archivedFlag = 0x00;
            switch (typeId)
            {
                case 0x05: // Program
                case 0x06: // ProtectedProgram
                case 0x15: // AppVar/PythonAppVar
                case 0x17: // GroupObject
                {
                    if (offset + 4 > data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject VAT data");
                    }
                    offset += 2;
                    archivedFlag = data[offset++];
                    const uint8_t nameLength = data[offset++];
                    if (offset + nameLength > data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject variable name");
                    }
                    std::copy(data.begin() + static_cast<long>(offset), data.begin() + static_cast<long>(offset + nameLength), rawName.begin());
                    offset += nameLength;
                    break;
                }

                case 0x01: // RealList
                case 0x0D: // ComplexList
                {
                    if (offset + 4 > data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject VAT data");
                    }
                    offset += 2;
                    archivedFlag = data[offset++];
                    const uint8_t nameLength = data[offset++];
                    if (nameLength == 0 || offset + nameLength > data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject list name");
                    }
                    std::copy(data.begin() + static_cast<long>(offset), data.begin() + static_cast<long>(offset + nameLength - 1), rawName.begin());
                    offset += nameLength;
                    break;
                }

                default:
                {
                    if (offset + fixedNameByteCount > data.size())
                    {
                        throw std::invalid_argument("Unexpected end of GroupObject VAT data");
                    }
                    offset += 2;
                    archivedFlag = data[offset++];
                    std::copy(data.begin() + static_cast<long>(offset), data.begin() + static_cast<long>(offset + fixedNameByteCount), rawName.begin());
                    offset += fixedNameByteCount;
                    break;
                }
            }

            size_t entryLength = get_group_entry_data_length(TIVarType{typeId}, data, offset);
            if (offset + entryLength > data.size())
            {
                throw std::invalid_argument("Unexpected end of GroupObject entry data");
            }

            const data_t entryData(data.begin() + static_cast<long>(offset), data.begin() + static_cast<long>(offset + entryLength));
            const TIVarType fullType = detect_full_type(typeId, entryData);
            json entry = {
                {"typeName", fullType.getName()},
                {"typeId", typeId},
                {"groupTypeByte", groupedTypeByte},
                {"version", version},
                {"archived", archivedFlag == archivedFlagValue},
                {"nameHex", to_hex(data_t(rawName.begin(), rawName.end()))},
                {"rawDataHex", to_hex(entryData)},
            };
            const std::string readableName = grouped_entry_name_to_string(fullType, rawName);
            if (!readableName.empty())
            {
                entry["name"] = readableName;
            }

            try
            {
                const std::string readable = std::get<1>(fullType.getHandlers())(entryData, {}, _ctx);
                try
                {
                    entry["readableContent"] = json::parse(readable);
                }
                catch (const std::exception&)
                {
                    json readableJson = readable;
                    (void)readableJson.dump();
                    entry["readableContent"] = readable;
                }
            }
            catch (const std::exception&)
            {
            }

            entries.push_back(entry);
            offset += entryLength;
        }

        return json{{"entries", entries}}.dump(4);
    }

    TIVarFileMinVersionByte TH_Group::getMinVersionFromData(const data_t& data)
    {
        if (data.size() < minimumDataByteCount)
        {
            return VER_NONE;
        }

        TIVarFileMinVersionByte version = VER_NONE;
        size_t offset = minimumDataByteCount;
        while (offset < data.size())
        {
            if (offset + 3 > data.size())
            {
                break;
            }

            const uint8_t groupedTypeByte = data[offset++];
            offset++; // Unused
            if (data[offset] > version)
            {
                version = (TIVarFileMinVersionByte)data[offset];
            }
            offset++;

            const uint8_t typeId = groupedTypeByte & 0x3F;
            switch (typeId)
            {
                case 0x05:
                case 0x06:
                case 0x15:
                case 0x17:
                    offset += 3;
                    if (offset > data.size()) return version;
                    offset += data[offset - 1];
                    break;

                case 0x01:
                case 0x0D:
                    offset += 3;
                    if (offset > data.size()) return version;
                    offset += data[offset - 1];
                    break;

                default:
                    offset += 6;
                    break;
            }

            if (offset >= data.size())
            {
                break;
            }

            try
            {
                const size_t entryLength = get_group_entry_data_length(TIVarType{typeId}, data, offset);
                offset += entryLength;
            }
            catch (const std::exception&)
            {
                break;
            }
        }

        return version;
    }
}
