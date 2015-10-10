/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "utils.h"
#include <sstream>
#include <iomanip>
#include <regex>

using namespace std;

bool is_in_vector_uint(const std::vector<unsigned int>& v, unsigned int element)
{
    return find(v.begin(), v.end(), element) != v.end();
}

bool is_in_vector_string(const std::vector<std::string>& v, std::string element)
{
    return find(v.begin(), v.end(), element) != v.end();
}

bool is_in_umap_string_uint(const unordered_map<string, unsigned int>& m, const string element)
{
    return m.find(element) != m.end();
}

bool is_in_umap_string_TIModel(const std::unordered_map<std::string, tivars::TIModel>& m, const std::string element)
{
    return m.find(element) != m.end();
}

bool is_in_umap_string_TIVarType(const std::unordered_map<std::string, tivars::TIVarType>& m, const std::string element)
{
    return m.find(element) != m.end();
}

bool has_option(const unordered_map<string, unsigned int>& m, const string element)
{
    return m.find(element) != m.end();
}

unsigned int hexdec(const string& str)
{
    return (unsigned int) stoul(str, nullptr, 16);
}

std::string dechex(unsigned int i)
{
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw((int) (sizeof(unsigned int) * 2)) << std::hex << i;
    return stream.str();
}

vector<string> explode(const string& str, char delim)
{
    vector<string> result;
    istringstream iss(str);

    for (string token; getline(iss, token, delim);)
    {
        result.push_back(move(token));
    }

    return result;
}

// trim from start
string& ltrim(string& s)
{
    s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
    return s;
}

// trim from end
string& rtrim(string& s)
{
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

// trim from both ends
string& trim(string& s)
{
    return ltrim(rtrim(s));
}

string str_repeat(const string& str, unsigned int times)
{
    string result;
    result.reserve(times * str.length()); // avoid repeated reallocation
    for (unsigned int i = 0; i < times; i++)
    {
        result += str;
    }
    return result;
}

// From http://stackoverflow.com/a/2481126/378298
void ParseCSV(const string& csvSource, vector<vector<string>>& lines)
{
    bool inQuote(false);
    bool newLine(false);
    string field;
    lines.clear();
    vector<string> line;

    string::const_iterator aChar = csvSource.begin();
    while (aChar != csvSource.end())
    {
        switch (*aChar)
        {
            case '"':
                newLine = false;
                inQuote = !inQuote;
                break;

            case ',':
                newLine = false;
                if (inQuote == true)
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
                if (inQuote == true)
                {
                    field += *aChar;
                }
                else
                {
                    if (newLine == false)
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

        aChar++;
    }

    if (field.size())
        line.push_back(field);

    if (line.size())
        lines.push_back(line);
}

bool is_numeric(const std::string& str)
{
    return std::regex_match(str, std::regex("[(-|+)|][0-9]*\\.?[0-9]+"));
}

// From http://rosettacode.org/wiki/Strip_a_set_of_characters_from_a_string#C.2B.2B
std::string stripchars(std::string str, const std::string& chars)
{
    str.erase(std::remove_if(str.begin(), str.end(), [&](char c){ return chars.find(c) != std::string::npos; }), str.end());
    return str;
}