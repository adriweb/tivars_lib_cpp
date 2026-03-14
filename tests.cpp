/*
 * Part of tivars_lib_cpp
 * (C) 2015-2024 Adrien "Adriweb" Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>

#ifndef _WIN32
    #include <sys/stat.h>
#endif

#include "src/TIModels.h"
#include "src/TIVarTypes.h"
#include "src/BinaryFile.h"
#include "src/TIVarFile.h"
#include "src/TIFlashFile.h"
#include "src/TypeHandlers/TypeHandlers.h"
#include "src/tivarslib_utils.h"
#include "src/json.hpp"

using namespace std;
using namespace std::string_literals;
using namespace tivars;
using namespace tivars::TypeHandlers;
using TypeHandlers::TH_Tokenized;
using json = nlohmann::json;

static bool compare_token_posinfo(const TH_Tokenized::token_posinfo& tp1, const TH_Tokenized::token_posinfo& tp2)
{
    return tp1.column == tp2.column && tp1.line == tp2.line && tp1.len == tp2.len;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    setlocale(LC_ALL, ".UTF-8");

    /* Init Stuff */

    TIModels::initTIModelsArray();
    TIVarTypes::initTIVarTypesArray();
    TH_Tokenized::initTokens();

    /* Tests */

    assert(TIVarType{"ExactRealPi"}.getId() == 32);

    {
        TIFlashFile flashOs = TIFlashFile::loadFromFile("testData/TI-84_Plus_CE-Python-OS-5.8.0.0022.8eu");
        assert(flashOs.hasMultipleHeaders() == false);
        const json flashOsJSON = json::parse(flashOs.getReadableContent());
        assert(flashOsJSON["typeName"] == "OperatingSystem");
        assert(flashOsJSON["name"] == "basecode");
        assert(flashOsJSON["revision"] == "5.8");
        assert(flashOsJSON["binaryFlag"] == 0);
        assert(flashOsJSON["objectType"] == 0);
        assert(flashOsJSON["date"][0] == 26);
        assert(flashOsJSON["date"][1] == 4);
        assert(flashOsJSON["date"][2] == 2022);
        assert(flashOsJSON["devices"][0]["deviceType"] == 0x73);
        assert(flashOsJSON["devices"][0]["typeId"] == 0x23);
        assert(flashOsJSON["productId"] == 0x13);
        assert(flashOsJSON["calcDataSize"] == 644962);
        assert(flashOsJSON.contains("fields"));
        assert(flashOsJSON["fields"].is_array());
        assert(flashOsJSON["fields"].size() >= 2);
        assert(flashOsJSON["fields"][0]["idHex"] == "800");
        assert(flashOsJSON["fields"][0]["name"] == "Master");
        assert(flashOsJSON["fields"][0].contains("fields"));
        assert(flashOsJSON["fields"][0]["fields"][1]["idHex"] == "802");
        assert(flashOsJSON["fields"][0]["fields"][1]["name"] == "Revision");
        assert(flashOsJSON["fields"][0]["fields"][1]["rawDataHex"] == "05");
        assert(flashOsJSON["fields"][1]["idHex"] == "023");
        assert(flashOsJSON["fields"][1]["name"] == "CE signature");
        assert(!flashOsJSON.contains("fieldsError"));

        TIFlashFile flashApp = TIFlashFile::loadFromFile("testData/smartpad.8xk");
        const json flashAppJSON = json::parse(flashApp.getReadableContent());
        assert(flashAppJSON["typeName"] == "FlashApp");
        assert(flashAppJSON["name"] == "SmartPad");
        assert(flashAppJSON["revision"] == "1.1");
        assert(flashAppJSON["binaryFlag"] == 1);
        assert(flashAppJSON["objectType"] == 0x88);
        assert(flashAppJSON["blocks"].size() == 78);
        assert(flashAppJSON["blocks"][0]["address"] == "0000");
        assert(flashAppJSON["blocks"][0]["blockType"] == "02");
        assert(flashAppJSON["blocks"][0]["dataHex"] == "0000");

        TIFlashFile recreatedFlashApp = TIFlashFile::createNew(TIVarType{"FlashApp"}, "SmartPad", TIModel{"84+"});
        recreatedFlashApp.setContentFromString(flashApp.getReadableContent());
        assert(recreatedFlashApp.make_bin_data() == flashApp.make_bin_data());

        TIFlashFile multiFlash = TIFlashFile::createNew(TIVarType{"FlashApp"}, "APP", TIModel{"84+"});
        multiFlash.setContentFromString(R"({
    "typeName": "FlashApp",
    "revision": "1.1",
    "binaryFlag": 1,
    "objectType": 136,
    "date": [1, 12, 2006],
    "name": "APP",
    "devices": [{"deviceType": 115, "typeId": 36}],
    "productId": 10,
    "hasChecksum": true,
    "blocks": [
        {"address": "0000", "blockType": "02", "dataHex": "0000"},
        {"address": "4000", "blockType": "00", "dataHex": "01020304"},
        {"address": "0000", "blockType": "01", "dataHex": ""}
    ]
})");
        multiFlash.addHeader(TIVarType{"OperatingSystem"}, "BASE", TIModel{"84+CE"}, true);
        multiFlash.setContentFromString(R"({
    "typeName": "OperatingSystem",
    "revision": "1.0",
    "binaryFlag": 0,
    "objectType": 0,
    "date": [2, 3, 2024],
    "name": "BASE",
    "devices": [{"deviceType": 115, "typeId": 35}],
    "productId": 19,
    "hasChecksum": true,
    "calcDataHex": "01020304A0"
})", 1);
        const std::string multiFlashPath = multiFlash.saveToFile("/tmp/tivars_lib_cpp_multi_flash.8ek");
        TIFlashFile reloadedMultiFlash = TIFlashFile::loadFromFile(multiFlashPath);
        assert(reloadedMultiFlash.hasMultipleHeaders() == true);
        assert(reloadedMultiFlash.getHeaders().size() == 2);
        assert(reloadedMultiFlash.make_bin_data() == multiFlash.make_bin_data());
        assert(remove(multiFlashPath.c_str()) == 0);
    }

    {
        TIVarFile testReal = TIVarFile::createNew("Real", "A");
        for (const auto& str : { "0.0", "0", "+0.0", "+0" })
        {
            testReal.setContentFromString(str);
            assert(testReal.getRawContent() == data_t({ 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }));
            assert(testReal.getReadableContent() == "0");
        }
    }

    {
        const data_t undefinedReal = TH_GenericReal::makeDataFromString("1", {{"_type", 0x0E}});
        assert(undefinedReal[0] == 0x0E);
        assert(TH_GenericReal::makeStringFromData(undefinedReal) == "1");
        assert(TH_GenericReal::getMinVersionFromData(undefinedReal) == 0x00);
    }

    {
        TIVarFile toksPrgm = TIVarFile::loadFromFile("testData/ALLTOKS.8Xp");
        cout << toksPrgm.getReadableContent() << "\n" << endl;
    }

#ifndef _WIN32
    {
        const std::string path = "/tmp/tivars_lib_cpp_readonly_test.8xp";
        TIVarFile readonlyPrgm = TIVarFile::createNew("Program", "READONLY");
        readonlyPrgm.setContentFromString("Disp 42");
        readonlyPrgm.saveVarToFile(path);
        assert(chmod(path.c_str(), 0444) == 0);

        TIVarFile readonlyLoaded = TIVarFile::loadFromFile(path);
        assert(readonlyLoaded.getReadableContent() == "Disp 42");

        assert(chmod(path.c_str(), 0644) == 0);
        assert(remove(path.c_str()) == 0);
    }
