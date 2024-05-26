/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
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

namespace tivars
{
    namespace TH_Tokenized
    {
        std::unordered_map<uint16_t, std::vector<std::string>> tokens_BytesToName;
        std::unordered_map<std::string, uint16_t> tokens_NameToBytes;
        uint8_t lengthOfLongestTokenName;
        std::vector<uint8_t> firstByteOfTwoByteTokens;
        constexpr uint16_t squishedASMTokens[] = { 0xBB6D, 0xEF69, 0xEF7B }; // 83+/84+, 84+CSE, CE
        const std::regex toPrettifyRX(R"(\[?\|([a-zA-Z]+)\]?)");
    }

    data_t TH_Tokenized::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
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
                std::cerr << "[Warning] Byte count (" << (dataSize - 2) << ") and size field (" << howManyBytes  << ") mismatch!";
            }
        }

        if (!fromRawBytes && howManyBytes >= 2 && dataSize >= 4)
        {
            const uint16_t twoFirstBytes = (uint16_t) ((data[3] & 0xFF) + ((data[2] & 0xFF) << 8));
            if (std::find(std::begin(squishedASMTokens), std::end(squishedASMTokens), twoFirstBytes) != std::end(squishedASMTokens))
            {
                return "[Error] This is a squished ASM program - cannnot preview it!";
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
                    std::cerr << "[Warning] Encountered an unfinished two-byte token! Setting the second byte to 0x00";
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
            std::cerr << "[Warning] " << errCount << " token(s) could not be detokenized (' [???] ' was used)!";
        }

        if (has_option(options, "prettify") && options.at("prettify") == 1)
        {
            str = std::regex_replace(str, toPrettifyRX, "$1");
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
                std::cerr << "[Warning] Encountered an unfinished two-byte token!";
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
            tokStr = std::regex_replace(tokStr, toPrettifyRX, "$1");
        }

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
                throw std::invalid_argument("This is a squished ASM program - cannnot process it!");
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
        for (uint16_t i = lastNewLineOffset+1; i <= byteOffset; i++)
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
                    std::cerr << "[Warning] Encountered an unfinished two-byte token! Setting the second byte to 0x00";
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
                tokStr = std::regex_replace(tokStr, toPrettifyRX, "$1");
            }

            posinfo.column += (uint16_t)tokStr.size();

            if (posinfo.len == 0 && ((currIdx == byteOffset && !is2ByteTok) || (currIdx == byteOffset-1 && is2ByteTok)))
            {
                posinfo.len = (uint8_t)tokStr.size();
                posinfo.column -= posinfo.len; // column will be the beginning of the token
            }
        }

        return posinfo;
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
