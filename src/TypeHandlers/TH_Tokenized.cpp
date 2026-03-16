/*
 * Part of tivars_lib_cpp
 * (C) 2015-2025 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../json.hpp"
#include "../TIVarFile.h"
#include "../tivarslib_utils.h"

#ifndef CEMU_VERSION
  #include <iterator>
#else
  #include <QtCore/QFile>
#endif
#include <stdexcept>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <array>

using namespace std::string_literals;
using json = nlohmann::json;

static size_t strlen_mb(const std::string& s)
{
    size_t len = 0;
    for (const char* p = s.data(); *p != 0; ++p)
        len += (*p & 0xc0) != 0x80;
    return len;
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

static data_t hex_to_bytes(const std::string& str)
{
    if (str.size() % 2 != 0)
    {
        throw std::invalid_argument("rawDataHex must contain an even number of hex digits");
    }

    data_t out;
    out.reserve(str.size() / 2);
    for (const char c : str)
    {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
        {
            throw std::invalid_argument("rawDataHex must be valid hexadecimal");
        }
    }
    for (size_t i = 0; i < str.size(); i += 2)
    {
        out.push_back(tivars::hexdec(str.substr(i, 2)));
    }
    return out;
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
        std::unordered_map<uint16_t, std::array<std::string, TH_Tokenized::LANG_MAX>> tokens_BytesToName;
        std::unordered_map<std::string, uint16_t> tokens_NameToBytes;
        uint8_t lengthOfLongestTokenName;
        std::vector<uint8_t> firstByteOfTwoByteTokens;
        constexpr uint16_t squishedASMTokens[] = { 0xBB6D, 0xEF69, 0xEF7B }; // 83+/84+, 84+CSE, CE
    }

    data_t TH_Tokenized::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;
        const std::string trimmed = tivars::trim(str);
        if (!trimmed.empty() && trimmed.front() == '{')
        {
            const json j = json::parse(trimmed);
            if (j.contains("rawDataHex"))
            {
                data_t payload = hex_to_bytes(j.at("rawDataHex").get<std::string>());
                data_t data = {
                    static_cast<uint8_t>(payload.size() & 0xFF),
                    static_cast<uint8_t>((payload.size() >> 8) & 0xFF)
                };
                tivars::vector_append(data, payload);
                return data;
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

        // two bytes reserved for the size. Filled later
        data.push_back(0); data.push_back(0);

        bool isInCustomName = false; // after a "prgm" or ʟ token (
        bool isWithinString = false;
        bool inEvaluatedString = false; // CE OS 5.2 added string interpolation with eval() for TI-Innovator commands
        uint16_t lastTokenBytes = 0;

        for (size_t strCursorPos = 0; strCursorPos < str_new.length(); strCursorPos++)
        {
            const std::string currChar = str_new.substr(strCursorPos, 1);
            if(detect_strings)
            {
                if((lastTokenBytes == 0x5F || lastTokenBytes == 0xEB)) { // prgm and ʟ
                    isInCustomName = true;
                } else if(currChar == "\"") {
                    isWithinString = !isWithinString;
                    inEvaluatedString = isWithinString && lastTokenBytes == 0xE7; // Send(
                } else if(currChar == "\n" || (strCursorPos < str_new.length()-strlen("→") && memcmp(&str_new[strCursorPos], "→", strlen("→")) == 0)) {
                    isInCustomName = false;
                    isWithinString = false;
                    inEvaluatedString = false;
                } else if(isInCustomName && !isalnum(currChar[0])) {
                    isInCustomName = false;
                }
            }

            const uint8_t maxTokSearchLen = std::min(str_new.length() - strCursorPos, (size_t)lengthOfLongestTokenName);
            const bool needMinMunch = isInCustomName || (isWithinString && !inEvaluatedString);

            /* needMinMunch => minimum token length, otherwise maximal munch */
            for (size_t currentLength = needMinMunch ? 1 : maxTokSearchLen;
                 needMinMunch ? (currentLength <= maxTokSearchLen) : (currentLength > 0);
                 currentLength += (needMinMunch ? 1 : -1))
            {
                std::string currentSubString = str_new.substr(strCursorPos, currentLength);

                // We want to use true-lowercase alpha tokens in this case.
                if ((isWithinString && !inEvaluatedString) && currentLength == 1 && std::islower(currentSubString[0]))
                {
                    // 0xBBB0 is 'a', etc. But we skip what would be 'l' at 0xBBBB which doesn't exist (prefix conflict)
                    const char letter = currentSubString[0];
                    uint16_t tokenValue = 0xBBB0 + (letter - 'a') + (letter >= 'l' ? 1 : 0);
                    data.push_back(tokenValue >> 8);
                    data.push_back(tokenValue & 0xFF);
                    lastTokenBytes = tokenValue;
                    break;
                }

                if (tokens_NameToBytes.contains(currentSubString))
                {
                    uint16_t tokenValue = tokens_NameToBytes[currentSubString];
                    if (tokenValue > 0xFF)
                    {
                        data.push_back((uint8_t)(tokenValue >> 8));
                    }
                    data.push_back((uint8_t)(tokenValue & 0xFF));
                    strCursorPos += currentLength - 1;
                    lastTokenBytes = tokenValue;
                    break;
                }
            }
        }

        size_t actualDataLen = data.size() - 2;
        data[0] = (uint8_t)(actualDataLen & 0xFF);
        data[1] = (uint8_t)((actualDataLen >> 8) & 0xFF);
        return data;
    }

    std::string TH_Tokenized::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
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

        uint16_t errCount = 0;
        std::string str;
        for (size_t i = fromRawBytes ? 0 : 2; i < dataSize; i++)
        {
            const uint8_t currentToken = data[i];
            uint8_t nextToken = (i < dataSize-1) ? data[i+1] : (uint8_t)-1;
            uint16_t bytesKey = currentToken;
            if (is_in_vector(firstByteOfTwoByteTokens, currentToken))
            {
                if (nextToken == (uint8_t)-1)
                {
                    std::cerr << "[Warning] Encountered an unfinished two-byte token! Setting the second byte to 0x00" << std::endl;
                    nextToken = 0x00;
                }
                bytesKey = nextToken + (currentToken << 8);
                i++;
            }
            if (tokens_BytesToName.contains(bytesKey))
            {
                str += tokens_BytesToName[bytesKey][langIdx];
            } else {
                str += " [???] ";
                errCount++;
            }
        }

        if (errCount > 0)
        {
            std::cerr << "[Warning] " << errCount << " token(s) could not be detokenized (' [???] ' was used)!" << std::endl;
        }

        if (options.contains("prettify") && options.at("prettify") == 1)
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
                } else if (currChar == "\n" || (strIdx < line.length()-strlen("→") && memcmp(&line[strIdx], "→", strlen("→")) == 0)) {
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

        std::string tokStr;
        if (tokens_BytesToName.contains(bytesKey))
        {
            tokStr = tokens_BytesToName[bytesKey][langIdx];
        } else {
            tokStr = " [???] ";
        }
        if (fromPrettified)
        {
            tokStr = prettify_token_string(tokStr);
        }

        return tokStr;
    }

    std::string TH_Tokenized::oneTokenBytesToString(uint16_t tokenBytes)
    {
        if (tokenBytes < 0xFF && is_in_vector(firstByteOfTwoByteTokens, (uint8_t)(tokenBytes & 0xFF)))
        {
            std::cerr << "[Warning] Encountered an unfinished two-byte token!" << std::endl;
            return "";
        }

        std::string tokStr;
        if (tokens_BytesToName.contains(tokenBytes))
        {
            tokStr = tokens_BytesToName[tokenBytes][LANG_EN];
        } else {
            tokStr = " [???] ";
        }

        tokStr = prettify_token_string(tokStr);

        return tokStr;
    }

    TH_Tokenized::token_posinfo TH_Tokenized::getPosInfoAtOffset(const data_t& data, uint16_t byteOffset, const options_t& options)
    {
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

            std::string tokStr;
            if (tokens_BytesToName.contains(bytesKey))
            {
                tokStr = tokens_BytesToName[bytesKey][langIdx];
            } else {
                tokStr = " [???] ";
            }
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

    void TH_Tokenized::initTokens()
    {
#ifndef CEMU_VERSION
        initTokensFromCSVFilePath("programs_tokens.csv");
#else
        std::string csvFileStr;
        QFile inputFile(QStringLiteral(":/other/tivars_lib_cpp/programs_tokens.csv"));
        if (inputFile.open(QIODevice::ReadOnly))
        {
            csvFileStr = inputFile.readAll().toStdString();
        }

        initTokensFromCSVContent(csvFileStr);
#endif
    }

    void TH_Tokenized::initTokensFromCSVFilePath(const std::string& csvFilePath)
    {
        std::ifstream t(csvFilePath);
        const std::string csvFileStr((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        initTokensFromCSVContent(csvFileStr);
    }

    void TH_Tokenized::initTokensFromCSVContent(const std::string& csvFileStr)
    {
        if (!csvFileStr.empty())
        {
            std::vector<std::vector<std::string>> lines;
            ParseCSV(csvFileStr, lines);

            for (const auto& tokenInfo : lines)
            {
                uint16_t bytes;
                if (tokenInfo[6] == "2") // number of bytes for the token
                {
                    if (!is_in_vector(firstByteOfTwoByteTokens, hexdec(tokenInfo[7])))
                    {
                        firstByteOfTwoByteTokens.push_back(hexdec(tokenInfo[7]));
                    }
                    bytes = hexdec(tokenInfo[8]) + (hexdec(tokenInfo[7]) << 8);
                } else {
                    bytes = hexdec(tokenInfo[7]);
                }
                tokens_BytesToName[bytes] = { tokenInfo[4], tokenInfo[5] }; // EN, FR
                tokens_NameToBytes[tokenInfo[4]] = bytes; // EN
                tokens_NameToBytes[tokenInfo[5]] = bytes; // FR
                const uint8_t maxLenName = (uint8_t) std::max(tokenInfo[4].length(), tokenInfo[5].length());
                if (maxLenName > lengthOfLongestTokenName)
                {
                    lengthOfLongestTokenName = maxLenName;
                }
            }
        } else {
            throw std::runtime_error("Could not open the tokens CSV file or read its data");
        }
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
        function("TH_Tokenized_oneTokenBytesToString"       , &tivars::TypeHandlers::TH_Tokenized::oneTokenBytesToString);
    }
#endif
