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
        DummyHandler() = delete;
        DummyHandler(const DummyHandler&) = delete;
        DummyHandler& operator=(const DummyHandler) = delete;
        th();
    };

    using TypeHandlersTuple = std::tuple<decltype(&DummyHandler::makeDataFromString), decltype(&DummyHandler::makeStringFromData), decltype(&DummyHandler::getMinVersionFromData)>;
    #define SpecificHandlerTuple(which) TypeHandlersTuple{ &(which::makeDataFromString), &(which::makeStringFromData), &(which::getMinVersionFromData) }

    class TH_GenericReal : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCount = 9;
    };

    class STH_FP : public TH_GenericReal
    {
    public:
        th();
        static const constexpr size_t dataByteCount = 9;
        static const constexpr char* validPattern = "([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]{1,2})?)";
    };

    class STH_ExactFraction : public TH_GenericReal
    {
    public:
        th();
        static const constexpr size_t dataByteCount = 9;
    };

    class STH_ExactRadical : public TH_GenericReal
    {
    public:
        th();
        static const constexpr size_t dataByteCount = 9;
    };

    class STH_ExactPi : public TH_GenericReal
    {
    public:
        th();
        static const constexpr size_t dataByteCount = 9;
    };

    class STH_ExactFractionPi : public TH_GenericReal
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

    enum StructuredAppVarSubtype : uint8_t
    {
        APPVAR_SUBTYPE_PYTHON_MODULE = 0,
        APPVAR_SUBTYPE_PYTHON_IMAGE,
        APPVAR_SUBTYPE_STUDY_CARDS,
        APPVAR_SUBTYPE_STUDY_CARDS_SETTINGS,
        APPVAR_SUBTYPE_CELSHEET,
        APPVAR_SUBTYPE_CELSHEET_STATE,
    };

    std::string detectStructuredAppVarTypeName(const data_t& data);

    class TH_StructuredAppVar : public TH_GenericAppVar
    {
    public:
        th();
    };

    class TH_Group : public DummyHandler
    {
    public:
        th();
        static const constexpr uint8_t archivedFlagValue = 0x80;
        static const constexpr size_t minimumDataByteCount = 2;
        static const constexpr size_t fixedNameByteCount = 3;
    };

    class TH_Picture : public DummyHandler
    {
    public:
        th();
        static const constexpr size_t minimumDataByteCount = 2;
        static const constexpr size_t monoPictureDataByteCount = 758;
        static const constexpr size_t colorPictureDataByteCount = 21947;
        static const constexpr size_t imageDataByteCount = 22247;
        static const constexpr size_t monoPictureWidth = 96;
        static const constexpr size_t monoPictureHeight = 63;
        static const constexpr size_t colorPictureWidth = 266;
        static const constexpr size_t colorPictureHeight = 165;
        static const constexpr size_t imageWidth = 133;
        static const constexpr size_t imageHeight = 83;
        static const constexpr uint8_t imageMagic = 0x81;
    };

    class STH_DataAppVar : public TH_GenericAppVar
    {
        public:
        th();
    };

    class STH_PythonAppVar : public TH_GenericAppVar
    {
        public:
        th();
        static const constexpr char ID_SCRIPT[] = "PYSC";
        static const constexpr char ID_CODE[] = "PYCD";
    };

    class TH_GDB : public DummyHandler
    {
        public:
        th();
        static const constexpr size_t dataByteCountMinimum = 100;
        static const constexpr char* magic84CAndLaterSectionMarker = "84C";
    };

    class TH_Settings : public DummyHandler
    {
        public:
        th();
        static const constexpr uint8_t typeReal = 0x00;
        static const constexpr uint8_t typeUndefinedReal = 0x0E;
        static const constexpr uint8_t typeWindowSettings = 0x0F;
        static const constexpr uint8_t typeRecallWindow = 0x10;
        static const constexpr uint8_t typeTableRange = 0x11;
        static const constexpr size_t realDataByteCount = TH_GenericReal::dataByteCount;
        static const constexpr size_t windowSettingsDataByteCount = 210;
        static const constexpr size_t recallWindowDataByteCount = 209;
        static const constexpr size_t tableRangeDataByteCount = 20;
        static const constexpr size_t windowSettingsHeaderByteCount = 3;
        static const constexpr size_t recallWindowHeaderByteCount = 2;
        static const constexpr size_t tableRangeHeaderByteCount = 2;
        static const constexpr uint8_t windowSettingsHeader[] = {0xD0, 0x00, 0x00};
        static const constexpr uint8_t recallWindowHeader[] = {0xCF, 0x00};
        static const constexpr uint8_t tableRangeHeader[] = {0x12, 0x00};
    };

    // Program, Protected Program, Y-Variable, String
    class TH_Tokenized : public DummyHandler
    {
        public:
        th();
        enum lang { LANG_EN = 0, LANG_FR, LANG_MAX };
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

#undef th

}

#endif
