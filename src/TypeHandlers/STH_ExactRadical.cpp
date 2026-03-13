/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "TypeHandlers.h"
#include "../tivarslib_utils.h"
#include "../TIVarTypes.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <regex>

namespace
{
    void replaceAll(std::string& str, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return;
        }

        for (size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
        {
            str.replace(pos, from.size(), to);
        }
    }

    int parseRadicalPartValue(const std::string& str)
    {
        if (str.empty())
        {
            return 1;
        }

        if (str == "+" || str == "-")
        {
            return std::stoi(str + "1");
        }

        return std::stoi(str);
    }

    std::string formatThreeDigits(int value)
    {
        std::ostringstream stream;
        stream << std::setw(3) << std::setfill('0') << value;
        return stream.str();
    }
}

namespace tivars::TypeHandlers
{

    data_t STH_ExactRadical::makeDataFromString(const std::string& str, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        std::string normalized = str;
        normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }), normalized.end());
        replaceAll(normalized, "*", "");
        replaceAll(normalized, "sqrt", "√");
        replaceAll(normalized, "√(", "√");
        replaceAll(normalized, "(", "");
        replaceAll(normalized, ")", "");

        if (normalized.empty())
        {
            throw std::invalid_argument("Invalid input string. Needs to be a valid Exact Real Radical");
        }
        if (normalized.find('.') != std::string::npos)
        {
            throw std::invalid_argument("Radical type only accepts integer components");
        }

        replaceAll(normalized, "-", "+-");
        if (!normalized.empty() && normalized.front() == '+')
        {
            normalized.erase(0, 1);
        }

        if (normalized.find('/') == std::string::npos)
        {
            normalized += "/1";
        }
        if (normalized.find("√") == std::string::npos)
        {
            normalized = "0√1+" + normalized;
            replaceAll(normalized, "/", "√1/");
        }
        if (normalized.find('+') == std::string::npos)
        {
            replaceAll(normalized, "/", "+0√1/");
        }

        const size_t slashPos = normalized.find('/');
        if (slashPos == std::string::npos || normalized.find('/', slashPos + 1) != std::string::npos)
        {
            throw std::invalid_argument("Invalid radical string");
        }

        const std::string top = normalized.substr(0, slashPos);
        const std::string denominatorString = normalized.substr(slashPos + 1);
        const size_t plusPos = top.find('+');
        if (plusPos == std::string::npos || top.find('+', plusPos + 1) != std::string::npos)
        {
            throw std::invalid_argument("Invalid radical numerator");
        }

        std::string left = top.substr(0, plusPos);
        std::string right = top.substr(plusPos + 1);
        if (left.find("√") == std::string::npos)
        {
            left += "√1";
        }
        if (right.find("√") == std::string::npos)
        {
            right += "√1";
        }

        const size_t leftRootPos = left.find("√");
        const size_t rightRootPos = right.find("√");
        if (leftRootPos == std::string::npos || rightRootPos == std::string::npos)
        {
            throw std::invalid_argument("Invalid radical component");
        }

        int leftScalar = parseRadicalPartValue(left.substr(0, leftRootPos));
        int leftRadicand = std::stoi(left.substr(leftRootPos + std::string("√").size()));
        int rightScalar = parseRadicalPartValue(right.substr(0, rightRootPos));
        int rightRadicand = std::stoi(right.substr(rightRootPos + std::string("√").size()));
        int denominator = std::stoi(denominatorString);

        if (leftRadicand < 0 || rightRadicand < 0)
        {
            throw std::invalid_argument("Square roots cannot be negative");
        }
        if (denominator == 0)
        {
            throw std::invalid_argument("Denominator must be nonzero");
        }

        if (leftRadicand < rightRadicand)
        {
            std::swap(leftScalar, rightScalar);
            std::swap(leftRadicand, rightRadicand);
        }
        else if (leftRadicand == rightRadicand)
        {
            leftScalar += rightScalar;
            rightScalar = 0;
            rightRadicand = 0;
        }

        if (denominator < 0)
        {
            denominator = -denominator;
            leftScalar = -leftScalar;
            rightScalar = -rightScalar;
        }

        uint8_t signType = 0;
        if (leftScalar < 0)
        {
            signType |= 0x1;
        }
        if (rightScalar < 0)
        {
            signType |= 0x2;
        }

        leftScalar = std::abs(leftScalar);
        rightScalar = std::abs(rightScalar);
        if (denominator > 999 || leftScalar > 999 || rightScalar > 999 || leftRadicand > 999 || rightRadicand > 999)
        {
            throw std::invalid_argument("Radical components must fit in three digits");
        }

        const std::string encoded = "00"
                                  + std::string(1, "0123456789ABCDEF"[signType])
                                  + formatThreeDigits(denominator)
                                  + formatThreeDigits(rightScalar)
                                  + formatThreeDigits(leftScalar)
                                  + formatThreeDigits(rightRadicand)
                                  + formatThreeDigits(leftRadicand);

        data_t data(dataByteCount);
        for (size_t i = 0; i < dataByteCount; i++)
        {
            data[i] = hexdec(encoded.substr(i * 2, 2));
        }
        return data;
    }

    // TODO: handle sign bit?
    std::string STH_ExactRadical::makeStringFromData(const data_t& data, const options_t& options, const TIVarFile* _ctx)
    {
        (void)options;
        (void)_ctx;

        if (data.size() != dataByteCount)
        {
            throw std::invalid_argument("Invalid data array. Needs to contain " + std::to_string(dataByteCount) + " bytes");
        }

        std::string dataStr;
        for (size_t i = 0; i < dataByteCount; i++)
        {
            dataStr += dechex(data[i]);
        }

        const auto type = data[0] & ~0x80; // sign bit discarded
        if (type != TIVarType{"ExactRealRadical"}.getId() && type != TIVarType{"ExactComplexRadical"}.getId())
        {
            throw std::invalid_argument("Invalid data bytes - invalid vartype: " + std::to_string(type));
        }

        const auto variant = hexdec(dataStr.substr(2, 1));
        if (variant > 3)
        {
            throw std::invalid_argument("Invalid data bytes - unknown type variant: " + std::to_string(variant));
        }

        const std::vector<std::string> parts = {
            (variant == 1 || variant == 3 ? "-" : "") + trimZeros(dataStr.substr(9, 3)),
            trimZeros(dataStr.substr(15, 3)),
            (variant == 2 || variant == 3 ? "-" : "+") + trimZeros(dataStr.substr(6, 3)),
            trimZeros(dataStr.substr(12, 3)),
            trimZeros(dataStr.substr(3, 3))
        };

        std::string str = "(" + parts[0] + "*√(" + parts[1] + ")" + parts[2] + "*√(" + parts[3]  + "))/" + parts[4];

        // Improve final display
        str = std::regex_replace(str, std::regex("\\+1\\*"), "+");  str = std::regex_replace(str, std::regex("\\(1\\*"),  "(");
        str = std::regex_replace(str, std::regex("-1\\*"),   "-");  str = std::regex_replace(str, std::regex("\\(-1\\*"), "(-");
        str = std::regex_replace(str, std::regex("\\+-"),    "-");

        return str;
    }

    uint8_t STH_ExactRadical::getMinVersionFromData(const data_t& data)
    {
        // handled in TH_GenericXXX
        (void)data;
        return 0;
    }
}
