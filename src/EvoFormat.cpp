/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "EvoFormat.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <unordered_map>

#include "TIVarTypes.h"
#include "TypeHandlers/TypeHandlers.h"
#include "tivarslib_utils.h"

namespace tivars::EvoFormat
{
    namespace
    {
        std::string normalize_theta_chars(std::string name)
        {
            for (const auto& token : {"θ", "Θ", "ϴ", "ᶿ"})
            {
                replace_all(name, token, "[");
            }
            return name;
        }

        std::string utf8_from_codepoint(uint16_t codepoint)
        {
            if (codepoint < 0x80)
            {
                return std::string(1, static_cast<char>(codepoint));
            }
            if (codepoint < 0x800)
            {
                return {
                    static_cast<char>(0xC0 | (codepoint >> 6)),
                    static_cast<char>(0x80 | (codepoint & 0x3F)),
                };
            }
            return {
                static_cast<char>(0xE0 | (codepoint >> 12)),
                static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)),
                static_cast<char>(0x80 | (codepoint & 0x3F)),
            };
        }
    }

uint16_t evo_checksum(const data_t& body)
{
    if (body.size() < 3)
    {
        return 0;
    }

    size_t adjusted = body.size() - 3;
    size_t wordCount = adjusted >> 1;
    if ((adjusted & 1U) != 0 && wordCount > 0)
    {
        wordCount--;
    }

    uint16_t checksum = 0;
    for (size_t i = 0; i < wordCount; i++)
    {
        checksum ^= static_cast<uint16_t>(body[i * 2] | (body[i * 2 + 1] << 8));
    }
    return checksum;
}

uint64_t read_cbor_uint_arg(const data_t& data, size_t& offset, uint8_t additional)
{
    if (additional < 24)
    {
        return additional;
    }
    if (additional == 24)
    {
        if (offset >= data.size()) throw std::invalid_argument("Invalid Evo CBOR integer");
        return data[offset++];
    }
    if (additional == 25)
    {
        if (offset + 1 >= data.size()) throw std::invalid_argument("Invalid Evo CBOR integer");
        const uint64_t value = (static_cast<uint64_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        return value;
    }
    if (additional == 26)
    {
        if (offset + 3 >= data.size()) throw std::invalid_argument("Invalid Evo CBOR integer");
        const uint64_t value = (static_cast<uint64_t>(data[offset]) << 24)
                             | (static_cast<uint64_t>(data[offset + 1]) << 16)
                             | (static_cast<uint64_t>(data[offset + 2]) << 8)
                             | data[offset + 3];
        offset += 4;
        return value;
    }
    throw std::invalid_argument("Unsupported Evo CBOR integer width");
}

EvoCBORValue parse_cbor_value(const data_t& data, size_t& offset)
{
    if (offset >= data.size())
    {
        throw std::invalid_argument("Unexpected end of Evo CBOR data");
    }

    const size_t start = offset;
    const uint8_t initial = data[offset++];
    if (initial == 0xFF)
    {
        EvoCBORValue value;
        value.raw = { initial };
        return value;
    }

    const uint8_t major = initial >> 5;
    const uint8_t additional = initial & 0x1F;
    EvoCBORValue value;

    switch (major)
    {
        case 0:
            value.kind = EvoCBORValue::Kind::Unsigned;
            value.unsignedValue = read_cbor_uint_arg(data, offset, additional);
            break;

        case 2:
        {
            value.kind = EvoCBORValue::Kind::Bytes;
            const uint64_t len = read_cbor_uint_arg(data, offset, additional);
            if (offset + len > data.size()) throw std::invalid_argument("Invalid Evo CBOR byte string length");
            value.bytes.assign(data.begin() + static_cast<long long>(offset), data.begin() + static_cast<long long>(offset + len));
            offset += static_cast<size_t>(len);
            break;
        }

        case 3:
        {
            value.kind = EvoCBORValue::Kind::Text;
            const uint64_t len = read_cbor_uint_arg(data, offset, additional);
            if (offset + len > data.size()) throw std::invalid_argument("Invalid Evo CBOR text string length");
            value.text.assign(reinterpret_cast<const char*>(data.data() + offset), static_cast<size_t>(len));
            offset += static_cast<size_t>(len);
            break;
        }

        case 4:
        {
            value.kind = EvoCBORValue::Kind::Array;
            if (additional == 31)
            {
                while (offset < data.size() && data[offset] != 0xFF)
                {
                    value.array.push_back(parse_cbor_value(data, offset));
                }
                if (offset >= data.size()) throw std::invalid_argument("Unterminated Evo CBOR array");
                offset++;
            }
            else
            {
                const uint64_t count = read_cbor_uint_arg(data, offset, additional);
                for (uint64_t i = 0; i < count; i++)
                {
                    value.array.push_back(parse_cbor_value(data, offset));
                }
            }
            break;
        }

        case 5:
        {
            value.kind = EvoCBORValue::Kind::Map;
            const auto read_one_pair = [&]()
            {
                EvoCBORValue key = parse_cbor_value(data, offset);
                EvoCBORValue child = parse_cbor_value(data, offset);
                if (key.kind == EvoCBORValue::Kind::Text)
                {
                    value.map[key.text] = child;
                }
            };

            if (additional == 31)
            {
                while (offset < data.size() && data[offset] != 0xFF)
                {
                    read_one_pair();
                }
                if (offset >= data.size()) throw std::invalid_argument("Unterminated Evo CBOR map");
                offset++;
            }
            else
            {
                const uint64_t count = read_cbor_uint_arg(data, offset, additional);
                for (uint64_t i = 0; i < count; i++)
                {
                    read_one_pair();
                }
            }
            break;
        }

        default:
            value.kind = EvoCBORValue::Kind::Simple;
            if (additional >= 24)
            {
                (void)read_cbor_uint_arg(data, offset, additional);
            }
            break;
    }

    value.raw.assign(data.begin() + static_cast<long long>(start), data.begin() + static_cast<long long>(offset));
    return value;
}

bool is_evo_file_data(const data_t& fileData)
{
    if (fileData.size() < 3 || fileData.front() != 0xBF)
    {
        return false;
    }

    try
    {
        const data_t body(fileData.begin(), fileData.end() - 2);
        size_t offset = 0;
        const EvoCBORValue root = parse_cbor_value(body, offset);
        if (root.kind != EvoCBORValue::Kind::Map || offset != body.size())
        {
            return false;
        }

        const auto metaIt = root.map.find("metaData");
        if (metaIt == root.map.end() || metaIt->second.kind != EvoCBORValue::Kind::Map)
        {
            return false;
        }

        const auto& meta = metaIt->second.map;
        const auto has_uint = [&meta](const std::string& key)
        {
            const auto it = meta.find(key);
            return it != meta.end() && it->second.kind == EvoCBORValue::Kind::Unsigned;
        };
        const auto typeIt = meta.find("type");
        const auto nameIt = meta.find("name");
        if (typeIt == meta.end() || typeIt->second.kind != EvoCBORValue::Kind::Unsigned
            || !has_uint("version") || !has_uint("flags")
            || nameIt == meta.end() || nameIt->second.kind != EvoCBORValue::Kind::Bytes)
        {
            return false;
        }

        return typeIt->second.unsignedValue <= 17;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void append_cbor_uint(data_t& out, uint64_t value)
{
    if (value < 24)
    {
        out.push_back(static_cast<uint8_t>(value));
    }
    else if (value <= 0xFF)
    {
        out.push_back(0x18);
        out.push_back(static_cast<uint8_t>(value));
    }
    else if (value <= 0xFFFF)
    {
        out.push_back(0x19);
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value & 0xFF));
    }
    else
    {
        out.push_back(0x1A);
        out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(value & 0xFF));
    }
}

void append_cbor_text(data_t& out, std::string_view text)
{
    const size_t len = text.size();
    if (len < 24)
    {
        out.push_back(static_cast<uint8_t>(0x60 | len));
    }
    else
    {
        out.push_back(0x78);
        out.push_back(static_cast<uint8_t>(len));
    }
    out.insert(out.end(), text.begin(), text.end());
}

void append_cbor_bytes(data_t& out, const data_t& bytes)
{
    const size_t len = bytes.size();
    if (len < 24)
    {
        out.push_back(static_cast<uint8_t>(0x40 | len));
    }
    else if (len <= 0xFF)
    {
        out.push_back(0x58);
        out.push_back(static_cast<uint8_t>(len));
    }
    else
    {
        out.push_back(0x59);
        out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(len & 0xFF));
    }
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void append_cbor_key_uint(data_t& out, std::string_view key, uint64_t value)
{
    append_cbor_text(out, key);
    append_cbor_uint(out, value);
}

void append_evo_name_word(data_t& out, uint16_t word)
{
    out.push_back(static_cast<uint8_t>(word & 0xFF));
    out.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
}

std::string char_from_evo_name_word(uint16_t word)
{
    if (word >= 0xE800 && word <= 0xE819) return std::string(1, static_cast<char>('A' + (word - 0xE800)));
    if (word == 0xE81A) return "θ";
    if (word >= 0xE401 && word <= 0xE40A) return std::string(1, static_cast<char>('0' + (word - 0xE401)));
    if ((word >= 'a' && word <= 'z') || (word >= 'A' && word <= 'Z') || (word >= '0' && word <= '9')) return std::string(1, static_cast<char>(word));
    if (word == 0x005F || word == 0xE400) return "_";
    return "_";
}

std::vector<uint16_t> evo_name_words(const data_t& nameBytes)
{
    std::vector<uint16_t> words;
    for (size_t i = 0; i + 1 < nameBytes.size(); i += 2)
    {
        const uint16_t word = static_cast<uint16_t>(nameBytes[i] | (nameBytes[i + 1] << 8));
        if (word == 0)
        {
            break;
        }
        words.push_back(word);
    }
    return words;
}

std::string decode_evo_name(uint8_t evoTypeID, const data_t& nameBytes)
{
    const std::vector<uint16_t> words = evo_name_words(nameBytes);
    if (words.empty())
    {
        return "";
    }

    const uint16_t first = words[0];
    if (evoTypeID == 1)
    {
        if (first >= 0xE830 && first <= 0xE835) return "L" + std::to_string(first - 0xE830 + 1);
        std::string out;
        const size_t start = first == 0xE836 ? 1 : 0;
        for (size_t i = start; i < words.size(); i++) out += char_from_evo_name_word(words[i]);
        return out;
    }
    if (evoTypeID == 3)
    {
        if (first == 0xE899) return "GDB0";
        if (first >= 0xE890 && first <= 0xE898) return "GDB" + std::to_string(first - 0xE890 + 1);
    }
    if (evoTypeID == 4)
    {
        if (first == 0xE889) return "Pic0";
        if (first >= 0xE880 && first <= 0xE888) return "Pic" + std::to_string(first - 0xE880 + 1);
    }
    if (evoTypeID == 5)
    {
        if (first == 0xE8B9) return "Image0";
        if (first >= 0xE8B0 && first <= 0xE8B8) return "Image" + std::to_string(first - 0xE8B0 + 1);
    }
    if (evoTypeID == 6 && first >= 0xE820 && first <= 0xE829)
    {
        return std::string(1, static_cast<char>('A' + (first - 0xE820)));
    }
    if (evoTypeID == 7)
    {
        if (first >= 0xE840 && first <= 0xE849) return "Y" + std::to_string(first == 0xE849 ? 0 : first - 0xE840 + 1);
        if (first >= 0xE850 && first <= 0xE85B)
        {
            const uint16_t idx = static_cast<uint16_t>((first - 0xE850) / 2 + 1);
            return std::string(1, ((first - 0xE850) % 2 == 0) ? 'X' : 'Y') + std::to_string(idx) + "T";
        }
        if (first >= 0xE860 && first <= 0xE865) return "r" + std::to_string(first - 0xE860 + 1);
        if (first >= 0xE870 && first <= 0xE872) return std::string(1, static_cast<char>('u' + (first - 0xE870)));
    }
    if (evoTypeID == 10)
    {
        if (first == 0xE8A9) return "Str0";
        if (first >= 0xE8A0 && first <= 0xE8A8) return "Str" + std::to_string(first - 0xE8A0 + 1);
    }
    if (evoTypeID == 12 && first == 0xE8BA) return "Window";
    if (evoTypeID == 13 && first == 0xE8BB) return "RclWindw";
    if (evoTypeID == 14 && first == 0xE8BC) return "TblSet";

    std::string out;
    for (const uint16_t word : words)
    {
        out += char_from_evo_name_word(word);
    }
    return out;
}

data_t encode_evo_custom_name(std::string name, bool allowLowerAscii)
{
    data_t out;
    name = normalize_theta_chars(name);
    if (!allowLowerAscii)
    {
        name = strtoupper(name);
    }
    for (const unsigned char c : name)
    {
        if (c >= 'A' && c <= 'Z') append_evo_name_word(out, static_cast<uint16_t>(0xE800 + (c - 'A')));
        else if (c >= '0' && c <= '9') append_evo_name_word(out, static_cast<uint16_t>(0xE401 + (c - '0')));
        else if (allowLowerAscii && c >= 'a' && c <= 'z') append_evo_name_word(out, c);
        else if (c == '[') append_evo_name_word(out, 0xE81A);
        else if (c == '_') append_evo_name_word(out, 0xE400);
        else append_evo_name_word(out, 0xE400);
    }
    append_evo_name_word(out, 0);
    return out;
}

