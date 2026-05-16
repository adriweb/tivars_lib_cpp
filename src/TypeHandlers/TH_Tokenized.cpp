/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../json.hpp"
#include "../TIVarFile.h"
#include "../tivarslib_utils.h"

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <cstring>
#include <array>
#include <algorithm>
#include <set>

#include <pugixml.hpp>

using namespace std::string_literals;
using json = nlohmann::ordered_json;

extern "C"
{
    extern const unsigned char tivars_builtin_tokens_xml[];
    extern const size_t tivars_builtin_tokens_xml_size;
}

static size_t strlen_mb(const std::string& s)
{
    size_t len = 0;
    for (const char* p = s.data(); *p != 0; ++p)
        len += (*p & 0xc0) != 0x80;
    return len;
}

static void advance_source_pos(const std::string& s, uint16_t& line, uint16_t& column)
{
    for (const char i : s)
    {
        const unsigned char c = static_cast<unsigned char>(i);
        if (c == '\n')
        {
            line++;
            column = 0;
            continue;
        }
        if ((c & 0xc0) != 0x80)
        {
            column++;
        }
    }
}

static size_t token_boundary_separator_len_at(const std::string& s, size_t pos)
{
    static constexpr std::array<std::string_view, 3> separators = { "␟", " ", "‌" };

    for (const auto& separator : separators)
    {
        if (pos + separator.size() <= s.size() && std::memcmp(&s[pos], separator.data(), separator.size()) == 0)
        {
            return separator.size();
        }
    }

    return 0;
}

static size_t utf8_codepoint_len_at(const std::string& s, size_t pos)
{
    if (pos >= s.size())
    {
        return 0;
    }

    const uint8_t lead = static_cast<uint8_t>(s[pos]);
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

static bool source_has_literal_at(const std::string& s, size_t pos, std::string_view literal)
{
    return pos + literal.size() <= s.size() && std::memcmp(&s[pos], literal.data(), literal.size()) == 0;
}

static size_t source_segment_len_until_boundary(const std::string& s, size_t pos)
{
    size_t cursor = pos;

    while (cursor < s.size())
    {
        if (s[cursor] == '\\' || token_boundary_separator_len_at(s, cursor) > 0)
        {
            break;
        }

        cursor += utf8_codepoint_len_at(s, cursor);
    }

    return cursor - pos;
}

static bool parse_raw_token_escape_at(const std::string& s, size_t pos, size_t& consumedLen, uint16_t& tokenValue)
{
    if (pos + 4 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'x' &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 2])) &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 3])))
    {
        tokenValue = static_cast<uint16_t>(tivars::hexdec(s.substr(pos + 2, 2)));
        consumedLen = 4;
        return true;
    }

    if (pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u' &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 2])) &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 3])) &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 4])) &&
        std::isxdigit(static_cast<unsigned char>(s[pos + 5])))
    {
        tokenValue = static_cast<uint16_t>((tivars::hexdec(s.substr(pos + 2, 2)) << 8) |
                                           tivars::hexdec(s.substr(pos + 4, 2)));
        consumedLen = 6;
        return true;
    }

    return false;
}

static std::string format_raw_token_escape(uint16_t tokenValue)
{
    if (tokenValue <= 0xFF)
    {
        return "\\x" + tivars::dechex(static_cast<unsigned char>(tokenValue));
    }

    return "\\u" + tivars::dechex(static_cast<unsigned char>(tokenValue >> 8))
           + tivars::dechex(static_cast<unsigned char>(tokenValue & 0xFF));
}

static std::string format_token_hex(uint16_t tokenValue)
{
    if (tokenValue <= 0xFF)
    {
        return tivars::dechex(static_cast<unsigned char>(tokenValue));
    }

    return tivars::dechex(static_cast<unsigned char>(tokenValue >> 8))
           + tivars::dechex(static_cast<unsigned char>(tokenValue & 0xFF));
}

static void ltrim_program_whitespace(std::string& s)
{
    size_t pos = 0;
    while (pos < s.size())
    {
        const uint8_t c = static_cast<uint8_t>(s[pos]);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == 0xA0)
        {
            pos++;
            continue;
        }
        if (pos + 1 < s.size() && c == 0xC2 && static_cast<uint8_t>(s[pos + 1]) == 0xA0)
        {
            pos += 2;
            continue;
        }
        break;
    }

    s.erase(0, pos);
}

static std::string get_first_command(const std::string& line)
{
    const std::string trimmedLine = tivars::trim(line);
    if (trimmedLine.empty())
    {
        return "";
    }

    size_t endPos = trimmedLine.find(' ');
    if (endPos == std::string::npos)
    {
        endPos = trimmedLine.find('(');
    }

    return tivars::trim(trimmedLine.substr(0, endPos));
}

static std::string prettify_token_string(std::string str)
{
    for (char c = 'a'; c <= 'f'; c++)
    {
        tivars::replace_all(str, "[|"s + c + "]", std::string(1, c));
    }

    for (const char c : "prszt"s)
    {
        tivars::replace_all(str, "["s + c + "]", std::string(1, c));
    }

    for (const char c : "uvw"s)
    {
        tivars::replace_all(str, "|"s + c, std::string(1, c));
    }

    return str;
}

static bool is_standard_keyboard_typable(const std::string& str)
{
    return std::ranges::all_of(str, [](unsigned char c) { return c == '\n' || (c >= 0x20 && c <= 0x7E); });
}

static std::string format_last_resort_detok_string(uint16_t tokenValue)
{
    return tokenValue == 0x3F ? "\n" : format_raw_token_escape(tokenValue);
}

static std::string format_detok_string_for_log(const std::string& str)
{
    return str == "\n" ? "\\n" : str;
}

static void split_var_keyword_lines(std::string& str, const std::string& keyword)
{
    size_t pos = 0;
    while ((pos = str.find(keyword, pos)) != std::string::npos)
    {
        if (pos > 0 && !std::isspace(static_cast<unsigned char>(str[pos - 1])))
        {
            str.insert(pos, "\n");
            pos += 1 + keyword.size();
        }
        else
        {
            pos += keyword.size();
        }
    }
}

