/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <unordered_map>
#include <vector>
#include <string>

bool is_in_vector_uint(const std::vector<unsigned int>& v, unsigned int element);
bool is_in_vector_string(const std::vector<std::string>& v, std::string element);
bool is_in_umap_string_uint(const std::unordered_map<std::string, unsigned int>& m, const std::string element);

bool has_option(const std::unordered_map<std::string, unsigned int>& m, const std::string element);

unsigned int hexdec(const std::string& str);

std::string dechex(unsigned int i);

std::vector<std::string> explode(const std::string& str, char delim);

std::string& ltrim(std::string& s);

std::string& rtrim(std::string& s);

std::string& trim(std::string& s);

std::string str_repeat(const std::string& str, unsigned int times);

void ParseCSV(const std::string& csvSource, std::vector<std::vector<std::string>>& lines);

bool is_numeric(const std::string& str);