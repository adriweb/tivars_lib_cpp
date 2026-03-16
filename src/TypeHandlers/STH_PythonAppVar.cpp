/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"

#include "../json.hpp"
#include "../tivarslib_utils.h"

#include <stdexcept>
#include <cstring>

using json = nlohmann::json;

namespace tivars::TypeHandlers
{
    namespace
    {
        constexpr size_t sizePrefixByteCount = 2;
        constexpr size_t magicByteCount = 4;
        constexpr size_t minimumPayloadByteCount = magicByteCount + 1;
        constexpr uint8_t metadataRecordTypeFilename = 0x01;
        constexpr size_t maxMetadataLengthByteCount = 2;
        constexpr size_t maxTotalByteCount = 65514;
        constexpr size_t maxPayloadByteCount = maxTotalByteCount - sizePrefixByteCount;

        struct python_metadata_record_t
        {
            uint8_t type = 0x00;
            data_t data;
        };

        std::string default_magic()
        {
            return std::string(STH_PythonAppVar::ID_CODE, magicByteCount);
        }

        bool is_valid_magic(const uint8_t* data)
        {
            return std::memcmp(data, STH_PythonAppVar::ID_CODE, magicByteCount) == 0
                || std::memcmp(data, STH_PythonAppVar::ID_SCRIPT, magicByteCount) == 0;
        }

        std::string read_magic(const data_t& data)
        {
            return std::string(data.begin() + sizePrefixByteCount,
                               data.begin() + (sizePrefixByteCount + magicByteCount));
        }

        std::string to_hex_string(const data_t& data)
        {
            std::string hex;
            hex.reserve(data.size() * 2);
            for (uint8_t byte : data)
            {
                hex += dechex(byte);
            }
            return hex;
        }

        std::string strip_utf8_bom(std::string text)
        {
            if (text.size() >= 3
             && static_cast<uint8_t>(text[0]) == 0xEF
             && static_cast<uint8_t>(text[1]) == 0xBB
             && static_cast<uint8_t>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }
            return text;
        }

        std::string trim_trailing_crlf(std::string text)
        {
            while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
            {
                text.pop_back();
            }
            return text;
        }

