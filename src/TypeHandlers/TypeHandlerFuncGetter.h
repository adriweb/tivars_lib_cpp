/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include "ITIVarTypeHandler.h"
#include "TH_0x00.h"
#include "TH_0x05.h"
#include "TH_0x06.h"

#ifndef TIVARS_LIB_CPP_TYPEHANDLERFUNCGETTER_H
#define TIVARS_LIB_CPP_TYPEHANDLERFUNCGETTER_H

namespace tivars
{

    class TypeHandlerFuncGetter
    {

    public:

        static auto getStringFromDataFunc(int type)
        {
            auto func = &ITIVarTypeHandler::makeStringFromData;
            switch (type)
            {
                case 0x00:
                    func = &TH_0x00::makeStringFromData;
                    break;

                case 0x05:
                case 0x06:
                    func = &TH_0x05::makeStringFromData;
                    break;

                default:
                    break;
            }
            return func;
        }

        static auto getDataFromStringFunc(int type)
        {
            auto func = &ITIVarTypeHandler::makeDataFromString;
            switch (type)
            {
                case 0x00:
                    func = &TH_0x00::makeDataFromString;
                    break;

                case 0x05:
                case 0x06:
                    func = &TH_0x05::makeDataFromString;
                    break;

                default:
                    break;
            }
            return func;
        }
    };

}

#endif //TIVARS_LIB_CPP_TYPEHANDLERFUNCGETTER_H