data_t encode_evo_name(uint8_t evoTypeID, std::string displayName)
{
    data_t out;
    const std::string upperName = strtoupper(normalize_theta_chars(displayName));
    const auto append_terminated = [&]()
    {
        append_evo_name_word(out, 0);
        return out;
    };

    if (evoTypeID == 1)
    {
        if (upperName.size() == 2 && upperName[0] == 'L' && upperName[1] >= '1' && upperName[1] <= '6')
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE830 + (upperName[1] - '1')));
            return append_terminated();
        }
        append_evo_name_word(out, 0xE836);
        data_t custom = encode_evo_custom_name(displayName, false);
        custom.pop_back();
        custom.pop_back();
        out.insert(out.end(), custom.begin(), custom.end());
        return append_terminated();
    }
    if (evoTypeID == 3 && upperName.rfind("GDB", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE899 : static_cast<uint16_t>(0xE890 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == 4 && upperName.rfind("PIC", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE889 : static_cast<uint16_t>(0xE880 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == 5 && upperName.rfind("IMAGE", 0) == 0 && upperName.size() == 6 && std::isdigit(static_cast<unsigned char>(upperName[5])))
    {
        const int idx = upperName[5] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE8B9 : static_cast<uint16_t>(0xE8B0 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == 6)
    {
        std::string mat = upperName;
        if (mat.size() == 3 && mat.front() == '[' && mat.back() == ']') mat = mat.substr(1, 1);
        if (mat.size() == 1 && mat[0] >= 'A' && mat[0] <= 'J')
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE820 + (mat[0] - 'A')));
            return append_terminated();
        }
    }
    if (evoTypeID == 7)
    {
        if (upperName.size() == 2 && upperName[0] == 'Y' && std::isdigit(static_cast<unsigned char>(upperName[1])))
        {
            const int idx = upperName[1] == '0' ? 9 : upperName[1] - '1';
            append_evo_name_word(out, static_cast<uint16_t>(0xE840 + idx));
            return append_terminated();
        }
        if (upperName.size() == 3 && (upperName[0] == 'X' || upperName[0] == 'Y') && upperName[1] >= '1' && upperName[1] <= '6' && upperName[2] == 'T')
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE850 + (upperName[1] - '1') * 2 + (upperName[0] == 'Y' ? 1 : 0)));
            return append_terminated();
        }
        if (upperName.size() == 2 && upperName[0] == 'R' && upperName[1] >= '1' && upperName[1] <= '6')
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE860 + (upperName[1] - '1')));
            return append_terminated();
        }
        if (upperName == "U" || upperName == "V" || upperName == "W")
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE870 + (upperName[0] - 'U')));
            return append_terminated();
        }
    }
    if (evoTypeID == 10 && upperName.rfind("STR", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE8A9 : static_cast<uint16_t>(0xE8A0 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == 12)
    {
        append_evo_name_word(out, 0xE8BA);
        return append_terminated();
    }
    if (evoTypeID == 13)
    {
        append_evo_name_word(out, 0xE8BB);
        return append_terminated();
    }
    if (evoTypeID == 14)
    {
        append_evo_name_word(out, 0xE8BC);
        return append_terminated();
    }

    return encode_evo_custom_name(displayName, evoTypeID == 8 || evoTypeID == 15);
}

TIVarType type_from_evo_type(uint8_t evoTypeID)
{
    switch (evoTypeID)
    {
        case 0: return TIVarType{"Real"};
        case 1: return TIVarType{"RealList"};
        case 2: return TIVarType{"Program"};
        case 3: return TIVarType{"GraphDataBase"};
        case 4: return TIVarType{"Picture"};
        case 5: return TIVarType{"Image"};
        case 6: return TIVarType{"Matrix"};
        case 7: return TIVarType{"Equation"};
        case 8: return TIVarType{"AppVar"};
        case 9: return TIVarType{"GroupObject"};
        case 10: return TIVarType{"String"};
        case 11: return TIVarType{"FlashApp"};
        case 12: return TIVarType{"WindowSettings"};
        case 13: return TIVarType{"RecallWindow"};
        case 14: return TIVarType{"TableRange"};
        default: return TIVarType{"AppVar"};
    }
}

uint8_t evo_type_from_type(const TIVarType& type, uint8_t fallback)
{
    if (fallback != 0 || type.getName() == "Real")
    {
        return fallback;
    }
    const std::string typeName = type.getName();
    if (typeName == "Complex" || typeName.rfind("Exact", 0) == 0) return 0;
    if (typeName == "RealList" || typeName == "ComplexList") return 1;
    if (typeName == "Program" || typeName == "ProtectedProgram") return 2;
    if (typeName == "GraphDataBase") return 3;
    if (typeName == "Picture") return 4;
    if (typeName == "Image") return 5;
    if (typeName == "Matrix") return 6;
    if (typeName == "Equation" || typeName == "SmartEquation") return 7;
    if (typeName.find("AppVar") != std::string::npos) return 8;
    if (typeName == "GroupObject") return 9;
    if (typeName == "String") return 10;
    if (typeName == "FlashApp") return 11;
    if (typeName == "WindowSettings") return 12;
    if (typeName == "RecallWindow") return 13;
    if (typeName == "TableRange") return 14;
    return 8;
}

std::string bytes_to_hex_string(const data_t& data)
{
    std::string result;
    result.reserve(data.size() * 2);
    for (const uint8_t byte : data)
    {
        result += dechex(byte);
    }
    return result;
}

std::string type_name_from_evo_type(uint8_t evoTypeID)
{
    if (evoTypeID == 15)
    {
        return "EvoPythonScript";
    }
    return type_from_evo_type(evoTypeID).getName();
}

std::string extension_from_evo_type(uint8_t evoTypeID)
{
    switch (evoTypeID)
    {
        case 0: return "8xn2";
        case 1: return "8xl2";
        case 2: return "8xp2";
        case 3: return "8xd2";
        case 4: return "8ci2";
        case 5: return "8ca2";
        case 6: return "8xm2";
        case 7: return "8xy2";
        case 8: return "8xv2";
        case 9: return "8xg2";
        case 10: return "8xs2";
        case 11: return "8ek2";
        case 12: return "8xw2";
        case 13: return "8xz2";
        case 14: return "8xt2";
        case 15: return "8xpy2";
        default: return "";
    }
}

namespace
{
    uint16_t read_le16(const data_t& data, size_t offset)
    {
        if (offset + 1 >= data.size())
        {
            throw std::invalid_argument("Unexpected end of Evo Python script payload");
        }
        return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
    }

    uint32_t read_le32(const data_t& data, size_t offset)
    {
        if (offset + 3 >= data.size())
        {
            throw std::invalid_argument("Unexpected end of Evo Python script payload");
        }
        return static_cast<uint32_t>(data[offset])
             | (static_cast<uint32_t>(data[offset + 1]) << 8)
             | (static_cast<uint32_t>(data[offset + 2]) << 16)
             | (static_cast<uint32_t>(data[offset + 3]) << 24);
    }

    std::string printable_ascii_string(const data_t& data, size_t offset, size_t len)
    {
        if (offset + len > data.size())
        {
            throw std::invalid_argument("Unexpected end of Evo Python script payload");
        }
        std::string out;
        out.reserve(len);
        for (size_t i = 0; i < len; i++)
        {
            const uint8_t c = data[offset + i];
            if ((c >= 0x20 && c < 0x7F) || c == '\n' || c == '\r' || c == '\t')
            {
                out.push_back(static_cast<char>(c));
            }
            else
            {
                return "";
            }
        }
        return out;
    }
}

EvoPythonScriptInfo parse_evo_python_script_payload(const data_t& data)
{
    if (data.size() < 21)
    {
        throw std::invalid_argument("Invalid Evo Python script payload: too short");
    }

    EvoPythonScriptInfo info;
    info.magic.assign(data.begin(), data.begin() + 4);
    info.bodyLen = read_le32(data, 4);
    info.nameLen = read_le32(data, 8);
    if (info.nameLen == 0 || info.nameLen > 255 || 12 + info.nameLen + 1 + 4 > data.size())
    {
        throw std::invalid_argument("Invalid Evo Python script payload: bad name length");
    }

    const size_t nameOffset = 12;
    const size_t afterName = nameOffset + info.nameLen;
    if (data[afterName] != 0)
    {
        throw std::invalid_argument("Invalid Evo Python script payload: missing name terminator");
    }
    info.name.assign(reinterpret_cast<const char*>(data.data() + nameOffset), info.nameLen);

    const size_t bodyLenOffset = afterName + 1;
    info.scriptLen = read_le16(data, bodyLenOffset);
    info.scriptType = data[bodyLenOffset + 3];
    const size_t bodyOffset = bodyLenOffset + 4;
    if (bodyOffset + info.scriptLen > data.size())
    {
        throw std::invalid_argument("Invalid Evo Python script payload: script body exceeds payload");
    }

    info.body.assign(data.begin() + static_cast<ptrdiff_t>(bodyOffset), data.begin() + static_cast<ptrdiff_t>(bodyOffset + info.scriptLen));
    info.trailer.assign(data.begin() + static_cast<ptrdiff_t>(bodyOffset + info.scriptLen), data.end());
    info.code = printable_ascii_string(data, bodyOffset, info.scriptLen);
    info.bodyIsText = !info.code.empty();
    return info;
}

struct EvoTokenInfo
{
    uint16_t value;
    const char* name;
};

#include "EvoTokens.inc"

const char* evo_token_name(uint16_t token)
{
    static const std::unordered_map<uint16_t, const char*> names = [] {
        std::unordered_map<uint16_t, const char*> map;
        for (const auto& [value, name] : evoTokenInfos)
        {
            map.emplace(value, name);
        }
        return map;
    }();

    const auto it = names.find(token);
    return it == names.end() ? nullptr : it->second;
}

std::string evo_token_source_from_name(const std::string& name)
{
    static const std::unordered_map<std::string, std::string> aliases = {
        {"TOK_EOS", ""},
        {"TOK_UNDERSCORE", "_"}, {"TOK_DECPT", "."}, {"TOK_EE", "ᴇ"}, {"TOK_PI", "π"},
        {"TOK_IMAGINARY_I", "𝑖"}, {"TOK_CONST_E", "𝑒"}, {"TOK_LPAREN", "("}, {"TOK_RPAREN", ")"},
        {"TOK_LBRACK", "["}, {"TOK_RBRACK", "]"}, {"TOK_LBRACE", "{"}, {"TOK_RBRACE", "}"},
        {"TOK_STRING", "\""}, {"TOK_COMMA", ","}, {"TOK_COLON", ":"}, {"TOK_SPACE", " "},
        {"TOK_APOST", "'"}, {"TOK_QUEST", "?"}, {"TOK_NEW_LINE", "\n"}, {"TOK_STORE", "→"},
        {"TOK_RECIP", "⁻¹"}, {"TOK_SQUARE", "²"}, {"TOK_CUBE", "³"}, {"TOK_FACTORIAL", "!"},
        {"TOK_FROM_RAD", "ʳ"}, {"TOK_FROM_DEG", "°"}, {"TOK_FROM_GRAD", "ᵍ"},
        {"TOK_TO_DMS", "►DMS"}, {"TOK_TO_DEC", "►Dec"}, {"TOK_TO_ABC", "►Frac"},
        {"TOK_ADD", "+"}, {"TOK_SUB", "-"}, {"TOK_MUL", "*"}, {"TOK_DIV", "/"}, {"TOK_POWER", "^"},
        {"TOK_X_ROOT", "ˣ√"}, {"TOK_CHS", "⁻"}, {"TOK_INT", "int("}, {"TOK_ABS", "abs("},
        {"TOK_ROUND", "round("}, {"TOK_IPART", "iPart("}, {"TOK_FPART", "fPart("},
        {"TOK_RAND", "rand"}, {"TOK_RAND_INT_NO_REP", "randIntNoRep("}, {"TOK_RANDINT", "randInt("},
        {"TOK_RANDBIN", "randBin("}, {"TOK_RANDNORM", "randNorm("}, {"TOK_SQRT", "√("},
        {"TOK_CUBRT", "³√("}, {"TOK_LN", "ln("}, {"TOK_EXP", "𝑒^("}, {"TOK_LOG", "log("},
        {"TOK_ALOG", "₁₀^("}, {"TOK_SIN", "sin("}, {"TOK_ASIN", "sin⁻¹("}, {"TOK_COS", "cos("},
        {"TOK_ACOS", "cos⁻¹("}, {"TOK_TAN", "tan("}, {"TOK_ATAN", "tan⁻¹("},
        {"TOK_SINH", "sinh("}, {"TOK_ASINH", "sinh⁻¹("}, {"TOK_COSH", "cosh("},
        {"TOK_ACOSH", "cosh⁻¹("}, {"TOK_TANH", "tanh("}, {"TOK_ATANH", "tanh⁻¹("},
        {"TOK_LOGN", "logBASE("}, {"TOK_SERIES", "seq("}, {"TOK_FNINT", "fnInt("}, {"TOK_NDERIV", "nDeriv("},
        {"TOK_FMAX", "fMax("}, {"TOK_FMIN", "fMin("}, {"TOK_MAX", "max("}, {"TOK_MIN", "min("},
        {"TOK_MEAN", "mean("}, {"TOK_MEDIAN", "median("}, {"TOK_SUM", "sum("}, {"TOK_PROD", "prod("},
        {"TOK_ROW_SWAP", "rowSwap("}, {"TOK_ROW_PLUS", "row+("}, {"TOK_MUL_ROW", "*row("},
        {"TOK_MUL_ROW_PLUS", "*row+("}, {"TOK_DET", "det("}, {"TOK_TRANSPOSE", "ᵀ"},
        {"TOK_REF", "ref("}, {"TOK_RREF", "rref("}, {"TOK_DIM", "dim("}, {"TOK_FILL", "Fill("},
        {"TOK_IDENT", "identity("}, {"TOK_RAND_MAT", "randM("}, {"TOK_AUGMENT", "augment("},
        {"TOK_CONJ", "conj("}, {"TOK_REAL", "real("}, {"TOK_IMAG", "imag("}, {"TOK_ANGLE", "angle("},
        {"TOK_CUMSUM", "cumSum("}, {"TOK_EXPR", "expr("}, {"TOK_LENGTH", "length("},
        {"TOK_DELTALST", "ΔList("}, {"TOK_LCM", "lcm("}, {"TOK_GCD", "gcd("},
        {"TOK_SUBSTRING", "sub("}, {"TOK_INSTRING", "inString("}, {"TOK_REMAINDER", "remainder("},
        {"TOK_OR", " or "}, {"TOK_XOR", " xor "}, {"TOK_AND", " and "},
        {"TOK_NPR", " nPr "}, {"TOK_NCR", " nCr "}, {"TOK_EQ", "="}, {"TOK_LT", "<"},
        {"TOK_GT", ">"}, {"TOK_LE", "≤"}, {"TOK_GE", "≥"}, {"TOK_NE", "≠"}, {"TOK_NOT", "not("},
        {"TOK_INV_BINOM", "invBinom("}, {"TOK_BINPDF", "binompdf("}, {"TOK_BINCDF", "binomcdf("},
        {"TOK_POIPDF", "poissonpdf("}, {"TOK_POICDF", "poissoncdf("}, {"TOK_GEOPDF", "geometpdf("},
        {"TOK_GEOCDF", "geometcdf("}, {"TOK_NORMALPDF", "normalpdf("}, {"TOK_INVNORM", "invNorm("},
        {"TOK_DNORMAL", "normalcdf("}, {"TOK_TPDF", "tpdf("}, {"TOK_CHIPDF", "χ²pdf("},
        {"TOK_FPDF", "𝙵pdf("}, {"TOK_IF", "If "}, {"TOK_THEN", "Then"}, {"TOK_ELSE", "Else"},
        {"TOK_WHILE", "While "}, {"TOK_REPEAT", "Repeat "}, {"TOK_FOR", "For("}, {"TOK_END", "End"},
        {"TOK_RETURN", "Return"}, {"TOK_LBL", "Lbl "}, {"TOK_GOTO", "Goto "}, {"TOK_PAUSE", "Pause "},
        {"TOK_WAIT", "Wait "}, {"TOK_STOP", "Stop"}, {"TOK_ISG", "IS>("}, {"TOK_DSL", "DS<("},
        {"TOK_MENU", "Menu("}, {"TOK_INPUT", "Input "}, {"TOK_GETKEY", "getKey"}, {"TOK_PROMPT", "Prompt "},
        {"TOK_DISP", "Disp "}, {"TOK_DISPG", "DispGraph"}, {"TOK_DISPTAB", "DispTable"},
        {"TOK_OUTPUT", "Output("}, {"TOK_CLLCD", "ClrHome"}, {"TOK_PRTSCRN", "PrintScreen"},
        {"TOK_SORTA", "SortA("}, {"TOK_SORTD", "SortD("}, {"TOK_GET_CALC", "GetCalc("},
        {"TOK_MAT_TO_LIST", "Matr►list("}, {"TOK_LIST_TO_MAT", "List►matr("},
        {"TOK_SETUP_LIST", "SetUpEditor "}, {"TOK_CLR_ALL_LIST", "ClrAllLists"},
        {"TOK_DEL_VAR", "DelVar "}, {"TOK_EQU_TO_STRING", "Equ►String("},
        {"TOK_STRING_TO_EQU", "String►Equ("}, {"TOK_TO_STRING", "toString("},
        {"TOK_ARCHIVE", "Archive "}, {"TOK_UNARCHIVE", "UnArchive "}, {"TOK_GARBAGEC", "GarbageCollect"},
        {"TOK_CIRCLE", "Circle("}, {"TOK_CLEAR_DRAW", "ClrDraw"}, {"TOK_LINE", "Line("},
        {"TOK_PT_CHANGE", "Pt-Change("}, {"TOK_PT_OFF", "Pt-Off("}, {"TOK_PT_ON", "Pt-On("},
        {"TOK_PXL_CHANGE", "Pxl-Change("}, {"TOK_PXL_OFF", "Pxl-Off("}, {"TOK_PXL_ON", "Pxl-On("},
        {"TOK_PXL_TEST", "pxl-Test("}, {"TOK_VERT", "Vertical "}, {"TOK_HORZ", "Horizontal "},
        {"TOK_TEXT", "Text("}, {"TOK_TANLN", "Tangent("}, {"TOK_EXPR_ON", "ExprOn"}, {"TOK_EXPR_OFF", "ExprOff"},
        {"TOK_ONE_VAR", "1-Var Stats "}, {"TOK_TWO_VAR", "2-Var Stats "}, {"TOK_CLRLST", "ClrList "},
        {"TOK_CLRTBL", "ClrTable"}, {"TOK_BOX_PLOT", "Boxplot"}, {"TOK_HIST", "Histogram"},
        {"TOK_XYLINE", "xyLine"}, {"TOK_SCATTER", "Scatter"}, {"TOK_MAN_FIT", "Manual-Fit "},
        {"TOK_MATH_PRINT", "MATHPRINT"}, {"TOK_CLASSIC", "CLASSIC"}, {"TOK_SCI", "Sci"},
        {"TOK_SIMP_FRAC", "n⁄d"}, {"TOK_MIX_FRAC", "Un⁄d"},
        {"TOK_ANS_AUTO", "AUTO"}, {"TOK_ANS_DEC", "DEC"},
        {"TOK_ENG", "Eng"}, {"TOK_FLOAT", "Float"}, {"TOK_FIX", "Fix "}, {"TOK_RAD", "Radian"},
        {"TOK_DEG", "Degree"}, {"TOK_GRAD", "Grad"}, {"TOK_FUNC", "Func"}, {"TOK_PARAM", "Param"},
        {"TOK_POLAR", "Polar"}, {"TOK_SEQ", "Seq"}, {"TOK_FULL", "Full"}, {"TOK_HORIZ", "Horiz"},
        {"TOK_BLUE", "BLUE"}, {"TOK_RED", "RED"}, {"TOK_BLACK", "BLACK"}, {"TOK_MAGENTA", "MAGENTA"},
        {"TOK_GREEN", "GREEN"}, {"TOK_ORANGE", "ORANGE"}, {"TOK_BROWN", "BROWN"}, {"TOK_NAVY", "NAVY"},
        {"TOK_LTBLUE", "LTBLUE"}, {"TOK_YELLOW", "YELLOW"}, {"TOK_WHITE", "WHITE"},
        {"TOK_LTGRAY", "LTGRAY"}, {"TOK_MEDGRAY", "MEDGRAY"}, {"TOK_GRAY", "GRAY"},
        {"TOK_DARKGRAY", "DARKGRAY"}, {"TOK_GRAPH_COLOR", "GraphColor("}, {"TOK_TEXT_COLOR", "TextColor("},
        {"TOK_BORDER_COLOR", "BorderColor "}, {"TOK_DETECT_ASYM_ON", "DetectAsymOn"},
        {"TOK_DETECT_ASYM_OFF", "DetectAsymOff"}, {"TOK_FRAC_SLASH", "⁄"}, {"TOK_FRAC_UNIT", "󸏵"},
        {"TOK_SIMP_MIX", "►n⁄d◄►Un⁄d"}, {"TOK_FRAC_DEC", "►F◄►D"}, {"TOK_VAR_ANS", "Ans"},
        {"TOK_VAR_LIST_NAME", "ʟ"},
        {"TOK_PRGM", "prgm"}, {"TOK_VAR_REGEQ", "RegEQ"}, {"TOK_VAR_STAT_N", "n"},
        {"TOK_VAR_X_MEAN", "x̄"}, {"TOK_VAR_SUM_X", "Σx"}, {"TOK_VAR_SUM_X_SQUARE", "Σx²"},
        {"TOK_VAR_STD_X", "Sx"}, {"TOK_VAR_STD_POP_X", "σx"}, {"TOK_VAR_MIN_X", "minX"},
        {"TOK_VAR_MAX_X", "maxX"}, {"TOK_VAR_MIN_Y", "minY"}, {"TOK_VAR_MAX_Y", "maxY"},
        {"TOK_VAR_Y_MEAN", "ȳ"}, {"TOK_VAR_SUM_Y", "Σy"}, {"TOK_VAR_SUM_Y_SQUARE", "Σy²"},
        {"TOK_VAR_STD_Y", "Sy"}, {"TOK_VAR_STD_POP_Y", "σy"}, {"TOK_VAR_SUM_XY", "Σxy"},
        {"TOK_VAR_CORR", "r"}, {"TOK_VAR_MED", "Med"}, {"TOK_VAR_Q1", "Q₁"}, {"TOK_VAR_Q3", "Q₃"},
        {"TOK_VAR_QUAD_A", "a"}, {"TOK_VAR_QUAD_B", "b"}, {"TOK_VAR_QUAD_C", "c"},
        {"TOK_VAR_CUBE_D", "d"}, {"TOK_VAR_QUART_E", "e"},
        {"TOK_RECT_TO_POL_R", "R►Pr("}, {"TOK_RECT_TO_POL_THETA", "R►Pθ("},
        {"TOK_POL_TO_RECT_X", "P►Rx("}, {"TOK_POL_TO_RECT_Y", "P►Ry("},
        {"TOK_TORECT", "►Rect"}, {"TOK_TOPOLAR", "►Polar"},
        {"TOK_FIN_FPMT", "tvm_Pmt"}, {"TOK_FIN_FI", "tvm_I%"}, {"TOK_FIN_FPV", "tvm_PV"},
        {"TOK_FIN_FN", "tvm_𝗡"}, {"TOK_FIN_FFV", "tvm_FV"}, {"TOK_FIN_NPV", "npv("},
        {"TOK_FIN_IRR", "irr("}, {"TOK_FIN_BAL", "bal("}, {"TOK_FIN_PRN", "ΣPrn("},
        {"TOK_FIN_INT", "ΣInt("}, {"TOK_FIN_TONOM", "►Nom("}, {"TOK_FIN_TOEFF", "►Eff("},
        {"TOK_FIN_DBD", "dbd("}, {"TOK_FIN_PMT_END", "Pmt_End"}, {"TOK_FIN_PMT_BEG", "Pmt_Bgn"},
        {"TOK_STDDEV", "stdDev("}, {"TOK_VARIANCE", "variance("}, {"TOK_DT", "tcdf("},
        {"TOK_CHI", "χ²cdf("}, {"TOK_DF", "𝙵cdf("}, {"TOK_Z_TEST", "Z-Test("},
        {"TOK_T_TEST", "T-Test "}, {"TOK_2SAMP_Z_TEST", "2-SampZTest("},
        {"TOK_2SAMP_T_TEST", "2-SampTTest "}, {"TOK_1PROP_Z_TEST", "1-PropZTest("},
        {"TOK_2PROP_Z_TEST", "2-PropZTest("}, {"TOK_CHI_TEST", "χ²-Test("},
        {"TOK_2SAMP_F_TEST", "2-Samp𝙵Test "}, {"TOK_ZINT_VAL", "ZInterval "},
        {"TOK_TINT_VAL", "TInterval "}, {"TOK_2SAMP_TINT", "2-SampTInt "},
        {"TOK_2SAMP_ZINT", "2-SampZInt("}, {"TOK_1PROP_ZINT", "1-PropZInt("},
        {"TOK_2PROP_ZINT", "2-PropZInt("}, {"TOK_SELECT", "Select("}, {"TOK_ANOVA", "ANOVA("},
        {"TOK_MODBOX", "ModBoxplot"}, {"TOK_NORM_PROB", "NormProbPlot"},
        {"TOK_PLOTON", "PlotsOn "}, {"TOK_PLOTOFF", "PlotsOff "},
        {"TOK_PLOT1", "Plot1("}, {"TOK_PLOT2", "Plot2("}, {"TOK_PLOT3", "Plot3("},
        {"TOK_DEL_LAST", "Clear Entries"},
        {"TOK_NORMAL_FORMAT", "Normal"}, {"TOK_MGT", "G-T"}, {"TOK_REALM", "Real"}, {"TOK_POLARM", "a+b𝑖"},
        {"TOK_RECTM", "r𝑒^θ𝑖"}, {"TOK_DIAG_ON", "DiagnosticOn"}, {"TOK_DIAG_OFF", "DiagnosticOff"},
        {"TOK_SEQG", "Sequential"}, {"TOK_SIMULG", "Simul"}, {"TOK_POLARG", "PolarGC"},
        {"TOK_RECTG", "RectGC"}, {"TOK_COORDON", "CoordOn"}, {"TOK_COORDOFF", "CoordOff"},
        {"TOK_AUTO_FILL_ON", "IndpntAuto"}, {"TOK_AUTO_FILL_OFF", "IndpntAsk"},
        {"TOK_AUTO_CALC_ON", "DependAuto"}, {"TOK_AUTO_CALC_OFF", "DependAsk"},
        {"TOK_GRID_LINE", "GridLine "}, {"TOK_AXESON", "AxesOn "}, {"TOK_AXESOFF", "AxesOff"},
        {"TOK_BOX_ICON", "squareplot"}, {"TOK_CROSS_ICON", "crossplot"}, {"TOK_DOT_ICON", "dotplot"},
        {"TOK_SMALL_DOT_ICON", "plottinydot"}, {"TOK_THIN_LINE", "Thin"}, {"TOK_THIN_DOT", "Dot-Thin"},
        {"TOK_GRIDDOT", "GridDot "}, {"TOK_GRIDOFF", "GridOff"}, {"TOK_LBLON", "LabelOn"},
        {"TOK_LBLOFF", "LabelOff"}, {"TOK_WEBON", "Web"}, {"TOK_UV", "uvAxes"},
        {"TOK_VW", "vwAxes"}, {"TOK_UW", "uwAxes"}, {"TOK_DRAWLINE", "Thick"},
        {"TOK_DRAWDOT", "Dot-Thick"}, {"TOK_WEBOFF", "Time"}, {"TOK_UN_MINUS1", "U𝑛-₁"},
        {"TOK_VN_MINUS1", "V𝑛-₁"}, {"TOK_INV_T", "invT("}, {"TOK_CHI_GOF_TEST", "χ²GOF-Test("},
        {"TOK_LR", "LinReg(ax+b) "}, {"TOK_LR1", "LinReg(a+bx) "}, {"TOK_LREXP", "ExpReg "},
        {"TOK_LRLN", "LnReg "}, {"TOK_LRPWR", "PwrReg "}, {"TOK_MEDMED", "Med-Med "},
        {"TOK_QUAD", "QuadReg "}, {"TOK_CUBIC_REG", "CubicReg "}, {"TOK_QUART_REG", "QuartReg "},
        {"TOK_SINREG", "SinReg "}, {"TOK_LOGISTIC", "Logistic "},
        {"TOK_LINREGTTEST", "LinRegTTest "}, {"TOK_LIN_REG_TINT", "LinRegTInt "},
        {"TOK_TRACE", "Trace"}, {"TOK_ZOOM_STD", "ZStandard"}, {"TOK_ZOOM_TRIG", "ZTrig"},
        {"TOK_ZOOM_BOX", "ZBox"}, {"TOK_ZOOM_IN", "Zoom In"}, {"TOK_ZOOM_OUT", "Zoom Out"},
        {"TOK_ZOOM_SQUARE", "ZSquare"}, {"TOK_ZOOM_INT", "ZInteger"}, {"TOK_ZOOM_PREV", "ZPrevious"},
        {"TOK_ZOOM_DEC", "ZDecimal"}, {"TOK_ZOOM_STAT", "ZoomStat"}, {"TOK_ZOOM_RCL", "ZoomRcl"},
        {"TOK_ZOOM_STO", "ZoomSto"}, {"TOK_ZOOM_FIT", "ZoomFit"}, {"TOK_FNON", "FnOn "},
        {"TOK_FNOFF", "FnOff "}, {"TOK_STOPIC", "StorePic "}, {"TOK_RCLPIC", "RecallPic "},
        {"TOK_STOGDB", "StoreGDB "}, {"TOK_RCLGDB", "RecallGDB "}, {"TOK_SHADE", "Shade("},
        {"TOK_DRINV", "DrawInv "}, {"TOK_DRAWF", "DrawF "}, {"TOK_SHADE_NORM", "ShadeNorm("},
        {"TOK_SHADE_T", "Shade_t("}, {"TOK_SHADE_CHI", "Shadeχ²("}, {"TOK_SHADE_F", "Shade𝙵("},
        {"TOK_GRAPH_STYLE", "GraphStyle("}, {"TOK_SOLVE_ROOT", "solve("},
        {"TOK_RECV_MBL", "Get("}, {"TOK_SEND_MBL", "Send("},
        {"TOK_TIME_CNV", "timeCnv("}, {"TOK_DAY_OF_WEEK", "dayOfWk("},
        {"TOK_VAR_RECURE_N", "𝑛"}, {"TOK_VAR_PIC1", "Pic1"}, {"TOK_VAR_PIC2", "Pic2"}, {"TOK_VAR_PIC3", "Pic3"},
        {"TOK_VAR_PIC4", "Pic4"}, {"TOK_VAR_PIC5", "Pic5"}, {"TOK_VAR_PIC6", "Pic6"},
        {"TOK_VAR_PIC7", "Pic7"}, {"TOK_VAR_PIC8", "Pic8"}, {"TOK_VAR_PIC9", "Pic9"},
        {"TOK_VAR_PIC0", "Pic0"}, {"TOK_VAR_GDB1", "GDB1"}, {"TOK_VAR_GDB2", "GDB2"},
        {"TOK_VAR_GDB3", "GDB3"}, {"TOK_VAR_GDB4", "GDB4"}, {"TOK_VAR_GDB5", "GDB5"},
        {"TOK_VAR_GDB6", "GDB6"}, {"TOK_VAR_GDB7", "GDB7"}, {"TOK_VAR_GDB8", "GDB8"},
        {"TOK_VAR_GDB9", "GDB9"}, {"TOK_VAR_GDB0", "GDB0"}, {"TOK_VAR_MED_X1", "x₁"},
        {"TOK_VAR_MED_X2", "x₂"}, {"TOK_VAR_MED_X3", "x₃"}, {"TOK_VAR_MED_Y1", "y₁"},
        {"TOK_VAR_MED_Y2", "y₂"}, {"TOK_VAR_MED_Y3", "y₃"}, {"TOK_VAR_STAT_P", "p"},
        {"TOK_VAR_STAT_Z", "z"}, {"TOK_VAR_STAT_T", "t"}, {"TOK_VAR_STAT_CHI", "χ²"},
        {"TOK_VAR_STAT_F", "𝙵"}, {"TOK_VAR_STAT_DF", "df"}, {"TOK_VAR_STAT_P_HAT", "p̂"},
        {"TOK_VAR_STAT_P_HAT1", "p̂₁"}, {"TOK_VAR_STAT_P_HAT2", "p̂₂"},
        {"TOK_VAR_STAT_MEAN_X1", "x̄₁"}, {"TOK_VAR_STAT_MEAN_X2", "x̄₂"},
        {"TOK_VAR_STAT_STD_X1", "Sx₁"}, {"TOK_VAR_STAT_STD_X2", "Sx₂"},
        {"TOK_VAR_STAT_N1", "n₁"}, {"TOK_VAR_STAT_N2", "n₂"}, {"TOK_VAR_STAT_STD_XP", "Sxp"},
        {"TOK_VAR_STAT_LOWER", "lower"}, {"TOK_VAR_STAT_UPPER", "upper"}, {"TOK_VAR_STAT_S", "s"},
        {"TOK_VAR_LR_SQR", "r²"}, {"TOK_VAR_BR_SQR", "R²"}, {"TOK_VAR_ANOVA_DFF", "df"},
        {"TOK_VAR_ANOVA_SSF", "SS"}, {"TOK_VAR_ANOVA_MSF", "MS"}, {"TOK_VAR_ANOVA_DFE", "df"},
        {"TOK_VAR_ANOVA_SSE", "SS"}, {"TOK_VAR_ANOVA_MSE", "MS"}, {"TOK_VAR_RECUR_U0", "u(𝑛Min)"},
        {"TOK_VAR_RECUR_V0", "v(𝑛Min)"}, {"TOK_VAR_RECUR_W0", "w(𝑛Min)"}, {"TOK_VAR_UN1", "U𝑛-₁"},
        {"TOK_VAR_VN1", "V𝑛-₁"}, {"TOK_VAR_URECUR_U0", "Zu(𝑛Min)"}, {"TOK_VAR_URECUR_V0", "Zv(𝑛Min)"},
        {"TOK_VAR_URECUR_W0", "Zw(𝑛Min)"}, {"TOK_VAR_XMIN", "Xmin"}, {"TOK_VAR_XMAX", "Xmax"},
        {"TOK_VAR_XSCL", "Xscl"}, {"TOK_VAR_X_RES", "Xres"}, {"TOK_VAR_YMIN", "Ymin"},
        {"TOK_VAR_YMAX", "Ymax"}, {"TOK_VAR_YSCL", "Yscl"}, {"TOK_VAR_TMIN", "Tmin"},
        {"TOK_VAR_TMAX", "Tmax"}, {"TOK_VAR_T_STEP", "Tstep"}, {"TOK_VAR_THETA_MIN", "θmin"},
        {"TOK_VAR_THETA_MAX", "θmax"}, {"TOK_VAR_THETA_STEP", "θstep"},
        {"TOK_VAR_UTHETA_MIN", "Zθmin"}, {"TOK_VAR_UTHETA_MAX", "Zθmax"},
        {"TOK_VAR_UTHETA_STEP", "Zθstep"}, {"TOK_VAR_UT_MIN", "ZTmin"},
        {"TOK_VAR_UT_MAX", "ZTmax"}, {"TOK_VAR_UT_STEP", "ZTstep"},
        {"TOK_VAR_N_MIN", "𝑛Min"}, {"TOK_VAR_UN_MIN", "Z𝑛Min"},
        {"TOK_VAR_N_MAX", "𝑛Max"}, {"TOK_VAR_UN_MAX", "Z𝑛Max"},
        {"TOK_VAR_PLOT_START", "PlotStart"}, {"TOK_VAR_UPLOT_START", "ZPlotStart"},
        {"TOK_VAR_PLOT_STEP", "PlotStep"},
        {"TOK_VAR_UPLOT_STEP", "ZPlotStep"}, {"TOK_VAR_UXRES", "ZXres"},
        {"TOK_VAR_TRACE_STEP", "TraceStep"}, {"TOK_VAR_DELTA_X", "ΔX"}, {"TOK_VAR_DELTA_Y", "ΔY"},
        {"TOK_VAR_X_FACTOR", "XFact"}, {"TOK_VAR_Y_FACTOR", "YFact"}, {"TOK_VAR_TBL_MIN", "TblStart"},
        {"TOK_VAR_TBL_STEP", "ΔTbl"}, {"TOK_VAR_TBL_INPUT", "TblInput"}, {"TOK_VAR_FIN_N", "𝗡"},
        {"TOK_VAR_FIN_I", "I%"}, {"TOK_VAR_FIN_PV", "PV"}, {"TOK_VAR_FIN_PMT", "PMT"},
        {"TOK_VAR_FIN_FV", "FV"}, {"TOK_VAR_FIN_PY", "P/Y"}, {"TOK_VAR_FIN_CY", "C/Y"},
        {"TOK_PREV_ZOOM_XMIN", "ZXmin"}, {"TOK_PREV_ZOOM_XMAX", "ZXmax"},
        {"TOK_PREV_ZOOM_XSCL", "ZXscl"}, {"TOK_PREV_ZOOM_YMIN", "ZYmin"},
        {"TOK_PREV_ZOOM_YMAX", "ZYmax"}, {"TOK_PREV_ZOOM_YSCL", "ZYscl"},
    };

    const auto aliasIt = aliases.find(name);
    if (aliasIt != aliases.end())
    {
        return aliasIt->second;
    }

    if (name.size() == 8 && name.rfind("TOK_LC_", 0) == 0 && name[7] >= 'A' && name[7] <= 'Z')
    {
        return std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(name[7]))));
    }
    if (name.size() == 5 && name.rfind("TOK_", 0) == 0 && name[4] >= '0' && name[4] <= '9')
    {
        return std::string(1, name[4]);
    }

    return name;
}