static std::string bytes_to_hex(const data_t& data)
{
    std::string result;
    for (const uint8_t byte : data)
    {
        result += tivars::dechex(byte);
    }
    return result;
}

static uint16_t read_be16(const data_t& data, size_t offset)
{
    if (offset + 1 >= data.size())
    {
        return 0;
    }
    return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
}

static std::string read_c_string_ascii(const data_t& data, size_t offset)
{
    std::string result;
    for (size_t i = offset; i < data.size() && data[i] != 0; i++)
    {
        result.push_back(static_cast<char>(data[i]));
    }
    return result;
}

static json parse_asm_metadata(const data_t& rawBytes)
{
    json j = {
        {"isAssembly", false},
        {"rawDataHex", bytes_to_hex(rawBytes)},
    };

    if (rawBytes.size() < 2)
    {
        return j;
    }

    const uint16_t header = read_be16(rawBytes, 0);
    if (header == 0xBB6D)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";

        if (rawBytes.size() >= 4 && rawBytes[2] == 0xC9)
        {
            const uint8_t kind = rawBytes[3];
            if (kind == 0x01 || kind == 0x03)
            {
                j["shell"] = "MirageOS";
                j["variant"] = "ShellProgram";
                j["type"] = kind;
                j["description"] = read_c_string_ascii(rawBytes, kind == 0x03 ? 36 : 34);
            }
            else if (kind == 0x02)
            {
                j["shell"] = "MirageOS";
                j["variant"] = "ExternalInterface";
                if (rawBytes.size() >= 6)
                {
                    j["interfaceId"] = rawBytes[4];
                    j["xCoord"] = rawBytes[5];
                }
                j["description"] = read_c_string_ascii(rawBytes, 6);
            }
            else if (kind == 0x30)
            {
                j["shell"] = "Ion";
                j["variant"] = "Ion";
                j["description"] = read_c_string_ascii(rawBytes, 5);
            }
            else if (kind == 0x54)
            {
                j["shell"] = "TSE";
                j["variant"] = "TSE";
                if (rawBytes.size() >= 11 && read_be16(rawBytes, 4) == 0x5445)
                {
                    j["programTitle"] = read_c_string_ascii(rawBytes, 8);
                }
            }
        }
        else if (rawBytes.size() >= 4 && rawBytes[2] == 0xAF && rawBytes[3] == 0x30)
        {
            j["shell"] = "Ion";
            j["variant"] = "IonAndOS";
            j["description"] = read_c_string_ascii(rawBytes, 5);
        }
        return j;
    }

    if (header == 0xC930)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        j["shell"] = "Ion";
        j["variant"] = "83Ion";
        j["description"] = read_c_string_ascii(rawBytes, 3);
        return j;
    }

    if ((header & 0x00FF) == 0x18)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        j["shell"] = rawBytes[0] != 0 ? "TI-Explorer" : "ASHELL83";
        j["jr"] = tivars::dechex(rawBytes[1]) + tivars::dechex(rawBytes.size() > 2 ? rawBytes[2] : 0);
        if (rawBytes.size() >= 7)
        {
            j["tablePointer"] = read_be16(rawBytes, 3);
            j["descriptionPointer"] = read_be16(rawBytes, 5);
        }
        return j;
    }

    if (header == 0xAF28)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        j["shell"] = "SOS";
        if (rawBytes.size() >= 7)
        {
            j["librariesPointer"] = read_be16(rawBytes, 3);
            j["descriptionPointer"] = read_be16(rawBytes, 5);
        }
        return j;
    }

    if (header == 0xEF7B)
    {
        j["isAssembly"] = true;
        j["cpu"] = "eZ80";
        j["shell"] = "Asm84CEPrgm";
        if (rawBytes.size() >= 3)
        {
            if (rawBytes[2] == 0x00)
            {
                j["variant"] = "C";
            }
            else if (rawBytes[2] == 0x7F)
            {
                j["variant"] = "ICE";
            }
        }
        if (rawBytes.size() >= 7 && rawBytes[3] == 0xC3 && (rawBytes[6] == 0x01 || rawBytes[6] == 0x02))
        {
            j["shellHeaderType"] = rawBytes[6];
            if (rawBytes[6] == 0x01 && rawBytes.size() >= 265)
            {
                j["iconWidth"] = rawBytes[7];
                j["iconHeight"] = rawBytes[8];
                j["description"] = read_c_string_ascii(rawBytes, 265);
            }
            else
            {
                j["description"] = read_c_string_ascii(rawBytes, 7);
            }
        }
        return j;
    }

    if (header == 0xD900)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        if (rawBytes.size() >= 6 && std::memcmp(&rawBytes[2], "Duck", 4) == 0)
        {
            j["shell"] = "Mallard";
            if (rawBytes.size() >= 8)
            {
                j["startAddress"] = read_be16(rawBytes, 6);
                j["description"] = read_c_string_ascii(rawBytes, 8);
            }
        }
        else
        {
            j["shell"] = "ASH";
            j["description"] = read_c_string_ascii(rawBytes, 3);
        }
        return j;
    }

    if (header == 0xD500)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        j["shell"] = "Crash";
        j["description"] = read_c_string_ascii(rawBytes, 3);
        return j;
    }

    if (header == 0xEF69)
    {
        j["isAssembly"] = true;
        j["cpu"] = "Z80";
        j["shell"] = "CSE";
        return j;
    }

    return j;
}

namespace tivars::TypeHandlers
{
    namespace
    {
        struct TokenNames
        {
            std::array<std::string, TH_Tokenized::LANG_MAX> display{};
            std::array<std::set<std::string>, TH_Tokenized::LANG_MAX> variants{};
            std::array<std::set<std::string>, TH_Tokenized::LANG_MAX> accessibles{};
        };

        std::unordered_map<uint16_t, TokenNames> tokens_BytesToNames;
        std::unordered_map<std::string, uint16_t> tokens_NameToBytes;
        uint8_t lengthOfLongestTokenName;
        std::vector<uint8_t> firstByteOfTwoByteTokens;
        bool tokensInitialized = false;
        constexpr uint16_t squishedASMTokens[] = { 0xBB6D, 0xEF69, 0xEF7B }; // 83+/84+, 84+CSE, CE

