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

#include <cctype>
#include <stdexcept>

using namespace std::string_literals;
using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        uint16_t read_le16_at(const data_t& data, size_t offset, const char* fieldName)
        {
            if (offset + 1 >= data.size())
            {
                throw std::invalid_argument("Invalid backup data. Missing "s + fieldName);
            }
            return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
        }

        void append_le16(data_t& data, uint16_t value)
        {
            data.push_back(static_cast<uint8_t>(value & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        std::string to_hex_string(const data_t& data)
        {
            std::string result;
            for (uint8_t byte : data)
            {
                result += dechex(byte);
            }
            return result;
        }

        data_t parse_hex_string(const std::string& str, const char* fieldName)
        {
            if (str.size() % 2 != 0)
            {
                throw std::invalid_argument(std::string(fieldName) + " must contain an even number of hex digits");
            }

            for (char c : str)
            {
                if (!std::isxdigit(static_cast<unsigned char>(c)))
                {
                    throw std::invalid_argument(std::string(fieldName) + " must be valid hexadecimal");
                }
            }

            data_t out;
            out.reserve(str.size() / 2);
            for (size_t i = 0; i < str.size(); i += 2)
            {
                out.push_back(hexdec(str.substr(i, 2)));
            }
            return out;
        }
    }

    TH_Backup::backup_contents_t TH_Backup::parseInternal(const data_t& data)
    {
        if (data.empty())
        {
            throw std::invalid_argument("Invalid backup data. Empty payload");
        }

        const uint8_t segmentCount = data[0];
        if (segmentCount != internalSegmentCount3 && segmentCount != internalSegmentCount4)
        {
            throw std::invalid_argument("Invalid backup data. Unsupported segment count");
        }

        const bool hasData4 = segmentCount == internalSegmentCount4;
        const size_t headerSize = hasData4 ? internalHeaderByteCount4 : internalHeaderByteCount3;
        if (data.size() < headerSize)
        {
            throw std::invalid_argument("Invalid backup data. Header is truncated");
        }

        backup_contents_t contents;
        contents.hasData4 = hasData4;
        const uint16_t len1 = read_le16_at(data, 1, "data1 length");
        const uint16_t len2 = read_le16_at(data, 3, "data2 length");
        const uint16_t len3 = read_le16_at(data, 5, "data3 length");
        contents.addressOfData2 = read_le16_at(data, 7, "addressOfData2");
        const uint16_t len4 = hasData4 ? read_le16_at(data, 9, "data4 length") : 0;

        const size_t expectedSize = headerSize + len1 + len2 + len3 + len4;
        if (data.size() != expectedSize)
        {
            throw std::invalid_argument("Invalid backup data. Length fields do not match payload size");
        }

        size_t pos = headerSize;
        contents.data1 = data_t(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + len1));
        pos += len1;
        contents.data2 = data_t(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + len2));
        pos += len2;
        contents.data3 = data_t(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + len3));
        pos += len3;
        if (hasData4)
        {
            contents.data4 = data_t(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + len4));
        }

        return contents;
    }

    data_t TH_Backup::buildInternal(const backup_contents_t& contents)
    {
        if (contents.data1.size() > 0xFFFF || contents.data2.size() > 0xFFFF || contents.data3.size() > 0xFFFF || contents.data4.size() > 0xFFFF)
        {
            throw std::invalid_argument("Backup data segment is too large");
        }

        data_t data;
        data.push_back(contents.hasData4 ? internalSegmentCount4 : internalSegmentCount3);
        append_le16(data, static_cast<uint16_t>(contents.data1.size()));
        append_le16(data, static_cast<uint16_t>(contents.data2.size()));
        append_le16(data, static_cast<uint16_t>(contents.data3.size()));
        append_le16(data, contents.addressOfData2);
        if (contents.hasData4)
        {
            append_le16(data, static_cast<uint16_t>(contents.data4.size()));
        }
        vector_append(data, contents.data1);
        vector_append(data, contents.data2);
        vector_append(data, contents.data3);
        if (contents.hasData4)
        {
            vector_append(data, contents.data4);
        }
        return data;
    }

    data_t TH_Backup::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const json j = json::parse(str);
        backup_contents_t contents;
        contents.data1 = parse_hex_string(j.at("data1Hex").get<std::string>(), "data1Hex");
        contents.data2 = parse_hex_string(j.at("data2Hex").get<std::string>(), "data2Hex");
        contents.data3 = parse_hex_string(j.at("data3Hex").get<std::string>(), "data3Hex");
        contents.addressOfData2 = static_cast<uint16_t>(j.value("addressOfData2", 0));
        if (j.contains("data4Hex"))
        {
            contents.hasData4 = true;
            contents.data4 = parse_hex_string(j.at("data4Hex").get<std::string>(), "data4Hex");
        }

        return buildInternal(contents);
    }

    std::string TH_Backup::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const backup_contents_t contents = parseInternal(data);
        json j = {
            {"typeName", "Backup"},
            {"segmentCount", contents.hasData4 ? 4 : 3},
            {"addressOfData2", contents.addressOfData2},
            {"data1Length", contents.data1.size()},
            {"data1Hex", to_hex_string(contents.data1)},
            {"data2Length", contents.data2.size()},
            {"data2Hex", to_hex_string(contents.data2)},
            {"data3Length", contents.data3.size()},
            {"data3Hex", to_hex_string(contents.data3)},
        };
        if (contents.hasData4)
        {
            j["data4Length"] = contents.data4.size();
            j["data4Hex"] = to_hex_string(contents.data4);
        }
        return j.dump(4);
    }

    uint8_t TH_Backup::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return 0;
    }
}