std::string evo_token_to_string(uint16_t token)
{
    if (token == 0x0000) return "";
    if (token >= 0xE800 && token <= 0xE819) return std::string(1, static_cast<char>('A' + (token - 0xE800)));
    if (token == 0xE81A) return "θ";
    if (token >= 0xE401 && token <= 0xE40A) return std::string(1, static_cast<char>('0' + (token - 0xE401)));
    if (token >= 0xE830 && token <= 0xE835) return "L" + std::to_string(token - 0xE830 + 1);
    if (token >= 0xE820 && token <= 0xE829) return "[" + std::string(1, static_cast<char>('A' + (token - 0xE820))) + "]";
    if (token >= 0x20 && token <= 0x7E && !(token >= 'A' && token <= 'Z')) return std::string(1, static_cast<char>(token));
    if (token >= 0xE840 && token <= 0xE849) return "Y" + std::to_string(token == 0xE849 ? 0 : token - 0xE840 + 1);
    if (token >= 0xE8A0 && token <= 0xE8A9) return "Str" + std::to_string(token == 0xE8A9 ? 0 : token - 0xE8A0 + 1);
    if ((token >= 0x00A1 && token <= 0x00FF) || (token >= 0x0391 && token <= 0x03C9)) return utf8_from_codepoint(token);
    if ((token >= 0x2070 && token <= 0x209F) || token == 0x02E3 || token == 0x029F || token == 0x1D1B) return utf8_from_codepoint(token);
    if (token == 0x2026 || token == 0x2191 || token == 0x2193 || token == 0x221A || token == 0x2220 || token == 0x222B) return utf8_from_codepoint(token);
    if (token == 0x2338 || token == 0x25A1 || token == 0x25BA || token == 0x25C4 || token == 0xFE62) return utf8_from_codepoint(token);
    if (token == 0x007C || token == 0x0060) return utf8_from_codepoint(token);
    if (token >= 0xE850 && token <= 0xE85B)
    {
        const uint16_t idx = static_cast<uint16_t>((token - 0xE850) / 2 + 1);
        return std::string(1, ((token - 0xE850) % 2 == 0) ? 'X' : 'Y') + std::to_string(idx) + "T";
    }
    if (token >= 0xE860 && token <= 0xE865) return "r" + std::to_string(token - 0xE860 + 1);
    if (token >= 0xE870 && token <= 0xE872) return std::string(1, static_cast<char>('u' + (token - 0xE870)));

    const char* name = evo_token_name(token);
    if (name != nullptr)
    {
        return evo_token_source_from_name(name);
    }
    return "\\u" + dechex(static_cast<uint8_t>(token >> 8)) + dechex(static_cast<uint8_t>(token & 0xFF));
}