#endif

    {
        TIVarFile testPrgmStr1 = TIVarFile::createNew("Program", "asdf");
        testPrgmStr1.setContentFromString("\"42→Str1:Str2:123");
        assert(trim(testPrgmStr1.getReadableContent({{"prettify", true}, {"reindent", true}})) == "\"42→Str1\nStr2\n123");
    }

    {
        TIVarFile eightCharPrgm = TIVarFile::createNew("Program", "ABCDEFGH");
        eightCharPrgm.setContentFromString("piecewise(");
        const std::string savePath = eightCharPrgm.saveVarToFile("/tmp", "");
        assert(savePath == "/tmp/ABCDEFGH.8xp");

        TIVarFile reloadedEightCharPrgm = TIVarFile::loadFromFile(savePath);
        const std::string resavedPath = reloadedEightCharPrgm.saveVarToFile("/tmp", "");
        assert(resavedPath == savePath);
        assert(remove(savePath.c_str()) == 0);
    }

    {
        TIVarFile archivedPrgm = TIVarFile::createNew("Program", "ARCHIVE");
        archivedPrgm.setContentFromString("Disp 42");
        archivedPrgm.setArchived(true);
        const std::string savePath = archivedPrgm.saveVarToFile("/tmp", "ARCHIVE");

        TIVarFile reloadedArchivedPrgm = TIVarFile::loadFromFile(savePath);
        assert(reloadedArchivedPrgm.getReadableContent() == "Disp 42");
        assert(reloadedArchivedPrgm.getVarEntries()[0].archivedFlag == 0x80);
        assert(remove(savePath.c_str()) == 0);
    }

    {
        // See https://wikiti.brandonw.net/index.php?title=83Plus:OS:Variable_Versions
        TIVarFile testPrgmStr1 = TIVarFile::createNew("Program", "asdf");
        const auto& ver = testPrgmStr1.getVarEntries()[0].version;
        assert(ver == 0x00);
        testPrgmStr1.setContentFromString("Disp 41+1");
        assert((ver & ~0x20) == 0x00);
        testPrgmStr1.setContentFromString("Archive A");
        assert((ver & ~0x20) == 0x01);
        testPrgmStr1.setContentFromString("GarbageCollect");
        assert((ver & ~0x20) == 0x01);
        testPrgmStr1.setContentFromString("Disp 42%");
        assert((ver & ~0x20) == 0x02);
        testPrgmStr1.setContentFromString("~A");
        assert((ver & ~0x20) == 0x02);
        testPrgmStr1.setContentFromString("Disp \"…\"");
        assert((ver & ~0x20) == 0x03);
        testPrgmStr1.setContentFromString("Disp \"⌸\"");
        assert((ver & ~0x20) == 0x03);
        testPrgmStr1.setContentFromString("setDate(A,B,C)");
        assert((ver & ~0x20) == 0x04);
        testPrgmStr1.setContentFromString("ExecLib \"A\"");
        assert((ver & ~0x20) == 0x04);
        testPrgmStr1.setContentFromString("Manual-Fit ");
        assert((ver & ~0x20) == 0x05);
        testPrgmStr1.setContentFromString("ZQuadrant1");
        assert((ver & ~0x20) == 0x06);
        testPrgmStr1.setContentFromString("FRAC");
        assert((ver & ~0x20) == 0x06);
        testPrgmStr1.setContentFromString("STATWIZARD ON");
        assert((ver & ~0x20) == 0x07);
        testPrgmStr1.setContentFromString("STATWIZARD OFF");
        assert((ver & ~0x20) == 0x07);
        testPrgmStr1.setContentFromString("BLUE");
        assert((ver & ~0x20) == 0x0A);
        testPrgmStr1.setContentFromString("Dot-Thin");
        assert((ver & ~0x20) == 0x0A);
        testPrgmStr1.setContentFromString("TraceStep");
        assert((ver & ~0x20) == 0x00); // 63** token ranges are not considered by a calculator when it generates the version.
        testPrgmStr1.setContentFromString("Asm84CEPrgm:C9");
        assert((ver & ~0x20) == 0x0B);
        testPrgmStr1.setContentFromString("Disp eval(Str1");
        assert((ver & ~0x20) == 0x0B); // 0Bh is used for all of TI-84 Plus CE OS 5.0 through 5.2, despite tokens being added between them.
        testPrgmStr1.setContentFromString("Quartiles Setting…");
        assert((ver & ~0x20) == 0x0B);
        testPrgmStr1.setContentFromString("Execute Program");
        assert((ver & ~0x20) == 0x0C);
        testPrgmStr1.setContentFromString("piecewise(");
        assert((ver & ~0x20) == 0x0C);
    }

    {
        assert(TH_Tokenized::oneTokenBytesToString(0x00) == "");
        assert(TH_Tokenized::oneTokenBytesToString(0xBB) == "");
        assert(TH_Tokenized::oneTokenBytesToString(0x3F) == "\n");
        assert(TH_Tokenized::oneTokenBytesToString(0xAD) == "getKey");
        assert(TH_Tokenized::oneTokenBytesToString(0xEF97) == "toString(");
    }

    {
        TIVarFile asmProgram = TIVarFile::createNew("Program", "ASMHDR");
        asmProgram.setContentFromString(R"({
    "rawDataHex": "BB6DC90100000000000000000000000000000000000000000000000000000000000048656C6C6F00"
})");
        const json asmMetadata = json::parse(asmProgram.getReadableContent({{"metadata", true}}));
        assert(asmMetadata["isAssembly"] == true);
        assert(asmMetadata["shell"] == "MirageOS");
        assert(asmMetadata["description"] == "Hello");
        assert(asmMetadata["typeName"] == "Program");

        TIVarFile metadataProgram = TIVarFile::createNew("Program", "JSONASM");
        metadataProgram.setContentFromString(R"({
    "rawDataHex": "BB6DC901000000000000000000000000000000000000000000000000000000000000486900"
})");
        const json metadataProgramJSON = json::parse(metadataProgram.getReadableContent({{"metadata", true}}));
        assert(metadataProgramJSON["isAssembly"] == true);
        assert(metadataProgramJSON["shell"] == "MirageOS");
    }

    {
        TIVarFile basicProgram = TIVarFile::createNew("Program", "META");
        basicProgram.setContentFromString("Disp 42");
        const json basicMetadata = json::parse(basicProgram.getReadableContent({{"metadata", true}}));
        assert(basicMetadata["isAssembly"] == false);
        assert(basicMetadata["code"] == "Disp 42");
    }

    {
        TH_Tokenized::token_posinfo actual{}, expected{};
        const data_t data = { 0x12,0x00,0x41,0x40,0x42,0x3f,0xde,0x2a,0x41,0x29,0xbb,0xb0,0xbb,0xbe,0xbb,0xb3,0x29,0x42,0x2a,0x3f };
        actual = TH_Tokenized::getPosInfoAtOffset(data, 2);
        expected = { 0, 0, 1 };
        assert(compare_token_posinfo(actual, expected) == true);
        actual = TH_Tokenized::getPosInfoAtOffset(data, 3);
        expected = { 0, 1, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
        actual = TH_Tokenized::getPosInfoAtOffset(data, 6);
        expected = { 1, 0, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
    }

    {
        TH_Tokenized::token_posinfo actual{}, expected{};
        const std::string hexStr = "12004140423fde2a4129bbb0bbbebbb329422a3f";
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 2);
        expected = { 0, 0, 1 };
        assert(compare_token_posinfo(actual, expected) == true);
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 3);
        expected = { 0, 1, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 6);
        expected = { 1, 0, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
    }

    {
        TH_Tokenized::token_posinfo actual{}, expected{};
        const std::string hexStr = "0700DEEF983170323F";
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 2);
        expected = { 0, 0, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 3);
        expected = { 0, 5, 5 };
        assert(compare_token_posinfo(actual, expected) == true);
    }

    {
        TH_Tokenized::token_posinfo actual{}, expected{};
        const std::string hexStr = "010004";
        actual = TH_Tokenized::getPosInfoAtOffsetFromHexStr(hexStr, 2);
        expected = { 0, 0, 1 };
        assert(compare_token_posinfo(actual, expected) == true);
    }

    {
        // Make sure \r\n is tokenized the same as \n (because \r is just ignored as it's not a known token)
        TIVarFile testPrgm = TIVarFile::createNew("Program", "TEST1");
        testPrgm.setContentFromString("Pause 1\nPause 1");
        TIVarFile testPrgm2 = TIVarFile::createNew("Program", "TEST2");
        testPrgm2.setContentFromString("Pause 1\r\nPause 1");
        assert(testPrgm.getRawContentHexStr() == testPrgm2.getRawContentHexStr());
    }

    {
        // Test lower alpha being the expected lowercase tokens, not special-meaning ones
        TIVarFile testPrgm = TIVarFile::createNew("Program", "INTERP");
        testPrgm.setContentFromString("Disp \"abcdefghijklmnopqrstuvwxyz\"");
        string detok_fr = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});
        string detok_en = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_EN}});
        string hex = testPrgm.getRawContentHexStr();
        assert(detok_fr == "Disp \"abcdefghijklmnopqrstuvwxyz\"");
        assert(detok_en == "Disp \"abcdefghijklmnopqrstuvwxyz\"");
        assert(hex == "3700de2abbb0bbb1bbb2bbb3bbb4bbb5bbb6bbb7bbb8bbb9bbbabbbcbbbdbbbebbbfbbc0bbc1bbc2bbc3bbc4bbc5bbc6bbc7bbc8bbc9bbca2a");
    }

    {
        // Test string interpolation behaviour
        TIVarFile testPrgm = TIVarFile::createNew("Program", "INTERP");
        testPrgm.setContentFromString(R"(A and B:Disp "A and B":Send("SET SOUND eval(A and B) TIME 2)");
        string detok_fr = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});
        string detok_en = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_EN}});
        assert(detok_en == R"(A and B:Disp "A and B":Send("SET SOUND eval(A and B) TIME 2)");
        assert(detok_fr == R"(A et B:Disp "A and B":Envoi("SET SOUND eval(A et B) TIME 2)");
    }

    {
        // Test tokenization exceptions
        TIVarFile testPrgm = TIVarFile::createNew("Program", "FOOBAR");
        testPrgm.setContentFromString(R"(Disp "WHITE,ʟWHITE,prgmWHITE",WHITE,ʟWHITE:prgmWHITE:prgmABCDEF)");
        string detok_fr = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});
        string detok_en = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_EN}});
        assert(detok_en == R"(Disp "WHITE,ʟWHITE,prgmWHITE",WHITE,ʟWHITE:prgmWHITE:prgmABCDEF)");
        assert(detok_fr == R"(Disp "WHITE,ʟWHITE,prgmWHITE",BLANC,ʟWHITE:prgmWHITE:prgmABCDEF)");
        // While this is visually fine, the "prgm" inside the token should probably be the token, not p r g m ...
    }

    {
        // Test tokenization exceptions in an interpolated string
        TIVarFile testPrgm = TIVarFile::createNew("Program", "FOOBAR");
        testPrgm.setContentFromString(R"(Send("SET SOUND eval(A and prgmWHITE) TIME 2)");
        string detok_fr = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});
        string detok_en = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_EN}});
        assert(detok_en == R"(Send("SET SOUND eval(A and prgmWHITE) TIME 2)");
        assert(detok_fr == R"(Envoi("SET SOUND eval(A et prgmWHITE) TIME 2)");
    }

    {
        TIVarFile clibs = TIVarFile::loadFromFile("testData/clibs.8xg");
        const auto& entries = clibs.getVarEntries();
        assert(entries.size() == 9);
        for (int i=0; i<9; i++) {
            assert(entries[i].typeID == TIVarType{"AppVar"}.getId());
        }
        assert((char*)entries[1].varname == "GRAPHX"s);
        cout << clibs.getReadableContent() << "\n" << endl;
    }

    {
        TIVarFile groupObject = TIVarFile::createNew("GroupObject", "GROUP");
        groupObject.setContentFromString(R"({
    "entries": [
        {
            "typeName": "String",
            "name": "Str1",
            "readableContent": "Hello Group"
        },
        {
            "typeName": "AppVar",
            "name": "DATA",
            "readableContent": "ABCD1234"
        },
        {
            "typeName": "Real",
            "name": "A",
            "readableContent": "42"
        }
    ]
})");

        const json groupObjectJSON = json::parse(groupObject.getReadableContent());
        assert(groupObjectJSON["entries"].size() == 3);
        assert(groupObjectJSON["entries"][0]["typeName"] == "String");
        assert(groupObjectJSON["entries"][0]["name"] == "Str1");
        assert(groupObjectJSON["entries"][0]["readableContent"] == "Hello Group");
        assert(groupObjectJSON["entries"][1]["typeName"] == "AppVar");
        assert(groupObjectJSON["entries"][1]["name"] == "DATA");
        assert(groupObjectJSON["entries"][1]["readableContent"] == "ABCD1234");
        assert(groupObjectJSON["entries"][2]["typeName"] == "Real");
        assert(groupObjectJSON["entries"][2]["name"] == "A");
        assert(groupObjectJSON["entries"][2]["readableContent"] == 42);

        TIVarFile recreatedGroupObject = TIVarFile::createNew("GroupObject", "GROUP");
        recreatedGroupObject.setContentFromString(groupObject.getReadableContent());
        assert(recreatedGroupObject.getRawContent() == groupObject.getRawContent());

        const std::string groupObjectPath = groupObject.saveVarToFile("/tmp", "GROUPOBJ");
        TIVarFile reloadedGroupObject = TIVarFile::loadFromFile(groupObjectPath);
        assert(reloadedGroupObject.getReadableContent() == groupObject.getReadableContent());
        assert(remove(groupObjectPath.c_str()) == 0);

        TIVarFile artifi82 = TIVarFile::loadFromFile("testData/ARTIFI82.8cg");
        const json artifi82JSON = json::parse(artifi82.getReadableContent());
        assert(artifi82JSON["entries"].size() == 570);
        assert(artifi82JSON["entries"][0]["typeName"] == "PythonAppVar");
        assert(artifi82JSON["entries"][0]["name"] == "00");
        assert(artifi82JSON["entries"][1]["name"] == "01");
        assert(artifi82JSON["entries"][37].contains("name") == false);
        assert(artifi82JSON["entries"][569]["nameHex"] == "4380000000000000");
    }

    {
        TIVarFile testAppVar = TIVarFile::createNew("AppVar", "TEST");
        testAppVar.setContentFromString("ABCD1234C9C8C7C6"); // random but valid hex string
        assert(testAppVar.getReadableContent() == "ABCD1234C9C8C7C6");
        assert(testAppVar.getRawContent().size() == strlen("ABCD1234C9C8C7C6") / 2 + 2);
        testAppVar.saveVarToFile("testData", "testAVnew");
    }

    {
        TIVarFile testString = TIVarFile::loadFromFile("testData/String.8xs");
        assert(testString.getReadableContent() == "Hello World");
    }

    {
        TIVarFile testPrgmQuotes = TIVarFile::loadFromFile("testData/testPrgmQuotes.8xp");
        cout << "testPrgmQuotes.getReadableContent() : " << testPrgmQuotes.getReadableContent() << endl;
        assert(testPrgmQuotes.getReadableContent() == "Pause \"2 SECS\",2");
    }

    {
        TIVarFile testEquation = TIVarFile::loadFromFile("testData/Equation_Y1T.8xy");
        assert(testEquation.getReadableContent() == "3sin(T)+4");
    }

    {
        TIVarFile testReal = TIVarFile::loadFromFile("testData/Real.8xn");
        assert(testReal.getRawContent() == data_t({0x80,0x81,0x42,0x13,0x37,0x00,0x00,0x00,0x00}));
        assert(testReal.getRawContentHexStr() == "808142133700000000");
        assert(testReal.getReadableContent() == "-42.1337");
        testReal.setContentFromString("5");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80,0x50,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "5");
        testReal.setContentFromString(".5");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x7F,0x50,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "0.5");
        testReal.setContentFromString(".000000999999999999999e105");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80+99,0x10,0x00,0x00,0x00,0x00,0x00,0x00}));
        assert(testReal.getReadableContent() == "1e99");
        testReal.setContentFromString("3.14159265358979323846264");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x00,0x80   ,0x31,0x41,0x59,0x26,0x53,0x58,0x98}));
        assert(testReal.getReadableContent() == "3.1415926535898");
        testReal.setContentFromString("-1234567890123456789e+12");
        cout << "testReal.getReadableContent() : " << testReal.getReadableContent() << endl;
        assert(testReal.getRawContent() == data_t({0x80,0x80+30,0x12,0x34,0x56,0x78,0x90,0x12,0x35}));
        assert(testReal.getReadableContent() == "-1.2345678901235e30");
    }

