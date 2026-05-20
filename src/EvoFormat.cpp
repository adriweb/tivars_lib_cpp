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
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TIVarTypes.h"
#include "TypeHandlers/TypeHandlers.h"
#include "json.hpp"
#include "tivarslib_utils.h"

using json = nlohmann::ordered_json;

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

        bool is_displayable_ucs2_scalar(uint16_t codepoint)
        {
            static constexpr std::pair<uint16_t, uint16_t> acceptedRanges[] = {
                {0x0020, 0x007E}, {0x00A0, 0x00FF}, {0x0177, 0x0177}, {0x0394, 0x0394},
                {0x03A3, 0x03A3}, {0x03A9, 0x03A9}, {0x03B1, 0x03B5}, {0x03B8, 0x03B8},
                {0x03BB, 0x03BC}, {0x03C0, 0x03C1}, {0x03C3, 0x03C4}, {0x03C6, 0x03C7},
                {0x2010, 0x2010}, {0x2026, 0x2026}, {0x2070, 0x2070}, {0x2074, 0x2079},
                {0x2080, 0x2089}, {0x2122, 0x2122}, {0x2190, 0x2193}, {0x221A, 0x221A},
                {0x2220, 0x2220}, {0x222B, 0x222B}, {0x2260, 0x2260}, {0x2264, 0x2265},
                {0x238C, 0x238C}, {0x25A0, 0x25A0}, {0x25AB, 0x25AB}, {0x25B2, 0x25B2},
                {0x25B6, 0x25B6}, {0x25B8, 0x25B8}, {0x25BC, 0x25BC}, {0x25C0, 0x25C0},
                {0x25C2, 0x25C2}, {0xF000, 0xF032}, {0xF038, 0xF03A}, {0xF041, 0xF04D},
                {0xF04F, 0xF058}, {0xF05B, 0xF061},
            };

            return std::ranges::any_of(acceptedRanges, [codepoint](const auto& range)
            {
                const auto& [first, last] = range;
                return codepoint >= first && codepoint <= last;
            });
        }

        bool utf8_to_single_codepoint(const std::string& text, uint16_t& codepoint)
        {
            if (text.empty())
            {
                return false;
            }

            const uint8_t first = static_cast<uint8_t>(text[0]);
            size_t length = 0;
            uint32_t value = 0;
            if ((first & 0x80) == 0)
            {
                length = 1;
                value = first;
            }
            else if ((first & 0xE0) == 0xC0)
            {
                length = 2;
                value = first & 0x1F;
            }
            else if ((first & 0xF0) == 0xE0)
            {
                length = 3;
                value = first & 0x0F;
            }
            else
            {
                return false;
            }

            if (text.size() != length)
            {
                return false;
            }

            for (size_t i = 1; i < length; i++)
            {
                const uint8_t byte = static_cast<uint8_t>(text[i]);
                if ((byte & 0xC0) != 0x80)
                {
                    return false;
                }
                value = (value << 6) | (byte & 0x3F);
            }

            if (value > 0xFFFF)
            {
                return false;
            }

            codepoint = static_cast<uint16_t>(value);
            return true;
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

static uint64_t read_cbor_uint_arg(const data_t& data, size_t& offset, uint8_t additional)
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

static void append_cbor_uint(data_t& out, uint64_t value)
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

static void append_evo_name_word(data_t& out, uint16_t word)
{
    out.push_back(static_cast<uint8_t>(word & 0xFF));
    out.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
}

static std::string char_from_evo_name_word(uint16_t word)
{
    if (word >= 0xE800 && word <= 0xE819) return std::string(1, static_cast<char>('A' + (word - 0xE800)));
    if (word == 0xE81A) return "θ";
    if (word >= 0xE401 && word <= 0xE40A) return std::string(1, static_cast<char>('0' + (word - 0xE401)));
    if ((word >= 'a' && word <= 'z') || (word >= 'A' && word <= 'Z') || (word >= '0' && word <= '9')) return std::string(1, static_cast<char>(word));
    if (word == 0x005F || word == 0xE400) return "_";
    return "_";
}

static std::vector<uint16_t> evo_name_words(const data_t& nameBytes)
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

std::string decode_evo_name(EvoTypeID evoTypeID, const data_t& nameBytes)
{
    const std::vector<uint16_t> words = evo_name_words(nameBytes);
    if (words.empty())
    {
        return "";
    }

    const uint16_t first = words[0];
    if (evoTypeID == EvoTypeID::List)
    {
        if (first >= 0xE830 && first <= 0xE835) return "L" + std::to_string(first - 0xE830 + 1);
        std::string out;
        const size_t start = first == 0xE836 ? 1 : 0;
        for (size_t i = start; i < words.size(); i++) out += char_from_evo_name_word(words[i]);
        return out;
    }
    if (evoTypeID == EvoTypeID::GraphDataBase)
    {
        if (first == 0xE899) return "GDB0";
        if (first >= 0xE890 && first <= 0xE898) return "GDB" + std::to_string(first - 0xE890 + 1);
    }
    if (evoTypeID == EvoTypeID::Picture)
    {
        if (first == 0xE889) return "Pic0";
        if (first >= 0xE880 && first <= 0xE888) return "Pic" + std::to_string(first - 0xE880 + 1);
    }
    if (evoTypeID == EvoTypeID::Image)
    {
        if (first == 0xE8B9) return "Image0";
        if (first >= 0xE8B0 && first <= 0xE8B8) return "Image" + std::to_string(first - 0xE8B0 + 1);
    }
    if (evoTypeID == EvoTypeID::Matrix && first >= 0xE820 && first <= 0xE829)
    {
        return std::string(1, static_cast<char>('A' + (first - 0xE820)));
    }
    if (evoTypeID == EvoTypeID::Equation)
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
    if (evoTypeID == EvoTypeID::String)
    {
        if (first == 0xE8A9) return "Str0";
        if (first >= 0xE8A0 && first <= 0xE8A8) return "Str" + std::to_string(first - 0xE8A0 + 1);
    }
    if (evoTypeID == EvoTypeID::WindowSettings && first == 0xE8BA) return "Window";
    if (evoTypeID == EvoTypeID::RecallWindow && first == 0xE8BB) return "RclWindw";
    if (evoTypeID == EvoTypeID::TableRange && first == 0xE8BC) return "TblSet";

    std::string out;
    for (const uint16_t word : words)
    {
        out += char_from_evo_name_word(word);
    }
    return out;
}

static data_t encode_evo_custom_name(std::string name, bool allowLowerAscii)
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

data_t encode_evo_name(EvoTypeID evoTypeID, std::string displayName)
{
    data_t out;
    const std::string upperName = strtoupper(normalize_theta_chars(displayName));
    const auto append_terminated = [&]()
    {
        append_evo_name_word(out, 0);
        return out;
    };

    if (evoTypeID == EvoTypeID::List)
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
    if (evoTypeID == EvoTypeID::GraphDataBase && upperName.rfind("GDB", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE899 : static_cast<uint16_t>(0xE890 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::Picture && upperName.rfind("PIC", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE889 : static_cast<uint16_t>(0xE880 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::Image && upperName.rfind("IMAGE", 0) == 0 && upperName.size() == 6 && std::isdigit(static_cast<unsigned char>(upperName[5])))
    {
        const int idx = upperName[5] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE8B9 : static_cast<uint16_t>(0xE8B0 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::Matrix)
    {
        std::string mat = upperName;
        if (mat.size() == 3 && mat.front() == '[' && mat.back() == ']') mat = mat.substr(1, 1);
        if (mat.size() == 1 && mat[0] >= 'A' && mat[0] <= 'J')
        {
            append_evo_name_word(out, static_cast<uint16_t>(0xE820 + (mat[0] - 'A')));
            return append_terminated();
        }
    }
    if (evoTypeID == EvoTypeID::Equation)
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
    if (evoTypeID == EvoTypeID::String && upperName.rfind("STR", 0) == 0 && upperName.size() == 4 && std::isdigit(static_cast<unsigned char>(upperName[3])))
    {
        const int idx = upperName[3] - '0';
        append_evo_name_word(out, idx == 0 ? 0xE8A9 : static_cast<uint16_t>(0xE8A0 + idx - 1));
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::WindowSettings)
    {
        append_evo_name_word(out, 0xE8BA);
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::RecallWindow)
    {
        append_evo_name_word(out, 0xE8BB);
        return append_terminated();
    }
    if (evoTypeID == EvoTypeID::TableRange)
    {
        append_evo_name_word(out, 0xE8BC);
        return append_terminated();
    }

    return encode_evo_custom_name(displayName, evoTypeID == EvoTypeID::AppVar || evoTypeID == EvoTypeID::PythonScript);
}

TIVarType type_from_evo_type(EvoTypeID evoTypeID)
{
    return TIVarType{std::string{ti_type_name_from_evo_type(evoTypeID)}};
}

EvoTypeID evo_type_from_type(const TIVarType& type)
{
    return evo_type_id_from_ti_type_name(type.getName());
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

static const char* evo_token_name(uint16_t token)
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

static bool direct_legacy_token_for_evo(uint16_t evoToken, uint16_t& legacyToken);
static bool direct_legacy_payload_for_evo(uint16_t evoToken, data_t& payload);
static bool direct_evo_token_for_legacy(uint16_t legacyToken, uint16_t& evoToken);
static void append_evo_token(data_t& out, uint16_t evoToken);
static bool legacy_token_to_evo_ucs2(uint16_t legacyToken, uint16_t& evoToken);

static const std::unordered_map<uint16_t, std::string>& evo_private_display_aliases()
{
    static const std::unordered_map<uint16_t, std::string> aliases = {
        {0xF000, "ᴇ"}, {0xF001, "E"}, {0xF002, "e"}, {0xF003, "𝙵"},
        {0xF004, "𝑖"}, {0xF005, "ʟ"}, {0xF006, "𝗡"}, {0xF007, "𝑛"},
        {0xF008, "p̂"}, {0xF009, "ʳ"}, {0xF00A, "ᵀ"}, {0xF00B, "ᴛ"},
        {0xF00C, "ˣ"}, {0xF00D, "x̄"}, {0xF00E, "ȳ"}, {0xF00F, "⁺"},
        {0xF010, "⁻"}, {0xF011, "⁻¹"}, {0xF012, "₁₀"},{0xF013, "²"},
        {0xF014, "³"}, {0xF015, "⧸"}, {0xF016, "⟋"}, {0xF017, "␣"},
        {0xF018, "⁄"}, {0xF019, "`"},
        {0xF01A, "⸩"}, {0xF01B, "🡅"}, {0xF01C, "🡇"}, {0xF01D, "🠺"},
        {0xF01E, "↑"}, {0xF01F, "↓"},
        {0xF020, "⌸"}, {0xF021, "▮"}, {0xF022, "↑"},
        {0xF023, "A"}, {0xF024, "a"}, {0xF025, "_"}, {0xF026, "↑͟"},
        {0xF027, "A͟"}, {0xF028, "a͟"}, {0xF029, "░"}, {0xF02A, "⬚"},
        {0xF02B, "╲"}, {0xF02C, "＼"}, {0xF02D, "◥"}, {0xF02E, "◣"},
        {0xF02F, "⌕"}, {0xF030, "⁰"}, {0xF031, "⧵"}, {0xF032, "⧹"},
        {0xF038, "⬩"}, {0xF039, "⎵"}, {0xF03A, "🔒"}, {0xF041, "⋅"},
        {0xF042, "₀"}, {0xF043, "₁"}, {0xF044, "₂"}, {0xF045, "₃"},
        {0xF046, "₄"}, {0xF047, "₅"}, {0xF048, "₆"}, {0xF049, "₇"},
        {0xF04A, "₈"}, {0xF04B, "₉"}, {0xF04C, "□"},
        {0xF04F, "⬌"}, {0xF050, "▯"}, {0xF051, "⸋"}, {0xF052, "𝅆"},
        {0xF053, "ᵍ"}, {0xF054, "▫"}, {0xF055, "ᵃ"}, {0xF056, "🔒"},
        {0xF057, "◣̏"}, {0xF058, "◥̤"},
        {0xF05B, "Β"}, {0xF05C, "Ε"}, {0xF05D, "Ｆ"},
        {0xF05E, "‹"}, {0xF05F, "›"}, {0xF060, "≤"}, {0xF061, "≥"}
    };
    return aliases;
}

static bool source_text_tokenizes_without_private_alias(const std::string& text)
{
    const auto scanned = TypeHandlers::TH_Tokenized::scanSourceTokens(text);
    if (scanned.size() != 1)
    {
        return false;
    }

    const auto& [scannedText, legacyToken, matched] = scanned[0];
    if (!matched || scannedText != text)
    {
        return false;
    }

    uint16_t evoToken = 0;
    return direct_evo_token_for_legacy(legacyToken, evoToken);
}

static const std::vector<std::pair<std::string, uint16_t>>& evo_private_source_aliases()
{
    static const std::vector<std::pair<std::string, uint16_t>> aliases = [] {
        std::vector<std::pair<std::string, uint16_t>> result;
        for (const auto& [token, text] : evo_private_display_aliases())
        {
            if (source_text_tokenizes_without_private_alias(text))
            {
                continue;
            }
            result.emplace_back(text, token);
        }
        std::ranges::sort(result, [](const auto& lhs, const auto& rhs) {
            if (lhs.first.size() != rhs.first.size())
            {
                return lhs.first.size() > rhs.first.size();
            }
            return lhs.second < rhs.second;
        });
        return result;
    }();
    return aliases;
}

static std::string normalize_evo_private_source_aliases(const std::string& source)
{
    std::string normalized;
    normalized.reserve(source.size());

    for (size_t pos = 0; pos < source.size();)
    {
        bool matched = false;
        for (const auto& [text, token] : evo_private_source_aliases())
        {
            if (source.compare(pos, text.size(), text) != 0)
            {
                continue;
            }

            normalized += "\\u" + dechex(static_cast<uint8_t>(token >> 8)) + dechex(static_cast<uint8_t>(token & 0xFF));
            pos += text.size();
            matched = true;
            break;
        }

        if (!matched)
        {
            normalized += source[pos++];
        }
    }

    return normalized;
}

static bool evo_token_starts_evaluated_string(uint16_t evoToken)
{
    return evoToken == 0xE6C7   // TOK_SEND_MBL
        || evoToken == 0xE470;  // TOK_EXPR
}

static std::string evo_token_to_string(uint16_t token)
{
    if (token == 0x0000) return "";
    if (const auto it = evo_private_display_aliases().find(token); it != evo_private_display_aliases().end()) return it->second;
    if (is_displayable_ucs2_scalar(token)) return utf8_from_codepoint(token);
    if (token >= 0xE800 && token <= 0xE819) return std::string(1, static_cast<char>('A' + (token - 0xE800)));
    if (token == 0xE81A) return "θ";
    if (token >= 0xE401 && token <= 0xE40A) return std::string(1, static_cast<char>('0' + (token - 0xE401)));
    if (token >= 0xE830 && token <= 0xE835) return "L" + std::to_string(token - 0xE830 + 1);
    if (token >= 0xE820 && token <= 0xE829) return "[" + std::string(1, static_cast<char>('A' + (token - 0xE820))) + "]";
    if (token >= 0xE840 && token <= 0xE849) return "Y" + std::to_string(token == 0xE849 ? 0 : token - 0xE840 + 1);
    if (token >= 0xE8A0 && token <= 0xE8A9) return "Str" + std::to_string(token == 0xE8A9 ? 0 : token - 0xE8A0 + 1);
    if (token == 0xE41A) return "'";
    if (token == 0xE424) return "ᵍ";
    if (token == 0xE589) return "Grad";
    if (token == 0xE9D6) return "►ʳ";
    if (token == 0xE9D7) return "►ᵍ";
    if (token == 0xE9D8) return "►º";
    if (token >= 0xE850 && token <= 0xE85B)
    {
        const uint16_t idx = static_cast<uint16_t>((token - 0xE850) / 2 + 1);
        return std::string(1, ((token - 0xE850) % 2 == 0) ? 'X' : 'Y') + std::to_string(idx) + "T";
    }
    if (token >= 0xE860 && token <= 0xE865) return "r" + std::to_string(token - 0xE860 + 1);
    if (token >= 0xE870 && token <= 0xE872) return std::string(1, static_cast<char>('u' + (token - 0xE870)));

    uint16_t legacyToken = 0;
    if (direct_legacy_token_for_evo(token, legacyToken))
    {
        return TypeHandlers::TH_Tokenized::oneTokenBytesToString(legacyToken);
    }

    const char* name = evo_token_name(token);
    if (name != nullptr)
    {
        return name;
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

data_t tokenize_evo_token_words(const std::string& source, const options_t& options)
{
    const bool deindent = options.contains("deindent") && options.at("deindent") == 1;
    const bool detectStrings = !options.contains("detect_strings") || options.at("detect_strings") != 0;

    std::string sourceText = source;
    const std::string trimmed = trim(sourceText);
    if (!trimmed.empty() && trimmed.front() == '{')
    {
        try
        {
            const json j = json::parse(trimmed);
            if (j.contains("rawDataHex"))
            {
                return hex_string_to_bytes(j.at("rawDataHex").get<std::string>(), "rawDataHex");
            }
            if (j.contains("code"))
            {
                sourceText = j.at("code").get<std::string>();
            }
        }
        catch (const json::exception&)
        {
            // Ignore non-JSON input and fall back to regular Evo tokenized parsing.
        }
    }

    std::string normalizedSource;
    if (deindent)
    {
        std::istringstream lines{sourceText};
        std::string line;
        while (std::getline(lines, line))
        {
            normalizedSource += ltrim(line) + "\n";
        }
        if (!normalizedSource.empty())
        {
            normalizedSource.pop_back();
        }
    }
    else
    {
        normalizedSource = sourceText;
    }
    normalizedSource = normalize_evo_private_source_aliases(normalizedSource);

    static constexpr uint16_t legacyStore = 0x04;
    static constexpr uint16_t legacyQuote = 0x2A;
    static constexpr uint16_t legacyNewLine = 0x3F;

    data_t evo;
    evo.reserve((normalizedSource.size() + 1) * 2);
    bool isWithinString = false;
    bool inEvaluatedString = false;
    uint16_t lastEvoToken = 0;

    for (const auto& [text, token, matched] : TypeHandlers::TH_Tokenized::scanSourceTokens(normalizedSource, detectStrings))
    {
        if (matched)
        {
            uint16_t evoToken = 0;
            if (token == legacyStore || token == legacyNewLine)
            {
                isWithinString = false;
                inEvaluatedString = false;
            }

            if (text.rfind("\\u", 0) == 0 && is_displayable_ucs2_scalar(token) && !direct_evo_token_for_legacy(token, evoToken))
            {
                evoToken = token;
            }
            else if (!direct_evo_token_for_legacy(token, evoToken)
                && !(isWithinString && !inEvaluatedString && token != legacyQuote && legacy_token_to_evo_ucs2(token, evoToken)))
            {
                std::cerr << "[Warning] Cannot convert 84+CE token "
                          << TypeHandlers::TH_Tokenized::oneTokenBytesToString(token)
                          << " to an Evo token; replacing with ?" << std::endl;
                evoToken = 0xE41B;
            }

            append_evo_token(evo, evoToken);
            if (token == legacyQuote)
            {
                isWithinString = !isWithinString;
                inEvaluatedString = isWithinString && evo_token_starts_evaluated_string(lastEvoToken);
            }
            lastEvoToken = evoToken;
            continue;
        }

        // The shared scanner leaves unknown-but-valid UTF-8 source text here;
        // Evo can store displayable UCS-2 characters directly as token words.
        uint16_t codepoint = 0;
        if (utf8_to_single_codepoint(text, codepoint) && is_displayable_ucs2_scalar(codepoint))
        {
            append_evo_token(evo, codepoint);
        }
        else if (!text.empty())
        {
            std::cerr << "[Warning] Cannot encode source text \"" << text
                      << "\" as an Evo token; skipping it." << std::endl;
        }
    }

    append_evo_token(evo, 0);
    return evo;
}

bool is_evo_tokenized_entry(const TIVarFile::var_entry_t& entry)
{
    const EvoTypeID evoTypeID = entry.evoTypeID;
    return evoTypeID == EvoTypeID::Program || evoTypeID == EvoTypeID::Equation || evoTypeID == EvoTypeID::String;
}

static void append_legacy_token(data_t& out, uint16_t legacyToken)
{
    if (legacyToken > 0xFF)
    {
        out.push_back(static_cast<uint8_t>((legacyToken >> 8) & 0xFF));
    }
    out.push_back(static_cast<uint8_t>(legacyToken & 0xFF));
}

static void append_evo_token(data_t& out, uint16_t evoToken)
{
    out.push_back(static_cast<uint8_t>(evoToken & 0xFF));
    out.push_back(static_cast<uint8_t>((evoToken >> 8) & 0xFF));
}

static bool legacy_payload_for_evo_ucs2(uint16_t evoToken, data_t& payload)
{
    if (!is_displayable_ucs2_scalar(evoToken))
    {
        return false;
    }

    try
    {
        const data_t legacy = TypeHandlers::TH_Tokenized::makeDataFromString(utf8_from_codepoint(evoToken));
        if (legacy.size() < 3 || legacy.size() != static_cast<size_t>(2 + legacy[0] + (legacy[1] << 8)))
        {
            return false;
        }
        payload.assign(legacy.begin() + 2, legacy.end());
        return !payload.empty();
    }
    catch (...)
    {
        return false;
    }
}

static bool direct_legacy_payload_for_evo(uint16_t evoToken, data_t& payload)
{
    payload.clear();

    // ►{angle} conv token
    if (evoToken == 0xE9D6 || evoToken == 0xE9D7 || evoToken == 0xE9D8)
    {
        append_legacy_token(payload, 0xBBEC);
        append_legacy_token(payload, evoToken == 0xE9D6 ? 0x0A : evoToken == 0xE9D7 ? 0xAF : 0x0B);
        return true;
    }

    uint16_t legacyToken = 0;
    if (!direct_legacy_token_for_evo(evoToken, legacyToken))
    {
        return false;
    }

    append_legacy_token(payload, legacyToken);
    return true;
}

static bool legacy_token_to_evo_ucs2(uint16_t legacyToken, uint16_t& evoToken)
{
    const std::string text = TypeHandlers::TH_Tokenized::oneTokenBytesToString(legacyToken);
    uint16_t codepoint = 0;
    if (!utf8_to_single_codepoint(text, codepoint) || !is_displayable_ucs2_scalar(codepoint))
    {
        return false;
    }

    evoToken = codepoint;
    return true;
}

static bool direct_legacy_token_for_evo(uint16_t evoToken, uint16_t& legacyToken)
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
    if (evoToken >= 0xE8B0 && evoToken <= 0xE8B9)
    {
        legacyToken = static_cast<uint16_t>(0xEF50 + (evoToken - 0xE8B0));
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
    if (evoToken >= 0xE60E && evoToken <= 0xE610)
    {
        legacyToken = static_cast<uint16_t>(0xEF82 + (evoToken - 0xE60E));
        return true;
    }
    if (evoToken >= 0xE613 && evoToken <= 0xE619)
    {
        legacyToken = static_cast<uint16_t>(0xEF87 + (evoToken - 0xE613));
        return true;
    }
    if (evoToken >= 0xE61A && evoToken <= 0xE61C)
    {
        legacyToken = static_cast<uint16_t>(0xEF8F + (evoToken - 0xE61A));
        return true;
    }
    if (evoToken >= 0xE640 && evoToken <= 0xE642)
    {
        legacyToken = static_cast<uint16_t>(0xEF92 + (evoToken - 0xE640));
        return true;
    }
    if (evoToken >= 0xE68E && evoToken <= 0xE694)
    {
        legacyToken = static_cast<uint16_t>(0xEF17 + (evoToken - 0xE68E));
        return true;
    }
    static const std::unordered_map<uint16_t, uint16_t> direct = {
        {0xE000, 0xB9}, {0xE001, 0xBA}, {0xE105, 0xEF9E}, {0xE400, 0xBBD9}, {0xE40B, 0x3A}, {0xE40C, 0x3B},
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
        {0xE44C, 0x22}, {0xE452, 0xEF33}, {0xE453, 0xEFA6},
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
        {0x00D7, 0xBBF0}, {0x222B, 0xBBF1},
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
        {0xE6AE, 0xEF79},
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
        {0xE93B, 0x6203}, {0xE93C, 0x622B}, {0xE93D, 0x622E}, {0xE93E, 0xBBA6},
        {0xE941, 0x622D}, {0xE942, 0x6230}, {0xE943, 0x6206}, {0xE944, 0x622C},
        {0xE945, 0x622F}, {0xE946, 0xBBCB}, {0xE95C, 0x6227},
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
        {0xE99C, 0x6312}, {0xE99D, 0x6313}, {0xE99E, 0x6300},
        {0xE9A0, 0x6314}, {0xE9A1, 0x6315}, {0xE9A2, 0x6301},
        {0xE425, 0x0001},
        {0xE426, 0x0002},
        {0xE427, 0x0003},
        {0xE42F, 0x00B1},
        {0xE430, 0x00B2},
        {0xE431, 0x0012},
        {0xE432, 0x00B9},
        {0xE433, 0x00BA},
        {0xE434, 0x00AB},
        {0xE435, 0xEF35},
        {0xE436, 0xBB0A},
        {0xE437, 0xBB0B},
        {0xE438, 0xBB1F},
        {0xE43A, 0x00BD},
        {0xE43B, 0x00BE},
        {0xE43C, 0x00BF},
        {0xE43D, 0x00C0},
        {0xE43F, 0x00C2},
        {0xE440, 0x00C3},
        {0xE441, 0x00C4},
        {0xE442, 0x00C5},
        {0xE443, 0x00C6},
        {0xE444, 0x00C7},
        {0xE445, 0x00C8},
        {0xE446, 0x00C9},
        {0xE447, 0x00CA},
        {0xE448, 0x00CB},
        {0xE449, 0x00CC},
        {0xE44A, 0x00CD},
        {0xE44B, 0xEF34},
        {0xE44E, 0x0024},
        {0xE44F, 0x0025},
        {0xE450, 0x0028},
        {0xE451, 0x0027},
        {0xE454, 0x0019},
        {0xE455, 0x001A},
        {0xE456, 0x0021},
        {0xE457, 0x001F},
        {0xE458, 0x00B6},
        {0xE459, 0x00B7},
        {0xE45A, 0x001B},
        {0xE45B, 0x001C},
        {0xE45C, 0x001D},
        {0xE45D, 0x001E},
        {0xE45E, 0x0015},
        {0xE45F, 0x0016},
        {0xE460, 0x0017},
        {0xE461, 0x0018},
        {0xE462, 0x00B3},
        {0xE464, 0xBB2D},
        {0xE465, 0xBB2E},
        {0xE466, 0x00B5},
        {0xE467, 0x00E2},
        {0xE468, 0x00B4},
        {0xE469, 0x0020},
        {0xE46A, 0x0014},
        {0xE46B, 0xBB25},
        {0xE46C, 0xBB26},
        {0xE46D, 0xBB27},
        {0xE46E, 0xBB28},
        {0xE46F, 0xBB29},
        {0xE470, 0xBB2A},
        {0xE471, 0xBB2B},
        {0xE472, 0xBB2C},
        {0xE473, 0xBB2F},
        {0xE474, 0xBB30},
        {0xE475, 0xBB08},
        {0xE476, 0xBB09},
        {0xE477, 0xBB0C},
        {0xE478, 0xBB0F},
        {0xE479, 0xEF32},
        {0xE47D, 0x0094},
        {0xE47E, 0x0095},
        {0xE485, 0x00B8},
        {0xE486, 0xBB20},
        {0xE487, 0xBB21},
        {0xE488, 0xBB22},
        {0xE489, 0xBB23},
        {0xE48A, 0xBB24},
        {0xE48B, 0xBB00},
        {0xE48C, 0xBB01},
        {0xE48D, 0xBB02},
        {0xE48E, 0xBB03},
        {0xE48F, 0xBB04},
        {0xE490, 0xBB05},
        {0xE491, 0xBB06},
        {0xE492, 0xBB07},
        {0xE493, 0xBB4B},
        {0xE494, 0xBB4C},
        {0xE495, 0xBB0D},
        {0xE496, 0xBB0E},
        {0xE497, 0xBB10},
        {0xE498, 0xBB11},
        {0xE499, 0xBB12},
        {0xE49A, 0xBB13},
        {0xE49B, 0xBB14},
        {0xE49C, 0xBB15},
        {0xE49D, 0xBB16},
        {0xE49E, 0xEF95},
        {0xE49F, 0xBB17},
        {0xE4A0, 0xBB18},
        {0xE4A1, 0xBB19},
        {0xE4A2, 0xBB1A},
        {0xE4A3, 0xBB1B},
        {0xE4A4, 0xBB1C},
        {0xE4A5, 0xBB1D},
        {0xE4A6, 0xBB1E},
        {0xE4A7, 0xBB3B},
        {0xE4A8, 0xBB3C},
        {0xE4A9, 0xBB3D},
        {0xE4AA, 0xBB46},
        {0xE4AB, 0xBB3E},
        {0xE4AC, 0xBB3F},
        {0xE4AD, 0xBB40},
        {0xE4AE, 0xBB47},
        {0xE4AF, 0xBB41},
        {0xE4B0, 0xBB48},
        {0xE4B1, 0xBB49},
        {0xE4B2, 0xBB42},
        {0xE4B3, 0xBB43},
        {0xE4B4, 0xBB44},
        {0xE4C0, 0x00CE},
        {0xE4C1, 0x00CF},
        {0xE4C2, 0x00D0},
        {0xE4C4, 0x00D2},
        {0xE4C5, 0x00D3},
        {0xE4C7, 0x00D5},
        {0xE4C8, 0x00D6},
        {0xE4C9, 0x00D7},
        {0xE4CA, 0x00D8},
        {0xE4CB, 0xEF96},
        {0xE4CC, 0x00D9},
        {0xE4CD, 0x00DA},
        {0xE4CE, 0x00DB},
        {0xE4CF, 0x00E6},
        {0xE4D0, 0x00DC},
        {0xE4D1, 0x00AD},
        {0xE4D2, 0x00DD},
        {0xE4D3, 0x00DE},
        {0xE4D4, 0x00DF},
        {0xE4D5, 0x00E5},
        {0xE4D6, 0x00E0},
        {0xE4D7, 0x00E1},
        {0xE4D8, 0x0091},
        {0xE4D9, 0x00E3},
        {0xE4DA, 0x00E4},
        {0xE4DB, 0xBB53},
        {0xE4DC, 0xBB39},
        {0xE4DD, 0xBB3A},
        {0xE4DE, 0xBB4A},
        {0xE4DF, 0xBB52},
        {0xE4E0, 0xBB54},
        {0xE4E1, 0xBB55},
        {0xE4E2, 0xBB56},
        {0xE4E3, 0xEF97},
        {0xE4E4, 0xEF98},
        {0xE4E5, 0xBB68},
        {0xE4E6, 0xBB69},
        {0xE4E7, 0xBBCE},
        {0xE4E8, 0xBB58},
        {0xE4E9, 0x00A5},
        {0xE4EB, 0x009C},
        {0xE4EC, 0x00A0},
        {0xE4ED, 0x009F},
        {0xE4EE, 0x009E},
        {0xE4EF, 0x00A3},
        {0xE4F0, 0x00A2},
        {0xE4F1, 0x00A1},
        {0xE4F2, 0x0013},
        {0xE4F3, 0x009D},
        {0xE4F4, 0x00A6},
        {0xE4F6, 0x00A7},
        {0xE4F7, 0xBB50},
        {0xE4F8, 0xBB51},
        {0xE4FA, 0xBB59},
        {0xE4FB, 0xBB5A},
        {0xE4FC, 0xBB5B},
        {0xE4FD, 0x00E9},
        {0xE4FE, 0x00EA},
        {0xE4FF, 0x00EC},
        {0xE500, 0x00ED},
        {0xE501, 0x00EE},
        {0xE502, 0x00F2},
        {0xE503, 0x00F3},
        {0xE504, 0x00FA},
        {0xE505, 0x00FB},
        {0xE506, 0x0005},
        {0xE507, 0x00FC},
        {0xE508, 0x00FD},
        {0xE509, 0x00FE},
        {0xE50A, 0xEF16},
        {0xE582, 0x0066},
        {0xE583, 0x0067},
        {0xE584, 0x0068},
        {0xE585, 0x0069},
        {0xE586, 0x0073},
        {0xE587, 0x0064},
        {0xE588, 0x0065},
        {0xE58A, 0x0076},
        {0xE58B, 0x0077},
        {0xE58C, 0x0078},
        {0xE58D, 0x0079},
        {0xE58E, 0xBB4D},
        {0xE58F, 0xBB4E},
        {0xE590, 0xBB4F},
        {0xE591, 0x0075},
        {0xE592, 0x0074},
        {0xE59A, 0xBB66},
        {0xE59B, 0xBB67},
        {0xE59C, 0x007A},
        {0xE59D, 0x007B},
        {0xE59E, 0x007C},
        {0xE59F, 0x007D},
        {0xE5A0, 0xEF41},
        {0xE5A1, 0xEF42},
        {0xE5A2, 0xEF43},
        {0xE5A3, 0xEF44},
        {0xE5A4, 0xEF45},
        {0xE5A5, 0xEF46},
        {0xE5A6, 0xEF47},
        {0xE5A7, 0xEF48},
        {0xE5A8, 0xEF49},
        {0xE5A9, 0xEF4A},
        {0xE5AA, 0xEF4B},
        {0xE5AB, 0xEF4C},
        {0xE5AC, 0xEF4D},
        {0xE5AD, 0xEF4E},
        {0xE5AE, 0xEF4F},
        {0xE5AF, 0x7E00},
        {0xE5B0, 0x7E01},
        {0xE5B1, 0x7E02},
        {0xE5B2, 0x7E03},
        {0xE5B3, 0x7E04},
        {0xE5B4, 0x7E05},
        {0xE5B5, 0xEF5A},
        {0xE5B6, 0xEF5B},
        {0xE5B7, 0xEF64},
        {0xE5B8, 0xEF65},
        {0xE5B9, 0xEF67},
        {0xE5BA, 0xEF6C},
        {0xE5BB, 0xEF6A},
        {0xE5BC, 0xEF6B},
        {0xE600, 0x7E06},
        {0xE601, 0x7E07},
        {0xE603, 0x7E08},
        {0xE605, 0x7E0A},
        {0xE606, 0x7E0B},
        {0xE607, 0x7E0C},
        {0xE608, 0x7E0D},
        {0xE609, 0x7E0E},
        {0xE60A, 0x7E0F},
        {0xE60B, 0x7E10},
        {0xE60C, 0x7E11},
        {0xE60D, 0x7E12},
        {0xE611, 0x6306},
        {0xE612, 0x6307},
        {0xE643, 0x00FF},
        {0xE644, 0x00F4},
        {0xE645, 0x00F5},
        {0xE646, 0x00F6},
        {0xE647, 0x00F7},
        {0xE648, 0x00F8},
        {0xE649, 0x00F9},
        {0xE64A, 0x002E},
        {0xE64B, 0x002F},
        {0xE64C, 0xBB32},
        {0xE64D, 0xBB33},
        {0xE64E, 0xBB34},
        {0xE64F, 0xEF13},
        {0xE650, 0xEF14},
        {0xE651, 0xEF15},
        {0xE680, 0x0084},
        {0xE681, 0x0086},
        {0xE682, 0x0087},
        {0xE683, 0x0088},
        {0xE684, 0x0089},
        {0xE685, 0x008A},
        {0xE686, 0x008B},
        {0xE687, 0x008C},
        {0xE688, 0x008D},
        {0xE689, 0x008E},
        {0xE68A, 0x008F},
        {0xE68B, 0x0090},
        {0xE68C, 0x0092},
        {0xE68D, 0xBB65},
        {0xE6A0, 0x0096},
        {0xE6A1, 0x0097},
        {0xE6A2, 0x0098},
        {0xE6A3, 0x0099},
        {0xE6A4, 0x009A},
        {0xE6A5, 0x009B},
        {0xE6A6, 0x00A4},
        {0xE6A7, 0x00A8},
        {0xE6A8, 0x00A9},
        {0xE6A9, 0xBB35},
        {0xE6AA, 0xBB36},
        {0xE6AB, 0xBB37},
        {0xE6AC, 0xBB38},
        {0xE6AD, 0xBB45},
        {0xE6B7, 0xEF05},
        {0xE6B8, 0xEF06},
        {0xE6C2, 0xEF2E},
        {0xE6C3, 0xEF2F},
        {0xE6C4, 0xEF30},
        {0xE6C5, 0xEF31},
        {0xE81B, 0x0072},
        {0xE81C, 0x005F},
        {0xF000, 0x3B},
        {0xF001, 0x45},
        {0xF002, 0xBBB4},
        {0xF003, 0xBBAF},
        {0xF004, 0x2C},
        {0xF005, 0xEB},
        {0xF006, 0x632B},
        {0xF007, 0x6221},
        {0xF008, 0xBBAD},
        {0xF009, 0x0A},
        {0xF00A, 0x0E},
        {0xF00B, 0xBBDF},
        {0xF00C, 0xBBDE},
        {0xF00D, 0x6203},
        {0xF00E, 0x620C},
        {0xF010, 0xB0},
        {0xF011, 0x0C},
        {0xF012, 0xBBEA},
        {0xF013, 0x0D},
        {0xF014, 0x0F},
        {0xF018, 0xEF2E},
        {0xF019, 0xBB9B},
        {0xF01B, 0xBBF2},
        {0xF01C, 0xBBF3},
        {0xF01E, 0xBBED},
        {0xF01F, 0xBBEE},
        {0xF020, 0xBBF5},
        {0xF022, 0xBBED},
        {0xF023, 0x41},
        {0xF024, 0xBBB0},
        {0xF025, 0xBBD9},
        {0xF02A, 0xEF1E},
        {0xF042, 0xBBE0},
        {0xF043, 0xBBE1},
        {0xF044, 0xBBE2},
        {0xF045, 0xBBE3},
        {0xF046, 0xBBE4},
        {0xF047, 0xBBE5},
        {0xF048, 0xBBE6},
        {0xF049, 0xBBE7},
        {0xF04A, 0xBBE8},
        {0xF04B, 0xBBE9},
        {0xF04C, 0x7F},
        {0xF060, 0x6D},
        {0xF061, 0x6E},
    };

    const auto it = direct.find(evoToken);
    if (it == direct.end())
    {
        return false;
    }
    legacyToken = it->second;
    return true;
}

static bool direct_evo_token_for_legacy(uint16_t legacyToken, uint16_t& evoToken)
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
    if (legacyToken >= 0xEF50 && legacyToken <= 0xEF59)
    {
        evoToken = static_cast<uint16_t>(0xE8B0 + (legacyToken - 0xEF50));
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
    if (legacyToken >= 0xEF82 && legacyToken <= 0xEF84)
    {
        evoToken = static_cast<uint16_t>(0xE60E + (legacyToken - 0xEF82));
        return true;
    }
    if (legacyToken >= 0xEF87 && legacyToken <= 0xEF8D)
    {
        evoToken = static_cast<uint16_t>(0xE613 + (legacyToken - 0xEF87));
        return true;
    }
    if (legacyToken >= 0xEF8F && legacyToken <= 0xEF91)
    {
        evoToken = static_cast<uint16_t>(0xE61A + (legacyToken - 0xEF8F));
        return true;
    }
    if (legacyToken >= 0xEF92 && legacyToken <= 0xEF94)
    {
        evoToken = static_cast<uint16_t>(0xE640 + (legacyToken - 0xEF92));
        return true;
    }
    if (legacyToken >= 0xEF17 && legacyToken <= 0xEF1D)
    {
        evoToken = static_cast<uint16_t>(0xE68E + (legacyToken - 0xEF17));
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
        {0xEF33, 0xE452}, {0xEFA6, 0xE453},
        {0x3C, 0xE47A}, {0x3D, 0xE47B}, {0x40, 0xE47C}, {0x6A, 0xE47F},
        {0x6B, 0xE480}, {0x6C, 0xE481}, {0x6D, 0xE482}, {0x6E, 0xE483},
        {0x6F, 0xE484}, {0x5B, 0xE81A},
        {0xD1, 0xE4C3}, {0xD4, 0xE4C6}, {0x85, 0xE4EA}, {0x93, 0xE4F5},
        {0x7E09, 0xE604},
        {0xEF37, 0xE580}, {0xEF38, 0xE581}, {0xEF39, 0xE594}, {0xEF3A, 0xE595},
        {0xEF3B, 0xE596}, {0xEF3C, 0xE597},
        {0xEF9E, 0xE105},
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
        {0xBBF0, 0x00D7}, {0xBBF1, 0x222B},
        {0xBBCF, 0x007E}, {0xBBCB, 0x03C3}, {0xBBCC, 0x03C4}, {0xBBCD, 0x00CD},
        {0xBBD1, 0x0040}, {0xBBD2, 0x0023}, {0xBBD3, 0x0024}, {0xBBD4, 0x0026},
        {0xBBD5, 0x0060}, {0xBBD6, 0x003B}, {0xBBD7, 0x005C}, {0xBBDA, 0x0025}, {0xBBDC, 0x2220},
        {0xBBDD, 0x00DF}, {0xBBDE, 0x02E3}, {0xBBDF, 0x1D1B}, {0xBBE0, 0x2080},
        {0xBBE1, 0x2081}, {0xBBE2, 0x2082}, {0xBBE3, 0x2083}, {0xBBE4, 0x2084},
        {0xBBE5, 0x2085}, {0xBBE6, 0x2086}, {0xBBE7, 0x2087}, {0xBBE8, 0x2088},
        {0xBBE9, 0x2089}, {0xBBEB, 0x25C4}, {0xBBEC, 0x25BA}, {0xBBED, 0x2191},
        {0xBBEE, 0x2193}, {0xBBF4, 0x221A}, {0xEB, 0xE836}, {0x22, 0xE44C},
        {0x7F, 0xE5BD}, {0x80, 0xE5BE}, {0x81, 0xE5BF}, {0xEF73, 0xE5C0},
        {0xEF74, 0xE5C1}, {0xEF75, 0xE5C2},
        {0xBB57, 0xE4F9}, {0xBB64, 0xE593}, {0xE8, 0xE6C6}, {0xE7, 0xE6C7},
        {0xEF79, 0xE6AE},
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
        {0x0001, 0xE425},
        {0x0002, 0xE426},
        {0x0003, 0xE427},
        {0x00B1, 0xE42F},
        {0x00B2, 0xE430},
        {0x0012, 0xE431},
        {0x00B9, 0xE432},
        {0x00BA, 0xE433},
        {0x00AB, 0xE434},
        {0xEF35, 0xE435},
        {0xBB0A, 0xE436},
        {0xBB0B, 0xE437},
        {0xBB1F, 0xE438},
        {0x00BD, 0xE43A},
        {0x00BE, 0xE43B},
        {0x00BF, 0xE43C},
        {0x00C0, 0xE43D},
        {0x00C2, 0xE43F},
        {0x00C3, 0xE440},
        {0x00C4, 0xE441},
        {0x00C5, 0xE442},
        {0x00C6, 0xE443},
        {0x00C7, 0xE444},
        {0x00C8, 0xE445},
        {0x00C9, 0xE446},
        {0x00CA, 0xE447},
        {0x00CB, 0xE448},
        {0x00CC, 0xE449},
        {0x00CD, 0xE44A},
        {0xEF34, 0xE44B},
        {0x0024, 0xE44E},
        {0x0025, 0xE44F},
        {0x0028, 0xE450},
        {0x0027, 0xE451},
        {0x0019, 0xE454},
        {0x001A, 0xE455},
        {0x0021, 0xE456},
        {0x001F, 0xE457},
        {0x00B6, 0xE458},
        {0x00B7, 0xE459},
        {0x001B, 0xE45A},
        {0x001C, 0xE45B},
        {0x001D, 0xE45C},
        {0x001E, 0xE45D},
        {0x0015, 0xE45E},
        {0x0016, 0xE45F},
        {0x0017, 0xE460},
        {0x0018, 0xE461},
        {0x00B3, 0xE462},
        {0xBB2D, 0xE464},
        {0xBB2E, 0xE465},
        {0x00B5, 0xE466},
        {0x00E2, 0xE467},
        {0x00B4, 0xE468},
        {0x0020, 0xE469},
        {0x0014, 0xE46A},
        {0xBB25, 0xE46B},
        {0xBB26, 0xE46C},
        {0xBB27, 0xE46D},
        {0xBB28, 0xE46E},
        {0xBB29, 0xE46F},
        {0xBB2A, 0xE470},
        {0xBB2B, 0xE471},
        {0xBB2C, 0xE472},
        {0xBB2F, 0xE473},
        {0xBB30, 0xE474},
        {0xBB08, 0xE475},
        {0xBB09, 0xE476},
        {0xBB0C, 0xE477},
        {0xBB0F, 0xE478},
        {0xEF32, 0xE479},
        {0x0094, 0xE47D},
        {0x0095, 0xE47E},
        {0x00B8, 0xE485},
        {0xBB20, 0xE486},
        {0xBB21, 0xE487},
        {0xBB22, 0xE488},
        {0xBB23, 0xE489},
        {0xBB24, 0xE48A},
        {0xBB00, 0xE48B},
        {0xBB01, 0xE48C},
        {0xBB02, 0xE48D},
        {0xBB03, 0xE48E},
        {0xBB04, 0xE48F},
        {0xBB05, 0xE490},
        {0xBB06, 0xE491},
        {0xBB07, 0xE492},
        {0xBB4B, 0xE493},
        {0xBB4C, 0xE494},
        {0xBB0D, 0xE495},
        {0xBB0E, 0xE496},
        {0xBB10, 0xE497},
        {0xBB11, 0xE498},
        {0xBB12, 0xE499},
        {0xBB13, 0xE49A},
        {0xBB14, 0xE49B},
        {0xBB15, 0xE49C},
        {0xBB16, 0xE49D},
        {0xEF95, 0xE49E},
        {0xBB17, 0xE49F},
        {0xBB18, 0xE4A0},
        {0xBB19, 0xE4A1},
        {0xBB1A, 0xE4A2},
        {0xBB1B, 0xE4A3},
        {0xBB1C, 0xE4A4},
        {0xBB1D, 0xE4A5},
        {0xBB1E, 0xE4A6},
        {0xBB3B, 0xE4A7},
        {0xBB3C, 0xE4A8},
        {0xBB3D, 0xE4A9},
        {0xBB46, 0xE4AA},
        {0xBB3E, 0xE4AB},
        {0xBB3F, 0xE4AC},
        {0xBB40, 0xE4AD},
        {0xBB47, 0xE4AE},
        {0xBB41, 0xE4AF},
        {0xBB48, 0xE4B0},
        {0xBB49, 0xE4B1},
        {0xBB42, 0xE4B2},
        {0xBB43, 0xE4B3},
        {0xBB44, 0xE4B4},
        {0x00CE, 0xE4C0},
        {0x00CF, 0xE4C1},
        {0x00D0, 0xE4C2},
        {0x00D2, 0xE4C4},
        {0x00D3, 0xE4C5},
        {0x00D5, 0xE4C7},
        {0x00D6, 0xE4C8},
        {0x00D7, 0xE4C9},
        {0x00D8, 0xE4CA},
        {0xEF96, 0xE4CB},
        {0x00D9, 0xE4CC},
        {0x00DA, 0xE4CD},
        {0x00DB, 0xE4CE},
        {0x00E6, 0xE4CF},
        {0x00DC, 0xE4D0},
        {0x00AD, 0xE4D1},
        {0x00DD, 0xE4D2},
        {0x00DE, 0xE4D3},
        {0x00DF, 0xE4D4},
        {0x00E5, 0xE4D5},
        {0x00E0, 0xE4D6},
        {0x00E1, 0xE4D7},
        {0x0091, 0xE4D8},
        {0x00E3, 0xE4D9},
        {0x00E4, 0xE4DA},
        {0xBB53, 0xE4DB},
        {0xBB39, 0xE4DC},
        {0xBB3A, 0xE4DD},
        {0xBB4A, 0xE4DE},
        {0xBB52, 0xE4DF},
        {0xBB54, 0xE4E0},
        {0xBB55, 0xE4E1},
        {0xBB56, 0xE4E2},
        {0xEF97, 0xE4E3},
        {0xEF98, 0xE4E4},
        {0xBB68, 0xE4E5},
        {0xBB69, 0xE4E6},
        {0xBBCE, 0xE4E7},
        {0xBB58, 0xE4E8},
        {0x00A5, 0xE4E9},
        {0x009C, 0xE4EB},
        {0x00A0, 0xE4EC},
        {0x009F, 0xE4ED},
        {0x009E, 0xE4EE},
        {0x00A3, 0xE4EF},
        {0x00A2, 0xE4F0},
        {0x00A1, 0xE4F1},
        {0x0013, 0xE4F2},
        {0x009D, 0xE4F3},
        {0x00A6, 0xE4F4},
        {0x00A7, 0xE4F6},
        {0xBB50, 0xE4F7},
        {0xBB51, 0xE4F8},
        {0xBB59, 0xE4FA},
        {0xBB5A, 0xE4FB},
        {0xBB5B, 0xE4FC},
        {0x00E9, 0xE4FD},
        {0x00EA, 0xE4FE},
        {0x00EC, 0xE4FF},
        {0x00ED, 0xE500},
        {0x00EE, 0xE501},
        {0x00F2, 0xE502},
        {0x00F3, 0xE503},
        {0x00FA, 0xE504},
        {0x00FB, 0xE505},
        {0x0005, 0xE506},
        {0x00FC, 0xE507},
        {0x00FD, 0xE508},
        {0x00FE, 0xE509},
        {0xEF16, 0xE50A},
        {0x0066, 0xE582},
        {0x0067, 0xE583},
        {0x0068, 0xE584},
        {0x0069, 0xE585},
        {0x0073, 0xE586},
        {0x0064, 0xE587},
        {0x0065, 0xE588},
        {0x0076, 0xE58A},
        {0x0077, 0xE58B},
        {0x0078, 0xE58C},
        {0x0079, 0xE58D},
        {0xBB4D, 0xE58E},
        {0xBB4E, 0xE58F},
        {0xBB4F, 0xE590},
        {0x0075, 0xE591},
        {0x0074, 0xE592},
        {0xBB66, 0xE59A},
        {0xBB67, 0xE59B},
        {0x007A, 0xE59C},
        {0x007B, 0xE59D},
        {0x007C, 0xE59E},
        {0x007D, 0xE59F},
        {0xEF41, 0xE5A0},
        {0xEF42, 0xE5A1},
        {0xEF43, 0xE5A2},
        {0xEF44, 0xE5A3},
        {0xEF45, 0xE5A4},
        {0xEF46, 0xE5A5},
        {0xEF47, 0xE5A6},
        {0xEF48, 0xE5A7},
        {0xEF49, 0xE5A8},
        {0xEF4A, 0xE5A9},
        {0xEF4B, 0xE5AA},
        {0xEF4C, 0xE5AB},
        {0xEF4D, 0xE5AC},
        {0xEF4E, 0xE5AD},
        {0xEF4F, 0xE5AE},
        {0x7E00, 0xE5AF},
        {0x7E01, 0xE5B0},
        {0x7E02, 0xE5B1},
        {0x7E03, 0xE5B2},
        {0x7E04, 0xE5B3},
        {0x7E05, 0xE5B4},
        {0xEF5A, 0xE5B5},
        {0xEF5B, 0xE5B6},
        {0xEF64, 0xE5B7},
        {0xEF65, 0xE5B8},
        {0xEF67, 0xE5B9},
        {0xEF6C, 0xE5BA},
        {0xEF6A, 0xE5BB},
        {0xEF6B, 0xE5BC},
        {0x7E06, 0xE600},
        {0x7E07, 0xE601},
        {0x7E08, 0xE603},
        {0x7E0A, 0xE605},
        {0x7E0B, 0xE606},
        {0x7E0C, 0xE607},
        {0x7E0D, 0xE608},
        {0x7E0E, 0xE609},
        {0x7E0F, 0xE60A},
        {0x7E10, 0xE60B},
        {0x7E11, 0xE60C},
        {0x7E12, 0xE60D},
        {0x00FF, 0xE643},
        {0x00F4, 0xE644},
        {0x00F5, 0xE645},
        {0x00F6, 0xE646},
        {0x00F7, 0xE647},
        {0x00F8, 0xE648},
        {0x00F9, 0xE649},
        {0x002E, 0xE64A},
        {0x002F, 0xE64B},
        {0xBB32, 0xE64C},
        {0xBB33, 0xE64D},
        {0xBB34, 0xE64E},
        {0xEF13, 0xE64F},
        {0xEF14, 0xE650},
        {0xEF15, 0xE651},
        {0x0084, 0xE680},
        {0x0086, 0xE681},
        {0x0087, 0xE682},
        {0x0088, 0xE683},
        {0x0089, 0xE684},
        {0x008A, 0xE685},
        {0x008B, 0xE686},
        {0x008C, 0xE687},
        {0x008D, 0xE688},
        {0x008E, 0xE689},
        {0x008F, 0xE68A},
        {0x0090, 0xE68B},
        {0x0092, 0xE68C},
        {0xBB65, 0xE68D},
        {0x0096, 0xE6A0},
        {0x0097, 0xE6A1},
        {0x0098, 0xE6A2},
        {0x0099, 0xE6A3},
        {0x009A, 0xE6A4},
        {0x009B, 0xE6A5},
        {0x00A4, 0xE6A6},
        {0x00A8, 0xE6A7},
        {0x00A9, 0xE6A8},
        {0xBB35, 0xE6A9},
        {0xBB36, 0xE6AA},
        {0xBB37, 0xE6AB},
        {0xBB38, 0xE6AC},
        {0xBB45, 0xE6AD},
        {0xEF05, 0xE6B7},
        {0xEF06, 0xE6B8},
        {0xEF2E, 0xE6C2},
        {0xEF2F, 0xE6C3},
        {0xEF30, 0xE6C4},
        {0xEF31, 0xE6C5},
        {0x0072, 0xE81B},
        {0x005F, 0xE81C},
        {0xBBAF, 0xF003},
        {0xBBAD, 0xF008},
        {0xBBEA, 0xF012},
        {0xBBF2, 0xF01B},
        {0xBBF3, 0xF01C},
        {0xBBF5, 0xF020},
        {0xEF1E, 0xF02A},
    };

    const auto it = direct.find(legacyToken);
    if (it == direct.end())
    {
        return false;
    }
    evoToken = it->second;
    return true;
}

static bool tokenized_legacy_payload_for_evo(uint16_t evoToken, data_t& payload)
{
    return direct_legacy_payload_for_evo(evoToken, payload)
        || legacy_payload_for_evo_ucs2(evoToken, payload);
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

data_t legacy_tokenized_data_to_evo(const data_t& legacyData, bool smartConversion)
{
    if (legacyData.size() < 2)
    {
        throw std::invalid_argument("Invalid tokenized legacy data");
    }

    static constexpr uint16_t legacyStore = 0x04;
    static constexpr uint16_t legacyQuote = 0x2A;
    static constexpr uint16_t legacyColon = 0x3E;
    static constexpr uint16_t legacyNewLine = 0x3F;
    static constexpr uint16_t legacyDelVar = 0xBB54;
    static constexpr uint16_t evoNewLine = 0xE41C;

    data_t evo;
    bool isWithinString = false;
    bool inEvaluatedString = false;
    uint16_t lastEvoToken = 0;
    bool delVarStatementOpen = false;
    bool delVarHasArgument = false;
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

        if (smartConversion && !isWithinString && legacyToken == legacyDelVar && delVarStatementOpen && delVarHasArgument)
        {
            append_evo_token(evo, evoNewLine);
            delVarStatementOpen = false;
            delVarHasArgument = false;
        }

        uint16_t evoToken = 0;
        if (legacyToken == legacyStore || legacyToken == legacyNewLine)
        {
            isWithinString = false;
            inEvaluatedString = false;
        }

        if (!direct_evo_token_for_legacy(legacyToken, evoToken)
            && !(isWithinString && !inEvaluatedString && legacyToken != legacyQuote && legacy_token_to_evo_ucs2(legacyToken, evoToken)))
        {
            std::cerr << "[Warning] Cannot convert 84+CE token "
                      << TypeHandlers::TH_Tokenized::oneTokenBytesToString(legacyToken)
                      << " to an Evo token; replacing with ?" << std::endl;
            evoToken = 0xE41B;
        }
        append_evo_token(evo, evoToken);

        if (!isWithinString)
        {
            if (legacyToken == legacyColon || legacyToken == legacyNewLine)
            {
                delVarStatementOpen = false;
                delVarHasArgument = false;
            }
            else if (legacyToken == legacyDelVar)
            {
                delVarStatementOpen = true;
                delVarHasArgument = false;
            }
            else if (delVarStatementOpen)
            {
                delVarHasArgument = true;
            }
        }

        if (legacyToken == legacyQuote)
        {
            isWithinString = !isWithinString;
            inEvaluatedString = isWithinString && evo_token_starts_evaluated_string(lastEvoToken);
        }
        lastEvoToken = evoToken;

        i += static_cast<size_t>(incr);
    }
    append_evo_token(evo, 0);
    return evo;
}

static uint16_t read_le16_at(const data_t& data, size_t offset)
{
    if (offset + 1 >= data.size())
    {
        throw std::invalid_argument("Unexpected end of Evo data");
    }
    return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
}

static void append_le16(data_t& data, uint16_t word)
{
    data.push_back(static_cast<uint8_t>(word & 0xFF));
    data.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
}

static std::vector<uint16_t> evo_words(const data_t& data)
{
    std::vector<uint16_t> words;
    words.reserve(data.size() / 2);
    for (size_t i = 0; i + 1 < data.size(); i += 2)
    {
        words.push_back(read_le16_at(data, i));
    }
    return words;
}

static uint8_t real_equivalent_subtype(uint8_t subtype)
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

static data_t normalize_legacy_real_member(data_t data)
{
    if (data.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        throw std::invalid_argument("Invalid legacy real payload");
    }
    data[0] = static_cast<uint8_t>((data[0] & 0x80) | real_equivalent_subtype(data[0] & 0x3F));
    return data;
}

static bool legacy_real_is_zero(data_t data)
{
    if (data.size() != TypeHandlers::TH_GenericReal::dataByteCount)
    {
        return false;
    }
    data = normalize_legacy_real_member(data);
    return TypeHandlers::TH_GenericReal::makeStringFromData(data) == "0";
}

static data_t legacy_fp_to_evo_decimal(const data_t& legacyReal)
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

static data_t evo_decimal_to_legacy_fp(const data_t& evoData, size_t offset, uint8_t legacyType)
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

static uint64_t parse_evo_integer_limbs(const std::vector<uint16_t>& words, size_t begin, size_t end)
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

static data_t decimal_string_to_legacy_real(const std::string& value, uint8_t legacyType)
{
    data_t legacy = TypeHandlers::STH_FP::makeDataFromString(value);
    legacy[0] = static_cast<uint8_t>((legacy[0] & 0x80) | (legacyType & 0x3F));
    return legacy;
}

static data_t evo_scalar_to_legacy_real(const data_t& evoData, size_t& offset, uint8_t legacyType = 0x00)
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

static void append_evo_uint64(data_t& data, uint64_t value)
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

static data_t legacy_real_to_evo_scalar(data_t legacyReal)
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
