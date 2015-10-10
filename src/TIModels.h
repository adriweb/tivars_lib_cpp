/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIMODELS_H
#define TIMODELS_H

#include "autoloader.h"
#include "TIModel.h"
#include "TIVarType.h"

namespace tivars
{

    class TIModels
    {

    public:

        static void initTIModelsArray();

        static std::string getDefaultNameFromFlags(uint flags);

        /**
         * @param   std::string  name   The model name
         * @return  int             The model flags for that name
         */
        static uint getFlagsFromName(std::string name);

        /**
         * @param   int     flags  The model flags
         * @return  std::string          The signature for those flags
         */
        static std::string getSignatureFromFlags(uint flags);

        /**
         * @param   std::string  name
         * @return  std::string          The signature for that name
         */
        static std::string getSignatureFromName(std::string name);
        /**
         * @param   std::string  sig    The signature
         * @return  std::string          The default calc name whose file formats use that signature
         */
        static std::string getDefaultNameFromSignature(std::string sig);

        /**
         * @param   std::string  sig    The signature
         * @return  int             The default calc order ID whose file formats use that signature
         */
        static int getDefaultOrderIDFromSignature(std::string sig);

        /**
         * @param   std::string  name
         * @return  int             The default calc order ID whose file formats use that signature
         */
        static int getOrderIDFromName(std::string name);

        /**
         * @param   int     flags  The model flags
         * @return  int             The default calc order ID whose file formats use that signature
         */
        static int getDefaulOrderIDFromFlags(uint flags);

        /**
         * @param   std::string  sig    The signature
         * @return  std::string          The minimum compatibility flags for that signaure
         */
        static uint getMinFlagsFromSignature(std::string sig);


        static bool isValidFlags(uint flags);

        static bool isValidName(std::string name);

        static bool isValidSignature(std::string sig);

    private:
        static void insertModel(int orderID, uint flags, const std::string name, const std::string sig);

    };

}

#endif