#ifndef __EMSCRIPTEN__
    {
        try
        {
            data_t bad_real = data_t({1, 0, 0, 0, 0, 0, 0, 0, 0});
            TH_GenericReal::makeStringFromData(bad_real);
            assert(false);
        }
        catch (exception& e)
        {
            cout << "Caught expected exception: " << e.what() << endl;
        }
    }
#endif

    {
        TIVarFile testReal42 = TIVarFile::createNew("Real", "R");
        testReal42.setCalcModel("84+");
        testReal42.setContentFromString("9001.42");
        cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
        assert(testReal42.getReadableContent() == "9001.42");
        testReal42.setContentFromString("-0.00000008");
        cout << "testReal42.getReadableContent() : " << testReal42.getReadableContent() << endl;
        assert(atof(testReal42.getReadableContent().c_str()) == -8e-08);
        testReal42.saveVarToFile("testData", "Real_new");
    }

    {
        string test = "Disp 42\nInput A,\"?\":For(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
        const std::string reindented = TH_Tokenized::reindentCodeString(test);
        cout << "Indented code:" << endl << reindented << endl;
        const std::string expected = R"(Disp 42
Input A,"?"
For(I,1,10)
Then
   Disp I
   For(J,1,10)
   Then
      Disp J
   End
End)";
        assert(reindented == expected);
    }

    {
        string test = "If A:Then\nDisp 1\nEnd";
        const std::string reindented = TH_Tokenized::reindentCodeString(test);
        cout << "Indented code:" << endl << reindented << endl;
        const std::string expected = R"(If A
Then
   Disp 1
End)";
        assert(reindented == expected);
    }

    {
        string test = "u(𝒏-2):³√(9";
        const std::string reindented = TH_Tokenized::reindentCodeString(test);
        cout << "Indented code:" << endl << reindented << endl;
        const std::string expected = R"(u(𝒏-2)
³√(9)";
        assert(reindented == expected);
    }

    {
        string test = "   Disp 42\nInput A,\"?\":  For(I,1,10)\n Then\n \xA0 Disp I:For(J,1,10)\nThen\n Disp J\nEnd\nEnd";
        const std::string reindented = TH_Tokenized::reindentCodeString(test);
        cout << "Indented code:" << endl << reindented << endl;
        const std::string expected = R"(Disp 42
Input A,"?"
For(I,1,10)
Then
   Disp I
   For(J,1,10)
   Then
      Disp J
   End
End)";
        assert(reindented == expected);
    }

    {
        string test = "Disp 42\nInput A,\"?\":For(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
        const std::string reindented = TH_Tokenized::reindentCodeString(test, {{"indent_n", 8}});
        cout << "Indented code:" << endl << reindented << endl;
        const std::string expected = R"(Disp 42
Input A,"?"
For(I,1,10)
Then
        Disp I
        For(J,1,10)
        Then
                Disp J
        End
End)";
        assert(reindented == expected);
    }

{
    string test = "Disp 42\nInput A,\"?\":For(I,1,10)\nThen\nDisp I:For(J,1,10)\nThen\nDisp J\nEnd\nEnd";
    const std::string reindented = TH_Tokenized::reindentCodeString(test, {{"indent_char", TH_Tokenized::INDENT_CHAR_TAB}});
    cout << "Indented code:" << endl << reindented << endl;
    const std::string expected = R"(Disp 42
Input A,"?"
For(I,1,10)
Then
	Disp I
	For(J,1,10)
	Then
		Disp J
	End
End)";
    assert(reindented == expected);
}

    {
        TIVarFile testPrgmReindent = TIVarFile::createNew("Program", "asdf");
        testPrgmReindent.setContentFromString("\"http://TIPlanet.org");
        assert(trim(testPrgmReindent.getReadableContent({{"prettify", true}, {"reindent", true}})) == "\"http://TIPlanet.org");
    }

#ifndef __EMSCRIPTEN__
    {
        try
        {
            auto goodTypeForCalc = TIVarFile::createNew("Program", "Bla", "83PCE");
        } catch (exception& e) {
            cout << "Caught unexpected exception: " << e.what() << endl;
            assert(false);
        }
        try
        {
            auto badTypeForCalc = TIVarFile::createNew("ExactComplexFrac", "Bla", "84+");
            assert(false);
        } catch (exception& e) {
            cout << "Caught expected exception: " << e.what() << endl;
        }
    }
#endif

    assert(TIVarType{"ExactRealPi"}.getId() == 32);

    {
        TIVarFile testPrgm = TIVarFile::loadFromFile("testData/Program.8xp");
        cout << "testPrgm.getHeader().entries_len = " << testPrgm.getHeader().entries_len
             << "\t testPrgm.size() - 57 == " << (testPrgm.size() - 57) << endl;
        assert(testPrgm.getHeader().entries_len == testPrgm.size() - 57);
        string testPrgmcontent = testPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});

        TIVarFile newPrgm = TIVarFile::createNew("Program");
        newPrgm.setContentFromString(testPrgmcontent);
        string newPrgmcontent = newPrgm.getReadableContent({{"lang", TH_Tokenized::LANG_FR}});

        assert(testPrgmcontent == newPrgmcontent);
        newPrgm.saveVarToFile("testData", "Program_new");

        cout << endl << "testPrgmcontent : " << endl << testPrgmcontent << endl;
        cout << "\n\n\n" << endl;
    }

    {
        TIVarFile testPrgm42 = TIVarFile::createNew("Program", "asdf");
        testPrgm42.setCalcModel("82A");
        testPrgm42.setContentFromString("Grande blabla:Disp \"Grande blabla");
        testPrgm42.setVarName("MyProgrm");
        assert(testPrgm42.getReadableContent() == "Grande blabla:Disp \"Grande blabla");
        testPrgm42.saveVarToFile("testData", "testMinTok_new");
        testPrgm42.setArchived(true);
        testPrgm42.saveVarToFile("testData", "testMinTok_archived_new");
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew("Program", "asdf");
        testPrgm.setContentFromString("Pause 42:Pause 43:Disp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK");
        string testPrgmcontent = testPrgm.getReadableContent({{"prettify", true},  {"reindent", true}});
        assert(trim(testPrgmcontent) == "Pause 42\nPause 43\nDisp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK");
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew("Program", "asdf");
        testPrgm.setContentFromString("   Pause 42:Pause 43:\tDisp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK", { {"deindent", true} });
        string testPrgmcontent = testPrgm.getReadableContent({{"prettify", true},  {"reindent", true}});
        assert(testPrgmcontent == "Pause 42\nPause 43\nDisp \"\",\"Bouh la =/*: déf\",\"suite :\",\" OK");
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew("Program", "asdf");
        testPrgm.setContentFromString("   Pause 42:Pause 43", { {"deindent", false} });
        string testPrgmcontent = testPrgm.getReadableContent();
        assert(testPrgmcontent == "   Pause 42:Pause 43");
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew("Program", "asdf");
        options_t options;
        options["detect_strings"] = true;
        testPrgm.setContentFromString("\"prgm", options);
        assert(testPrgm.getRawContent() == data_t({0x09, 0x00, 0x2A, 0xBB, 0xC0, 0xBB, 0xC2, 0xBB, 0xB6, 0xBB, 0xBD}));
    }

    {
        TIVarFile testPrgm = TIVarFile::createNew("Program", "asdf");
        options_t options;
        options["detect_strings"] = false;
        testPrgm.setContentFromString("\"prgm", options);
        assert(testPrgm.getRawContent() == data_t({0x02, 0x00, 0x2A, 0x5F}));
    }

    {
        TIVarFile testRealList = TIVarFile::loadFromFile("testData/RealList.8xl");
        cout << "Before: " << testRealList.getReadableContent() << "\n   Now: ";
        testRealList.setContentFromString("{9, 0, .5, -6e-8}");
        cout << testRealList.getReadableContent() << "\n";
        testRealList.saveVarToFile("testData", "RealList_new");
    }

    {
        TIVarFile testRealList = TIVarFile::createNew("RealList", "\x5D\x00"); // L₁
        assert(testRealList.getRawContent().empty());
        const std::string content = "{9,0,0.5,-6e-8}";
        testRealList.setContentFromString(content);
        assert(testRealList.getReadableContent() == content);
    }

    {
        TIVarFile testStandardMatrix = TIVarFile::loadFromFile("testData/Matrix_3x3_standard.8xm");
        cout << "Before: " << testStandardMatrix.getReadableContent() << "\n   Now: ";
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9]]");
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7.5,-8,-9][1,2,3][4,5,6][-0.002,-8,-9]]");
        cout << testStandardMatrix.getReadableContent() << "\n";
        testStandardMatrix.saveVarToFile("testData", "Matrix_new");
    }

