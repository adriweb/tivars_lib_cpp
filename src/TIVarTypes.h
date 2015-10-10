/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARTYPES_H
#define TIVARTYPES_H

#include "autoloader.h"

namespace tivars
{
    class TIVarTypes
    {

    public:
        static void initTIVarTypesArray();

        static bool isValidName(std::string name);

        static bool isValidID(int id);

        static std::vector<std::string> getExtensionsFromName(std::string name);
        static std::vector<std::string> getExtensionsFromTypeID(int id);
        static int getIDFromName(std::string name);
        static std::string getNameFromID(int id);

    private:
        static void insertType(std::string name, int id, std::vector<std::string> exts);

    };
}

#endif //TIVARTYPES_H