std::string detokenize_evo_token_words(const data_t& data)
{
    std::string out;
    for (size_t i = 0; i + 1 < data.size(); i += 2)
    {
        const uint16_t token = static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
        out += evo_token_to_string(token);
    }
    return out;
}

bool is_evo_tokenized_entry(const TIVarFile::var_entry_t& entry)
{
    const uint8_t evoTypeID = evo_type_from_type(entry._type, entry.evoTypeID);
    return evoTypeID == 2 || evoTypeID == 7 || evoTypeID == 10;
}

void append_legacy_token(data_t& out, uint16_t legacyToken)
{
    if (legacyToken > 0xFF)
    {
        out.push_back(static_cast<uint8_t>((legacyToken >> 8) & 0xFF));
    }
    out.push_back(static_cast<uint8_t>(legacyToken & 0xFF));
}

void append_evo_token(data_t& out, uint16_t evoToken)
{
    out.push_back(static_cast<uint8_t>(evoToken & 0xFF));
    out.push_back(static_cast<uint8_t>((evoToken >> 8) & 0xFF));
}

bool direct_legacy_token_for_evo(uint16_t evoToken, uint16_t& legacyToken)
{
    if (evoToken >= 0xE401 && evoToken <= 0xE40A)
    {
        legacyToken = static_cast<uint16_t>(0x30 + (evoToken - 0xE401));
        return true;
    }
    if (evoToken >= 0xE800 && evoToken <= 0xE819)
    {
        legacyToken = static_cast<uint16_t>(0x41 + (evoToken - 0xE800));
        return true;
    }
    if (evoToken >= 0xE820 && evoToken <= 0xE829)
    {
        legacyToken = static_cast<uint16_t>(0x5C00 + (evoToken - 0xE820));
        return true;
    }
    if (evoToken >= 0xE830 && evoToken <= 0xE835)
    {
        legacyToken = static_cast<uint16_t>(0x5D00 + (evoToken - 0xE830));
        return true;
    }
    if (evoToken >= 0xE840 && evoToken <= 0xE849)
    {
        legacyToken = static_cast<uint16_t>(0x5E10 + (evoToken - 0xE840));
        return true;
    }
    if (evoToken >= 0xE850 && evoToken <= 0xE85B)
    {
        legacyToken = static_cast<uint16_t>(0x5E20 + (evoToken - 0xE850));
        return true;
    }
    if (evoToken >= 0xE860 && evoToken <= 0xE865)
    {
        legacyToken = static_cast<uint16_t>(0x5E40 + (evoToken - 0xE860));
        return true;
    }
    if (evoToken >= 0xE870 && evoToken <= 0xE872)
    {
        legacyToken = static_cast<uint16_t>(0x5E80 + (evoToken - 0xE870));
        return true;
    }
    if (evoToken >= 0xE880 && evoToken <= 0xE889)
    {
        legacyToken = static_cast<uint16_t>(0x6000 + (evoToken - 0xE880));
        return true;
    }
    if (evoToken >= 0xE890 && evoToken <= 0xE899)
    {
        legacyToken = static_cast<uint16_t>(0x6100 + (evoToken - 0xE890));
        return true;
    }
    if (evoToken >= 0xE8A0 && evoToken <= 0xE8A9)
    {
        legacyToken = static_cast<uint16_t>(0xAA00 + (evoToken - 0xE8A0));
        return true;
    }
    if (evoToken >= 0x0061 && evoToken <= 0x006B)
    {
        legacyToken = static_cast<uint16_t>(0xBBB0 + (evoToken - 0x0061));
        return true;
    }
    if (evoToken >= 0x006C && evoToken <= 0x007A)
    {
        legacyToken = static_cast<uint16_t>(0xBBBC + (evoToken - 0x006C));
        return true;
    }

    static const std::unordered_map<uint16_t, uint16_t> direct = {
        {0xE000, 0xB9}, {0xE001, 0xBA}, {0xE400, 0xBBD9}, {0xE40B, 0x3A}, {0xE40C, 0x3B},
        {0xE40E, 0x2C}, {0xE40F, 0xBB31},
        {0xE410, 0x10}, {0xE411, 0x11}, {0xE412, 0x06}, {0xE413, 0x07},
        {0xE414, 0x08}, {0xE415, 0x09}, {0xE416, 0x2A}, {0xE417, 0x2B},
        {0xE418, 0x3E}, {0xE419, 0x29}, {0xE41A, 0xAE}, {0xE41C, 0x3F}, {0xE41D, 0x04},
        {0xE41B, 0xAF}, {0xE41E, 0x0C}, {0xE41F, 0x0D}, {0xE420, 0x0F}, {0xE421, 0x2D},
        {0xE422, 0x0A}, {0xE423, 0x0B}, {0xE463, 0x0E},
        {0xE40D, 0xAC},
        {0xE428, 0x70}, {0xE429, 0x71}, {0xE42A, 0x82}, {0xE42B, 0x83},
        {0xE42E, 0xB0},
        {0xE439, 0xBC}, {0xE43E, 0xC1},
        {0xE42C, 0xF0}, {0xE42D, 0xF1}, {0xE44D, 0x23}, {0xE47A, 0x3C}, {0xE47B, 0x3D},
        {0xE47C, 0x40}, {0xE47F, 0x6A}, {0xE480, 0x6B}, {0xE481, 0x6C},
        {0xE482, 0x6D}, {0xE483, 0x6E}, {0xE484, 0x6F}, {0xE81A, 0x5B},
        {0xE4C3, 0xD1}, {0xE4C6, 0xD4}, {0xE4EA, 0x85}, {0xE4F5, 0x93},
        {0xE604, 0x7E09},
        {0xE580, 0xEF37}, {0xE581, 0xEF38}, {0xE594, 0xEF39}, {0xE595, 0xEF3A},
        {0xE596, 0xEF3B}, {0xE597, 0xEF3C},
        {0x00C1, 0xBB6E}, {0x00C0, 0xBB6F}, {0x00C2, 0xBB70}, {0x00C4, 0xBB71},
        {0x00E1, 0xBB72}, {0x00E0, 0xBB73}, {0x00E2, 0xBB74}, {0x00E4, 0xBB75},
        {0x00C9, 0xBB76}, {0x00C8, 0xBB77}, {0x00CA, 0xBB78}, {0x00CB, 0xBB79},
        {0x00E9, 0xBB7A}, {0x00E8, 0xBB7B}, {0x00EA, 0xBB7C}, {0x00EB, 0xBB7D},
        {0x00CC, 0xBB7F}, {0x00CE, 0xBB80}, {0x00CF, 0xBB81}, {0x00ED, 0xBB82},
        {0x00EC, 0xBB83}, {0x00EE, 0xBB84}, {0x00EF, 0xBB85}, {0x00D3, 0xBB86},
        {0x00D2, 0xBB87}, {0x00D4, 0xBB88}, {0x00D6, 0xBB89}, {0x00F3, 0xBB8A},
        {0x00F2, 0xBB8B}, {0x00F4, 0xBB8C}, {0x00F6, 0xBB8D}, {0x00DA, 0xBB8E},
        {0x00D9, 0xBB8F}, {0x00DB, 0xBB90}, {0x00DC, 0xBB91}, {0x00FA, 0xBB92},
        {0x00F9, 0xBB93}, {0x00FB, 0xBB94}, {0x00FC, 0xBB95}, {0x00C7, 0xBB96},
        {0x00E7, 0xBB97}, {0x00D1, 0xBB98}, {0x00F1, 0xBB99}, {0x00B4, 0xBB9A},
        {0x0060, 0xBBD5}, {0x00A8, 0xBB9C}, {0x00BF, 0xBB9D}, {0x00A1, 0xBB9E},
        {0x03B1, 0xBB9F}, {0x03B2, 0xBBA0}, {0x03B3, 0xBBA1}, {0x0394, 0xBBA2},
        {0x03B4, 0xBBA3}, {0x03B5, 0xBBA4}, {0x03BB, 0xBBA5}, {0x03BC, 0xBBA6},
        {0x03C0, 0xBBA7}, {0x03C1, 0xBBA8}, {0x03A3, 0xBBA9}, {0x03A6, 0xBBAB},
        {0x03A9, 0xBBAC}, {0x03C7, 0xBBAE}, {0x007C, 0xBBD8}, {0x2026, 0xBBDB},
        {0x00D7, 0xBBF0}, {0x222B, 0xBBF1}, {0x2338, 0xBBF5},
        {0x007E, 0xBBCF}, {0x03C3, 0xBBCB}, {0x03C4, 0xBBCC}, {0x00CD, 0xBBCD},
        {0x0040, 0xBBD1}, {0x0023, 0xBBD2}, {0x0024, 0xBBD3}, {0x0026, 0xBBD4},
        {0x003B, 0xBBD6}, {0x005C, 0xBBD7}, {0x0025, 0xBBDA}, {0x2220, 0xBBDC},
        {0x00DF, 0xBBDD}, {0x02E3, 0xBBDE}, {0x1D1B, 0xBBDF}, {0x2080, 0xBBE0},
        {0x2081, 0xBBE1}, {0x2082, 0xBBE2}, {0x2083, 0xBBE3}, {0x2084, 0xBBE4},
        {0x2085, 0xBBE5}, {0x2086, 0xBBE6}, {0x2087, 0xBBE7}, {0x2088, 0xBBE8},
        {0x2089, 0xBBE9}, {0x25C4, 0xBBEB}, {0x25BA, 0xBBEC}, {0x2191, 0xBBED},
        {0x2193, 0xBBEE}, {0x221A, 0xBBF4}, {0x25A1, 0x7F}, {0xFE62, 0x80},
        {0x00B7, 0x81}, {0x029F, 0xEB}, {0xE836, 0xEB}, {0xE0D1, 0x22},
        {0xE5BD, 0x7F}, {0xE5BE, 0x80}, {0xE5BF, 0x81}, {0xE5C0, 0xEF73},
        {0xE5C1, 0xEF74}, {0xE5C2, 0xEF75},
        {0xE4F9, 0xBB57}, {0xE593, 0xBB64}, {0xE6C6, 0xE8}, {0xE6C7, 0xE7},
        {0xE900, 0x6201}, {0xE901, 0x6202}, {0xE902, 0x6203}, {0xE903, 0x6204},
        {0xE904, 0x6205}, {0xE905, 0x6206}, {0xE906, 0x6207}, {0xE907, 0x6208},
        {0xE908, 0x6209}, {0xE909, 0x620A}, {0xE90A, 0x620B}, {0xE90B, 0x620C},
        {0xE90C, 0x620D}, {0xE90D, 0x620E}, {0xE90E, 0x620F}, {0xE90F, 0x6210},
        {0xE910, 0x6211}, {0xE911, 0x6212}, {0xE912, 0x6213}, {0xE913, 0x6214},
        {0xE914, 0x6215}, {0xE915, 0x6216}, {0xE916, 0x6217}, {0xE917, 0x6218},
        {0xE918, 0x6219}, {0xE919, 0x621A},
        {0xE873, 0x6221}, {0xE91A, 0x621B}, {0xE91B, 0x621C}, {0xE91C, 0x621D}, {0xE91D, 0x621E},
        {0xE91E, 0x621F}, {0xE91F, 0x6220}, {0xE920, 0x6222}, {0xE921, 0x6223},
        {0xE922, 0x6224}, {0xE923, 0x6225}, {0xE924, 0x6226}, {0xE925, 0x6227},
        {0xE926, 0x6228}, {0xE927, 0x6229}, {0xE928, 0x622A}, {0xE929, 0x622B},
        {0xE92A, 0x622E}, {0xE92B, 0x622C}, {0xE92C, 0x622F}, {0xE92D, 0x622D},
        {0xE92E, 0x6230}, {0xE92F, 0x6231}, {0xE930, 0x6232}, {0xE931, 0x6233},
        {0xE932, 0x6234}, {0xE933, 0x6235}, {0xE934, 0x6236}, {0xE935, 0x6237},
        {0xE936, 0x6238}, {0xE937, 0x6239}, {0xE938, 0x623A}, {0xE939, 0x623B},
        {0xE93A, 0x623C}, {0xE980, 0x6304}, {0xE981, 0x6305}, {0xE982, 0x6332},
        {0xE983, 0x6306}, {0xE984, 0x6307}, {0xE985, 0x6308}, {0xE986, 0x6309},
        {0xE987, 0x6333}, {0xE98F, 0x630A}, {0xE990, 0x630B}, {0xE991, 0x6302},
        {0xE992, 0x6336}, {0xE993, 0x630C}, {0xE994, 0x630D}, {0xE995, 0x6303},
        {0xE996, 0x630E}, {0xE997, 0x630F}, {0xE998, 0x6322}, {0xE999, 0x6310},
        {0xE99A, 0x6311}, {0xE99B, 0x6323}, {0xE99F, 0x6337}, {0xE9A3, 0x6316},
        {0xE9A4, 0x6317}, {0xE9A5, 0x6325}, {0xE9A6, 0x6318}, {0xE9A7, 0x6319},
        {0xE9A8, 0x6324}, {0xE9A9, 0x631F}, {0xE9AA, 0x6320}, {0xE9AB, 0x631D},
        {0xE9AC, 0x631E}, {0xE9AD, 0x631B}, {0xE9AE, 0x631C}, {0xE9AF, 0x6334}, {0xE9B0, 0x6335},
        {0xE9B1, 0x6338}, {0xE9B2, 0x6326},
        {0xE9B3, 0x6327}, {0xE9B4, 0x6328}, {0xE9B5, 0x6329}, {0xE9B6, 0x631A},
        {0xE9B7, 0x6321}, {0xE9B8, 0x632A}, {0xE9B9, 0x632B}, {0xE9BA, 0x632C},
        {0xE9BB, 0x632D}, {0xE9BC, 0x632E}, {0xE9BD, 0x632F}, {0xE9BE, 0x6330},
        {0xE9BF, 0x6331}, {0xE9D9, 0x6312}, {0xE9DA, 0x6313}, {0xE9DB, 0x6300},
        {0xE9DC, 0x6314}, {0xE9DD, 0x6315}, {0xE9DE, 0x6301},
    };

    const auto it = direct.find(evoToken);
    if (it == direct.end())
    {
        return false;
    }
    legacyToken = it->second;
    return true;
}