        std::string normalize_line_endings(std::string text)
        {
            std::string normalized;
            normalized.reserve(text.size());
            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == '\r')
                {
                    if (i + 1 < text.size() && text[i + 1] == '\n')
                    {
                        i++;
                    }
                    normalized.push_back('\n');
                }
                else
                {
                    normalized.push_back(text[i]);
                }
            }
            return normalized;
        }

        std::string readable_script_from_bytes(const data_t& scriptBytes)
        {
            std::string script(scriptBytes.begin(), scriptBytes.end());
            script = strip_utf8_bom(script);
            return normalize_line_endings(script);
        }

        data_t encode_metadata_length(size_t value)
        {
            if (value == 0)
            {
                throw std::invalid_argument("Python metadata record length cannot be zero");
            }

            data_t out;
            do
            {
                uint8_t byte = static_cast<uint8_t>(value & 0x7F);
                value >>= 7;
                if (value != 0)
                {
                    byte |= 0x80;
                }
                out.push_back(byte);
            }
            while (value != 0);

            if (out.size() > maxMetadataLengthByteCount)
            {
                throw std::invalid_argument("Python metadata record length exceeds supported size");
            }
            return out;
        }

        size_t decode_metadata_length(const data_t& data, size_t& pos)
        {
            size_t value = 0;
            int shift = 0;
            size_t lengthByteCount = 0;
            while (pos < data.size())
            {
                const uint8_t byte = data[pos++];
                value |= static_cast<size_t>(byte & 0x7F) << shift;
                lengthByteCount++;
                if ((byte & 0x80) == 0)
                {
                    if (value == 0)
                    {
                        throw std::invalid_argument("Invalid Python AppVar metadata record length");
                    }
                    return value;
                }
                shift += 7;
                if (lengthByteCount >= maxMetadataLengthByteCount)
                {
                    throw std::invalid_argument("Python AppVar metadata record length exceeds supported size");
                }
            }

            throw std::invalid_argument("Unexpected end of Python AppVar metadata");
        }

        std::vector<python_metadata_record_t> parse_metadata_records(const data_t& data, size_t& scriptOffset)
        {
            std::vector<python_metadata_record_t> records;
            size_t pos = sizePrefixByteCount + magicByteCount;
            while (pos < data.size())
            {
                if (data[pos] == 0x00)
                {
                    scriptOffset = pos + 1;
                    return records;
                }

                const size_t recordLength = decode_metadata_length(data, pos);
                if (pos >= data.size())
                {
                    throw std::invalid_argument("Unexpected end of Python AppVar metadata record");
                }

                python_metadata_record_t record{};
                record.type = data[pos++];
                const size_t recordDataLength = recordLength - 1;
                if (pos + recordDataLength > data.size())
                {
                    throw std::invalid_argument("Unexpected end of Python AppVar metadata record");
                }
                record.data.assign(data.begin() + static_cast<ptrdiff_t>(pos),
                                   data.begin() + static_cast<ptrdiff_t>(pos + recordDataLength));
                pos += recordDataLength;
                records.push_back(record);
            }

            throw std::invalid_argument("Python AppVar metadata terminator not found");
        }

        data_t build_payload(const std::string& magic,
                             const std::vector<python_metadata_record_t>& records,
                             const data_t& scriptBytes,
                             bool preserveTrailingCRLF = false)
        {
            if (magic.size() != magicByteCount || !is_valid_magic(reinterpret_cast<const uint8_t*>(magic.data())))
            {
                throw std::invalid_argument("Python AppVar magic must be PYCD or PYSC");
            }

            size_t basePayloadSize = magicByteCount + 1;
            for (const auto& [type, data] : records)
            {
                (void)type;
                const data_t encodedLength = encode_metadata_length(data.size() + 1);
                basePayloadSize += encodedLength.size() + 1 + data.size();
            }
            if (basePayloadSize > maxPayloadByteCount)
            {
                throw std::invalid_argument("Invalid input string. Python AppVar metadata is too large");
            }

            data_t fittedScriptBytes = scriptBytes;
            const size_t maxScriptByteCount = maxPayloadByteCount - basePayloadSize;
            if (fittedScriptBytes.size() > maxScriptByteCount)
            {
                if (preserveTrailingCRLF
                 && fittedScriptBytes.size() >= 2
                 && fittedScriptBytes[fittedScriptBytes.size() - 2] == '\r'
                 && fittedScriptBytes.back() == '\n')
                {
                    if (maxScriptByteCount < 2)
                    {
                        throw std::invalid_argument("Invalid input string. Python AppVar metadata leaves no room for trailing CRLF");
                    }
                    fittedScriptBytes.resize(maxScriptByteCount);
                    fittedScriptBytes[maxScriptByteCount - 2] = '\r';
                    fittedScriptBytes[maxScriptByteCount - 1] = '\n';
                }
                else
                {
                    fittedScriptBytes.resize(maxScriptByteCount);
                }
            }

            data_t payload;
            payload.reserve(basePayloadSize + fittedScriptBytes.size());
            payload.insert(payload.end(), magic.begin(), magic.end());

            for (const auto& [type, data] : records)
            {
                const data_t encodedLength = encode_metadata_length(data.size() + 1);
                payload.insert(payload.end(), encodedLength.begin(), encodedLength.end());
                payload.push_back(type);
                payload.insert(payload.end(), data.begin(), data.end());
            }

            payload.push_back(0x00);
            payload.insert(payload.end(), fittedScriptBytes.begin(), fittedScriptBytes.end());

            data_t data(sizePrefixByteCount + payload.size());
            data[0] = static_cast<uint8_t>(payload.size() & 0xFF);
            data[1] = static_cast<uint8_t>((payload.size() >> 8) & 0xFF);
            std::ranges::copy(payload, data.begin() + sizePrefixByteCount);
            return data;
        }

        data_t parse_payload_hex(const std::string& hex)
        {
            if (hex.size() % 2 != 0)
            {
                throw std::invalid_argument("rawDataHex must contain an even number of hex digits");
            }

            data_t payload;
            payload.reserve(hex.size() / 2);
            for (size_t i = 0; i < hex.size(); i += 2)
            {
                payload.push_back(hexdec(hex.substr(i, 2)));
            }

            if (payload.size() < minimumPayloadByteCount || !is_valid_magic(payload.data()))
            {
                throw std::invalid_argument("rawDataHex does not contain a valid Python AppVar payload");
            }

            data_t data(sizePrefixByteCount + payload.size());
            data[0] = static_cast<uint8_t>(payload.size() & 0xFF);
            data[1] = static_cast<uint8_t>((payload.size() >> 8) & 0xFF);
            std::ranges::copy(payload, data.begin() + sizePrefixByteCount);

            size_t scriptOffset = 0;
            (void)parse_metadata_records(data, scriptOffset);
            return data;
        }

        data_t script_bytes_from_text(std::string text, bool appendTrailingCRLF)
        {
            text = strip_utf8_bom(text);
            if (appendTrailingCRLF)
            {
                text = trim_trailing_crlf(text);
                text += "\r\n";
            }
            return data_t(text.begin(), text.end());
        }

        json metadata_record_to_json(const python_metadata_record_t& record)
        {
            json out = {
                {"type", record.type},
                {"dataHex", to_hex_string(record.data)},
                {"length", record.data.size() + 1},
            };
            if (record.type == metadataRecordTypeFilename)
            {
                out["name"] = std::string(record.data.begin(), record.data.end());
            }
            return out;
        }

        std::vector<python_metadata_record_t> metadata_records_from_json(const json& j)
        {
            std::vector<python_metadata_record_t> records;

            if (j.contains("filename"))
            {
                const std::string filename = j.at("filename").get<std::string>();
                records.push_back({metadataRecordTypeFilename, data_t(filename.begin(), filename.end())});
            }

            if (j.contains("metadataRecords"))
            {
                if (!j.at("metadataRecords").is_array())
                {
                    throw std::invalid_argument("metadataRecords must be an array");
                }

                for (const json& recordJson : j.at("metadataRecords"))
                {
                    python_metadata_record_t record{};
                    record.type = static_cast<uint8_t>(recordJson.at("type").get<int>() & 0xFF);
                    if (record.type == metadataRecordTypeFilename && j.contains("filename"))
                    {
                        continue;
                    }

                    if (recordJson.contains("dataHex"))
                    {
                        const std::string hex = recordJson.at("dataHex").get<std::string>();
                        if (hex.size() % 2 != 0)
                        {
                            throw std::invalid_argument("metadataRecords.dataHex must contain an even number of hex digits");
                        }
                        record.data.reserve(hex.size() / 2);
                        for (size_t i = 0; i < hex.size(); i += 2)
                        {
                            record.data.push_back(hexdec(hex.substr(i, 2)));
                        }
                    }
                    else if (recordJson.contains("name"))
                    {
                        const std::string name = recordJson.at("name").get<std::string>();
                        record.data.assign(name.begin(), name.end());
                    }
                    else if (recordJson.contains("text"))
                    {
                        const std::string text = recordJson.at("text").get<std::string>();
                        record.data.assign(text.begin(), text.end());
                    }
                    else
                    {
                        throw std::invalid_argument("metadataRecords entries need dataHex, name, or text");
                    }

                    records.push_back(record);
                }
            }

            return records;
        }
    }

    data_t STH_PythonAppVar::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        const std::string trimmed = trim(str);
        if (!trimmed.empty() && trimmed.front() == '{')
        {
            const json j = json::parse(trimmed);
            if (j.contains("rawDataHex"))
            {
                return parse_payload_hex(j.at("rawDataHex").get<std::string>());
            }

            const std::string magic = j.value("magic", default_magic());
            const std::vector<python_metadata_record_t> records = metadata_records_from_json(j);

            std::string text;
            if (j.contains("code"))
            {
                text = j.at("code").get<std::string>();
            }
            else if (j.contains("script"))
            {
                text = j.at("script").get<std::string>();
            }
            else if (j.contains("text"))
            {
                text = j.at("text").get<std::string>();
            }

            const bool appendTrailingCRLF = j.contains("appendTrailingCRLF") && j.at("appendTrailingCRLF").get<bool>();
            const data_t scriptBytes = script_bytes_from_text(text, appendTrailingCRLF);
            return build_payload(magic, records, scriptBytes, appendTrailingCRLF);
        }

        const data_t scriptBytes = script_bytes_from_text(str, false);
        return build_payload(default_magic(), {}, scriptBytes);
    }

    std::string STH_PythonAppVar::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;

        const size_t byteCount = data.size();
        const size_t lengthDat = byteCount - sizePrefixByteCount;

        if (byteCount < sizePrefixByteCount + minimumPayloadByteCount)
        {
            throw std::invalid_argument("Invalid data array. Need at least 7 bytes, got " + std::to_string(lengthDat));
        }

        const size_t lengthExp = static_cast<size_t>((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        if (lengthExp != lengthDat)
        {
            throw std::invalid_argument("Invalid data array. Expected " + std::to_string(lengthExp) + " bytes, got " + std::to_string(lengthDat));
        }

        if (!is_valid_magic(&data[sizePrefixByteCount]))
        {
            throw std::invalid_argument("Invalid data array. Magic header 'PYCD' or 'PYSC' not found");
        }

        size_t scriptOffset = 0;
        const std::vector<python_metadata_record_t> records = parse_metadata_records(data, scriptOffset);
        const data_t scriptBytes(data.begin() + static_cast<ptrdiff_t>(scriptOffset), data.end());
        const std::string readableScript = readable_script_from_bytes(scriptBytes);

        if (options.contains("metadata") && options.at("metadata") == 1)
        {
            json metadataRecords = json::array();
            std::string filename;
            for (const auto& record : records)
            {
                metadataRecords.push_back(metadata_record_to_json(record));
                if (record.type == metadataRecordTypeFilename && filename.empty())
                {
                    filename.assign(record.data.begin(), record.data.end());
                }
            }

            json out = {
                {"typeName", "PythonAppVar"},
                {"magic", read_magic(data)},
                {"metadataRecordCount", records.size()},
                {"metadataRecords", metadataRecords},
                {"code", readableScript},
                {"codeHex", to_hex_string(scriptBytes)},
                {"rawDataHex", to_hex_string(data_t(data.begin() + sizePrefixByteCount, data.end()))},
            };
            if (!filename.empty())
            {
                out["filename"] = filename;
            }
            return out.dump(4);
        }

        return readableScript;
    }

    TIVarFileMinVersionByte STH_PythonAppVar::getMinVersionFromData(const data_t& data)
    {
        (void)data;
        return VER_NONE;
    }
}