#ifndef __EMSCRIPTEN__
    {
        try
        {
            TIVarFile::createNew("Program", "1BADNAME");
            assert(false);
        }
        catch (const invalid_argument&)
        {
        }

        try
        {
            TIVarFile badMatrix = TIVarFile::createNew("Matrix", "\x5C\x00");
            badMatrix.setContentFromString("[[1,2][3]]");
            assert(false);
        }
        catch (const invalid_argument&)
        {
        }

        try
        {
            TH_Tokenized::getPosInfoAtOffsetFromHexStr("123", 0);
            assert(false);
        }
        catch (const invalid_argument&)
        {
        }
    }
#endif

    {
        TIVarFile testComplex = TIVarFile::loadFromFile("testData/Complex.8xc"); // -5 + 2i
        cout << "Before: " << testComplex.getReadableContent() << "\n   Now: ";
        assert(testComplex.getReadableContent() == "-5+2i");
        TIVarFile newComplex = TIVarFile::createNew("Complex", "C");
        newComplex.setContentFromString("-5+2i");
        assert(newComplex.getRawContent() == newComplex.getRawContent());
        newComplex.setContentFromString("2.5+0.001i");
        cout << "After: " << newComplex.getReadableContent() << endl;
        testComplex.saveVarToFile("testData", "Complex_new");
    }

    {
        TIVarFile testComplexList = TIVarFile::loadFromFile("testData/ComplexList.8xl");
        cout << "Before: " << testComplexList.getReadableContent() << "\n   Now: ";
        testComplexList.setContentFromString("{9+2i, 0i, .5, -0.5+6e-8i}");
        cout << testComplexList.getReadableContent() << "\n";
        testComplexList.saveVarToFile("testData", "ComplexList_new");
    }

    {
        TIVarFile testExact_RealRadical = TIVarFile::loadFromFile("testData/Exact_RealRadical.8xn");
        cout << "Before: " << testExact_RealRadical.getReadableContent() << endl;
        assert(testExact_RealRadical.getReadableContent() == "(41*√(789)+14*√(654))/259");
        TIVarFile newExact_RealRadical = TIVarFile::createNew("ExactRealRadical", "A", "83PCE");
        newExact_RealRadical.setContentFromString("(41*√(789)+14*√(654))/259");
        assert(testExact_RealRadical.getRawContent() == newExact_RealRadical.getRawContent());
        assert(newExact_RealRadical.getVarEntries()[0].version == 0x10);
    }

    {
        TIVarFile testExactComplexFrac = TIVarFile::loadFromFile("testData/Exact_ComplexFrac.8xc");
        cout << "Before: " << testExactComplexFrac.getReadableContent() << endl;
        assert(testExactComplexFrac.getReadableContent() == "1/5-2/5i");
        TIVarFile newExactComplexFrac = TIVarFile::createNew("ExactComplexFrac", "A", "83PCE");
        newExactComplexFrac.setContentFromString("1/5-2/5i");
        assert(testExactComplexFrac.getRawContent() == newExactComplexFrac.getRawContent());
        assert(newExactComplexFrac.getVarEntries()[0].version == 0x0B);
    }


    {
        TIVarFile testExactComplexPi = TIVarFile::loadFromFile("testData/Exact_ComplexPi.8xc");
        cout << "Before: " << testExactComplexPi.getReadableContent() << endl;
        assert(testExactComplexPi.getReadableContent() == "1/5-3πi");
        TIVarFile newExactComplexPi = TIVarFile::createNew("ExactComplexPi", "A", "83PCE");
        newExactComplexPi.setContentFromString("1/5-3πi");
        assert(testExactComplexPi.getRawContent() == newExactComplexPi.getRawContent());
        assert(newExactComplexPi.getVarEntries()[0].version == 0x10);
    }

    {
        TIVarFile testExactComplexPiFrac = TIVarFile::loadFromFile("testData/Exact_ComplexPiFrac.8xc");
        cout << "Before: " << testExactComplexPiFrac.getReadableContent() << endl;
        assert(testExactComplexPiFrac.getReadableContent() == "2π/7i");
        TIVarFile newExactComplexPiFrac = TIVarFile::createNew("ExactComplexPiFrac", "A", "83PCE");
        newExactComplexPiFrac.setContentFromString("2π/7i");
        assert(testExactComplexPiFrac.getRawContent() == newExactComplexPiFrac.getRawContent());
        assert(newExactComplexPiFrac.getVarEntries()[0].version == 0x10);
    }

    {
        TIVarFile testExactComplexRadical = TIVarFile::loadFromFile("testData/Exact_ComplexRadical.8xc");
        cout << "Before: " << testExactComplexRadical.getReadableContent() << endl;

        assert(testExactComplexRadical.getReadableContent() == "(√(6)+√(2))/4+(√(6)-√(2))/4i");
        TIVarFile newExactComplexRadical = TIVarFile::createNew("ExactComplexRadical", "A", "83PCE");
        newExactComplexRadical.setContentFromString("(√(6)+√(2))/4+(√(6)-√(2))/4i");
        assert(testExactComplexRadical.getRawContent() == newExactComplexRadical.getRawContent());
        assert(newExactComplexRadical.getVarEntries()[0].version == 0x10);
    }

    {
        TIVarFile testExactRealPi = TIVarFile::loadFromFile("testData/Exact_RealPi.8xn");
        cout << "Before: " << testExactRealPi.getReadableContent() << endl;
        assert(testExactRealPi.getReadableContent() == "30π");
        TIVarFile newExactRealPi = TIVarFile::createNew("ExactRealPi", "A", "83PCE");
        newExactRealPi.setContentFromString("30π");
        assert(testExactRealPi.getRawContent() == newExactRealPi.getRawContent());
        assert(newExactRealPi.getVarEntries()[0].version == 0x10);
    }

    {
        TIVarFile testExactRealPiFrac = TIVarFile::loadFromFile("testData/Exact_RealPiFrac.8xn");
        cout << "Before: " << testExactRealPiFrac.getReadableContent() << endl;
        assert(testExactRealPiFrac.getReadableContent() == "2π/7");
        TIVarFile newExactRealPiFrac = TIVarFile::createNew("ExactRealPiFrac", "A", "83PCE");
        newExactRealPiFrac.setContentFromString("2π/7");
        assert(testExactRealPiFrac.getRawContent() == newExactRealPiFrac.getRawContent());
        assert(newExactRealPiFrac.getVarEntries()[0].version == 0x10);
    }

    {
        //                   ...size...  prgm  #name   C     O     U     R     A     G     E   ..offset..    D     <     2
        data_t tmp = data_t({0x0e, 0x00, 0x05, 0x07, 0x43, 0x4f, 0x55, 0x52, 0x41, 0x47, 0x45, 0x43, 0x02, 0x44, 0x6b, 0x32});
        std::string tempEquIfCond = TH_TempEqu::makeStringFromData(tmp);
        assert(tempEquIfCond == "prgmCOURAGE:579:D<2");
    }

    {
        TIVarFile appvarTest = TIVarFile::loadFromFile("testData/AppVar.8xv");
        cout << "appvarTest.getReadableContent() : " << appvarTest.getReadableContent() << endl;
    }

    {
        TIVarFile pythonModule = TIVarFile::createNew("PythonModuleAppVar", "PYMOD", "83PCE");
        pythonModule.setContentFromString(R"({
    "typeName": "PythonModuleAppVar",
    "version": 2,
    "menuDefinitions": "#MENULABEL Demo\n#MENUITEM sin|sin(\n",
    "menuDefinitionsNullTerminated": true,
    "compiledDataHex": "4D500300"
})");
        const json pythonModuleJSON = json::parse(pythonModule.getReadableContent());
        assert(pythonModuleJSON["typeName"] == "PythonModuleAppVar");
        assert(pythonModuleJSON["subtype"] == "PythonModule");
        assert(pythonModuleJSON["version"] == 2);
        assert(pythonModuleJSON["menuDefinitions"] == "#MENULABEL Demo\n#MENUITEM sin|sin(\n");
        assert(pythonModuleJSON["compiledDataHex"] == "4D500300");

        const std::string modulePath = pythonModule.saveVarToFile("/tmp", "PYMOD");
        TIVarFile reloadedModule = TIVarFile::loadFromFile(modulePath);
        assert(reloadedModule.getVarEntries()[0]._type.getName() == "PythonModuleAppVar");
        assert(reloadedModule.getRawContent() == pythonModule.getRawContent());
        assert(remove(modulePath.c_str()) == 0);
    }

    {
        TIVarFile pythonImage = TIVarFile::createNew("PythonImageAppVar", "PYIMG", "83PCE");
        pythonImage.setContentFromString(R"({
    "typeName": "PythonImageAppVar",
    "width": 16,
    "height": 8,
    "palette": {
        "hasAlpha": true,
        "transparentIndex": 1,
        "entries": [31, 63488, 2016]
    },
    "imageDataHex": "01020304"
})");
        const json pythonImageJSON = json::parse(pythonImage.getReadableContent());
        assert(pythonImageJSON["typeName"] == "PythonImageAppVar");
        assert(pythonImageJSON["width"] == 16);
        assert(pythonImageJSON["height"] == 8);
        assert(pythonImageJSON["palette"]["entryCount"] == 3);
        assert(pythonImageJSON["palette"]["hasAlpha"] == true);
        assert(pythonImageJSON["palette"]["entries"][1] == 63488);
        assert(pythonImageJSON["imageDataLength"] == 4);
        assert(pythonImageJSON["imageDataHex"] == "01020304");

        const std::string imagePath = pythonImage.saveVarToFile("/tmp", "PYIMG");
        TIVarFile reloadedImage = TIVarFile::loadFromFile(imagePath);
        assert(reloadedImage.getVarEntries()[0]._type.getName() == "PythonImageAppVar");
        assert(reloadedImage.getRawContent() == pythonImage.getRawContent());
        assert(remove(imagePath.c_str()) == 0);

        TIVarFile realPythonImage = TIVarFile::loadFromFile("testData/TESTIM8C.8xv");
        assert(realPythonImage.getVarEntries()[0]._type.getName() == "PythonImageAppVar");
        const json realPythonImageJSON = json::parse(realPythonImage.getReadableContent());
        assert(realPythonImageJSON["width"] == 154);
        assert(realPythonImageJSON["height"] == 42);
        assert(realPythonImageJSON["palette"]["marker"] == 1);
        assert(realPythonImageJSON["palette"]["entryCount"] == realPythonImageJSON["palette"]["entries"].size());
        assert(realPythonImageJSON["imageDataLength"] > 0);

        TIVarFile rebuiltRealImage = TIVarFile::createNew("PythonImageAppVar", "TESTIM8C", "83PCE");
        rebuiltRealImage.setContentFromString(realPythonImage.getReadableContent());
        assert(rebuiltRealImage.getRawContent() == realPythonImage.getRawContent());
    }

    {
        TIVarFile genericStructuredAppVar = TIVarFile::createNew("AppVar", "SCSET", "83PCE");
        genericStructuredAppVar.setContentFromString(R"({
    "typeName": "StudyCardsSetgsAppVar",
    "keepKnownCards": true,
    "reintroduceCards": false,
    "shuffleCards": true,
    "ignoreLevels": true,
    "animateFlip": false,
    "boxMode5": true,
    "currentAppVar": "CARDS"
})");
        assert(genericStructuredAppVar.getVarEntries()[0]._type.getName() == "StudyCardsSetgsAppVar");
        const json studyCardsSettingsJSON = json::parse(genericStructuredAppVar.getReadableContent());
        assert(studyCardsSettingsJSON["typeName"] == "StudyCardsSetgsAppVar");
        assert(studyCardsSettingsJSON["keepKnownCards"] == true);
        assert(studyCardsSettingsJSON["shuffleCards"] == true);
        assert(studyCardsSettingsJSON["ignoreLevels"] == true);
        assert(studyCardsSettingsJSON["currentAppVar"] == "CARDS");
    }

    {
        TIVarFile cellSheet = TIVarFile::createNew("CellSheetAppVar", "SHEET", "83PCE");
        cellSheet.setContentFromString(R"({
    "typeName": "CellSheetAppVar",
    "name": "CELLS",
    "displayHelp": false,
    "displayEquationPreview": true,
    "number": 7,
    "payloadHex": "AABBCCDD"
})");
        const json cellSheetJSON = json::parse(cellSheet.getReadableContent());
        assert(cellSheetJSON["typeName"] == "CellSheetAppVar");
        assert(cellSheetJSON["name"] == "CELLS");
        assert(cellSheetJSON["displayHelp"] == false);
        assert(cellSheetJSON["displayEquationPreview"] == true);
        assert(cellSheetJSON["number"] == 7);
        assert(cellSheetJSON["payloadHex"] == "AABBCCDD");
    }

    {
        TIVarFile cabriFile = TIVarFile::createNew("CabriJrAppVar", "CABF", "83PCE");
        cabriFile.setContentFromString(R"({
    "typeName": "CabriJrAppVar",
    "variant": "File",
    "structure": 4,
    "unknownBeforeWordHex": "AA",
    "unknownWord": 4660,
    "unknownAfterWordHex": "BB",
    "nameOffsetUnits": 3,
    "dataHex": "01020304"
})");
        const json cabriFileJSON = json::parse(cabriFile.getReadableContent());
        assert(cabriFileJSON["typeName"] == "CabriJrAppVar");
        assert(cabriFileJSON["variant"] == "File");
        assert(cabriFileJSON["compressed"] == false);
        assert(cabriFileJSON["structure"] == 4);
        assert(cabriFileJSON["unknownBeforeWordHex"] == "AA");
        assert(cabriFileJSON["unknownWord"] == 4660);
        assert(cabriFileJSON["unknownAfterWordHex"] == "BB");
        assert(cabriFileJSON["nameOffsetUnits"] == 3);
        assert(cabriFileJSON["nameOffset"] == 63);
        assert(cabriFileJSON["dataHex"] == "01020304");

        TIVarFile genericCabriFile = TIVarFile::createNew("AppVar", "GCBF", "83PCE");
        genericCabriFile.setContentFromString(cabriFile.getReadableContent());
        assert(genericCabriFile.getVarEntries()[0]._type.getName() == "CabriJrAppVar");
        assert(genericCabriFile.getRawContent() == cabriFile.getRawContent());
    }

    {
        TIVarFile cabriCompressed = TIVarFile::createNew("CabriJrAppVar", "CABC", "83PCE");
        cabriCompressed.setContentFromString(R"({
    "typeName": "CabriJrAppVar",
    "variant": "File",
    "structure": 3,
    "unreadHex": "99",
    "nameOffsetUnits": 4,
    "entryOffsetUnits": 32,
    "blocksHex": ["000102030405060708090A0B0C0D0E0F1011"],
    "trailingWord": 48879,
    "trailingDataHex": "AA55"
})");
        const json cabriCompressedJSON = json::parse(cabriCompressed.getReadableContent());
        assert(cabriCompressedJSON["variant"] == "File");
        assert(cabriCompressedJSON["compressed"] == true);
        assert(cabriCompressedJSON["structure"] == 3);
        assert(cabriCompressedJSON["unreadHex"] == "99");
        assert(cabriCompressedJSON["nameOffsetUnits"] == 4);
        assert(cabriCompressedJSON["entryOffsetUnits"] == 32);
        assert(cabriCompressedJSON["blockCount"] == 1);
        assert(cabriCompressedJSON["blocksHex"][0] == "000102030405060708090A0B0C0D0E0F1011");
        assert(cabriCompressedJSON["trailingWord"] == 48879);
        assert(cabriCompressedJSON["trailingDataHex"] == "AA55");
    }

    {
        TIVarFile cabriLanguage = TIVarFile::createNew("CabriJrAppVar", "CABL", "83PCE");
        cabriLanguage.setContentFromString(R"({
    "typeName": "CabriJrAppVar",
    "variant": "Language",
    "languageId": "ENG",
    "lines": ["MAIN,SUB", "HELP LINE", ""]
})");
        const json cabriLanguageJSON = json::parse(cabriLanguage.getReadableContent());
        assert(cabriLanguageJSON["variant"] == "Language");
        assert(cabriLanguageJSON["unknownHex"] == "015F");
        assert(cabriLanguageJSON["languageId"] == "ENG");
        assert(cabriLanguageJSON["text"] == "MAIN,SUB\rHELP LINE\r");
        assert(cabriLanguageJSON["lines"].size() == 3);
        assert(cabriLanguageJSON["lines"][0] == "MAIN,SUB");
        assert(cabriLanguageJSON["lines"][2] == "");
    }

    {
        TIVarFile studyCards = TIVarFile::createNew("StudyCardsAppVar", "STUDY", "84+");
        studyCards.setContentFromString(R"({
    "typeName": "StudyCardsAppVar",
    "rawDataHex": "F347BFA7010000001400160018001A00010203004100420043004400"
})");
        const json studyCardsJSON = json::parse(studyCards.getReadableContent());
        assert(studyCardsJSON["typeName"] == "StudyCardsAppVar");
        assert(studyCardsJSON["version"] == 1);
        assert(studyCardsJSON["cardCount"] == 0);
        assert(studyCardsJSON["titles"][0] == "A");
        assert(studyCardsJSON["titles"][3] == "D");
    }

    {
        TIVarFile notefolio = TIVarFile::createNew("NotefolioAppVar", "NOTES", "83+");
        notefolio.setContentFromString(R"({
    "typeName": "NotefolioAppVar",
    "reservedHex": "00000000",
    "name": "NOTES",
    "headerUnknownHex": "010203040506",
    "text": "Hello NoteFolio",
    "textNullTerminated": true,
    "trailingDataHex": "AA55"
})");
        assert(notefolio.getVarEntries()[0]._type.getName() == "NotefolioAppVar");
        const json notefolioJSON = json::parse(notefolio.getReadableContent());
        assert(notefolioJSON["typeName"] == "NotefolioAppVar");
        assert(notefolioJSON["subtype"] == "Notefolio");
        assert(notefolioJSON["magic"] == "F347BFAF");
        assert(notefolioJSON["reservedHex"] == "00000000");
        assert(notefolioJSON["name"] == "NOTES");
        assert(notefolioJSON["storedTextLength"] == 16);
        assert(notefolioJSON["headerUnknownHex"] == "010203040506");
        assert(notefolioJSON["text"] == "Hello NoteFolio");
        assert(notefolioJSON["textNullTerminated"] == true);
        assert(notefolioJSON["textDataHex"] == "48656C6C6F204E6F7465466F6C696F00");
        assert(notefolioJSON["trailingDataHex"] == "AA55");

        TIVarFile genericNotefolio = TIVarFile::createNew("AppVar", "NFGEN", "83+");
        genericNotefolio.setContentFromString(notefolio.getReadableContent());
        assert(genericNotefolio.getVarEntries()[0]._type.getName() == "NotefolioAppVar");
        assert(genericNotefolio.getRawContent() == notefolio.getRawContent());

        TIVarFile rawNotefolio = TIVarFile::createNew("NotefolioAppVar", "RAWNOTES", "83+");
        rawNotefolio.setContentFromString(R"({
    "typeName": "NotefolioAppVar",
    "rawDataHex": "F347BFAF000000004E4F544553000000100001020304050648656C6C6F204E6F7465466F6C696F00AA55"
})");
        assert(rawNotefolio.getRawContent() == notefolio.getRawContent());
    }

    {
        TIVarFile testPython = TIVarFile::createNew("PythonAppVar", "TEST123", "83PCE");
        testPython.setContentFromString("from math import *\nprint(math)\n\n# plop");
        testPython.saveVarToFile("testData", "Pythontest_new");

        TIVarFile pythonFromTest = TIVarFile::loadFromFile("testData/Pythontest_new.8xv");
        cout << "pythonFromTest.getReadableContent() : " << pythonFromTest.getReadableContent() << endl;
        assert(pythonFromTest.getReadableContent() == "from math import *\nprint(math)\n\n# plop");

        TIVarFile pythonFromTest2 = TIVarFile::loadFromFile("testData/python_HELLO.8xv");
        cout << "pythonFromTest2.getReadableContent() : " << pythonFromTest2.getReadableContent() << endl;
        assert(pythonFromTest2.getReadableContent() == "import sys\nprint(sys.version)\n");

        const json pythonMetadata = json::parse(pythonFromTest2.getReadableContent({{"metadata", true}}));
        assert(pythonMetadata["typeName"] == "PythonAppVar");
        assert(pythonMetadata["magic"] == "PYCD");
        assert(pythonMetadata["metadataRecordCount"] == 0);
        assert(pythonMetadata["code"] == "import sys\nprint(sys.version)\n");
        assert(pythonMetadata["rawDataHex"] == "5059434400696D706F7274207379730A7072696E74287379732E76657273696F6E290A");
    }

    {
        TIVarFile pythonBom = TIVarFile::createNew("PythonAppVar", "BOMTEST", "83PCE");
        pythonBom.setContentFromString(std::string("\xEF\xBB\xBF") + "print(42)\n");
        assert(pythonBom.getReadableContent() == "print(42)\n");

        TIVarFile pythonWithMetadata = TIVarFile::createNew("PythonAppVar", "METAPY", "83PCE");
        pythonWithMetadata.setContentFromString(R"({
    "typeName": "PythonAppVar",
    "filename": "hello.py",
    "code": "print(\"hi\")\n",
    "appendTrailingCRLF": true
})");
        const json pythonWithMetadataJSON = json::parse(pythonWithMetadata.getReadableContent({{"metadata", true}}));
        assert(pythonWithMetadataJSON["filename"] == "hello.py");
        assert(pythonWithMetadataJSON["metadataRecordCount"] == 1);
        assert(pythonWithMetadataJSON["metadataRecords"][0]["type"] == 1);
        assert(pythonWithMetadataJSON["metadataRecords"][0]["name"] == "hello.py");
        assert(pythonWithMetadataJSON["code"] == "print(\"hi\")\n");

        const std::string pythonWithoutMetadata = pythonWithMetadata.getReadableContent({{"metadata", false}});
        assert(pythonWithoutMetadata == "print(\"hi\")\n");

        TIVarFile pythonFromMetadataJSON = TIVarFile::createNew("PythonAppVar", "METAPY", "83PCE");
        pythonFromMetadataJSON.setContentFromString(pythonWithMetadataJSON.dump());
        assert(pythonFromMetadataJSON.getRawContent() == pythonWithMetadata.getRawContent());

        TIVarFile oversizedPython = TIVarFile::createNew("PythonAppVar", "BIGPY", "83PCE");
        oversizedPython.setContentFromString(std::string(70000, 'A'));
        assert(oversizedPython.getRawContent().size() == 65514);
        assert(oversizedPython.getReadableContent().size() == 65507);
        assert(oversizedPython.getReadableContent() == std::string(65507, 'A'));

        TIVarFile oversizedPythonMetadata = TIVarFile::createNew("PythonAppVar", "BIGMETA", "83PCE");
        oversizedPythonMetadata.setContentFromString(R"({
    "typeName": "PythonAppVar",
    "filename": "really_long_name.py",
    "code": ")"s + std::string(70000, 'B') + R"(",
    "appendTrailingCRLF": true
})");
        const json oversizedPythonMetadataJSON = json::parse(oversizedPythonMetadata.getReadableContent({{"metadata", true}}));
        assert(oversizedPythonMetadata.getRawContent().size() == 65514);
        assert(oversizedPythonMetadataJSON["filename"] == "really_long_name.py");
        assert(oversizedPythonMetadataJSON["code"].get<std::string>().back() == '\n');
        assert(oversizedPythonMetadataJSON["code"].get<std::string>().size() == 65485);
    }

    {
        TIVarFile backupVar = TIVarFile::createNew("Backup", "BACKUP", "84+");
        backupVar.setContentFromString(R"({
    "typeName": "Backup",
    "addressOfData2": 4660,
    "data1Hex": "01020304",
    "data2Hex": "A0B0",
    "data3Hex": "C1D2E3"
})");
        const json backupJSON = json::parse(backupVar.getReadableContent());
        assert(backupJSON["typeName"] == "Backup");
        assert(backupJSON["segmentCount"] == 3);
        assert(backupJSON["addressOfData2"] == 4660);
        assert(backupJSON["data1Hex"] == "01020304");
        assert(backupJSON["data2Hex"] == "A0B0");
        assert(backupJSON["data3Hex"] == "C1D2E3");

        const std::string backupPath = backupVar.saveVarToFile("/tmp", "BACKUP");
        TIVarFile reloadedBackup = TIVarFile::loadFromFile(backupPath);
        assert(reloadedBackup.getVarEntries()[0]._type.getName() == "Backup");
        assert(reloadedBackup.getReadableContent() == backupVar.getReadableContent());
        assert(remove(backupPath.c_str()) == 0);
    }

    {
        TIVarFile testTheta = TIVarFile::createNew("Program", "θΘϴᶿ");
        uint8_t testThetaVarName[8] = {0x5B, 0x5B, 0x5B, 0x5B};
        const auto& firstVarEntry = testTheta.getVarEntries()[0];
        cout << "testTheta firstVarEntry varname : " << firstVarEntry.varname << endl;
        assert(std::equal(firstVarEntry.varname, firstVarEntry.varname + 8, testThetaVarName));
    }

    {
        TIVarFile listDefaultVar = TIVarFile::createNew("RealList");
        const auto& listDefault = listDefaultVar.getVarEntries()[0];
        const uint8_t expectedListDefault[8] = {0x5D, 0x00};
        assert(std::equal(listDefault.varname, listDefault.varname + 8, expectedListDefault));

        TIVarFile customList = TIVarFile::createNew("RealList", "abcde");
        const uint8_t expectedCustomList[8] = {0x5D, 'A', 'B', 'C', 'D', 'E'};
        assert(std::equal(customList.getVarEntries()[0].varname, customList.getVarEntries()[0].varname + 8, expectedCustomList));

        TIVarFile stdList = TIVarFile::createNew("RealList", "L6");
        const uint8_t expectedStdList[8] = {0x5D, 0x05};
        assert(std::equal(stdList.getVarEntries()[0].varname, stdList.getVarEntries()[0].varname + 8, expectedStdList));

        TIVarFile idList = TIVarFile::createNew("RealList", "IDList");
        const uint8_t expectedIdList[8] = {0x5D, 0x40};
        assert(std::equal(idList.getVarEntries()[0].varname, idList.getVarEntries()[0].varname + 8, expectedIdList));

        TIVarFile customDigitList = TIVarFile::createNew("RealList", "A1B2C");
        const uint8_t expectedCustomDigitList[8] = {0x5D, 'A', '1', 'B', '2', 'C'};
        assert(std::equal(customDigitList.getVarEntries()[0].varname, customDigitList.getVarEntries()[0].varname + 8, expectedCustomDigitList));

        TIVarFile namedComplexList = TIVarFile::createNew("ComplexList", "AB12");
        const uint8_t expectedNamedComplexList[8] = {0x5D, 'A', 'B', '1', '2'};
        assert(std::equal(namedComplexList.getVarEntries()[0].varname, namedComplexList.getVarEntries()[0].varname + 8, expectedNamedComplexList));

        bool threwInvalidList = false;
        try
        {
            (void)TIVarFile::createNew("RealList", "1ABC");
        }
        catch (const std::invalid_argument&)
        {
            threwInvalidList = true;
        }
        assert(threwInvalidList);

        bool threwTooLongList = false;
        try
        {
            (void)TIVarFile::createNew("RealList", "ABCDEF");
        }
        catch (const std::invalid_argument&)
        {
            threwTooLongList = true;
        }
        assert(threwTooLongList);
    }

    {
        TIVarFile namedListFromFile = TIVarFile::loadFromFile("testData/LISTABC.8xl");
        const uint8_t expectedNamedList[8] = {0x5D, 'A', 'B', 'C'};
        assert(std::equal(namedListFromFile.getVarEntries()[0].varname, namedListFromFile.getVarEntries()[0].varname + 8, expectedNamedList));
        assert(namedListFromFile.saveVarToFile("/tmp", "") == "/tmp/ABC.8xl");
        assert(remove("/tmp/ABC.8xl") == 0);

        TIVarFile recreatedNamedList = TIVarFile::createNew("RealList", "ABC");
        recreatedNamedList.setContentFromString(namedListFromFile.getReadableContent());
        assert(recreatedNamedList.getRawContent() == namedListFromFile.getRawContent());

        const std::string savedNamedListPath = recreatedNamedList.saveVarToFile("/tmp", "");
        assert(savedNamedListPath == "/tmp/ABC.8xl");
        TIVarFile reloadedNamedList = TIVarFile::loadFromFile(savedNamedListPath);
        assert(reloadedNamedList.getRawContent() == namedListFromFile.getRawContent());
        assert(remove(savedNamedListPath.c_str()) == 0);
    }

    {
        TIVarFile matrixDefaultVar = TIVarFile::createNew("Matrix");
        const auto& matrixDefault = matrixDefaultVar.getVarEntries()[0];
        const uint8_t expectedMatrixDefault[8] = {0x5C, 0x00};
        assert(std::equal(matrixDefault.varname, matrixDefault.varname + 8, expectedMatrixDefault));

        TIVarFile matrixB = TIVarFile::createNew("Matrix", "[b]");
        const uint8_t expectedMatrixB[8] = {0x5C, 0x01};
        assert(std::equal(matrixB.getVarEntries()[0].varname, matrixB.getVarEntries()[0].varname + 8, expectedMatrixB));

        TIVarFile exactMatrix = TIVarFile::loadFromFile("testData/Matrix_2x2_exact.8xm");
        assert(!exactMatrix.getReadableContent().empty());
        assert(exactMatrix.getVarEntries()[0].version >= 0x06);
    }

    {
        TIVarFile equationDefaultVar = TIVarFile::createNew("Equation");
        const auto& equationDefault = equationDefaultVar.getVarEntries()[0];
        const uint8_t expectedEquationDefault[8] = {0x5E, 0x10};
        assert(std::equal(equationDefault.varname, equationDefault.varname + 8, expectedEquationDefault));

        TIVarFile equationY0 = TIVarFile::createNew("Equation", "Y0");
        const uint8_t expectedEquationY0[8] = {0x5E, 0x19};
        assert(std::equal(equationY0.getVarEntries()[0].varname, equationY0.getVarEntries()[0].varname + 8, expectedEquationY0));

        TIVarFile equationX3T = TIVarFile::createNew("Equation", "{x3t}");
        const uint8_t expectedEquationX3T[8] = {0x5E, 0x24};
        assert(std::equal(equationX3T.getVarEntries()[0].varname, equationX3T.getVarEntries()[0].varname + 8, expectedEquationX3T));

        TIVarFile equationR4 = TIVarFile::createNew("Equation", "r4");
        const uint8_t expectedEquationR4[8] = {0x5E, 0x43};
        assert(std::equal(equationR4.getVarEntries()[0].varname, equationR4.getVarEntries()[0].varname + 8, expectedEquationR4));

        TIVarFile equationU = TIVarFile::createNew("Equation", "u");
        const uint8_t expectedEquationU[8] = {0x5E, 0x80};
        assert(std::equal(equationU.getVarEntries()[0].varname, equationU.getVarEntries()[0].varname + 8, expectedEquationU));

        const std::pair<const char*, const char*> equationSamples[] = {
            {"testData/Equation_Y1.8xy", "Y1"},
            {"testData/Equation_X1T.8xy", "X1T"},
            {"testData/Equation_r1.8xy", "R1"},
            {"testData/Equation_u.8xy", "U"},
        };
        for (const auto& [path, expectedName] : equationSamples)
        {
            TIVarFile equationSample = TIVarFile::loadFromFile(path);
            assert(entry_name_to_string(equationSample.getVarEntries()[0]._type, equationSample.getVarEntries()[0].varname) == expectedName);

            TIVarFile recreatedEquation = TIVarFile::createNew("Equation", expectedName);
            recreatedEquation.setContentFromString(equationSample.getReadableContent());
            assert(recreatedEquation.getRawContent() == equationSample.getRawContent());
        }

        const uint8_t altEquationY1T[8] = {'Y', 0x81};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationY1T) == "Y1T");
        const uint8_t altEquationX3T[8] = {'X', 0x83};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationX3T) == "X3T");
        const uint8_t altEquationR6[8] = {'r', 0x86};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationR6) == "R6");
        const uint8_t altEquationU[8] = {'U', 0x00};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationU) == "U");
        const uint8_t altEquationV[8] = {'V', 0x00};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationV) == "V");
        const uint8_t altEquationW[8] = {'W', 0x00};
        assert(entry_name_to_string(TIVarType{"Equation"}, altEquationW) == "W");
    }

    {
        TIVarFile stringDefaultVar = TIVarFile::createNew("String");
        const auto& stringDefault = stringDefaultVar.getVarEntries()[0];
        const uint8_t expectedStringDefault[8] = {0xAA, 0x00};
        assert(std::equal(stringDefault.varname, stringDefault.varname + 8, expectedStringDefault));

        TIVarFile string0 = TIVarFile::createNew("String", "str0");
        const uint8_t expectedString0[8] = {0xAA, 0x09};
        assert(std::equal(string0.getVarEntries()[0].varname, string0.getVarEntries()[0].varname + 8, expectedString0));
    }

    {
        TIVarFile picDefaultVar = TIVarFile::createNew("Picture");
        const auto& picDefault = picDefaultVar.getVarEntries()[0];
        const uint8_t expectedPicDefault[8] = {0x60, 0x00};
        assert(std::equal(picDefault.varname, picDefault.varname + 8, expectedPicDefault));

        TIVarFile pic0 = TIVarFile::createNew("Picture", "pic0");
        const uint8_t expectedPic0[8] = {0x60, 0x09};
        assert(std::equal(pic0.getVarEntries()[0].varname, pic0.getVarEntries()[0].varname + 8, expectedPic0));
    }

    {
        TIVarFile imageDefaultVar = TIVarFile::createNew("Image");
        const auto& imageDefault = imageDefaultVar.getVarEntries()[0];
        const uint8_t expectedImageDefault[8] = {0x3C, 0x00};
        assert(std::equal(imageDefault.varname, imageDefault.varname + 8, expectedImageDefault));

        TIVarFile image0 = TIVarFile::createNew("Image", "image0");
        const uint8_t expectedImage0[8] = {0x3C, 0x09};
        assert(std::equal(image0.getVarEntries()[0].varname, image0.getVarEntries()[0].varname + 8, expectedImage0));
    }

    {
        const auto raw_to_hex = [](const data_t& data)
        {
            std::string hex;
            hex.reserve(data.size() * 2);
            for (const uint8_t byte : data)
            {
                hex += dechex(byte);
            }
            return hex;
        };

        TIVarFile monoPicture = TIVarFile::loadFromFile("testData/BartSimpson.8xi");
        const json monoPictureJSON = json::parse(monoPicture.getReadableContent());
        assert(monoPictureJSON["kind"] == "MonoPicture");
        assert(monoPictureJSON["typeName"] == "Picture");
        assert(monoPictureJSON["width"] == 96);
        assert(monoPictureJSON["height"] == 63);
        assert(monoPictureJSON["hasColor"] == false);
        assert(monoPictureJSON["dataLength"] == 758);
        assert(monoPictureJSON["storage"]["encoding"] == "L1");
        assert(monoPictureJSON["storage"]["pixelsPerByte"] == 8);

        TIVarFile colorPicture = TIVarFile::loadFromFile("testData/Pic1.8ci");
        const json colorPictureJSON = json::parse(colorPicture.getReadableContent());
        assert(colorPictureJSON["kind"] == "ColorPicture");
        assert(colorPictureJSON["typeName"] == "Picture");
        assert(colorPictureJSON["width"] == 266);
        assert(colorPictureJSON["height"] == 165);
        assert(colorPictureJSON["hasColor"] == true);
        assert(colorPictureJSON["dataLength"] == 21947);
        assert(colorPictureJSON["storage"]["encoding"] == "RGBPalette");
        assert(colorPictureJSON["storage"]["paletteSize"] == 15);
        assert(colorPicture.getVarEntries()[0].version == 0x0A);

        TIVarFile image = TIVarFile::loadFromFile("testData/Image1.8ca");
        const json imageJSON = json::parse(image.getReadableContent());
        assert(imageJSON["kind"] == "Image");
        assert(imageJSON["typeName"] == "Image");
        assert(imageJSON["width"] == 133);
        assert(imageJSON["height"] == 83);
        assert(imageJSON["hasColor"] == true);
        assert(imageJSON["dataLength"] == 22247);
        assert(imageJSON["storage"]["encoding"] == "RGB565");
        assert(imageJSON["storage"]["imageMagic"] == "81");
        assert(imageJSON["storage"]["rowPaddingBytes"] == 2);

        TIVarFile recreatedMonoPicture = TIVarFile::createNew("Picture", "Pic1");
        recreatedMonoPicture.setContentFromString(R"({
    "typeName": "Picture",
    "rawDataHex": ")" + raw_to_hex(monoPicture.getRawContent()) + R"("
})");
        assert(recreatedMonoPicture.getRawContent() == monoPicture.getRawContent());

        TIVarFile recreatedColorPicture = TIVarFile::createNew("Picture", "Pic1");
        recreatedColorPicture.setContentFromString(R"({
    "typeName": "Picture",
    "rawDataHex": ")" + raw_to_hex(colorPicture.getRawContent()) + R"("
})");
        assert(recreatedColorPicture.getRawContent() == colorPicture.getRawContent());

        TIVarFile recreatedImage = TIVarFile::createNew("Image", "Image1");
        recreatedImage.setContentFromString(R"({
    "typeName": "Image",
    "rawDataHex": ")" + raw_to_hex(image.getRawContent()) + R"("
})");
        assert(recreatedImage.getRawContent() == image.getRawContent());

        TIVarFile colorPictureCopy = TIVarFile::loadFromFile("testData/Pic1 copy.8ci");
        const json colorPictureCopyJSON = json::parse(colorPictureCopy.getReadableContent());
        assert(colorPictureCopyJSON["kind"] == "ColorPicture");
        assert(colorPictureCopyJSON["typeName"] == "Picture");
        assert(colorPictureCopyJSON["width"] == colorPictureJSON["width"]);
        assert(colorPictureCopyJSON["height"] == colorPictureJSON["height"]);
        assert(colorPictureCopyJSON["storage"]["encoding"] == colorPictureJSON["storage"]["encoding"]);

        TIVarFile imageCopy = TIVarFile::loadFromFile("testData/Image1 copy.8ca");
        const json imageCopyJSON = json::parse(imageCopy.getReadableContent());
        assert(imageCopyJSON["kind"] == "Image");
        assert(imageCopyJSON["typeName"] == "Image");
        assert(imageCopyJSON["width"] == imageJSON["width"]);
        assert(imageCopyJSON["height"] == imageJSON["height"]);
        assert(imageCopyJSON["storage"]["encoding"] == imageJSON["storage"]["encoding"]);

        bool pictureWriteFailed = false;
        try
        {
            monoPicture.setContentFromString("{}");
        }
        catch (const std::runtime_error&)
        {
            pictureWriteFailed = true;
        }
        assert(pictureWriteFailed);
    }

    {
        TIVarFile gdbDefaultVar = TIVarFile::createNew("GraphDataBase");
        const auto& gdbDefault = gdbDefaultVar.getVarEntries()[0];
        const uint8_t expectedGdbDefault[8] = {0x61, 0x00};
        assert(std::equal(gdbDefault.varname, gdbDefault.varname + 8, expectedGdbDefault));

        TIVarFile gdb0 = TIVarFile::createNew("GraphDataBase", "gdb0");
        const uint8_t expectedGdb0[8] = {0x61, 0x09};
        assert(std::equal(gdb0.getVarEntries()[0].varname, gdb0.getVarEntries()[0].varname + 8, expectedGdb0));
    }

    {
        const auto expected_save_path = [](const TIVarFile& file, const char* ext)
        {
            const auto& entry = file.getVarEntries()[0];
            return "/tmp/"s + entry_name_to_string(entry._type, entry.varname) + "." + ext;
        };
        const auto raw_to_hex = [](const data_t& data)
        {
            std::string hex;
            hex.reserve(data.size() * 2);
            for (const uint8_t byte : data)
            {
                hex += dechex(byte);
            }
            return hex;
        };

        TIVarFile namedProgram = TIVarFile::createNew("Program", "A1B2C3");
        namedProgram.setContentFromString("Disp 42");
        const std::string namedProgramPath = expected_save_path(namedProgram, "8xp");
        assert(namedProgram.saveVarToFile("/tmp", "") == namedProgramPath);
        assert(remove(namedProgramPath.c_str()) == 0);

        TIVarFile namedAppVar = TIVarFile::createNew("AppVar", "DATA123", "83PCE");
        namedAppVar.setContentFromString("DEADBEEF");
        const std::string namedAppVarPath = expected_save_path(namedAppVar, "8xv");
        assert(namedAppVar.saveVarToFile("/tmp", "") == namedAppVarPath);
        assert(remove(namedAppVarPath.c_str()) == 0);

        TIVarFile namedGroup = TIVarFile::createNew("GroupObject", "GROUP123", "83PCE");
        namedGroup.setContentFromString(R"({
    "entries": [
        {
            "typeName": "Real",
            "name": "A",
            "readableContent": "1"
        }
    ]
})");
        const std::string namedGroupPath = expected_save_path(namedGroup, "8cg");
        assert(namedGroup.saveVarToFile("/tmp", "") == namedGroupPath);
        assert(remove(namedGroupPath.c_str()) == 0);

        TIVarFile defaultPicture = TIVarFile::loadFromFile("testData/BartSimpson.8xi");
        const std::string defaultPicturePath = expected_save_path(defaultPicture, "8xi");
        assert(defaultPicture.saveVarToFile("/tmp", "") == defaultPicturePath);
        assert(remove(defaultPicturePath.c_str()) == 0);

        TIVarFile sourceImage = TIVarFile::loadFromFile("testData/Image1.8ca");
        TIVarFile defaultImage = TIVarFile::createNew("Image", "Image0", "83PCE");
        defaultImage.setContentFromString(R"({
    "typeName": "Image",
    "rawDataHex": ")"s + raw_to_hex(sourceImage.getRawContent()) + R"("
})");
        const std::string defaultImagePath = expected_save_path(defaultImage, "8ca");
        assert(defaultImage.saveVarToFile("/tmp", "") == defaultImagePath);
        assert(remove(defaultImagePath.c_str()) == 0);

        TIVarFile defaultGdb = TIVarFile::createNew("GraphDataBase", "GDB1", "83PCE");
        defaultGdb.setContentFromString(TIVarFile::loadFromFile("testData/GraphDataBase_Func.8xd").getReadableContent());
        const std::string defaultGdbPath = expected_save_path(defaultGdb, "8xd");
        assert(defaultGdb.saveVarToFile("/tmp", "") == defaultGdbPath);
        assert(remove(defaultGdbPath.c_str()) == 0);

        TIVarFile defaultWindow = TIVarFile::createNew("WindowSettings");
        defaultWindow.setContentFromString(TIVarFile::loadFromFile("testData/Window.8xw").getReadableContent());
        const std::string defaultWindowPath = expected_save_path(defaultWindow, "8xw");
        assert(defaultWindow.saveVarToFile("/tmp", "") == defaultWindowPath);
        assert(remove(defaultWindowPath.c_str()) == 0);

        TIVarFile defaultRecall = TIVarFile::createNew("RecallWindow");
        defaultRecall.setContentFromString(TIVarFile::loadFromFile("testData/RecallWindow.8xz").getReadableContent());
        const std::string defaultRecallPath = expected_save_path(defaultRecall, "8xz");
        assert(defaultRecall.saveVarToFile("/tmp", "") == defaultRecallPath);
        assert(remove(defaultRecallPath.c_str()) == 0);

        TIVarFile defaultTable = TIVarFile::createNew("TableRange");
        defaultTable.setContentFromString(TIVarFile::loadFromFile("testData/TableRange.8xt").getReadableContent());
        const std::string defaultTablePath = expected_save_path(defaultTable, "8xt");
        assert(defaultTable.saveVarToFile("/tmp", "") == defaultTablePath);
        assert(remove(defaultTablePath.c_str()) == 0);

        TIVarFile protectedProgram = TIVarFile::loadFromFile("testData/ProtectedProgram.8xp");
        assert(protectedProgram.getVarEntries()[0]._type.getName() == "ProtectedProgram");
        const std::string protectedName = entry_name_to_string(protectedProgram.getVarEntries()[0]._type, protectedProgram.getVarEntries()[0].varname);
        assert(!protectedName.empty());
        const std::string protectedPath = "/tmp/"s + protectedName + ".8xp";
        assert(protectedProgram.saveVarToFile("/tmp", "") == protectedPath);
        assert(remove(protectedPath.c_str()) == 0);

        TIVarFile longProtectedProgram = TIVarFile::loadFromFile("testData/ProtectedProgram_long.8xp");
        assert(longProtectedProgram.getVarEntries()[0]._type.getName() == "ProtectedProgram");
        const std::string longProtectedName = entry_name_to_string(longProtectedProgram.getVarEntries()[0]._type, longProtectedProgram.getVarEntries()[0].varname);
        assert(!longProtectedName.empty());
        const std::string longProtectedPath = "/tmp/"s + longProtectedName + ".8xp";
        assert(longProtectedProgram.saveVarToFile("/tmp", "") == longProtectedPath);
        assert(remove(longProtectedPath.c_str()) == 0);
    }

    {
        TIVarFile tableRange = TIVarFile::loadFromFile("testData/TableRange.8xt");
        const json tableRangeJSON = json::parse(tableRange.getReadableContent());
        assert(tableRangeJSON["TblMin"] == 0);
        assert(tableRangeJSON["DeltaTbl"] == 1);

        TIVarFile newTableRange = TIVarFile::createNew("TableRange");
        const uint8_t expectedName[8] = {'T', 'b', 'l', 'S', 'e', 't'};
        assert(std::equal(newTableRange.getVarEntries()[0].varname, newTableRange.getVarEntries()[0].varname + 8, expectedName));
        newTableRange.setContentFromString(R"({
    "TblMin": 0,
    "DeltaTbl": 1
})");
        assert(newTableRange.getRawContent() == tableRange.getRawContent());
    }

    {
        TIVarFile windowSettings = TIVarFile::loadFromFile("testData/Window.8xw");
        const json windowSettingsJSON = json::parse(windowSettings.getReadableContent());
        assert(windowSettingsJSON["Xmin"] == -10.0);
        assert(windowSettingsJSON["Ymin"] == -20.0);
        assert(windowSettingsJSON["Xres"] == 2);
        assert(windowSettingsJSON["Thetamax"] == "6.283185307");
        assert(windowSettingsJSON["Thetastep"] == "0.13089969389957");

        TIVarFile newWindowSettings = TIVarFile::createNew("WindowSettings");
        const uint8_t expectedName[8] = {'W', 'i', 'n', 'd', 'o', 'w'};
        assert(std::equal(newWindowSettings.getVarEntries()[0].varname, newWindowSettings.getVarEntries()[0].varname + 8, expectedName));
        newWindowSettings.setContentFromString(windowSettings.getReadableContent());
        assert(newWindowSettings.getRawContent() == windowSettings.getRawContent());
    }

    {
        TIVarFile recallWindow = TIVarFile::loadFromFile("testData/RecallWindow.8xz");
        assert(entry_name_to_string(recallWindow.getVarEntries()[0]._type, recallWindow.getVarEntries()[0].varname) == "RclWindw");
        const json recallWindowJSON = json::parse(recallWindow.getReadableContent());
        assert(recallWindowJSON["Xmin"] == -10.0);
        assert(recallWindowJSON["Xmax"] == 10.0);
        assert(recallWindowJSON["Xres"] == 1);
        assert(recallWindowJSON["Thetamax"] == "6.283185307");
        assert(recallWindowJSON["Thetastep"] == "0.13089969389957");

        TIVarFile newRecallWindow = TIVarFile::createNew("RecallWindow");
        const uint8_t expectedName[8] = {'R', 'c', 'l', 'W', 'i', 'n', 'd', 'w'};
        assert(std::equal(newRecallWindow.getVarEntries()[0].varname, newRecallWindow.getVarEntries()[0].varname + 8, expectedName));
        newRecallWindow.setContentFromString(recallWindow.getReadableContent());
        assert(newRecallWindow.getRawContent() == recallWindow.getRawContent());
    }