bool direct_evo_token_for_legacy(uint16_t legacyToken, uint16_t& evoToken)
{
    if (legacyToken >= 0x30 && legacyToken <= 0x39)
    {
        evoToken = static_cast<uint16_t>(0xE401 + (legacyToken - 0x30));
        return true;
    }
    if (legacyToken >= 0x41 && legacyToken <= 0x5A)
    {
        evoToken = static_cast<uint16_t>(0xE800 + (legacyToken - 0x41));
        return true;
    }
    if (legacyToken >= 0x5C00 && legacyToken <= 0x5C09)
    {
        evoToken = static_cast<uint16_t>(0xE820 + (legacyToken - 0x5C00));
        return true;
    }
    if (legacyToken >= 0x5D00 && legacyToken <= 0x5D05)
    {
        evoToken = static_cast<uint16_t>(0xE830 + (legacyToken - 0x5D00));
        return true;
    }
    if (legacyToken >= 0x5E10 && legacyToken <= 0x5E19)
    {
        evoToken = static_cast<uint16_t>(0xE840 + (legacyToken - 0x5E10));
        return true;
    }
    if (legacyToken >= 0x5E20 && legacyToken <= 0x5E2B)
    {
        evoToken = static_cast<uint16_t>(0xE850 + (legacyToken - 0x5E20));
        return true;
    }
    if (legacyToken >= 0x5E40 && legacyToken <= 0x5E45)
    {
        evoToken = static_cast<uint16_t>(0xE860 + (legacyToken - 0x5E40));
        return true;
    }
    if (legacyToken >= 0x5E80 && legacyToken <= 0x5E82)
    {
        evoToken = static_cast<uint16_t>(0xE870 + (legacyToken - 0x5E80));
        return true;
    }
    if (legacyToken >= 0x6000 && legacyToken <= 0x6009)
    {
        evoToken = static_cast<uint16_t>(0xE880 + (legacyToken - 0x6000));
        return true;
    }
    if (legacyToken >= 0x6100 && legacyToken <= 0x6109)
    {
        evoToken = static_cast<uint16_t>(0xE890 + (legacyToken - 0x6100));
        return true;
    }
    if (legacyToken >= 0xAA00 && legacyToken <= 0xAA09)
    {
        evoToken = static_cast<uint16_t>(0xE8A0 + (legacyToken - 0xAA00));
        return true;
    }
    if (legacyToken >= 0xBBB0 && legacyToken <= 0xBBBA)
    {
        evoToken = static_cast<uint16_t>(0x0061 + (legacyToken - 0xBBB0));
        return true;
    }
    if (legacyToken >= 0xBBBC && legacyToken <= 0xBBCA)
    {
        evoToken = static_cast<uint16_t>(0x006C + (legacyToken - 0xBBBC));
        return true;
    }

    static const std::unordered_map<uint16_t, uint16_t> direct = {
        {0xBBD9, 0xE400}, {0x3A, 0xE40B}, {0x3B, 0xE40C}, {0x2C, 0xE40E}, {0xBB31, 0xE40F},
        {0x10, 0xE410}, {0x11, 0xE411},
        {0x06, 0xE412}, {0x07, 0xE413}, {0x08, 0xE414}, {0x09, 0xE415},
        {0x2A, 0xE416}, {0x2B, 0xE417}, {0x3E, 0xE418}, {0x29, 0xE419},
        {0xAE, 0xE41A}, {0x3F, 0xE41C}, {0x04, 0xE41D}, {0xAF, 0xE41B}, {0x0C, 0xE41E}, {0x0D, 0xE41F},
        {0x0F, 0xE420}, {0x2D, 0xE421}, {0x0A, 0xE422}, {0x0B, 0xE423},
        {0x0E, 0xE463}, {0xAC, 0xE40D}, {0x23, 0xE44D}, {0x70, 0xE428}, {0x71, 0xE429},
        {0xBC, 0xE439}, {0xC1, 0xE43E},
        {0x82, 0xE42A}, {0x83, 0xE42B}, {0xF0, 0xE42C}, {0xF1, 0xE42D},
        {0xB0, 0xE42E},
        {0x3C, 0xE47A}, {0x3D, 0xE47B}, {0x40, 0xE47C}, {0x6A, 0xE47F},
        {0x6B, 0xE480}, {0x6C, 0xE481}, {0x6D, 0xE482}, {0x6E, 0xE483},
        {0x6F, 0xE484}, {0x5B, 0xE81A},
        {0xD1, 0xE4C3}, {0xD4, 0xE4C6}, {0x85, 0xE4EA}, {0x93, 0xE4F5},
        {0x7E09, 0xE604},
        {0xEF37, 0xE580}, {0xEF38, 0xE581}, {0xEF39, 0xE594}, {0xEF3A, 0xE595},
        {0xEF3B, 0xE596}, {0xEF3C, 0xE597},
        {0xBB6E, 0x00C1}, {0xBB6F, 0x00C0}, {0xBB70, 0x00C2}, {0xBB71, 0x00C4},
        {0xBB72, 0x00E1}, {0xBB73, 0x00E0}, {0xBB74, 0x00E2}, {0xBB75, 0x00E4},
        {0xBB76, 0x00C9}, {0xBB77, 0x00C8}, {0xBB78, 0x00CA}, {0xBB79, 0x00CB},
        {0xBB7A, 0x00E9}, {0xBB7B, 0x00E8}, {0xBB7C, 0x00EA}, {0xBB7D, 0x00EB},
        {0xBB7F, 0x00CC}, {0xBB80, 0x00CE}, {0xBB81, 0x00CF}, {0xBB82, 0x00ED},
        {0xBB83, 0x00EC}, {0xBB84, 0x00EE}, {0xBB85, 0x00EF}, {0xBB86, 0x00D3},
        {0xBB87, 0x00D2}, {0xBB88, 0x00D4}, {0xBB89, 0x00D6}, {0xBB8A, 0x00F3},
        {0xBB8B, 0x00F2}, {0xBB8C, 0x00F4}, {0xBB8D, 0x00F6}, {0xBB8E, 0x00DA},
        {0xBB8F, 0x00D9}, {0xBB90, 0x00DB}, {0xBB91, 0x00DC}, {0xBB92, 0x00FA},
        {0xBB93, 0x00F9}, {0xBB94, 0x00FB}, {0xBB95, 0x00FC}, {0xBB96, 0x00C7},
        {0xBB97, 0x00E7}, {0xBB98, 0x00D1}, {0xBB99, 0x00F1}, {0xBB9A, 0x00B4},
        {0xBB9B, 0xF019}, {0xBB9C, 0x00A8}, {0xBB9D, 0x00BF}, {0xBB9E, 0x00A1},
        {0xBB9F, 0x03B1}, {0xBBA0, 0x03B2}, {0xBBA1, 0x03B3}, {0xBBA2, 0x0394},
        {0xBBA3, 0x03B4}, {0xBBA4, 0x03B5}, {0xBBA5, 0x03BB}, {0xBBA6, 0x03BC},
        {0xBBA7, 0x03C0}, {0xBBA8, 0x03C1}, {0xBBA9, 0x03A3}, {0xBBAB, 0x03A6},
        {0xBBAC, 0x03A9}, {0xBBAE, 0x03C7}, {0xBBD8, 0x007C}, {0xBBDB, 0x2026},
        {0xBBF0, 0x00D7}, {0xBBF1, 0x222B}, {0xBBF5, 0x2338},
        {0xBBCF, 0x007E}, {0xBBCB, 0x03C3}, {0xBBCC, 0x03C4}, {0xBBCD, 0x00CD},
        {0xBBD1, 0x0040}, {0xBBD2, 0x0023}, {0xBBD3, 0x0024}, {0xBBD4, 0x0026},
        {0xBBD5, 0x0060}, {0xBBD6, 0x003B}, {0xBBD7, 0x005C}, {0xBBDA, 0x0025}, {0xBBDC, 0x2220},
        {0xBBDD, 0x00DF}, {0xBBDE, 0x02E3}, {0xBBDF, 0x1D1B}, {0xBBE0, 0x2080},
        {0xBBE1, 0x2081}, {0xBBE2, 0x2082}, {0xBBE3, 0x2083}, {0xBBE4, 0x2084},
        {0xBBE5, 0x2085}, {0xBBE6, 0x2086}, {0xBBE7, 0x2087}, {0xBBE8, 0x2088},
        {0xBBE9, 0x2089}, {0xBBEB, 0x25C4}, {0xBBEC, 0x25BA}, {0xBBED, 0x2191},
        {0xBBEE, 0x2193}, {0xBBF4, 0x221A}, {0xEB, 0xE836}, {0x22, 0xE0D1},
        {0x7F, 0xE5BD}, {0x80, 0xE5BE}, {0x81, 0xE5BF}, {0xEF73, 0xE5C0},
        {0xEF74, 0xE5C1}, {0xEF75, 0xE5C2},
        {0xBB57, 0xE4F9}, {0xBB64, 0xE593}, {0xE8, 0xE6C6}, {0xE7, 0xE6C7},
        {0x6201, 0xE900}, {0x6202, 0xE901}, {0x6203, 0xE902}, {0x6204, 0xE903},
        {0x6205, 0xE904}, {0x6206, 0xE905}, {0x6207, 0xE906}, {0x6208, 0xE907},
        {0x6209, 0xE908}, {0x620A, 0xE909}, {0x620B, 0xE90A}, {0x620C, 0xE90B},
        {0x620D, 0xE90C}, {0x620E, 0xE90D}, {0x620F, 0xE90E}, {0x6210, 0xE90F},
        {0x6211, 0xE910}, {0x6212, 0xE911}, {0x6213, 0xE912}, {0x6214, 0xE913},
        {0x6215, 0xE914}, {0x6216, 0xE915}, {0x6217, 0xE916}, {0x6218, 0xE917},
        {0x6219, 0xE918}, {0x621A, 0xE919},
        {0x6221, 0xE873}, {0x621B, 0xE91A}, {0x621C, 0xE91B}, {0x621D, 0xE91C}, {0x621E, 0xE91D},
        {0x621F, 0xE91E}, {0x6220, 0xE91F}, {0x6222, 0xE920}, {0x6223, 0xE921},
        {0x6224, 0xE922}, {0x6225, 0xE923}, {0x6226, 0xE924}, {0x6227, 0xE925},
        {0x6228, 0xE926}, {0x6229, 0xE927}, {0x622A, 0xE928}, {0x622B, 0xE929},
        {0x622E, 0xE92A}, {0x622C, 0xE92B}, {0x622F, 0xE92C}, {0x622D, 0xE92D},
        {0x6230, 0xE92E}, {0x6231, 0xE92F}, {0x6232, 0xE930}, {0x6233, 0xE931},
        {0x6234, 0xE932}, {0x6235, 0xE933}, {0x6236, 0xE934}, {0x6237, 0xE935},
        {0x6238, 0xE936}, {0x6239, 0xE937}, {0x623A, 0xE938}, {0x623B, 0xE939},
        {0x623C, 0xE93A}, {0x6304, 0xE980}, {0x6305, 0xE981}, {0x6332, 0xE982},
        {0x6306, 0xE983}, {0x6307, 0xE984}, {0x6308, 0xE985}, {0x6309, 0xE986},
        {0x6333, 0xE987}, {0x630A, 0xE98F}, {0x630B, 0xE990}, {0x6302, 0xE991},
        {0x6336, 0xE992}, {0x630C, 0xE993}, {0x630D, 0xE994}, {0x6303, 0xE995},
        {0x630E, 0xE996}, {0x630F, 0xE997}, {0x6322, 0xE998}, {0x6310, 0xE999},
        {0x6311, 0xE99A}, {0x6323, 0xE99B}, {0x6337, 0xE99F}, {0x6316, 0xE9A3},
        {0x6317, 0xE9A4}, {0x6325, 0xE9A5}, {0x6318, 0xE9A6}, {0x6319, 0xE9A7},
        {0x6324, 0xE9A8}, {0x631F, 0xE9A9}, {0x6320, 0xE9AA}, {0x631D, 0xE9AB},
        {0x631E, 0xE9AC}, {0x631B, 0xE9AD}, {0x631C, 0xE9AE}, {0x6334, 0xE9AF}, {0x6335, 0xE9B0},
        {0x6338, 0xE9B1}, {0x6326, 0xE9B2},
        {0x6327, 0xE9B3}, {0x6328, 0xE9B4}, {0x6329, 0xE9B5}, {0x631A, 0xE9B6},
        {0x6321, 0xE9B7}, {0x632A, 0xE9B8}, {0x632B, 0xE9B9}, {0x632C, 0xE9BA},
        {0x632D, 0xE9BB}, {0x632E, 0xE9BC}, {0x632F, 0xE9BD}, {0x6330, 0xE9BE},
        {0x6331, 0xE9BF}, {0x6312, 0xE9D9}, {0x6313, 0xE9DA}, {0x6300, 0xE9DB},
        {0x6314, 0xE9DC}, {0x6315, 0xE9DD}, {0x6301, 0xE9DE},
    };

    const auto it = direct.find(legacyToken);
    if (it == direct.end())
    {
        return false;
    }
    evoToken = it->second;
    return true;
}

