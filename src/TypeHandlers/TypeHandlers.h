/*
 * Part of tivars_lib_cpp
 * (C) 2015-2018 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TYPE_HANDLERS_H
#define TYPE_HANDLERS_H

#include "../CommonTypes.h"

namespace tivars
{

#define th()    data_t      makeDataFromString(const std::string& str,  const options_t& options = options_t()); \
                std::string makeStringFromData(const data_t& data,      const options_t& options = options_t())

    namespace DummyHandler { th(); }

    namespace STH_FP
    {
        th();
        const constexpr size_t dataByteCount = 9;
        const std::string validPattern = "([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]{1,2})?)";
    }
    namespace STH_ExactFraction
    {
        th();
        const std::string validPattern = "===UNIMPLEMENTED==="; // TODO
    }
    namespace STH_ExactRadical
    {
        th();
        const std::string validPattern = "===UNIMPLEMENTED==="; // TODO
    }
    namespace STH_ExactPi
    {
        th();
        const std::string validPattern = "===UNIMPLEMENTED==="; // TODO
    }
    namespace STH_ExactFractionPi
    {
        th();
        const std::string validPattern = "===UNIMPLEMENTED==="; // TODO
    }

    namespace TH_GenericReal
    {
        th();
        const constexpr size_t dataByteCount = 9;
    }
    namespace TH_GenericComplex
    {
        th();
        const constexpr size_t dataByteCount = 18;
    }

    namespace TH_0x01 { th(); }  // Real list
    namespace TH_0x0D { th(); }  // Complex list

    namespace TH_0x02 { th(); }  // Matrix
    namespace TH_0x05 { th(); }  // Program

    namespace TH_0x15 { th(); }  // Application variable

    /* The following ones use the same handlers as 0x05 */
    namespace TH_0x03 = TH_0x05; // Y-Variable
    namespace TH_0x04 = TH_0x05; // String
    namespace TH_0x06 = TH_0x05; // Protected Program

#undef th


    /* Additional things */

    namespace TH_0x05   // Program
    {
        enum lang { LANG_EN = 0, LANG_FR };
        enum typelang { PRGMLANG_BASIC = 0, PRGMLANG_AXE };
        std::string reindentCodeString(const std::string& str_orig, const options_t& options = options_t());
        void initTokens();
    }

    using dataFromString_handler_t = decltype(&DummyHandler::makeDataFromString);
    using stringFromData_handler_t = decltype(&DummyHandler::makeStringFromData);
    using handler_pair_t           = std::pair<dataFromString_handler_t, stringFromData_handler_t>;

#define make_handler_pair(cls)   make_pair(&cls::makeDataFromString, &cls::makeStringFromData)

}

#endif