#if defined(TH_GDB_SUPPORT) || defined(__cpp_lib_variant)
/*
    {
        TIVarFile GDB1 = TIVarFile::loadFromFile("testData/GraphDataBase_Func.8xd");
        const std::string gdb1JSON = GDB1.getReadableContent();
        cout << "GDB1.getReadableContent() : " << gdb1JSON << endl;

        TIVarFile GDB_new = TIVarFile::createNew("GraphDataBase", "a");
        GDB_new.setContentFromString(gdb1JSON);
        GDB_new.saveVarToFile("testData", "GraphDataBase_Func_new");
        assert(GDB1.getRawContent() == GDB_new.getRawContent());
        assert(gdb1JSON == GDB_new.getReadableContent());
    }
*/
    {
        TIVarFile GDB1 = TIVarFile::loadFromFile("testData/GraphDataBase_Param.8xd");
        const std::string gdb2JSON = GDB1.getReadableContent();

        std::ifstream jsonFile("testData/GDB_Parametric.json");
        std::stringstream gdbJSON;
        gdbJSON << jsonFile.rdbuf();

        TIVarFile paramGDB = TIVarFile::createNew("GraphDataBase", "\x61\x00"); // GDB0
        paramGDB.setContentFromString(gdbJSON.str());
        cout << "paramGDB.getReadableContent() : " << paramGDB.getReadableContent() << endl;
        paramGDB.saveVarToFile("testData", "GraphDataBase_Param_new");
        assert(GDB1.getRawContent() == paramGDB.getRawContent());
    }
#else
    cout << "GDB tests skipped (not built with C++20-capable compiler)" << endl;
#endif

    return 0;
}