bool tokenized_legacy_payload_for_evo(uint16_t evoToken, data_t& payload)
{
    uint16_t legacyToken = 0;
    if (direct_legacy_token_for_evo(evoToken, legacyToken))
    {
        payload.clear();
        append_legacy_token(payload, legacyToken);
        return true;
    }

    const std::string source = evo_token_to_string(evoToken);
    if (source.empty() || source.rfind("TOK_", 0) == 0)
    {
        return false;
    }

    try
    {
        data_t data = TypeHandlers::TH_Tokenized::makeDataFromString(source, {{"detect_strings", 0}});
        if (data.size() < 3)
        {
            return false;
        }
        payload.assign(data.begin() + 2, data.end());
        return payload.size() <= 2;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

const std::unordered_map<std::string, uint16_t>& evo_source_to_token_map()
{
    static const std::unordered_map<std::string, uint16_t> map = [] {
        std::unordered_map<std::string, uint16_t> out;
        for (const auto& [value, name] : evoTokenInfos)
        {
            const std::string source = evo_token_to_string(value);
            if (!source.empty() && source.rfind("TOK_", 0) != 0)
            {
                out.emplace(source, value);
            }
        }
        return out;
    }();
    return map;
}

bool evo_token_for_legacy_token(uint16_t legacyToken, uint16_t& evoToken)
{
    if (direct_evo_token_for_legacy(legacyToken, evoToken))
    {
        return true;
    }

    const std::string source = TypeHandlers::TH_Tokenized::oneTokenBytesToString(legacyToken);
    const auto& map = evo_source_to_token_map();
    const auto it = map.find(source);
    if (it != map.end())
    {
        evoToken = it->second;
        return true;
    }
    return false;
}

data_t evo_tokenized_data_to_legacy(const data_t& evoData)
{
    data_t payload;
    for (size_t i = 0; i + 1 < evoData.size(); i += 2)
    {
        const uint16_t evoToken = static_cast<uint16_t>(evoData[i] | (evoData[i + 1] << 8));
        if (evoToken == 0)
        {
            break;
        }

        data_t legacyTokenPayload;
        if (!tokenized_legacy_payload_for_evo(evoToken, legacyTokenPayload))
        {
            const char* name = evo_token_name(evoToken);
            std::cerr << "[Warning] Cannot convert Evo token " << (name == nullptr ? evo_token_to_string(evoToken) : name)
                      << " to an 84+CE token; replacing with '?'." << std::endl;
            append_legacy_token(legacyTokenPayload, 0xAF);
        }
        payload.insert(payload.end(), legacyTokenPayload.begin(), legacyTokenPayload.end());
    }

    data_t legacy = {
        static_cast<uint8_t>(payload.size() & 0xFF),
        static_cast<uint8_t>((payload.size() >> 8) & 0xFF)
    };
    legacy.insert(legacy.end(), payload.begin(), payload.end());
    return legacy;
}

data_t legacy_tokenized_data_to_evo(const data_t& legacyData)
{
    if (legacyData.size() < 2)
    {
        throw std::invalid_argument("Invalid tokenized legacy data");
    }

    data_t evo;
    for (size_t i = 2; i < legacyData.size();)
    {
        int incr = 1;
        TypeHandlers::TH_Tokenized::tokenToString(data_t(legacyData.begin() + i, legacyData.end()), &incr, {});
        if (incr <= 0 || i + static_cast<size_t>(incr) > legacyData.size())
        {
            throw std::runtime_error("Invalid tokenized legacy data");
        }

        const uint16_t legacyToken = incr == 2
            ? static_cast<uint16_t>((legacyData[i] << 8) | legacyData[i + 1])
            : legacyData[i];
        uint16_t evoToken = 0;
        if (!evo_token_for_legacy_token(legacyToken, evoToken))
        {
            std::cerr << "[Warning] Cannot convert 84+CE token "
                      << TypeHandlers::TH_Tokenized::oneTokenBytesToString(legacyToken)
                      << " to an Evo token; replacing with ?" << std::endl;
            evoToken = 0xE41B;
        }
        append_evo_token(evo, evoToken);
        i += static_cast<size_t>(incr);
    }
    append_evo_token(evo, 0);
    return evo;
}

uint16_t read_le16_at(const data_t& data, size_t offset)
{
    if (offset + 1 >= data.size())
    {
        throw std::invalid_argument("Unexpected end of Evo data");
    }
    return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
}

void append_le16(data_t& data, uint16_t word)
{
    data.push_back(static_cast<uint8_t>(word & 0xFF));
    data.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
}

std::vector<uint16_t> evo_words(const data_t& data)
{
    std::vector<uint16_t> words;
    words.reserve(data.size() / 2);
    for (size_t i = 0; i + 1 < data.size(); i += 2)
    {
        words.push_back(read_le16_at(data, i));
    }
    return words;
}

uint8_t real_equivalent_subtype(uint8_t subtype)
{
    switch (subtype)
    {
        case 0x0C: return 0x00;
        case 0x1B: return 0x18;
        case 0x1D: return 0x1C;
        case 0x1E: return 0x20;
        case 0x1F: return 0x21;
        default: return subtype;
    }
}

data_t normalize_legacy_real_member(data_t data)
{
    if (data.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy real payload");
    }
    data[0] = static_cast<uint8_t>((data[0] & 0x80) | real_equivalent_subtype(data[0] & 0x3F));
    return data;
}

bool legacy_real_is_zero(data_t data)
{
    if (data.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        return false;
    }
    data = normalize_legacy_real_member(data);
    return TypeHandlers::TH_GenericReal::makeStringFromData(data) == "0";
}

data_t legacy_fp_to_evo_decimal(const data_t& legacyReal)
{
    if (legacyReal.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy real payload");
    }

    data_t evo;
    evo.reserve(12);
    evo.push_back(0);
    for (size_t i = 0; i < 7; i++)
    {
        evo.push_back(legacyReal[8 - i]);
    }
    evo.push_back((legacyReal[0] & 0x80) ? 0xFF : 0x01);
    evo.push_back(static_cast<uint8_t>(legacyReal[1] - 0x80));
    append_le16(evo, 0x0023);
    return evo;
}

data_t evo_decimal_to_legacy_fp(const data_t& evoData, size_t offset, uint8_t legacyType)
{
    if (offset + 11 >= evoData.size() || read_le16_at(evoData, offset + 10) != 0x0023)
    {
        throw std::invalid_argument("Invalid Evo decimal payload");
    }

    data_t legacy(TypeHandlers::TH_GenericReal::dataByteCount, 0);
    legacy[0] = legacyType;
    if (evoData[offset + 8] == 0xFF)
    {
        legacy[0] |= 0x80;
    }
    legacy[1] = static_cast<uint8_t>(0x80 + static_cast<int8_t>(evoData[offset + 9]));
    for (size_t i = 0; i < 7; i++)
    {
        legacy[2 + i] = evoData[offset + 7 - i];
    }
    if (std::ranges::all_of(data_t(legacy.begin() + 2, legacy.end()), [](uint8_t byte) { return byte == 0; }))
    {
        legacy[1] = 0x80;
    }
    return legacy;
}

uint64_t parse_evo_integer_limbs(const std::vector<uint16_t>& words, size_t begin, size_t end)
{
    if (end <= begin)
    {
        return 0;
    }
    const uint16_t limbCount = words[end - 1];
    if (begin + limbCount != end - 1)
    {
        throw std::invalid_argument("Invalid Evo integer limb count");
    }

    uint64_t value = 0;
    for (size_t i = 0; i < limbCount; i++)
    {
        if (i >= 4 && words[begin + i] != 0)
        {
            throw std::runtime_error("Evo integer is too large to convert");
        }
        value |= static_cast<uint64_t>(words[begin + i]) << (16 * i);
    }
    return value;
}

data_t decimal_string_to_legacy_real(const std::string& value, uint8_t legacyType)
{
    data_t legacy = TypeHandlers::STH_FP::makeDataFromString(value);
    legacy[0] = static_cast<uint8_t>((legacy[0] & 0x80) | (legacyType & 0x3F));
    return legacy;
}

data_t evo_scalar_to_legacy_real(const data_t& evoData, size_t& offset, uint8_t legacyType = 0x00)
{
    const std::vector<uint16_t> words = evo_words(evoData);
    const size_t wordOffset = offset / 2;
    if (offset % 2 != 0 || wordOffset >= words.size())
    {
        throw std::invalid_argument("Invalid Evo scalar offset");
    }

    for (size_t i = wordOffset; i < words.size(); i++)
    {
        const uint16_t tag = words[i];
        if (tag == 0x0023)
        {
            data_t legacy = evo_decimal_to_legacy_fp(evoData, offset, legacyType);
            offset = (i + 1) * 2;
            return legacy;
        }
        if (tag == 0x001F || tag == 0x0020)
        {
            uint64_t magnitude = 0;
            if (i == wordOffset + 1 && words[wordOffset] == 0)
            {
                magnitude = 0;
            }
            else
            {
                magnitude = parse_evo_integer_limbs(words, wordOffset, i);
            }
            std::string value = std::to_string(magnitude);
            if (tag == 0x0020 && magnitude != 0)
            {
                value.insert(value.begin(), '-');
            }
            offset = (i + 1) * 2;
            return decimal_string_to_legacy_real(value, legacyType);
        }
        if (tag == 0x0021 || tag == 0x0022)
        {
            if (i <= wordOffset + 3)
            {
                throw std::invalid_argument("Invalid Evo fraction payload");
            }
            const uint16_t numCount = words[i - 1];
            const size_t numBegin = i - 1 - numCount;
            if (numBegin <= wordOffset)
            {
                throw std::invalid_argument("Invalid Evo fraction numerator");
            }
            const uint16_t denCount = words[numBegin - 1];
            if (wordOffset + denCount != numBegin - 1)
            {
                throw std::invalid_argument("Invalid Evo fraction denominator");
            }

            const uint64_t denominator = parse_evo_integer_limbs(words, wordOffset, numBegin);
            const uint64_t numerator = parse_evo_integer_limbs(words, numBegin, i);
            if (denominator == 0)
            {
                throw std::invalid_argument("Invalid Evo fraction denominator");
            }

            std::string value = std::to_string(numerator) + "/" + std::to_string(denominator);
            if (tag == 0x0022 && numerator != 0)
            {
                value.insert(value.begin(), '-');
            }
            data_t legacy = TypeHandlers::STH_ExactFraction::makeDataFromString(value);
            legacy[0] = static_cast<uint8_t>((legacy[0] & 0x80) | (legacyType == 0x0C ? 0x1B : 0x18));
            offset = (i + 1) * 2;
            return legacy;
        }
    }

    throw std::invalid_argument("Unterminated Evo scalar payload");
}

data_t evo_scalar_to_legacy_value(const data_t& evoData, size_t& offset)
{
    data_t first = evo_scalar_to_legacy_real(evoData, offset);
    if (offset + 3 < evoData.size() && read_le16_at(evoData, offset) == 0x0026 && read_le16_at(evoData, offset + 2) == 0x008F)
    {
        offset += 4;
        const bool negativeImag = offset + 1 < evoData.size() && read_le16_at(evoData, offset) == 0x007A;
        if (negativeImag)
        {
            first[0] |= 0x80;
            offset += 2;
        }
        data_t complex(TypeHandlers::TH_GenericComplex::dataByteCount, 0);
        complex[0] = 0x0C;
        complex[1] = 0x80;
        complex.insert(complex.begin() + TypeHandlers::TH_GenericReal::dataByteCount, first.begin(), first.end());
        complex.resize(TypeHandlers::TH_GenericComplex::dataByteCount);
        complex[TypeHandlers::TH_GenericReal::dataByteCount] = static_cast<uint8_t>((complex[TypeHandlers::TH_GenericReal::dataByteCount] & 0x80) | 0x0C);
        return complex;
    }

    if (offset < evoData.size())
    {
        try
        {
            size_t secondOffset = offset;
            data_t second = evo_scalar_to_legacy_real(evoData, secondOffset);
            if (secondOffset + 5 < evoData.size()
             && read_le16_at(evoData, secondOffset) == 0x0026
             && read_le16_at(evoData, secondOffset + 2) == 0x008F
             && (read_le16_at(evoData, secondOffset + 4) == 0x008B || read_le16_at(evoData, secondOffset + 4) == 0x008D))
            {
                if (read_le16_at(evoData, secondOffset + 4) == 0x008D)
                {
                    second[0] |= 0x80;
                }
                offset = secondOffset + 6;
                data_t complex = first;
                complex[0] = static_cast<uint8_t>((complex[0] & 0x80) | 0x0C);
                second[0] = static_cast<uint8_t>((second[0] & 0x80) | 0x0C);
                complex.insert(complex.end(), second.begin(), second.end());
                return complex;
            }
        }
        catch (const std::exception&)
        {
        }
    }

    return first;
}

void append_evo_uint64(data_t& data, uint64_t value)
{
    uint16_t limbCount = 0;
    do
    {
        append_le16(data, static_cast<uint16_t>(value & 0xFFFF));
        value >>= 16;
        limbCount++;
    }
    while (value != 0);
    append_le16(data, limbCount);
}

data_t legacy_real_to_evo_scalar(data_t legacyReal)
{
    if (legacyReal.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy real payload");
    }

    legacyReal = normalize_legacy_real_member(legacyReal);
    const uint8_t subtype = legacyReal[0] & 0x3F;
    if (subtype == 0x00 || subtype == 0x0E)
    {
        legacyReal[0] = static_cast<uint8_t>(legacyReal[0] & 0x80);
        return legacy_fp_to_evo_decimal(legacyReal);
    }

    std::string value = TypeHandlers::TH_GenericReal::makeStringFromData(legacyReal);
    if (value.find('/') != std::string::npos)
    {
        const bool negative = !value.empty() && value[0] == '-';
        if (negative)
        {
            value.erase(value.begin());
        }
        const size_t slash = value.find('/');
        const uint64_t numerator = std::stoull(value.substr(0, slash));
        const uint64_t denominator = std::stoull(value.substr(slash + 1));
        data_t evo;
        append_evo_uint64(evo, denominator);
        append_evo_uint64(evo, numerator);
        append_le16(evo, negative ? 0x0022 : 0x0021);
        return evo;
    }

    legacyReal = TypeHandlers::STH_FP::makeDataFromString(value);
    return legacy_fp_to_evo_decimal(legacyReal);
}

data_t legacy_value_to_evo_expression(const data_t& legacyValue)
{
    if (legacyValue.size() == TypeHandlers::TH_GenericReal::dataByteCount)
    {
        return legacy_real_to_evo_scalar(legacyValue);
    }
    if (legacyValue.size() != TypeHandlers::TH_GenericComplex::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy numeric payload");
    }

    data_t real(legacyValue.begin(), legacyValue.begin() + TypeHandlers::TH_GenericReal::dataByteCount);
    data_t imag(legacyValue.begin() + TypeHandlers::TH_GenericReal::dataByteCount, legacyValue.end());
    const bool realZero = legacy_real_is_zero(real);
    const bool imagZero = legacy_real_is_zero(imag);
    if (imagZero)
    {
        return legacy_real_to_evo_scalar(real);
    }

    const bool imagNegative = (imag[0] & 0x80) != 0;
    imag[0] &= 0x7F;

    data_t evo;
    if (!realZero)
    {
        vector_append(evo, legacy_real_to_evo_scalar(real));
    }
    vector_append(evo, legacy_real_to_evo_scalar(imag));
    append_le16(evo, 0x0026);
    append_le16(evo, 0x008F);
    if (realZero)
    {
        if (imagNegative)
        {
            append_le16(evo, 0x007A);
        }
    }
    else
    {
        append_le16(evo, imagNegative ? 0x008D : 0x008B);
    }
    return evo;
}

bool legacy_value_is_exact_fraction(const data_t& legacyValue)
{
    return !legacyValue.empty() && ((legacyValue[0] & 0x3F) == 0x18 || (legacyValue[0] & 0x1F) == 0x1B);
}

data_t legacy_list_to_evo(const data_t& legacyData, bool complexList, std::map<std::string, uint64_t>& fields)
{
    if (legacyData.size() < 2)
    {
        throw std::invalid_argument("Invalid legacy list payload");
    }
    const size_t count = legacyData[0] | (legacyData[1] << 8);
    const size_t itemSize = complexList ? TypeHandlers::TH_GenericComplex::dataByteCount : TypeHandlers::TH_GenericReal::dataByteCount;
    if (legacyData.size() != 2 + count * itemSize)
    {
        throw std::invalid_argument("Invalid legacy list length");
    }

    if (count == 0)
    {
        fields["version"] = 1;
        fields["type"] = 0;
        fields["len"] = 0;
        return {};
    }

    data_t evo;
    append_le16(evo, 0x00E5);
    std::vector<uint8_t> flags(count, 0);
    for (size_t i = count; i > 0; i--)
    {
        const size_t src = 2 + (i - 1) * itemSize;
        data_t value(legacyData.begin() + static_cast<ptrdiff_t>(src), legacyData.begin() + static_cast<ptrdiff_t>(src + itemSize));
        if (legacy_value_is_exact_fraction(value))
        {
            flags[i - 1] = 6;
        }
        vector_append(evo, legacy_value_to_evo_expression(value));
    }
    append_le16(evo, 0x00D9);
    const size_t arrayLen = evo.size() / 2;
    evo.insert(evo.end(), flags.begin(), flags.end());

    fields["version"] = 1;
    fields["type"] = 0;
    fields["len"] = count;
    fields["arraylen"] = arrayLen;
    fields["size"] = evo.size();
    return evo;
}

data_t evo_list_to_legacy(const data_t& evoData, uint64_t len, bool& complexList)
{
    complexList = false;
    if (len == 0)
    {
        return {0, 0};
    }
    if (evoData.size() < 4 || read_le16_at(evoData, 0) != 0x00E5)
    {
        throw std::invalid_argument("Invalid Evo list payload");
    }

    std::vector<data_t> payloadOrder;
    size_t offset = 2;
    while (offset + 1 < evoData.size() && read_le16_at(evoData, offset) != 0x00D9)
    {
        data_t value = evo_scalar_to_legacy_value(evoData, offset);
        if (value.size() == TypeHandlers::TH_GenericComplex::dataByteCount)
        {
            complexList = true;
        }
        payloadOrder.push_back(value);
    }
    if (offset + 1 >= evoData.size() || payloadOrder.size() != len)
    {
        throw std::invalid_argument("Invalid Evo list element count");
    }

    data_t legacy = {static_cast<uint8_t>(len & 0xFF), static_cast<uint8_t>((len >> 8) & 0xFF)};
    for (size_t i = payloadOrder.size(); i > 0; i--)
    {
        data_t value = payloadOrder[i - 1];
        if (complexList && value.size() == TypeHandlers::TH_GenericReal::dataByteCount)
        {
            data_t imag(TypeHandlers::TH_GenericReal::dataByteCount, 0);
            imag[0] = 0x0C;
            imag[1] = 0x80;
            value[0] = static_cast<uint8_t>((value[0] & 0x80) | 0x0C);
            value.insert(value.end(), imag.begin(), imag.end());
        }
        legacy.insert(legacy.end(), value.begin(), value.end());
    }
    return legacy;
}

data_t legacy_matrix_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields)
{
    if (legacyData.size() < 2)
    {
        throw std::invalid_argument("Invalid legacy matrix payload");
    }
    const size_t cols = legacyData[0];
    const size_t rows = legacyData[1];
    if (cols == 0 || rows == 0 || legacyData.size() != 2 + rows * cols * TypeHandlers::TH_GenericReal::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy matrix length");
    }

    data_t evo;
    append_le16(evo, 0x00E5);
    std::vector<uint8_t> flags(rows * cols, 0);
    for (size_t row = rows; row > 0; row--)
    {
        append_le16(evo, 0x00E5);
        for (size_t col = cols; col > 0; col--)
        {
            const size_t idx = (row - 1) * cols + (col - 1);
            const size_t src = 2 + idx * TypeHandlers::TH_GenericReal::dataByteCount;
            data_t value(legacyData.begin() + static_cast<ptrdiff_t>(src), legacyData.begin() + static_cast<ptrdiff_t>(src + TypeHandlers::TH_GenericReal::dataByteCount));
            if (legacy_value_is_exact_fraction(value))
            {
                flags[idx] = 6;
            }
            vector_append(evo, legacy_value_to_evo_expression(value));
        }
        append_le16(evo, 0x00D9);
    }
    append_le16(evo, 0x00D9);
    const size_t arrayLen = evo.size() / 2;
    evo.insert(evo.end(), flags.begin(), flags.end());

    fields["version"] = 1;
    fields["rows"] = rows;
    fields["cols"] = cols;
    fields["arraylen"] = arrayLen;
    fields["size"] = evo.size();
    return evo;
}

data_t evo_matrix_to_legacy(const data_t& evoData, uint64_t rows, uint64_t cols)
{
    if (rows == 0 || cols == 0 || rows > 255 || cols > 255 || evoData.size() < 4 || read_le16_at(evoData, 0) != 0x00E5)
    {
        throw std::invalid_argument("Invalid Evo matrix payload");
    }

    std::vector<data_t> cells(static_cast<size_t>(rows * cols));
    size_t offset = 2;
    for (size_t payloadRow = 0; payloadRow < rows; payloadRow++)
    {
        if (offset + 1 >= evoData.size() || read_le16_at(evoData, offset) != 0x00E5)
        {
            throw std::invalid_argument("Invalid Evo matrix row payload");
        }
        offset += 2;
        for (size_t payloadCol = 0; payloadCol < cols; payloadCol++)
        {
            data_t value = evo_scalar_to_legacy_value(evoData, offset);
            if (value.size() != TypeHandlers::TH_GenericReal::dataByteCount)
            {
                throw std::runtime_error("Complex matrix cells cannot be converted to legacy CE matrix data");
            }
            const size_t displayRow = static_cast<size_t>(rows) - payloadRow - 1;
            const size_t displayCol = static_cast<size_t>(cols) - payloadCol - 1;
            cells[displayRow * static_cast<size_t>(cols) + displayCol] = value;
        }
        if (offset + 1 >= evoData.size() || read_le16_at(evoData, offset) != 0x00D9)
        {
            throw std::invalid_argument("Invalid Evo matrix row terminator");
        }
        offset += 2;
    }
    if (offset + 1 >= evoData.size() || read_le16_at(evoData, offset) != 0x00D9)
    {
        throw std::invalid_argument("Invalid Evo matrix terminator");
    }

    data_t legacy = {static_cast<uint8_t>(cols), static_cast<uint8_t>(rows)};
    for (const data_t& cell : cells)
    {
        legacy.insert(legacy.end(), cell.begin(), cell.end());
    }
    return legacy;
}

namespace
{
    uint8_t get_4bpp_pixel(const data_t& data, size_t baseOffset, size_t rowStride, size_t x, size_t y)
    {
        const uint8_t packed = data[baseOffset + y * rowStride + x / 2];
        return (x % 2 == 0) ? static_cast<uint8_t>(packed >> 4) : static_cast<uint8_t>(packed & 0x0F);
    }

    void set_4bpp_pixel(data_t& data, size_t baseOffset, size_t rowStride, size_t x, size_t y, uint8_t value)
    {
        uint8_t& packed = data[baseOffset + y * rowStride + x / 2];
        value &= 0x0F;
        if (x % 2 == 0)
        {
            packed = static_cast<uint8_t>((packed & 0x0F) | (value << 4));
        }
        else
        {
            packed = static_cast<uint8_t>((packed & 0xF0) | value);
        }
    }
}

data_t legacy_picture_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields)
{
    constexpr size_t ceWidth = TypeHandlers::TH_Picture::colorPictureWidth;
    constexpr size_t ceHeight = TypeHandlers::TH_Picture::colorPictureHeight;
    constexpr size_t ceStride = ceWidth / 2;
    constexpr size_t evoWidth = 320;
    constexpr size_t evoHeight = 209;
    constexpr size_t evoStride = evoWidth / 2;
    if (legacyData.size() != TypeHandlers::TH_Picture::colorPictureDataByteCount)
    {
        throw std::invalid_argument("Invalid legacy Picture payload");
    }
    const uint16_t storedLength = static_cast<uint16_t>(legacyData[0] | (legacyData[1] << 8));
    if (storedLength != legacyData.size() - TypeHandlers::TH_Picture::minimumDataByteCount)
    {
        throw std::invalid_argument("Invalid legacy Picture payload length");
    }

    data_t evo(1 + evoStride * evoHeight, 0x00);
    evo[0] = 0x0F;
    const size_t xOffset = (evoWidth - ceWidth) / 2;
    const size_t yOffset = (evoHeight - ceHeight) / 2;
    for (size_t y = 0; y < ceHeight; y++)
    {
        for (size_t x = 0; x < ceWidth; x++)
        {
            const uint8_t value = get_4bpp_pixel(legacyData, TypeHandlers::TH_Picture::minimumDataByteCount, ceStride, x, y);
            set_4bpp_pixel(evo, 1, evoStride, x + xOffset, y + yOffset, value);
        }
    }

    fields["version"] = 1;
    fields["size"] = evo.size();
    return evo;
}

