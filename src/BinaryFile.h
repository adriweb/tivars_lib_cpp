/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARS_LIB_CPP_BINARYFILE_H
#define TIVARS_LIB_CPP_BINARYFILE_H

#include "autoloader.h"

namespace tivars
{
    class BinaryFile
    {
    public:
        BinaryFile()
        {}

        BinaryFile(const std::string filePath);

        ~BinaryFile()
        {
            close();
        }

        data_t get_raw_bytes(uint bytes);
        std::string get_string_bytes(uint bytes);
        size_t size();
        void close();

    protected:
        FILE* file = nullptr;
        std::string filePath;
        size_t fileSize;

    };
}

#endif //TIVARS_LIB_CPP_BINARYFILE_H