        static void parse_tokens_xml_and_register(const std::string& xml);

        static void init_tokens()
        {
            if (tokensInitialized)
            {
                return;
            }

            tokens_BytesToNames.clear();
            tokens_NameToBytes.clear();
            firstByteOfTwoByteTokens.clear();
            lengthOfLongestTokenName = 0;

            const std::string xmlStr(
                reinterpret_cast<const char*>(tivars_builtin_tokens_xml),
                tivars_builtin_tokens_xml_size
            );
            if (xmlStr.empty())
            {
                throw std::runtime_error("Embedded token XML is empty");
            }

            parse_tokens_xml_and_register(xmlStr);
            tokens_BytesToNames[0x00].display = { "", "" };
            tokensInitialized = true;
        }

        static void ensure_tokens_initialized()
        {
            if (!tokensInitialized)
            {
                init_tokens();
            }
        }

        struct TokenScanState
        {
            bool isInCustomName = false; // after a "prgm" or ʟ token
            bool isWithinString = false;
            bool inEvaluatedString = false; // CE OS 5.2 added string interpolation with eval() for TI-Innovator commands
            uint16_t lastTokenBytes = 0;
        };

        static void register_token_lookup_name(const std::string& name, uint16_t tokenValue)
        {
            if (name.empty())
            {
                return;
            }

            tokens_NameToBytes[name] = tokenValue;
            if (name.size() > lengthOfLongestTokenName)
            {
                lengthOfLongestTokenName = static_cast<uint8_t>(name.size());
            }
        }

        static bool can_start_explicit_string_alias(char c)
        {
            return std::string_view("[]{}|^_").find(c) != std::string_view::npos;
        }

        static bool starts_evaluated_string(uint16_t token)
        {
            return token == 0xE7     // Send(
                || token == 0xBB2A;  // expr(
        }