data_t evo_picture_to_legacy(const data_t& evoData)
{
    constexpr size_t ceWidth = TypeHandlers::TH_Picture::colorPictureWidth;
    constexpr size_t ceHeight = TypeHandlers::TH_Picture::colorPictureHeight;
    constexpr size_t ceStride = ceWidth / 2;
    constexpr size_t evoWidth = 320;
    constexpr size_t evoHeight = 209;
    constexpr size_t evoStride = evoWidth / 2;
    if (evoData.size() != 1 + evoStride * evoHeight || evoData[0] != 0x0F)
    {
        throw std::invalid_argument("Invalid Evo Picture payload");
    }

    data_t legacy(TypeHandlers::TH_Picture::colorPictureDataByteCount, 0x00);
    const uint16_t storedLength = static_cast<uint16_t>(legacy.size() - TypeHandlers::TH_Picture::minimumDataByteCount);
    legacy[0] = static_cast<uint8_t>(storedLength & 0xFF);
    legacy[1] = static_cast<uint8_t>((storedLength >> 8) & 0xFF);
    const size_t xOffset = (evoWidth - ceWidth) / 2;
    const size_t yOffset = (evoHeight - ceHeight) / 2;
    for (size_t y = 0; y < ceHeight; y++)
    {
        for (size_t x = 0; x < ceWidth; x++)
        {
            const uint8_t value = get_4bpp_pixel(evoData, 1, evoStride, x + xOffset, y + yOffset);
            set_4bpp_pixel(legacy, TypeHandlers::TH_Picture::minimumDataByteCount, ceStride, x, y, value);
        }
    }

    return legacy;
}

