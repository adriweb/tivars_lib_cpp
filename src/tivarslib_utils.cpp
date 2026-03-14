/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "tivarslib_utils.h"
#include <sstream>
#include <cmath>
#include <cstring>

using namespace std::string_literals;

namespace tivars
{

unsigned char hexdec(const std::string& str)
{
    return (unsigned char) stoul(str, nullptr, 16);
}

std::string dechex(unsigned char i, bool zeropad)
{
    std::string str = "00";
    snprintf(&str[0], 3, zeropad ? "%02X" : "%X", i);
    return str;
}

std::string strtoupper(const std::string& str)
{
    std::string newStr(str);
    for (char& c : newStr)
    {
        c = (char) toupper(c);
    }
    return newStr;
}

void replace_all(std::string& str, const std::string& from, const std::string& to)
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

std::vector<std::string> explode(const std::string& str, const std::string& delim)
{
    std::vector<std::string> result;

    size_t last = 0;
    size_t next = 0;
    while ((next = str.find(delim, last)) != std::string::npos)
    {
        result.push_back(str.substr(last, next - last));
        last = next + delim.length();
    }
    result.push_back(str.substr(last));

    return result;
}

std::vector<std::string> explode(const std::string& str, char delim)
{
    return explode(str, std::string(1, delim));
}

// trim from start
std::string ltrim(std::string s, const char* t)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from end
std::string rtrim(std::string s, const char* t)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from both ends
std::string trim(const std::string& s, const char* t)
{
    return ltrim(rtrim(s, t), t);
}

// From http://stackoverflow.com/a/2481126/378298
void ParseCSV(const std::string& csvSource, std::vector<std::vector<std::string>>& lines)
{
    bool inQuote(false);
    bool newLine(false);
    std::string field;
    lines.clear();
    std::vector<std::string> line;

    auto aChar = csvSource.begin();

    while (aChar != csvSource.end())
    {
        auto tmp = aChar;
        switch (*aChar)
        {
            case '"':
                newLine = false;
                // Handle weird escaping of quotes ("""" => ")
                if (*(tmp+1) == '"' && *(tmp+2) == '"' && *(tmp+3) == '"') {
                    field.push_back(*aChar);
                    aChar += 3;
                } else {
                    inQuote = !inQuote;
                }
                break;

            case ',':
                newLine = false;
                if (inQuote)
                {
                    field += *aChar;
                }
                else
                {
                    line.push_back(field);
                    field.clear();
                }
                break;

            case '\n':
            case '\r':
                if (inQuote)
                {
                    field += *aChar;
                }
                else
                {
                    if (!newLine)
                    {
                        line.push_back(field);
                        lines.push_back(line);
                        field.clear();
                        line.clear();
                        newLine = true;
                    }
                }
                break;

            default:
                newLine = false;
                field.push_back(*aChar);
                break;
        }

        ++aChar;
    }

    if (!field.empty())
        line.push_back(field);

    if (!line.empty())
        lines.push_back(line);
}

bool is_numeric(const std::string& str)
{
    char* p;
    const double ignored = strtod(str.c_str(), &p);
    (void)ignored;
    return !*p;
}

bool file_exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (FILE *file = fopen(path.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}

std::string str_pad(const std::string& str, unsigned long pad_length, const std::string& pad_string)
{
    unsigned long i, x;
    const unsigned long str_size = str.size();
    const unsigned long pad_size = pad_string.size();

    if (pad_length <= str_size || pad_size < 1)
    {
        return str;
    }

    std::string o;
    o.reserve(pad_length);

    for (i = 0, x = str_size; i < x; i++)
    {
        o.push_back(str[i]);
    }
    for (i = str_size; i < pad_length;)
    {
        for (unsigned long j = 0; j < pad_size && i < pad_length; j++, i++)
        {
            o.push_back(pad_string[j]);
        }
    }

    return o;
}

std::string multiple(int num, const std::string &var) {
    const std::string unit = var.empty() ? "1" : var;
    switch (num) {
        case 0:
            return "0";
        case 1:
            return unit;
        case -1:
            return "-" + unit;
        default:
            return std::to_string(num) + var;
    }
}

// Adapted from http://stackoverflow.com/a/32903747/378298
std::string dec2frac(double num, const std::string& var, double err)
{
    if (err <= 0.0 || err >= 1.0)
    {
        err = 0.001;
    }

    const int sign = ( num > 0 ) ? 1 : ( ( num < 0 ) ? -1 : 0 );

    if (sign == -1)
    {
        num = std::abs(num);
    }

    if (sign != 0)
    {
        // err is the maximum relative err; convert to absolute
        err *= num;
    }

    const int n = (int) std::floor(num);
    num -= n;

    if (num < err)
    {
        return multiple(sign * n, var);
    }

    if (1 - err < num)
    {
        return multiple(sign * (n + 1), var);
    }

    // The lower fraction is 0/1
    int lower_n = 0;
    int lower_d = 1;

    // The upper fraction is 1/1
    int upper_n = 1;
    int upper_d = 1;

    while (true)
    {
        // The middle fraction is (lower_n + upper_n) / (lower_d + upper_d)
        const int middle_n = lower_n + upper_n;
        const int middle_d = lower_d + upper_d;

        if (middle_d * (num + err) < middle_n)
        {
            // real + err < middle : middle is our new upper
            upper_n = middle_n;
            upper_d = middle_d;
        }
        else if (middle_n < (num - err) * middle_d)
        {
            // middle < real - err : middle is our new lower
            lower_n = middle_n;
            lower_d = middle_d;
        } else {
            // Middle is our best fraction
            return multiple((n * middle_d + middle_n) * sign, var) + "/" + std::to_string(middle_d);
        }
    }
}

std::string trimZeros(const std::string& str)
{
    return std::to_string(std::stoi(str));
}

std::string entry_name_to_string(const TIVarType& type, const uint8_t* nameBytes, size_t size)
{
    if (nameBytes == nullptr || size == 0)
    {
        return "";
    }

    const uint8_t first = nameBytes[0];
    const uint8_t second = size > 1 ? nameBytes[1] : 0x00;
    const uint8_t typeId = static_cast<uint8_t>(type.getId());

    const auto nulPos = static_cast<const uint8_t*>(memchr(nameBytes, '\0', size));
    const size_t asciiLen = nulPos ? static_cast<size_t>(nulPos - nameBytes) : size;
    const auto asciiString = [&]() {
        return std::string(reinterpret_cast<const char*>(nameBytes), asciiLen);
    };
    const auto printableAsciiString = [&]() {
        for (size_t i = 0; i < asciiLen; i++)
        {
            if (nameBytes[i] < 0x20 || nameBytes[i] > 0x7E)
            {
                return std::string();
            }
        }
        return asciiString();
    };
    const auto canonicalize_equation_name = [&](uint8_t& outSecond) {
        if (first == 0x5E)
        {
            outSecond = second;
            return true;
        }
        if (first == 'r' && second >= 0x81 && second <= 0x86)
        {
            outSecond = static_cast<uint8_t>(0x40 + (second - 0x81));
            return true;
        }
        if (first == 'U')
        {
            outSecond = 0x80;
            return true;
        }
        if (first == 'V')
        {
            outSecond = 0x81;
            return true;
        }
        if (first == 'W')
        {
            outSecond = 0x82;
            return true;
        }
        if (first == 'X' && second >= 0x81 && second <= 0x86)
        {
            outSecond = static_cast<uint8_t>(0x20 + ((second - 0x81) * 2));
            return true;
        }
        if (first == 'Y' && second >= 0x81 && second <= 0x86)
        {
            outSecond = static_cast<uint8_t>(0x21 + ((second - 0x81) * 2));
            return true;
        }
        return false;
    };

    if ((typeId == 0x01 || typeId == 0x0D) && first == 0x5D)
    {
        if (second <= 0x05)
        {
            return "L"s + static_cast<char>('1' + second);
        }
        if (second == 0x40)
        {
            return "IDList";
        }

        const auto listNulPos = static_cast<const uint8_t*>(memchr(nameBytes + 1, '\0', size - 1));
        const size_t listLen = listNulPos ? static_cast<size_t>(listNulPos - (nameBytes + 1)) : (size - 1);
        return std::string(reinterpret_cast<const char*>(nameBytes + 1), listLen);
    }

    if (typeId == 0x02 && first == 0x5C && second <= 0x09)
    {
        return "["s + static_cast<char>('A' + second) + "]";
    }

    if (typeId == 0x03 || typeId == 0x0B)
    {
        uint8_t canonicalSecond = 0x00;
        if (!canonicalize_equation_name(canonicalSecond))
        {
            canonicalSecond = second;
        }

        if (canonicalSecond >= 0x10 && canonicalSecond <= 0x19)
        {
            const char digit = canonicalSecond == 0x19 ? '0' : static_cast<char>('1' + (canonicalSecond - 0x10));
            return "Y"s + digit;
        }
        if (canonicalSecond >= 0x20 && canonicalSecond <= 0x2B)
        {
            const uint8_t idx = static_cast<uint8_t>((canonicalSecond - 0x20) / 2);
            const char axis = (canonicalSecond % 2 == 0) ? 'X' : 'Y';
            return std::string(1, axis) + static_cast<char>('1' + idx) + "T";
        }
        if (canonicalSecond >= 0x40 && canonicalSecond <= 0x45)
        {
            return "R"s + static_cast<char>('1' + (canonicalSecond - 0x40));
        }
        if (canonicalSecond >= 0x80 && canonicalSecond <= 0x82)
        {
            return std::string(1, static_cast<char>('U' + (canonicalSecond - 0x80)));
        }
    }

    if (typeId == 0x04 && first == 0xAA)
    {
        const char digit = second == 9 ? '0' : static_cast<char>('1' + second);
        return "Str"s + digit;
    }

    if (typeId == 0x07 && first == 0x60)
    {
        const char digit = second == 9 ? '0' : static_cast<char>('1' + second);
        return "Pic"s + digit;
    }

    if (typeId == 0x08 && first == 0x61)
    {
        const char digit = second == 9 ? '0' : static_cast<char>('1' + second);
        return "GDB"s + digit;
    }

    if (typeId == 0x0F)
    {
        return "Window";
    }
    if (typeId == 0x10)
    {
        return "RclWindw";
    }
    if (typeId == 0x11)
    {
        return "TblSet";
    }

    if (typeId == 0x1A && first == 0x3C)
    {
        const char digit = second == 9 ? '0' : static_cast<char>('1' + second);
        return "Image"s + digit;
    }

    if (typeId == 0x05 || typeId == 0x06 || typeId == 0x15 || typeId == 0x17 || typeId == 0x26)
    {
        return printableAsciiString();
    }

    if (first >= 'A' && first <= 'Z')
    {
        return std::string(1, static_cast<char>(first));
    }
    if (first == 0x5B)
    {
        return "θ";
    }

    return printableAsciiString();
}

std::string sanitize_for_host_filename(const std::string& name)
{
    std::string sanitized(name);

    replace_all(sanitized, "", "T");
    replace_all(sanitized, "₀", "0");
    replace_all(sanitized, "₁", "1");
    replace_all(sanitized, "₂", "2");
    replace_all(sanitized, "₃", "3");
    replace_all(sanitized, "₄", "4");
    replace_all(sanitized, "₅", "5");
    replace_all(sanitized, "₆", "6");
    replace_all(sanitized, "₇", "7");
    replace_all(sanitized, "₈", "8");
    replace_all(sanitized, "₉", "9");
    replace_all(sanitized, "/", "_");

    return sanitized;
}

}