        template<typename OnToken, typename OnSkipped>
        static void scan_source_tokens_impl(const std::string& str, bool detect_strings, TokenScanState& state, OnToken&& onToken, OnSkipped&& onSkipped)
        {
            static const std::string backslashStr = "\\";

            for (size_t strCursorPos = 0; strCursorPos < str.length(); strCursorPos++)
            {
                const size_t separatorLen = token_boundary_separator_len_at(str, strCursorPos);
                if (separatorLen > 0)
                {
                    state.isInCustomName = false;
                    state.lastTokenBytes = 0;
                    onSkipped(str.substr(strCursorPos, separatorLen));
                    strCursorPos += separatorLen - 1;
                    continue;
                }

                const std::string currChar = str.substr(strCursorPos, 1);

                if (currChar == backslashStr)
                {
                    size_t escapeLen = 0;
                    uint16_t escapedTokenValue = 0;
                    if (parse_raw_token_escape_at(str, strCursorPos, escapeLen, escapedTokenValue))
                    {
                        onToken(str.substr(strCursorPos, escapeLen), escapedTokenValue);
                        state.lastTokenBytes = escapedTokenValue;
                        strCursorPos += escapeLen - 1;
                        continue;
                    }

                    const auto backslashTokenIt = tokens_NameToBytes.find(backslashStr);
                    if (strCursorPos + 1 < str.length() && str[strCursorPos + 1] == '\\' && backslashTokenIt != tokens_NameToBytes.end())
                    {
                        onToken("\\\\", backslashTokenIt->second);
                        state.lastTokenBytes = backslashTokenIt->second;
                        strCursorPos++;
                    } else {
                        state.isInCustomName = false;
                        state.lastTokenBytes = 0;
                        onSkipped(currChar);
                    }
                    continue;
                }

                if (detect_strings)
                {
                    if ((state.lastTokenBytes == 0x5F || state.lastTokenBytes == 0xEB)) { // prgm and ʟ
                        state.isInCustomName = true;
                    } else if (currChar == "\"") {
                        state.isWithinString = !state.isWithinString;
                        state.inEvaluatedString = state.isWithinString && starts_evaluated_string(state.lastTokenBytes);
                    } else if (currChar == "\n" ||
                        source_has_literal_at(str, strCursorPos, "→") || source_has_literal_at(str, strCursorPos, "->"))
                    {
                        state.isInCustomName = false;
                        state.isWithinString = false;
                        state.inEvaluatedString = false;
                    } else if (state.isInCustomName && !isalnum(currChar[0])) {
                        state.isInCustomName = false;
                    }
                }

                const size_t maxTokSearchLen = std::min(source_segment_len_until_boundary(str, strCursorPos),
                                                        (size_t)lengthOfLongestTokenName);
                const bool needMinMunch = state.isInCustomName || (state.isWithinString && !state.inEvaluatedString);
                bool matched = false;

                if (state.isWithinString && !state.inEvaluatedString && can_start_explicit_string_alias(str[strCursorPos]))
                {
                    for (size_t currentLength = maxTokSearchLen; currentLength > 1; currentLength--)
                    {
                        const std::string currentSubString = str.substr(strCursorPos, currentLength);
                        const auto tokenIt = tokens_NameToBytes.find(currentSubString);
                        if (tokenIt == tokens_NameToBytes.end())
                        {
                            continue;
                        }

                        onToken(currentSubString, tokenIt->second);
                        strCursorPos += currentLength - 1;
                        state.lastTokenBytes = tokenIt->second;
                        matched = true;
                        break;
                    }
                }

                /* needMinMunch => minimum token length, otherwise maximal munch */
                for (size_t currentLength = needMinMunch ? 1 : maxTokSearchLen;
                     !matched && (needMinMunch ? (currentLength <= maxTokSearchLen) : (currentLength > 0));
                     currentLength += (needMinMunch ? 1 : -1))
                {
                    std::string currentSubString = str.substr(strCursorPos, currentLength);

                    // We want to use true-lowercase alpha tokens in this case.
                    if ((state.isWithinString && !state.inEvaluatedString) && currentLength == 1 && std::islower(static_cast<unsigned char>(currentSubString[0])))
                    {
                        // 0xBBB0 is 'a', etc. But we skip what would be 'l' at 0xBBBB which doesn't exist (prefix conflict)
                        const char letter = currentSubString[0];
                        uint16_t tokenValue = 0xBBB0 + (letter - 'a') + (letter >= 'l' ? 1 : 0);
                        onToken(currentSubString, tokenValue);
                        state.lastTokenBytes = tokenValue;
                        matched = true;
                        break;
                    }

                    const auto tokenIt = tokens_NameToBytes.find(currentSubString);
                    if (tokenIt != tokens_NameToBytes.end())
                    {
                        onToken(currentSubString, tokenIt->second);
                        strCursorPos += currentLength - 1;
                        state.lastTokenBytes = tokenIt->second;
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                {
                    onSkipped(currChar);
                }
            }
        }

        template<typename OnToken, typename OnSkipped>
        static void scan_source_tokens(const std::string& str, bool detect_strings, OnToken&& onToken, OnSkipped&& onSkipped)
        {
            TokenScanState state{};
            scan_source_tokens_impl(str, detect_strings, state, std::forward<OnToken>(onToken), std::forward<OnSkipped>(onSkipped));
        }

        static data_t tokenize_source_to_raw_bytes(const std::string& str, bool detect_strings, TokenScanState initialState)
        {
            data_t data;

            scan_source_tokens_impl(str, detect_strings, initialState,
                                    [&](const std::string&, uint16_t tokenValue)
                                    {
                                        if (tokenValue > 0xFF)
                                        {
                                            data.push_back((uint8_t)(tokenValue >> 8));
                                        }
                                        data.push_back((uint8_t)(tokenValue & 0xFF));
                                    },
                                    [](const std::string&) {});

            return data;
        }

        static void advance_token_scan_state(const std::string& str, TokenScanState& state)
        {
            scan_source_tokens_impl(str, true, state,
                                    [](const std::string&, uint16_t) {},
                                    [](const std::string&) {});
        }

        static bool append_can_merge_with_previous_token(const std::string& existing, const std::string& appended)
        {
            if (existing.empty() || appended.empty() || lengthOfLongestTokenName < 2)
            {
                return false;
            }

            const size_t maxSuffixLen = std::min(existing.size(), static_cast<size_t>(lengthOfLongestTokenName - 1));
            for (size_t suffixLen = 1; suffixLen <= maxSuffixLen; suffixLen++)
            {
                const std::string suffix = existing.substr(existing.size() - suffixLen);
                const size_t maxPrefixLen = std::min(appended.size(), static_cast<size_t>(lengthOfLongestTokenName) - suffixLen);
                for (size_t prefixLen = 1; prefixLen <= maxPrefixLen; prefixLen++)
                {
                    if (tokens_NameToBytes.contains(suffix + appended.substr(0, prefixLen)))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        static std::string get_detok_primary_string(uint16_t bytesKey, uint8_t langIdx, const options_t& options)
        {
            using TH_Tokenized::LANG_EN;

            const auto it = tokens_BytesToNames.find(bytesKey);
            if (it == tokens_BytesToNames.end())
            {
                return format_raw_token_escape(bytesKey);
            }

            const TokenNames& tokenNames = it->second;
            const std::string& display = tokenNames.display[langIdx];
            const bool accessibleDetok = options.contains("accessible") && options.at("accessible") == 1;
            if (!accessibleDetok || is_standard_keyboard_typable(display))
            {
                return display;
            }

            const auto& accessibles = tokenNames.accessibles[langIdx].empty() && langIdx != LANG_EN
                                      ? tokenNames.accessibles[LANG_EN]
                                      : tokenNames.accessibles[langIdx];

            return accessibles.empty() ? display : *accessibles.begin();
        }

        // Shared XML parsing routine used by both standard and Qt/CEmu builds
        static void parse_tokens_xml_and_register(const std::string& xml)
        {
            using namespace pugi;
            using TH_Tokenized::LANG_EN;
            using TH_Tokenized::LANG_FR;

            if (xml.empty()) return;

            xml_document doc;
            const xml_parse_result ok = doc.load_string(xml.c_str(), parse_default | parse_declaration);
            if (!ok) return;

            auto pick_display = [](const xml_node& tokenNode, const char* langCode) -> std::string
            {
                std::string lastVal;
                std::string preferredParen;
                for (const xml_node& ver : tokenNode.children("version"))
                {
                    for (const xml_node& lang : ver.children("lang"))
                    {
                        const char* code = lang.attribute("code").as_string("");
                        if (std::strcmp(code, langCode) != 0) continue;
                        const char* disp = lang.attribute("display").as_string(nullptr);
                        std::string val;
                        if (disp && *disp)
                        {
                            val = disp;
                        }
                        else
                        {
                            const xml_node& acc = lang.child("accessible");
                            if (acc) val = acc.child_value();
                        }
                        if (!val.empty())
                        {
                            lastVal = val;
                            if (preferredParen.empty() && val.back() == '(')
                                preferredParen = val;
                        }
                    }
                }
                return !preferredParen.empty() ? preferredParen : lastVal;
            };

            auto register_aliases = [&](uint16_t bytes, const xml_node& tokenNode)
            {
                for (const xml_node& ver : tokenNode.children("version"))
                {
                    for (const xml_node& lang : ver.children("lang"))
                    {
                        const char* code = lang.attribute("code").as_string("");
                        const uint8_t langIdx = std::strcmp(code, "fr") == 0 ? LANG_FR : LANG_EN;

                        auto register_alias_nodes = [&](const char* nodeName, auto& aliasList)
                        {
                            for (const xml_node& v : lang.children(nodeName))
                            {
                                const std::string s = v.child_value();
                                if (!s.empty())
                                {
                                    aliasList.insert(s);
                                    register_token_lookup_name(s, bytes);
                                }
                            }
                        };

                        auto& tokenNames = tokens_BytesToNames[bytes];
                        register_alias_nodes("accessible", tokenNames.accessibles[langIdx]);
                        register_alias_nodes("variant", tokenNames.variants[langIdx]);
                    }
                }
            };

            auto process_token = [&](const xml_node& tokenNode, uint16_t bytes)
            {
                const std::string en = pick_display(tokenNode, "en");
                const std::string fr = pick_display(tokenNode, "fr");

                if (!en.empty() || !fr.empty())
                    tokens_BytesToNames[bytes].display = { en, fr.empty() ? en : fr };
                if (!en.empty())
                    register_token_lookup_name(en, bytes);
                if (!fr.empty())
                    register_token_lookup_name(fr, bytes);

                register_aliases(bytes, tokenNode);
            };

            const xml_node root = doc.child("tokens");
            if (!root) return;

            // Two-byte tokens
            for (const xml_node& twobyte : root.children("two-byte"))
            {
                const char* val = twobyte.attribute("value").as_string("");
                if (val[0] == '$' && strlen(val) >= 3)
                {
                    const uint8_t prefix = (uint8_t) strtol(val+1, nullptr, 16);
                    if (!is_in_vector(firstByteOfTwoByteTokens, prefix))
                        firstByteOfTwoByteTokens.push_back(prefix);

                    for (const xml_node& tok : twobyte.children("token"))
                    {
                        const char* s = tok.attribute("value").as_string("");
                        if (s[0] == '$' && strlen(s) >= 3)
                        {
                            const uint8_t second = (uint8_t) strtol(s+1, nullptr, 16);
                            const uint16_t bytes = (uint16_t)((prefix << 8) | second);
                            process_token(tok, bytes);
                        }
                    }
                }
            }

            // Single-byte tokens
            for (const xml_node& tok : root.children("token"))
            {
                const char* s = tok.attribute("value").as_string("");
                if (s[0] == '$' && strlen(s) >= 3)
                {
                    const uint8_t b = (uint8_t) strtol(s+1, nullptr, 16);
                    if (b == 0x00) continue; // set later
                    process_token(tok, b);
                }
            }
        }
    }

    data_t TH_Tokenized::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;
        ensure_tokens_initialized();

        const std::string trimmed = tivars::trim(str);
        if (!trimmed.empty() && trimmed.front() == '{')
        {
            try
            {
                const json j = json::parse(trimmed);
                if (j.contains("rawDataHex"))
                {
                    data_t payload = tivars::hex_string_to_bytes(j.at("rawDataHex").get<std::string>(), "rawDataHex");
                    data_t data;
                    data.reserve(2 + payload.size());
                    data.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
                    data.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
                    tivars::vector_append(data, payload);
                    return data;
                }
            }
            catch (const json::exception&)
            {
                // Ignore non-JSON input and fall back to regular tokenized parsing.
            }
        }

        data_t data;

        const bool deindent = options.contains("deindent") && options.at("deindent") == 1;
        const bool detect_strings = !options.contains("detect_strings") || options.at("detect_strings") != 0;

        std::string str_new;
        if (deindent)
        {
            std::istringstream f{str};
            std::string line;
            while (std::getline(f, line)) {
                str_new += ltrim(line) + "\n";
            }
            str_new.pop_back();
        } else {
            str_new = str;
        }

        // two bytes left for the size. Filled later
        data.reserve(2 + str_new.size());
        data.resize(2);

        scan_source_tokens(str_new, detect_strings,
                           [&](const std::string&, uint16_t tokenValue)
                           {
                               if (tokenValue > 0xFF)
                               {
                                   data.push_back((uint8_t)(tokenValue >> 8));
                               }
                               data.push_back((uint8_t)(tokenValue & 0xFF));
                           },
                           [](const std::string&) {});

        size_t actualDataLen = data.size() - 2;
        data[0] = (uint8_t)(actualDataLen & 0xFF);
        data[1] = (uint8_t)((actualDataLen >> 8) & 0xFF);
        return data;
    }

    std::string TH_Tokenized::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        ensure_tokens_initialized();

        const size_t dataSize = data.size();

        const bool fromRawBytes = options.contains("fromRawBytes") && options.at("fromRawBytes") == 1;
        if (fromRawBytes && dataSize == 0)
        {
            return "";
        }
        if (!fromRawBytes && dataSize < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes (size fields)");
        }

        const uint8_t langIdx = (options.contains("lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;

        const size_t howManyBytes = fromRawBytes ? (int)data.size() : ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        if (!fromRawBytes)
        {
            if (howManyBytes != dataSize - 2)
            {
                std::cerr << "[Warning] Byte count (" << (dataSize - 2) << ") and size field (" << howManyBytes  << ") mismatch!" << std::endl;
            }
        }

        const data_t rawBytes(data.begin() + (fromRawBytes ? 0 : 2), data.end());
        if (options.contains("metadata") && options.at("metadata") == 1)
        {
            json metadata = parse_asm_metadata(rawBytes);
            if (_ctx != nullptr && !_ctx->getVarEntries().empty())
            {
                metadata["typeName"] = _ctx->getVarEntries()[0]._type.getName();
            }
            if (!metadata["isAssembly"].get<bool>())
            {
                options_t baseOptions = options;
                baseOptions.erase("metadata");
                metadata["code"] = makeStringFromData(data, baseOptions, _ctx);
            }
            return metadata.dump(4);
        }

        if (!fromRawBytes && howManyBytes >= 2 && dataSize >= 4)
        {
            const uint16_t twoFirstBytes = (uint16_t) ((data[3] & 0xFF) + ((data[2] & 0xFF) << 8));
            if (std::ranges::find(squishedASMTokens, twoFirstBytes) != std::end(squishedASMTokens))
            {
                return "[Error] This is a squished ASM program - cannot preview it!";
            }
        }

        const bool prettify = options.contains("prettify") && options.at("prettify") == 1;
        const bool accessibleDetok = options.contains("accessible") && options.at("accessible") == 1;

        std::string str;
        data_t verifiedRawBytes;
        TokenScanState detokState{};

        auto validate_detok_token = [&](const std::string& token, const data_t& tokenRawBytes)
        {
            if (tokenize_source_to_raw_bytes(token, true, detokState) != tokenRawBytes)
            {
                return false;
            }
            if (!append_can_merge_with_previous_token(str, token))
            {
                return true;
            }

            data_t expectedFullBytes = verifiedRawBytes;
            tivars::vector_append(expectedFullBytes, tokenRawBytes);
            return tokenize_source_to_raw_bytes(str + token, true, TokenScanState{}) == expectedFullBytes;
        };

        auto accept_detok_token = [&](const std::string& token, const data_t& tokenRawBytes)
        {
            str += token;
            tivars::vector_append(verifiedRawBytes, tokenRawBytes);
            advance_token_scan_state(token, detokState);
        };

        for (size_t i = fromRawBytes ? 0 : 2; i < dataSize; i++)
        {
            const uint8_t currentToken = data[i];
            uint8_t nextToken = (i < dataSize-1) ? data[i+1] : (uint8_t)-1;
            uint16_t bytesKey = currentToken;
            data_t currentRawBytes = { currentToken };
            if (is_in_vector(firstByteOfTwoByteTokens, currentToken))
            {
                if (nextToken == (uint8_t)-1)
                {
                    std::cerr << "[Warning] Encountered an unfinished two-byte token! Setting the second byte to 0x00" << std::endl;
                    nextToken = 0x00;
                }
                bytesKey = nextToken + (currentToken << 8);
                i++;
                currentRawBytes.push_back(nextToken);
            }

            const auto tokenNamesIt = tokens_BytesToNames.find(bytesKey);
            if (tokenNamesIt != tokens_BytesToNames.end())
            {
                const TokenNames& tokenNames = tokenNamesIt->second;
                const std::string tokStr = get_detok_primary_string(bytesKey, langIdx, options);
                if (prettify || accessibleDetok)
                {
                    str += tokStr;
                }
                else if (tokStr.empty())
                {
                    accept_detok_token(format_last_resort_detok_string(bytesKey), currentRawBytes);
                }
                else if (validate_detok_token(tokStr, currentRawBytes))
                {
                    accept_detok_token(tokStr, currentRawBytes);
                }
                else
                {
                    bool acceptedFallback = false;

                    auto try_aliases = [&](const auto& aliases, uint8_t idx)
                    {
                        for (const auto& aliasCandidate : aliases[idx])
                        {
                            if (aliasCandidate != tokStr && validate_detok_token(aliasCandidate, currentRawBytes))
                            {
                                accept_detok_token(aliasCandidate, currentRawBytes);
                                acceptedFallback = true;
                                return;
                            }
                        }
                    };

                    try_aliases(tokenNames.variants, langIdx);
                    if (!acceptedFallback) try_aliases(tokenNames.accessibles, langIdx);
                    if (!acceptedFallback && langIdx != LANG_EN) try_aliases(tokenNames.variants, LANG_EN);
                    if (!acceptedFallback && langIdx != LANG_EN) try_aliases(tokenNames.accessibles, LANG_EN);

                    const std::string escapedToken = "\\" + tokStr;
                    if (!acceptedFallback && validate_detok_token(escapedToken, currentRawBytes))
                    {
                        accept_detok_token(escapedToken, currentRawBytes);
                        acceptedFallback = true;
                    }

                    if (!acceptedFallback)
                    {
                        const std::string rawEscape = format_last_resort_detok_string(bytesKey);
                        std::cerr << "[Warning] Appending token 0x" << format_token_hex(bytesKey)
                                  << " (" << tokStr << ") made the accumulated detokenized string non-roundtrippable, using "
                                  << format_detok_string_for_log(rawEscape) << " instead!" << std::endl;
                        accept_detok_token(rawEscape, currentRawBytes);
                    }
                }
            } else {
                const std::string rawEscape = format_last_resort_detok_string(bytesKey);
                std::cerr << "[Warning] Unknown token 0x" << format_token_hex(bytesKey)
                          << " detokenized as " << format_detok_string_for_log(rawEscape) << "!" << std::endl;
                if (prettify) {
                    str += rawEscape;
                } else {
                    accept_detok_token(rawEscape, currentRawBytes);
                }
            }
        }

        if (prettify)
        {
            str = prettify_token_string(str);
        }

        if (options.contains("reindent") && options.at("reindent") == 1)
        {
            options_t indent_options{};
            if (options.contains("indent_char"))
                indent_options["indent_char"] = options.at("indent_char");
            if (options.contains("indent_n"))
                indent_options["indent_n"] = options.at("indent_n");
            str = reindentCodeString(str, indent_options);
        }

        return str;
    }

    TIVarFileMinVersionByte TH_Tokenized::getMinVersionFromData(const data_t& data)
    {
        ensure_tokens_initialized();

        const size_t dataSize = data.size();
        if (dataSize < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes (size fields)");
        }

        bool usesRTC = false;
        int maxBB = -1;
        int maxEF = -1;
        uint16_t offset = 2;
        while (offset < dataSize) {
            const uint8_t firstByte = data[offset++];
            if (is_in_vector(firstByteOfTwoByteTokens, firstByte)) {
                if (offset >= dataSize) {
                    break;
                }
                const uint8_t secondByte = data[offset++];
                if (firstByte == 0xBB) {
                    if (secondByte > maxBB) maxBB = secondByte;
                } else if (firstByte == 0xEF) {
                    if (secondByte <= 0x10) usesRTC = true;
                    if (secondByte > maxEF) maxEF = secondByte;
                }
            }
        }
        uint8_t version = usesRTC ? (VER_NONE | MASK_USES_RTC) : VER_NONE;
        if (maxBB > 0xF5 || maxEF > 0xA6) {
            version = VER_INVALID;
        } else if (maxEF > 0x98) {
            version |= VER_CE_530;
        } else if (maxEF > 0x75) {
            version |= VER_CE_ALL;
        } else if (maxEF > 0x40) {
            version |= VER_84CSE_ALL;
        } else if (maxEF > 0x3E) {
            version |= VER_84P_255MP;
        } else if (maxEF > 0x16) {
            version |= VER_84P_253MP;
        } else if (maxEF > 0x12) {
            version |= VER_84P_230;
        } else if (maxEF > -1) {
            version |= VER_84P_ALL;
        } else if (maxBB > 0xDA) {
            version |= VER_83P_116;
        } else if (maxBB > 0xCE) {
            version |= VER_83P_115;
        } else if (maxBB > 0x67) {
            version |= VER_83P_ALL;
        }
        return (TIVarFileMinVersionByte)version;
    }

    std::string TH_Tokenized::reindentCodeString(const std::string& str_orig, const options_t& options)
    {
        uint8_t lang;
        if (options.contains("lang"))
        {
            lang = options.at("lang");
        } else if (str_orig.size() > 1 && str_orig[0] == '.' && (str_orig[1] == '.' || isalpha(str_orig[1]))) {
            lang = PRGMLANG_AXE;
        } else if (str_orig.substr(0, sizeof("\U0001D456") - 1) == "\U0001D456") {
            lang = PRGMLANG_ICE;
        } else {
            lang = PRGMLANG_BASIC;
        }

        char indent_char = INDENT_CHAR_SPACE;
        if (options.contains("indent_char") && options.at("indent_char") == INDENT_CHAR_TAB)
        {
            indent_char = INDENT_CHAR_TAB;
        }

        size_t indent_n = indent_char == INDENT_CHAR_SPACE ? 3 : 1;
        if (options.contains("indent_n"))
        {
            const size_t wanted_indent_n = options.at("indent_n");
            if (wanted_indent_n != indent_n)
            {
                indent_n = wanted_indent_n;
            }
        }

        std::string str(str_orig);

        split_var_keyword_lines(str, "DelVar ");
        split_var_keyword_lines(str, "EffVar ");

        std::vector<std::string> lines_tmp = explode(str, '\n');

        // Inplace-replace the appropriate ":" by new-line chars (ie, by inserting the split string in the lines_tmp array)
        for (size_t idx = 0; idx < lines_tmp.size(); idx++)
        {
            const auto line = lines_tmp[idx];
            bool isWithinString = false;
            for (size_t strIdx = 0; strIdx < line.size(); strIdx++)
            {
                const auto& currChar = line.substr(strIdx, 1);
                if (currChar == ":" && !isWithinString)
                {
                    lines_tmp[idx] = line.substr(0, strIdx); // replace "old" line by lhs
                    lines_tmp.insert(lines_tmp.begin() + idx + 1, line.substr(strIdx + 1)); // inserting rhs
                    break;
                } else if (currChar == "\"") {
                    isWithinString = !isWithinString;
                } else if (currChar == "\n" ||
                    source_has_literal_at(line, strIdx, "→") || source_has_literal_at(line, strIdx, "->"))
                {
                    isWithinString = false;
                }
            }
        }

        // Take care of NBSP stuff
        for (auto& line : lines_tmp)
        {
            ltrim_program_whitespace(line);
        }

        std::vector<std::pair<uint16_t, std::string>> lines(lines_tmp.size()); // indent, text
        for (const auto& line : lines_tmp)
        {
            lines.emplace_back(0, line);
        }

        std::vector<std::string> increaseIndentAfter   = { "If", "For", "While", "Repeat" };
        std::vector<std::string> decreaseIndentOfToken = { "Then", "Else", "End", "ElseIf", "EndIf", "End!If" };
        std::vector<std::string> closingTokens         = { "End", "EndIf", "End!If" };
        uint16_t nextIndent = 0;
        std::string oldFirstCommand, firstCommand;
        for (auto& [indent, text] : lines)
        {
            oldFirstCommand = firstCommand;
            firstCommand = get_first_command(text);

            indent = nextIndent;

            if (is_in_vector(increaseIndentAfter, firstCommand))
            {
                nextIndent++;
            }
            if (indent > 0 && is_in_vector(decreaseIndentOfToken, firstCommand))
            {
                indent--;
            }
            if (nextIndent > 0 && (is_in_vector(closingTokens, firstCommand) || (oldFirstCommand == "If" && firstCommand != "Then" && lang != PRGMLANG_AXE && lang != PRGMLANG_ICE)))
            {
                nextIndent--;
            }
        }

        str = "";
        for (const auto& [indent, text] : lines)
        {
            str += std::string(indent * indent_n, indent_char) + text + '\n';
        }

        return ltrim(rtrim(str, "\t\n\r\f\v"));
    }

    std::string TH_Tokenized::tokenToString(const data_t& data, int *incr, const options_t& options)
    {
        ensure_tokens_initialized();

        const size_t dataSize = data.size();

        const uint8_t currentToken = data[0];
        const uint8_t nextToken = dataSize > 1 ? data[1] : (uint8_t)-1;
        uint16_t bytesKey = currentToken;
        const bool is2ByteTok = is_in_vector(firstByteOfTwoByteTokens, currentToken);

        if (incr) {
            *incr = is2ByteTok ? 2 : 1;
        }

        const uint8_t langIdx = (options.contains("lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;
        const bool fromPrettified = options.contains("prettify") && options.at("prettify") == 1;

        if (is2ByteTok)
        {
            if (nextToken == (uint8_t)-1)
            {
                std::cerr << "[Warning] Encountered an unfinished two-byte token!" << std::endl;
                return std::string();
            }
            bytesKey = nextToken + (currentToken << 8);
        }

        std::string tokStr = get_detok_primary_string(bytesKey, langIdx, options);
        if (fromPrettified)
        {
            tokStr = prettify_token_string(tokStr);
        }

        return tokStr;
    }

    std::string TH_Tokenized::oneTokenBytesToString(uint16_t tokenBytes)
    {
        ensure_tokens_initialized();

        if (tokenBytes < 0xFF && is_in_vector(firstByteOfTwoByteTokens, (uint8_t)(tokenBytes & 0xFF)))
        {
            std::cerr << "[Warning] Encountered an unfinished two-byte token!" << std::endl;
            return "";
        }

        std::string tokStr = get_detok_primary_string(tokenBytes, LANG_EN, {});

        tokStr = prettify_token_string(tokStr);

        return tokStr;
    }

    TH_Tokenized::token_posinfo TH_Tokenized::getPosInfoAtOffset(const data_t& data, uint16_t byteOffset, const options_t& options)
    {
        ensure_tokens_initialized();

        const size_t dataSize = data.size();

        if (byteOffset >= dataSize)
        {
            throw std::invalid_argument("byteOffset cannot be >= dataSize!");
        }

        if (dataSize >= 2)
        {
            const uint16_t twoFirstBytes = (uint16_t) ((data[1] & 0xFF) + ((data[0] & 0xFF) << 8));
            if (std::ranges::find(squishedASMTokens, twoFirstBytes) != std::end(squishedASMTokens))
            {
                throw std::invalid_argument("This is a squished ASM program - cannot process it!");
            }
        }

        token_posinfo posinfo = { 0, 0, 0 };

        const uint8_t langIdx = (options.contains("lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;
        const bool fromPrettified = options.contains("prettify") && options.at("prettify") == 1;

        // Find line number
        uint16_t lastNewLineOffset = 0;
        for (uint16_t i = 0; i < byteOffset; i++)
        {
            if (data[i] == 0x3F) // newline token code
            {
                posinfo.line++;
                lastNewLineOffset = i;
            }
        }

        // Find column number and token length if byteOffset is reached
        for (uint16_t i = std::max(2, lastNewLineOffset+1); i <= byteOffset; i++)
        {
            const uint8_t currentToken = data[i];
            uint8_t nextToken = (i < dataSize-1) ? data[i+1] : (uint8_t)-1;
            uint16_t bytesKey = currentToken;
            const bool is2ByteTok = is_in_vector(firstByteOfTwoByteTokens, currentToken);
            const uint16_t currIdx = i;

            if (is2ByteTok)
            {
                if (nextToken == (uint8_t)-1)
                {
                    std::cerr << "[Warning] Encountered an unfinished two-byte token! Setting the second byte to 0x00" << std::endl;
                    nextToken = 0x00;
                }
                bytesKey = nextToken + (currentToken << 8);
                i++;
            }

            std::string tokStr = get_detok_primary_string(bytesKey, langIdx, options);
            if (fromPrettified)
            {
                tokStr = prettify_token_string(tokStr);
            }

            const size_t tokStrLen = strlen_mb(tokStr);
            posinfo.column += (uint16_t)tokStrLen;

            if (posinfo.len == 0 && ((currIdx == byteOffset && !is2ByteTok) || (currIdx >= byteOffset-1 && is2ByteTok)))
            {
                posinfo.len = (uint8_t)tokStrLen;
                posinfo.column -= posinfo.len; // column will be the beginning of the token
            }
        }

        return posinfo;
    }

    TH_Tokenized::token_posinfo TH_Tokenized::getPosInfoAtOffsetFromHexStr(const std::string& hexBytesStr, uint16_t byteOffset)
    {
        const size_t strLen = hexBytesStr.length();
        if (strLen % 2 != 0 || strLen > 65500 * 2) // todo: actual len?
        {
            throw std::invalid_argument("invalid hexBytesStr length!");
        }

        data_t data;
        for (size_t i = 0; i < strLen; i += 2) {
            data.push_back((char) strtol(hexBytesStr.substr(i, 2).c_str(), nullptr, 16));
        }

        return getPosInfoAtOffset(data, byteOffset, { { "prettify", 1 } });
    }

    TH_Tokenized::token_posinfo TH_Tokenized::getPosInfoAtOffsetInSourceString(const std::string& sourceStr, uint16_t byteOffset)
    {
        ensure_tokens_initialized();

        if (byteOffset < 2)
        {
            return { 0, 0, 0 };
        }

        token_posinfo posinfo = { 0, 0, 0 };
        uint16_t line = 0;
        uint16_t column = 0;
        uint16_t dataOffset = 2;
        bool found = false;

        scan_source_tokens(sourceStr, true,
                           [&](const std::string& tokenStr, uint16_t tokenValue)
                           {
                               const uint16_t emittedLen = tokenValue > 0xFF ? 2 : 1;
                               if (!found && byteOffset >= dataOffset && byteOffset < dataOffset + emittedLen)
                               {
                                   posinfo = { line, column, (uint8_t)strlen_mb(tokenStr) };
                                   found = true;
                               }

                               dataOffset += emittedLen;
                               advance_source_pos(tokenStr, line, column);
                           },
                           [&](const std::string& skippedStr)
                           {
                               advance_source_pos(skippedStr, line, column);
                           });

        if (!found)
        {
            if (byteOffset >= dataOffset)
            {
                throw std::invalid_argument("byteOffset cannot be >= dataSize!");
            }
            return { 0, 0, 0 };
        }

        return posinfo;
    }

}

#ifdef __EMSCRIPTEN__
    #include <emscripten/bind.h>
    using namespace emscripten;
    EMSCRIPTEN_BINDINGS(_thtokenized) {

        value_object<tivars::TypeHandlers::TH_Tokenized::token_posinfo>("token_posinfo")
            .field("line",   &tivars::TypeHandlers::TH_Tokenized::token_posinfo::line)
            .field("column", &tivars::TypeHandlers::TH_Tokenized::token_posinfo::column)
            .field("len",    &tivars::TypeHandlers::TH_Tokenized::token_posinfo::len);

        function("TH_Tokenized_getPosInfoAtOffsetFromHexStr", &tivars::TypeHandlers::TH_Tokenized::getPosInfoAtOffsetFromHexStr);
        function("TH_Tokenized_getPosInfoAtOffsetInSourceString", &tivars::TypeHandlers::TH_Tokenized::getPosInfoAtOffsetInSourceString);
        function("TH_Tokenized_reindentCodeString", select_overload<std::string(const std::string&, const options_t&)>(&tivars::TypeHandlers::TH_Tokenized::reindentCodeString));
        function("TH_Tokenized_oneTokenBytesToString"       , &tivars::TypeHandlers::TH_Tokenized::oneTokenBytesToString);
    }
#endif
