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

#include "src/TIModels.h"
#include "src/TIVarTypes.h"
#include "src/BinaryFile.h"
#include "src/TIVarFile.h"
#include "src/TypeHandlers/TypeHandlers.h"
#include "src/tivarslib_utils.h"

using namespace std;
using namespace tivars;
using namespace tivars::TypeHandlers;
using TypeHandlers::TH_Tokenized;

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
        TIVarFile testReal = TIVarFile::createNew("Real");
        for (const auto& str : { "0.0", "0", "+0.0", "+0" })
        {
            testReal.setContentFromString(str);
            assert(testReal.getRawContent() == data_t({ 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }));
            assert(testReal.getReadableContent() == "0");
        }
    }

    {
        TIVarFile toksPrgm = TIVarFile::loadFromFile("testData/ALLTOKS.8Xp");
        cout << toksPrgm.getReadableContent() << "\n" << endl;
    }

    {
        TIVarFile testPrgmStr1 = TIVarFile::createNew("Program", "asdf");
        testPrgmStr1.setContentFromString("\"42→Str1:Str2:123");
        assert(trim(testPrgmStr1.getReadableContent({{"prettify", true}, {"reindent", true}})) == "\"42→Str1\nStr2\n123");
    }

    {
        assert(TH_Tokenized::oneTokenBytesToString(0x00) == "");
        assert(TH_Tokenized::oneTokenBytesToString(0xBB) == "");
        assert(TH_Tokenized::oneTokenBytesToString(0x3F) == "\n");
        assert(TH_Tokenized::oneTokenBytesToString(0xAD) == "getKey");
        assert(TH_Tokenized::oneTokenBytesToString(0xEF97) == "toString(");
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
        assert((char*)entries[1].varname == std::string("GRAPHX"));
        cout << clibs.getReadableContent() << "\n" << endl;
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
        TIVarFile testStandardMatrix = TIVarFile::loadFromFile("testData/Matrix_3x3_standard.8xm");
        cout << "Before: " << testStandardMatrix.getReadableContent() << "\n   Now: ";
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7,-8,-9]]");
        testStandardMatrix.setContentFromString("[[1,2,3][4,5,6][-7.5,-8,-9][1,2,3][4,5,6][-0.002,-8,-9]]");
        cout << testStandardMatrix.getReadableContent() << "\n";
        testStandardMatrix.saveVarToFile("testData", "Matrix_new");
    }

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
        //TIVarFile newExact_RealRadical = TIVarFile::createNew("ExactRealRadical", "A", "83PCE");
        //newExact_RealRadical.setContentFromString("-42.1337");
        //assert(testExact_RealRadical.getRawContent() == newExact_RealRadical.getRawContent());
        //newExact_RealRadical.saveVarToFile("testData", "Exact_RealRadical_new");
    }

    {
        TIVarFile testExactComplexFrac = TIVarFile::loadFromFile("testData/Exact_ComplexFrac.8xc");
        cout << "Before: " << testExactComplexFrac.getReadableContent() << endl;
        assert(testExactComplexFrac.getReadableContent() == "1/5-2/5i");
        //TIVarFile newExactComplexFrac = TIVarFile::createNew("ExactComplexFrac", "A", "83PCE");
        //newExactComplexFrac.setContentFromString("-42.1337");
        //assert(testExactComplexFrac.getRawContent() == newExactComplexFrac.getRawContent());
        //newExactComplexFrac.saveVarToFile("testData", "Exact_ComplexFrac_new");
    }


    {
        TIVarFile testExactComplexPi = TIVarFile::loadFromFile("testData/Exact_ComplexPi.8xc");
        cout << "Before: " << testExactComplexPi.getReadableContent() << endl;
        assert(testExactComplexPi.getReadableContent() == "1/5-3πi");
        //TIVarFile newExactComplexPi = TIVarFile::createNew("ExactComplexPi", "A", "83PCE");
        //newExactComplexPi.setContentFromString("-42.1337");
        //assert(testExactComplexPi.getRawContent() == newExactComplexPi.getRawContent());
        //newExactComplexPi.saveVarToFile("testData", "Exact_ComplexPi_new");
    }

    {
        TIVarFile testExactComplexPiFrac = TIVarFile::loadFromFile("testData/Exact_ComplexPiFrac.8xc");
        cout << "Before: " << testExactComplexPiFrac.getReadableContent() << endl;
        assert(testExactComplexPiFrac.getReadableContent() == "2π/7i");
        //TIVarFile newExactComplexPiFrac = TIVarFile::createNew("ExactComplexPiFrac", "A", "83PCE");
        //newExactComplexPiFrac.setContentFromString("-42.1337");
        //assert(testExactComplexPiFrac.getRawContent() == newExactComplexPiFrac.getRawContent());
        //newExactComplexPiFrac.saveVarToFile("testData", "Exact_ComplexPiFrac_new");
    }

    {
        TIVarFile testExactComplexRadical = TIVarFile::loadFromFile("testData/Exact_ComplexRadical.8xc");
        cout << "Before: " << testExactComplexRadical.getReadableContent() << endl;

        assert(testExactComplexRadical.getReadableContent() == "(√(6)+√(2))/4+(√(6)-√(2))/4i");
        //TIVarFile newExactComplexRadical = TIVarFile::createNew("ExactComplexRadical", "A", "83PCE");
        //newExactComplexRadical.setContentFromString("-42.1337");
        //assert(testExactComplexRadical.getRawContent() == newExactComplexRadical.getRawContent());
        //newExactComplexRadical.saveVarToFile("testData", "Exact_ComplexRadical_new");
    }

    {
        TIVarFile testExactRealPi = TIVarFile::loadFromFile("testData/Exact_RealPi.8xn");
        cout << "Before: " << testExactRealPi.getReadableContent() << endl;
        assert(testExactRealPi.getReadableContent() == "30π");
        //TIVarFile newExactRealPi = TIVarFile::createNew("ExactRealPi", "A", "83PCE");
        //newExactRealPi.setContentFromString("-42.1337");
        //assert(testExactRealPi.getRawContent() == newExactRealPi.getRawContent());
        //newExactRealPi.saveVarToFile("testData", "Exact_RealPi_new");
    }

    {
        TIVarFile testExactRealPiFrac = TIVarFile::loadFromFile("testData/Exact_RealPiFrac.8xn");
        cout << "Before: " << testExactRealPiFrac.getReadableContent() << endl;
        assert(testExactRealPiFrac.getReadableContent() == "2π/7");
        //TIVarFile newExactRealPiFrac = TIVarFile::createNew("ExactRealPiFrac", "A", "83PCE");
        //newExactRealPiFrac.setContentFromString("-42.1337");
        //assert(testExactRealPiFrac.getRawContent() == newExactRealPiFrac.getRawContent());
        //newExactRealPiFrac.saveVarToFile("testData", "Exact_RealPiFrac_new");
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
        TIVarFile testPython = TIVarFile::createNew("PythonAppVar", "TEST123", "83PCE");
        testPython.setContentFromString("from math import *\nprint(math)\n\n# plop");
        testPython.saveVarToFile("testData", "Pythontest_new");

        TIVarFile pythonFromTest = TIVarFile::loadFromFile("testData/Pythontest_new.8xv");
        cout << "pythonFromTest.getReadableContent() : " << pythonFromTest.getReadableContent() << endl;
        assert(pythonFromTest.getReadableContent() == "from math import *\nprint(math)\n\n# plop");

        TIVarFile pythonFromTest2 = TIVarFile::loadFromFile("testData/python_HELLO.8xv");
        cout << "pythonFromTest2.getReadableContent() : " << pythonFromTest2.getReadableContent() << endl;
        assert(pythonFromTest2.getReadableContent() == "import sys\nprint(sys.version)\n");
    }

    {
        TIVarFile testTheta = TIVarFile::createNew("Program", "θΘϴᶿ");
        uint8_t testThetaVarName[8] = {0x5B, 0x5B, 0x5B, 0x5B};
        const auto& firstVarEntry = testTheta.getVarEntries()[0];
        cout << "testTheta firstVarEntry varname : " << firstVarEntry.varname << endl;
        assert(std::equal(firstVarEntry.varname, firstVarEntry.varname + 8, testThetaVarName));
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

        TIVarFile paramGDB = TIVarFile::createNew("GraphDataBase", "a");
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
