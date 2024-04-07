/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARSLIB_UTILS_H
#define TIVARSLIB_UTILS_H

#include <vector>
#include <string>
#include <algorithm>

#include "CommonTypes.h"
#include "TIModel.h"
#include "TIVarType.h"

namespace tivars
{

template<typename T>
bool is_in_vector(const std::vector<T>& v, T element)
{
    return std::find(v.begin(), v.end(), element) != v.end();
}

template<typename T>
void vector_append(std::vector<T>& vec, const std::vector<T>& other)
{
    vec.insert(vec.end(), other.begin(), other.end());
}

bool has_option(const options_t& m, const std::string& element);

unsigned char hexdec(const std::string& str);

std::string dechex(unsigned char i, bool zeropad = true);

std::string strtoupper(const std::string& str);

std::vector<std::string> explode(const std::string& str, const std::string& delim);
std::vector<std::string> explode(const std::string& str, char delim);

std::string ltrim(std::string s, const char* t = " \t\n\r\f\v");

std::string rtrim(std::string s, const char* t = " \t\n\r\f\v");

std::string trim(const std::string& s, const char* t = " \t\n\r\f\v");

void ParseCSV(const std::string& csvSource, std::vector<std::vector<std::string>>& lines);

bool is_numeric(const std::string& str);

bool file_exists(const std::string& filePath);

std::string str_pad(const std::string& str, unsigned long pad_length, std::string pad_string = " ");

std::string multiple(int num, const std::string& var);

std::string dec2frac(double num, const std::string& var = "", double err = 0.001);

std::string trimZeros(const std::string& str);

}

#endif
