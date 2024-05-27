/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifdef __EMSCRIPTEN__

#include "TIModels.h"
#include "TIVarTypes.h"
#include "TypeHandlers/TypeHandlers.h"

#include <locale.h>
#include <emscripten.h>

using namespace tivars;

extern "C" int EMSCRIPTEN_KEEPALIVE main(int, char**)
{
    setlocale(LC_ALL, ".UTF-8");

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_Tokenized::initTokens();

    puts("tivars_lib ready!");

    return 0;
}

#endif