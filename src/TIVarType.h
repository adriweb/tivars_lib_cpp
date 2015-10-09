/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIVARTYPE_H
#define TIVARTYPE_H

#include "autoloader.h"
#include "TypeHandlers/ITIVarTypeHandler.h"
#include "TIModel.h"

namespace tivars
{

    class TIVarType
    {

    public:

        TIVarType();
        ~TIVarType();

        /* Getters */
        int getId() { return this->id; }
        std::string getName() { return this->name; }
        std::vector<std::string> getExts() { return this->exts; }
        ITIVarTypeHandler* getTypeHandler() { return this->typeHandler; }


        /*** "Constructors" ***/
        /**
         * @param   int     flags  The version compatibliity flags
         * @return  TIModel
         * @throws  \Exception
         */
        static TIModel createFromFlags(uint flags);

        /**
         * @param   string  name   The version name
         * @return  TIModel
         * @throws  \Exception
         */
        static TIModel createFromName(std::string name);

        /**
         * @param   string  sig    The signature (magic bytes)
         * @return  TIModel
         * @throws  \Exception
         */
        static TIModel createFromSignature(std::string sig);


    private:
        int id = -1;
        std::string name = "Unknown";
        std::vector<std::string> exts;
        ITIVarTypeHandler* typeHandler = nullptr;

    };

}

#endif