data_t legacy_image_to_evo(const data_t& legacyData, std::map<std::string, uint64_t>& fields)
{
    constexpr size_t ceWidth = TypeHandlers::TH_Picture::imageWidth;
    constexpr size_t ceHeight = TypeHandlers::TH_Picture::imageHeight;
    constexpr size_t evoWidth = 160;
    constexpr size_t evoHeight = 105;
    if (legacyData.size() != TypeHandlers::TH_Picture::imageDataByteCount || legacyData[2] != TypeHandlers::TH_Picture::imageMagic)
    {
        throw std::invalid_argument("Invalid legacy Image payload");
    }

    data_t evo(1 + evoWidth * evoHeight * 2, 0xFF);
    evo[0] = 0x16;
    const size_t xOffset = (evoWidth - ceWidth) / 2;
    const size_t yOffset = (evoHeight - ceHeight) / 2;
    for (size_t y = 0; y < ceHeight; y++)
    {
        const size_t ceStoredRow = ceHeight - 1 - y;
        const size_t ceRowStart = TypeHandlers::TH_Picture::minimumDataByteCount + 1 + ceStoredRow * (ceWidth * 2 + TypeHandlers::TH_Picture::minimumDataByteCount);
        const size_t evoStoredRow = evoHeight - 1 - (y + yOffset);
        const size_t evoRowStart = 1 + evoStoredRow * evoWidth * 2 + xOffset * 2;
        for (size_t x = 0; x < ceWidth; x++)
        {
            evo[evoRowStart + x * 2] = legacyData[ceRowStart + x * 2];
            evo[evoRowStart + x * 2 + 1] = legacyData[ceRowStart + x * 2 + 1];
        }
    }

    fields["version"] = 1;
    fields["size"] = evo.size();
    return evo;
}

data_t evo_image_to_legacy(const data_t& evoData)
{
    constexpr size_t ceWidth = TypeHandlers::TH_Picture::imageWidth;
    constexpr size_t ceHeight = TypeHandlers::TH_Picture::imageHeight;
    constexpr size_t evoWidth = 160;
    constexpr size_t evoHeight = 105;
    if (evoData.size() != 1 + evoWidth * evoHeight * 2)
    {
        throw std::invalid_argument("Invalid Evo Image payload");
    }

    data_t legacy(TypeHandlers::TH_Picture::imageDataByteCount, 0);
    const uint16_t storedLength = static_cast<uint16_t>(legacy.size() - TypeHandlers::TH_Picture::minimumDataByteCount);
    legacy[0] = static_cast<uint8_t>(storedLength & 0xFF);
    legacy[1] = static_cast<uint8_t>((storedLength >> 8) & 0xFF);
    legacy[2] = TypeHandlers::TH_Picture::imageMagic;
    const size_t xOffset = (evoWidth - ceWidth) / 2;
    const size_t yOffset = (evoHeight - ceHeight) / 2;
    for (size_t y = 0; y < ceHeight; y++)
    {
        const size_t evoStoredRow = evoHeight - 1 - (y + yOffset);
        const size_t evoRowStart = 1 + evoStoredRow * evoWidth * 2 + xOffset * 2;
        const size_t ceStoredRow = ceHeight - 1 - y;
        const size_t ceRowStart = TypeHandlers::TH_Picture::minimumDataByteCount + 1 + ceStoredRow * (ceWidth * 2 + TypeHandlers::TH_Picture::minimumDataByteCount);
        for (size_t x = 0; x < ceWidth; x++)
        {
            legacy[ceRowStart + x * 2] = evoData[evoRowStart + x * 2];
            legacy[ceRowStart + x * 2 + 1] = evoData[evoRowStart + x * 2 + 1];
        }
    }
    return legacy;
}

bool is_legacy_numeric_entry(const TIVarFile::var_entry_t& entry)
{
    const std::string typeName = entry._type.getName();
    return typeName == "Real"
        || typeName == "Complex"
        || typeName == "RealFraction"
        || typeName.rfind("Exact", 0) == 0;
}

void set_entry_type(TIVarFile::var_entry_t& entry, const TIVarType& type)
{
    entry._type = type;
    entry.typeID = static_cast<uint8_t>(type.getId());
}

void set_numeric_entry_type_from_payload(TIVarFile::var_entry_t& entry)
{
    if (entry.data.size() == TypeHandlers::TH_GenericComplex::dataByteCount)
    {
        set_entry_type(entry, TIVarType{"Complex"});
        return;
    }

    uint8_t typeID = entry.data.empty() ? 0 : static_cast<uint8_t>(entry.data[0] & 0x3F);
    if (!TIVarTypes::isValidID(typeID))
    {
        typeID = 0;
    }
    set_entry_type(entry, TIVarType{typeID});
}

}
