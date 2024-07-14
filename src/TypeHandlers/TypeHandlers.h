/*
 * Part of tivars_lib_cpp
 * (C) 2015-2021 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#ifndef TYPE_HANDLERS_H
#define TYPE_HANDLERS_H

#include "../CommonTypes.h"

namespace tivars
{
    class TIVarFile;
}

namespace tivars::TypeHandlers
{
#define th()    static data_t      makeDataFromString(const std::string& str, const options_t& options = options_t(), const TIVarFile* _ctx = nullptr); \
                static std::string makeStringFromData(const data_t& data,     const options_t& options = options_t(), const TIVarFile* _ctx = nullptr); \
                static uint8_t     getMinVersionFromData(const data_t& data);

    class DummyHandler
    {
        public:
        th();
    };

    class STH_FP : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]{1,2})?)";
    };

    class STH_ExactFraction : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "===UNIMPLEMENTED==="; // TODO
    };

    class STH_ExactRadical : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "===UNIMPLEMENTED==="; // TODO
    };

    class STH_ExactPi : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "===UNIMPLEMENTED==="; // TODO
    };

    class STH_DataAppVar : public DummyHandler
    {
        public:
        th();
    };

    class STH_PythonAppVar : public DummyHandler
    {
        public:
        th();
        static const constexpr char ID_SCRIPT[] = "PYSC";
        static const constexpr char ID_CODE[] = "PYCD";
    };

    class STH_ExactFractionPi : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "===UNIMPLEMENTED==="; // TODO
    };

    class TH_GenericReal : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
    };

    class TH_GenericComplex : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 2 * TH_GenericReal::dataByteCount;
    };

    class TH_GenericList : public DummyHandler
    {
        public:
        th();
    };

    class TH_Matrix : public DummyHandler
    {
        public:
        th();
    };

    class TH_GenericAppVar : public DummyHandler
    {
        public:
        th();
    };

    class TH_GDB : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCountMinimum = 100;
        static const constexpr char* magic84CAndLaterSectionMarker = "84C";
    };

    // Program, Protected Program, Y-Variable, String
    class TH_Tokenized : public DummyHandler
    {
        public:
        th();
        enum lang { LANG_EN = 0, LANG_FR };
        enum typelang { PRGMLANG_BASIC = 0, PRGMLANG_AXE, PRGMLANG_ICE };
        enum indentchar : char { INDENT_CHAR_SPACE = ' ', INDENT_CHAR_TAB = '\t' };
        struct token_posinfo { uint16_t line; uint16_t column; uint8_t len; };
        static std::string reindentCodeString(const std::string& str_orig, const options_t& options = options_t());
        static token_posinfo getPosInfoAtOffset(const data_t& data, uint16_t byteOffset, const options_t& options = options_t());
        static token_posinfo getPosInfoAtOffsetFromHexStr(const std::string& hexBytesStr, uint16_t byteOffset);
        static std::string tokenToString(const data_t& data, int *incr, const options_t& options);
        static std::string oneTokenBytesToString(uint16_t tokenBytes);
        static void initTokens();
        static void initTokensFromCSVFilePath(const std::string& csvFilePath);
        static void initTokensFromCSVContent(const std::string& csvFileStr);
    };

    // Special temporary type that may appear as an equation, during basic program execution
    class TH_TempEqu : public DummyHandler
    {
        public:
        th();
    };

    using dataFromString_handler_t = decltype(&DummyHandler::makeDataFromString);
    using stringFromData_handler_t = decltype(&DummyHandler::makeStringFromData);
    using handler_pair_t           = std::pair<dataFromString_handler_t, stringFromData_handler_t>;

#define make_handler_pair(cls)   make_pair(&cls::makeDataFromString, &cls::makeStringFromData)

}

#endif
