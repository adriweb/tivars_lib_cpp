/*
 * Part of tivars_lib_cpp
 * (C) 2015-2026 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TIMODELS_H
#define TIMODELS_H

#include "CommonTypes.h"
#include "TIModel.h"
#include <unordered_map>

namespace tivars
{
    enum TIFeatureFlags
    {
        has82things  = 0b000000001, // (1 << 0);
        hasComplex   = 0b000000010, // (1 << 1);
        hasFlash     = 0b000000100, // (1 << 2);
        hasApps      = 0b000001000, // (1 << 3);
        hasClock     = 0b000010000, // (1 << 4);
        hasColorLCD  = 0b000100000, // (1 << 5);
        hasEZ80CPU   = 0b001000000, // (1 << 6);
        hasExactMath = 0b010000000, // (1 << 7);
        hasPython    = 0b100000000, // (1 << 8);
    };

    class TIModels
    {

    public:

        static TIModel fromName(const std::string& name);
        static TIModel fromSignature(const std::string& sig);
        static TIModel fromPID(uint8_t pid);

        static const std::unordered_map<std::string, TIModel>& all();

        static bool isValidPID(uint8_t pid);
        static bool isValidName(const std::string& name);
        static bool isValidSignature(const std::string& sig);

    };

}

#endif
