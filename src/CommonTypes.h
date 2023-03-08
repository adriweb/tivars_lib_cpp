/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef COMMON_H
#define COMMON_H

#include <vector>
#include <string>
#include <map>

#include <cstdint>

using data_t = std::vector<uint8_t>;
using options_t = std::map<std::string, int>;

#define u8enum(NAME) enum NAME : uint8_t

#endif