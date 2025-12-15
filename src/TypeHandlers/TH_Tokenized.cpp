/*
 * Part of tivars_lib_cpp
 * (C) 2015-2025 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
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
#include <regex>
#include <fstream>
#include <cstring>
#include <array>

static size_t strlen_mb(const std::string& s)
{
    size_t len = 0;
    for (const char* p = s.data(); *p != 0; ++p)
        len += (*p & 0xc0) != 0x80;
    return len;
}

namespace tivars::TypeHandlers
{
    // Optional override path for the tokens XML file (set by CLI)
    static std::string g_tokensXMLPathOverride;

    namespace
    {
        std::unordered_map<uint16_t, std::array<std::string, TH_Tokenized::LANG_MAX>> tokens_BytesToName;
        std::unordered_map<std::string, uint16_t> tokens_NameToBytes;
        uint8_t lengthOfLongestTokenName;
        std::vector<uint8_t> firstByteOfTwoByteTokens;
        constexpr uint16_t squishedASMTokens[] = { 0xBB6D, 0xEF69, 0xEF7B }; // 83+/84+, 84+CSE, CE
        const std::regex toPrettifyRX1(R"(\[\|([a-f])\])");
        const std::regex toPrettifyRX2(R"(\[([prszt])\])");
        const std::regex toPrettifyRX3(R"(\|([uvw]))");

        static inline std::string xml_decode_entities(const std::string& in)
        {
            std::string out;
            out.reserve(in.size());
            for (size_t i = 0; i < in.size(); )
            {
                if (in[i] == '&')
                {
                    if (in.compare(i, 5, "&amp;") == 0)       { out.push_back('&'); i += 5; continue; }
                    if (in.compare(i, 4, "&lt;") == 0)        { out.push_back('<'); i += 4; continue; }
                    if (in.compare(i, 4, "&gt;") == 0)        { out.push_back('>'); i += 4; continue; }
                    if (in.compare(i, 6, "&quot;") == 0)      { out.push_back('"'); i += 6; continue; }
                    if (in.compare(i, 6, "&apos;") == 0)      { out.push_back('\''); i += 6; continue; }
                    if (in.compare(i, 2, "&#") == 0)
                    {
                        size_t semi = in.find(';', i + 2);
                        if (semi != std::string::npos)
                        {
                            std::string num = in.substr(i + 2, semi - (i + 2));
                            int code = 0;
                            try {
                                if (!num.empty() && (num[0] == 'x' || num[0] == 'X')) {
                                    code = (int) strtol(num.c_str() + 1, nullptr, 16);
                                } else {
                                    code = std::stoi(num);
                                }
                                if (code >= 0 && code <= 0x10FFFF)
                                {
                                    // naïve: only support BMP/basic ASCII properly
                                    if (code <= 0x7F)
                                        out.push_back((char)code);
                                    else
                                    {
                                        // encode rudimentary UTF-8
                                        if (code <= 0x7FF) {
                                            out.push_back((char)(0xC0 | ((code >> 6) & 0x1F)));
                                            out.push_back((char)(0x80 | (code & 0x3F)));
                                        } else if (code <= 0xFFFF) {
                                            out.push_back((char)(0xE0 | ((code >> 12) & 0x0F)));
                                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                                            out.push_back((char)(0x80 | (code & 0x3F)));
                                        } else {
                                            out.push_back((char)(0xF0 | ((code >> 18) & 0x07)));
                                            out.push_back((char)(0x80 | ((code >> 12) & 0x3F)));
                                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                                            out.push_back((char)(0x80 | (code & 0x3F)));
                                        }
                                    }
                                }
                            } catch (...) {
                                // ignore, fall through
                            }
                            i = semi + 1;
                            continue;
                        }
                    }
                }
                out.push_back(in[i]);
                i++;
            }
            return out;
        }

        static inline std::string extract_attr(const std::string& tag, const std::string& attr)
        {
            // Use a custom raw-string delimiter to avoid )" inside the pattern
            std::regex rx(attr + R"ATTR(\s*=\s*"([^"]*)")ATTR");
            std::smatch m;
            if (std::regex_search(tag, m, rx) && m.size() > 1)
            {
                return m[1].str();
            }
            return {};
        }

        static inline uint8_t hex_to_byte(const std::string& s)
        {
            return (uint8_t) strtol(s.c_str(), nullptr, 16);
        }

        static inline bool has_prefix(const std::vector<uint8_t>& v, uint8_t b)
        {
            return is_in_vector(v, b);
        }

        // Extract the preferred display name for a given language code from a <token>...</token> block
        static inline std::string get_lang_display_name(const std::string& block, const std::string& langCode)
        {
            std::regex rxLang(std::string(R"(<lang code=\")") + langCode + R"(\"[^>]*>)");
            std::sregex_iterator it(block.begin(), block.end(), rxLang);
            std::sregex_iterator end;
            std::string lastVal;
            std::string preferredParen;
            for (; it != end; ++it)
            {
                const size_t langOpenPos = (size_t)it->position(0);
                const size_t langOpenEnd = langOpenPos + (size_t)it->length(0);
                const std::string langOpenTag = it->str(0);
                std::string disp = extract_attr(langOpenTag, "display");
                std::string val;
                if (!disp.empty())
                {
                    val = xml_decode_entities(disp);
                }
                else
                {
                    size_t langClosePos = block.find("</lang>", langOpenEnd);
                    if (langClosePos == std::string::npos) langClosePos = block.size();
                    const std::string langInner = block.substr(langOpenEnd, langClosePos - langOpenEnd);
                    std::regex rxAcc(R"(<accessible>(.*?)</accessible>)");
                    std::smatch am;
                    if (std::regex_search(langInner, am, rxAcc) && am.size() > 1)
                    {
                        val = xml_decode_entities(am[1].str());
                    }
                }
                if (!val.empty())
                {
                    lastVal = val;
                    if (!preferredParen.empty())
                        continue;
                    if (!val.empty() && val.back() == '(')
                        preferredParen = val;
                }
            }
            return !preferredParen.empty() ? preferredParen : lastVal;
        }

        // Shared XML parsing routine used by both standard and Qt/CEmu builds
        static void parse_tokens_xml_and_register(const std::string& xml)
        {
            using TH_Tokenized::LANG_EN;
            using TH_Tokenized::LANG_FR;

            if (xml.empty()) return;

            // Working copy used to blank out two-byte sections so that top-level tokens parsing doesn't see them
            std::string content(xml);

            auto registerAlias = [](uint16_t bytes, const std::string& alias) {
                if (alias.empty()) return;
                const std::string aliasDec = xml_decode_entities(alias);
                if (aliasDec.empty()) return;
                tokens_NameToBytes[aliasDec] = bytes;
                if (aliasDec.length() > lengthOfLongestTokenName)
                    lengthOfLongestTokenName = (uint8_t)aliasDec.length();
            };

            // Pass 1: parse <two-byte value="$PP"> ... </two-byte>
            size_t pos = 0;
            while (true)
            {
                size_t open = content.find("<two-byte", pos);
                if (open == std::string::npos) break;
                size_t openEnd = content.find('>', open);
                if (openEnd == std::string::npos) break;
                std::string openTag = content.substr(open, openEnd - open + 1);
                std::string prefixAttr = extract_attr(openTag, "value"); // like $BB
                uint8_t prefix = 0;
                if (prefixAttr.size() >= 3 && prefixAttr[0] == '$')
                    prefix = hex_to_byte(prefixAttr.substr(1));
                if (!has_prefix(firstByteOfTwoByteTokens, prefix))
                    firstByteOfTwoByteTokens.push_back(prefix);

                size_t close = content.find("</two-byte>", openEnd + 1);
                if (close == std::string::npos) break;
                std::string block = content.substr(openEnd + 1, close - (openEnd + 1));

                // find all nested <token value="$SS"> ... </token>
                size_t tpos = 0;
                while (true)
                {
                    size_t to = block.find("<token", tpos);
                    if (to == std::string::npos) break;
                    size_t toEnd = block.find('>', to);
                    if (toEnd == std::string::npos) break;
                    std::string tokOpen = block.substr(to, toEnd - to + 1);
                    std::string sval = extract_attr(tokOpen, "value");
                    if (sval.size() >= 3 && sval[0] == '$')
                    {
                        uint8_t second = hex_to_byte(sval.substr(1));
                        uint16_t bytes = (uint16_t)((prefix << 8) | second);

                        size_t tclose = block.find("</token>", toEnd + 1);
                        if (tclose == std::string::npos) break;
                        std::string tokContent = block.substr(toEnd + 1, tclose - (toEnd + 1));
                        std::string nameEN = get_lang_display_name(tokContent, "en");
                        std::string nameFR = get_lang_display_name(tokContent, "fr");
                        if (!nameEN.empty() || !nameFR.empty())
                        {
                            tokens_BytesToName[bytes] = { nameEN, nameFR };
                        }
                        if (!nameEN.empty())
                        {
                            tokens_BytesToName[bytes][LANG_EN] = nameEN;
                            tokens_NameToBytes[nameEN] = bytes;
                        }
                        if (!nameFR.empty())
                        {
                            tokens_BytesToName[bytes][LANG_FR] = nameFR;
                            tokens_NameToBytes[nameFR] = bytes;
                        } else {
                            tokens_BytesToName[bytes][LANG_FR] = nameEN; // fallback
                        }

                        const uint8_t maxLenName = (uint8_t) std::max(nameEN.length(), nameFR.length());
                        if (maxLenName > lengthOfLongestTokenName)
                            lengthOfLongestTokenName = maxLenName;

                        // Also register <variant> and <accessible> as aliases
                        {
                            std::regex rxVar(R"(<variant>(.*?)</variant>)");
                            std::sregex_iterator vit(tokContent.begin(), tokContent.end(), rxVar), vend;
                            for (; vit != vend; ++vit) {
                                registerAlias(bytes, (*vit)[1].str());
                            }
                            std::regex rxAcc(R"(<accessible>(.*?)</accessible>)");
                            std::sregex_iterator ait(tokContent.begin(), tokContent.end(), rxAcc);
                            for (; vit != vend; ++vit) {
                                registerAlias(bytes, (*vit)[1].str());
                            }
                        }
                        tpos = tclose + 8;
                    } else {
                        tpos = toEnd + 1;
                    }
                }

                // blank out processed section to avoid picking nested tokens on next pass
                std::fill(content.begin() + (long)open, content.begin() + (long)(close + strlen("</two-byte>")), ' ');
                pos = close + strlen("</two-byte>");
            }

            // Pass 2: parse top-level single-byte <token value="$XX"> ... </token>
            size_t spos = 0;
            while (true)
            {
                size_t to = content.find("<token", spos);
                if (to == std::string::npos) break;
                size_t toEnd = content.find('>', to);
                if (toEnd == std::string::npos) break;
                std::string tokOpen = content.substr(to, toEnd - to + 1);
                std::string sval = extract_attr(tokOpen, "value");
                size_t tclose = content.find("</token>", toEnd + 1);
                if (tclose == std::string::npos) break;
                std::string tokContent = content.substr(toEnd + 1, tclose - (toEnd + 1));
                if (sval.size() >= 3 && sval[0] == '$')
                {
                    uint8_t single = hex_to_byte(sval.substr(1));
                    if (single == 0x00) { spos = tclose + 8; continue; }
                    uint16_t bytes = single;
                    std::string nameEN = get_lang_display_name(tokContent, "en");
                    std::string nameFR = get_lang_display_name(tokContent, "fr");
                    if (!nameEN.empty() || !nameFR.empty())
                    {
                        tokens_BytesToName[bytes] = { nameEN, nameFR };
                    }
                    if (!nameEN.empty())
                    {
                        tokens_BytesToName[bytes][LANG_EN] = nameEN;
                        tokens_NameToBytes[nameEN] = bytes;
                    }
                    if (!nameFR.empty())
                    {
                        tokens_BytesToName[bytes][LANG_FR] = nameFR;
                        tokens_NameToBytes[nameFR] = bytes;
                    } else {
                        tokens_BytesToName[bytes][LANG_FR] = nameEN; // fallback
                    }
                    // Also register aliases for one-byte tokens
                    {
                        std::regex rxVar(R"(<variant>(.*?)</variant>)");
                        std::sregex_iterator vit(tokContent.begin(), tokContent.end(), rxVar), vend;
                        for (; vit != vend; ++vit) {
                            registerAlias(bytes, (*vit)[1].str());
                        }
                        std::regex rxAcc(R"(<accessible>(.*?)</accessible>)");
                        std::sregex_iterator ait(tokContent.begin(), tokContent.end(), rxAcc);
                        for (; ait != vend; ++ait) {
                            registerAlias(bytes, (*ait)[1].str());
                        }
                    }
                }
                spos = tclose + 8;
            }
        }
    }

    data_t TH_Tokenized::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)_ctx;
        data_t data;

        const bool deindent = has_option(options, "deindent") && options.at("deindent") == 1;
        const bool detect_strings = !has_option(options, "detect_strings") || options.at("detect_strings") != 0;

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

                if (tokens_NameToBytes.count(currentSubString))
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
        (void)_ctx;
        const size_t dataSize = data.size();

        const bool fromRawBytes = has_option(options, "fromRawBytes") && options.at("fromRawBytes") == 1;
        if (fromRawBytes && dataSize == 0)
        {
            return "";
        }
        if (!fromRawBytes && dataSize < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes (size fields)");
        }

        const uint8_t langIdx = (has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;

        const size_t howManyBytes = fromRawBytes ? (int)data.size() : ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        if (!fromRawBytes)
        {
            if (howManyBytes != dataSize - 2)
            {
                std::cerr << "[Warning] Byte count (" << (dataSize - 2) << ") and size field (" << howManyBytes  << ") mismatch!" << std::endl;
            }
        }

        if (!fromRawBytes && howManyBytes >= 2 && dataSize >= 4)
        {
            const uint16_t twoFirstBytes = (uint16_t) ((data[3] & 0xFF) + ((data[2] & 0xFF) << 8));
            if (std::find(std::begin(squishedASMTokens), std::end(squishedASMTokens), twoFirstBytes) != std::end(squishedASMTokens))
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
            if (tokens_BytesToName.find(bytesKey) != tokens_BytesToName.end())
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

        if (has_option(options, "prettify") && options.at("prettify") == 1)
        {
            str = std::regex_replace(str, toPrettifyRX1, "$1");
            str = std::regex_replace(str, toPrettifyRX2, "$1");
            str = std::regex_replace(str, toPrettifyRX3, "$1");
        }

        if (has_option(options, "reindent") && options.at("reindent") == 1)
        {
            options_t indent_options{};
            if (has_option(options, "indent_char"))
                indent_options["indent_char"] = options.at("indent_char");
            if (has_option(options, "indent_n"))
                indent_options["indent_n"] = options.at("indent_n");
            str = reindentCodeString(str, indent_options);
        }

        return str;
    }

    uint8_t TH_Tokenized::getMinVersionFromData(const data_t& data)
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
                uint8_t secondByte = data[offset++];
                if (firstByte == 0xBB) {
                    if (secondByte > maxBB) maxBB = secondByte;
                } else if (firstByte == 0xEF) {
                    if (secondByte <= 0x10) usesRTC = true;
                    if (secondByte > maxEF) maxEF = secondByte;
                }
            }
        }
        uint8_t version = usesRTC ? 0x20 : 0x00;
        if (maxBB > 0xF5 || maxEF > 0xA6) {
            version = 0xFF;
        } else if (maxEF > 0x98) {
            version |= 0x0C;
        } else if (maxEF > 0x75) {
            version |= 0x0B;
        } else if (maxEF > 0x40) {
            version |= 0x0A;
        } else if (maxEF > 0x3E) {
            version |= 0x07;
        } else if (maxEF > 0x16) {
            version |= 0x06;
        } else if (maxEF > 0x12) {
            version |= 0x05;
        } else if (maxEF > -1) {
            version |= 0x04;
        } else if (maxBB > 0xDA) {
            version |= 0x03;
        } else if (maxBB > 0xCE) {
            version |= 0x02;
        } else if (maxBB > 0x67) {
            version |= 0x01;
        }
        return version;
    }

    std::string TH_Tokenized::reindentCodeString(const std::string& str_orig, const options_t& options)
    {
        uint8_t lang;
        if (has_option(options, "lang"))
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
        if (has_option(options, "indent_char") && options.at("indent_char") == INDENT_CHAR_TAB)
        {
            indent_char = INDENT_CHAR_TAB;
        }

        size_t indent_n = indent_char == INDENT_CHAR_SPACE ? 3 : 1;
        if (has_option(options, "indent_n"))
        {
            const size_t wanted_indent_n = options.at("indent_n");
            if (wanted_indent_n != indent_n)
            {
                indent_n = wanted_indent_n;
            }
        }

        std::string str(str_orig);

        str = std::regex_replace(str, std::regex("([^\\s])(Del|Eff)Var "), "$1\n$2Var ");

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
            line = std::regex_replace(line, std::regex("^[\u00A0\uC2A0]*\\s*[\u00A0\uC2A0]*"), "");
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
        for (auto& line : lines)
        {
            oldFirstCommand = firstCommand;

            std::string trimmedLine = trim(line.second);
            if (!trimmedLine.empty()) {
                char* trimmedLine_c = (char*) trimmedLine.c_str();
                firstCommand = strtok(trimmedLine_c, " ");
                firstCommand = trim(firstCommand);
                trimmedLine = std::string(trimmedLine_c);
                if (firstCommand == trimmedLine)
                {
                    firstCommand = strtok((char*)trimmedLine.c_str(), "(");
                    firstCommand = trim(firstCommand);
                }
            } else {
                firstCommand = "";
            }

            line.first = nextIndent;

            if (is_in_vector(increaseIndentAfter, firstCommand))
            {
                nextIndent++;
            }
            if (line.first > 0 && is_in_vector(decreaseIndentOfToken, firstCommand))
            {
                line.first--;
            }
            if (nextIndent > 0 && (is_in_vector(closingTokens, firstCommand) || (oldFirstCommand == "If" && firstCommand != "Then" && lang != PRGMLANG_AXE && lang != PRGMLANG_ICE)))
            {
                nextIndent--;
            }
        }

        str = "";
        for (const auto& line : lines)
        {
            str += std::string(line.first * indent_n, indent_char) + line.second + '\n';
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

        const uint8_t langIdx = (has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;
        const bool fromPrettified = has_option(options, "prettify") && options.at("prettify") == 1;

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
        if (tokens_BytesToName.find(bytesKey) != tokens_BytesToName.end())
        {
            tokStr = tokens_BytesToName[bytesKey][langIdx];
        } else {
            tokStr = " [???] ";
        }
        if (fromPrettified)
        {
            tokStr = std::regex_replace(tokStr, toPrettifyRX1, "$1");
            tokStr = std::regex_replace(tokStr, toPrettifyRX2, "$1");
            tokStr = std::regex_replace(tokStr, toPrettifyRX3, "$1");
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
        if (tokens_BytesToName.find(tokenBytes) != tokens_BytesToName.end())
        {
            tokStr = tokens_BytesToName[tokenBytes][LANG_EN];
        } else {
            tokStr = " [???] ";
        }

        tokStr = std::regex_replace(tokStr, toPrettifyRX1, "$1");
        tokStr = std::regex_replace(tokStr, toPrettifyRX2, "$1");
        tokStr = std::regex_replace(tokStr, toPrettifyRX3, "$1");

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
            if (std::find(std::begin(squishedASMTokens), std::end(squishedASMTokens), twoFirstBytes) != std::end(squishedASMTokens))
            {
                throw std::invalid_argument("This is a squished ASM program - cannot process it!");
            }
        }

        token_posinfo posinfo = { 0, 0, 0 };

        const uint8_t langIdx = (has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN;
        const bool fromPrettified = has_option(options, "prettify") && options.at("prettify") == 1;

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
            if (tokens_BytesToName.find(bytesKey) != tokens_BytesToName.end())
            {
                tokStr = tokens_BytesToName[bytesKey][langIdx];
            } else {
                tokStr = " [???] ";
            }
            if (fromPrettified)
            {
                tokStr = std::regex_replace(tokStr, toPrettifyRX1, "$1");
                tokStr = std::regex_replace(tokStr, toPrettifyRX2, "$1");
                tokStr = std::regex_replace(tokStr, toPrettifyRX3, "$1");
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
        // Reset containers
        tokens_BytesToName.clear();
        tokens_NameToBytes.clear();
        firstByteOfTwoByteTokens.clear();
        lengthOfLongestTokenName = 0;

#ifndef CEMU_VERSION
        // Load from XML file on disk
        const std::string xmlPath = !g_tokensXMLPathOverride.empty() ? g_tokensXMLPathOverride : std::string("ti-toolkit-8x-tokens.xml");
        std::ifstream t(xmlPath);
        const std::string xmlStr((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
        if (xmlStr.empty()) {
            return;
        }
        parse_tokens_xml_and_register(xmlStr);
#else
        // In Qt/CEmu version, try loading XML from resources
        std::string xmlFileStr;
        QFile inputFile(QStringLiteral(":/other/tivars_lib_cpp/ti-toolkit-8x-tokens.xml"));
        if (inputFile.open(QIODevice::ReadOnly))
        {
            xmlFileStr = inputFile.readAll().toStdString();
        }
        if (xmlFileStr.empty())
        {
            return;
        }
        parse_tokens_xml_and_register(xmlFileStr);
#endif

        tokens_BytesToName[0x00] = { "", "" };
    }

    // Public setter to override the XML path before calling initTokens()
    void TH_Tokenized::setTokensXMLPath(const std::string& path)
    {
        g_tokensXMLPathOverride = path;
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
