/*
 * Part of tivars_lib_cpp
 * (C) 2015-2019 Adrien "Adriweb" Bertrand
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
#include <regex>
#include <fstream>

namespace tivars
{
    namespace TH_Tokenized
    {
        std::unordered_map<uint, std::vector<std::string>> tokens_BytesToName;
        std::unordered_map<std::string, uint> tokens_NameToBytes;
        uchar lengthOfLongestTokenName;
        std::vector<uchar> firstByteOfTwoByteTokens;
        const uint16_t squishedASMTokens[] = { 0xBB6D, 0xEF69, 0xEF7B }; // 83+/84+, 84+CSE, CE
        const std::regex toPrettifyRX(R"(\[?\|([a-zA-Z]+)\]?)");
    }

    /* TODO: handle TI-Innovator Send( exception for in-strings tokenization (=> not shortest tokens) */
    data_t TH_Tokenized::makeDataFromString(const std::string& str, const options_t& options)
    {
        data_t data;

        const bool detect_strings = !has_option(options, "detect_strings") || options.at("detect_strings") != 0;

        // two bytes reserved for the size. Filled later
        data.push_back(0); data.push_back(0);

        const uchar maxTokSearchLen = std::min((uchar)str.length(), lengthOfLongestTokenName);

        bool isWithinString = false;

        for (uint strCursorPos = 0; strCursorPos < str.length(); strCursorPos++)
        {
            const std::string currChar = str.substr(strCursorPos, 1);
            if(detect_strings)
            {
                if(currChar == "\"") {
                    isWithinString = !isWithinString;
                } else if(currChar == "\n" || currChar == "→") {
                    isWithinString = false;
                }
            }
            /* isWithinString => minimum token length, otherwise maximal munch */
            for (uint currentLength = isWithinString ? 1 : maxTokSearchLen;
                 isWithinString ? (currentLength <= maxTokSearchLen) : (currentLength > 0);
                 currentLength += (isWithinString ? 1 : -1))
            {
                std::string currentSubString = str.substr(strCursorPos, currentLength);
                if (tokens_NameToBytes.count(currentSubString))
                {
                    uint tokenValue = tokens_NameToBytes[currentSubString];
                    if (tokenValue > 0xFF)
                    {
                        data.push_back((uchar)(tokenValue >> 8));
                    }
                    data.push_back((uchar)(tokenValue & 0xFF));
                    strCursorPos += currentLength - 1;
                    break;
                }
            }
        }

        uint actualDataLen = (uint) (data.size() - 2);
        data[0] = (uchar)(actualDataLen & 0xFF);
        data[1] = (uchar)((actualDataLen >> 8) & 0xFF);
        return data;
    }

    std::string TH_Tokenized::makeStringFromData(const data_t& data, const options_t& options)
    {
        const size_t dataSize = data.size();

        const bool fromRawBytes = has_option(options, "fromRawBytes") && options.at("fromRawBytes") == 1;
        if (!fromRawBytes && dataSize < 2)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain at least 2 bytes (size fields)");
        }

        uint langIdx = (uint)((has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN);

        const int howManyBytes = fromRawBytes ? (int)data.size() : ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
        if (!fromRawBytes)
        {
            if (howManyBytes != (int)dataSize - 2)
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

        uint errCount = 0;
        std::string str;
        for (uint i = fromRawBytes ? 0 : 2; i < (uint)dataSize; i++)
        {
            uint currentToken = data[i];
            uint nextToken = (i < dataSize-1) ? data[i+1] : (uint)-1;
            uint bytesKey = currentToken;
            if (is_in_vector(firstByteOfTwoByteTokens, (uchar)currentToken))
            {
                if (nextToken == (uint)-1)
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
            str = reindentCodeString(str);
        }

        return str;
    }

    std::string TH_Tokenized::reindentCodeString(const std::string& str_orig, const options_t& options)
    {
        int lang;
        if (has_option(options, "lang"))
        {
            lang = options.at("lang");
        } else if (str_orig.size() > 1 && str_orig[0] == '.' && (str_orig[1] == '.' || ::isalpha(str_orig[1]))) {
            lang = PRGMLANG_AXE;
        } else if (str_orig.substr(0, sizeof("\U0001D456") - 1) == "\U0001D456") {
            lang = PRGMLANG_ICE;
        } else {
            lang = PRGMLANG_BASIC;
        }

        std::string str(str_orig);

        str = std::regex_replace(str, std::regex("([^\\s])(Del|Eff)Var "), "$1\n$2Var ");

        std::vector<std::string> lines_tmp = explode(str, '\n');

        // Inplace-replace the appropriate ":" by new-line chars (ie, by inserting the split string in the lines_tmp array)
        for (uint16_t idx = 0; idx < (uint16_t)lines_tmp.size(); idx++)
        {
            const auto line = lines_tmp[idx];
            bool isWithinString = false;
            for (uint16_t strIdx = 0; strIdx < (uint16_t)line.size(); strIdx++)
            {
                const auto currChar = line.substr(strIdx, 1);
                if (currChar == ":" && !isWithinString)
                {
                    lines_tmp[idx] = line.substr(0, strIdx); // replace "old" line by lhs
                    lines_tmp.insert(lines_tmp.begin() + idx + 1, line.substr(strIdx + 1)); // inserting rhs
                    break;
                } else if (currChar == "\"") {
                    isWithinString = !isWithinString;
                } else if (currChar == "\n" || currChar == "→") {
                    isWithinString = false;
                }
            }
        }

        std::vector<std::pair<uint, std::string>> lines(lines_tmp.size()); // indent, text
        for (const auto& line : lines_tmp)
        {
            lines.emplace_back(0, line);
        }

        std::vector<std::string> increaseIndentAfter   = { "If", "For", "While", "Repeat" };
        std::vector<std::string> decreaseIndentOfToken = { "Then", "Else", "End", "ElseIf", "EndIf", "End!If" };
        std::vector<std::string> closingTokens         = { "End", "EndIf", "End!If" };
        uint nextIndent = 0;
        std::string oldFirstCommand, firstCommand;
        for (auto& line : lines)
        {
            oldFirstCommand = firstCommand;

            std::string trimmedLine = trim(line.second);
            if (trimmedLine.length() > 0) {
                char* trimmedLine_c = (char*) trimmedLine.c_str();
                firstCommand = strtok(trimmedLine_c, " ");
                firstCommand = trim(firstCommand);
                trimmedLine = std::string(trimmedLine_c);
                trimmedLine_c = (char*) trimmedLine.c_str();
                if (firstCommand == trimmedLine)
                {
                    firstCommand = strtok(trimmedLine_c, "(");
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
            str += str_repeat(" ", line.first * 3) + line.second + '\n';
        }

        return ltrim(rtrim(str, "\t\n\r\f\v"));
    }

    std::string TH_Tokenized::tokenToString(const data_t& data, int *incr, const options_t& options)
    {
        const size_t dataSize = data.size();

        uint currentToken = data[0];
        uint nextToken = dataSize > 1 ? data[1] : -1u;
        uint bytesKey = currentToken;
        bool is2ByteTok = is_in_vector(firstByteOfTwoByteTokens, static_cast<uchar>(currentToken));

        if (incr) {
            *incr = is2ByteTok ? 2 : 1;
        }

        uint langIdx = static_cast<uint>((has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN);
        bool fromPrettified = has_option(options, "prettify") && options.at("prettify") == 1;

        if (is2ByteTok)
        {
            if (nextToken == -1u)
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

        TH_Tokenized::token_posinfo posinfo = { 0, 0, 0 };

        uint langIdx = (uint)((has_option(options, "lang") && options.at("lang") == LANG_FR) ? LANG_FR : LANG_EN);
        bool fromPrettified = has_option(options, "prettify") && options.at("prettify") == 1;

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
            uint currentToken = data[i];
            uint nextToken = (i < dataSize-1) ? data[i+1] : (uint)-1;
            uint bytesKey = currentToken;
            bool is2ByteTok = is_in_vector(firstByteOfTwoByteTokens, (uchar)currentToken);
            const uint16_t currIdx = i;

            if (is2ByteTok)
            {
                if (nextToken == (uint)-1)
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
        std::string csvFileStr;

        {
#ifndef CEMU_VERSION
            std::ifstream t("programs_tokens.csv");
            csvFileStr = std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
#else
            QFile inputFile(QStringLiteral(":/other/tivars_lib_cpp/programs_tokens.csv"));
            if (inputFile.open(QIODevice::ReadOnly))
            {
                csvFileStr = inputFile.readAll().toStdString();
            }
#endif
        }

        if (csvFileStr.length() > 0)
        {
            std::vector<std::vector<std::string>> lines;
            ParseCSV(csvFileStr, lines);

            for (const auto& tokenInfo : lines)
            {
                uint bytes;
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
                uchar maxLenName = (uchar) std::max(tokenInfo[4].length(), tokenInfo[5].length());
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
