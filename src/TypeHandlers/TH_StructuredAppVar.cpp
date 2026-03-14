/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 *
 * This code has been mostly possible thanks to LogicalJoe's research on
 * https://github.com/TI-Toolkit/tivars_hexfiend_templates/
 */

#include "TypeHandlers.h"

#include "../json.hpp"
#include "../tivarslib_utils.h"

#include <array>
#include <cctype>
#include <cstring>
#include <stdexcept>

using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        constexpr char PYTHON_MODULE_MAGIC[] = "PYMP";
        constexpr char PYTHON_IMAGE_MAGIC[] = "IM8C";
        constexpr std::array<uint8_t, 4> STUDY_CARDS_MAGIC          = {0xF3, 0x47, 0xBF, 0xA7};
        constexpr std::array<uint8_t, 4> STUDY_CARDS_SETTINGS_MAGIC = {0xF3, 0x47, 0xBF, 0xA8};
        constexpr std::array<uint8_t, 4> CELSHEET_MAGIC             = {0xF3, 0x47, 0xBF, 0xAA};
        constexpr std::array<uint8_t, 4> CELSHEET_STATE_MAGIC       = {0xF3, 0x47, 0xBF, 0xAB};
        constexpr std::array<uint8_t, 4> NOTEFOLIO_MAGIC            = {0xF3, 0x47, 0xBF, 0xAF};
        constexpr size_t appVarSizePrefixByteCount = 2;
        constexpr size_t magicByteCount = 4;
        constexpr uint8_t pythonImagePaletteMarker = 0x01;
        constexpr size_t studyCardsTitleCount = 4;
        constexpr size_t studyCardsSettingsNameByteCount = 9;
        constexpr size_t cellSheetNameByteCount = 8;
        constexpr size_t notefolioReservedByteCount = 4;
        constexpr size_t notefolioNameByteCount = 8;
        constexpr size_t notefolioHeaderUnknownByteCount = 6;

        StructuredAppVarSubtype subtype_from_type_name(const std::string& typeName)
        {
            if (typeName == "PythonModuleAppVar")
            {
                return APPVAR_SUBTYPE_PYTHON_MODULE;
            }
            if (typeName == "PythonImageAppVar")
            {
                return APPVAR_SUBTYPE_PYTHON_IMAGE;
            }
            if (typeName == "StudyCardsAppVar")
            {
                return APPVAR_SUBTYPE_STUDY_CARDS;
            }
            if (typeName == "StudyCardsSetgsAppVar")
            {
                return APPVAR_SUBTYPE_STUDY_CARDS_SETTINGS;
            }
            if (typeName == "CellSheetAppVar")
            {
                return APPVAR_SUBTYPE_CELSHEET;
            }
            if (typeName == "CellSheetStateAppVar")
            {
                return APPVAR_SUBTYPE_CELSHEET_STATE;
            }
            if (typeName == "NotefolioAppVar")
            {
                return APPVAR_SUBTYPE_NOTEFOLIO;
            }
            throw std::invalid_argument("Unknown structured AppVar type name: " + typeName);
        }

        bool has_prefix(const data_t& data, const char* magic)
        {
            return data.size() >= appVarSizePrefixByteCount + magicByteCount
                && memcmp(&data[appVarSizePrefixByteCount], magic, magicByteCount) == 0;
        }

        bool has_prefix(const data_t& data, const std::array<uint8_t, 4>& magic)
        {
            return data.size() >= appVarSizePrefixByteCount + magic.size()
                && std::equal(magic.begin(), magic.end(), data.begin() + static_cast<ptrdiff_t>(appVarSizePrefixByteCount));
        }

        void ensure_sized_payload(const data_t& data)
        {
            if (data.size() < appVarSizePrefixByteCount)
            {
                throw std::invalid_argument("Invalid AppVar data. Needs to contain at least 2 bytes");
            }

            const size_t expected = static_cast<size_t>(data[0]) | (static_cast<size_t>(data[1]) << 8);
            const size_t actual = data.size() - appVarSizePrefixByteCount;
            if (expected != actual)
            {
                throw std::invalid_argument("Invalid AppVar data. Expected " + std::to_string(expected) + " bytes, got " + std::to_string(actual));
            }
        }

        data_t payload_from_data(const data_t& data)
        {
            ensure_sized_payload(data);
            return data_t(data.begin() + static_cast<ptrdiff_t>(appVarSizePrefixByteCount), data.end());
        }

        data_t wrap_payload(const data_t& payload)
        {
            if (payload.size() > 0xFFFF)
            {
                throw std::invalid_argument("Structured AppVar payload is too large");
            }

            data_t data = {
                static_cast<uint8_t>(payload.size() & 0xFF),
                static_cast<uint8_t>((payload.size() >> 8) & 0xFF)
            };
            vector_append(data, payload);
            return data;
        }

        uint8_t read_u8(const data_t& data, size_t& pos, const char* fieldName)
        {
            if (pos >= data.size())
            {
                throw std::invalid_argument(std::string("Unexpected end of structured AppVar while reading ") + fieldName);
            }
            return data[pos++];
        }

        uint16_t read_le16(const data_t& data, size_t& pos, const char* fieldName)
        {
            if (pos + 1 >= data.size())
            {
                throw std::invalid_argument(std::string("Unexpected end of structured AppVar while reading ") + fieldName);
            }
            const uint16_t value = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            return value;
        }

        uint32_t read_le24(const data_t& data, size_t& pos, const char* fieldName)
        {
            if (pos + 2 >= data.size())
            {
                throw std::invalid_argument(std::string("Unexpected end of structured AppVar while reading ") + fieldName);
            }
            const uint32_t value = static_cast<uint32_t>(data[pos])
                                 | (static_cast<uint32_t>(data[pos + 1]) << 8)
                                 | (static_cast<uint32_t>(data[pos + 2]) << 16);
            pos += 3;
            return value;
        }

        std::string read_fixed_string(const data_t& data, size_t& pos, size_t length, const char* fieldName)
        {
            if (pos + length > data.size())
            {
                throw std::invalid_argument(std::string("Unexpected end of structured AppVar while reading ") + fieldName);
            }

            std::string value(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + length));
            pos += length;

            const size_t nulPos = value.find('\0');
            if (nulPos != std::string::npos)
            {
                value.erase(nulPos);
            }

            return value;
        }

        std::string to_hex_string(const data_t& data, size_t offset = 0)
        {
            std::string result;
            for (size_t i = offset; i < data.size(); i++)
            {
                result += dechex(data[i]);
            }
            return result;
        }

        data_t parse_hex_string(const std::string& str, const char* fieldName)
        {
            if (str.size() % 2 != 0)
            {
                throw std::invalid_argument(std::string(fieldName) + " must contain an even number of hex digits");
            }

            data_t out;
            out.reserve(str.size() / 2);
            for (char c : str)
            {
                if (!std::isxdigit(static_cast<unsigned char>(c)))
                {
                    throw std::invalid_argument(std::string(fieldName) + " must be valid hexadecimal");
                }
            }

            for (size_t i = 0; i < str.size(); i += 2)
            {
                out.push_back(hexdec(str.substr(i, 2)));
            }
            return out;
        }

        void append_le16(data_t& data, uint16_t value)
        {
            data.push_back(static_cast<uint8_t>(value & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void append_le24(data_t& data, uint32_t value)
        {
            if (value > 0xFFFFFF)
            {
                throw std::invalid_argument("24-bit AppVar field is out of range");
            }
            data.push_back(static_cast<uint8_t>(value & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        }

        uint32_t read_uleb128(const data_t& data, size_t& pos)
        {
            uint32_t value = 0;
            int shift = 0;
            while (true)
            {
                if (shift > 28)
                {
                    throw std::invalid_argument("ULEB128 field is too large");
                }
                const uint8_t byte = read_u8(data, pos, "ULEB128 value");
                value |= static_cast<uint32_t>(byte & 0x7F) << shift;
                if ((byte & 0x80) == 0)
                {
                    return value;
                }
                shift += 7;
            }
        }

        void append_uleb128(data_t& data, uint32_t value)
        {
            do
            {
                uint8_t byte = static_cast<uint8_t>(value & 0x7F);
                value >>= 7;
                if (value != 0)
                {
                    byte |= 0x80;
                }
                data.push_back(byte);
            }
            while (value != 0);
        }

        StructuredAppVarSubtype subtype_from_json(const json& j)
        {
            if (j.contains("typeName"))
            {
                return subtype_from_type_name(j.at("typeName").get<std::string>());
            }
            if (j.contains("subtype"))
            {
                const std::string subtype = j.at("subtype").get<std::string>();
                if (subtype == "PythonModule")
                {
                    return APPVAR_SUBTYPE_PYTHON_MODULE;
                }
                if (subtype == "PythonImage")
                {
                    return APPVAR_SUBTYPE_PYTHON_IMAGE;
                }
                if (subtype == "StudyCards")
                {
                    return APPVAR_SUBTYPE_STUDY_CARDS;
                }
                if (subtype == "StudyCardsSettings")
                {
                    return APPVAR_SUBTYPE_STUDY_CARDS_SETTINGS;
                }
                if (subtype == "CellSheet")
                {
                    return APPVAR_SUBTYPE_CELSHEET;
                }
                if (subtype == "CellSheetState")
                {
                    return APPVAR_SUBTYPE_CELSHEET_STATE;
                }
                if (subtype == "Notefolio")
                {
                    return APPVAR_SUBTYPE_NOTEFOLIO;
                }
            }
            throw std::invalid_argument("Structured AppVar JSON must contain typeName or subtype");
        }

        StructuredAppVarSubtype subtype_from_data_or_throw(const data_t& data)
        {
            const std::string typeName = detectStructuredAppVarTypeName(data);
            if (typeName == "AppVar" || typeName == "PythonAppVar")
            {
                throw std::invalid_argument("Unsupported structured AppVar payload");
            }
            return subtype_from_type_name(typeName);
        }

        data_t payload_from_json_or_raw(const json& j, StructuredAppVarSubtype subtype)
        {
            if (j.contains("rawDataHex"))
            {
                const data_t payload = parse_hex_string(j.at("rawDataHex").get<std::string>(), "rawDataHex");
                if (subtype_from_data_or_throw(wrap_payload(payload)) != subtype)
                {
                    throw std::invalid_argument("rawDataHex does not match the requested structured AppVar subtype");
                }
                return payload;
            }

            data_t payload;
            switch (subtype)
            {
                case APPVAR_SUBTYPE_PYTHON_MODULE:
                {
                    payload.insert(payload.end(), PYTHON_MODULE_MAGIC, PYTHON_MODULE_MAGIC + magicByteCount);

                    data_t menuData;
                    if (j.contains("menuDefinitionsHex"))
                    {
                        menuData = parse_hex_string(j.at("menuDefinitionsHex").get<std::string>(), "menuDefinitionsHex");
                    }
                    else
                    {
                        std::string menuDefinitions = j.contains("menuDefinitions") ? j.at("menuDefinitions").get<std::string>() : "";
                        const bool nullTerminated = !j.contains("menuDefinitionsNullTerminated") || j.at("menuDefinitionsNullTerminated").get<bool>();
                        menuData.assign(menuDefinitions.begin(), menuDefinitions.end());
                        if (nullTerminated)
                        {
                            menuData.push_back('\0');
                        }
                    }

                    append_uleb128(payload, static_cast<uint32_t>(menuData.size()));
                    payload.push_back(static_cast<uint8_t>(j.at("version").get<int>()));
                    vector_append(payload, menuData);
                    if (j.contains("compiledDataHex"))
                    {
                        vector_append(payload, parse_hex_string(j.at("compiledDataHex").get<std::string>(), "compiledDataHex"));
                    }
                    return payload;
                }

                case APPVAR_SUBTYPE_PYTHON_IMAGE:
                {
                    payload.insert(payload.end(), PYTHON_IMAGE_MAGIC, PYTHON_IMAGE_MAGIC + magicByteCount);
                    append_le24(payload, static_cast<uint32_t>(j.at("width").get<int>()));
                    append_le24(payload, static_cast<uint32_t>(j.at("height").get<int>()));
                    payload.push_back(pythonImagePaletteMarker);

                    const json& palette = j.at("palette");
                    payload.push_back(static_cast<uint8_t>(palette.at("hasAlpha").get<bool>() ? 1 : 0));
                    payload.push_back(static_cast<uint8_t>(palette.at("transparentIndex").get<int>()));

                    const json& entries = palette.at("entries");
                    if (!entries.is_array() || entries.empty() || entries.size() > 256)
                    {
                        throw std::invalid_argument("palette.entries must contain between 1 and 256 values");
                    }
                    payload.push_back(static_cast<uint8_t>(entries.size() == 256 ? 0 : entries.size()));
                    for (const json& value : entries)
                    {
                        append_le16(payload, static_cast<uint16_t>(value.get<int>()));
                    }

                    if (j.contains("imageDataHex"))
                    {
                        vector_append(payload, parse_hex_string(j.at("imageDataHex").get<std::string>(), "imageDataHex"));
                    }
                    return payload;
                }

                case APPVAR_SUBTYPE_STUDY_CARDS:
                    throw std::invalid_argument("StudyCardsAppVar string -> data currently requires rawDataHex");

                case APPVAR_SUBTYPE_STUDY_CARDS_SETTINGS:
                {
                    payload.insert(payload.end(), STUDY_CARDS_SETTINGS_MAGIC.begin(), STUDY_CARDS_SETTINGS_MAGIC.end());

                    int flags = j.value("flags", 0);
                    if (!j.contains("flags"))
                    {
                        flags |= j.value("keepKnownCards", false) ? 0x01 : 0;
                        flags |= j.value("reintroduceCards", false) ? 0x02 : 0;
                        flags |= j.value("shuffleCards", false) ? 0x04 : 0;
                        flags |= j.value("ignoreLevels", false) ? 0x08 : 0;
                        flags |= j.value("animateFlip", false) ? 0x10 : 0;
                        flags |= j.value("boxMode5", false) ? 0x20 : 0;
                    }
                    payload.push_back(static_cast<uint8_t>(flags & 0xFF));

                    const std::string currentAppVar = j.value("currentAppVar", "");
                    if (currentAppVar.size() > studyCardsSettingsNameByteCount)
                    {
                        throw std::invalid_argument("currentAppVar must be at most 9 characters");
                    }
                    const std::string padded = str_pad(currentAppVar, studyCardsSettingsNameByteCount, std::string(1, '\0'));
                    payload.insert(payload.end(), padded.begin(), padded.end());
                    return payload;
                }

                case APPVAR_SUBTYPE_CELSHEET:
                case APPVAR_SUBTYPE_CELSHEET_STATE:
                {
                    const auto& magic = subtype == APPVAR_SUBTYPE_CELSHEET ? CELSHEET_MAGIC : CELSHEET_STATE_MAGIC;
                    payload.insert(payload.end(), magic.begin(), magic.end());

                    const std::string name = j.value("name", "");
                    if (name.size() > cellSheetNameByteCount)
                    {
                        throw std::invalid_argument("name must be at most 8 characters");
                    }
                    const std::string paddedName = str_pad(name, cellSheetNameByteCount, std::string(1, '\0'));
                    payload.insert(payload.end(), paddedName.begin(), paddedName.end());

                    int flags = j.value("flags", 0);
                    if (!j.contains("flags"))
                    {
                        flags |= j.value("displayHelp", true) ? 0 : 0x04;
                        flags |= j.value("displayEquationPreview", true) ? 0 : 0x08;
                    }
                    payload.push_back(static_cast<uint8_t>(flags & 0xFF));
                    payload.push_back(static_cast<uint8_t>(j.value("number", 0) & 0xFF));

                    if (j.contains("payloadHex"))
                    {
                        vector_append(payload, parse_hex_string(j.at("payloadHex").get<std::string>(), "payloadHex"));
                    }
                    return payload;
                }

                case APPVAR_SUBTYPE_NOTEFOLIO:
                {
                    payload.insert(payload.end(), NOTEFOLIO_MAGIC.begin(), NOTEFOLIO_MAGIC.end());

                    const data_t reserved = j.contains("reservedHex")
                        ? parse_hex_string(j.at("reservedHex").get<std::string>(), "reservedHex")
                        : data_t(notefolioReservedByteCount, 0x00);
                    if (reserved.size() != notefolioReservedByteCount)
                    {
                        throw std::invalid_argument("reservedHex must contain exactly 4 bytes");
                    }
                    vector_append(payload, reserved);

                    const std::string name = j.value("name", "");
                    if (name.size() > notefolioNameByteCount)
                    {
                        throw std::invalid_argument("name must be at most 8 characters");
                    }
                    const std::string paddedName = str_pad(name, notefolioNameByteCount, std::string(1, '\0'));
                    payload.insert(payload.end(), paddedName.begin(), paddedName.end());

                    data_t textData;
                    if (j.contains("textDataHex"))
                    {
                        textData = parse_hex_string(j.at("textDataHex").get<std::string>(), "textDataHex");
                    }
                    else
                    {
                        const std::string text = j.value("text", "");
                        textData.assign(text.begin(), text.end());
                        if (j.value("textNullTerminated", true))
                        {
                            textData.push_back('\0');
                        }
                    }

                    const size_t storedTextLength = j.contains("storedTextLength")
                        ? j.at("storedTextLength").get<size_t>()
                        : textData.size();
                    if (storedTextLength != textData.size())
                    {
                        throw std::invalid_argument("storedTextLength must match the actual textData length");
                    }
                    if (storedTextLength > 0xFFFF)
                    {
                        throw std::invalid_argument("storedTextLength is too large");
                    }
                    append_le16(payload, static_cast<uint16_t>(storedTextLength));

                    const data_t headerUnknown = j.contains("headerUnknownHex")
                        ? parse_hex_string(j.at("headerUnknownHex").get<std::string>(), "headerUnknownHex")
                        : data_t(notefolioHeaderUnknownByteCount, 0x00);
                    if (headerUnknown.size() != notefolioHeaderUnknownByteCount)
                    {
                        throw std::invalid_argument("headerUnknownHex must contain exactly 6 bytes");
                    }
                    vector_append(payload, headerUnknown);
                    vector_append(payload, textData);

                    if (j.contains("trailingDataHex"))
                    {
                        vector_append(payload, parse_hex_string(j.at("trailingDataHex").get<std::string>(), "trailingDataHex"));
                    }
                    return payload;
                }
            }

            throw std::invalid_argument("Unsupported structured AppVar subtype");
        }

        json parse_python_module_appvar(const data_t& payload)
        {
            size_t pos = magicByteCount;
            const uint32_t menuLength = read_uleb128(payload, pos);
            const uint8_t version = read_u8(payload, pos, "version");
            if (pos + menuLength > payload.size())
            {
                throw std::invalid_argument("Invalid PythonModuleAppVar data length");
            }

            const data_t menuBytes(payload.begin() + static_cast<ptrdiff_t>(pos), payload.begin() + static_cast<ptrdiff_t>(pos + menuLength));
            pos += menuLength;

            std::string menuDefinitions(menuBytes.begin(), menuBytes.end());
            const bool nullTerminated = !menuDefinitions.empty() && menuDefinitions.back() == '\0';
            if (nullTerminated)
            {
                menuDefinitions.pop_back();
            }

            return json{
                {"typeName", "PythonModuleAppVar"},
                {"subtype", "PythonModule"},
                {"magic", PYTHON_MODULE_MAGIC},
                {"version", version},
                {"menuDefinitionsLength", menuLength},
                {"menuDefinitionsNullTerminated", nullTerminated},
                {"menuDefinitions", menuDefinitions},
                {"menuDefinitionsHex", to_hex_string(menuBytes)},
                {"compiledDataHex", to_hex_string(data_t(payload.begin() + static_cast<ptrdiff_t>(pos), payload.end()))},
                {"rawDataHex", to_hex_string(payload)}
            };
        }

        json parse_python_image_appvar(const data_t& payload)
        {
            size_t pos = magicByteCount;
            const uint32_t width = read_le24(payload, pos, "width");
            const uint32_t height = read_le24(payload, pos, "height");
            const uint8_t paletteMarker = read_u8(payload, pos, "palette marker");
            if (paletteMarker != pythonImagePaletteMarker)
            {
                throw std::invalid_argument("Invalid PythonImageAppVar palette marker");
            }
            const uint8_t hasAlpha = read_u8(payload, pos, "hasAlpha");
            const uint8_t transparentIndex = read_u8(payload, pos, "transparentIndex");
            uint16_t entryCount = read_u8(payload, pos, "palette entry count");
            if (entryCount == 0)
            {
                entryCount = 256;
            }

            json entries = json::array();
            for (uint16_t i = 0; i < entryCount; i++)
            {
                entries.push_back(read_le16(payload, pos, "palette entry"));
            }

            return json{
                {"typeName", "PythonImageAppVar"},
                {"subtype", "PythonImage"},
                {"magic", PYTHON_IMAGE_MAGIC},
                {"width", width},
                {"height", height},
                {"palette", {
                    {"marker", paletteMarker},
                    {"entryCount", entryCount},
                    {"hasAlpha", hasAlpha != 0},
                    {"transparentIndex", transparentIndex},
                    {"entries", entries}
                }},
                {"imageDataLength", payload.size() - pos},
                {"imageDataHex", to_hex_string(data_t(payload.begin() + static_cast<ptrdiff_t>(pos), payload.end()))},
                {"rawDataHex", to_hex_string(payload)}
            };
        }

        json parse_study_cards_appvar(const data_t& payload)
        {
            size_t pos = magicByteCount;
            const uint16_t version = read_le16(payload, pos, "version");
            const uint16_t flags = read_le16(payload, pos, "flags");

            std::array<uint16_t, studyCardsTitleCount> titleOffsets{};
            for (size_t i = 0; i < studyCardsTitleCount; i++)
            {
                titleOffsets[i] = read_le16(payload, pos, "title offset");
            }

            json out = {
                {"typeName", "StudyCardsAppVar"},
                {"subtype", "StudyCards"},
                {"magic", "F347BFA7"},
                {"version", version},
                {"flags", flags},
                {"usesLevels", (flags & 0x0001) != 0},
                {"selfCheck", (flags & 0x0002) != 0},
                {"titleOffsets", json::array()},
            };
            for (uint16_t offset : titleOffsets)
            {
                out["titleOffsets"].push_back(offset);
            }

            if ((flags & 0x0001) != 0)
            {
                out["levelCount"] = read_u8(payload, pos, "level count");
            }

            out["correctReward"] = read_u8(payload, pos, "correct reward");
            out["incorrectPenalty"] = read_u8(payload, pos, "incorrect penalty");
            out["skipPenalty"] = read_u8(payload, pos, "skip penalty");
            const uint8_t cardCount = read_u8(payload, pos, "card count");
            out["cardCount"] = cardCount;

            json cardOffsets = json::array();
            for (uint8_t i = 0; i < cardCount; i++)
            {
                cardOffsets.push_back(read_le16(payload, pos, "card offset"));
            }
            out["cardOffsets"] = cardOffsets;

            json titles = json::array();
            for (uint16_t offset : titleOffsets)
            {
                std::string title;
                if (offset < payload.size())
                {
                    for (size_t i = offset; i < payload.size() && payload[i] != '\0'; i++)
                    {
                        title.push_back(static_cast<char>(payload[i]));
                    }
                }
                titles.push_back(title);
            }
            out["titles"] = titles;
            out["rawDataHex"] = to_hex_string(payload);
            return out;
        }

        json parse_study_cards_settings_appvar(const data_t& payload)
        {
            size_t pos = magicByteCount;
            const uint8_t flags = read_u8(payload, pos, "flags");
            const std::string currentAppVar = read_fixed_string(payload, pos, studyCardsSettingsNameByteCount, "current appvar");

            return json{
                {"typeName", "StudyCardsSetgsAppVar"},
                {"subtype", "StudyCardsSettings"},
                {"magic", "F347BFA8"},
                {"flags", flags},
                {"keepKnownCards", (flags & 0x01) != 0},
                {"reintroduceCards", (flags & 0x02) != 0},
                {"shuffleCards", (flags & 0x04) != 0},
                {"ignoreLevels", (flags & 0x08) != 0},
                {"animateFlip", (flags & 0x10) != 0},
                {"boxMode5", (flags & 0x20) != 0},
                {"currentAppVar", currentAppVar},
                {"rawDataHex", to_hex_string(payload)}
            };
        }

        json parse_cellsheet_appvar(const data_t& payload, StructuredAppVarSubtype subtype)
        {
            size_t pos = magicByteCount;
            const std::string name = read_fixed_string(payload, pos, cellSheetNameByteCount, "name");
            const uint8_t flags = read_u8(payload, pos, "flags");
            const uint8_t number = read_u8(payload, pos, "number");

            const bool isCellSheet = subtype == APPVAR_SUBTYPE_CELSHEET;
            return json{
                {"typeName", isCellSheet ? "CellSheetAppVar" : "CellSheetStateAppVar"},
                {"subtype", isCellSheet ? "CellSheet" : "CellSheetState"},
                {"magic", isCellSheet ? "F347BFAA" : "F347BFAB"},
                {"name", name},
                {"flags", flags},
                {"displayHelp", (flags & 0x04) == 0},
                {"displayEquationPreview", (flags & 0x08) == 0},
                {"number", number},
                {"payloadHex", to_hex_string(data_t(payload.begin() + static_cast<ptrdiff_t>(pos), payload.end()))},
                {"rawDataHex", to_hex_string(payload)}
            };
        }

        json parse_notefolio_appvar(const data_t& payload)
        {
            size_t pos = magicByteCount;
            if (pos + notefolioReservedByteCount + notefolioNameByteCount + 2 + notefolioHeaderUnknownByteCount > payload.size())
            {
                throw std::invalid_argument("Invalid NotefolioAppVar data length");
            }
            const data_t reserved(payload.begin() + static_cast<ptrdiff_t>(pos),
                                  payload.begin() + static_cast<ptrdiff_t>(pos + notefolioReservedByteCount));
            pos += notefolioReservedByteCount;
            const std::string name = read_fixed_string(payload, pos, notefolioNameByteCount, "name");
            const uint16_t storedTextLength = read_le16(payload, pos, "stored text length");
            const data_t headerUnknown(payload.begin() + static_cast<ptrdiff_t>(pos),
                                       payload.begin() + static_cast<ptrdiff_t>(pos + notefolioHeaderUnknownByteCount));
            pos += notefolioHeaderUnknownByteCount;

            if (pos + storedTextLength > payload.size())
            {
                throw std::invalid_argument("Invalid NotefolioAppVar text length");
            }

            const data_t textData(payload.begin() + static_cast<ptrdiff_t>(pos),
                                  payload.begin() + static_cast<ptrdiff_t>(pos + storedTextLength));
            pos += storedTextLength;
            const data_t trailingData(payload.begin() + static_cast<ptrdiff_t>(pos), payload.end());

            std::string text(textData.begin(), textData.end());
            const size_t nulPos = text.find('\0');
            const bool textNullTerminated = nulPos != std::string::npos;
            if (textNullTerminated)
            {
                text.erase(nulPos);
            }

            return json{
                {"typeName", "NotefolioAppVar"},
                {"subtype", "Notefolio"},
                {"magic", "F347BFAF"},
                {"reservedHex", to_hex_string(reserved)},
                {"name", name},
                {"storedTextLength", storedTextLength},
                {"headerUnknownHex", to_hex_string(headerUnknown)},
                {"text", text},
                {"textNullTerminated", textNullTerminated},
                {"textDataHex", to_hex_string(textData)},
                {"trailingDataHex", to_hex_string(trailingData)},
                {"rawDataHex", to_hex_string(payload)}
            };
        }
    }

    std::string detectStructuredAppVarTypeName(const data_t& data)
    {
        if (has_prefix(data, STH_PythonAppVar::ID_CODE) || has_prefix(data, STH_PythonAppVar::ID_SCRIPT))
        {
            return "PythonAppVar";
        }
        if (has_prefix(data, PYTHON_MODULE_MAGIC))
        {
            return "PythonModuleAppVar";
        }
        if (has_prefix(data, PYTHON_IMAGE_MAGIC))
        {
            return "PythonImageAppVar";
        }
        if (has_prefix(data, STUDY_CARDS_MAGIC))
        {
            return "StudyCardsAppVar";
        }
        if (has_prefix(data, STUDY_CARDS_SETTINGS_MAGIC))
        {
            return "StudyCardsSetgsAppVar";
        }
        if (has_prefix(data, CELSHEET_MAGIC))
        {
            return "CellSheetAppVar";
        }
        if (has_prefix(data, CELSHEET_STATE_MAGIC))
        {
            return "CellSheetStateAppVar";
        }
        if (has_prefix(data, NOTEFOLIO_MAGIC))
        {
            return "NotefolioAppVar";
        }
        return "AppVar";
    }

    data_t TH_StructuredAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        const json j = json::parse(str);
        StructuredAppVarSubtype subtype;
        if (options.contains("_appvarSubtype"))
        {
            subtype = static_cast<StructuredAppVarSubtype>(options.at("_appvarSubtype"));
        }
        else
        {
            subtype = subtype_from_json(j);
        }

        return wrap_payload(payload_from_json_or_raw(j, subtype));
    }

    std::string TH_StructuredAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const data_t payload = payload_from_data(data);
        const StructuredAppVarSubtype subtype = subtype_from_data_or_throw(data);

        switch (subtype)
        {
            case APPVAR_SUBTYPE_PYTHON_MODULE:
                return parse_python_module_appvar(payload).dump(4);
            case APPVAR_SUBTYPE_PYTHON_IMAGE:
                return parse_python_image_appvar(payload).dump(4);
            case APPVAR_SUBTYPE_STUDY_CARDS:
                return parse_study_cards_appvar(payload).dump(4);
            case APPVAR_SUBTYPE_STUDY_CARDS_SETTINGS:
                return parse_study_cards_settings_appvar(payload).dump(4);
            case APPVAR_SUBTYPE_CELSHEET:
            case APPVAR_SUBTYPE_CELSHEET_STATE:
                return parse_cellsheet_appvar(payload, subtype).dump(4);
            case APPVAR_SUBTYPE_NOTEFOLIO:
                return parse_notefolio_appvar(payload).dump(4);
        }

        throw std::invalid_argument("Unsupported structured AppVar subtype");
    }

    uint8_t TH_StructuredAppVar::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
